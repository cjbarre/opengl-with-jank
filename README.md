# OpenGL with Jank

A jank/OpenGL game engine and Strafe Combat Academy, a game with networked multiplayer, skeletal animation, and Quake-style movement.

[Demo video](https://youtu.be/hVQB7G6YVKQ)

## Development

Install jank, Leiningen, Babashka, CMake, a C/C++ compiler, pkg-config, GLFW, and OpenGL. Linux also requires GLEW and bubblewrap. On macOS, the additional tools can be installed with `brew install leiningen borkdude/brew/babashka cmake pkgconf glfw python`.

```bash
git submodule update --init --recursive
cd engine
lein install
cd ../game
export JANK_REAL="${JANK_REAL:-$(command -v jank)}"
export PATH="$PWD/../scripts/eager:$PATH"
lein run -- server
# In another terminal, from game/:
lein run -- client
lein run -- editor
lein run -- viewer
```

Set these environment variables in each development terminal before running a mode. `JANK_REAL` records the installed compiler before the wrapper enters `PATH`. The wrapper adds `--eagerness eager`, compiling functions before gameplay to avoid pauses on first use. Remove it once lein-jank supports eagerness in `project.clj`.

Reinstall the engine package after changing engine sources. Lein-jank manages native dependency builds and caches. The game uses `project.clj` and `jank-build.bb` for source paths, dependencies, and include paths.

If Xcode tool shims fail inside the native-build sandbox, select Command Line Tools:

```bash
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
```

## Release

From `game/`:

```bash
lein with-profile release compile    # target/release/sca
./scripts/package                    # dist/sca, including runtime libraries and assets
./dist/sca/sca_run server
./dist/sca/sca_run client
```

The package script runs release compilation and assembles `dist/sca`. Packaging needs Python 3 and, on Linux, patchelf. The release uses jank's static runtime and runs without an installed jank compiler or LLVM toolchain. OS graphics drivers are supplied by the target machine.

## Build layout

- `engine/project.clj`: reusable engine source package; commons OpenGL/GLFW dependencies.
- `engine/jank-build.bb` and `engine/native/CMakeLists.txt`: ozz, STB, cgltf, ENet, GLEW discovery, and installed headers. GLM remains a source submodule.
- `engine/scripts/embed-assets.clj`: shader/font header generation.
- `scripts/eager/jank`: temporary development workaround for lein-jank’s missing eagerness setting.
- `game/project.clj`: development and release settings, with `sca.core` as the common entry point.
- `game/scripts/package` and `bundle-native.py`: release asset copying, native-library copying, loader paths, and macOS signing.

## Modes

| Mode | Purpose |
| --- | --- |
| `client [host]` | Join a server; defaults to localhost |
| `server` | Host on UDP/7777 |
| `editor` | Course designer |
| `viewer` | Animation viewer |
| `net-test server` / `net-test client` | Network echo test |

## Features

- **Networking** — Server-authoritative architecture with client-side prediction and snapshot interpolation. ENet UDP transport.
- **Animation** — ozz-animation runtime with GPU skinning, skeletal line rendering, behavior-tree state machine over 50+ states.
- **Physics** — Quake-style movement (friction, accel, air control, force-jump). Slope normals.
- **Collision** — Raycast against glTF collision meshes.
- **Behavior trees** — Vector DSL for game logic.
- **Course editor** — Place / resize brushes, save/load `.edn` and JKA-compatible `.map`.
- **Debug overlays** — F3 (FPS, position, velocity), F4 CGaz strafehelper.

## Verification

```bash
JANK_REAL="${JANK_REAL:-$(command -v jank)}" PATH="$PWD/scripts/eager:$PATH" \
  python3 scripts/smoke-test.py --cwd game -- lein run --
JANK_REAL="${JANK_REAL:-$(command -v jank)}" PATH="$PWD/scripts/eager:$PATH" \
  python3 scripts/smoke-test.py --graphics --cwd game -- lein run --
```

Graphical tests require a server connection, 120 completed frames, and a clean client exit. CI runs development tests and tests relocated releases with jank absent from `PATH`. Linux graphical tests run under Xvfb; macOS CI uses network tests. See [distribution details](engine/docs/bake-distribution.md).
