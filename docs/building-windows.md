# Building on Windows

## Scope and prerequisites

This is the Windows x64 development build path. The public workflow checks the
open-source dependencies and original synthetic fixtures. A real game build,
graphics, speech, audio and controller behavior still need private Windows
testing. See [issue 38](https://github.com/RealAmethyst/BakuganBB-recomp/issues/38).

Keep dependencies, game files, generated code, logs and build output outside the
repository. Do not upload game inputs, translated code or native game binaries to
GitHub Actions, issues, caches, artifacts or releases. These commands do not
launch the game or load a screen reader.

Use Windows 10 or newer on x64, a CPU supporting x86-64-v2, Git, Python 3.12, CMake 3.25 or newer,
Ninja, LLVM/Clang and Visual Studio's x64 C++ build tools and Windows SDK.
Run PowerShell 7 in an x64 Visual Studio developer environment so `cl`, `link`,
`midl`, `dumpbin`, `clang`, `clang++` and `llvm-readobj` are on PATH. The initial
Windows dependency checks used the `windows-2025-vs2026` image, Clang 20.1.8 and
MSVC 19.51.36260.0. Older toolchains have not been validated. Prism requires
C23/C++23 library support; ReXGlue's Windows preset uses the GNU-style Clang
driver (`clang++`), not `clang-cl` or MinGW.

Playing also requires a suitable Vulkan driver and the supported game files.
The [Linux guide](building.md#before-you-start) records the executable identity;
the same identity check applies on Windows. No game acquisition steps are
provided.

## 1. Choose private working directories

Start in the repository. Select a new work directory and your existing game
directory. Paths with spaces are supported. Keep the variables in this
PowerShell session; the native-command setting stops a command block on failure.

```powershell
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$projectSource = (Get-Location).Path
$work = 'C:/brawlers-build'
$gameDirectory = 'C:/path/to/existing-game'
if (Test-Path $work) { throw 'Choose a new work directory' }
New-Item -ItemType Directory $work | Out-Null
```

## 2. Build and install the pinned SDK

The project's patch tool verifies the pinned revision and patch checksums.
Preserve checkout bytes so Git cannot rewrite checksummed source on Windows.

```powershell
$sdkManifest = Get-Content patches/rexglue/manifest.json -Raw | ConvertFrom-Json
git -c core.autocrlf=false clone --no-checkout https://github.com/rexglue/rexglue-sdk.git "$work/rexglue"
git -C "$work/rexglue" config core.autocrlf false
git -C "$work/rexglue" checkout --detach $sdkManifest.revision
git -C "$work/rexglue" submodule update --init --recursive -- `
  thirdparty/FFmpeg thirdparty/catch2 thirdparty/cli11 thirdparty/fmt `
  thirdparty/glslang thirdparty/imgui thirdparty/inja thirdparty/libmspack `
  thirdparty/o1heap thirdparty/sdl3 thirdparty/simde thirdparty/spdlog `
  thirdparty/spirv-headers thirdparty/spirv-tools thirdparty/tomlplusplus `
  thirdparty/tracy thirdparty/utfcpp thirdparty/vulkan-headers `
  thirdparty/vulkan-memory-allocator thirdparty/xxHash
python tools/apply_dependency_patches.py rexglue --source "$work/rexglue"
cmake --preset win-amd64 -S "$work/rexglue" -B "$work/sdk-build" `
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF `
  -DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_USE_D3D12=OFF `
  -DREXGLUE_USE_VULKAN=ON -DCMAKE_INSTALL_PREFIX="$work/sdk-install"
cmake --build "$work/sdk-build" --config Release --target install --parallel 2
```

The SDK installs its public headers, exported CMake targets, generator, runtime
and GPU plugin into the chosen prefix. Its upstream install also registers that
prefix in the current user's CMake package registry. The host explicitly selects
this prefix. It uses the upstream application helper for the Windows entrypoint
and DLL staging, and the upstream translation exception-handling requirement.
It does not reuse Linux linker flags or ELF library names.

## 3. Generate and compile privately

```powershell
python tools/generate_game.py --rexglue "$work/sdk-install/bin/rexglue.exe" `
  --game-executable "$gameDirectory/default.xex" --output "$work/translation"
cmake -S "$projectSource" -B "$work/native-build" -G Ninja `
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF -DBRAWLERS_BUILD_NATIVE=ON `
  -DBRAWLERS_REXGLUE_SOURCE_DIR="$work/rexglue" `
  -DBRAWLERS_REXGLUE_INSTALL_DIR="$work/sdk-install" `
  -DBRAWLERS_GENERATED_DIR="$work/translation/generated"
cmake --build "$work/native-build" --target battle-brawlers --parallel 2
```

The original project host links to the installed SDK; only the explicit source
list from the external translation is consumed. The source checkout remains
needed for the pinned JSON header, which the SDK does not install. The result
is under `native-build/native`. Do not run the source-only CI linkage fixture
as a game: its guest functions abort and its image descriptor is empty.

Generation failures retain private output for diagnosis. Choose a new output
directory to retry; do not bypass the executable identity check.

## 4. Build screen-reader support

Use a fresh Prism checkout and build directory. All backends default to off;
only the seven Windows screen-reader backends are enabled.

```powershell
$prismManifest = Get-Content patches/prism/manifest.json -Raw | ConvertFrom-Json
git -c core.autocrlf=false clone --no-checkout https://github.com/ethindp/prism.git "$work/prism"
git -C "$work/prism" config core.autocrlf false
git -C "$work/prism" checkout --detach $prismManifest.revision
python tools/apply_dependency_patches.py prism --source "$work/prism"
cmake -S "$work/prism" -B "$work/prism-build" -G Ninja `
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_SHARED_LIBS=ON -DPRISM_DEPENDENCY_PROVIDER=BUNDLED `
  -DPRISM_BACKEND_DEFAULT=OFF -DPRISM_ENABLE_NVDA_BACKEND=ON `
  -DPRISM_ENABLE_JAWS_BACKEND=ON -DPRISM_ENABLE_ZOOM_TEXT_BACKEND=ON `
  -DPRISM_ENABLE_SENSE_READER_BACKEND=ON -DPRISM_ENABLE_PC_TALKER_BACKEND=ON `
  -DPRISM_ENABLE_ZDSR_BACKEND=ON -DPRISM_ENABLE_BOY_PC_READER_BACKEND=ON `
  -DPRISM_ENABLE_POWER_MANAGEMENT=OFF -DPRISM_ENABLE_TESTS=OFF `
  -DPRISM_ENABLE_DEMOS=OFF -DPRISM_ENABLE_GDEXTENSION=OFF `
  -DPRISM_ENABLE_SHIMS=OFF -DPRISM_BUILD_WINELIBS=OFF `
  -DPRISM_REGENERATE_DJINNI=OFF -DPRISM_IPO=OFF
cmake --build "$work/prism-build" --target prism --parallel 2
```

Prism's C API uses `__cdecl`, matching the existing Python `ctypes.CDLL` adapter.
The NVDA backend compiles its own RPC client; no `nvdaControllerClient64.dll` is
needed. Other readers may use their installed COM servers or optional vendor
helpers. The build does not redistribute those helpers or enable standalone TTS.
Compiling seven backends does not prove that all seven work at runtime.

## 5. Stage and check the private installation

```powershell
python tools/stage_game.py --platform windows `
  --native-build "$work/native-build" --prism-build "$work/prism-build" `
  --prism-source "$work/prism" --rexglue-source "$work/rexglue" `
  --game-dir "$gameDirectory" --output "$work/game"
python "$work/game/launch.py" --check
```

Staging requires a new output directory. It checks the Windows backend
configuration, audits binary architecture and dependencies without loading the
DLLs, and retains the license notices. Virtual Windows API-set imports are
checked with the OS availability query; missing physical DLLs and unavailable
contracts remain errors. See [Microsoft's API-set contract](https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-apisets). `game-data.json` points to the external
game directory; no administrator access, junction or symbolic link is required.
The check-only launcher does not load Prism, launch a process or create user data.

When ready for manual testing, the tester starts their preferred screen reader
and launches the private build:

```powershell
python "$work/game/launch.py"
```

The launcher uses shared speech policy and Windows file locking. Settings, saves,
caches and private logs stay under `game/user-data`. Absence of a supported
screen reader remains silent. A second launcher for the same installation
should report that it is already running.

## Optional spatial-audio development build

The same `BRAWLERS_STEAM_AUDIO_SDK_DIR` option accepts external Steam Audio 4.8.1
headers. Windows loads `bin/phonon.dll`; Linux loads `bin/libphonon.so`. File
loading preserves Unicode paths. The SDK's function-pointer types retain its
Windows calling convention.

Standard staging does not yet prepare this optional engine's runtime files or
private clips. Follow the [audio development record](accessibility-audio.md)
for the clip format and offline boundaries. Keep the SDK license and all
third-party notices. GPU acceleration dependencies must not be inferred from
a successful CPU binaural build. Cue behavior and actual output need separate
listening checks; see [issue 31](https://github.com/RealAmethyst/BakuganBB-recomp/issues/31).

## Testing handoff

Automated compilation and synthetic checks cannot establish playable behavior.
A Windows tester should report the source commit, toolchain, GPU/driver,
screen reader and controller, then check:

- A private build completes and `launch.py --check` passes.
- Startup and menu speech are audible; movement interrupts correctly and opening
  announcements stay in order. No standalone voice should replace a missing reader.
- Game audio plays, controller navigation works, and dialogs accept and cancel input.
- Settings persist, a second launch is rejected, and closing then reopening works.
- Optional spatial cues work only if that development feature was built and its
  private resources were prepared.

Keep raw logs private; review and redact any diagnostic excerpts before sharing.

## Upstream build contracts

This integration follows the pinned sources, not the latest branch defaults:

- [ReXGlue presets](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/CMakePresets.json),
  [application helpers](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/cmake/rexglue_helpers.cmake),
  [generated build template](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/resources/templates/init/rexglue_cmake.inja).
- [Prism Windows build](https://github.com/ethindp/prism/blob/b68e9c94240cefb1f08b2306c26376e3ab300a7a/.github/workflows/build-windows.yml),
  [C ABI](https://github.com/ethindp/prism/blob/b68e9c94240cefb1f08b2306c26376e3ab300a7a/include/prism.h),
  [backend and CRT options](https://github.com/ethindp/prism/blob/b68e9c94240cefb1f08b2306c26376e3ab300a7a/cmake/PrismOptions.cmake).
