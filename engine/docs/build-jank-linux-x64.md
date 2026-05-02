# Build Workflow: LLVM/Clang + Jank for Linux x86_64

Build jank's custom LLVM 22 and jank compiler on native Linux x64 to compile the opengl-with-jank game (server and client distributions).

> **Important**: This guide uses cjbarre's fork of jank with Windows/cross-platform improvements:
> https://github.com/cjbarre/jank/tree/add-windows

---

## Verified Working Build (2024-12-30)

Successfully built on Ubuntu 24.04 (GCP instance, 32 cores, 125GB RAM).

### Critical: Use GCC 14 (not GCC 13)

Ubuntu 24.04 ships with GCC 13, but **GCC 14 is required** for full C++20 `<chrono>` support (`std::chrono::parse`).

```bash
# Install GCC 14 and set as default
sudo apt-get install -y gcc-14 g++-14 libstdc++-14-dev
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-14

# Verify
gcc --version  # Should show 14.2.0
```

**Why?** Jank uses `std::chrono::parse` for `#inst` date literals. This C++20 feature exists in:
- ❌ libstdc++ (GCC 13) - NOT implemented
- ❌ libc++ (LLVM 22) - NOT implemented
- ✅ libstdc++ (GCC 14) - Works!

Clang automatically uses the system's libstdc++ headers, so installing GCC 14 makes it work.

### Additional Package Required

```bash
sudo apt-get install -y libzstd-dev  # Required for AOT compilation
```

### Build Times (32 cores)
- LLVM/Clang 22: ~15 minutes
- Jank: ~3 minutes

### Alternative: Patch for GCC 13

If you can't use GCC 14, patch `inst.cpp` to disable `#inst` parsing:

```bash
sed -i 's/#ifdef _LIBCPP_VERSION/#if defined(_LIBCPP_VERSION) || defined(__GLIBCXX__)/' \
  src/cpp/jank/runtime/obj/inst.cpp
```

---

## Prerequisites

### System Requirements
- Linux x86_64 (Ubuntu 22.04+ or Debian 12+ recommended)
- ~50GB disk space (LLVM build is large)
- 16GB+ RAM recommended (LLVM build is memory-intensive)
- Multi-core CPU (speeds up parallel builds significantly)

### Install System Dependencies

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
  curl git git-lfs build-essential \
  libssl-dev libdouble-conversion-dev \
  pkg-config ninja-build cmake \
  zlib1g-dev libffi-dev libbz2-dev \
  doctest-dev gcc g++ \
  clang \
  autoconf automake libtool \
  patchelf \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev libgl1-mesa-dev
  # clang              - Bootstrap clang for building LLVM
  # autoconf/automake  - For building some libraries from source
  # patchelf           - For rpath manipulation on Linux
  # libx*-dev          - X11 headers for GLFW
```

---

## Phase 1: Build LLVM/Clang 22

### Key Requirements

| Requirement | Value |
|-------------|-------|
| LLVM/Clang Version | **22.0.x** (from jank fork) |
| Repository | `https://github.com/jank-lang/llvm-project.git` |
| Branch | `jank-snapshot/llvm22` |
| C++ Standard Library | **libc++** (NOT libstdc++) |
| RTTI | Enabled |
| Exceptions | Enabled |
| Compiler | Must use clang/clang++ |

### Clone Jank Repository

```bash
# Use cjbarre's fork with cross-platform improvements
git clone --recursive -b add-windows https://github.com/cjbarre/jank.git
cd jank/compiler+runtime
```

### Build LLVM

```bash
# Set clang as the compiler (required)
export CC=clang
export CXX=clang++

# Run the build script (takes 1-3 hours depending on CPU)
# Uses all CPU cores by default (detected via nproc)
./bin/build-clang

# Or specify cores manually with -j flag:
# ./bin/build-clang -j $(nproc)
```

> **Tip**: The build automatically uses all available CPU cores. On a machine with many cores, this significantly speeds up the ~1-3 hour LLVM build. Monitor with `htop` to ensure all cores are utilized.

