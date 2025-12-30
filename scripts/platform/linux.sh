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

# ============================================================================
# Standalone Distribution (Bundled glibc)
# ============================================================================

# Generate a standalone launcher that uses bundled ld-linux loader
# This makes the distribution completely independent of system glibc
generate_standalone_launcher_script() {
    local output_path="$1"
    local executable_rel_path="$2"
    local lib_rel_path="$3"

    cat > "$output_path" << 'LAUNCHER'
#!/bin/bash
# Standalone launcher - uses bundled glibc/loader for maximum portability
# This distribution does not depend on system glibc version

# Get the directory where this script lives (resolve symlinks)
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do
    DIR="$(cd -P "$(dirname "$SOURCE")" && pwd)"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
DIR="$(cd -P "$(dirname "$SOURCE")" && pwd)"

LAUNCHER

    # Add the exec line with the bundled loader
    cat >> "$output_path" << 'LAUNCHER_BODY'
LIB_DIR="$DIR/lib"
LAUNCHER_BODY
    echo "BIN=\"\$DIR/$executable_rel_path\"" >> "$output_path"
    cat >> "$output_path" << 'LAUNCHER_EXEC'

GLIBC_DIR="$LIB_DIR/glibc"

# Detect loader name based on architecture
if [[ -f "$GLIBC_DIR/ld-linux-aarch64.so.1" ]]; then
    LOADER="$GLIBC_DIR/ld-linux-aarch64.so.1"
elif [[ -f "$GLIBC_DIR/ld-linux-x86-64.so.2" ]]; then
    LOADER="$GLIBC_DIR/ld-linux-x86-64.so.2"
else
    echo "ERROR: Cannot find bundled dynamic loader in $GLIBC_DIR" >&2
    exit 1
fi

# Set LD_LIBRARY_PATH for child processes (e.g., clang spawned by jank for JIT)
# IMPORTANT: Only include app/LLVM libs, NOT glibc libs!
# Child processes must use system glibc to avoid version conflicts.
export LD_LIBRARY_PATH="$LIB_DIR:${LD_LIBRARY_PATH:-}"

# Add jank's bundled clang to PATH so jank can find it for JIT
export PATH="$LIB_DIR/jank/0.1/bin:$PATH"

# Set C++ include path so clang finds bundled libstdc++ headers (not system headers)
JANK_INCLUDE="$LIB_DIR/jank/0.1/include"
export CPLUS_INCLUDE_PATH="$JANK_INCLUDE/c++/14:$JANK_INCLUDE/aarch64-linux-gnu/c++/14:$JANK_INCLUDE"

# Use the bundled dynamic loader with both lib dirs:
# - LIB_DIR: app/LLVM libraries
# - GLIBC_DIR: bundled glibc (only for main process, not inherited by children)
exec "$LOADER" --library-path "$LIB_DIR:$GLIBC_DIR" "$BIN" "$@"
LAUNCHER_EXEC

    chmod +x "$output_path"
}

