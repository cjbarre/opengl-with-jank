#!/usr/bin/env bash
# Windows-specific platform functions (STUB - Not yet implemented)
# This file is sourced by platform/common.sh - do not run directly
#
# Note: Windows support requires MSYS2/MinGW or WSL for bash compatibility

_platform_not_supported() {
    echo "ERROR: Windows support is not yet implemented." >&2
    echo "" >&2
    echo "To add Windows support:" >&2
    echo "  1. Build libraries for $PLATFORM and place in libs/$PLATFORM/" >&2
    echo "  2. Implement the platform functions in platform/windows.sh" >&2
    echo "  3. Use MSYS2/MinGW for bash script compatibility" >&2
    echo "" >&2
    echo "Required libraries:" >&2
    echo "  - glfw (glfw3.dll)" >&2
    echo "  - ozz-animation (ozz_*.dll)" >&2
    echo "  - stb (stb_all.dll)" >&2
    echo "  - enet (enet.dll)" >&2
    echo "  - cgltf (cgltf.dll)" >&2
    echo "" >&2
    echo "See CLAUDE.md for build instructions." >&2
    exit 1
}

# ============================================================================
# Platform Info
# ============================================================================

platform_is_supported() {
    return 1  # Windows not yet supported
}

get_supported_platforms() {
    echo "macos-arm64, macos-x86_64"
}

# ============================================================================
# Library Naming (implemented for reference)
# ============================================================================

get_lib_extension() {
    echo "dll"
}

get_lib_prefix() {
    echo ""  # Windows DLLs typically don't have lib prefix
}

get_static_lib_extension() {
    echo "lib"
}

format_lib_name() {
    local name="$1"
    local version="${2:-}"

    # Windows doesn't typically version DLLs in the filename
    echo "${name}.dll"
}

# ============================================================================
# Library Path Environment
# ============================================================================

get_lib_search_path_var() {
    echo "PATH"  # Windows uses PATH for DLL lookup
}

set_lib_search_path() {
    local paths="$1"
    export PATH="${paths}:${PATH:-}"
}

# ============================================================================
# Rpath and Library Manipulation
# ============================================================================

fix_lib_install_name() {
    _platform_not_supported
    # Windows doesn't have install names - DLLs are found via PATH
}

add_rpath() {
    _platform_not_supported
    # Windows doesn't have rpath - DLLs must be in PATH or same directory
}

delete_rpath() {
    _platform_not_supported
}

change_lib_path() {
    _platform_not_supported
}

# ============================================================================
# Code Signing
# ============================================================================

code_sign() {
    _platform_not_supported
    # Implementation would use: signtool sign /f cert.pfx /p password "$path"
}

code_sign_adhoc() {
    :  # No ad-hoc signing concept on Windows
}

# ============================================================================
# OpenGL/Graphics Setup
# ============================================================================

create_opengl_stub() {
    _platform_not_supported
    # Windows links directly to opengl32.dll
}

get_opengl_link_flags() {
    echo "-lopengl32 -lgdi32"
}

# ============================================================================
# Package Creation
# ============================================================================

create_app_bundle() {
    _platform_not_supported
    # Implementation would create installer or portable directory
}

create_distribution_archive() {
    local source_dir="$1"
    local output_path="$2"

    # Windows commonly uses zip
    (cd "$source_dir" && zip -ryq "$output_path" .)
}

# ============================================================================
# System Dependencies
# ============================================================================

get_homebrew_prefix() {
    echo ""  # N/A on Windows
}

get_system_lib_path() {
    local lib_name="$1"
    echo "/c/Windows/System32"  # MSYS2 path style
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

    # Create a .bat file for Windows
    local bat_path="${output_path%.sh}.bat"
    if [[ "$output_path" != *.bat ]]; then
        bat_path="${output_path}.bat"
    fi

    # Convert forward slashes to backslashes for Windows paths
    local win_exe_path="${executable_rel_path//\//\\}"
    local win_lib_path="${lib_rel_path//\//\\}"

    cat > "$bat_path" << EOF
@echo off
set DIR=%~dp0
set PATH=%DIR%${win_lib_path};%PATH%
"%DIR%${win_exe_path}" %*
EOF

    echo "Created Windows launcher: $bat_path"
}

generate_app_launcher_script() {
    _platform_not_supported
}
