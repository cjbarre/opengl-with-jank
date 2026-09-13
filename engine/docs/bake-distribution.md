# Baked-game distribution

`engine/scripts/bake` ships a jank game as a self-contained bundle with no
end-user install prerequisites: no jank, no XCode CLI tools, no clang, and no
LLVM runtime dylibs.

## Distribution model

There are two separate products:

| Path | Output | Runtime model | Audience |
|---|---|---|---|
| `engine/scripts/build-engine` | `engine/dist/jank-engine/` | dynamic runtime; uses installed jank to JIT-load loose game source | development |
| `engine/scripts/bake <game-dir>` | `<game-dir>/dist/<name>/` | static runtime; engine + game AOT'd into one binary | shipping |

The dev engine must stay dynamic because `engine.runtime.core` reads
`jank-engine.edn`, adds the game's source paths, and `(require ...)`s the game
entry namespace at runtime. Jank's static runtime cannot read loose modules.
The dev bundle is intentionally jank-native: it packages the engine binary,
engine-native dylibs, and C++ headers, but it expects `jank` on `PATH` to
provide LLVM/clang/JIT resources.
On macOS, `jank-engine_run` resolves those dynamic runtime libraries through
bundle-local rpath symlinks created at launch. `JANK_LLVM`, `JANK_CRYPTO_DIR`,
and `JANK_ZSTD_DIR` can override the discovered locations for non-standard jank
or package-manager installs.

The game project selects `:jank {:runtime :static}`, which lein-jank passes to
jank as `--runtime static`. Static runtime removes the startup JIT/clang path and lets
the baked bundle omit `libLLVM`, `libclang-cpp`, bundled clang, clang
resource directories, jank stdlib source, runtime headers, and the old macOS
`xcrun` shim.

Native dependency compilation belongs to lein-jank. `scripts/setup` initializes
submodules and installs the engine source package with `lein install`.
`build-engine` runs `lein compile`; `bake` installs the engine package and runs
`lein with-profile release compile` in the game directory. Their target
directories are `engine/target/engine` and `game/target/release`. Packaging
copies those outputs into the distribution directory without rewriting the
cached build artifacts.

Engine shaders and fonts are embedded through a generated `engine_assets.h`
header. They do not require a separate asset library.

## Path catalog

Build-time inputs:

| Path source | Used by | Portability handling |
|---|---|---|
| `jank` on `PATH` | both scripts | Required developer prerequisite; resolved at build or launch time. |
| `JANK_DIR` | both scripts | Optional override for the jank compiler+runtime checkout. Auto-derived from `jank` when it points at `.../build/jank`. |
| `JANK_LLVM` | both scripts | Optional override for the LLVM root used by jank. Auto-detected from the jank build first, then common system LLVM roots. |
| `JANK_CRYPTO_DIR`, `JANK_ZSTD_DIR` | `build-engine` launcher on macOS | Optional runtime overrides for non-standard OpenSSL/zstd installs. |
| `engine/jank-build.bb` and `engine/native/CMakeLists.txt` | lein-jank | Build native dependencies and generated asset headers in jank’s target cache. |
| commons `gl-sys` and `glfw-sys` packages | lein-jank | Supply platform OpenGL/GLFW compiler and linker settings through the dependency graph. |
| `<game-dir>/jank-engine.edn` and `<game-dir>/project.clj` | `bake` | Game-relative config; `bake` resolves the game dir to an absolute path during build only. |
| `<game-dir>/<asset>` entries from `:assets` | `bake` | Copied into the output bundle; asset entries must be relative and cannot contain `..`. |

Runtime paths after packaging:

| Output | Runtime lookup |
|---|---|
| `engine/dist/jank-engine/bin/jank-engine` | Loads engine-native dylibs from `@executable_path/../lib/jank-engine`. On macOS, the launcher creates temporary bundle-local symlinks to the installed jank LLVM/OpenSSL/zstd library dirs and removes them on exit. |
| `engine/dist/jank-engine/include` | Headers consumed by loose game source during dev JIT. Paths are inside the dev bundle, not the builder's checkout. |
| `<dist>/bin/<name>` from `bake` | Loads only bundled dylibs from `@executable_path/../lib/<name>` plus OS libraries/frameworks. It does not require jank, LLVM, clang, or local headers at runtime. |
| `<dist>/<name>_run` from `bake` | Resolves its own directory with bash parameter expansion, sets library path to `lib/<name>`, `cd`s into the bundle root, then runs the binary. |

## Bundle contents

macOS layout (support libraries depend on the compiler installation):

```text
<game-dir>/dist/<name>/
├── bin/<name>                  AOT executable: engine + game source baked in
├── lib/<name>/                 bundled dylibs loaded through @rpath
│   ├── libglfw.3.dylib
│   ├── libozz_*.dylib
│   ├── libstb_all.dylib
│   ├── libenet.dylib
│   ├── libcgltf.dylib
│   ├── libc++.1.dylib
│   ├── libc++abi.1.dylib
│   ├── libunwind.1.dylib
│   └── libzstd.1.dylib
├── models/ textures/ ...       game assets from :assets
└── <name>_run                  launcher
```

`bundle-native.py` copies the linked executable’s native dependency closure.
`libc++`, `libc++abi`, `libunwind`, and `libzstd` are bundled when the binary or
its native libraries reference non-system copies. They are runtime
support libraries, not LLVM/clang JIT components.

