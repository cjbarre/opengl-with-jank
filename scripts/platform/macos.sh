#!/usr/bin/env bash
# macOS-specific platform functions
# This file is sourced by platform/common.sh - do not run directly

# ============================================================================
# Platform Info
# ============================================================================

platform_is_supported() {
    return 0  # macOS is fully supported
}

get_supported_platforms() {
    echo "macos-arm64, macos-x86_64"
}

# ============================================================================
# Library Naming
# ============================================================================

get_lib_extension() {
    echo "dylib"
}

get_lib_prefix() {
    echo "lib"
}

get_static_lib_extension() {
    echo "a"
}

# Format library name: libfoo.dylib or libfoo.1.dylib
format_lib_name() {
    local name="$1"
    local version="${2:-}"

    if [[ -n "$version" ]]; then
        echo "lib${name}.${version}.dylib"
    else
        echo "lib${name}.dylib"
    fi
}

# ============================================================================
# Library Path Environment
# ============================================================================

get_lib_search_path_var() {
    echo "DYLD_LIBRARY_PATH"
}

set_lib_search_path() {
    local paths="$1"
    export DYLD_LIBRARY_PATH="${paths}:${DYLD_LIBRARY_PATH:-}"
}

# ============================================================================
# Rpath and Install Name Manipulation
# ============================================================================

# Fix library install name to use @rpath
fix_lib_install_name() {
    local lib_path="$1"
    local lib_name
    lib_name="$(basename "$lib_path")"

    install_name_tool -id "@rpath/$lib_name" "$lib_path"
}

# Add rpath to executable or library
add_rpath() {
    local rpath="$1"
    local executable="$2"

    install_name_tool -add_rpath "$rpath" "$executable"
}

# Delete rpath from executable (silently ignores if not present)
delete_rpath() {
    local rpath="$1"
    local executable="$2"

    install_name_tool -delete_rpath "$rpath" "$executable" 2>/dev/null || true
}

# Change library reference in executable
change_lib_path() {
    local executable="$1"
    local old_path="$2"
    local new_path="$3"

    install_name_tool -change "$old_path" "$new_path" "$executable"
}

# ============================================================================
# Code Signing
# ============================================================================

# Sign with specified identity (use "-" for ad-hoc)
code_sign() {
    local path="$1"
    local identity="${2:--}"

    codesign -f -s "$identity" "$path"
}

# Sign with ad-hoc signature (required after modifying binaries)
code_sign_adhoc() {
    local path="$1"
    codesign -f -s - "$path"
}

# ============================================================================
# OpenGL/Graphics Setup
# ============================================================================

# Create OpenGL stub library for linking
create_opengl_stub() {
    local output_path="$1"

    clang -dynamiclib -o "$output_path" \
        -framework OpenGL \
        -install_name "@rpath/libOpenGL.dylib" \
        -Wl,-reexport_framework,OpenGL
}

# Get OpenGL framework linker flags
get_opengl_link_flags() {
    echo "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation"
}

# ============================================================================
# Package Creation
# ============================================================================

# Create macOS .app bundle structure
create_app_bundle() {
    local bundle_path="$1"
    local app_name="$2"
    local version="${3:-1.0}"
    local bundle_id="${4:-com.example.$app_name}"

    mkdir -p "$bundle_path/Contents/MacOS"
    mkdir -p "$bundle_path/Contents/Resources"

    # Create Info.plist
    cat > "$bundle_path/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>$app_name</string>
    <key>CFBundleDisplayName</key>
    <string>$app_name</string>
    <key>CFBundleIdentifier</key>
    <string>$bundle_id</string>
    <key>CFBundleVersion</key>
    <string>$version</string>
    <key>CFBundleShortVersionString</key>
    <string>$version</string>
    <key>CFBundleExecutable</key>
    <string>$app_name</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.games</string>
</dict>
</plist>
EOF
}

# Create distribution archive (zip for macOS)
create_distribution_archive() {
    local source_dir="$1"
    local output_path="$2"

    local output_dir
    output_dir="$(dirname "$output_path")"
    local archive_name
    archive_name="$(basename "$output_path")"

    (cd "$source_dir" && zip -ryq "$output_dir/$archive_name" .)
}

# ============================================================================
# System Dependencies (Homebrew)
# ============================================================================

# Get Homebrew prefix based on architecture
get_homebrew_prefix() {
    if [[ "$PLATFORM_ARCH" == "arm64" ]]; then
        echo "/opt/homebrew"
    else
        echo "/usr/local"
    fi
}

# Get path to system dependency
get_system_lib_path() {
    local lib_name="$1"
    local brew_prefix
    brew_prefix="$(get_homebrew_prefix)"

    case "$lib_name" in
        openssl|libcrypto*)
            echo "$brew_prefix/opt/openssl@3/lib"
            ;;
        zstd|libzstd*)
            echo "$brew_prefix/opt/zstd/lib"
            ;;
        glfw|libglfw*)
            echo "$brew_prefix/opt/glfw/lib"
            ;;
        *)
            echo "$brew_prefix/lib"
            ;;
    esac
}

# Copy and fix system dependency for bundling
bundle_system_lib() {
    local lib_file="$1"
    local dest_dir="$2"

    local lib_name
    lib_name="$(basename "$lib_file")"

    if [[ -f "$lib_file" ]]; then
        cp "$lib_file" "$dest_dir/"
        fix_lib_install_name "$dest_dir/$lib_name"
        code_sign_adhoc "$dest_dir/$lib_name"
    else
        echo "WARNING: System library not found: $lib_file" >&2
        return 1
    fi
}

# ============================================================================
# Launcher Script Generation
# ============================================================================

# Generate launcher script for executable
generate_launcher_script() {
    local output_path="$1"
    local executable_rel_path="$2"
    local lib_rel_path="$3"

    cat > "$output_path" << 'LAUNCHER_HEADER'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_HEADER

    echo "DYLD_LIBRARY_PATH=\"\$DIR/$lib_rel_path:\${DYLD_LIBRARY_PATH:-}\" exec \"\$DIR/$executable_rel_path\" \"\$@\"" >> "$output_path"

    chmod +x "$output_path"
}

# Generate app bundle launcher script
generate_app_launcher_script() {
    local output_path="$1"
    local executable_rel_path="$2"
    local lib_rel_path="$3"

    cat > "$output_path" << 'LAUNCHER_HEADER'
#!/bin/bash
DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"
LAUNCHER_HEADER

    echo "export DYLD_LIBRARY_PATH=\"\$DIR/$lib_rel_path:\${DYLD_LIBRARY_PATH:-}\"" >> "$output_path"
    echo "cd \"\$DIR\"" >> "$output_path"
    echo "exec \"\$DIR/$executable_rel_path\" \"\$@\"" >> "$output_path"

    chmod +x "$output_path"
}
