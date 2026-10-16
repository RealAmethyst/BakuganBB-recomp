# Building on Linux

## Before you start

This guide builds the game locally from your own files. It does not download game files or provide a prebuilt game. Keep the game, dependencies, generated code and build output outside this repository. Do not upload the resulting installation or generated files.

The current target is Linux on a CPU supporting x86-64-v2. Other platforms are not supported by these commands. You need:

- Git, Python 3.9 or newer, CMake 3.25 or newer, Ninja, Make, pkg-config and Binutils (`readelf`).
- Clang 18 or newer and a C++23 standard library for ReXGlue and the native game. Prism also needs a C23/C++23 toolchain supporting `std::expected` and `std::jthread`.
- Development libraries providing `glibmm-2.68` and `giomm-2.68`, version 2.68.0 or newer, for Prism.
- The development dependencies for SDL's Linux window, input and audio backends. Check the SDK configure summary: a successful build with missing platform backends is not proof that your desktop setup is supported.
- For playing: a Vulkan driver with scalar block layout support. Minimum GPU requirements have not been established.
- Your game directory, containing `default.xex` and the remaining game files.

The only supported executable currently has size **11,055,104 bytes** and SHA-256 **`7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b`**. The generation tool checks this before using the game-specific hooks. Other versions are not automatically compatible. See the [executable baseline](executable-baseline.md).

These are large builds. Start with two parallel jobs, as below. No step installs system libraries, changes a screen reader, or launches the game automatically.

## 1. Choose working directories

Start in this repository. Set `GAME_DIR` to your existing game directory and choose a new external directory for `WORK`:

```sh
PROJECT_SOURCE="$(pwd)"
GAME_DIR="/path/to/your/game-directory"
WORK="$HOME/brawlers-build"
mkdir "$WORK"
```

Use fresh dependency checkouts and build directories. The patch and generation tools reject mismatched source, existing changes or unsupported game files instead of guessing.

## 2. Build the patched SDK

Only open-source dependencies are cloned here. Their versions are pinned; do not substitute a newer SDK without rechecking the integration.

```sh
git clone --no-checkout https://github.com/rexglue/rexglue-sdk.git "$WORK/rexglue"
git -C "$WORK/rexglue" checkout --detach c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C "$WORK/rexglue" submodule update --init --recursive -- \
  thirdparty/FFmpeg thirdparty/catch2 thirdparty/cli11 thirdparty/fmt \
  thirdparty/glslang thirdparty/imgui thirdparty/inja thirdparty/libmspack \
  thirdparty/o1heap thirdparty/sdl3 thirdparty/simde thirdparty/spdlog \
  thirdparty/spirv-headers thirdparty/spirv-tools thirdparty/tomlplusplus \
  thirdparty/tracy thirdparty/utfcpp thirdparty/vulkan-headers \
  thirdparty/vulkan-memory-allocator thirdparty/xxHash
python3 "$PROJECT_SOURCE/tools/apply_dependency_patches.py" rexglue \
  --source "$WORK/rexglue"
cmake --preset linux-amd64 -S "$WORK/rexglue" -B "$WORK/sdk-build" \
  -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_USE_VULKAN=ON
cmake --build "$WORK/sdk-build" --config Release \
  --target rexglue rexgpu-xenos --parallel 2
```

The SDK writes its executables and libraries to `WORK/rexglue/out/linux-amd64/Release`, even though the CMake build directory is separate. Its [patch guide](../patches/rexglue/README.md) explains the corrections and retained notices.

## 3. Generate and compile the game

```sh
python3 "$PROJECT_SOURCE/tools/generate_game.py" \
  --rexglue "$WORK/rexglue/out/linux-amd64/Release/rexglue" \
  --game-executable "$GAME_DIR/default.xex" \
  --output "$WORK/translation"
cmake -S "$PROJECT_SOURCE" -B "$WORK/native-build" -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DBRAWLERS_BUILD_NATIVE=ON \
  -DBRAWLERS_REXGLUE_SOURCE_DIR="$WORK/rexglue" \
  -DBRAWLERS_REXGLUE_BUILD_DIR="$WORK/sdk-build" \
  -DBRAWLERS_REXGLUE_OUTPUT_DIR="$WORK/rexglue/out/linux-amd64/Release" \
  -DBRAWLERS_GENERATED_DIR="$WORK/translation/generated"
cmake --build "$WORK/native-build" --target battle-brawlers --parallel 2
```

The translation directory contains private generated game code and generation logs. The native output is under `WORK/native-build/native`. Neither belongs in a commit, issue attachment or release upload.

If generation fails, inspect `translation/codegen.log`. The tool preserves failed output for diagnosis; choose a new output directory for another attempt. Do not bypass an executable identity failure.

### Optional spatial-audio development build

The native target can also compile the accessibility audio engine with
`-DBRAWLERS_STEAM_AUDIO_SDK_DIR=/path/to/external/steamaudio`. The directory must
contain the Steam Audio 4.8.1 headers at `include/phonon.h`. It must remain outside
the repository. Omitting this option preserves the build without the spatial
audio dependency.

Runtime initialization expects `bin/libphonon.so` and private mono float32,
48 kHz clips at `accessibility-audio/gate-friendly.wav` and
`accessibility-audio/gate-enemy.wav`, relative to the installation root. These are
single sound cycles; the engine adds an equally long silent interval. Keep the
SDK license and third-party notices with a private installation. No SDK binary
or game-derived clip belongs in source control.

