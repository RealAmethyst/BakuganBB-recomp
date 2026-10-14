# Linux static recompilation toolchain trial

Inspection date: 2026-10-14. Owning investigation: [issue 4](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/4).

## Unmodified baseline result

ReXGlue revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` built successfully on Linux without source edits. All 1,547 Release build actions completed, including the runtime, CLI, Vulkan plugin and synthetic PowerPC assembly-to-C++ test pipeline.

The unmodified test suite did **not** pass: 1,671 discovered tests produced 1,662 passes, five failures and four upstream skips. All 1,458 synthetic PowerPC cases passed. The isolated candidate fixes described below subsequently eliminate those five failures. Neither result establishes dependency adoption or game compatibility. No game executable was translated or run in this trial. Vulkan compilation does not establish rendering correctness.

## Reproducible configuration

Host: Linux `7.2.9-2-cachyos`, x86-64, glibc 2.44. Tools: Clang 23.1.1, CMake 4.4.4, Ninja 1.13.2, Python 3.14.7 and GCC/libstdc++ 16.2.1. Upstream's release path uses Clang 20; the newer local toolchain is a variable in these results.

From the pinned source checkout, equivalent relative-path commands are:

```sh
cmake --preset linux-amd64 -B ../linux-toolchain-trial/build -DREXGLUE_BUILD_TESTS=ON -DREXGLUE_ENABLE_TRACY=OFF -DCMAKE_INSTALL_PREFIX=../linux-toolchain-trial/install
cmake --build ../linux-toolchain-trial/build --config Release --parallel 2
```

Configuration: Ninja Multi-Config, C++23, x86-64-v2, Vulkan enabled, D3D12 disabled, FidelityFX disabled by upstream default. Tracy was disabled for this offline trial; performance counters retained their default enabled setting. No full-build sanitizer or warning suppression was added. [Build configuration](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/CMakeLists.txt).

Twenty Linux source submodules and their recursive dependencies were fetched at pinned gitlinks. Apple-only MoltenVK and Vulkan-Loader were not needed for this build path. The bundled GNU Binutils 2.24 assembler includes VMX128 support. The Vulkan manifest specifies SDK 1.4.357.0; its validator and three synthetic manifest checks passed, including a deliberate pin mismatch. Full dependency hashes and installed library versions are retained in the private `environment.json` and submodule log.

All required tools were already installed. No system installation or SDK installation was performed. The upstream install step writes the user's CMake package registry; installation behavior remains outside the trial.

## Test scope and isolation

The complete Release CTest suite ran once, serially, with no filters or exclusions. Tests use upstream synthetic instructions, buffers, files, fake controllers and memory, with no game inputs. The 1,458 PowerPC cases come from 166 assembly fixtures. The remaining 213 unit tests produced 204 passes, five failures and four skips.

Bubblewrap isolated network, IPC, PID, devices, runtime and temporary directories. The environment was cleared; source and system tools were read-only, and only dedicated trial scratch was writable. An isolation check confirmed that host desktop runtime, user configuration and audio devices were absent. No live desktop, screen reader, game, UI or audio output was exercised.

Private evidence includes `test-command.json`, `test-isolation.log`, `ctest-results.xml`, `ctest-release.log`, configuration/build logs and `evidence-sha256.json`. CTest exited with status 8. Passing synthetic cases validate their encoded expectations, not complete instruction coverage or this game's control flow.

## Diagnosed failures

### Host clock range

Four chrono tests fail around FILETIME zero, representing 1601-01-01: calendar decomposition, known-date recomposition, decomposition/recomposition round-trip, and whole-second clock round-trip. One result becomes year 2185 instead of 1601.

`NtSystemClock::to_sys` converts an epoch-adjusted 100-nanosecond duration to the host clock duration. This Linux standard library uses signed 64-bit nanoseconds, which cannot represent 1601. An original arithmetic-only probe reproduced signed integer overflow under UBSan: multiplying `-116444736000000000` by 100 exceeds the representation. This establishes a host duration-range defect on the tested configuration; the full SDK was not sanitizer-tested, and impact on Bakugan is unknown. [Clock conversion](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/chrono/chrono.h#L75).

### Depfile expectation

The fifth failure is `Depfile escapes characters make treats as syntax`. Expected C++ strings use invalid escape sequences for space and `#`; Clang warns and drops the backslashes. The implementation explicitly emits those backslashes. The observed failure therefore identifies a defective test expectation, not demonstrated failure of that implementation. It does not establish correctness for all make syntax. [Test literals](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/tests/unit/codegen/output_stamp_test.cpp#L227), [escaping implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/codegen/output_stamp.cpp#L103).

