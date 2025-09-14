# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

### Running the Application
```bash
./run
```

The `run` script compiles and executes the Jank application with OpenGL/GLFW support:
- Links against GLFW library (`libs/glfw/lib/libglfw.3.dylib`)
- Includes headers from `libs/glfw/include` and `include/`
- Uses Clojure classpath for module resolution
- Executes `app.core` as the main entry point

### Development Dependencies
- **Jank**: The main language/runtime
- **GLFW**: For window management and OpenGL context
- **Clojure**: For classpath and dependency management
- **STB Image**: C library for texture loading (included in `include/stb_image.h`)

## Architecture

### Project Structure
This is a **Jank + OpenGL** graphics application with the following modular structure:

```
src/app/
├── core.jank           # Main application entry point and render loop
├── clet.jank           # C-style error handling macro
├── geometry/           # Vertex data and buffer management
├── shaders/            # Shader compilation and program management
├── textures/           # Texture loading and binding
└── io/                 # File I/O utilities
```

### Core Architecture Components

#### 1. Main Application Flow (`app.core`)
- **Window Setup**: GLFW window creation with OpenGL 3.3 core profile
- **Shader Loading**: Loads vertex/fragment shaders from `shaders/` directory
- **Geometry Setup**: Creates textured rectangle with VAO/VBO/EBO
- **Texture Loading**: Loads and binds multiple textures
- **Render Loop**: Standard OpenGL render loop with input processing

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
- **Interface/Core Split**: Each module has `interface.jank` (public API) and `core.jank` (implementation)
- **Pure Functions**: Most functions are stateless transformations
- **Resource Management**: Explicit OpenGL resource creation/binding

### Key Technical Details

#### Shader System (`app.shaders`)
- Compiles GLSL shaders from file paths
- Links shader programs with error checking
- Manages VAO creation and binding
- Uses C wrapper functions for OpenGL extensions

#### Geometry System (`app.geometry`)
- Defines vertex data as C arrays using `cpp/raw`
- Creates and configures OpenGL buffers (VBO/EBO)
- Sets up vertex attribute pointers for position, color, and texture coordinates
- Returns structured data with buffer IDs and metadata

#### Texture System (`app.textures`)
- Loads image files using STB Image library
- Handles both RGB and RGBA formats
- Manages OpenGL texture creation and configuration
- Binds textures to shader uniforms

#### I/O System (`app.io`)
- File reading utilities for shaders and textures
- Memory management for loaded data
- Cross-platform file path handling

### Dependencies and Libraries

#### Required Libraries
- **GLFW 3.x**: Window/context management (dylib in `libs/glfw/`)
- **OpenGL 3.3+**: Core graphics API
- **STB Image**: Header-only image loading (`include/stb_image.h`)

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
- Supports common formats (JPG, PNG)
- Alpha channel handling for RGBA textures