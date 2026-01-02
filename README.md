# OpenGL with Jank

A 3D game engine and multiplayer demo built with [Jank](https://jank-lang.org/) (a Clojure dialect) and OpenGL. Features networked client-server gameplay, skeletal animation, and Quake-style movement physics.

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
| `./bin/compile-server` | Compile headless dedicated server |
| `./bin/build-dist` | Build distributable .app bundle (macOS) |
| `./bin/package-client` | Create macOS .app bundle |
| `./bin/build-linux` | Cross-compile client for Linux (via SSH) |
| `./bin/build-linux-server` | Cross-compile server for Linux |
| `./bin/build-gla2ozz` | Build GLA→ozz animation converter |
| `./bin/build-ozz2gltf` | Build ozz→glTF skeleton exporter |

## Project Structure

```
src/
├── engine/                  # Reusable game engine
│   ├── 2d/                  # 2D graphics and text rendering
│   ├── 3d/                  # 3D rendering subsystems
│   │   ├── animation/       # ozz-animation integration, skinning
│   │   ├── collision/       # Raycast ground detection
│   │   ├── geometry/        # Shapes, VBO/EBO management
│   │   ├── gltf/            # glTF loading (+ headless for server)
│   │   ├── lines/           # Debug line rendering
│   │   └── textures/        # Texture loading (STB)
│   ├── behavior-tree/       # AI/game logic DSL
│   ├── events/              # Event sourcing
│   ├── gl/                  # Low-level OpenGL wrappers
│   ├── io/                  # File I/O utilities
│   ├── math/                # GLM helpers
│   ├── networking/          # ENet UDP multiplayer
│   ├── shaders/             # Shader compilation
│   └── macros.jank          # clet macro for C-style error handling
└── examples/
    └── demo/                # Multiplayer demo game
        ├── core.jank        # Standalone entry point
        ├── client.jank      # Networked client with prediction
        ├── server.jank      # Authoritative server (60 tick)
        ├── shared.jank      # Shared physics (Quake-style)
        ├── camera.jank      # Third-person camera
        ├── animation/       # Animation state machine
        ├── networking/      # Prediction, interpolation, snapshots
        └── strafehelper/    # CGaz-style velocity HUD

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

## Features

**Networking** - Server-authoritative architecture with client-side prediction and snapshot interpolation for smooth multiplayer gameplay.

**Animation** - ozz-animation integration with GPU skinning, skeletal line rendering, and behavior-tree-driven state machine supporting 50+ animation states.

**Physics** - Quake-style movement with friction, acceleration, air control, and force jump mechanics.

**Collision** - Raycast ground detection against glTF collision meshes.

**Behavior Trees** - Vector DSL for AI and game logic with sequence, fallback, condition, and action nodes.

**Debug Tools** - F3 overlay (FPS, position, velocity, speed) and F4 CGaz-style strafehelper HUD.

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

## Animation Tools

Custom C++ tools in `tools/` for animation pipeline work:

- **gla2ozz** - Convert Quake 3/JKA GLA skeletal animations to ozz format
- **ozz2gltf** - Export ozz skeletons and animations to glTF for visualization
- **ozz-retarget** - Retarget animations between different skeleton rigs

Build with `./bin/build-gla2ozz`, `./bin/build-ozz2gltf`, etc.

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

**macOS (Apple Silicon)** - Primary development platform with full support.

**Linux** - Cross-compilation via SSH to a Linux VM (`./bin/build-linux`, `./bin/build-linux-server`). Produces standalone distributions with bundled glibc.

**Windows** - Build system stubs exist but not yet implemented.

## License

This project is for learning purposes. Use as you see fit.