### Existing skipped coverage

Four `BitStream::Write` tests explicitly skip: byte-aligned writes, preservation of surrounding bits, non-byte-aligned writes and 16-bit writes. Upstream states that stores do not byte-swap. These are unresolved coverage/implementation limitations, not passing tests or exclusions added for this trial. Actual use by the target remains unknown. [Skipped cases](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/tests/unit/core/stream_test.cpp#L296).

## Verified candidate fixes

A separate checkout at the same pinned revision now contains two focused candidate changes: [clock range, issue 7](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/7), and [depfile expectations, issue 8](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/8). The original baseline source and build remain unchanged.

The separate [candidate patches, license and hash manifest](patches/rexglue/README.md) are retained locally. Both apply cleanly to the pinned baseline. Dependencies were copied from its pinned source folders; their copied `.git` pointers are not independently initialized submodule repositories. Candidate status therefore uses explicit file diffs or ignores submodule metadata. No dependency upgrade was made.

Clock conversion now returns a system-epoch time point with 100-nanosecond duration, retaining the NT clock's precision and range instead of implicitly narrowing to native Linux nanoseconds. Calendar recomposition retains milliseconds until it has validated the date against the signed NT limit. Invalid fields return false without changing the guest's output. No exception path was introduced.

The supported absolute FILETIME contract is zero through `INT64_MAX`. Negative values and the upper unsigned half remain outside that contract; unchecked low-level calls can still overflow for those inputs. This is not a checked arbitrary-range chrono API, and the void time-to-fields export's invalid-input behavior remains unresolved. `to_sys` now returns `sys_time<hundrednanoseconds>`, so external callers that explicitly require a native clock duration must check representability and cast deliberately. The inspected SDK callers remain compatible. Windows and macOS effects were source-reviewed, but native builds on those platforms were not run.

The depfile change corrects expected string literals and adds a dollar-sign case. Production depfile escaping remains unchanged. A GNU make check accepts the actual generated dependency file and requires each special-character input. Removing each required escape causes failure; an intentionally unescaped production mutant also fails all three new/updated expectations.

The candidate full Release build succeeded. Its full serial CTest run discovered **1,675 tests: 1,671 passed, zero failed, four skipped**. The four additional chrono cases cover supported tick endpoints, source precision, dates outside native nanosecond range, and actual runtime exports using original synthetic guest memory. All 1,458 PowerPC cases still pass. The same four upstream BitStream skips remain unresolved.

An isolated UBSan probe of the actual clock header passed 100,000 sampled nonnegative signed-range values, with endpoint and clock-cast checks. This is focused sanitizer evidence, not a sanitizer run of the complete SDK. A read-only review found no correctness blocker within the documented contract. Build/test execution preserved the private desktop-safe isolation used by the baseline trial.

## Decision and remaining evidence

Continue evaluating ReXGlue with both the unmodified baseline and the separately verified candidate patches retained. The original five failures are fixed in that candidate; the four upstream skips remain visible. No tests were suppressed and no upstream submission, commit or push was made. A local candidate patch is not an upstream fix or permanent dependency adoption.

Target control-flow coverage, runtime semantics, shader behavior, graphics, audio, input and accessibility remain separate requirements. The subsequent [first-build investigation](first-build-gates.md) corrected a gap-fill defect, completed private translation and linked a native Linux host. That report owns current translation, graphics/audio and test-readiness evidence. The skipped writer's relevance and complete runtime behavior remain unresolved; no playable milestone has been established.
