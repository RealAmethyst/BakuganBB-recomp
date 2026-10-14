# First private diagnostic launch

Inspection date: 2026-10-14. Owning task: [issue 12](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/12), under [issue 4](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/4). This follows the [first native build](first-build-gates.md).

## Scope and speech decision

Prepare a private, user-controlled diagnostic launch. The native executable remains a static recompilation; the launcher supervises that process. Compilation and offline checks do not establish boot, rendering, game audio, menu accessibility or playability.

The accessibility designer selected automatic startup/failure speech through Prism, with standalone text-to-speech backends disabled. Speech targets a running screen reader; absence of one should be silent. In particular, direct Speech Dispatcher and SAPI output are excluded. Diagnostic wording lives in `mod/messages.en.json`. Game content is not narrated by this milestone.

## Private data and package

Current private build: `diagnostic-20261014.4`, installed with its predecessor backed up. The user-controlled test confirms smooth opening playback and working menus, then unresponsive character-creator input on Story entry. The application entry remains Bakugan Battle Brawlers Diagnostic. The [timing record](timing-contracts.md) owns the current binary identities, clock/wait and idle-overlay corrections, passive diagnostics and remaining timing gaps. Check-only preflight passes without speech initialization or game execution.

The first user launch of `.1` produced a recognizable game cursor sound and then aborted on a missing translated function. The [first-launch failure record](first-launch-failure.md) owns that evidence and translator corrections. The subsequent `.2` user test reached the title screen with working input and music and ended normally. Diagnostic speech and complete gameplay remain awaiting verification. The .4 test substantially improves opening timing; measured results are in the timing record. The next blocker is [Story character creation](story-entry-freeze.md), tracked in issue 21.

The new private data directory contains 8,075 files and 14 directories, totaling 3,340,953,615 bytes. Every path, capitalization and size matches the supplied baseline inventory; every file digest was compared with an independent read of that baseline. A private SHA-256 manifest covers every deployed file. The source image and executable match [their recorded identities](executable-baseline.md). No case-fold collisions were found.

The inspected runtime maps `game:` and `d:` to the supplied directory. Guest lookup is ASCII case-insensitive; the host's initial `default.xex` existence check is case-sensitive. The preserved layout satisfies both. Game-directory writes remain disabled by default.

The host uses explicit game, user and cache paths. Its configuration lives with writable user data, established before runtime setup. Each launch has a separate console/runtime log directory, and an exclusive launch lock prevents overlapping supervised instances. A check-only mode neither initializes Prism nor starts the host.

The first `.1` native executable had SHA-256 `6584f5585ae9809c514f8f77aebc93c3a95b6f0507d347580f3d7318bd621abb`. Its runtime and Xenos plugin are staged beside it with `$ORIGIN` RUNPATHs. Recursive static ELF inspection resolved all dependencies across 15 objects on this machine. This is not a portability or loader-ABI execution test. The original link-trial source and binaries were backed up before replacement.

## Graphics contract

The [scalar-layout candidate](patches/rexglue/scalar-layout.patch) requires the uniform layout already used by the shaders, rather than changing the host/shader ABI. Vulkan 1.2 and later use the core feature; earlier versions use the extension feature. Unsupported devices are rejected before logical-device creation with a device-named diagnostic. Non-GPU presentation-only behavior is preserved.

An original fixture exercises the actual baseline and candidate `vulkan_device.cpp` through synthetic dispatch functions. Results: baseline 9/19, candidate 19/19. Ten regressions fail before and pass after; nine controls are preserved. Checks cover Vulkan 1.0/1.1 extension paths, 1.2/1.3 core paths, exclusive feature chains, feature enablement, missing support and existing blend/swapchain requirements. The unchanged shader ABI retains the earlier 120-case validator evidence.

The runtime and plugin rebuilt successfully. The compilation reports 27 aggregate-initializer warnings, including two from the added feature-structure instance; these were not suppressed. The native host compiled and linked without warnings. The patch applies cleanly to the pinned SDK baseline.

A headless hardware query, isolated from desktop and accessibility resources, reports scalarBlockLayout support on both installed physical GPUs. The first restricted query lacked the NVIDIA device nodes and only enumerated the integrated device; a second query exposing the required GPU nodes enumerated both. No display surface, game rendering or presentation was tested.

## Audio evidence

