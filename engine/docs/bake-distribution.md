# Baked-game distribution: design and mechanics

This document describes how `engine/scripts/bake` ships a jank-based game to
end users with **zero installation prerequisites** — no XCode CLI tools, no
jank, no clang. It covers the architectural choice, the mechanic that makes
it work, the assumptions about jank's behavior we depend on, and how to
detect if those assumptions break in a future jank release.

If `bake` ever breaks for non-obvious reasons, start here.

---

## 1. The two-tier distribution model

```
                  dev iteration                       shipping
                  ─────────────                       ────────
              ./scripts/build-engine            ./scripts/bake <game-dir>
                       │                                │
                       ▼                                ▼
        engine/dist/jank-engine/               <game-dir>/dist/<name>/
        ├── bin/jank-engine                    ├── bin/<name>
        ├── lib/jank-engine/                   ├── lib/<name>/
        └── jank-engine_run                    ├── shim/xcrun
                                               └── <name>_run
        - Reusable across games                - One game baked in
        - JIT-loads consumer source            - Engine + game AOT'd
        - Needs CLI tools on machine           - Needs nothing on machine
```

`build-engine` produces the runtime; `bake` produces a finished product.
The two are independent — neither needs the other at runtime. Devs use the
first while iterating; shipping uses the second.

`bake` compiles through the game directory's lein-jank `project.clj`. The
game's `jank-engine.edn` is still used for bundle metadata: output name,
asset directories, and defensive include directories.

The split exists because they have opposite optimization targets:

| Goal | jank-engine | bake |
|---|---|---|
| Optimize for | iteration speed | end-user simplicity |
| Game source | loose `.jank`, edited in place | AOT'd into binary |
| Compile time per change | ~0 s (just JIT on next run) | ~3-4 min (full AOT bake) |
| Runtime clang invocation | yes (every consumer namespace) | never |
| End-user prereqs | XCode CLI tools | none |

---

## 2. Why end users normally need XCode CLI tools

Jank embeds LLVM/clang for runtime JIT (eval, dynamic require, runtime
`cpp/raw`). Even an AOT'd jank program constructs a `jit::processor` at
startup:

- `compiler+runtime/src/cpp/jank/runtime/context.cpp:47` — `runtime::context`
  initializer list eagerly constructs `jit_prc{ binary_version }`. No opt-out.
- `compiler+runtime/src/cpp/jank/aot/processor.cpp:113` — the AOT entrypoint
  always passes `init_default_ctx=true` to `jank_init_with_pch`, which
  triggers the runtime context construction.

The `jit::processor` constructor in turn:

- `compiler+runtime/src/cpp/jank/jit/processor.cpp:90-249` — calls
  `find_clang()` (throws if missing), `find_clang_resource_dir()`,
  `find_pch()`, and `add_system_flags()`.
- `compiler+runtime/src/cpp/jank/util/environment.cpp:225-272` —
  `add_system_flags()` spawns `xcrun --show-sdk-path` on macOS and throws if
  `xcrun` is missing, fails, or returns an empty string. This is the
  source of the XCode CLI tools requirement.

We bundle clang ourselves (`lib/jank/0.1/bin/clang-22`) so `find_clang()`
succeeds on any machine. The blocker is `xcrun`.

---

## 3. The xcrun shim: how it bypasses the requirement

`add_system_flags()` captures whatever `xcrun --show-sdk-path` writes to
stdout and stashes it as `-isysroot <path>` in the args passed to
CppInterOp's `Interpreter`. **The path is never opened at startup** — the
PCH that the Interpreter would normally load from the SDK is embedded in
the AOT binary as a virtual resource at `/virtual/incremental.pch`:

- `compiler+runtime/src/cpp/jank/util/clang.cpp:224-260` — `find_pch()` returns
  the virtual path backed by in-memory data.
- `compiler+runtime/src/cpp/jank/aot/processor.cpp:71` — the PCH bytes are
  embedded as a resource at AOT-compile time.

