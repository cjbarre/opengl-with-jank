#!/usr/bin/env bash
# Platform detection and common utilities for cross-platform builds
#
# Usage: source this file at the start of build scripts
#   source "$DIR/platform/common.sh"
#   require_platform_support
#   load_jank_paths

set -euo pipefail

# ============================================================================
# Platform Detection
# ============================================================================

detect_platform() {
    local os arch

    os="$(uname -s)"
    arch="$(uname -m)"

    case "$os" in
        Darwin)
            case "$arch" in
                arm64) echo "macos-arm64" ;;
                x86_64) echo "macos-x86_64" ;;
                *) echo "macos-unknown" ;;
            esac
            ;;
        Linux)
            case "$arch" in
                x86_64|amd64) echo "linux-x86_64" ;;
                aarch64|arm64) echo "linux-arm64" ;;
                *) echo "linux-unknown" ;;
            esac
            ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT)
            echo "windows-x86_64"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# Allow override via environment variable
PLATFORM="${PLATFORM_OVERRIDE:-$(detect_platform)}"
PLATFORM_OS="${PLATFORM%%-*}"        # e.g., "macos"
PLATFORM_ARCH="${PLATFORM##*-}"      # e.g., "arm64"

export PLATFORM PLATFORM_OS PLATFORM_ARCH

# ============================================================================
# Directory Setup
# ============================================================================

PLATFORM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$PLATFORM_DIR/.." && pwd)"
PROJECT_DIR="$(cd "$SCRIPTS_DIR/.." && pwd)"

export PLATFORM_DIR SCRIPTS_DIR PROJECT_DIR

# ============================================================================
# Load Platform-Specific Implementation
# ============================================================================

case "$PLATFORM_OS" in
    macos)
        source "$PLATFORM_DIR/macos.sh"
        ;;
    linux)
        source "$PLATFORM_DIR/linux.sh"
        ;;
    windows)
        source "$PLATFORM_DIR/windows.sh"
        ;;
    *)
        echo "ERROR: Unsupported platform: $PLATFORM" >&2
        echo "       Detected OS: $(uname -s), Arch: $(uname -m)" >&2
        exit 1
        ;;
esac

# ============================================================================
# Common Utilities
# ============================================================================

# Get platform-specific library directory
get_libs_dir() {
    echo "$PROJECT_DIR/libs/$PLATFORM"
}

# Get library path for a specific library
get_lib_path() {
    local lib_name="$1"
    echo "$(get_libs_dir)/$lib_name/lib"
}

# Get include path for a specific library (prefers shared, falls back to platform)
get_include_path() {
    local lib_name="$1"
    if [[ -d "$PROJECT_DIR/libs/include/$lib_name" ]]; then
        echo "$PROJECT_DIR/libs/include/$lib_name"
    elif [[ -d "$PROJECT_DIR/libs/$lib_name" ]]; then
        # Header-only libs at top level (e.g., glm)
        echo "$PROJECT_DIR/libs/$lib_name"
    else
        echo "$(get_libs_dir)/$lib_name/include"
    fi
}

# Verify platform is supported, exit with helpful message if not
require_platform_support() {
    if ! platform_is_supported; then
        echo "ERROR: Platform '$PLATFORM' is not yet supported." >&2
        echo "" >&2
        echo "Supported platforms: $(get_supported_platforms)" >&2
        echo "" >&2
        echo "To add support for $PLATFORM:" >&2
        echo "  1. Build libraries and place in libs/$PLATFORM/" >&2
        echo "  2. Implement platform functions in platform/${PLATFORM_OS}.sh" >&2
        echo "" >&2
        echo "See CLAUDE.md for details." >&2
        exit 1
    fi
}

