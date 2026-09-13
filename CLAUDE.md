# Repository guidance

## Build and run

The engine is a Leiningen source package. The game depends on it through `project.clj`.

```bash
cd engine
lein install
cd ../game
export JANK_REAL="${JANK_REAL:-$(command -v jank)}"
export PATH="$PWD/../scripts/eager:$PATH"
lein run -- server
lein run -- client
lein with-profile release compile
./scripts/package
```

Reinstall the engine after source changes. Use `project.clj` and `jank-build.bb` for build settings. Native dependency compilation belongs to jank-build; release packaging belongs in `game/scripts/package` and `game/scripts/bundle-native.py`.

The engine has no standalone loader. `sca.core` requires all supported modes so the same entry point works with `lein run` and static-runtime release compilation. Keep compiler workarounds documented in `engine/docs/bake-distribution.md` until their isolated repros pass without them.

## Layout

- `engine/src/engine`: engine namespaces, generally with interface/core splits.
- `engine/include`: engine and third-party C/C++ headers.
- `engine/native`: native CMake build and implementation translation units.
- `engine/assets`: embedded shaders and fonts.
- `engine/libs/glm`, `engine/third_party/ozz-animation`: source submodules.
- `game/src/sca`, `game/include`: game code and headers.
- `game/models`, `game/textures`: release assets.

## Validation

`scripts/smoke-test.py` checks a development command or packaged executable with a server/client pair. `--graphics` checks connection and entry into the game loop; the default checks echoed network messages. `--standalone` removes jank and custom library search paths from the process environment. Release tests move the bundle before running it.

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

Engine-shipped assets (shaders, fonts) live in `engine/assets/`. The native build runs `engine/scripts/embed-assets.clj` to generate `engine_assets.h`. Its `engine_register_resources()` function is called from a top-level form in `engine.resources.core`, which populates jank's `aot::find_resource` registry.

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
