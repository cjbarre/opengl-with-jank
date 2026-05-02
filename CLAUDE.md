# CLAUDE.md

Guidance for Claude Code working in this repository.

## Layout

```
opengl-with-jank/
├── engine/               # Reusable jank+OpenGL runtime
│   ├── deps.edn          ; {:paths ["src"]}
│   ├── src/engine/       ; 16 jank namespaces
│   ├── include/          ; engine *_impl.h + bundled third-party (cgltf, enet, stb_*, gl_*, ozz_mesh, animation_types)
│   ├── assets/           ; shaders/, fonts/ — embedded into the engine binary
│   ├── scripts/          ; setup, build-engine, asset pipeline, platform/
│   ├── third_party/      ; ozz-animation submodule, tinygltf
│   ├── libs/             ; glm submodule + per-platform native libs (macos-arm64/, linux-arm64/, …)
│   ├── tools/            ; gla2ozz, ozz2gltf, ozz-retarget (C++ asset pipeline)
│   ├── docs/
│   └── build/  dist/  target/   ; gitignored
│
├── game/                 # Strafe Combat Academy
│   ├── deps.edn          ; {:paths ["src"]}
│   ├── jank-engine.edn   ; {:entry sca.core :paths ["src"] :includes ["include"]}
│   ├── src/sca/          ; client, server, editor/, viewer, networking/, …
│   ├── include/sca/      ; game *_impl.h
│   └── models/  textures/  course.edn  output.map
│
├── CLAUDE.md  README.md  CPP_INTEROP_DOCUMENTATION.md
└── .gitmodules
```

The two trees are independent: no symlinks between them, no cross-references. The engine knows nothing about the game; the game references the engine only by invoking the `jank-engine` binary.

## Commands

```bash
cd engine
./scripts/setup           # build native deps + libengine_assets
./scripts/build-engine    # produces dist/jank-engine/jank-engine_run

cd ../game
../engine/dist/jank-engine/jank-engine_run . server          # host
../engine/dist/jank-engine/jank-engine_run . client [ip]     # join
../engine/dist/jank-engine/jank-engine_run . editor          # course designer
../engine/dist/jank-engine/jank-engine_run . viewer          # animation viewer
../engine/dist/jank-engine/jank-engine_run . net-test server # ENet smoke
```

`jank-engine_run` arguments:
- arg 1: game directory (contains `jank-engine.edn`)
- arg 2+: passed to the entry namespace's `-main`

## Runtime model

`jank-engine` is a single AOT-compiled binary that bakes in every `engine.*` namespace plus all native deps (GLFW, ozz, ENet, STB, cgltf, GLM headers, libc++, embedded clang for JIT). At startup it:

1. Resolves `<bin>/../include` via `_NSGetExecutablePath` and adds it to `Cpp::AddIncludePath` so consumer `cpp/raw` blocks find glm/GLFW/ozz/engine `*_impl.h`.
2. Reads `<game-dir>/jank-engine.edn` (`:entry`, `:paths`, `:includes`).
3. `chdir`s into the game directory so asset paths resolve relative to it.
4. Adds the game's `:paths` to the jank module loader and its `:includes` to clang.
5. `(require ...)` the entry namespace and invokes its `-main`.

Engine assets (shaders/fonts) are embedded into `libengine_assets.dylib` at engine-build time and registered into jank's static `aot::resource` registry by a top-level form in `engine.resources.core`. Consumers access them through engine helpers (see "Resource registry" below) — the game CWD does **not** need to contain a `shaders/` or `fonts/` directory.

## Engine modules (`engine.*`)

