# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

### Running the Application
```bash
./bin/run              # runs the default example (demo)
./bin/run demo         # runs examples.demo.core
./bin/run my-game      # runs examples.my-game.core
```

The `run` script compiles and executes a Jank game with OpenGL/GLFW support:
- Accepts game name as argument (defaults to `demo`)
- Uses platform abstraction layer for library paths
- Includes headers from libs and `include/`
- Uses Clojure classpath for module resolution

### AOT Compilation (Standalone Executable)
```bash
./bin/compile              # compiles demo to standalone executable
./bin/compile demo         # same as above
./bin/compile my-game      # compiles examples.my-game.core

# Run the compiled executable
./dist/demo_run            # launcher script with library paths
```

The `compile` script produces a self-contained distribution (~324MB):
```
dist/
├── bin/demo                    # 62MB Mach-O executable
├── lib/demo/                   # ~155MB bundled dylibs
├── lib/jank/0.1/               # ~113MB jank runtime resources
│   ├── bin/clang++             # Clang for JIT compilation
│   ├── include/                # Headers (libc++, jank)
│   ├── lib/clang/22/           # Clang resource directory
│   └── src/                    # jank stdlib
└── demo_run                    # Launcher script
```

**End-user requirement:** XCode Command Line Tools must be installed (`xcode-select --install`)

### Initial Setup

After cloning, run the setup script to build dependencies:

```bash
git clone --recursive ...
./bin/setup
```

### Development Dependencies
- **Jank**: The main language/runtime (with AOT library fix applied)
- **GLFW**: For window management and OpenGL context
- **Clojure**: For classpath and dependency management
- **STB Image**: C library for texture loading (included in `include/stb_image.h`)
- **ozz-animation**: Skeletal animation (submodule, built by `./setup`)
- **enet**: UDP networking for multiplayer

## Cross-Platform Support

### Platform Abstraction Layer

All build scripts use a platform abstraction layer in `scripts/platform/`:

```
scripts/
├── platform/
│   ├── common.sh     # Platform detection, loads platform-specific file
│   ├── macos.sh      # Full macOS implementation
│   ├── linux.sh      # Stub (not yet supported)
│   └── windows.sh    # Stub (not yet supported)
├── setup             # Build dependencies
├── run               # Run in JIT mode
├── compile           # AOT compile to executable
├── compile-server    # Compile headless server
├── package-client    # Create .app bundle
└── build-dist        # Full distribution build
```

`bin/` contains symlinks (`run`, `compile`, `setup`, `build-dist`) pointing to `scripts/` for convenience.

Scripts source the abstraction with:
```bash
source "$SCRIPT_DIR/platform/common.sh"
require_platform_support  # Fails if platform unsupported
load_jank_paths            # Loads JANK_DIR, JANK_LLVM paths
LIBS_DIR="$(get_libs_dir)" # Platform-specific libs directory
```

### Key Platform Functions

| Function | Description |
|----------|-------------|
| `get_lib_extension()` | `dylib` (macOS), `so` (Linux), `dll` (Windows) |
| `get_lib_prefix()` | `lib` (macOS/Linux), empty (Windows) |
| `format_lib_name name` | Formats library name for platform |
| `get_libs_dir()` | Returns `libs/macos-arm64/`, etc. |
| `get_homebrew_prefix()` | `/opt/homebrew` (arm64), `/usr/local` (x86) |
| `fix_lib_install_name lib` | Fixes library ID to use @rpath |
| `add_rpath rpath executable` | Adds rpath to executable |
| `delete_rpath rpath executable` | Removes rpath from executable |
| `change_lib_path binary old new` | Changes library reference |
| `bundle_system_lib lib dest` | Copies and fixes system library |
| `code_sign_adhoc path` | Ad-hoc signs binary |
| `create_app_bundle path name` | Creates .app bundle structure |
| `generate_launcher_script path exec libs` | Creates launcher script |

### Library Directory Structure

Platform-specific libraries are organized under `libs/{platform}/`:

```
libs/
├── macos-arm64/           # macOS Apple Silicon libraries
│   ├── glfw/
│   ├── ozz-animation/
│   ├── stb/
│   ├── enet/
│   ├── cgltf/
│   └── opengl/
├── linux-x86_64/          # Linux libraries (placeholder)
├── windows-x86_64/        # Windows libraries (placeholder)
├── glm/                   # Header-only (shared across platforms)
└── include/               # Shared headers (placeholder)
```

