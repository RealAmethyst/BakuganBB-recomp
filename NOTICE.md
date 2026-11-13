# Licensing and provenance

## Original project work

The [BSD-3-Clause license](LICENSE) covers original project source, tools, synthetic fixtures and documentation. Code and documentation were prepared with AI assistance from Codex and Claude. Existing copyright and author attribution must be preserved.

The game, its assets and trademarks are not covered by this license. The localization resource contains short, verified artwork captions where the game has no text resource, alongside original accessibility and diagnostic wording. Those transcriptions do not grant rights to the underlying artwork or brands. Other game-content speech comes from runtime localized text consumers.

## ReXGlue and Xenia

The [SDK patch collection](patches/rexglue/README.md) modifies pinned ReXGlue source. Its unchanged [upstream license](patches/rexglue/LICENSE.rexglue) and [affected-file notices](patches/rexglue/NOTICE.md) retain ReXGlue, Xenia and other contributor attribution. The [manifest](patches/rexglue/manifest.json) identifies the upstream revision and patch hashes.

These notices cover the source included here. A release package additionally carries, in its `licenses` folder, the SDK license, the license files of the SDK's vendored components and a list of the components whose terms appear only in their source headers. The [release guide](docs/releasing.md) describes how that folder is gathered and its limits.

## Prism

The independently authored Python adapter calls the Prism API. The complete Prism library is not vendored here. The [Prism patch and build instructions](patches/prism/README.md) provide the local change at its pinned revision, under MPL-2.0, with upstream notices retained. Distribution of a Prism binary must preserve its notices and satisfy the applicable source obligations, including local changes. Earlier integration evidence is recorded in the [diagnostic record](docs/diagnostic-launch.md).

## Release packages

A release package contains the compiled host, the patched ReXGlue runtime and graphics plugin, the patched Prism library, the Python launcher and, on Windows, the unmodified Python embeddable runtime under its own license. The compiled host includes a machine translation of the game's executable code. The package contains no game data, artwork, audio, text resources or original executable, and it cannot run without the player's own game files. The game and its trademarks remain the property of their owners; no rights to them are granted.

FFmpeg is built in its LGPL 2.1-or-later configuration and linked into the ReXGlue runtime library, whose complete source and build instructions are public. Each package's `release.json` names the exact project, ReXGlue and Prism revisions it was built from.

## Reference projects

Other recompilation projects informed architecture research. Their source, assets and icons have not been copied into this repository. Their licenses and contribution policies still apply to any future reuse or upstream submission.
