#!/usr/bin/env bash
# Linux-specific platform functions (STUB - Not yet implemented)
# This file is sourced by platform/common.sh - do not run directly

_platform_not_supported() {
    echo "ERROR: Linux support is not yet implemented." >&2
    echo "" >&2
    echo "To add Linux support:" >&2
    echo "  1. Build libraries for $PLATFORM and place in libs/$PLATFORM/" >&2
    echo "  2. Implement the platform functions in platform/linux.sh" >&2
    echo "" >&2
    echo "Required libraries:" >&2
    echo "  - glfw (libglfw.so)" >&2
    echo "  - ozz-animation (libozz_*.so)" >&2
    echo "  - stb (libstb_all.so)" >&2
    echo "  - enet (libenet.so)" >&2
    echo "  - cgltf (libcgltf.so)" >&2
    echo "" >&2
    echo "See CLAUDE.md for build instructions." >&2
    exit 1
}

# ============================================================================
# Platform Info
# ============================================================================

platform_is_supported() {
    return 1  # Linux not yet supported
}

get_supported_platforms() {
    echo "macos-arm64, macos-x86_64"
}

# ============================================================================
# Library Naming (implemented for reference)
# ============================================================================

get_lib_extension() {
    echo "so"
}

get_lib_prefix() {
    echo "lib"
}

get_static_lib_extension() {
    echo "a"
}

format_lib_name() {
    local name="$1"
    local version="${2:-}"

    if [[ -n "$version" ]]; then
        echo "lib${name}.so.${version}"
    else
        echo "lib${name}.so"
    fi
}

# ============================================================================
# Library Path Environment
# ============================================================================

get_lib_search_path_var() {
    echo "LD_LIBRARY_PATH"
}

set_lib_search_path() {
    local paths="$1"
    export LD_LIBRARY_PATH="${paths}:${LD_LIBRARY_PATH:-}"
}

# ============================================================================
# Rpath and Library Manipulation
# ============================================================================

fix_lib_install_name() {
    _platform_not_supported
    # Implementation would use: patchelf --set-soname "libfoo.so" "$lib_path"
}

add_rpath() {
    _platform_not_supported
    # Implementation would use: patchelf --add-rpath "$rpath" "$executable"
}

delete_rpath() {
    _platform_not_supported
    # Implementation would use: patchelf --remove-rpath "$executable"
}

change_lib_path() {
    _platform_not_supported
    # Implementation would use: patchelf --replace-needed "$old" "$new" "$executable"
}

# ============================================================================
# Code Signing (no-op on Linux)
# ============================================================================

code_sign() {
    :  # No code signing on Linux
}

code_sign_adhoc() {
    :  # No code signing on Linux
}

# ============================================================================
# OpenGL/Graphics Setup
# ============================================================================

create_opengl_stub() {
    _platform_not_supported
    # Implementation would link against -lGL
}

get_opengl_link_flags() {
    echo "-lGL -lX11"
}

# ============================================================================
# Package Creation
# ============================================================================

create_app_bundle() {
    _platform_not_supported
    # Implementation would create AppImage or similar
}

create_distribution_archive() {
    local source_dir="$1"
    local output_path="$2"

    # Linux uses tar.gz
    tar -czf "$output_path" -C "$source_dir" .
}

# ============================================================================
# System Dependencies
# ============================================================================

get_homebrew_prefix() {
    echo ""  # N/A on Linux
}

get_system_lib_path() {
    local lib_name="$1"
    # Debian/Ubuntu path
    echo "/usr/lib/x86_64-linux-gnu"
}

bundle_system_lib() {
    _platform_not_supported
}

# ============================================================================
# Launcher Script Generation
# ============================================================================

generate_launcher_script() {
    local output_path="$1"
    local executable_rel_path="$2"
    local lib_rel_path="$3"

    cat > "$output_path" << 'LAUNCHER_HEADER'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_HEADER

    echo "LD_LIBRARY_PATH=\"\$DIR/$lib_rel_path:\${LD_LIBRARY_PATH:-}\" exec \"\$DIR/$executable_rel_path\" \"\$@\"" >> "$output_path"

    chmod +x "$output_path"
}

generate_app_launcher_script() {
    _platform_not_supported
}
