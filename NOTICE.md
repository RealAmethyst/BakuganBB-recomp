# Licensing and provenance

## Original project work

The [BSD-3-Clause license](LICENSE) covers original project source, tools, synthetic fixtures and documentation. Code and documentation were prepared with Codex assistance. Existing copyright and author attribution must be preserved.

The game, its assets and trademarks are not covered by this license. The localization resource contains short, verified artwork captions where the game has no text resource, alongside original accessibility and diagnostic wording. Those transcriptions do not grant rights to the underlying artwork or brands. Other game-content speech comes from runtime localized text consumers.

## ReXGlue and Xenia

The [SDK patch collection](patches/rexglue/README.md) modifies pinned ReXGlue source. Its unchanged [upstream license](patches/rexglue/LICENSE.rexglue) and [affected-file notices](patches/rexglue/NOTICE.md) retain ReXGlue, Xenia and other contributor attribution. The [manifest](patches/rexglue/manifest.json) identifies the upstream revision and patch hashes.

These notices cover the source included here. They are not a complete notice inventory for a future compiled runtime distribution, which must account for its actual linked dependencies.

## Prism

The independently authored Python adapter calls the Prism API. The complete Prism library is not vendored here. The [Prism patch and build instructions](patches/prism/README.md) provide the local change at its pinned revision, under MPL-2.0, with upstream notices retained. Distribution of a Prism binary must preserve its notices and satisfy the applicable source obligations, including local changes. Earlier integration evidence is recorded in the [diagnostic record](docs/diagnostic-launch.md).

## Reference projects

Other recompilation projects informed architecture research. Their source, assets and icons have not been copied into this repository. Their licenses and contribution policies still apply to any future reuse or upstream submission.
