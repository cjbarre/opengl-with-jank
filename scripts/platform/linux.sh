#!/usr/bin/env bash
# Linux-specific platform functions
# This file is sourced by platform/common.sh - do not run directly

# ============================================================================
# Platform Info
# ============================================================================

platform_is_supported() {
    return 0  # Linux is supported
}

get_supported_platforms() {
    echo "macos-arm64, macos-x86_64, linux-arm64, linux-x86_64"
}

# ============================================================================
# Library Naming
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
    local lib_path="$1"
    local lib_name
    lib_name="$(basename "$lib_path")"

    # Set SONAME to just the filename for clean rpath resolution
    patchelf --set-soname "$lib_name" "$lib_path" 2>/dev/null || true
}

add_rpath() {
    local rpath="$1"
    local executable="$2"

    # Convert macOS @executable_path to Linux $ORIGIN
    local linux_rpath="${rpath//@executable_path/\$ORIGIN}"

    patchelf --add-rpath "$linux_rpath" "$executable" 2>/dev/null || true
}

delete_rpath() {
    local rpath="$1"
    local executable="$2"

    # patchelf doesn't have selective delete, remove all rpaths
    # The rpath argument is ignored but kept for API compatibility
    patchelf --remove-rpath "$executable" 2>/dev/null || true
}

change_lib_path() {
    local executable="$1"
    local old_path="$2"
    local new_path="$3"

    local old_name
    old_name="$(basename "$old_path")"
    local new_name
    new_name="$(basename "$new_path")"

    patchelf --replace-needed "$old_name" "$new_name" "$executable" 2>/dev/null || true
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
    local output_path="$1"
    # No stub needed on Linux - OpenGL links directly via -lGL
    # Create parent directory if needed
    mkdir -p "$(dirname "$output_path")"
    # Touch a placeholder file so callers don't error
    touch "$output_path.placeholder"
    echo "Note: No OpenGL stub library needed on Linux (links directly to system libGL)" >&2
}

get_opengl_link_flags() {
    # X11 and related libs are required for GLFW on Linux
    echo "-lGL -lX11 -lpthread -ldl -lm"
}

# ============================================================================
# Package Creation
# ============================================================================

create_app_bundle() {
    local bundle_path="$1"
    local app_name="$2"
    local version="${3:-1.0.0}"
    local bundle_id="${4:-com.example.$app_name}"

    # Linux doesn't have .app bundles
    # Create a standard directory structure instead
    mkdir -p "$bundle_path/bin"
    mkdir -p "$bundle_path/lib"
    mkdir -p "$bundle_path/share/$app_name"

    echo "Created Linux app structure at: $bundle_path"
}

create_distribution_archive() {
    local source_dir="$1"
    local output_path="$2"

    # Linux uses tar.gz for distribution
    tar -czf "$output_path" -C "$(dirname "$source_dir")" "$(basename "$source_dir")"
}

# ============================================================================
# System Dependencies
# ============================================================================

get_homebrew_prefix() {
    echo ""  # N/A on Linux
}

get_system_lib_path() {
    local lib_name="$1"

    # Determine architecture-specific lib path
    local arch
    arch="$(uname -m)"

    case "$arch" in
        x86_64|amd64)
            # Debian/Ubuntu path for x86_64
            if [[ -d "/usr/lib/x86_64-linux-gnu" ]]; then
                echo "/usr/lib/x86_64-linux-gnu"
            else
                echo "/usr/lib64"
            fi
            ;;
        aarch64|arm64)
            # Debian/Ubuntu path for ARM64
            if [[ -d "/usr/lib/aarch64-linux-gnu" ]]; then
                echo "/usr/lib/aarch64-linux-gnu"
            else
                echo "/usr/lib"
            fi
            ;;
        *)
            echo "/usr/lib"
            ;;
    esac
}

bundle_system_lib() {
    local lib_file="$1"
    local dest_dir="$2"

    local lib_name
    lib_name="$(basename "$lib_file")"

    if [[ -f "$lib_file" ]]; then
        cp "$lib_file" "$dest_dir/"
        fix_lib_install_name "$dest_dir/$lib_name"
        return 0
    else
        echo "WARNING: System library not found: $lib_file" >&2
        return 1
    fi
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
# Auto-generated launcher script for Linux
DIR="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_HEADER

    # Add library path and execute
    echo "export LD_LIBRARY_PATH=\"\$DIR/$lib_rel_path:\${LD_LIBRARY_PATH:-}\"" >> "$output_path"
    echo "exec \"\$DIR/$executable_rel_path\" \"\$@\"" >> "$output_path"

    chmod +x "$output_path"
}

generate_app_launcher_script() {
    local output_path="$1"
    local executable_rel_path="$2"
    local lib_rel_path="$3"

    # Same as regular launcher on Linux (no .app bundle concept)
    generate_launcher_script "$output_path" "$executable_rel_path" "$lib_rel_path"
}
