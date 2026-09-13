#!/usr/bin/env bash
# Shared distribution helpers. Native builds belong to lein-jank.
set -euo pipefail
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPTS_DIR/.." && pwd)"
case "$(uname -s)" in
    Darwin) PLATFORM_OS=macos ;;
    Linux) PLATFORM_OS=linux ;;
    *) echo "Unsupported distribution platform: $(uname -s)" >&2; exit 1 ;;
esac

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
            JANK_DIR="$(cd "$(dirname "$jank_path")/.." && pwd)"
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
    elif [[ "$PLATFORM_OS" == "macos" ]]; then
        local llvm_library
        llvm_library="$(otool -L "$(command -v jank)" | awk '/libLLVM/ {print $1; exit}')"
        if [[ "$llvm_library" == /* ]]; then
            JANK_LLVM="$(dirname "$(dirname "$llvm_library")")"
        else
            JANK_LLVM="$(brew --prefix llvm)"
        fi
    elif [[ -d "$JANK_DIR/build/llvm-install/usr/local" ]]; then
        JANK_LLVM="$JANK_DIR/build/llvm-install/usr/local"
    elif [[ -d "/usr/lib/llvm-23" ]]; then
        JANK_LLVM="/usr/lib/llvm-23"
    elif [[ -d "/usr/lib/llvm-22" ]]; then
        JANK_LLVM="/usr/lib/llvm-22"
    else
        JANK_LLVM="$JANK_DIR/build/llvm-install/usr/local"
    fi

    export JANK_DIR JANK_LLVM
}

# Distribution builds normalize compiler metadata; ordinary development can use lein directly.
run_lein_compile() {
    local project_dir="$1"
    shift
    local prefix_map_flags="-ffile-prefix-map=$JANK_DIR=/jank -ffile-prefix-map=$PROJECT_DIR=/engine -ffile-prefix-map=$project_dir=/project"
    if [[ -n "${JANK_LLVM:-}" ]]; then
        prefix_map_flags="$prefix_map_flags -ffile-prefix-map=$JANK_LLVM=/jank-llvm"
    fi
    (cd "$project_dir" && JANK_EXTRA_FLAGS="${JANK_EXTRA_FLAGS:-} $prefix_map_flags" lein "$@")
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
    rewrite_embedded_path_prefix "$file" "$HOME/.m2" "/maven"
    if [[ -n "${JANK_LLVM:-}" ]]; then
        rewrite_embedded_path_prefix "$file" "$(cd "$JANK_LLVM" && pwd -P)" "/portable/jank-llvm"
        rewrite_embedded_path_prefix "$file" "$JANK_LLVM" "/portable/jank-llvm"
    fi
    for prefix in /opt/homebrew/opt/openssl@3 /opt/homebrew/opt/zstd; do
        rewrite_embedded_path_prefix "$file" "$prefix" "/portable/support"
    done

    local extra
    for extra in "$@"; do
        rewrite_embedded_path_prefix "$file" "$extra" "/portable/project"
    done
}
