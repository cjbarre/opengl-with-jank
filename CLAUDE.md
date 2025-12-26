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

### Development Dependencies
- **Jank**: The main language/runtime
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