# Bundle all shared library dependencies for standalone distribution
# This copies glibc, libstdc++, and all transitive dependencies
#
# Structure:
#   lib/         - LLVM and app libraries (exported via LD_LIBRARY_PATH for child processes)
#   lib/glibc/   - Core glibc libraries (only used by bundled loader, NOT in LD_LIBRARY_PATH)
#
# This separation is critical: child processes (like clang spawned by jank) must use
# system glibc, not bundled glibc, to avoid version mismatches.
bundle_all_dependencies() {
    local dist_dir="$1"
    local lib_dir="$dist_dir/lib"
    local glibc_dir="$dist_dir/lib/glibc"
    local bin_dir="$dist_dir/bin"

    echo "=== Bundling system libraries for standalone distribution ==="

    # Ensure directories exist
    mkdir -p "$lib_dir"
    mkdir -p "$glibc_dir"

    # Helper to get library dependencies
    get_deps() {
        ldd "$1" 2>/dev/null | grep "=> /" | awk '{print $3}' | sort -u
    }

    # Libraries that are part of glibc and must NOT be in LD_LIBRARY_PATH
    # Child processes must use system glibc to avoid version conflicts
    is_glibc_lib() {
        local name="$1"
        case "$name" in
            libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|libresolv.so*|\
            libnss_*.so*|libnsl.so*|libcrypt.so*|libutil.so*|libBrokenLocale.so*|\
            libthread_db.so*|libanl.so*|libmvec.so*|ld-linux*.so*)
                return 0 ;;
            *)
                return 1 ;;
        esac
    }

    # Find the main executable
    local executable
    executable="$(find "$bin_dir" -type f -executable | head -1)"
    if [[ -z "$executable" ]]; then
        echo "ERROR: No executable found in $bin_dir" >&2
        return 1
    fi

    echo "Analyzing: $executable"

    # Copy the dynamic loader to glibc dir
    echo "Copying dynamic loader..."
    if [[ -f /lib/ld-linux-aarch64.so.1 ]]; then
        cp -L /lib/ld-linux-aarch64.so.1 "$glibc_dir/"
    elif [[ -f /lib64/ld-linux-x86-64.so.2 ]]; then
        cp -L /lib64/ld-linux-x86-64.so.2 "$glibc_dir/"
    else
        echo "ERROR: Cannot find dynamic loader" >&2
        return 1
    fi
    chmod 755 "$glibc_dir"/ld-linux*.so.*

    # Collect dependencies from executable
    echo "Copying libraries from executable..."
    for lib in $(get_deps "$executable"); do
        if [[ -f "$lib" ]]; then
            libname="$(basename "$lib")"
            if is_glibc_lib "$libname"; then
                if [[ ! -f "$glibc_dir/$libname" ]]; then
                    echo "  Copying (glibc): $libname"
                    cp -L "$lib" "$glibc_dir/" 2>/dev/null || true
                fi
            else
                if [[ ! -f "$lib_dir/$libname" ]]; then
                    echo "  Copying: $libname"
                    cp -L "$lib" "$lib_dir/" 2>/dev/null || true
                fi
            fi
        fi
    done

    # Copy LLVM libraries if jank is installed
    if [[ -d "${JANK_LLVM_LIB:-}" ]]; then
        echo "Copying LLVM libraries..."
        for lib in "$JANK_LLVM_LIB"/*.so*; do
            if [[ -f "$lib" ]]; then
                libname="$(basename "$lib")"
                if [[ ! -f "$lib_dir/$libname" ]]; then
                    echo "  Copying: $libname"
                    cp -L "$lib" "$lib_dir/" 2>/dev/null || true
                fi
            fi
        done
    fi

    # Iteratively copy transitive dependencies (3 passes)
    for pass in 1 2 3; do
        echo "Transitive dependency pass $pass..."
        for lib in "$lib_dir"/*.so* "$glibc_dir"/*.so*; do
            if [[ -f "$lib" ]]; then
                for dep in $(get_deps "$lib"); do
                    depname="$(basename "$dep")"
                    if is_glibc_lib "$depname"; then
                        if [[ ! -f "$glibc_dir/$depname" ]] && [[ -f "$dep" ]]; then
                            echo "  Copying (glibc): $depname"
                            cp -L "$dep" "$glibc_dir/" 2>/dev/null || true
                        fi
                    else
                        if [[ ! -f "$lib_dir/$depname" ]] && [[ -f "$dep" ]]; then
                            echo "  Copying: $depname"
                            cp -L "$dep" "$lib_dir/" 2>/dev/null || true
                        fi
                    fi
                done
            fi
        done
    done

    # Create symlinks for versioned libraries
    echo "Creating library symlinks..."
    for dir in "$lib_dir" "$glibc_dir"; do
        (
            cd "$dir"
            for lib in *.so.*; do
                if [[ -f "$lib" ]]; then
                    base="${lib%.so.*}.so"
                    if [[ ! -e "$base" ]] && [[ "$lib" != "$base" ]]; then
                        ln -sf "$lib" "$base"
                    fi
                fi
            done
        )
    done

    local lib_count glibc_count
    lib_count="$(find "$lib_dir" -maxdepth 1 -name "*.so*" | wc -l)"
    glibc_count="$(find "$glibc_dir" -name "*.so*" | wc -l)"
    echo "=== Bundled $lib_count app/LLVM libraries + $glibc_count glibc libraries ==="
}

# Create a fully standalone distribution
make_standalone_distribution() {
    local dist_dir="$1"
    local output_name="${2:-app}"

    echo "Creating standalone distribution in: $dist_dir"

    # Bundle all dependencies
    bundle_all_dependencies "$dist_dir"

    # Find the executable name
    local executable
    executable="$(find "$dist_dir/bin" -type f -executable -printf '%f\n' | head -1)"

    # Generate standalone launcher
    generate_standalone_launcher_script \
        "$dist_dir/${output_name}_run" \
        "bin/$executable" \
        "lib"

    echo "Created launcher: $dist_dir/${output_name}_run"
}