# Load jank paths from environment or auto-detect
load_jank_paths() {
    # Try environment variable first
    if [[ -n "${JANK_DIR:-}" ]]; then
        :
    # Try to find jank in PATH and derive location (follow symlinks)
    elif command -v jank &>/dev/null; then
        local jank_path
        jank_path="$(command -v jank)"
        # Resolve symlinks so a /usr/local/bin/jank → …/build/jank link still works
        if command -v readlink &>/dev/null; then
            local resolved
            resolved="$(readlink -f "$jank_path" 2>/dev/null || echo "$jank_path")"
            jank_path="$resolved"
        fi
        # Heuristic: jank binary is in build/ dir, compiler+runtime is parent
        if [[ "$jank_path" == */build/jank ]]; then
            JANK_DIR="${jank_path%/build/jank}"
        else
            echo "ERROR: Found jank at $jank_path but cannot derive JANK_DIR" >&2
            echo "       Please set JANK_DIR environment variable" >&2
            exit 1
        fi
    else
        echo "ERROR: JANK_DIR not set and jank not found in PATH" >&2
        echo "" >&2
        echo "Set JANK_DIR to the jank compiler+runtime directory:" >&2
        echo "  export JANK_DIR=/path/to/jank/compiler+runtime" >&2
        exit 1
    fi

    # JANK_LLVM points at the LLVM install. With jank_local_clang=ON it lives
    # inside the jank build dir; with jank_local_clang=OFF (system Clang),
    # let the caller override or fall back to the system LLVM root.
    if [[ -n "${JANK_LLVM:-}" ]]; then
        :
    elif [[ -d "$JANK_DIR/build/llvm-install/usr/local" ]]; then
        JANK_LLVM="$JANK_DIR/build/llvm-install/usr/local"
    elif [[ -d "/usr/lib/llvm-22" ]]; then
        JANK_LLVM="/usr/lib/llvm-22"
    else
        JANK_LLVM="$JANK_DIR/build/llvm-install/usr/local"
    fi

    export JANK_DIR JANK_LLVM
}

# Run `lein compile` with stable compiler flags. Linux still preloads the ozz
# runtime libraries because some generated namespaces can need those symbols
# before the final executable is linked. On macOS, injecting arm64 project
# dylibs into the host Java process is not portable because GitHub's Java can
# run as arm64e; lein-jank links the libraries through the project config there.
run_lein_compile_with_ozz_preload() {
    local project_dir="$1"
    shift

    local libs_dir lib_ext ozz_preload prefix_map_flags
    libs_dir="$(get_libs_dir)"
    lib_ext="$(get_lib_extension)"
    prefix_map_flags="-ffile-prefix-map=$JANK_DIR=/jank"
    prefix_map_flags="$prefix_map_flags -ffile-prefix-map=$PROJECT_DIR=/engine"
    prefix_map_flags="$prefix_map_flags -ffile-prefix-map=$project_dir=/project"
    if [[ -n "${JANK_LLVM:-}" ]]; then
        prefix_map_flags="$prefix_map_flags -ffile-prefix-map=$JANK_LLVM=/jank-llvm"
    fi
    if [[ -n "${JANK_EXTRA_FLAGS:-}" ]]; then
        prefix_map_flags="$JANK_EXTRA_FLAGS $prefix_map_flags"
    fi
    local ozz_preload_libs=(
        "$libs_dir/ozz-animation/lib/libozz_animation_r.$lib_ext"
        "$libs_dir/ozz-animation/lib/libozz_base_r.$lib_ext"
        "$libs_dir/ozz-animation/lib/libozz_geometry_r.$lib_ext"
    )

    if [[ "$PLATFORM_OS" == "linux" ]]; then
        ozz_preload="$(IFS=:; echo "${ozz_preload_libs[*]}")${LD_PRELOAD:+:$LD_PRELOAD}"
        (cd "$project_dir" && env "$@" JANK_EXTRA_FLAGS="$prefix_map_flags" LD_PRELOAD="$ozz_preload" lein compile --no-debug)
    else
        local lein_jar java_bin
        lein_jar="${LEIN_JAR:-}"
        if [[ -z "$lein_jar" ]] && command -v brew >/dev/null 2>&1; then
            lein_jar="$(find "$(brew --prefix leiningen)/libexec" -name 'leiningen-*-standalone.jar' | head -1)"
        fi
        if [[ -z "$lein_jar" ]]; then
            echo "ERROR: Leiningen standalone jar not found." >&2
            echo "       Install Leiningen with Homebrew or set LEIN_JAR." >&2
            exit 1
        fi
        java_bin="${JAVA_CMD:-$(command -v java)}"
        (cd "$project_dir" && env "$@" JANK_EXTRA_FLAGS="$prefix_map_flags" "$java_bin" \
            -Dfile.encoding=UTF-8 \
            -Dmaven.wagon.http.ssl.easy=false \
            -Dleiningen.original.pwd="$project_dir" \
            -Dleiningen.script="$(command -v lein)" \
            -classpath "$lein_jar" \
            clojure.main -m leiningen.core.main compile --no-debug)
    fi
}

