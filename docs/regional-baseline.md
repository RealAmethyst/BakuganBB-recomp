# Xbox 360 regional baseline recommendation

Research date: 2026-10-14.

## Follow-up inspection

The multilingual candidate was accepted. Subsequent private inspection found that both supplied archives contain the same disc image by length and SHA-256. Seven language resource groups are present. See the [executable baseline findings](executable-baseline.md) for current evidence and remaining verification. The sections below retain the reasoning and evidence available before that inspection.

## Recommendation

Use the USA/Europe variant labelled with English, French, German, Spanish, Italian, Dutch and Swedish as the first candidate for executable analysis. Keep the USA-labelled variant available for comparison.

This recommendation favors localization coverage for a public, accessible port. It is not a claim that the multilingual executable is newer, less buggy or already compatible with a particular recompilation toolchain. The baseline remains provisional until executable identity and actual localization resources are verified.

## Local evidence

A read-only inspection found two Battle Brawlers ZIP archives in the privately supplied directory:

- A USA-labelled variant.
- A USA/Europe variant labelled `En,Fr,De,Es,It,Nl,Sv`.

Each ZIP contains one ISO entry of 7,838,695,424 uncompressed bytes. Only archive directory metadata was read. Neither ISO was extracted or launched, and no full archive integrity check was performed.

Equal image sizes do not establish identical executables or content. Filenames do not independently verify language availability, voice coverage, executable revision or region flags. The USA filename does not prove that it is English-only.

The private storage path and its provenance are deliberately absent from this project document.

## Community research

Searches for Xbox 360 regional differences, PAL/NTSC comparisons, bugs and preferred versions did not establish a reliable community preference for either of these Xbox 360 builds. This is a limit of the evidence found, not proof that no differences exist.

The community's [definitive-version discussion](https://www.reddit.com/r/Bakugan/comments/1n89kpx/) primarily compares platforms, including graphics, motion controls and handheld play. It contains differing preferences, rather than an agreed best regional Xbox 360 release. Platform and special-edition claims must not be transferred to these two builds without verification.

A separate [native Xbox 360 port discussion](https://www.reddit.com/r/Bakugan/comments/1pz0khy/bakugan_battle_brawlers_xbox_360_native_port/) expresses interest in static recompilation and translation work. That thread is a proposal/discussion, not evidence of an existing completed port or a recommended executable revision.

No acquisition sources, media attachments or game downloads were used as evidence.

## Verification before fixing the baseline

Privately inspect both existing variants and compare executable hashes, execution/version metadata, modules and imports. Verify the multilingual variant's actual localization resources and how the game selects a language. Distinguish text, interface and voice language coverage.

If the executables match, investigate data and localization differences separately. If they differ, identify code and runtime consequences before assuming one configuration supports both. Record one verified baseline first; additional variants can receive separate support evidence later.

Public records may retain reviewed identity and compatibility findings, but must exclude private paths, game content, generated proprietary source and acquisition guidance. No baseline has yet been declared verified.
