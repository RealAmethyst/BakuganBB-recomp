# Release packaging

## Scope

Only the project owner publishes release packages, and each release needs an explicit request. A package contains the compiled program, launcher, notices and [player guide](playing.md). It never contains game files, the original executable or generated source. The launcher asks each player for their own game directory and accepts only the [supported executable](executable-baseline.md).

The compiled host includes a machine translation of the game's executable code. Publishing it is the owner's decision; [NOTICE.md](../NOTICE.md) states what a package contains. Contributors do not upload their own builds.

Packages are built on a Linux host that holds the private game files. Nothing in this process launches the game or loads a screen reader. Keep every input and output outside the repository.

## Build environment

A build made directly on a rolling distribution is not portable: on 2026-11-14 such a build required glibc 2.44 and GCC 16 runtime libraries. Release binaries are therefore built inside an unprivileged Ubuntu 24.04 root file system, entered through a user namespace with only the work directory, a read-only source checkout and the read-only game directory mounted. The root shares no desktop, session bus, audio or accessibility resources.

Inside the root the mounts have neutral names, such as `/work`, `/src` and `/game`, so compiled files record no personal paths. The root needs the packages the [Linux build guide](building.md) lists, with these differences:

- Use Clang 20 and LLD 20 (`clang-20`, `lld-20`, `llvm-20`). Clang 18 cannot use `std::expected` from the GNU C++ library.
- Install `libstdc++-14-dev`, and the development packages for SDL's X11, Wayland, ALSA, PulseAudio, PipeWire, JACK and sndio backends. Compare SDL's configure summary with a normal build; a missing backend does not fail the build.

## Linux binaries

Follow steps 2 to 4 of the [Linux build guide](building.md) inside the root, adding `-DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20` to the SDK and Prism configuration and using `clang++-20` for the host. Build without `BRAWLERS_STEAM_AUDIO_SDK_DIR`: the spatial cue clips derive from game audio and cannot be distributed.

Check the result's baseline with `objdump -T`. The 0.1.0-alpha.1 binaries need at most glibc 2.38 and `GLIBCXX_3.4.32`.

## Windows binaries

The Windows host, runtime and graphics plugin are cross-compiled in the same root with Clang and LLD 20 targeting `x86_64-pc-windows-msvc`. The [Windows workflow](../.github/workflows/windows-build.yml) validates this configuration with Clang 20 on Windows itself.

Microsoft's C runtime and Windows SDK headers and libraries come from [xwin](https://github.com/Jake-Shadle/xwin), which requires accepting Microsoft's license terms. Unpack them in the `/winsysroot` layout:

```sh
xwin --accept-license splat --use-winsysroot-style --preserve-ms-arch-notation --output "$WORK/winsysroot"
```

The SDK's sources include Windows headers with casing that differs from the files on disk. Give Clang a virtual file system overlay that lists every file under the two include roots, `Windows Kits/10/Include/<version>` and `VC/Tools/MSVC/<version>/include`, with `"case-sensitive": "false"`. LLVM's `WinMsvc.cmake` uses the same technique.

