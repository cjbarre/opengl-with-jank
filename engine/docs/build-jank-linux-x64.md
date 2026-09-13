# Linux x86_64 builds

The [Linux workflow](../../.github/workflows/build-linux.yml) contains the compiler prerequisites and jank build commands used by this project: Ubuntu 24.04, LLVM 23, and libstdc++ 14. Its compiler cache is keyed by the resolved jank commit and toolchain versions.

With jank on `PATH`:

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

Lein-jank builds native dependencies inside bubblewrap. When testing in Docker, nested sandboxing requires `--cap-add SYS_ADMIN --security-opt systempaths=unconfined` for the `/proc` mount. Use a UTF-8 locale, such as `C.UTF-8`.

Release bundles use jank's static runtime. See [packaging and runtime tests](bake-distribution.md).