The `build-clang` script does the following:

1. Clones `jank-lang/llvm-project` branch `jank-snapshot/llvm22` to `build/llvm/`
2. Configures cmake with these critical flags:
   ```
   -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt"
   -DLLVM_BUILD_LLVM_DYLIB=ON
   -DLLVM_LINK_LLVM_DYLIB=ON
   -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra"
   -DLLVM_TARGETS_TO_BUILD="host"
   -DLLVM_ENABLE_EH=ON
   -DLLVM_ENABLE_RTTI=ON
   ```
3. Builds with ninja and installs to `build/llvm-install/usr/local/`

### Verify LLVM Build

```bash
./build/llvm-install/usr/local/bin/clang++ --version
# Should show: clang version 22.0.0 (or similar)
```

---

## Phase 2: Build Jank

### Switch to Custom-Built Clang

**CRITICAL**: Jank must be compiled with the **same clang version** it embeds. The cmake configuration validates this.

```bash
export CC=$PWD/build/llvm-install/usr/local/bin/clang
export CXX=$PWD/build/llvm-install/usr/local/bin/clang++
```

### Configure Jank

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -Djank_local_clang=ON \
  -Djank_test=OFF
```

Key flags:
- `-Djank_local_clang=ON` - Uses the LLVM we just built (auto-finds `build/llvm-install/`)
- `-DCMAKE_BUILD_TYPE=Release` - Optimized build
- `-Djank_test=OFF` - Skip tests for faster build

### Build Jank

```bash
# Uses all CPU cores by default
cmake --build build --parallel

# Or specify cores explicitly:
# cmake --build build --parallel $(nproc)
```

### Verify Jank

```bash
./build/jank --help
./build/jank check-health
```

### Install (Optional)

```bash
# Default install to /usr/local
sudo cmake --install build

# Or custom prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/jank -B build -G Ninja ...
sudo cmake --install build
```

---

## Phase 3: Prepare opengl-with-jank Project

### Clear macOS Cache Files

When copying the opengl-with-jank project from macOS, clean out platform-specific caches and build artifacts:

```bash
cd opengl-with-jank

# Remove jank compilation cache
rm -rf .jank-cache/

# Remove any compiled artifacts
rm -rf dist/
rm -rf build/

# Remove macOS-specific library cache
rm -rf libs/macos-arm64/

