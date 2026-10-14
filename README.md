# Bakugan Battle Brawlers native PC port

An experimental native x86-64 port of the Xbox 360 version of Bakugan Battle Brawlers (2009), with Linux and Vulkan as the primary targets. Accurate gameplay and complete accessibility are core requirements.

## Current milestone

The private diagnostic build reaches the title and main menus with working input, music and smooth opening playback. Story entry currently reaches an unresponsive character creator. Complete gameplay, saving and game-content speech are not established.

The game CPU code is statically recompiled. The evaluated runtime uses ReXGlue's Xenos-to-Vulkan backend, including Xenia-derived graphics and console-service code. It does not launch a traditional emulator executable.

## Repository contents

- [Diagnostic integration](mod/README.md): independently authored host hooks, launcher, Prism adapter and synthetic checks.
- [Timing evidence](research/timing-contracts.md): verified clock contracts, local runtime corrections and user-test results.
- [Story-entry investigation](research/story-entry-freeze.md): current blocker and evidence limits.
- [Localization research](research/localization-consumers.md): verified resource consumers for future accessibility work.
- [ReXGlue evaluation patches](research/patches/rexglue/README.md): pinned local candidates, original fixtures and retained notices.
- [Project instructions](AGENTS.md): scope, source boundaries and authoritative tracker.

GitHub Issues is the authoritative work tracker. Research records distinguish measurements, user observations, hypotheses and untested behavior.

## Build and content boundaries

This is a source and research snapshot, not a complete game distribution or a standalone build of the proprietary translation. Game executables, assets, generated game code, shader caches, dumps and native game binaries are excluded. Private integration requires separately prepared inputs and generated outputs; this repository provides no asset acquisition instructions.

Offline synthetic checks and private integration evidence are described in the component records. Only user-controlled in-game testing establishes listening usability and gameplay behavior.

## Accessibility and attribution

Diagnostic startup/failure speech uses Prism with standalone text-to-speech disabled. A running screen reader receives announcements; without one, diagnostics remain silent. Full game-content speech is planned and is not implemented by the current launcher.

Code and research were prepared with Codex assistance. ReXGlue-derived changes retain their [upstream license](research/patches/rexglue/LICENSE.rexglue) and [affected-file notices](research/patches/rexglue/NOTICE.md). Dependency adoption, public visibility, native release packaging and project-wide licensing remain separate decisions.