| Namespace | Purpose |
|---|---|
| `engine.macros` | `clet` macro for C-style error handling |
| `engine.io` | File reads |
| `engine.math` | GLM wrappers (`gimmie`, `*->`) |
| `engine.shaders` | Shader/program compilation, VAOs, default-* helpers |
| `engine.gl` | Low-level OpenGL state + constants |
| `engine.gc` | BDWGC incremental control for frame budgets |
| `engine.events` | Atom-based event store |
| `engine.networking` | ENet UDP client/server, packet send/recv, polling |
| `engine.resources` | Static resource registry init |
| `engine.runtime` | The runtime binary's `-main` (binary entry) |
| `engine.gfx2d.graphics` | 2D primitives (lines, arcs, filled) |
| `engine.gfx2d.text` | STB TrueType font rendering |
| `engine.gfx3d.geometry` | Vertex data, VBO/EBO setup |
| `engine.gfx3d.textures` | STB Image |
| `engine.gfx3d.gltf` | cgltf parsing (+ `.headless` for server) |
| `engine.gfx3d.animation` | ozz integration, skinning |
| `engine.gfx3d.collision` | Raycast ground detection |
| `engine.gfx3d.lines` | Debug line rendering |
| `engine.behavior-tree` | Vector DSL for AI/game logic |

Each module uses an interface/core split: `interface.jank` (public API) and `core.jank` (impl).

## Adding a new game

1. Create a new directory next to `game/` with this minimum:
   ```
   my-game/
   ├── jank-engine.edn   ; {:entry my-game.core :paths ["src"] :includes ["include"]}
   └── src/my-game/core.jank
   ```
2. In `core.jank`, define `(ns my-game.core)` and a `-main` function (zero-arity OK).
3. Run with `<engine-dist>/jank-engine_run /path/to/my-game [args...]`.

The game gets all engine namespaces by `(:require [engine.shaders.interface :as shaders] ...)` etc. — no engine paths or build flags to know about.

## C++ interop conventions

The codebase uses Jank's C++ interop heavily for OpenGL, GLFW, glm, ozz, ENet:

- `cpp/raw "..."` — embed C/C++ code (function defs, `#include` lines)
- `cpp/foo` — call C/C++ function `foo`
- `cpp/box` / `cpp/unbox` — manage C++ pointers from jank
- `cpp/&` — address-of
- `cpp/cast` with type DSL — e.g. `(cpp/cast (:* void) data)`
- Type DSL: `(:* T)` pointer, `(std.vector float)` template, `(#cpp (:unsigned int))` value-init
- `#cpp` reader tag — access C++ values, e.g. `#cpp SEEK_END`

### cpp/raw name collisions

C++ functions defined in `cpp/raw` blocks must not share names with jank `defn`s in the same namespace **after hyphen-to-underscore munging**. Use `_impl` or `_helper` suffixes for the C++ side.

### `extern "C"` is unreliable in AOT mode

Use `static` linkage in `cpp/raw` blocks for helpers consumed only by that translation unit. `extern "C"` symbols defined in jank source are not always visible to the AOT linker.

### Static-init-order in AOT

Don't use `__attribute__((constructor))` in `cpp/raw` blocks if the constructor depends on jank runtime globals (e.g. the resource registry). dyld may run your constructor before jank's globals are constructed → `std::overflow_error: __next_prime overflow` or similar. Instead, expose a normal `static` function and call it from a top-level `(cpp/...)` form in your namespace — it runs as part of namespace init, after the runtime is up.

## `clet` macro

Custom macro for early-return on failure:

```clojure
(clet [result (some-operation)
       :when (failed? result)
       :error (handle-error)
       next-step (something-else result)]
  (use-it next-step))
```

`:when` + `:error` form a guard pair: if `:when` is truthy, evaluate `:error` and short-circuit. Defined in `engine/src/engine/macros.jank`.

## Resource registry

Engine-shipped assets (shaders, fonts) live in `engine/assets/` and are baked into `libengine_assets.dylib` by `engine/scripts/embed-assets.clj`. The dylib has a `engine_register_resources()` function called from a top-level form in `engine.resources.core`, which populates jank's `aot::find_resource` registry.

To consume engine-shipped shaders/fonts from a game:

```clojure
(shaders/basic)        ; basic vertex+fragment
(shaders/line)         ; vertex+geometry+fragment
(shaders/text)
(shaders/graphics2d)
(shaders/skinned)

(text/font 20.0 text-shader)
```

