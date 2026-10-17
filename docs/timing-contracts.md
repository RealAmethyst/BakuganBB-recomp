# Console timing and opening playback

Inspection date: 2026-10-14. Owning investigation: [issue 15](https://github.com/RealAmethyst/BakuganBB-recomp/issues/15). Shared-clock correction: [issue 16](https://github.com/RealAmethyst/BakuganBB-recomp/issues/16). POSIX readiness correction: [issue 17](https://github.com/RealAmethyst/BakuganBB-recomp/issues/17). Remaining guest timeout contracts: [issue 18](https://github.com/RealAmethyst/BakuganBB-recomp/issues/18).

## Observations and scope

The user-controlled `diagnostic-20261014.2` test reached the title screen with working input and music, then closed normally. Logs confirm process exit zero. Opening video reportedly remained visible after audio ended, with the game continuously focused. No playback offset was independently measured, and the screenshots interpreted elsewhere were not supplied to this investigation.

The requirement is a faithful console timing foundation before frame-rate changes or graphics enhancements. Keep simulation elapsed time, Vulkan presentation cadence, and media scheduling distinct. A low frame rate does not identify a GPU bottleneck. Original gameplay timing must not be compensated with guessed movie delays or arbitrary clock scalars.

## Diagnostic .3 user retest

The next user-controlled test still reported opening desynchronization. Main-menu navigation additionally produced a prompt cursor cue but a separate image-change effect starting late or playing after navigation stopped. The host exited normally with code zero. The original menu trigger is being traced under [issue 20](https://github.com/RealAmethyst/BakuganBB-recomp/issues/20); a common cause with movie delay is not assumed.

Across 244 diagnostic update windows, host elapsed time totals 244.021996 seconds, the guest counter 244.023269 seconds, and supplied update deltas 244.023270 seconds. The instrumented update-container entry runs 5,502 to 5,602 times per window; this is not rendered frame rate or proof that every gameplay task runs that often.

Movie ID four reaches a sampled requested video timestamp of 24,966 milliseconds while its media clock is 69,825 milliseconds: the frame is 44,859 milliseconds late. The stop routine returns after 70.178 seconds, without establishing natural EOF. This directly establishes late video scheduling, though no audible sample position was measured. Other logos vary: maximum sampled lateness is about 4.456, 0.939 and 0.293 seconds for IDs zero through two; ID three remains slightly ahead of its deadline.

The opening interval has about 10.53 guest GPU swap requests per second and about 8,182.44 accepted Vulkan presentations per second. There are no placeholder skips or new outdated-swapchain results within that interval. Across the recorded presentation window, 1,951,428 acceptances over 243.140 seconds represent about 8,025.93 per second. These repeated host presentations are not game frame production or physical scanout.

Source inspection finds an always-registered achievement notification dialog whose empty queue draws nothing, but whose presence keeps `ImGuiDrawer::Draw` requesting continuous UI repaint. `Presenter::WaitForUITick` has no Linux pacing implementation. The SDL paint loop and immediate presentation therefore permit continuous idle host repaint. [Issue 19](https://github.com/RealAmethyst/BakuganBB-recomp/issues/19) owns a focused correction. The exact contribution of this wasted work versus unoptimized translated execution remains unmeasured.

The SDL output consumes already-mixed six-channel PCM in 256-sample blocks at 48 kHz. It has no separate cursor/image-effect identity. Eight default producer credits represent approximately 42.667 milliseconds of guest PCM ahead if each guest callback submits one block; this is not a hard queue-size or physical latency bound. Credits return when data enters SDL, and downstream device buffering is unmeasured. No audio submission errors are logged. Selective cue lateness suggests an upstream event/scheduling difference, but does not prove one.

## Original main-loop and movie contracts

The original executable establishes a shared clock, not separate guessed rates:

- `0x82637848` obtains `KeQueryPerformanceFrequency` through verified import `0x828ECC64`. Initialization `0x82130688` stores integer frequency at `0x829E7938` and double frequency at `0x829E7930`.
- `0x823BC1D8` reads `mftb` and stores the full counter. Main loop `0x82127BA8` subtracts samples held at `0x829E79B8`, divides by the frequency, and passes float-rounded seconds in `f1` to `0x8212E0C8`. That consumer stores the delta at update-container offset `0x3C`; dispatcher `0x8212D9C8` uses the same field.
- Movie setup `0x82496B10` calls `0x82496638`, which constructs player `0x8249D9D8` and clock `0x8249D358`. Clock table is `0x8205CAB0`, player table `0x8205CB30`. Configuration offset four carries the clock through `0x824A9C78` and `0x824A7D98` into player offset `0x2C`.
- Clock table slot `0x74`, target `0x8249D818`, queries the same counter/frequency pair and calculates milliseconds. Slot `0x4C`, target `0x8249D668`, advances accumulated time at clock offset `0x120` using prior sample `0x128` and float rate `0x154`, then writes requested timestamp minus elapsed media time through the output argument. Default rate constant `0x82000E18` is exactly 1.0.
- Actual video consumer `0x824AA460` stores a queued frame timestamp at player offset `0x120`, loads the clock through `0x2C`, and calls its deadline method at `0x824AA9D0`. On the normal forward-playback path it sleeps for deadline minus five milliseconds when the deadline exceeds five.
- The sleep goes through `0x823BC230` and `0x823BE670` to `KeDelayExecutionThread`, import `0x828EC924`. Ordinary millisecond input is multiplied by minus 10,000 to form the signed relative 100-nanosecond interval. The original minus-one input explicitly constructs `INT64_MIN`.

All 303 selected original instruction words covering these consumers and the movie lifecycle matched an independent original-XEX decode. A further six epilogue instructions were checked for passive register observation. No reverse-engineering annotations were added. Exact disassembly, register dumps and game media remain private.

## Movie sequence and source timing

The actual index/filename consumer identifies four startup logo records, a separate opening and guide, and seven localized introductions. Logo handler `0x822D8F58` requests IDs zero through three through `0x823AFA68`, waiting for manager state offset four between requests. `0x822D9A48` selects opening ID four and guide ID five on repeat; `0x822D9C98` selects language plus five for IDs six through twelve. The request stores ID at manager offset `0x28`; the filename is resolved through `0x8238E140` and `0x82130F68`, then passed to `0x82190990`. The word “intro” alone does not identify the active resource.

All thirteen unique private movie files inspect as ASF with WMV3 video at 1280 by 720, 30 frames per second, and stereo WMAv2 audio at 44.1 kHz. Available packet timestamps are monotonic and stream endpoints are close. The opening has 1,350 video packets and approximately 45 seconds duration; localized introductions have 1,203 video packets and approximately 40.157 seconds duration.

Isolated audio decoding to a null output finds approximately 1.54 seconds below minus 45 dB at the end of the opening, and approximately one second at the end of one logo. This threshold-based result does not prove perceptual silence. It shows why a short silent tail alone cannot establish playback drift. No audio device or guest code was used for media inspection.

## Shared clock defects and correction

Actual-source controlled-clock fixtures reproduced loss of fractional guest ticks on every read, a sample-order race that double-counted elapsed time, reversed non-unit deadline scaling, inaccurate fractional scalars, and undefined signed FILETIME conversion. Linux also reported a frequency based on clock resolution while returning nanosecond counts.

The candidate samples under the clock mutex, retains fractional progress, applies rate changes after accruing the preceding interval, and uses an exact representable rational rate. Invalid or unrepresentable rates leave the prior state intact with a diagnostic. Duration/deadline conversion uses the correct direction, preserves signed values and existing sentinels, and checks range. Linux frequency now matches its nanosecond units. The installed default remains 1x; no movie-specific rate adjustment is made.

An independent review reproduced a candidate regression when setting guest system time below elapsed time. Restoring the original modular base/offset addition corrected it; reset-to-zero and continued progression are covered. Extreme deadline clamping also occurs after applying the destination epoch.

Final actual-source clock checks: baseline 17/45, candidate 66/66, including 21 additional invalid-input/boundary controls. Clang strict-warning and GCC undefined-behavior/float-cast sanitizer configurations pass. Portable multiplication/division passes 11,408 arbitrary-integer reference cases; rate decomposition passes 10,001 exact-rational reference cases. Simulated Linux resolution checks improve from 1/3 to 3/3. Windows/macOS native execution remains untested.

A separate probe links the actual shared runtime, with no clock or configuration stubs. Under continuous single-thread polling, the baseline reports approximately 0.761 guest seconds per host second; the corrected runtime reports a ratio of 0.999998935. Eight-thread ratios are approximately 0.987575196 before and 0.999999464 after. The remaining microsecond-scale differences include sampling boundaries. These stress measurements demonstrate the defect and correction, not the game's actual polling frequency or playback slowdown.

The integrated SDK rebuild passes its full suite: 1,671 passes, four existing BitStream skips, zero failures, including all 1,458 synthetic PPC cases. These fixtures contain no game code.

## Wait contracts and remaining gaps

The separate POSIX correction inspects ready objects even when an alertable deadline is already expired. Existing locking and APC priority are retained. Actual SDK fixture results improve from 22/31 to 31/31, preserving auto-reset consumption, partial WaitAll behavior, signal side effects and finite/infinite controls.

The broader audit is not complete console equivalence. Positive absolute guest deadlines, sub-millisecond truncation, large relative-duration wrapping and the original `INT64_MIN` delay path remain open in issue 18. Ordinary traced movie waits use whole milliseconds. These remaining issues are not established causes of the reported opening drift. Non-unit audio scaling and physical device/display latency also remain outside the verified baseline.

## Passive game diagnostics

`src/game/native_timing.cpp` wraps verified generated symbols and forwards the original implementations. It does not change guest state, poll game structures or supply game content speech. Instrumentation restores the host floating-point environment around its own calculations.

- `0x823AFA68`: incoming `r4` identifies a new movie request. Internal loop replay may omit this request.
- `0x82190990`: incoming `r3` identifies the wrapper; returned `r3` is exactly zero or one. A successful start records setup completion, not the first displayed frame or audible sample.
- `0x821908D0`: records return from a stop call for a previously started wrapper. The original routine can return early; the event establishes neither natural EOF nor unconditional resource release.
- `0x8212E0C8`: aggregates supplied `f1` update deltas and update count against host elapsed time and the guest counter, roughly once per second.
- `0x8249D668`: captures incoming requested timestamp, clock identity and LR. After status zero, verified original register effects leave accumulated media time in `r7` and the written deadline in `r5`. Incoming LR `0x824AA9D4` identifies the traced video-frame caller. Generic observations are labelled media deadlines, not physical presentation or audio position.

Original synthetic hook checks verify complete context forwarding, output mutations from the original stub, nested stop-on-start, failed starts, and preservation of floating-point rounding and exception flags. A safe audio sample-position hook is not yet verified.

## Presentation diagnostics

The optional SDK diagnostics count guest swap requests and asynchronous shader-placeholder skips separately from Vulkan acquisition/presentation calls. Cumulative counts include accepted, suboptimal and outdated results, with CPU/API duration and a host steady-clock timestamp approximately once per second. Game update and media-deadline records use the same steady-clock domain. The host enables this flag before worker startup; SDK default is disabled.

Ten original helper checks pass under strict Clang and GCC undefined-behavior sanitizers, including eight-thread counting and the disabled path performing no clock work. The integrated final SDK suite again has 1,671 passes, four existing skips and zero failures. The SDK build retains existing warnings; these checks are not warning-free build evidence.

Queue acceptance is not physical scanout, and API duration is not GPU execution time. There is no GPU-fence measurement or verified audible sample-position hook. Counters aggregate process presenters, and a final partial logging interval is not flushed. Instrumentation adds observation overhead but introduces no extra frame wait, cap or presentation-policy change.

## Diagnostic .3 deployment

Build `diagnostic-20261014.3` is installed, with `.2` native binaries and manifest preserved privately. Translated CPU compilation remains at O0, as in the successful `.2` test; a separately built O3 candidate is held out of this comparison. Guest time scale remains 1x. Frame-rate and presentation settings are unchanged.

Deployed SHA-256 identities:

- Host: `27bb80b8d5fa78f7ed6bcac29080ec89580d5ca6617a2e4c0d03edf7e1d08c3c`.
- Runtime: `4de583dc444129892442d98ca2fc8029e48657858eff4038472f710946dc438a`.
- Xenos plugin: `062a056c96f29a0f2c30ab44faba78d54dd8792c8f64e3e23d0533a56a340e8e`.

All three native artifacts retain `$ORIGIN` RUNPATHs. Static direct dependency resolution passes. The final executable contains all five strong timing wrappers and their original implementations. All nine retained patches apply to the pinned baseline in manifest order; resulting source hashes match the integrated overlays. Installation used the launcher lock, checked for a running installed game, and verified prior manifest hashes before replacing files. Check-only preflight exits zero without loading speech or starting the game.

The launcher, Prism library, diagnostic wording and private game data were unchanged. The subsequent user-controlled .3 retest is recorded above. No game was launched by the coding agent, and no claim of complete console timing equivalence is made.

## Original menu transition and sound requests

The main menu is constructed by `0x822CED20` from dispatcher `0x822D1BB0` case `0xE`. Its selector at menu offset `0x58` uses table `0x82003900`; the input consumer `0x822CEDF8` calls slot four, resolving to `0x821DEDC0`. A changed nonnegative selection with the sound flag enabled requests effect ID five through `0x821DCD78`, with incoming LR `0x821DF114`.

The menu constructor creates a separate resource-backed widget through `0x822D0E68`, table `0x820120CC`, update callback `0x822D0F50`. This consumer follows its owner record to the same selector. In normal transition substate one it requests ID thirteen (LR `0x822D1174`) and begins an animation. After that phase, substate three counts unchanged enabled selection observations, resetting on a change. At fifteen qualifying increments it requests ID twelve (LR `0x822D1264`) and starts another phase. Animation positions eight, fourteen and nineteen are animation units, not seconds.

Thus a deferred transition request after navigation stops is explicitly authored behavior. The amount of real elapsed delay, callback cadence, and which audible asset corresponds to each ID remain unverified. No fixed 30/60 Hz or half-second expectation is inferred, and no menu trigger is changed.

All 890 selected original non-padding instruction words for this chain match an independent original-XEX decode. No RE annotations were added. Unsupported VMX decompilation gaps in shared helpers remain explicit; they were not treated as empty functions.

Two additional passive wrappers observe `0x821DCD78` entry request ID in `r3`, second argument in `r4`, and LR, plus aggregate entries to `0x822D0F50`. They forward the original exactly once without guest-memory reads. Synthetic controls preserve complete context and floating-point state. Request time is not audible onset; aggregate callback counts do not identify entries that passed the original phase/selection gates, and an interval spanning absence includes that gap. These diagnostics will distinguish requests from listening reports without inventing a replacement sound schedule.

## Idle overlay correction and diagnostic .4 deployment

The idle-toast correction adds a repaint-demand query whose default preserves continuous painting for ordinary dialogs. Achievement toasts request continuous painting only while their queue is nonempty. New toasts enqueue a UI repaint using weak dialog and drawer lifetime guards. Achievement callback ownership is acquired on the UI thread so a worker cannot release the last dialog reference during shutdown.

Independent review caught an ordinary-dialog opening regression: a persistent idle toast kept the drawer registered, so adding settings or another overlay did not automatically wake it. A one-shot repaint after successful dialog insertion corrects that path, including insertion during drawing without duplicate drawer registration. Rendering and notification features are retained; no arbitrary FPS cap or sleep is introduced.

Actual complete method bodies run with original synthetic UI/context boundaries and a controlled clock. Baseline passes 19/31; candidate passes 33/33, with twelve baseline failures and two additional new-lifetime guards. GCC strict warnings, AddressSanitizer and UndefinedBehaviorSanitizer pass. The fixture preserves the original floating-duration expiry predicate; it does not silently change the toast lifetime. Independent review found no remaining blocker within the existing UI-thread ownership and context-lifetime contracts. These synthetic checks do not establish actual SDL/Vulkan behavior.

The integrated SDK rebuild passes 1,671 tests with four existing BitStream skips and zero failures, including 1,458 synthetic PPC tests. Existing SDK compiler warnings remain. The final native application compiles and links, and its seven wrappers plus original implementations are strong symbols. Expanded synthetic hook checks pass against the final runtime, preserving context forwarding and floating-point flags. All ten patches apply in manifest order to the pinned source; patched file hashes match the integrated overlays.

Current private build `diagnostic-20261014.4` includes the overlay correction, seven passive timing hooks and O3 compilation for all 149 generated translation units. The same strict floating-point and no-strict-aliasing options are retained. Host diagnostics stay at O0. This is a compiler-optimization comparison, not a game-speed or frame-rate setting change; the guest clock remains 1x. The separate effects of optimized translation and idle-repaint correction are not isolated by this combined build.

Deployed SHA-256 identities:

- Host: `cd7786162797541efc0becffdcfb07b284526898d439ba85557ae68cca604ca3`.
- Runtime: `da305e642ce26a3f071dce96fdf27aa3afd5bc1df62ed65af22eb3a1cafe7610`.
- Xenos plugin: `062a056c96f29a0f2c30ab44faba78d54dd8792c8f64e3e23d0533a56a340e8e`.

The .3 package's native artifacts and manifest are backed up. Deployment held the launcher lock, checked that the installed game was not running, verified the previous manifest and new file hashes, and retained `$ORIGIN` RUNPATHs with direct dependencies resolved. Check-only preflight exits zero without game execution or speech initialization. Prism, launcher wording, game data and frame/presentation settings remain unchanged. Windows/macOS builds and actual toast rendering remain unverified. The subsequent user-controlled run is recorded below; exact audible onset of the secondary menu effect remains unconfirmed.

The audio audit also found conditional recovery gaps: an extra 500-millisecond sleep after a non-pumping worker wait, and a lost producer credit after a failed SDL data submission. Neither condition is demonstrated in this test, and no speculative audio correction is included. The known guest timeout gaps in issue 18 remain separate.

## Diagnostic .4 user result

The user reports that video no longer lags, menus work, and Story reaches an unresponsive character creator while audio continues. This is listening/gameplay evidence for the opening improvement, not complete console timing equivalence or a complete playthrough.

The opening movie's observed lifecycle is 45.148608 seconds, compared with 70.178131 seconds in .3. Maximum sampled video lateness falls from 44,859 to 46 milliseconds. Host queue acceptances during the opening average 30.000 per second, and guest swap requests average 29.978 per second. Queue acceptance is not physical scanout; sampled lateness does not establish audible sample alignment.

Across the run, accumulated guest time differs from measured host time by approximately 0.906 milliseconds over 221 seconds. Continuing high-frequency update callbacks are not a simulation-frame-rate measurement. Original simulation cadence still requires a verified simulation-step event.

Near Story entry, host UI presentations rise sharply while guest swaps and updates continue. The process later closes normally with exit zero. The [Story-entry record](story-entry-freeze.md) distinguishes the active-dialog hypothesis from confirmed evidence. An ordinary dialog's continuous repaint demand is preserved by the idle-toast correction; this new observation does not establish that the empty-toast defect recurred.

Private collection `story-freeze-20261014/log-analysis` retains the unchanged log snapshot, comparisons and analysis. No runtime code or installed build changed during this inspection.

## Evidence and boundaries

Private collection `opening-timing-20261014` owns the initial traces, media inspection and clock correction. Collection `timing-followup-20261014` owns the .3 test snapshot, presentation measurements, original menu trace, audio audit, overlay candidate/review, expanded hooks, .4 build/deployment and exact commands. Shared patches/fixtures must retain SDK attribution and contain no game inputs. No game was launched or controlled by the coding agent, and no live accessibility service was changed.