# Clean any IDE/editor caches
rm -rf .idea/ .vscode/*.log
```

### Build Game Libraries for Linux x64

Build these libraries for Linux x64:

### Library Requirements

| Library | Type | Server | Client | Notes |
|---------|------|--------|--------|-------|
| enet | Shared (.so) | ✓ | ✓ | UDP networking |
| cgltf | Shared (.so) | ✓ | ✓ | glTF loading |
| glm | Header-only | ✓ | ✓ | Math (no build needed) |
| glfw | Shared (.so) | - | ✓ | Windowing |
| stb | Shared (.so) | - | ✓ | Texture loading |
| ozz-animation | Shared (.so) | - | ✓ | Skeletal animation |

### Target Directory Structure

```
libs/linux-x86_64/
├── enet/
│   ├── include/
│   └── lib/
│       └── libenet.so
├── cgltf/
│   ├── include/
│   └── lib/
│       └── libcgltf.so
├── glfw/
│   ├── include/
│   └── lib/
│       └── libglfw.so.3
├── stb/
│   └── lib/
│       └── libstb_all.so
└── ozz-animation/
    ├── include/
    └── lib/
        ├── libozz_animation_r.so
        ├── libozz_base_r.so
        └── libozz_geometry_r.so
```

### Building Individual Libraries

#### enet (CRITICAL: use project's single-header version)

The project uses a single-header `enet.h` in `include/` that has different symbol names than the upstream enet library. The header defines `enet_address_set_host` as a macro that expands to `enet_address_set_host_old`. **You MUST build enet from the project's header, NOT from upstream.**

```bash
# Build from project's single-header enet.h (REQUIRED)
cat > /tmp/enet_impl.c << 'EOF'
#define ENET_IMPLEMENTATION
#include "enet.h"
EOF

cd opengl-with-jank
gcc -shared -fPIC -o libs/linux-x86_64/enet/lib/libenet.so /tmp/enet_impl.c -Iinclude -lm

# Verify the _old suffix symbols are exported (required by the project)
nm -gD libs/linux-x86_64/enet/lib/libenet.so | grep address_set_host_old
# Should show: enet_address_set_host_old
```

**DO NOT** use system `libenet-dev` or build from upstream - these export different symbols and will cause "Symbols not found: enet_address_set_host_old" errors at runtime.

#### cgltf (header-only to shared lib)

```bash
cat > cgltf_impl.c << 'EOF'
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
EOF

gcc -shared -fPIC -o libcgltf.so cgltf_impl.c -Iinclude
```

#### stb (header-only to shared lib)

The STB library includes both image loading (stb_image) and font rendering (stb_truetype):

```bash
cat > stb_impl.c << 'EOF'
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
EOF

gcc -shared -fPIC -o libs/linux-x86_64/stb/lib/libstb_all.so stb_impl.c -Iinclude -lm
```

#### ozz-animation (from submodule)

```bash
cd third_party/ozz-animation
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
make -j$(nproc)
# Copy libs to libs/linux-x86_64/ozz-animation/lib/
```

#### glfw (from package or source)

```bash
# Option 1: System package
sudo apt-get install libglfw3-dev

# Option 2: Build from source
git clone https://github.com/glfw/glfw.git
cd glfw
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
```

---

## Phase 4: Compile Game for Linux x64

### Server Build (Direct Compilation)

When running directly on Linux with jank in PATH:

```bash
cd opengl-with-jank

# Set up environment
export PATH="$HOME/jank/compiler+runtime/build:$PATH"  # Adjust to your jank location
export LIBRARY_PATH="$PWD/libs/linux-x86_64/enet/lib:$PWD/libs/linux-x86_64/cgltf/lib"
export LD_LIBRARY_PATH="$PWD/libs/linux-x86_64/enet/lib:$PWD/libs/linux-x86_64/cgltf/lib"

# Clear any stale cache
rm -rf .jank-cache

# Compile server
jank \
  -I libs/glm -I include \
  -L libs/linux-x86_64/enet/lib -lenet \
  -L libs/linux-x86_64/cgltf/lib -lcgltf \
  --module-path src compile examples.demo.server -o demo-server

# Run server
./demo-server
```

### Client Build

The client requires Mesa/OpenGL libraries. On headless servers, install Mesa for software rendering:

```bash
# Install Mesa and GLEW (required for headless compilation)
sudo apt-get install -y libosmesa6 libgl1-mesa-dri mesa-utils libglew-dev
```

```bash
# Set up environment with system library paths
export LIBRARY_PATH="$PWD/libs/linux-x86_64/enet/lib:$PWD/libs/linux-x86_64/cgltf/lib:$PWD/libs/linux-x86_64/glfw/lib:$PWD/libs/linux-x86_64/stb/lib:$PWD/libs/linux-x86_64/ozz-animation/lib:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LIBRARY_PATH"

jank \
  -I libs/glm -I include \
  -I libs/linux-x86_64/glfw/include \
  -I libs/linux-x86_64/ozz-animation/include \
  -L libs/linux-x86_64/enet/lib -lenet \
  -L libs/linux-x86_64/cgltf/lib -lcgltf \
  -L libs/linux-x86_64/glfw/lib -lglfw \
  -L libs/linux-x86_64/stb/lib -lstb_all \
  -L libs/linux-x86_64/ozz-animation/lib -lozz_animation_r -lozz_base_r -lozz_geometry_r \
  -L /lib/x86_64-linux-gnu -lGL -lX11 -lGLEW \
  --module-path src compile examples.demo.core -o demo
```

### Distribution Structure

```
dist/
├── bin/
│   ├── demo              # Client executable
│   └── demo-server       # Server executable
├── lib/
│   ├── demo/             # Client dylibs
│   ├── demo-server/      # Server dylibs (fewer)
│   └── jank/0.1/         # Jank runtime (clang, headers, stdlib)
├── demo_run              # Client launcher script
├── demo-server_run       # Server launcher script
├── shaders/              # GLSL shaders
├── textures/             # Texture assets
└── models/               # 3D models
```

---

## Scripts Status for Linux Support

| Script | Status | Notes |
|--------|--------|-------|
| `platform/linux.sh` | ✅ Complete | Full implementation of platform functions |
| `setup` | Partial | Manual library building required (see Phase 3) |
| `compile` | Needs testing | Platform abstraction layer should work |
| `compile-server` | Needs testing | Platform abstraction layer should work |
| `package-client` | Needs work | Linux packaging not yet implemented |

### Key Platform Differences (macOS vs Linux)

| Aspect | macOS | Linux |
|--------|-------|-------|
| Library extension | `.dylib` | `.so` |
| Library prefix | `lib` | `lib` |
| Rpath flag | `-Wl,-rpath,@executable_path/...` | `-Wl,-rpath,'$ORIGIN/...'` |
| Install name tool | `install_name_tool` | `patchelf` |
| Code signing | `codesign --ad-hoc` | Not required |
| Bundle format | `.app` | AppImage or directory |

---

## Troubleshooting

### "Library not loaded" errors
```bash
# Check library dependencies
ldd ./dist/bin/demo

# Verify rpath
readelf -d ./dist/bin/demo | grep RPATH

# Set LD_LIBRARY_PATH as fallback
export LD_LIBRARY_PATH=$PWD/dist/lib/demo:$LD_LIBRARY_PATH
```

### "Unable to find a suitable Clang 22 binary"
- Ensure `dist/lib/jank/0.1/bin/clang++` exists
- Check clang can find its libraries: `ldd dist/lib/jank/0.1/bin/clang-22`

### LLVM build fails with OOM
```bash
# Reduce parallelism
./bin/build-clang -j 4
```

### "no member named 'parse' in namespace 'std::chrono'" error
You forgot to install GCC 14! Clang uses the system's libstdc++ headers, and GCC 13's version doesn't have `std::chrono::parse`.

**Fix:**
```bash
sudo apt-get install -y gcc-14 g++-14 libstdc++-14-dev
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-14
```

Then rebuild jank from scratch (reconfigure cmake).

### "version 'GLIBCXX_3.4.xx' not found"
This means mixing libstdc++ and libc++. Ensure:
- LLVM was built with libc++ (the default in build-clang)
- Jank was compiled with the custom clang (which uses libc++)

---

## Quick Reference

```bash
# ============================================================================
# Full build from scratch on fresh Ubuntu 24.04
# ============================================================================

# 1. Install all dependencies
sudo apt-get update
sudo apt-get install -y curl git git-lfs build-essential libssl-dev \
  libdouble-conversion-dev pkg-config ninja-build cmake zlib1g-dev \
  libffi-dev libbz2-dev doctest-dev clang libzstd-dev \
  gcc-14 g++-14 libstdc++-14-dev \
  autoconf automake libtool patchelf \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev libgl1-mesa-dev

# 2. Set GCC 14 as default (required for std::chrono::parse)
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-14

# 3. Clone cjbarre's fork with cross-platform improvements
git clone --recursive -b add-windows https://github.com/cjbarre/jank.git
cd jank/compiler+runtime

# 4. Build LLVM/Clang 22
export CC=clang CXX=clang++
./bin/build-clang                    # ~15 min with 32 cores

# 5. Build jank
export CC=$PWD/build/llvm-install/usr/local/bin/clang
export CXX=$PWD/build/llvm-install/usr/local/bin/clang++
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -Djank_local_clang=ON -Djank_test=OFF
cmake --build build --parallel       # Uses all CPU cores

./build/jank check-health            # Verify installation (all ✅)

# ============================================================================
# Build game libraries (in opengl-with-jank directory)
# ============================================================================

cd ~/opengl-with-jank
mkdir -p libs/linux-x86_64/{enet,cgltf,glfw,stb,ozz-animation}/{include,lib}

# enet (MUST use project's single-header version!)
cat > /tmp/enet_impl.c << 'EOF'
#define ENET_IMPLEMENTATION
#include "enet.h"
EOF
gcc -shared -fPIC -o libs/linux-x86_64/enet/lib/libenet.so /tmp/enet_impl.c -Iinclude -lm

# cgltf
cat > /tmp/cgltf_impl.c << 'EOF'
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
EOF
gcc -shared -fPIC -o libs/linux-x86_64/cgltf/lib/libcgltf.so /tmp/cgltf_impl.c -Iinclude

# stb (includes both image loading and font rendering)
cat > /tmp/stb_impl.c << 'EOF'
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
EOF
gcc -shared -fPIC -o libs/linux-x86_64/stb/lib/libstb_all.so /tmp/stb_impl.c -Iinclude -lm

# glfw
cd /tmp && git clone https://github.com/glfw/glfw.git
cd glfw && cmake -B build -DBUILD_SHARED_LIBS=ON -DGLFW_BUILD_WAYLAND=OFF \
  -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
cmake --build build -j$(nproc)
cp -r include/GLFW ~/opengl-with-jank/libs/linux-x86_64/glfw/include/
cp build/src/libglfw.so* ~/opengl-with-jank/libs/linux-x86_64/glfw/lib/

# ozz-animation
cd ~/opengl-with-jank/third_party/ozz-animation
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -Dozz_build_samples=OFF -Dozz_build_tools=OFF -Dozz_build_howtos=OFF
make -j$(nproc)
cp src/animation/runtime/libozz_animation_r.so ~/opengl-with-jank/libs/linux-x86_64/ozz-animation/lib/
cp src/base/libozz_base_r.so ~/opengl-with-jank/libs/linux-x86_64/ozz-animation/lib/
cp src/geometry/runtime/libozz_geometry_r.so ~/opengl-with-jank/libs/linux-x86_64/ozz-animation/lib/
cp -r ../include/ozz ~/opengl-with-jank/libs/linux-x86_64/ozz-animation/include/

# ============================================================================
# Compile game server
# ============================================================================

cd ~/opengl-with-jank
export PATH="$HOME/jank/compiler+runtime/build:$PATH"
export LIBRARY_PATH="$PWD/libs/linux-x86_64/enet/lib:$PWD/libs/linux-x86_64/cgltf/lib"
export LD_LIBRARY_PATH="$PWD/libs/linux-x86_64/enet/lib:$PWD/libs/linux-x86_64/cgltf/lib"

rm -rf .jank-cache  # Clear cache if switching platforms

jank \
  -I libs/glm -I include \
  -L libs/linux-x86_64/enet/lib -lenet \
  -L libs/linux-x86_64/cgltf/lib -lcgltf \
  --module-path src compile examples.demo.server -o demo-server

# Run server
./demo-server

# ============================================================================
# Compile game client (requires Mesa/GLEW for OpenGL)
# ============================================================================

# Install Mesa and GLEW (needed even on headless servers for compilation)
sudo apt-get install -y libosmesa6 libgl1-mesa-dri mesa-utils libglew-dev

# Add system library paths for OpenGL
export LIBRARY_PATH="$LIBRARY_PATH:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu"

jank \
  -I libs/glm -I include \
  -I libs/linux-x86_64/glfw/include \
  -I libs/linux-x86_64/ozz-animation/include \
  -L libs/linux-x86_64/enet/lib -lenet \
  -L libs/linux-x86_64/cgltf/lib -lcgltf \
  -L libs/linux-x86_64/glfw/lib -lglfw \
  -L libs/linux-x86_64/stb/lib -lstb_all \
  -L libs/linux-x86_64/ozz-animation/lib -lozz_animation_r -lozz_base_r -lozz_geometry_r \
  -L /lib/x86_64-linux-gnu -lGL -lX11 -lGLEW \
  --module-path src compile examples.demo.core -o demo

# Run client (requires display - won't work on headless server)
./demo
```