So at startup, the SDK path is captured but only used later, when the
Interpreter is asked to actually compile something. For a baked game, that
"later" never arrives: all `cpp/raw` is AOT'd to machine code (see §5),
no `eval` runs, no source `.jank` is dynamically required.

The shim is therefore three lines:

```sh
#!/bin/sh
echo "/dev/null"
```

It's at `engine/scripts/xcrun-stub` and gets copied into every baked bundle
at `<bundle>/shim/xcrun`. The launcher prepends `<bundle>/shim/` to PATH
before exec'ing the binary, so jank's `findProgramByName("xcrun")` finds
our shim first.

### Why prepend, not append

On end-user macOS without XCode CLI tools installed, `/usr/bin/xcrun` is
not absent — it's a system stub that, when invoked non-interactively,
exits with `xcrun: error: invalid active developer path` (and in
interactive contexts, prompts the user to run `xcode-select --install`).
Appending the shim to PATH would lose to this stub. Prepending makes our
shim win, side-stepping the stub entirely.

On dev machines with real CLI tools, prepending also wins — but that's
fine, because the baked binary doesn't actually use the SDK path. The shim
is strictly safer than relying on whatever the user's environment provides.

---

## 4. Bundle contents (and why each piece is there)

```
<game-dir>/dist/<name>/
├── bin/<name>                   AOT executable: engine + game source baked in
├── lib/<name>/                  bundled dylibs (rpath = @executable_path/../lib/<name>)
│   ├── libglfw.3.dylib
│   ├── libozz_*.dylib
│   ├── libstb_all.dylib
│   ├── libenet.dylib
│   ├── libcgltf.dylib
│   ├── libengine_assets.dylib   shaders + fonts embedded by setup
│   ├── libOpenGL.dylib          stub linking to OpenGL.framework
│   ├── libLLVM.dylib            STILL NEEDED (see below)
│   ├── libclang-cpp.dylib       STILL NEEDED
│   ├── libc++.1.dylib
│   ├── libc++abi.1.dylib
│   ├── libunwind.1.dylib
│   ├── libcrypto.3.dylib
│   └── libzstd.1.dylib
├── lib/jank/0.1/                jank's "runtime resources" tree
│   ├── bin/clang-22             needed for find_clang() at startup
│   ├── include/                 libc++, jank
│   ├── lib/clang/22/            clang resource directory
│   └── src/                     jank stdlib (.jank files)
├── include/                     bundled third-party headers (defensive)
│   ├── glm/  GLFW/  ozz/        ...
│   └── engine/  *.h             engine *_impl.h + loose third-party
├── shim/
│   └── xcrun                    the 3-line shim
├── models/  textures/  ...      game assets per :assets in jank-engine.edn
└── <name>_run                   launcher script
```

### Why libLLVM and libclang-cpp are still bundled

A baked binary never JIT-compiles anything, but the `jit::processor`
constructor still runs at startup, and that constructor's body references
LLVM/CppInterOp symbols. The dylibs must be loadable, even if the
functions they expose are never called with real work. Stripping them
requires the upstream lazy-JIT patch (see §7).

### Why clang-22 is bundled

`find_clang()` at `compiler+runtime/src/cpp/jank/util/clang.cpp:85-124`
checks `JANK_CLANG_PATH` (compiled-in dev path, won't exist on end-user
machines) then `<resource_dir>/bin/clang++`. We point the second one at
our bundled clang.

`find_clang_resource_dir()` (clang.cpp:131-161) then spawns
`clang++ -print-resource-dir` because the found clang ≠ `JANK_CLANG_PATH`.
That subprocess is real — clang must actually be loadable and runnable.
Hence the rpath fix on `lib/jank/0.1/bin/clang-22` so it finds the
bundled libLLVM.

### Why include/ is bundled

