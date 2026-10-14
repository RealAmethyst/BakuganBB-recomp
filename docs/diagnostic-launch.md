# First private diagnostic launch

Inspection date: 2026-10-14. Owning task: [issue 12](https://github.com/RealAmethyst/bb-recomp/issues/12), under [issue 4](https://github.com/RealAmethyst/bb-recomp/issues/4). This follows the [first native build](first-build-gates.md).

## Scope and speech decision

Prepare a private, user-controlled diagnostic launch. The native executable remains a static recompilation; the launcher supervises that process. Compilation and offline checks do not establish boot, rendering, game audio, menu accessibility or playability. The original launch scope below is followed by the keyboard-dialog milestone recorded under Private data and package.

The accessibility designer selected automatic startup/failure speech through Prism, with standalone text-to-speech backends disabled. Speech targets a running screen reader; absence of one should be silent. In particular, direct Speech Dispatcher and SAPI output are excluded. Diagnostic wording lives in `resources/messages.en.json`. Later game-content narration is recorded below separately from the initial milestone.

## Save, popup and logo-timing follow-up

Current private build: `diagnostic-20261014.7`, installed with `.6` backed up under [issue 22](https://github.com/RealAmethyst/bb-recomp/issues/22). It adds the native save picker's final formatted name/date/play-time text and displayed hints; shared native one-choice/two-choice popup body, actual selection and hints; and caption-visible startup logo cues. No save-screen heading is invented. Native game input and the deletion popup's initial No selection are preserved.

Logo narration now follows actual compressed-picture identity through decoder reordering, picture copies, queued frames and successful drawing. It speaks only inside privately verified readable intervals. Loading, first submission, wall-clock delays and counted draw attempts do not determine the cue. Normal startup and reset behavior were traced separately. The [localization record](localization-consumers.md) owns all original consumers, addresses and remaining limits.

Host XAM option dialogs separately gain D-pad Left/Right navigation and A confirmation through the existing SDL driver/UI boundary. The requested initial option receives focus. Modal capture suppresses underlying game input, and held presses remain blocked until their release reaches the drained driver state. Focus loss, nested dialogs, connection changes and concurrent UI/guest drains have explicit handling. Physical-keyboard name entry remains unchanged. The alternative Windows XInput backend has no new modal navigation implementation.

Offline verification passed: 52 Python tests; native ASan/UBSan fixtures with 165 original forwarding calls and 451 bounded reads; actual ImGui/message/channel/SDL-dispatch fixtures with ASan/UBSan; separate SDL/channel concurrency checks with ThreadSanitizer; and existing runtime input tests with 21 assertions in eight cases. The keyboard fixture also passes. Independent source reviews found and resolved stale hint ownership, picture-copy ownership and failed decoder-replacement issues. These checks do not establish physical controller or listening usability.

All fourteen patches apply to the pinned SDK. The 27 affected runtime files match the final ordered build mounts; four generator files intentionally remain baseline in that runtime checkout because private translation uses its separate verified generator. The 48-hook translation retains all 46,056 function identities and every original body after removing observer calls. Its 21 existing generator warnings are unchanged. The SDK and host build/link pass; the final host build has no diagnostics. All 31 game-UI wrappers retain their originals, all hook callbacks link once, and the superseded generic video wrapper is absent.

Installed host SHA-256: `b3fe26aa1ee615fe4fbb643e621b919dcf209300fffc7e19fa959c91fa3d7195`. Runtime SHA-256: `88e43d54f38ccf53aab4367c0494df3b1e146a72c0422179a79b77245dcecf21`. Xenos and Prism are unchanged. Private collection `popup-controller-20261014` preserves source snapshots, original-code audits, isolated test/build commands, dependency checks, installed manifest and the `.6` backup. Installation held the launch lock, verified no installed game process was running, and passed check-only preflight without loading speech or executing the game.

Status: awaiting user-controlled listening and controller checks on `.7`. Check all four logo cues, existing and empty save cards, native popup default/movement/single-choice behavior, parent return, host D-pad/A input and existing main-menu/title/name-entry behavior. `XamShowMessageBoxUIEx`, the native battery warning, other save modes, later screens and complete gameplay remain outside this coverage. No game, desktop or screen-reader process was controlled by the coding agent. Launch normally to use the new build; no system or screen-reader restart is needed.

## Startup and menu speech candidate

Previous private build: `diagnostic-20261014.6`, installed with `.5` backed up under [issue 22](https://github.com/RealAmethyst/bb-recomp/issues/22). It adds verified startup-logo artwork wording, title/prompt, actual host option-message text and focused choices, and the five main-menu captions/descriptions/positions with opening hints. The [localization record](localization-consumers.md) owns original behavior, recovered transitions and remaining consumer gaps. The project instructions now explicitly treat game understanding and reusable documentation as core deliverables for future mod support.

Fourteen original instruction events replace recurring menu/title-state reads and repeated widget-record traversal. Native rendering captures only owned, uncached text. A bounded output worker serializes copied events outside runtime critical locks. The runtime's guarded-copy API synchronizes mapping validation and copying; it does not replace ownership evidence. Screen wording and delayed-item/hint policy remain in the separate launcher layer.

Offline verification passed: 51 Python speech/supervisor tests; 81 synthetic native forwarding calls with event/lifetime, cached-read, argument and floating-point checks; JSON serialization; four emitted-and-executed synthetic instruction-hook variants; queue ASan/UBSan and separate ThreadSanitizer checks; and actual keyboard/message-box fixtures with ASan/UBSan. Guarded reads passed strict full-source syntax checking and actual-runtime UBSan checks with 360,000 concurrent reads. The real fixed-address SDK arena is incompatible with ASan shadow mappings, so that particular fixture uses UBSan.

All thirteen evaluation patches apply sequentially. All 23 affected runtime/UI/test files match the built overlay; four codegen files remain baseline in that runtime checkout because translation uses the separately verified candidate generator. Fresh translation retains the same 46,056 function identities, with fourteen added event calls in ten function bodies and no other instruction changes. Registration, initialization and PCH files remain identical. The SDK and host build/link pass; existing SDK warnings remain, while the native host build reports none. All thirteen game wrappers and their originals are strongly linked. Installed check-only preflight passes without initializing speech or executing the game.

Installed host SHA-256: `15dece7e13524a87cc76c296646286ded941759ae15f41f0c515dc9709fe4651`. Runtime SHA-256: `4c7a3408a3a31ed496bf33baf4ba3fc8ba89c9aa5383e6b403c23620d4d3cd2e`. Xenos and Prism retain their previous identities. Private collection `startup-menu-integration-20261014` preserves generated audit, commands, source snapshots, offline results, deployment manifests and the replaced files.

The subsequent user-controlled test confirms main-menu, title and logo speech. It identifies three gaps: the save picker is silent, its game-rendered deletion confirmation is silent, and logo announcements precede the visible lettering, particularly Activision. First successful video submission is therefore insufficient for the required caption timing. Detailed announcement ordering and every navigation case remain separately unverified.

The designer selected D-pad navigation for host option dialogs where it has no competing role, with A confirmation. The native save picker and native popups retain their existing game controls. Follow-up work under issue 22 traces their final formatted text, selection and lifetime events, and correlates each logo caption with the submitted video's frame timing. `XamShowMessageBoxUIEx`, the native controller-battery warning and later gameplay screens remain outside this coverage. No screen-reader or system restart is required by these application changes.

## Private data and package

Previous private build: `diagnostic-20261014.5`, installed with `.4` backed up. It adds spoken physical-keyboard name entry, corrects primary cancellation results and UTF-16 capacity handling, and records passive keyboard request/result events. The [Story record](story-entry-freeze.md) owns the original consumer trace and chosen interaction. The subsequent tester confirmation establishes that the reported name-entry blocker is resolved; it does not individually verify every edge case or later gameplay.

Offline verification passed: 38 Python speech/supervisor tests, 107 synthetic native forwarding calls, native JSON serialization, and the actual ImGui keyboard/completion fixture with ASan/UBSan in an isolated environment. All eleven SDK patches apply sequentially to the pinned revision, with resulting keyboard files matching the tested overlay. The SDK and native host build/link successfully; SDK warnings remain in existing source, with no warnings from the host build. All nine passive wrappers and preserved originals are strongly linked. Static ELF inspection resolves 15 dependency objects. Check-only preflight passes without speech initialization or game execution.

The `.5` host SHA-256 is `e5fbb2a3583f4d2b13d381b0ff92097141ab524a2699c0396f4aedae0c926d10`; runtime SHA-256 is `0e71c9e7a336979c86e3fab5b908a2721a9f5b4cf5e8487172ff02e2190b4123`. The Xenos plugin and Prism library retain their `.4` identities. Private collection `creator-dialog-20261014` retains deployment manifests, the prior installed files, isolated commands, logs and evidence hashes.

The `.4` user-controlled test confirms smooth opening playback and working menus, then unresponsive character-creator input on Story entry. Those observations are the tested baseline, not a `.5` gameplay result. The application entry remains Bakugan Battle Brawlers Diagnostic. The [timing record](timing-contracts.md) owns the `.4` binary identities, clock/wait and idle-overlay corrections, passive diagnostics and remaining timing gaps.

The first user launch of `.1` produced a recognizable game cursor sound and then aborted on a missing translated function. The [first-launch failure record](first-launch-failure.md) owns that evidence and translator corrections. The subsequent `.2` user test reached the title screen with working input and music and ended normally. The `.4` test substantially improves opening timing; measured results are in the timing record. The later [Story name-entry blocker](story-entry-freeze.md), issue 21, is resolved by the tester-confirmed `.5` fix.

The new private data directory contains 8,075 files and 14 directories, totaling 3,340,953,615 bytes. Every path, capitalization and size matches the supplied baseline inventory; every file digest was compared with an independent read of that baseline. A private SHA-256 manifest covers every deployed file. The source image and executable match [their recorded identities](executable-baseline.md). No case-fold collisions were found.

The inspected runtime maps `game:` and `d:` to the supplied directory. Guest lookup is ASCII case-insensitive; the host's initial `default.xex` existence check is case-sensitive. The preserved layout satisfies both. Game-directory writes remain disabled by default.

The host uses explicit game, user and cache paths. Its configuration lives with writable user data, established before runtime setup. Each launch has a separate console/runtime log directory, and an exclusive launch lock prevents overlapping supervised instances. A check-only mode neither initializes Prism nor starts the host.

The first `.1` native executable had SHA-256 `6584f5585ae9809c514f8f77aebc93c3a95b6f0507d347580f3d7318bd621abb`. Its runtime and Xenos plugin are staged beside it with `$ORIGIN` RUNPATHs. Recursive static ELF inspection resolved all dependencies across 15 objects on this machine. This is not a portability or loader-ABI execution test. The original link-trial source and binaries were backed up before replacement.

## Graphics contract

The [scalar-layout candidate](../patches/rexglue/scalar-layout.patch) requires the uniform layout already used by the shaders, rather than changing the host/shader ABI. Vulkan 1.2 and later use the core feature; earlier versions use the extension feature. Unsupported devices are rejected before logical-device creation with a device-named diagnostic. Non-GPU presentation-only behavior is preserved.

An original fixture exercises the actual baseline and candidate `vulkan_device.cpp` through synthetic dispatch functions. Results: baseline 9/19, candidate 19/19. Ten regressions fail before and pass after; nine controls are preserved. Checks cover Vulkan 1.0/1.1 extension paths, 1.2/1.3 core paths, exclusive feature chains, feature enablement, missing support and existing blend/swapchain requirements. The unchanged shader ABI retains the earlier 120-case validator evidence.

The runtime and plugin rebuilt successfully. The compilation reports 27 aggregate-initializer warnings, including two from the added feature-structure instance; these were not suppressed. The native host compiled and linked without warnings. The patch applies cleanly to the pinned SDK baseline.

A headless hardware query, isolated from desktop and accessibility resources, reports scalarBlockLayout support on both installed physical GPUs. The first restricted query lacked the NVIDIA device nodes and only enumerated the integrated device; a second query exposing the required GPU nodes enumerated both. No display surface, game rendering or presentation was tested.

## Audio evidence

An original synthetic XMA fixture exercises the actual built FFmpeg decoder without game bytes or output devices. All eight mono/stereo and sample-rate combinations decode four valid silence frames each: 32 frames and 24,576 scalar samples total. Frame dimensions, planar float format and zero samples match the fixture; additional receive returns EAGAIN and drain returns EOF. A one-bit length defect is rejected as invalid data.

The fixture follows the decoder grammar; it is not an independent codec-conformance test. This verifies a valid decode path, not real game streams, audible output, timing, looping or movie support. The runtime only opens the SDL audio output when the guest registers an audio client. Runtime setup alone therefore cannot establish output-device availability.

Source inspection also found that the imported audio registration entrypoint dereferences the runtime's audio-system pointer. The diagnostic host fails before guest launch if runtime setup has disabled that subsystem. It does not reinterpret failed audio initialization as successful readiness.

## Diagnostic behavior and verification

`src/runtime/native_host.cpp` emits startup-stage events through verified ReXApp hooks. `thread_created` means the main guest thread exists but is still suspended at that callback; it is not a gameplay-success event. An error-level log sink reports errors without relying solely on process exit status. Early SDL failures occur before these hooks and are caught by the launcher from process termination and captured output.

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
