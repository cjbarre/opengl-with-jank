# OpenGL with Jank

A 3D game engine and demo built with [Jank](https://jank-lang.org/) (a Clojure dialect) and OpenGL. This project serves as a learning vehicle for OpenGL, Jank's C++ interop, and game development.

> **Note:** JIT mode (`./bin/run`) works with standard Jank. AOT compilation (`./bin/compile`) currently requires a custom Jank build with library linking fixes not yet merged upstream.

## Quick Start

```bash
git clone --recursive <repo-url>
./bin/setup      # Build dependencies (ozz-animation, OpenGL stub)
./bin/run        # Run the demo in JIT mode
```

## Commands

| Command | Description |
|---------|-------------|
| `./bin/run` | Run demo in JIT mode |
| `./bin/run my-game` | Run a different example |
| `./bin/compile` | AOT compile to standalone executable |
| `./bin/build-dist` | Build distributable .app bundle (macOS) |

## Project Structure

```
src/
├── engine/              # Reusable game engine
│   ├── macros.jank      # clet macro for C-style error handling
│   ├── io/              # File I/O
│   ├── math/            # GLM math helpers
│   ├── shaders/         # Shader compilation
│   ├── geometry/        # Basic shapes
│   ├── textures/        # Texture loading (STB)
│   ├── events/          # Event store
│   ├── gltf/            # glTF model loading
│   └── 3d/              # 3D rendering, animation (ozz)
└── examples/
    └── demo/            # Demo game with 3D player, camera, networking

bin/                     # User commands (symlinks to scripts/)
scripts/                 # Build scripts
├── platform/            # Cross-platform abstraction
│   ├── common.sh        # Platform detection
│   ├── macos.sh         # macOS implementation
│   ├── linux.sh         # Linux stub (not yet supported)
│   └── windows.sh       # Windows stub (not yet supported)
├── run, compile, setup  # Main scripts
└── ...

libs/
├── macos-arm64/         # Platform-specific libraries
│   ├── glfw/, ozz-animation/, stb/, enet/, cgltf/, opengl/
├── linux-x86_64/        # Placeholder
├── windows-x86_64/      # Placeholder
└── glm/                 # Header-only math library
```

## Dependencies

- **Jank** - Clojure dialect with C++ interop
- **GLFW** - Window/input management
- **OpenGL 3.3+** - Graphics API
- **GLM** - Math library (header-only)
- **STB** - Image loading
- **cgltf** - glTF model parsing
- **ozz-animation** - Skeletal animation
- **enet** - UDP networking

## AOT Compilation

Build a standalone executable with bundled libraries:

```bash
./bin/compile            # Creates dist/bin/demo + dist/lib/
./dist/demo_run          # Run the compiled executable
```

Build a distributable macOS .app:

```bash
./bin/build-dist         # Creates dist/Demo.zip
```

**Note:** End users need XCode Command Line Tools (`xcode-select --install`).

## Creating a New Game

1. `mkdir -p src/examples/my-game`
2. Create `src/examples/my-game/core.jank` with namespace `examples.my-game.core`
3. Implement a `-main` function
4. `./bin/run my-game`

## Points of Interest

### [clet macro](src/engine/macros.jank)

A macro for C-style error handling that flattens nested conditionals:

```clojure
(clet [result (some-operation)
       :when (failed? result)
       :error (handle-error)])
```

### [C++ Interop Guide](CPP_INTEROP_DOCUMENTATION.md)

AI-generated guide to Jank's C++ interop patterns. May have inaccuracies but useful as a starting point.

## Platform Support

Currently macOS (Apple Silicon) only. The build system is prepared for cross-platform support but Linux/Windows implementations are not yet complete.

## License

This project is for learning purposes. Use as you see fit.
