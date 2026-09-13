# Game distribution

Development uses `lein run` from `game/`, with the eager-compilation wrapper configured as shown in the [README](../../README.md#development). Release compilation uses `lein with-profile release compile`. Both use `sca.core`, which requires every supported mode.

## Packaging

Run `game/scripts/package` after installing the engine source package with `lein install`. It compiles the release and creates `game/dist/sca`:

```text
sca/
  bin/sca
  lib/sca/
  models/
  textures/
  sca_run
```

`bundle-native.py` copies linked runtime libraries and rewrites loader paths to resolve inside the bundle. It signs the staged binaries on macOS. Linux libc and graphics drivers remain OS dependencies. Engine shaders/fonts are embedded by the native build; game models/textures are copied by the package script.

The executable uses jank's static runtime. It does not need jank, LLVM, source headers, or build caches at runtime. Compilation paths may remain in diagnostic strings; packaging does not rewrite binary strings.

## Validation

From the repository root:

```bash
JANK_REAL="${JANK_REAL:-$(command -v jank)}" PATH="$PWD/scripts/eager:$PATH" \
  python3 scripts/smoke-test.py --cwd game -- lein run --
JANK_REAL="${JANK_REAL:-$(command -v jank)}" PATH="$PWD/scripts/eager:$PATH" \
  python3 scripts/smoke-test.py --graphics --cwd game -- lein run --
# After packaging, move dist/sca to a separate directory, then:
python3 scripts/smoke-test.py --standalone -- /absolute/path/to/sca/sca_run
python3 scripts/smoke-test.py --standalone --graphics -- /absolute/path/to/sca/sca_run
```

Network tests require echoed messages and a normal client exit. Graphical tests set `SCA_SMOKE_TEST=1` and require a welcome message, 120 completed frames, and a normal client exit while the server remains alive. Leave that variable unset for normal play. CI tests development and relocated releases; Linux uses Xvfb for graphics, while macOS CI runs network tests.

Verified locally on 2026-09-13 with jank `aa421eef05d6394fe4d04f30e52da74312778813`:

| Check | macOS arm64 | Linux x86_64 |
| --- | --- | --- |
| Development: connected client, 120 frames, normal exit | Passed | Passed |
| Relocated release: network echo | Passed | Passed |
| Relocated release: 120 frames, normal exit | Passed | Passed |
| Release with source and toolchain access denied | Passed (sandbox-exec) | Passed (bubblewrap) |

The client releases animation contexts on shutdown and remote-player disconnect. This resolves the ozz allocator assertion exposed by the development dependency build’s debug configuration.

## Compiler compatibility

- Native `case` results use `identity` to box their values.
- Conditional throws in the glTF parsers call throwing helper functions.
- The shared glTF header explicitly instantiates `std::allocator<Vertex>` under libstdc++; collision code does the same for `glm::vec3` and `unsigned int` for Linux JIT compilation.
- `scripts/eager/jank` adds `--eagerness eager` during development. Lein-jank 2026.09-7 does not expose this option; lazy compilation caused an apparent freeze after removing the custom loader. The eager run was manually confirmed responsive on macOS.

See the [case/throw regression boundaries](../../docs/jank-compiler-repros.md) and [Linux allocator repro](../../docs/jank-linux-vector-repro.md).

CI builds jank with LLVM 23 and a forced `cmath` include for CppInterop. Linux uses `C.UTF-8` for the Unicode reader.