An original synthetic XMA fixture exercises the actual built FFmpeg decoder without game bytes or output devices. All eight mono/stereo and sample-rate combinations decode four valid silence frames each: 32 frames and 24,576 scalar samples total. Frame dimensions, planar float format and zero samples match the fixture; additional receive returns EAGAIN and drain returns EOF. A one-bit length defect is rejected as invalid data.

The fixture follows the decoder grammar; it is not an independent codec-conformance test. This verifies a valid decode path, not real game streams, audible output, timing, looping or movie support. The runtime only opens the SDL audio output when the guest registers an audio client. Runtime setup alone therefore cannot establish output-device availability.

Source inspection also found that the imported audio registration entrypoint dereferences the runtime's audio-system pointer. The diagnostic host fails before guest launch if runtime setup has disabled that subsystem. It does not reinterpret failed audio initialization as successful readiness.

## Diagnostic behavior and verification

`mod/native_host.cpp` emits startup-stage events through verified ReXApp hooks. `thread_created` means the main guest thread exists but is still suspended at that callback; it is not a gameplay-success event. An error-level log sink reports errors without relying solely on process exit status. Early SDL failures occur before these hooks and are caught by the launcher from process termination and captured output.

The supervisor treats a zero process exit before thread creation as failure, records abnormal exits, deduplicates known events and ignores unknown event names for speech. If log writing fails after process creation, it continues draining output and supervising the child while retaining the launch lock. Failures before log creation do not start the child.

Fifteen original launcher tests passed inside an isolated environment with no game inputs, desktop sockets, audio devices or live accessibility resources. Tests cover normal lifecycle boundaries, zero-code early failure, error events despite zero exit, pre-hook failure, signal termination, malformed text, event filtering, failed process creation, incomplete installation, check-only behavior and logging failure before/after process creation. A read-only independent review found the logging ownership defect; the final tests cover its correction.

There is no watchdog for a process that stays alive without progress. Last-stage text and private logs aid diagnosis, but a freeze is not automatically classified as a crash. General console dialogs, menu speech, controller navigation, saving and gameplay remain separate work.

## Prism integration

Prism 0.18.3 is pinned to revision `b68e9c94240cefb1f08b2306c26376e3ab300a7a`. The adapter enumerates backend priority but creates only explicitly allowlisted screen-reader backends. It checks runtime availability before initialization and never uses the best-backend APIs that can select standalone synthesis. A missing reader is normal silence; a missing or incompatible library is a separate text diagnostic.

The private Linux library disables standalone TTS at build time. A focused local Orca backend patch connects only to an explicit existing Unix session-bus address, prevents service activation and bounds connection/IPC waits. It does not restart, configure or replace Orca, its accessibility bus or speech services. The adapter drops unavailable or failed backend handles and rediscovers on the next announcement.

Ten mocked adapter tests passed alongside the fifteen supervisor tests. Native integration used a private fake D-Bus service, synthetic identity/configuration and an isolated runtime directory with no access to the live desktop. The unpatched library reproduced unwanted activation; the candidate prevented it. A stalled speech call returned after approximately 500 milliseconds and later recovered. An isolated Unix connection that stalled authentication was also cancelled after approximately 500 milliseconds. This bound applies per operation, not to the complete announcement, which may include several probes.

The same-instance restart test spoke to fake owner A, remained silent while no reader owned the name, then spoke successfully after a new connection acquired the name as owner B. Missed announcements are not replayed. A complete session-bus replacement or a changed Orca dialect may lose the first announcement before the next reconnects; ordinary same-bus close/reopen passed. This verifies IPC and reconnection behavior, not actual listening usability. Native no-reader checks also pass without fallback speech.

The deployed Prism library SHA-256 is `89ebdce7d8c7db164787dc2ff88f6dabad030935ea56548b437f77ef555dca52`.

Prism's current contribution instructions disallow wholly AI-authored contributions. This is a private local dependency candidate, not an upstream submission. Its original MPL-2.0 license, notices, source identity and local patch are retained with private evidence.

## Evidence boundaries

Private evidence collections are `launch-readiness-20261014`, `vulkan-contract-20261014` and `synthetic-valid-xma-20261014`. They hold exact commands, source snapshots, hashes, data manifests, isolated test logs and build logs. Original synthetic fixtures contain no game content. All supplied assets, generated translation and native game binaries remain outside the publishable workspace.

The coding agent did not launch the game, change live screen-reader services, commit or push. The first user-controlled launch confirmed an audible game cue; the subsequent failure and replacement are recorded separately above. Further listening and gameplay checks remain user-controlled.
