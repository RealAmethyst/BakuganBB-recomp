# Bakugan Battle Brawlers Recomp

An early, work-in-progress recompilation of the Xbox 360 version of Bakugan Battle Brawlers (2009) for native PC play. Linux and Vulkan are the first targets, with other platforms planned for the future. Accurate gameplay, blind accessibility and understanding the original game are core goals.

**You need your own copy of the game to build or run the native game.** This repository contains no game files, generated game code or playable binary, and provides no instructions for obtaining game assets.

## Current status

Input is working, audio works, graphics should work (see later section on that) Expect crashes, do not excpect to get any far in the game, cool if you do, but I haven't verified it.

## Building

You need your own game files to build and run the native game. No game files, generated game code or playable binaries are included in this repository.

A complete public build procedure for the playable game is still being prepared. The native build currently depends on a specific game executable, separately generated translation, a patched ReXGlue SDK and a separately built Prism library. The [build guide](docs/building.md) records those requirements and the remaining gaps.

You can already build the shared core and run the source-only tests without the game. On Linux, install:

- System Python 3.9 or newer.
- CMake 3.20 or newer.
- A C++17-capable compiler and Make.
- Bubblewrap (`bwrap`), with permission to create unprivileged namespaces.

From the repository root, run:

```sh
python3 tools/run_tests.py
```

This builds the core and runs the synthetic tests in isolation. It does not build or launch the game. See the [build guide](docs/building.md) for retained test output and launcher staging.

## Reporting bugs

This is early development, and complete gameplay and saving have not been verified. Graphics are especially under-tested: I am blind and cannot visually inspect the game's output. Reports of rendering problems are welcome; please describe what looks wrong and what you expected to see.

[Open an issue](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/new) with:

- The build or commit you tested, your operating system, GPU and driver version.
- Steps to reproduce the problem and the screen or part of the game affected.
- What happened, what you expected, and whether it happens consistently.
- For graphics problems, the graphics settings and a description of missing, distorted or incorrectly displayed elements.ed game code or private logs. 

## Get involved

Help with testing, graphics, accessibility, reverse engineering, documentation and cross-platform support is welcome. You can [report a problem or discuss an idea in GitHub Issues](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues), or contribute a focused pull request. Read [CONTRIBUTING.md](CONTRIBUTING.md) before starting.

https://discord.gg/ftWEJ7YDx8

## Documentation

- [Architecture](docs/architecture.md): shared core, game integration, runtime and platform responsibilities.
- [Documentation index](docs/README.md): recovered game behavior, evidence and outstanding limits.

## Source layout

- `src/core`: shared C++ utilities and Python accessibility policy.
- `src/game`: verified game hooks and integration.
- `src/runtime`: ReXGlue host and event transport.
- `src/platform/linux`: Linux process supervisor.
- `config` and `resources`: hook configuration and project localization resources.
- `tests`: original synthetic native and Python fixtures.
- `patches`: pinned SDK corrections and retained third-party notices.
- `tools`: offline validation and source staging helpers.
- `docs`: architecture, build guidance and reverse-engineering findings.

## Accessibility and game understanding

Speech uses Prism with standalone text-to-speech disabled. A running screen reader receives announcements; absence of one remains silent. Coverage is incomplete and grows alongside playable milestones.

Reverse engineering is part of the deliverable. Each screen, feature and mechanic should gain documented ownership, state transitions, text consumers and input behavior as it is investigated. Verified semantic hooks are preferred over repeated memory polling. Raw decompilation and generated proprietary code stay outside this repository.

## License and attribution

Original project code and documentation use [BSD-3-Clause](LICENSE). Third-party material retains its own terms; see [NOTICE.md](NOTICE.md). This license grants no rights to the game, its assets or trademarks. This is an independent project, not an official game release.

Code and documentation were prepared with Codex assistance. Progress and contributions are tracked in [GitHub Issues](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues).
