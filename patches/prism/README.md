# Prism build for Linux screen-reader speech

## Scope

This local patch targets Prism 0.18.3 at `b68e9c94240cefb1f08b2306c26376e3ab300a7a`. It changes only `source/backends/orca.cpp`. The patch uses an explicit existing Unix session-bus address, prevents service activation, and bounds each connection or method wait to 500 milliseconds. Several sequential operations can take longer than one bound.

The launcher still filters screen-reader backends and initializes Prism without background availability monitoring. With no running reader, it remains silent. This build does not start, configure or replace Orca or any speech service. See [notices and authorship](NOTICE.md).

## Requirements

- Linux, CMake 3.24 or later, Ninja, Git and pkg-config.
- A C23/C++23 compiler and standard library supporting `std::expected` and `std::jthread`.
- Development files providing `glibmm-2.68 >= 2.68.0` and `giomm-2.68 >= 2.68.0` through pkg-config.

The pinned source includes the required bundled dependencies: fmt, Highway, simdutf, concurrentqueue and dr_wav. The Godot submodule is not needed with GDExtension disabled. The verified build uses the bundled dependency provider. Build against the target system's toolchain; the earlier GCC 16.2.1 build is not a portable binary compatibility guarantee.

## Prepare the source

Use a fresh external checkout of [upstream Prism](https://github.com/ethindp/prism.git), detached at the exact revision above. Keep its source and build directories outside this project. Confirm `git rev-parse HEAD` matches the pin and `git status --porcelain` is empty before applying the patch.

Set `PROJECT_SOURCE` to this project's absolute path, `PRISM_SOURCE` to the fresh external Prism checkout, and `PRISM_BUILD` to a new external build directory. The dependency preparation tool uses [manifest.json](manifest.json) to verify the pin, patch hashes and application order. For manual application:

```sh
git -C "$PRISM_SOURCE" apply --check "$PROJECT_SOURCE/patches/prism/orca-existing-reader.patch"
git -C "$PRISM_SOURCE" apply "$PROJECT_SOURCE/patches/prism/orca-existing-reader.patch"
```

The patched `source/backends/orca.cpp` SHA-256 must be `8034b18ee058805b5719948218d3a431fd1c6661fbb9e48ec171101ae7f8e077`. Do not apply this patch to another revision or an already modified checkout.

## Configure and build

Use a new build directory so cached backend settings cannot enable another backend:

```sh
cmake -S "$PRISM_SOURCE" -B "$PRISM_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DPRISM_DEPENDENCY_PROVIDER=BUNDLED \
  -DPRISM_BACKEND_DEFAULT=OFF \
  -DPRISM_ENABLE_ORCA_BACKEND=ON \
  -DPRISM_ENABLE_POWER_MANAGEMENT=OFF \
  -DPRISM_ENABLE_TESTS=OFF \
  -DPRISM_ENABLE_DEMOS=OFF \
  -DPRISM_ENABLE_GDEXTENSION=OFF \
  -DPRISM_ENABLE_SHIMS=OFF \
  -DPRISM_BUILD_WINELIBS=OFF \
  -DPRISM_REGENERATE_DJINNI=OFF \
  -DPRISM_IPO=OFF
cmake --build "$PRISM_BUILD" --target prism --parallel 2
```

The output is `PRISM_BUILD/libprism.so`. Configuration must report `orca=on` and every other backend off. No standalone TTS backend is enabled. Build only the library target; do not run upstream demos or speech tests as part of build validation. Do not install over a system or another application's Prism library.

## Verification and safety

The source patch applies cleanly to the pinned upstream and produces the exact previously tested source bytes. Earlier isolated fake-reader tests verified no service activation, bounded calls and reconnection. Those tests did not establish listening usability.

A fresh isolated Linux build using the commands above passed. Static inspection confirmed only the Orca backend and all C API symbols required by the launcher. The library was not loaded. Existing upstream compiler warnings were retained; the build does not prove speech works or establish binary compatibility with other systems.

Compiler checks and static inspection with `readelf` or `nm` do not require loading Prism. A library build is not a speech test. Native speech testing requires an independently verified private display, runtime, configuration, service activation and bus environment that cannot reach or overwrite live resources. Removing display variables or starting a private bus alone is insufficient isolation.

When staging a built library, preserve the upstream `LICENSE`, `NOTICE`, complete `LICENSES` directory and corresponding-source information described in [NOTICE.md](NOTICE.md). This patch does not change the host launcher or its installed package layout.
