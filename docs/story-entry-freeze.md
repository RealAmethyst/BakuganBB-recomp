# Story entry and character-creator input

Inspection date: 2026-10-14. Owning work: [21](https://github.com/RealAmethyst/bb-recomp/issues/21); accessibility work: [issue 5](https://github.com/RealAmethyst/bb-recomp/issues/5). Build identity and opening timing are in [timing contracts](timing-contracts.md).

## User result

The diagnostic-20261014.4 user test reports smooth video playback and working menus. Story reaches the character creator, but input appears unresponsive while audio continues. No screenshot or native screen-identity event was captured in this investigation; the screen identity and audible continuation are the tester's observations.

## Log findings

Guest update callbacks and approximately 30 GPU swap requests per second continue until the normal window-close path. The process exits zero, with no logged error, critical, assertion or abort. These counters show continuing activity; they do not prove the creator's own state machine or input handling progresses.

Near host second 191, host UI presentation jumps from approximately 30 to approximately 7,940 acceptances per second. Guest swaps remain near 30. This pattern is compatible with an active host dialog continuously repainting while waiting for interaction. It differs from the prior idle-toast defect, whose empty-queue demand is removed in this build. The actual dialog is not named by the captured log, so the cause remains unconfirmed.

Forty-two file lookups return status `0xC000000F`, including texture-related requests around creator activity. They may be probes followed by another resolution path. No missing-content or freeze-causality claim is justified without tracing the consumer and fallback.

## Bounded source hypothesis

The pinned runtime's `src/kernel/xam/xam_ui.cpp` implements `XamShowKeyboardUI` with an ImGui `KeyboardInputDialog` in the normal UI path. It uses text entry and confirmation/cancellation controls; dialog dismissal supplies the asynchronous completion. An active instance retains continuous repaint demand. The prior [localized keyboard-title trace](localization-consumers.md) supplies a game-side research starting point, not proof this particular call happened during the run.

A pending name-entry dialog, input capture or incomplete console-UI completion is therefore a concrete next-session hypothesis. Current evidence does not distinguish these from other creator-specific state/resource issues. No controller mapping, dialog acceptance, native screen-reader service or game state was changed.

## Resumed keyboard investigation

The resumed investigation traced the original instructions again with read-only Ghidra. No database annotations were added. Private evidence is in `creator-dialog-20261014`; menu research is separately retained in `menu-speech-20261014`.

At `0x82300D44`, the name-entry state handler stores object field `0x30` plus one as the requested buffer capacity. It puts the output buffer at object `0x1A0` into the argument block, resolves the title through the existing localized-provider call at `0x82300D68`, and calls `0x823B6A10`. That wrapper dereferences object `0x8` before calling `0x823B7820`. The latter initializes seven words at service `0x38`, passes that overlap pointer in `r10`, and loads capacity/output/title/default/flags into `r9/r8/r6/r5/r4`. It calls `0x823BC4F0`, whose original instruction tail-calls the verified `XamShowKeyboardUI` import. Return `0x3E5` means dispatch accepted and pending, not that a popup became visible.

The completion consumer is `0x823B6A40 → 0x823B7C58 → 0x823B7B68`. The first wrapper dereferences its object's `0x8`; the second selects operation 1. The consumer checks service `0x14` for that operation and overlap result at service `0x38` for pending. Once complete it calls `0x823BB1F0`, passing the overlap pointer. That function returns the primary word at overlap offset zero at `0x823BB26C`; it does not read the extended error. `0x823B7B68` maps zero to state 2 and nonzero to state 3. State 1 means pending; state 0 means the requested operation is not selected. The outer name-entry handler processes text on state 2 at `0x82300E50` and takes its cancellation path on state 3 at `0x82301028`. State 3 is not proof of a particular error code by itself.

This demonstrates a cancellation defect in the evaluated runtime: its keyboard close callback returned success as the primary result even when cancellation was stored in the extended-error field. The game therefore followed its success path after Cancel. The candidate returns cancellation in both fields and preserves the output buffer. This is a separate confirmed contract defect, not proof of the initial freeze's cause.

## Keyboard interaction and verification

The accessibility designer selected physical-keyboard name entry, speech for typed and deleted characters, Enter confirmation and Escape cancellation. The candidate keeps the existing native popup and observes its actual opening, focused field/buttons, edits and close. Its displayed title and description feed the existing Prism screen-reader-only adapter. Native ImGui title suffixes after `##` are omitted because the title renderer hides them; description and field contents remain literal. No game-name or screen-title fallback is invented by the speech layer.

Opening queues the displayed prompt, description, current text field and keyboard hints. Tab reaches the existing OK and Cancel buttons with their positions. Returning to the field does not reopen the dialog. Cursor/selection snapshots use validated UTF-8 boundaries; typed/deleted spaces have explicit localized wording. Entered names and captions are omitted from the launcher's persistent diagnostic log. Missing screen readers remain silent.

The previous UI also confused UTF-8 bytes with a guest UTF-16 capacity, allowing an extra ASCII character that the result copy discarded. The candidate reserves the terminator, validates UTF-16 units and preserves complete code points. An oversized edit is rejected as a whole, preserving the previous name and selection. The pinned ImGui build still rejects newly typed supplementary Unicode characters; enabling its wider character ABI is outside this change.

Two passive wrappers in `src/game/native_dialogs.cpp` record keyboard dispatch/return and aggregate native result queries. They forward the original CPU context exactly, dereference no guest pointers and record no text. Actual popup lifecycle events distinguish a pending request from a displayed dialog. Original synthetic forwarding checks cover 107 calls, including nested callbacks, error returns and floating-point environment changes.

The [keyboard runtime patch and fixture](../patches/rexglue/README.md) own the candidate source. Tests execute actual extracted dialog/completion code with pinned ImGui core, synthetic input and sanitizer checks; a separate before/after test reproduces the wrong cancellation result. Launcher/speech tests cover announcement order, edits, invalid data, absent readers and log redaction. These checks do not establish live speech, physical keyboard routing, character-creator continuation or regression-free gameplay.

The name-entry candidate was installed as `diagnostic-20261014.5`, with `.4` backed up. The [launch record](diagnostic-launch.md) owns binary identities and final offline check results. In the subsequent user-controlled test, the tester confirms the reported name-entry blocker is resolved and the interaction works. This general confirmation does not separately establish every cancellation, Unicode or later gameplay case. Issue 21 is closed. Startup, option-message and main-menu narration is now installed as `.6`, with `.5` backed up, under [issue 22](https://github.com/RealAmethyst/bb-recomp/issues/22), awaiting listening verification. Recovered menu behavior and event contracts are recorded in [localization consumers](localization-consumers.md).

## Remaining direction

Trace the active creator state through any console UI request, overlap completion and controller routing, then correct the demonstrated blocker. Preserve original behavior and provide accessible interaction; do not auto-accept a dialog as a substitute.

The current game-content speech milestone uses the existing Prism screen-reader-only path for verified logo wording, title screen, main-menu items and host option dialogs. Semantic transition hooks and actual localized render consumers are implemented and checked offline; the next dependency is user listening verification. Host popup controller navigation remains an unresolved design choice. Scope, sequencing and unfinished acceptance remain in issues 5 and 22, not a second backlog here.

## Original log-review boundary

Private collection `story-freeze-20261014/log-analysis` holds the unchanged user-log snapshot, parser, comparison and assessment. That original log-review session implemented and deployed no changes. The resumed work above also does not claim the creator freeze is fixed. No game was launched, attached to, closed or controlled by the coding agent; listening and gameplay verification remain user-controlled.