### Adding a New Platform

1. Create `scripts/platform/{platform}.sh` implementing all functions
2. Update `detect_platform()` in `scripts/platform/common.sh` to detect the platform
3. Create `libs/{platform}/` and build/copy libraries
4. Test with `./bin/setup && ./bin/run`

## Architecture

### Project Structure
This is a **Jank + OpenGL** game engine with example games:

```
src/
├── engine/                 # Reusable engine code
│   ├── macros.jank         # C-style error handling macro (clet)
│   ├── io/                 # File I/O utilities
│   ├── math/               # GLM math helpers and macros
│   ├── shaders/            # Shader compilation and program management
│   ├── geometry/           # Vertex data and buffer management
│   ├── textures/           # Texture loading and binding
│   ├── events/             # Event store for game state
│   └── gltf/               # glTF model loading
└── examples/               # Example games
    └── demo/               # Demo game with 3D player and camera
        └── core.jank       # Game entry point
```

### Creating a New Game
1. Create directory: `mkdir -p src/examples/my-game`
2. Create `src/examples/my-game/core.jank` with namespace `examples.my-game.core`
3. Implement `-main` function (zero-arity supported)
4. Run with: `./run my-game`

### Core Architecture Components

#### 1. Engine Modules (`engine.*`)
- **engine.macros**: `clet` macro for C-style error handling
- **engine.io**: File reading utilities
- **engine.math**: GLM wrappers (`gimmie`, `*->` macros)
- **engine.shaders**: Shader compilation, VAO management
- **engine.geometry**: Basic shapes (rectangles, cubes)
- **engine.textures**: STB Image integration
- **engine.events**: Event sourcing for game state
- **engine.gltf**: glTF model parsing and loading

#### 2. C++ Interop Pattern
The codebase extensively uses **Jank's C++ interop** (`cpp/` forms) for direct OpenGL API calls:
- `cpp/raw` for embedding C/C++ code and function definitions
- `cpp/` prefixed calls for OpenGL functions (e.g., `cpp/glUseProgram`)
- `cpp/box`/`cpp/unbox` for managing C++ pointers in Jank
- `cpp/&` for address-of operations
- `cpp/cast` and `cpp/type` for type conversions

#### 3. Error Handling with `clet`
Custom macro `clet` provides C-style error checking:
```clojure
(clet [result (some-operation)
       :when (failed? result)
       :error (handle-error)])
```

#### 4. Namespace Organization
- **Interface/Core Split**: Each engine module has `interface.jank` (public API) and `core.jank` (implementation)
- **Pure Functions**: Most functions are stateless transformations
- **Resource Management**: Explicit OpenGL resource creation/binding

### Key Technical Details

#### Shader System (`engine.shaders`)
- Compiles GLSL shaders from file paths
- Links shader programs with error checking
- Manages VAO creation and binding
- Uses C wrapper functions for OpenGL extensions

#### Geometry System (`engine.geometry`)
- Defines vertex data as C arrays using `cpp/raw`
- Creates and configures OpenGL buffers (VBO/EBO)
- Sets up vertex attribute pointers for position, color, and texture coordinates
- Returns structured data with buffer IDs and metadata

#### Texture System (`engine.textures`)
- Loads image files using STB Image library
- Handles both RGB and RGBA formats
- Manages OpenGL texture creation and configuration
- Binds textures to shader uniforms

#### glTF System (`engine.gltf`)
- Parses glTF files using cgltf
- Loads meshes with positions, normals, UVs
- Supports PBR materials and textures
- Configurable `base-path` for asset loading

#### I/O System (`engine.io`)
- File reading utilities for shaders and textures
- Memory management for loaded data
- Cross-platform file path handling

#### Event System (`engine.events`)
- Atom-based event store
- Event filtering by tags/types
- Compare-and-swap (CAS) support

### Dependencies and Libraries

