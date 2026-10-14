# BB Recomp

An early, work-in-progress PC recompilation of Bakugan Battle Brawlers (2009) for Xbox 360. Linux is the first target, with other platforms planned. The aim is to preserve the original game and make it accessible to blind players.

**You need your own game files to build and play. No game files are included here.**

## Current status

Input and audio are working, but this is still very early. Expect crashes and unfinished features. I haven't tested far into the game, so don't expect a complete playthrough yet.

## Building

There isn't a prebuilt release yet. You can build the game locally on Linux using your own game files. Follow the [build guide](docs/building.md) for the required tools and steps.

Developers can already run the automated code checks with `python3 tools/run_tests.py`. These checks don't run the game or need game files. The build guide lists the tools needed to run them.

## Reporting bugs

I am blind, so I can't visually check the graphics. Reports of missing, distorted or incorrectly displayed graphics are especially helpful. Please describe what looks wrong and what you expected to see.

[Report a bug on GitHub](https://github.com/RealAmethyst/bb-recomp/issues/new), including:

- Your build or commit, operating system, GPU and driver version.
- How to reproduce the problem and where it happens in the game.
- What happened and what you expected. For speech problems, include what you heard or what was missing.

Please don't upload game files or private logs.

## Community and contributing

[Join the Discord community](https://discord.gg/ftWEJ7YDx8) to talk about the project, share feedback or get involved.

Help with testing, graphics, blind accessibility, reverse engineering and other platforms is welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) before contributing code. The [documentation](docs/README.md) covers the code layout and what we've learned about the game.

## License and credits

Original project code and documentation use [BSD-3-Clause](LICENSE). Third-party credits and licenses are listed in [NOTICE.md](NOTICE.md). The game and its assets are not covered by this license. This is an unofficial project.

Code and documentation were prepared with Codex assistance.
