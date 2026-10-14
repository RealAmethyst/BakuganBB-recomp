# Bakugan Battle Brawlers native PC port

## Project direction

- Statically recompile the Xbox 360 version of Bakugan Battle Brawlers (2009) into a native x86-64 application. Linux is the primary platform; Vulkan is the primary graphics API.
- Preserve original gameplay and content behavior. Full accessibility and complete start-to-finish playability are core requirements. Higher resolutions, frame-rate changes and other enhancements follow the accurate, playable foundation.
- Native execution may use reimplemented console services and graphics translation. The deliverable must not require running a traditional Xbox 360 emulator.
- Establish small, verifiable milestones. XenonRecomp and XenosRecomp are candidates, not confirmed solutions for this game. Verify the target executable, runtime requirements and shader behavior before selecting implementations.
- The multilingual Xbox 360 baseline candidate was accepted on 2026-10-14. Initial executable identity and localization evidence are recorded in `research/executable-baseline.md`; distinguish verified file metadata from unverified runtime behavior.

## Hard rule: no game assets or acquisition guidance

- Never include game assets in the repository, documentation, examples, test fixtures, CI artifacts or release packages. Never provide sources, links or instructions for obtaining game assets.
- Do not download game assets. Privately supplied game inputs and reverse-engineering material must remain separate from publishable source and artifacts.
- Keep game executables, extracted data, decompilation dumps and generated proprietary source out of version control. Review shader caches, embedded resources and generated build outputs for game content before any proposed publication; a generated file is not automatically suitable for redistribution.
- Use original synthetic fixtures for public tests. Check tracked files and packaged contents; ignore patterns alone do not enforce the rule.

## Engineering and accessibility

- Verify game-specific claims against the actual target and record supporting evidence. Reference ports demonstrate techniques; their addresses, layouts and behavior do not establish facts about this game.
- Keep generated translation, host runtime, rendering, game-specific integration and accessibility concerns modular. Prefer semantic game events and verified localized text sources for accessibility.
- Accessibility must accompany each playable milestone, including setup, settings, errors and saving. User-facing accessibility behavior remains the project owner's decision.
- Preserve required third-party attribution and verify licenses and contribution policies before reuse or upstream contributions. Keep shared content impersonal.
- GitHub Issues is the authoritative tracker: `RealAmethyst/BakuganBattleBrawlersRecomp`, created private with Issues enabled on 2026-10-14 following explicit approval of name and visibility. Research is grouped under issue 1. Maintain that tracker; do not create a competing Markdown backlog. Public visibility, commits and pushes require their existing separate authorization.
- The repository for the other Bakugan game is unrelated; do not reuse it. Executable identity and loader limits live in `research/executable-baseline.md`; import validation, candidate-runtime gaps and static caller evidence live in `research/runtime-imports.md`. First native compile/link evidence, codegen corrections and graphics/audio test gates live in `research/first-build-gates.md`. Diagnostic packaging and Prism decisions live in `research/diagnostic-launch.md`; the first audible launch failure and translator corrections live in `research/first-launch-failure.md`. Original clock/movie contracts, timing corrections and diagnostic measurement limits live in `research/timing-contracts.md`. The current Story character-creator blocker and next-session evidence boundaries live in `research/story-entry-freeze.md`.
- Global permissions still apply: do not commit or push without a request, launch or control the game without explicit permission, or alter live screen-reader services without permission for that specific action.
- Private diagnostic launch preparation and its verification boundaries live in `research/diagnostic-launch.md`. Speech uses Prism with standalone TTS backends disabled, including Speech Dispatcher and SAPI; a running screen reader receives speech, while absence of one remains silent. Launcher wording belongs in central localization resources.
