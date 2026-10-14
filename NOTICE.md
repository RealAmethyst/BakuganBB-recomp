# Licensing and provenance

## Original project work

The [BSD-3-Clause license](LICENSE) covers original project source, tools, synthetic fixtures and documentation. Code and documentation were prepared with Codex assistance. Existing copyright and author attribution must be preserved.

The game, its assets and trademarks are not covered by this license. The localization resource contains short, verified artwork captions where the game has no text resource, alongside original accessibility and diagnostic wording. Those transcriptions do not grant rights to the underlying artwork or brands. Other game-content speech comes from runtime localized text consumers.

## ReXGlue and Xenia

The [SDK patch collection](patches/rexglue/README.md) modifies pinned ReXGlue source. Its unchanged [upstream license](patches/rexglue/LICENSE.rexglue) and [affected-file notices](patches/rexglue/NOTICE.md) retain ReXGlue, Xenia and other contributor attribution. The [manifest](patches/rexglue/manifest.json) identifies the upstream revision and patch hashes.

These notices cover the source included here. They are not a complete notice inventory for a future compiled runtime distribution, which must account for its actual linked dependencies.

## Prism

The independently authored Python adapter calls the Prism API. Prism itself is not vendored or distributed here. The evaluated dependency uses MPL-2.0; its pinned identity and private build changes are recorded in the [diagnostic integration record](docs/diagnostic-launch.md). Distribution of a Prism binary must preserve its notices and satisfy the applicable source obligations, including local changes. This repository does not currently reproduce that private dependency build.

## Reference projects

Other recompilation projects informed architecture research. Their source, assets and icons have not been copied into this repository. Their licenses and contribution policies still apply to any future reuse or upstream submission.
