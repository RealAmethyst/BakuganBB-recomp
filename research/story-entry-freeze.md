# Story entry and character-creator input

Inspection date: 2026-10-14. Owning work: [21](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/21); accessibility work: [issue 5](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/5). Build identity and opening timing are in [timing contracts](timing-contracts.md).

## User result

The diagnostic-20261014.4 user test reports smooth video playback and working menus. Story reaches the character creator, but input appears unresponsive while audio continues. No screenshot or native screen-identity event was captured in this investigation; the screen identity and audible continuation are the tester's observations.

## Log findings

Guest update callbacks and approximately 30 GPU swap requests per second continue until the normal window-close path. The process exits zero, with no logged error, critical, assertion or abort. These counters show continuing activity; they do not prove the creator's own state machine or input handling progresses.

Near host second 191, host UI presentation jumps from approximately 30 to approximately 7,940 acceptances per second. Guest swaps remain near 30. This pattern is compatible with an active host dialog continuously repainting while waiting for interaction. It differs from the prior idle-toast defect, whose empty-queue demand is removed in this build. The actual dialog is not named by the captured log, so the cause remains unconfirmed.

Forty-two file lookups return status `0xC000000F`, including texture-related requests around creator activity. They may be probes followed by another resolution path. No missing-content or freeze-causality claim is justified without tracing the consumer and fallback.

## Bounded source hypothesis

The pinned runtime's `src/kernel/xam/xam_ui.cpp` implements `XamShowKeyboardUI` with an ImGui `KeyboardInputDialog` in the normal UI path. It uses text entry and confirmation/cancellation controls; dialog dismissal supplies the asynchronous completion. An active instance retains continuous repaint demand. The prior [localized keyboard-title trace](localization-consumers.md) supplies a game-side research starting point, not proof this particular call happened during the run.

A pending name-entry dialog, input capture or incomplete console-UI completion is therefore a concrete next-session hypothesis. Current evidence does not distinguish these from other creator-specific state/resource issues. No controller mapping, dialog acceptance, native screen-reader service or game state was changed.

## Next-session direction

Trace the active creator state through any console UI request, overlap completion and controller routing, then correct the demonstrated blocker. Preserve original behavior and provide accessible interaction; do not auto-accept a dialog as a substitute.

The next accessibility milestone is game-content speech through the existing Prism screen-reader-only path: verified displayed logo wording, title screen, menu items and dialogs. Resolve actual localized text and native screen lifetimes before hooking speech. Artwork-only wording requires independent verification that no text source exists. Scope, sequencing and unfinished acceptance remain in issue 5, not a second backlog here.

## Evidence boundary

Private collection `story-freeze-20261014/log-analysis` holds the unchanged user-log snapshot, parser, comparison and assessment. No game was launched, attached to, closed or controlled by the coding agent. No new runtime implementation or deployment occurred in this session. This report does not claim the creator freeze is fixed.