Defensive. All `cpp/raw` blocks are AOT-compiled (§5), so headers are
mostly only consulted if jank ever spawns clang at runtime. Bundling is
cheap (~few MB) and prevents mysterious failures if some unforeseen code
path triggers compilation.

### Why the launcher chdir's to the bundle root

Game asset paths in `sca.client`, `sca.server`, etc. are CWD-relative
(`"models/hills.gltf"`, `"models/player/animations/"`, etc.). The launcher
does `cd "$DIR"` before `exec`, so those resolve against the bundle root
where `models/` and `textures/` live.

---

## 5. cpp/raw blocks are AOT'd to machine code

This is the other half of why the shim works: our engine and game source
contain a lot of `cpp/raw` blocks (OpenGL/GLFW/glm/ozz/ENet glue), but
these are compiled to native code at AOT time, not deferred to runtime
JIT.

- `compiler+runtime/src/cpp/jank/codegen/processor.cpp:2009-2019` — with
  the default C++ codegen (`codegen_type::cpp`, set as default at
  `compiler+runtime/include/cpp/jank/util/cli.hpp:96`), `cpp/raw` blocks
  are appended verbatim to the generated C++ source file, which clang
  then compiles to a `.o` and links into the binary.

If jank ever switches the default to LLVM-IR codegen, or our build script
explicitly opts into it, this changes —
`compiler+runtime/src/cpp/jank/codegen/llvm_processor.cpp:2576-2591`
parses `cpp/raw` through the JIT even at AOT time. The bake script does
not pass `--codegen llvm-ir`, so we're on the safe path.

---

## 6. Failure modes (what would break a baked game)

The shim works because of three load-bearing assumptions about the baked
binary's runtime behavior. Violating any one breaks it.

| Violation | Symptom | Why |
|---|---|---|
| Game calls `(eval ...)` | Crash on first eval | Triggers JIT compile with `-isysroot /dev/null` |
| Game does dynamic `(require '<source-ns>)` | Crash on first such require | Loads `.jank` source via JIT |
| Game has runtime `cpp/raw` (e.g. inside an `(eval ...)`) | Crash when reached | JIT-compiles new C++ with broken sysroot |
| Engine namespace gets refactored to use `eval` | Crash early in startup | Same as above |
| Build switches to `--codegen llvm-ir` | Crash at first `cpp/raw` call | Code is JIT'd at runtime, not AOT'd |
| Jank starts validating xcrun output (e.g., `stat()` the path) | Startup error | Shim's `/dev/null` would fail validation |
| Jank starts opening SDK headers at startup | Startup error | Bundle doesn't include them |

The current jank version (whatever submodule pin we're using) does **not**
do any of these. Pin jank tightly and watch upstream changes to:

- `compiler+runtime/src/cpp/jank/util/environment.cpp` (xcrun caller)
- `compiler+runtime/src/cpp/jank/jit/processor.cpp` (startup eagerness)
- `compiler+runtime/src/cpp/jank/codegen/processor.cpp` (cpp/raw codegen path)

---

## 7. Verifying a baked game works without CLI tools

Don't `xcode-select --reset` (destructive). Instead, simulate the
end-user-without-CLI-tools state by shadowing `xcrun` with a stub that
errors:

```bash
mkdir -p /tmp/no-cli-tools
cat > /tmp/no-cli-tools/xcrun << 'EOF'
#!/bin/sh
echo "xcrun: error: invalid active developer path" >&2
exit 1
EOF
chmod +x /tmp/no-cli-tools/xcrun

# Run the baked binary with the stub xcrun shadowing the real one
PATH=/tmp/no-cli-tools:/usr/bin:/bin /path/to/dist/<name>/<name>_run server
```

If the baked binary reaches its normal "ready" output (e.g. `Server
ready. Waiting for clients...`), the shim worked: jank's startup found
our shim before the stub via the prepended PATH, captured `/dev/null`,
and proceeded. If it errors out with anything matching `Unable to find
'xcrun' binary` or `Unable to get a valid path`, the shim PATH wiring is
broken.

For stdout-redirected runs, wrap in `script -q /dev/null bash -c "..."`
to defeat block-buffering — jank's stdout is fully buffered when not on a
TTY, so output appears late or never to a captured log otherwise.

---

## 8. Future evolution

### Upstream jank patch: lazy JIT init

Would let baked binaries strip libLLVM (~80 MB savings) and skip the
xcrun shim entirely. Four changes in jank:

1. Make `jit::processor`'s constructor body lazy (extract into
   `lazy_init()`, called by each public method on first use).