The standard staging tool does not yet prepare those optional files. This is a
development integration, with cue behavior and gameplay validation tracked in
[issue 31](https://github.com/RealAmethyst/bb-recomp/issues/31). Release asset
extraction remains deferred. See the [audio record](accessibility-audio.md) for
the verified preparation and offline testing boundaries.

## 4. Build screen-reader support

```sh
git clone --no-checkout https://github.com/ethindp/prism.git "$WORK/prism"
git -C "$WORK/prism" checkout --detach b68e9c94240cefb1f08b2306c26376e3ab300a7a
python3 "$PROJECT_SOURCE/tools/apply_dependency_patches.py" prism --source "$WORK/prism"
cmake -S "$WORK/prism" -B "$WORK/prism-build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DPRISM_DEPENDENCY_PROVIDER=BUNDLED -DPRISM_BACKEND_DEFAULT=OFF \
  -DPRISM_ENABLE_ORCA_BACKEND=ON -DPRISM_ENABLE_POWER_MANAGEMENT=OFF \
  -DPRISM_ENABLE_TESTS=OFF -DPRISM_ENABLE_DEMOS=OFF \
  -DPRISM_ENABLE_GDEXTENSION=OFF -DPRISM_ENABLE_SHIMS=OFF \
  -DPRISM_BUILD_WINELIBS=OFF -DPRISM_REGENERATE_DJINNI=OFF -DPRISM_IPO=OFF
cmake --build "$WORK/prism-build" --target prism --parallel 2
```

This enables Orca only and prevents automatic speech-service startup. It does not load Prism or test speech. Use a fresh build directory so old backend settings cannot enable standalone speech. See the [Prism patch guide](../patches/prism/README.md) for details and licensing.

## 5. Prepare your local installation

```sh
python3 -B "$PROJECT_SOURCE/tools/stage_game.py" \
  --native-build "$WORK/native-build" --prism-build "$WORK/prism-build" \
  --prism-source "$WORK/prism" --rexglue-source "$WORK/rexglue" \
  --game-dir "$GAME_DIR" --output "$WORK/game"
python3 "$WORK/game/launch.py" --check
```

The staging tool copies the built code, launcher and notices into a new private directory. It links to your existing game directory without copying or modifying it. The destination must be separate from all inputs. Runtime library paths are adjusted only in the copied files.

The check command checks required files without starting the game or speech. When you are ready to play, run:

```sh
python3 "$WORK/game/launch.py"
```

Speech goes to an already running Orca. Without one, the launcher remains silent. Writable settings, saves, caches and logs stay under `WORK/game/user-data`. Keep the original game directory available while using this installation.

This produces a local build, not a portable release package. It has not been tested across Linux distributions. A future downloadable application that asks for game files needs separate packaging, compatibility and distribution work.

## Automated code checks

These checks use made-up data rather than running the game, so no game files or SDK are needed. Install system Python 3.9+, CMake 3.20+, a C++17 compiler, Make and Bubblewrap, then run:

```sh
python3 tools/run_tests.py
```

Bubblewrap must support unprivileged namespaces. The runner isolates the tests from the desktop, accessibility bus and audio devices. It will not retry without isolation. Use `--build-dir /path/to/new-external-directory` to retain test output.

The default checks cover the shared core, speech policy and launcher. SDK-dependent fixtures remain separate; see [integration.md](integration.md). Passing code checks does not establish graphics, speech or gameplay usability.

The settings adapters have additional isolated synthetic checks. Use the patched
SDK and its dependency checkout; each output directory must be new:

```sh
python3 tests/runtime/settings_runtime_regressions.py "$WORK/rexglue" \
  "$WORK/rexglue/thirdparty" "$WORK/settings-runtime-checks"
python3 tests/runtime/native_settings_render_regressions.py "$WORK/rexglue" \
  "$WORK/rexglue/thirdparty/simde" "$WORK/settings-render-checks"
python3 tests/runtime/native_settings_guards.py "$WORK/settings-guard-checks"
python3 tests/runtime/native_settings_lifecycle.py "$WORK/rexglue" \
  "$WORK/rexglue/thirdparty/simde" "$WORK/settings-lifecycle-checks"
```

These fixtures use synthetic window, audio and guest-memory boundaries. They do
not open a game window, load audio output or connect to accessibility services.

The VS capture fixtures also use synthetic memory and isolated native boundaries:

```sh
python3 tests/runtime/native_vs_regressions.py "$WORK/rexglue" \
  "$WORK/sdk-build" "$WORK/rexglue/thirdparty" "$WORK/vs-capture-checks"
```

They compile with AddressSanitizer and UndefinedBehaviorSanitizer. Passing them
does not establish complete menu coverage or in-game listening usability.

## Verification so far

On 2026-10-14, the normal SDK generator, runtime and Vulkan plugin built from the complete public patch series. Fresh game generation retained the verified functions and hooks, and the public CMake target compiled and linked successfully. Prism also built from its published patch. The staged installation passed the check-only preflight without loading Prism or launching the game.

This was checked with the pinned dependency sources already available locally, using isolated build environments. Downloading dependencies and following the guide on a clean machine, other Linux distributions, graphics and gameplay still need testing. The SDK and Prism retain upstream compiler warnings; the native application build had no compiler warnings. [Issue 24](https://github.com/RealAmethyst/bb-recomp/issues/24) tracks this build work.