The launcher changes to the bundle root before `exec`, so CWD-relative asset
paths like `models/hills.gltf` resolve against the shipped bundle.

Both packaging paths also sanitize embedded build-root strings after compile.
`otool` verifies dynamic loader paths, while `strings` catches source/debug
metadata paths that can otherwise leak the builder's local checkout or temp
directory into the binary. `engine/scripts/verify-portability` checks both
compiled objects and executable launchers for those leaks.

## Static-runtime constraints

Static baked games work only if every namespace needed at runtime was compiled
into the binary. The static module loader can call already-registered AOT load
functions, but it cannot read `.jank` source or jar entries at runtime.

Avoid these in baked/shipping code:

- runtime `eval`
- dynamically requiring source namespaces that were not included in the AOT
  compile
- runtime C++ generation through `cpp/raw` inside eval-like paths

Normal top-level `cpp/raw` blocks are fine: with the default C++ codegen, they
are appended to generated C++ and compiled during AOT.

## Compiler compatibility

The source uses the author's workarounds for two jank regressions:

- Native `case` results are boxed with `identity`.
- Conditional throws in the glTF parsers call a throwing helper function.

Both workarounds pass isolated JIT and AOT tests on unmodified jank `aa421eef`.
The throw test checks both the normal result and the expected exception.
Regression boundaries are recorded in [the repro document](../../docs/jank-compiler-repros.md).

On Linux with libstdc++ 14, the editor mesh code explicitly instantiates
`std::allocator<Vertex>` so JIT compilation emits its deallocation function.
The [isolated vector repro](../../docs/jank-linux-vector-repro.md) fails in plain
jank and passes with that instantiation. The declaration is guarded by
`__GLIBCXX__`; the changed namespace also passes macOS function realization.

## Verification

Validation on 2026-09-13:

- macOS arm64: the Leiningen bake completed and the packaged-path verifier
  passed all 10 files. The server bound UDP/7777; the client connected, loaded
  53 skeleton joints and 48 animations, and exited normally. Gameplay was
  confirmed manually.
- macOS development bundle: the packaged-path verifier passed all 9 files.
  The loose-source server bound UDP/7777; the client connected, entered its
  game loop, and exited normally. Gameplay was confirmed manually.
- With the source-level workarounds and unmodified jank `aa421eef`, the full
  macOS bake completed and the portability verifier passed all 13 files. The
  baked network test connected, exchanged echoed hello/ping messages, and
  exited normally. The earlier visual gameplay checks used the local compiler
  patch; that patch has now been removed from the local compiler and is not
  required by the source or CI. The rebuilt development bundle also passed
  portability and the loose-source network echo test with the compiler patch
  removed.
- Linux x86_64, Ubuntu 24.04: the full development build and game bake passed
  with unmodified jank `aa421eef`, LLVM 23.1.2, and libstdc++ 14. Portability
  passed for all 29 development files and all 22 shipping files. Both clients
  connected to their servers under Xvfb, loaded 53 skeleton joints and 48
  animations, entered the client loop, and remained running for eight seconds.
  The shipping bundle was rebuilt and retested after the allocator workaround.
  Native-library relocation also passed; a separate GLEW bundle ran after its
  system GLEW copy was removed.

Linux compiler builds use `-include cmath` for CppInterop's missing math-header
include and `C.UTF-8` for jank's Unicode reader. The Docker checks required
`--cap-add SYS_ADMIN --security-opt systempaths=unconfined` for bubblewrap's
`/proc` mount; jank's native-build sandbox remained enabled. CI runs directly
on Ubuntu and does not use these Docker options.

Build:

```bash
./engine/scripts/bake game -o /private/tmp/sca-static-bake
```

Run the packaged-path verifier:

```bash
./engine/scripts/verify-portability engine/dist/jank-engine
./engine/scripts/verify-portability /private/tmp/sca-static-bake
```

Expected: both commands pass. The verifier scans binaries, dylibs/shared
objects, dynamic loader metadata, and executable launcher scripts for local
checkout paths, temp output paths, and build-machine package-manager paths.

Smoke test:

```bash
cd /private/tmp/sca-static-bake
./sca_run server
./sca_run client
./sca_run editor
```

The server should bind UDP/7777, the client should connect and enter the client
loop, and the editor should start without requiring XCode CLI tools.

## Jank source points

These current jank source paths explain why static bake works and why the dev
engine cannot use it:

| File | Why it matters |
|---|---|
| `lein-jank/src/leiningen/jank/core.clj` | `:runtime` maps to `--runtime` |
| `compiler+runtime/src/cpp/jank/aot/processor.cpp` | static runtime link line omits `-lLLVM -lclang-cpp`; dynamic runtime includes them |
| `compiler+runtime/src/cpp/jank/jit/processor_static.cpp` | static JIT processor stubs eval/function creation |
| `compiler+runtime/src/cpp/jank/runtime/module/loader_static.cpp` | static loader only loads pre-registered AOT modules |
| `compiler+runtime/src/cpp/jank/runtime/context_static.cpp` | runtime eval/read/compile are disabled |
| `compiler+runtime/src/cpp/jank/codegen/cpp_processor.cpp` | default C++ codegen AOTs top-level `cpp/raw` |

If a future jank update changes static runtime behavior, re-run the dependency
and smoke checks above before changing the bundle layout.