2. Defer `add_system_flags()` (and thus xcrun) into the lazy path.
3. Add a `jank_no_jit` CMake / runtime mode that stubs `lazy_init()` to
   throw a clear error.
4. With `jank_no_jit`, drop `-lLLVM -lclang-cpp` from the AOT link line
   at `compiler+runtime/src/cpp/jank/aot/processor.cpp:287-300`.

Risk: changes are mechanical but need to demonstrate that all existing
jank tests pass. The MVP (lazy init, no new flags) is a strict refactor
and likely upstream-acceptable on its own.

### `.app` bundle wrapping

For double-click friendly distribution. Wrap `dist/<name>/` into a `.app`
with proper Info.plist, signed with an ad-hoc signature (or a real
Developer ID). A small wrapper script around `bake`'s output, not a
change to `bake` itself.

### Brew formula

`brew install jank-engine` → ships the `jank-engine` runtime + a
`jank-engine bake` subcommand → users with an existing game can
`jank-engine bake .` from anywhere.

### Linux

The shim mechanic is macOS-only — Linux jank doesn't call `xcrun`
(`add_system_flags()` is wrapped in
`if constexpr(jtl::current_platform == jtl::platform::macos_like)`). A
Linux baked game just needs the dylib bundling to work; no shim. The
existing `engine/scripts/platform/linux.sh` is a stub — building this
out is the gating work.

---

## 9. Citations index

Quick reference to the jank source paths this design depends on. All
paths are relative to `compiler+runtime/` in the jank repo
(`/Users/cam/Documents/code/jank/compiler+runtime/`).

| File:line | What |
|---|---|
| `src/cpp/jank/runtime/context.cpp:47` | Eager `jit_prc{...}` construction |
| `src/cpp/jank/aot/processor.cpp:113` | AOT entrypoint passes `init_default_ctx=true` |
| `src/cpp/jank/aot/processor.cpp:71` | PCH embedded as resource at AOT time |
| `src/cpp/jank/aot/processor.cpp:287-300` | LLVM/clang-cpp link line for AOT binaries |
| `src/cpp/jank/jit/processor.cpp:90-249` | The eager `jit::processor` constructor |
| `src/cpp/jank/util/environment.cpp:225-272` | `add_system_flags()` (xcrun caller) |
| `src/cpp/jank/util/clang.cpp:85-124` | `find_clang()` |
| `src/cpp/jank/util/clang.cpp:131-161` | `find_clang_resource_dir()` |
| `src/cpp/jank/util/clang.cpp:224-260` | `find_pch()` returns embedded resource |
| `src/cpp/jank/codegen/processor.cpp:2009-2019` | `cpp/raw` AOT'd to machine code (default codegen) |
| `src/cpp/jank/codegen/llvm_processor.cpp:2576-2591` | `cpp/raw` JIT'd at AOT time (alternate codegen — DO NOT USE) |
| `src/cpp/jank/runtime/module/loader.cpp:1090-1103` | Fast path: pre-registered AOT'd modules bypass JIT |
| `src/cpp/jank/c_api.cpp:1031-1078` | `jank_init_with_pch` entrypoint |
| `CMakeLists.txt:313-315` | `JANK_CLANG_PATH` baked in at jank build time |
| `include/cpp/jank/util/cli.hpp:96` | Default codegen = `codegen_type::cpp` |
