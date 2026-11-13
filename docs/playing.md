# BB Recomp alpha test build

## What this is

This is an early test build of BB Recomp, an unofficial PC version of Bakugan Battle Brawlers (2009, Xbox 360) with screen-reader speech for blind players.

It is an alpha. Expect crashes, missing features and graphics problems. It exists so that people can try it and tell us what works and what does not. A complete playthrough has not been tested.

**No game files are included.** You need your own copy of the game. The first time you start the build, it asks where your game files are.

## What you need

- Your own game files for Bakugan Battle Brawlers on Xbox 360, unpacked into a folder. The folder must contain `default.xex` and the `media` folder. Only one version of the game is supported; the launcher checks this for you and tells you if your files are a different version.
- A 64-bit PC with a graphics card and driver that support Vulkan.
- A controller is recommended.

On Windows you also need:

- Windows 10 or newer.
- The Microsoft Visual C++ Redistributable for x64. Many PCs already have it. If the launcher says it is missing, download it from Microsoft at <https://aka.ms/vs/17/release/vc_redist.x64.exe>, install it and start again.
- For speech, a running screen reader such as NVDA or JAWS.

On Linux you also need:

- A distribution with glibc 2.38 or newer and GCC 13 runtime libraries or newer. Ubuntu 24.04, Fedora 39, Debian 13 and current Arch-based systems qualify.
- Python 3.9 or newer, which most distributions include.
- For speech, a running Orca screen reader and the `glibmm` 2.68 library. Its package is called `libglibmm-2.68-1t64` on Ubuntu, `glibmm2.68` on Fedora and `glibmm-2.68` on Arch.
- For the folder dialog, `zenity` or `kdialog`. Without one, the launcher asks in the terminal instead.

## Starting the game

Unpack the download into a folder you can write to, such as a folder in your home directory or Documents. Do not put it in Program Files. Settings, saves and logs are stored inside this folder, in `user-data`.

- **Windows:** start your screen reader, then run `Play.bat`.
- **Linux:** start Orca, then run `./launch.py` in a terminal from the unpacked folder.

### First start

1. The launcher says that your game files are needed and opens a folder dialog.
2. Choose the folder that contains `default.xex`.
3. The launcher checks the files. If they are the supported version, it says "Game files verified", remembers the folder and starts the game.

If the folder is wrong, the launcher explains why and asks again. Cancelling the dialog closes the launcher without starting the game.

Your game files stay where they are. They are not copied, changed or uploaded.

You can also give the folder on the command line. This is useful if the dialog does not work for you:

- **Windows:** `Play.bat --game-dir "C:\path\to\game folder"`
- **Linux:** `./launch.py --game-dir "/path/to/game folder"`

To choose a different folder later, use `--game-dir` again or delete `game-data.json` from the unpacked folder.

### Speech

Speech goes to your running screen reader. If no supported screen reader is running, the game stays silent; it does not start a voice of its own.

## Known limits

- This is an early build. Crashes and unfinished features are expected.
- The Windows build has been compiled and checked automatically, but it had not been run on a real Windows PC before this release. Reports from Windows are especially valuable.
- The spatial gate-card sound cues from development builds are not in this release. Their sounds are made from game audio, which cannot be distributed. Speech for gate cards is included.
- The developer is blind and cannot check graphics. Please report anything that looks wrong.

## Giving feedback

Feedback is the whole point of this alpha. Please tell us what happened, good or bad.

- [Report a problem on GitHub](https://github.com/RealAmethyst/BakuganBB-recomp/issues/new)
- [Join the Discord community](https://discord.gg/ftWEJ7YDx8)

Helpful details:

- The version in `release.json`, your operating system, graphics card and driver version.
- Your screen reader and controller, if you use them.
- What you did, what happened and what you expected. For speech problems, what you heard or what was missing.

Logs are in `user-data/logs`. They can contain personal folder names, so read and trim them before sharing. Never upload game files.

## Licenses and source

This is an unofficial project. The game, its assets and its trademarks belong to their owners and are not covered by this project's license.

The project's own code uses the BSD-3-Clause license in `LICENSE`. Third-party notices are in `NOTICE.md` and the `licenses` folder. The complete source code, the changes made to third-party libraries and the build instructions are at <https://github.com/RealAmethyst/BakuganBB-recomp>. `release.json` names the exact source revisions this build was made from.
