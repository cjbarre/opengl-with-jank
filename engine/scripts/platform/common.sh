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
        JANK_DIR="$JANK_DIR"
    # Try to find jank in PATH and derive location
    elif command -v jank &>/dev/null; then
        local jank_path
        jank_path="$(command -v jank)"
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

    JANK_LLVM="$JANK_DIR/build/llvm-install/usr/local"

    export JANK_DIR JANK_LLVM
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