#### Required Libraries
- **GLFW 3.x**: Window/context management (dylib in `libs/{platform}/glfw/`)
- **OpenGL 3.3+**: Core graphics API (stub dylib in `libs/{platform}/opengl/`)
- **GLM**: Math library for vectors/matrices (header-only in `libs/glm/`)
- **STB Image**: Image loading (`include/stb_image.h`, dylib in `libs/{platform}/stb/`)
- **cgltf**: glTF parser (`include/cgltf.h`, dylib in `libs/{platform}/cgltf/`)
- **ozz-animation**: Skeletal animation (submodule in `third_party/`, dylib in `libs/{platform}/ozz-animation/`)
- **enet**: UDP networking library (dylib in `libs/{platform}/enet/`)

#### Build System
- Uses Jank compiler with custom flags for includes and linking
- Platform abstraction layer (`platform/`) for cross-platform support
- Clojure for dependency resolution (`deps.edn` with paths only)
- Shell scripts with error handling and platform detection

### Development Notes

#### Working with Shaders
- Vertex shaders: `shaders/basic_vertex.glsl`
- Fragment shaders: `shaders/basic_fragment.glsl`
- GLSL version 330 core profile
- Attributes: position (location 0), color (location 1), texture coords (location 2)

#### Memory Management
- Manual memory management for C interop (malloc/free)
- OpenGL resource cleanup handled explicitly
- Pointer boxing/unboxing for Jank<->C++ integration

#### Texture Assets
- Textures stored in `textures/` directory
- Models and their textures in `models/` directory
- Supports common formats (JPG, PNG)
- Alpha channel handling for RGBA textures

## AOT Compilation Details

### Overview

Jank supports ahead-of-time (AOT) compilation to produce standalone executables. This project uses AOT compilation to create distributable game binaries that don't require the Jank runtime to be installed.

### Library Setup

