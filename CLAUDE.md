# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

### Running the Application
```bash
./run              # runs the default example (demo)
./run demo         # runs examples.demo.core
./run my-game      # runs examples.my-game.core
```

The `run` script compiles and executes a Jank game with OpenGL/GLFW support:
- Accepts game name as argument (defaults to `demo`)
- Links against GLFW library (`libs/glfw/lib/libglfw.3.dylib`)
- Includes headers from `libs/glfw/include` and `include/`
- Uses Clojure classpath for module resolution

### AOT Compilation (Standalone Executable)
```bash
./compile              # compiles demo to standalone executable
./compile demo         # same as above
./compile my-game      # compiles examples.my-game.core

# Run the compiled executable
./demo_run             # launcher script with library paths
```

The `compile` script produces:
- `demo` - 62MB standalone Mach-O executable
- `demo_run` - Launcher script that sets DYLD_LIBRARY_PATH
- `demo_libs/` - Bundled dynamic libraries

### Development Dependencies
- **Jank**: The main language/runtime (with AOT library fix applied)
- **GLFW**: For window management and OpenGL context
- **Clojure**: For classpath and dependency management
- **STB Image**: C library for texture loading (included in `include/stb_image.h`)

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
- **GLFW 3.x**: Window/context management (dylib in `libs/glfw/`)
- **OpenGL 3.3+**: Core graphics API
- **GLM**: Math library for vectors/matrices
- **STB Image**: Header-only image loading (`include/stb_image.h`)
- **cgltf**: Header-only glTF parser (`include/cgltf.h`)

#### Build System
- Uses Jank compiler with custom flags for includes and linking
- Clojure for dependency resolution (`deps.edn` with paths only)
- Simple shell script runner with error handling

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

AOT compilation requires dynamic libraries (`.dylib` on macOS) for external dependencies. These are organized under `libs/`:

```
libs/
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
└── cgltf/
    └── lib/
        └── libcgltf.dylib
```

### Library Install Names

macOS dynamic libraries have "install names" that get baked into executables at link time. For proper runtime loading with `@rpath`, libraries must have install names like `@rpath/libfoo.dylib`:

```bash
# Check a library's install name
otool -D libs/stb/lib/libstb_all.dylib

# Fix install name if needed
install_name_tool -id "@rpath/libstb_all.dylib" libs/stb/lib/libstb_all.dylib
```

### Building Custom Libraries

#### STB (header-only to dylib)

```bash
# Create implementation file
cat > stb_impl.c << 'EOF'
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
EOF

# Compile to dylib
clang -dynamiclib -o libs/stb/lib/libstb_all.dylib stb_impl.c \
  -Iinclude -install_name "@rpath/libstb_all.dylib"
```

#### cgltf (header-only to dylib)

```bash
cat > cgltf_impl.c << 'EOF'
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
EOF

clang -dynamiclib -o libs/cgltf/lib/libcgltf.dylib cgltf_impl.c \
  -Iinclude -install_name "@rpath/libcgltf.dylib"
```

### Compile Script Workflow

The `./compile` script:

1. Sets `LIBRARY_PATH` so the linker finds libraries
2. Invokes `jank compile` with:
   - Include paths (`-I`)
   - Library paths (`-L`)
   - Libraries to link (`-l`)
   - Module path from Clojure
3. Bundles required dylibs into `{output}_libs/`
4. Creates launcher script `{output}_run`

### Distribution

To distribute a compiled game:

```bash
# These files/directories are needed:
demo              # The executable
demo_run          # Launcher script
demo_libs/        # Bundled libraries
shaders/          # GLSL shaders (loaded at runtime)
textures/         # Texture assets
models/           # 3D models
```

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
- Verify rpath in executable: `otool -l demo | grep -A2 LC_RPATH`
- Use launcher script or set `DYLD_LIBRARY_PATH`

**Undefined symbols during linking:**
- Ensure library is in `-L` path
- Ensure `-l` flag uses correct name (without `lib` prefix and `.dylib` suffix)
- Check if library exports the symbol: `nm -gU libfoo.dylib | grep symbol_name`

**Large executable size (~62MB):**
- Normal - includes LLVM/Clang for JIT capabilities embedded in jank runtime

## Headless Server Build

### Overview

For dedicated server deployment, use `./compile-server` which builds a headless binary without graphics dependencies (GLFW, OpenGL, STB textures, ozz-animation).

### Server Compilation

```bash
./compile-server              # compiles examples.demo.server
./compile-server demo         # same as above
./compile-server my-game      # compiles examples.my-game.server

# Run the compiled server
./demo-server_run
```

**Output:**
- `demo-server` - 62MB headless server executable
- `demo-server_run` - Launcher script
- `demo-server_libs/` - Bundled libraries (enet, cgltf only)

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