same_length_path_token() {
    local original="$1"
    local label="$2"
    local original_len=${#original}
    local label_len=${#label}

    if (( label_len > original_len )); then
        label="${label:0:$original_len}"
        label_len=$original_len
    fi

    printf '%s' "$label"
    local pad_count=$((original_len - label_len))
    if (( pad_count > 0 )); then
        printf '%*s' "$pad_count" '' | tr ' ' '_'
    fi
}

rewrite_embedded_path_prefix() {
    local file="$1"
    local old_prefix="$2"
    local label="$3"

    [[ -n "$old_prefix" && -e "$file" ]] || return 0
    if ! command -v perl >/dev/null 2>&1; then
        echo "WARN: perl not found; cannot sanitize embedded path prefix in $file" >&2
        return 0
    fi

    local replacement
    replacement="$(same_length_path_token "$old_prefix" "$label")"
    OLD_PREFIX="$old_prefix" NEW_PREFIX="$replacement" \
        perl -0pi -e 's/\Q$ENV{OLD_PREFIX}\E/$ENV{NEW_PREFIX}/g' "$file"
}

sanitize_embedded_build_paths() {
    local file="$1"
    shift || true

    local repo_root
    repo_root="$(cd "$PROJECT_DIR/.." && pwd)"

    rewrite_embedded_path_prefix "$file" "$repo_root" "/portable/opengl-with-jank"
    rewrite_embedded_path_prefix "$file" "$PROJECT_DIR" "/portable/engine"
    rewrite_embedded_path_prefix "$file" "${JANK_DIR:-}" "/portable/jank"
    rewrite_embedded_path_prefix "$file" "${JANK_LLVM:-}" "/portable/jank-llvm"

    local extra
    for extra in "$@"; do
        rewrite_embedded_path_prefix "$file" "$extra" "/portable/project"
    done
}

# Print platform info (useful for debugging)
print_platform_info() {
    echo "Platform: $PLATFORM"
    echo "  OS: $PLATFORM_OS"
    echo "  Arch: $PLATFORM_ARCH"
    echo "  Supported: $(platform_is_supported && echo "yes" || echo "no")"
    echo "  Libs dir: $(get_libs_dir)"
    echo "  Project dir: $PROJECT_DIR"
}

# Build LIBRARY_PATH for linker from library list
build_library_path() {
    local libs_dir
    libs_dir="$(get_libs_dir)"
    local path=""

    for lib in "$@"; do
        if [[ -n "$path" ]]; then
            path="$path:"
        fi
        path="$path$libs_dir/$lib/lib"
    done

    echo "$path"
}

# Build -L flags for compiler from library list
build_lib_flags() {
    local libs_dir
    libs_dir="$(get_libs_dir)"
    local flags=""

    for lib in "$@"; do
        flags="$flags -L$libs_dir/$lib/lib"
    done

    echo "$flags"
}

# Build -I flags for compiler from library list
build_include_flags() {
    local flags=""

    for lib in "$@"; do
        flags="$flags -I$(get_include_path "$lib")"
    done

    echo "$flags"
}
