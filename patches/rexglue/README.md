# ReXGlue evaluation patches

These separate candidate changes apply to ReXGlue revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. They are retained for reproducible evaluation, not permanent dependency adoption. Application order and file identities are recorded in `manifest.json`. Apply `gapfill-backward-exit.patch` before `gapfill-indirect-terminator.patch`; both modify the same partitioner. Apply `clock-timing.patch` after `chrono-range.patch`; both modify clock headers.

- [chrono-range.patch](chrono-range.patch): preserve 100-nanosecond clock range, validate calendar recomposition, and add boundary/actual-export tests. Tracked by [issue 7](https://github.com/RealAmethyst/BakuganBB-recomp/issues/7).
- [depfile-expectations.patch](depfile-expectations.patch): correct expected escapes and add dollar-sign coverage; production escaping is unchanged. Tracked by [issue 8](https://github.com/RealAmethyst/BakuganBB-recomp/issues/8).
- [scalar-layout.patch](scalar-layout.patch): consistently require and enable the shader uniform layout on Vulkan core and extension paths. The [diagnostic-launch record](../../docs/diagnostic-launch.md) owns the 19-case device-contract checks and build evidence. Tracked by [issue 10](https://github.com/RealAmethyst/BakuganBB-recomp/issues/10).

- [gapfill-backward-exit.patch](gapfill-backward-exit.patch): discover leaves following backward gap exits. [Issue 9](https://github.com/RealAmethyst/BakuganBB-recomp/issues/9) records local candidate verification; [gapfill-regressions.cpp](gapfill-regressions.cpp) is the original 21-case standalone fixture, including all ten earlier cases. It accepts an output JSON filename and runs the actual Scan/Discover/GapFill/Merge/Validate phases. Build it against the pinned SDK codegen/test-support headers and libraries; compare baseline linkage with the candidate phase object linked before librexcodegen. Exact toolchain/link commands remain in the private evidence collection. The fixture was extended with indirect-tail, conditional, linked-call and switch-ownership controls.

- [gapfill-indirect-terminator.patch](gapfill-indirect-terminator.patch): discover unseeded leaves following unconditional CTR tails. The 21-case fixture passes 17 cases before and all 21 after this addition. [Issue 13](https://github.com/RealAmethyst/BakuganBB-recomp/issues/13) and the [first-launch failure record](../../docs/first-launch-failure.md) own the crash trace, registration audit and replacement build.
- [emitting-owner.patch](emitting-owner.patch): classify emitted branches using the known function entry when instruction-address lookup is ambiguous. [emitting-owner-regressions.cpp](emitting-owner-regressions.cpp) drives actual C++ emission through 12 original cases: baseline 8/12, candidate 12/12, including a deliberately unresolved target that retains its error. It accepts an output directory. Tracked by [issue 14](https://github.com/RealAmethyst/BakuganBB-recomp/issues/14).

- [clock-timing.patch](clock-timing.patch): preserve fractional counter progress and ordered sampling, correct signed and scaled deadline conversions, and align Linux counter units with frequency. [clock-timing-regressions.cpp](clock-timing-regressions.cpp) compiles actual SDK clock sources with controlled clock/configuration support in `clock-test-support`. [Issue 16](https://github.com/RealAmethyst/BakuganBB-recomp/issues/16) and the [timing record](../../docs/timing-contracts.md) own the 66-case results, arithmetic reference checks and real-runtime stress comparison.
- [alertable-zero-readiness.patch](alertable-zero-readiness.patch): inspect ready objects before returning timeout from expired alertable POSIX waits. [alertable-readiness-regressions.cpp](alertable-readiness-regressions.cpp) links the actual runtime and preserves APC priority and existing wait semantics; baseline 22/31, candidate 31/31. Tracked by [issue 17](https://github.com/RealAmethyst/BakuganBB-recomp/issues/17).
- [presentation-timing.patch](presentation-timing.patch): optional cumulative swap/placeholder and Vulkan acquire/present diagnostics, disabled by default in the SDK and enabled by the diagnostic host. Records CPU/API observations, not physical scanout or GPU execution time. The [timing record](../../docs/timing-contracts.md) owns verification and limits.

- [idle-toast-repaint.patch](idle-toast-repaint.patch): stop empty achievement notifications from driving continuous UI repaint, retain ordinary-dialog wake/animation, and marshal notification ownership onto the UI thread. [Issue 19](https://github.com/RealAmethyst/BakuganBB-recomp/issues/19) and the [timing record](../../docs/timing-contracts.md) own the 33-case actual-method synthetic checks, independent review and integration evidence. Exact fixture generation, commands and source hashes are retained in the private evidence collection. This changes UI class layouts; rebuild the runtime and every application/plugin consumer together. Windows/macOS execution remains untested.

The [first-build report](../../docs/first-build-gates.md) owns gap-fill, generated-code compilation and graphics/audio results. The [trial report](../../docs/linux-toolchain-trial.md) owns build/test results and limitations. Patch identities are in [manifest.json](manifest.json). Apply to a separate checkout at the stated revision; retain the unmodified baseline for comparisons. No game inputs are involved.

## Keyboard dialog candidate

[keyboard-dialog.patch](keyboard-dialog.patch) adds an optional UI-thread observer for actual keyboard-dialog lifetimes, focus and editing. It scopes keyboard navigation to those dialogs, adds Escape cancellation, corrects the primary cancellation result and enforces the guest UTF-16 capacity without losing an existing name suffix. The [Story investigation](../../docs/story-entry-freeze.md) owns the game consumer trace and user-selected interaction. This does not establish the cause of the reported Story freeze or provide main-menu narration.

[keyboard-dialog-regressions.py](keyboard-dialog-regressions.py) extracts the patched dialog and completion callback into an original synthetic harness linked with pinned ImGui core. In an isolated environment, run `python keyboard-dialog-regressions.py /path/to/patched-sdk /path/to/new-output`. The output directory must not exist. Clang ASan/UBSan checks cover actual opening/focus/edit/close events, visible title suffix handling, keyboard navigation, optional/throwing observers, UTF-8 positions, UTF-16 capacities and cancellation without output writes. It uses no platform/render backend, game input or live clipboard. The fake dialog base and guest-buffer writer limit coverage; the full application and listening behavior still need user testing.

Install the observer before runtime threads start, or clear it after they stop. Callbacks must copy retained event data and must not reenter UI drawing. The pinned ImGui configuration still rejects newly entered supplementary Unicode characters; this patch does not change its character ABI. Native Windows/macOS builds remain untested.

## Message-box observer candidate

[message-box-observer.patch](message-box-observer.patch) adds an optional observer for actual message-box opening, focused buttons and native closing. Title and button events use the captions ImGui displays, including its hidden `##` suffix handling. Body text remains literal. Button positions come from the actual button list. Install or clear the observer only while runtime threads are stopped; callbacks are synchronous on the UI thread and must copy any retained values. Observer exceptions do not prevent native completion.

Applied alone, the observer patch preserves existing controls, requested-default handling and completion results: actual initial focus remains on the first button even when the initialized result names another requested default. It reports real ImGui focus rather than inferring focus from that result. The later popup-controller patch corrects initial focus to the requested button.

[message-box-regressions.py](message-box-regressions.py) extracts the actual dialog and completion callback and links pinned ImGui core. The current fixture targets the combined patch stack, including popup-controller.patch; it expects actual focus on the requested default and deliberately fails that assertion against the preceding observer-only implementation. In an isolated environment, run `python3 message-box-regressions.py /path/to/patched-sdk /path/to/new-output`. It also verifies opening/focus/closing, visible captions, positions, no idle repetition, native selection/completion, and absent or throwing observers under ASan/UBSan. It uses no platform/render backend, game input or live clipboard. Its fake dialog base limits application-lifetime coverage.

## Host popup controller candidate

[popup-controller.patch](popup-controller.patch) follows the observer and guarded-read patches. It gives the default SDL input backend D-pad Left/Right navigation and A confirmation for focused host message boxes. The actual requested button receives initial focus. Native game-rendered popups keep their original controls.

A window-owned value channel connects the existing SDL event watch to ImGui's gamepad events. Modal capture suppresses guest input until dismissal, then masks captured digital presses until their releases have reached the drained driver state. Sequence tracking preserves fresh release/repress input without exposing stale cached presses. Child attachment, focus changes and disconnect invalidate queued UI edges. Message construction and completion/fence installation occur together on the UI thread.

[popup-controller-regressions.py](popup-controller-regressions.py) exercises actual dialog, drawer input, channel, dispatch, and SDL buffering methods using synthetic events and pinned ImGui core. Run inside an isolated filesystem/runtime without desktop or input access: `python3 popup-controller-regressions.py /path/to/patched-sdk /path/to/new-output`. Actual-source ASan/UBSan fixtures and separate concurrent channel/driver TSan runs pass. The updated message fixture rejects the preceding first-button focus behavior and accepts the requested default; the existing keyboard fixture also passes.

Physical controller and listening checks remain necessary. The alternate Windows XInput backend is unchanged and does not gain this navigation. Windows/macOS execution was not tested. Existing shutdown ownership must stop guest calls before destroying the driver/window; this patch does not make concurrent object destruction safe. `XamShowMessageBoxUIEx` remains outside coverage.

## Guarded guest-read candidate

[safe-guest-read.patch](safe-guest-read.patch) adds `Memory::TryReadVirtual(address, destination, length)`. It holds the runtime global critical region, the selected heap mutex, and the physical parent mutex when applicable across validation and copying. It supports all three guest physical aliases. Every covered guest page and physical parent page must be committed and readable; watched physical pages fail closed without invoking callbacks. Null/empty, overflowing, missing-heap and cross-heap ranges are rejected without changing the destination.

The caller owns a writable destination outside the guest arena. This protects mapping/access lifetime; it does not establish game-object ownership or make concurrently written contents an atomic snapshot. Initialization, reset, save/restore, disposal and MMIO registration remain quiescent lifecycle operations. Current MMIO windows lie outside the managed heaps and are rejected.

Acquire the global critical region before other caller-held locks. Keep output and other I/O outside that region; an observer can enqueue copied values for a separate output worker. The guarded reader itself performs no output or guest calls.

[safe-guest-read-regressions.py](safe-guest-read-regressions.py) provides an original synthetic Linux x86-64 fixture with generic input paths. Run it inside an isolated environment with a patched SDK and a matching built runtime library:

```sh
python3 safe-guest-read-regressions.py /path/to/patched-sdk /path/to/new-output \
  --library /path/to/librexruntime.so --build-include /path/to/sdk-build/include
```

The fixture checks the complete modified translation unit, then compiles exact extracted read/protect/decommit methods and links the SDK's real allocation, mapping, protection and lock implementations. It uses UBSan with errors non-recovering. SDK fixed-address arena mapping conflicts with ASan's shadow layout, so this real-mapping fixture does not use ASan or add fault-recovery shims.

Coverage includes all seven heap ranges, cross-page rejection, physical watch and parent-protection rejection, release, argument boundaries, and both current MMIO windows. Three reader threads perform 360,000 racing reads against 24,000 protect/decommit cycles across virtual and physical heaps. Controlled inaccessible samples avoid assuming a particular scheduler interleaving. Failed reads must leave destination bytes intact. Linux with 4 KB host pages is tested; Windows/macOS and coarse host-page execution remain untested.

## Integrated patch verification

On 2026-10-14, all 14 patches in manifest order applied cleanly to a fresh archive of the pinned revision, with every manifest file hash verified. The final controller SDK build's mounted source matched 27 of the 31 affected files byte for byte. The comparison resolved bind mounts in order, including directory mounts that replace earlier individual header mounts. Matching files include the combined toast/keyboard/message-box/controller changes and guarded-read implementation.

The public keyboard, message-box and popup-controller fixtures passed ASan/UBSan against the matching combined sources. Concurrent channel and actual SDL driver-drain fixtures also passed TSan. The requested-default focus assertion failed against the preceding observer-only class as expected. The keyboard fixture conditionally includes the message-box observer header so its extracted shared observer namespace compiles after both patches.

The public guarded-read fixture previously passed against its built runtime under UBSan; its affected sources are unchanged in this final comparison. Tests ran with isolated filesystems, runtime directories and networking, without UI backends or access to live desktop services. These source and synthetic checks do not establish physical-controller or listening behavior.

Four codegen inputs in that runtime build remained at the pinned baseline: `include/rex/codegen/function_graph.h`, `src/codegen/builders/context.cpp`, `src/codegen/builders/control_flow.cpp`, and `src/codegen/phase_gapfill.cpp`. That runtime build therefore does not establish integration of the codegen patches. Their separate candidate-object and generated-code evidence is described above; do not equate successful patch application with a complete codegen build.

## Checked configuration saving

[checked-config-save.patch](checked-config-save.patch) adds a checked persistence
API for the [in-game settings redesign](../../docs/settings.md). It preserves
the existing void `SaveConfig` entry point as a wrapper. New callers can save an
explicit immutable list of registered name/value pairs, including values pending
a restart, without changing the live registry, invoking setters or recording
restart flags. Assignment, OS application and persistence are separate outcomes.

The checked operation validates the complete batch, merges it with existing TOML,
removes flat/nested aliases for selected defaults, and retains unrelated key
values. The formatter escapes strings. Comments and formatting may normalize.
Invalid input, invalid existing TOML, table collisions and nonregular destination
files fail closed. The parent directory must already exist.

An exclusively created sibling temporary file is written, synced and closed
before atomic replacement. Failures before replacement leave the original file
unchanged. Calls are serialized within the process; external writers require
exclusive ownership. Do not call from a registry change callback or mistake a
persisted snapshot for successful display application. Directory-entry durability
across power loss is not guaranteed.

[checked-config-save-regressions.py](checked-config-save-regressions.py) compiles
the actual patched registry/parser/persistence translation unit and uses synthetic
configuration, with logging/environment boundary stubs and injected filesystem
failures. Run inside an isolated Linux environment:

```sh
python3 checked-config-save-regressions.py /path/to/patched-sdk /path/to/new-output
```

On 2026-10-15 this fixture passed under AddressSanitizer and UndefinedBehaviorSanitizer
on Linux. It covers default-only and aliased resets, escaped strings, preservation
of unrelated values, immutable pending values, invalid/command/nonfinite rejection,
partial writes and write/sync/close/replace failures, temporary-file collisions and
cleanup, interrupted writes, nonregular destinations and concurrent disjoint saves.
An unpatched-source control reproduced the stale final-override defect. The full
15-patch ordered series also passed the checksum/application check against the
pinned clean SDK. These checks do not establish runtime UI integration.

The fixture was initially compiled and run directly with logging/environment
stubs and private scratch files. Its final binary then passed in Bubblewrap with
private filesystem/runtime/configuration resources, no desktop buses, display,
host home, network or audio devices. No live accessibility service was used.

The existing loader and numeric getters use `std::to_string` for doubles, which
limits decimal precision. File snapshot fidelity does not prove arbitrary
floating-point runtime reload equality. Menu values must stay within their
verified discrete ranges; changing loader precision is outside this patch.

Callers must also coordinate pending restart values with the existing all-flags
Save action, which otherwise snapshots current live values and can replace those
pending choices. The new primitive does not provide a shared settings owner.

The Windows implementation has not been compiled or run. This prerequisite does
not implement the in-game menu, native save adapter, or automatic save scheduling.
The in-game settings owner now connects this primitive; deployment and
interactive acceptance are tracked in issue 33.

## Host display settings

[settings-display.patch](settings-display.patch) follows checked-config-save in
the manifest. It adds checked UI-thread fullscreen/window-size requests and
observable completion, plus deferred presentation VSync refresh. Host dimensions
and presentation preferences are separate from guest video-mode and vblank flags.
Fullscreen completion uses matching SDL events; rejected requests retain errors.
Requests do not claim success merely because a configuration value was accepted.

[settings-display-regressions.py](settings-display-regressions.py) applies the
complete ordered series to a clean pinned SDK archive and exercises source-derived
display boundaries with synthetic SDL/Vulkan state in an isolated environment:

```sh
python3 settings-display-regressions.py /path/to/clean-pinned-sdk /path/to/new-output
```

The Linux fixture passed ASan/UBSan, including asynchronous fullscreen matching,
window constraints, UI ownership, present-mode selection and deferred refresh.
The complete 16-patch SDK builds with the settings integration. Real window
transitions, compositor behavior and in-game visual quality remain manual tests.
Windows and macOS were not validated.

## Compatibility limits

Additional original fixtures: [scalar-layout-regressions.cpp](scalar-layout-regressions.cpp) drives the actual baseline/candidate device code using synthetic Vulkan function pointers; [synthetic-xma.c](synthetic-xma.c) decodes generated silent XMA frames with the pinned FFmpeg archives. They contain no game input. Exact private build commands and limits are recorded in the diagnostic-launch evidence collections. The XMA fixture follows the decoder's own grammar and is not an independent codec-conformance test.

The gap-fill change is a local partitioning candidate, tested by object replacement, synthetic ownership controls and private target compilation. It does not prove every function boundary or instruction semantic. Both codegen fixtures are original synthetic code with no game inputs or private paths. The gap-fill fixture exercises discovery; the emitter fixture isolates known graph ownership. Manual switch fixtures do not establish automatic switch recognition.


The earlier `chrono-range.patch` supports absolute FILETIME values from zero through `INT64_MAX`. Unsupported negative/upper-half inputs can still overflow unchecked low-level conversions; it is not an arbitrary-input safety fix. The void time-to-fields export's behavior for invalid inputs remains unresolved.

`to_sys` returns `sys_time<hundrednanoseconds>`. External callers requiring the host clock's native duration must check range and cast explicitly where needed. Native Windows/macOS builds have not been tested. Four upstream BitStream tests remain skipped.

## Attribution and contribution

The upstream BSD-3-Clause [license](LICENSE.rexglue) and exact [affected-file notices](NOTICE.md) are preserved. Changes were prepared with Codex assistance. No upstream issue, pull request, commit or push has been made. Any future contribution must follow the project's current [contribution guide](https://github.com/rexglue/rexglue-sdk/wiki/Contributing) and applicable authorization.