AOT compilation requires dynamic libraries for external dependencies. These are organized under `libs/{platform}/` (see [Cross-Platform Support](#cross-platform-support) for structure).

For macOS (arm64), libraries are in `libs/macos-arm64/`:

```
libs/macos-arm64/
├── glfw/
│   ├── include/          # GLFW headers
│   └── lib/
│       └── libglfw.3.dylib
├── ozz-animation/
│   ├── include/          # ozz headers
│   └── lib/
│       ├── libozz_animation_r.dylib
│       ├── libozz_base_r.dylib
│       └── libozz_geometry_r.dylib
├── stb/
│   └── lib/
│       └── libstb_all.dylib
├── enet/
│   └── lib/
│       └── libenet.dylib
├── cgltf/
│   └── lib/
│       └── libcgltf.dylib
└── opengl/
    └── lib/
        └── libOpenGL.dylib    # Stub library linking to OpenGL.framework
```

### Library Install Names

macOS dynamic libraries have "install names" that get baked into executables at link time. For proper runtime loading with `@rpath`, libraries must have install names like `@rpath/libfoo.dylib`:

```bash
# Check a library's install name (use platform functions instead when possible)
otool -D libs/macos-arm64/stb/lib/libstb_all.dylib

# Fix install name using platform function
source platform/common.sh
fix_lib_install_name libs/macos-arm64/stb/lib/libstb_all.dylib
```

### Building Custom Libraries

#### STB (header-only to dylib)

```bash
# Create implementation file
cat > stb_impl.c << 'EOF'
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
EOF

# Compile to dylib (macOS)
clang -dynamiclib -o libs/macos-arm64/stb/lib/libstb_all.dylib stb_impl.c \
  -Iinclude -install_name "@rpath/libstb_all.dylib"
```

#### cgltf (header-only to dylib)

```bash
cat > cgltf_impl.c << 'EOF'
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
EOF

# Compile to dylib (macOS)
clang -dynamiclib -o libs/macos-arm64/cgltf/lib/libcgltf.dylib cgltf_impl.c \
  -Iinclude -install_name "@rpath/libcgltf.dylib"
```

#### ozz-animation (from submodule)

ozz-animation is built automatically by `./setup`. If you need to rebuild manually:

```bash
cd third_party/ozz-animation/build
make -j8
# Copy to platform-specific location
cp src/base/libozz_base_r.dylib ../../../libs/macos-arm64/ozz-animation/lib/
cp src/animation/runtime/libozz_animation_r.dylib ../../../libs/macos-arm64/ozz-animation/lib/
cp src/geometry/runtime/libozz_geometry_r.dylib ../../../libs/macos-arm64/ozz-animation/lib/
```

### Compile Script Workflow

The `./compile` script:

1. Sets `LIBRARY_PATH` so the linker finds libraries
2. Invokes `jank compile` with:
   - Include paths (`-I`)
   - Library paths (`-L`)
   - Libraries to link (`-l`)
   - Module path from Clojure
3. Creates self-contained distribution structure:
   - Moves executable to `dist/bin/`
   - Bundles dylibs into `dist/lib/{output}/`
   - Bundles jank runtime resources into `dist/lib/jank/0.1/`
   - Creates launcher script
4. Fixes rpaths so all libraries are found relative to executable

### Distribution

To distribute a compiled game:

```bash
# Copy the entire dist/ directory:
dist/
├── bin/demo                # The executable
├── lib/demo/               # Game dylibs
├── lib/jank/0.1/           # Jank runtime (clang, headers, stdlib)
├── demo_run                # Launcher script
# Plus your game assets (copy to dist/):
├── shaders/                # GLSL shaders (loaded at runtime)
├── textures/               # Texture assets
└── models/                 # 3D models
```

**End-user requirements:**
- macOS (arm64)
- XCode Command Line Tools (`xcode-select --install`)

The launcher script sets `DYLD_LIBRARY_PATH` to find the bundled libraries.

### Jank AOT Requirements

This project requires jank with the AOT library linking fix. The fix adds:

1. **User library passing** - `-l` flags are passed to the linker
2. **macOS JIT library naming** - Uses `lib{name}.dylib` format
3. **macOS framework linking** - Links OpenGL, Cocoa, IOKit, CoreFoundation

See `/Users/cam/Documents/code/jank/jank-aot-library-fix.md` for details.

### Troubleshooting

**"Library not loaded" errors:**
- Check library install names: `otool -D libfoo.dylib`
- Verify rpath in executable: `otool -l dist/bin/demo | grep -A2 LC_RPATH`
- Use launcher script or set `DYLD_LIBRARY_PATH`

**"Unable to find a suitable Clang 22 binary" or clang symbol errors:**
- Ensure `dist/lib/jank/0.1/bin/clang++` exists and is executable
- Verify clang can find dylibs: `otool -L dist/lib/jank/0.1/bin/clang-22`
- Check that clang rpath points to dylib directory

**"Unable to find 'xcrun' binary" errors:**
- End user needs XCode Command Line Tools: `xcode-select --install`

**Undefined symbols during linking:**
- Ensure library is in `-L` path
- Ensure `-l` flag uses correct name (without `lib` prefix and `.dylib` suffix)
- Check if library exports the symbol: `nm -gU libfoo.dylib | grep symbol_name`

**Large distribution size (~324MB):**
- Normal - includes bundled clang toolchain for JIT compilation at runtime
- Executable itself is ~62MB (LLVM/Clang embedded in jank runtime)
- Additional ~113MB for clang resource directory and headers

## Headless Server Build

### Overview

For dedicated server deployment, use `./compile-server` which builds a headless binary without graphics dependencies (GLFW, OpenGL, STB textures, ozz-animation).

### Server Compilation

```bash
./compile-server              # compiles examples.demo.server
./compile-server demo         # same as above
./compile-server my-game      # compiles examples.my-game.server

# Run the compiled server
./dist/demo-server_run
```

**Output structure (same as client, with fewer dylibs):**
```
dist/
├── bin/demo-server           # 62MB headless server executable
├── lib/demo-server/          # Server dylibs (enet, cgltf, LLVM/clang)
├── lib/jank/0.1/             # Jank runtime (shared with client)
└── demo-server_run           # Launcher script
```

### Server Dependencies

The server only requires:
- **enet** - UDP networking
- **cgltf** - glTF loading for collision meshes
- **GLM** - Math library (header-only)

It does NOT require:
- GLFW (windowing)
- OpenGL (rendering)
- STB (texture loading)
- ozz-animation (skeletal animation)

### Headless glTF Loader

The server uses `engine.3d.gltf.headless` instead of the full glTF loader:

```clojure
(require '[engine.3d.gltf.headless :as gltf])

;; Load only collision mesh data (no GPU operations)
(gltf/load-collision {:path "models/level.gltf"})
;; => [{:name "collision-mesh" :positions [...] :indices [...]}]
```