Don't call `(shaders/load-shader-program {:vertex-shader-path "..."})` from a game — it `fopen`s relative to CWD and engine assets aren't on disk in the game's CWD. The named helpers above route through the registry. Each call compiles+links a fresh GL program (not memoized) — call once at init and hold the returned ID.

## Native dependencies

Tracked under `engine/libs/{platform}/{libname}/lib/` and `engine/libs/{platform}/{libname}/include/`:

| Lib | Purpose |
|---|---|
| GLFW | Windowing |
| OpenGL (stub) | Linker stub on macOS; `OpenGL.framework` at runtime |
| GLM | Math (header-only, in `engine/libs/glm/`) |
| STB | Image + TrueType (header-only, compiled to dylib by `setup`) |
| cgltf | glTF parsing (header-only, compiled to dylib by `setup`) |
| ozz-animation | Skeletal animation (built from submodule by `setup`) |
| ENet | UDP networking (header-only, compiled to dylib by `setup`) |
| `libengine_assets` | Embedded shaders/fonts (generated by `setup` from `engine/assets/`) |

`libengine_assets.dylib` is **gitignored** — it gets regenerated whenever `engine/assets/` changes.

## Platform abstraction

`engine/scripts/platform/`:

- `common.sh` — detect OS/arch, load platform-specific file, exposes `PROJECT_DIR`, `PLATFORM`, `LIBS_DIR`, etc.
- `macos.sh` — full macOS implementation
- `linux.sh`, `windows.sh` — stubs

Sourced by all build scripts:

```bash
source "$SCRIPT_DIR/platform/common.sh"
require_platform_support
load_jank_paths
LIBS_DIR="$(get_libs_dir)"
```

Helpers: `get_lib_extension`, `format_lib_name`, `fix_lib_install_name`, `add_rpath`, `change_lib_path`, `bundle_system_lib`, `code_sign_adhoc`, `create_app_bundle`, `generate_launcher_script`.

To add a platform: implement `engine/scripts/platform/{name}.sh`, update `detect_platform()` in `common.sh`, populate `engine/libs/{platform}/`.

## Asset pipeline tools

`engine/tools/`:

- **gla2ozz** — Convert Quake 3 / JKA `.gla` skeletal animations to ozz format.
- **ozz2gltf** — Export ozz skeletons / animations to glTF for visualization.
- **ozz-retarget** — Retarget animations between skeleton rigs.

Build with `engine/scripts/build-gla2ozz`, `engine/scripts/build-ozz2gltf`, `engine/scripts/build-ozz-tools.sh`.

## Distribution

The engine produces a single distributable artifact: `engine/dist/jank-engine/` (~324 MB). It contains the binary, bundled native dylibs, the embedded clang toolchain for runtime JIT, jank stdlib, and headers — everything needed to run any game directory containing a `jank-engine.edn`.

There is no per-game packaging step. To distribute a game, ship the `dist/jank-engine/` tree alongside the game directory; users invoke `./jank-engine_run /path/to/game`. End users need XCode Command Line Tools (`xcode-select --install`).

Linux and `.app` packaging scripts that existed for the old per-game-AOT model have been removed. Future Linux support means running `./scripts/build-engine` on a Linux box; future `.app` packaging would be a fresh small wrapper around `dist/jank-engine/`.

## Common gotchas

- **Untyped float literals in `cpp/...` calls fail JIT compile.** Wrap each literal: `(cpp/glm.vec3 (cpp/float 3.0) (cpp/float 2.0) (cpp/float 3.0))`.
- **`try`/`catch` in jank takes a C++ exception type, not `:default`.** No catch-all keyword exists.
- **JVM-isms aren't available.** No `.startsWith` etc. — use `(subs s 0 n)` and friends.
- **Module-not-found from JIT.** Either the namespace isn't on `:paths` in `jank-engine.edn`, or its file path doesn't match the namespace (hyphens → underscores in path segments).
- **Header not found in consumer `cpp/raw`.** The game's `:includes` should list directories containing the headers; engine third-party (glm, GLFW, ozz, engine `*_impl.h`) is auto-added by the binary at startup.