Use a CMake toolchain file with these settings, adjusted to the mounted paths:

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER /usr/lib/llvm-20/bin/clang)
set(CMAKE_CXX_COMPILER /usr/lib/llvm-20/bin/clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_RC_COMPILER /usr/lib/llvm-20/bin/llvm-rc)
set(CMAKE_MT /usr/lib/llvm-20/bin/llvm-mt)
set(CMAKE_AR /usr/lib/llvm-20/bin/llvm-ar)
set(CMAKE_LINKER_TYPE LLD)
set(_winsys "-Xmicrosoft-windows-sys-root /winsysroot -ivfsoverlay /winwork/winsdk-vfs.yaml")
set(CMAKE_C_FLAGS_INIT "${_winsys}")
set(CMAKE_CXX_FLAGS_INIT "${_winsys}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
```

`CMAKE_RC_FLAGS_INIT` must also name the SDK's `um`, `shared` and `ucrt` include directories and the compiler's include directory. The Release try-compile setting matters because xwin omits the debug runtime libraries.

Configure the patched SDK with the options from the [Windows guide](building-windows.md), replacing the preset, which only applies on a Windows host:

```sh
F="-march=x86-64-v2 -Xmicrosoft-windows-sys-root /winsysroot -ivfsoverlay /winwork/winsdk-vfs.yaml"
cmake -S /winwork/rexglue -B /winwork/sdk-build -G "Ninja Multi-Config" \
  --toolchain /winwork/win-toolchain.cmake -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" \
  -DCMAKE_CXX_STANDARD=23 -DCMAKE_CONFIGURATION_TYPES=Release \
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_FIDELITYFX=OFF \
  -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON -DCMAKE_INSTALL_PREFIX=/winwork/sdk-install
cmake --build /winwork/sdk-build --config Release --target install
```

Set `HOME` inside the root: the SDK's install step writes the prefix to the user's CMake package registry.

The host build reuses the translation generated for Linux. The generator has no platform-specific output: on 2026-11-14 two generators built by different compilers produced byte-identical sources, differing only in the path-bearing stamp and dependency files. Configure the host with the toolchain file, `BRAWLERS_REXGLUE_INSTALL_DIR`, `BRAWLERS_REXGLUE_SOURCE_DIR` and `BRAWLERS_GENERATED_DIR`, as in the Windows guide.

Prism is not cross-compiled. Its NVDA client needs Microsoft's IDL compiler, which does not run on Linux. The Windows workflow builds the pinned, patched library with the Windows toolchain, audits it and uploads `prism.dll` with its `CMakeCache.txt` as the `prism-windows-x64` artifact. Download it from a run of the commit being released:

```sh
gh run download <run-id> -n prism-windows-x64 -D "$WORK/prism-windows"
```

That library links its C runtime statically. The cross-compiled binaries import the Visual C++ runtime (`MSVCP140`, `MSVCP140_ATOMIC_WAIT`, `VCRUNTIME140`, `VCRUNTIME140_1`), which the package does not redistribute; the launcher reports its absence.

## Packaging

Commit all source changes first; the tool refuses an uncommitted tree and records the revision. The Windows package bundles the official Python embeddable archive from python.org so players need no Python installation. Check its SHA-256 against python.org's published value before use.

```sh
python3 -B tools/package_release.py --platform linux --version 0.1.0-alpha.1 \
  --native-build "$WORK/linux/native-build" --prism-build "$WORK/linux/prism-build" \
  --prism-source "$WORK/prism" --rexglue-source "$WORK/rexglue" --output "$WORK/release-linux"
python3 -B tools/package_release.py --platform windows --version 0.1.0-alpha.1 \
  --native-build "$WORK/windows/native-build" --prism-build "$WORK/prism-windows" \
  --prism-source "$WORK/prism" --rexglue-source "$WORK/rexglue" \
  --windows-python "$WORK/python-3.14.10-embed-amd64.zip" --output "$WORK/release-windows"
```

The tool copies the launcher, binaries, player guide and notices into a new directory, then writes a `.tar.gz` or `.zip` archive and its `.sha256` file. Before creating anything it checks the Prism backend selection and either the Linux runtime search paths or the Windows architecture and imports. Afterwards it refuses, and removes, a package that contains links, game files, player state or a private build location. It executes none of the packaged files.

Two limits apply:

- Windows imports cannot be resolved against a real system directory on a Linux host. The tool accepts packaged libraries, C runtime API sets, the Visual C++ runtime and a reviewed list of Windows components, and refuses any other name. Review a refused name before extending the list.
- Third-party notices are gathered by file name from the SDK's vendored components. Components whose terms appear only in source headers are listed by name with the pinned source location. This is not a legal audit of every linked file.

## Before publishing

- Run `python3 tools/run_tests.py` and confirm the Windows workflow passed for the released commit.
- Unpack each archive and run its launcher with `--check` and no game directory. It must report that no game folder has been chosen, without asking or starting anything.
- Confirm that the archives contain no `data`, `user-data` or `game-data.json`.
- State plainly what was not verified. Automated checks do not establish graphics, speech or gameplay, and a cross-compiled Windows build is untested until someone runs it on Windows.

## Publishing

Tag the released commit on `main` as `v<version>` with a signed tag. Mark alpha and beta versions as pre-releases. Attach both archives and their `.sha256` files only.

Release notes must say that the build is an alpha for testing and feedback, that no game files are included and that the first start asks for the player's own, and must link the [player guide](playing.md) and the issue tracker. List known limits, including anything untested.

## Record

- **0.1.0-alpha.1, 2026-11-14.** Ubuntu 24.04.5 root; Clang 20.1.2 and LLD 20 for all binaries; Microsoft C runtime 14.44 and Windows SDK 10.0.26100 through xwin 0.10.0; Python 3.14.10 embeddable runtime; Prism from the Windows workflow. Linux baseline measured at glibc 2.38 and `GLIBCXX_3.4.32`, with the same SDL video, audio and joystick backends as a normal build. The launchers' check mode ran from the unpacked Linux package; the game itself was not started from either package before publication, and nothing from the Windows package had run on Windows.
