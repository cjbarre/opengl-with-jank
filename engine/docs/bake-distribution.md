# Baked-game distribution

`engine/scripts/bake` ships a jank game as a self-contained bundle with no
end-user install prerequisites: no jank, no XCode CLI tools, no clang, and no
LLVM runtime dylibs.

## Distribution model

There are two separate products:

| Path | Output | Runtime model | Audience |
|---|---|---|---|
| `engine/scripts/build-engine` | `engine/dist/jank-engine/` | dynamic runtime; JIT-loads loose game source | development |
| `engine/scripts/bake <game-dir>` | `<game-dir>/dist/<name>/` | static runtime; engine + game AOT'd into one binary | shipping |

The dev engine must stay dynamic because `engine.runtime.core` reads
`jank-engine.edn`, adds the game's source paths, and `(require ...)`s the game
entry namespace at runtime. Jank's static runtime cannot read loose modules.

The baked binary uses `JANK_RUNTIME=static`, which lein-jank passes to jank as
`--runtime static`. Static runtime removes the startup JIT/clang path and lets
the baked bundle omit `libLLVM`, `libclang-cpp`, bundled `clang-22`, clang
resource directories, jank stdlib source, runtime headers, and the old macOS
`xcrun` shim.

## Bundle contents

Current macOS layout:

```text
<game-dir>/dist/<name>/
├── bin/<name>                  AOT executable: engine + game source baked in
├── lib/<name>/                 bundled dylibs loaded through @rpath
│   ├── libglfw.3.dylib
│   ├── libozz_*.dylib
│   ├── libstb_all.dylib
│   ├── libenet.dylib
│   ├── libcgltf.dylib
│   ├── libengine_assets.dylib
│   ├── libOpenGL.dylib
│   ├── libc++.1.dylib
│   ├── libc++abi.1.dylib
│   ├── libunwind.1.dylib
│   └── libzstd.1.dylib
├── models/ textures/ ...       game assets from :assets
└── <name>_run                  launcher
```

`libc++`, `libc++abi`, `libunwind`, and `libzstd` are still bundled because the
static binary and bundled native libraries reference them. They are runtime
support libraries, not LLVM/clang JIT components.

The launcher changes to the bundle root before `exec`, so CWD-relative asset
paths like `models/hills.gltf` resolve against the shipped bundle.

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

## Verification

Build:

```bash
./engine/scripts/bake game -o /private/tmp/sca-static-bake
```

On macOS, confirm the binary has no LLVM/clang dependency:

```bash
otool -L /private/tmp/sca-static-bake/bin/sca | grep -E 'LLVM|clang' || true
find /private/tmp/sca-static-bake -maxdepth 4 \
  \( -name '*LLVM*' -o -name '*clang*' -o -name 'xcrun' \) -print
```

Expected: no output from either command.

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
