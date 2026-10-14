# Executable baseline and localization inventory

Inspection date: 2026-10-14.

## Findings

The project owner accepted the multilingual Xbox 360 variant as the starting candidate. The two privately supplied archives, labelled USA and USA/Europe multilingual, contain disc images with the same length and SHA-256 digest. They therefore provide the same image for this investigation, rather than two distinct regional builds.

Both archive members were fully decompressed into separate private directories. ZIP CRC verification passed for both. No original input was modified. No game was launched.

The shared image contains a retail-format XEX2 executable, seven populated language-specific audio directories, seven language-labelled introduction movies, and corresponding text/font resources. Subsequent static tracing connects language settings to seven bounded text resources; complete translation/voice coverage and runtime behavior remain unverified. Details live in [localization consumers](localization-consumers.md).

## Image and executable identity

Image length: 7,838,695,424 bytes.

Image SHA-256, identical for both variants:

```text
100ffaa99c539b2218a4ff54600a7bc0f4fd8c1f7b06480d7cbc57e5cbe254e0
```

Both ZIP members also declare CRC32 `ffd1684e`, verified during the complete read. CRC establishes archive integrity here; it is not a substitute for the SHA-256 identity comparison or external provenance verification.

The game partition contains one `.xex` file, `default.xex`, with a length of 11,055,104 bytes.

Executable SHA-256:

```text
7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b
```

No executable contents, keys, translated code or private storage paths are included in this document.

## XEX header observations

The following values were read from the executable's headers. They are declarations in the file, not results from executing or disassembling it.

- Format: `XEX2`.
- Module flags: `0x00000001`, the title-module flag.
- Header area: 12,288 bytes; 15 optional headers.
- Title ID: `41560857`.
- Media ID: `221503EA`.
- Version and base-version fields: both raw `0x00000001`.
- Disc number/count: 1 of 1.
- Region mask: `0xFFFFFFFF`, all regions in the referenced format definitions.
- Declared image base: `0x82000000`.
- Declared entry point: `0x823BCB28`.
- Declared image size: 11,206,656 bytes.
- Encryption type: 1, normal encryption.
- Compression type: 1, basic compression.

Header layouts and enum meanings were checked against [Xenia's XEX2 definitions](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/kernel/util/xex2_info.h). Optional-header handling and import-string alignment were cross-checked against [Xenia's loader implementation](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/xex_module.cc).

The metadata inspection used big-endian reads with bounds checks, checked optional-header uniqueness, and validated the sizes/alignment of import and static-library records. It did not decrypt the executable payload. Raw execution-version fields are retained without asserting a user-visible version number.

## Runtime dependency evidence

The import metadata declares two libraries:

- `xam.xex`: 212 import-table records.
- `xboxkrnl.exe`: 323 import-table records.

Both specify raw version `0x20247000` and minimum version `0x20213200`. Record counts are **not counts of unique imported functions**. Resolving ordinals, variable imports and thunk relationships requires inspection of the decoded image.

The static-library metadata names `XAPILIB`, `XBOXKRNL`, `XONLINE`, `LIBCPMT`, `XAUDIO2`, `XAPOBA`, `X3DAUD`, `XMP`, `D3D9`, `D3DX9`, `XGRAPHC`, `XNET` and `XMCORE`. All have major/minor/build fields 2/0/9328; XAUDIO2 and XAPOBA have QFE field 4, while the others have 0.

These are useful evidence for graphics, audio and platform integration research. They do not prove which individual APIs are reached, establish an engine identity, or imply that every named facility needs a complete implementation.

## Localization evidence and limits

The disc filesystem contains audio directories named English, French, German, Spanish, Italian, Dutch and Swedish. Each contains 1,140 files. Aggregate sizes differ between languages, so the directories are populated rather than empty labels. This does not establish that every file is a distinct translated voice recording; no audio was played.

Movie filenames include a language-labelled introduction for each of the seven languages. There are 13 movie files overall. Playback, stream composition, subtitles and language-selection behavior have not been examined.

The 387,296-byte main archive index contains names for `font/common_English.txb`, `font/common_French.txb`, `font/common_German.txb`, `font/common_Spanish.txb`, `font/common_Italian.txb`, `font/common_Dutch.txb` and `font/common_Swedish.txb`, plus `font/font.bin`. It also contains 18 language-labelled sound-bank paths for each of the seven languages.

An additional `font/common_Japanese.txb` name occurs in the index. This is **not evidence of an eighth supported game language**. It could be shared, unused or partial material; the actual consumer and data need examination.

The initial names-only inspection has been superseded by source-checked index parsing and bounded validation of the seven selected text resources. Each contains 4,269 nonempty UTF-16BE entries; the consumer chain and remaining limits are recorded in [localization consumers](localization-consumers.md). A filename or decoded entry alone is still not proof of the exact text displayed or spoken in a running game.

## Other subsystem inventory

The filesystem includes a main archive of 2,510,774,272 bytes and its separate index. The shader directory contains six `.par` files, including material, post-processing and parameter-oriented names. Audio files use `.past`; movie files use `.pam`.

These extensions and names are format leads only. They do not establish a codec, archive layout, shader compatibility or middleware identity. Do not import assumptions from another game's similarly named formats.

## Inspection tools and preparation

The inspection used Python's standard ZIP/hash facilities and the official `xdvdfs` 0.8.3 Linux release. The prepared executable reports `xdvdfs-cli 0.8.3`. Its SHA-256 is:

```text
ba860c079d756451e6396824e9866724d9765d4cabd1ecee148e53d18350a2e8
```

Upstream source supports full-disc offsets including XGD2. Its file listing and selective copy operations were used; no image rewriting occurred. [Filesystem offset handling](https://github.com/antangelo/xdvdfs/blob/v0.8.3/xdvdfs-core/src/blockdev.rs).

System-wide installation was requested from the project owner, following their preference for reusable tools. It is now available system-wide and still reports version 0.8.3. Initial inspection proceeded with the prepared executable before that installation.

Ghidra 12.1.2 DEV is already installed. No XEX loader was found in the inspected system or user extension directories. The upstream XEXLoaderWV release labelled for 12.1.2 has an internal `extension.properties` version of 13.0.0. Its published SHA-256 was verified, so the mismatch is in the release package rather than an observed download error. That archive was not installed.

Loader source was checked out at `d0af801aee083c86950b90c3db78b2e1c642067f`, outside this project. Its hardcoded extension version was replaced locally with Ghidra's documented `@extversion@` build token, matching the installed extension skeleton. Three blocks that print key values were removed without changing decoding operations. No commit or upstream contribution was made.

Gradle 9.8.1 is now installed. The loader compiled successfully against the installed Ghidra and was tested using a private Ghidra settings directory. The output extension metadata correctly specifies version 12.1.2. The checked archive has SHA-256 `bfc512e842299a13fbdb29c4b816ecc150de2c9e80bfa9291e60d9d5fbcabd2c`. The project owner has now installed the checked loader system-wide. Its installed metadata specifies version 12.1.2, and all 20 installed extension files were compared with the checked archive with no content mismatches.

## Private Ghidra import results

Headless import completed successfully, saved a private reverse-engineering project and exported metadata. Full auto-analysis was disabled. The game was not executed. The loader created functions from `.pdata`, which also produced disassembly and default symbols; disabling full auto-analysis does not imply that no automatic annotations were created.

The first import attempt stopped before loading because Ghidra rejects project paths containing components beginning with a dot. A separate non-hidden private project directory resolved that documented runtime error; game inputs and tool settings remained in their existing private locations.

The imported language is `PowerPC:BE:64:A2ALT-32addr`. Sixteen memory blocks were created. The principal code block starts at `0x82120000` and has a size of `0x7CD314`; `.rdata` starts at `0x82000600`, `.pdata` at `0x820F5800`, and `.data` at `0x82980000`. Eight additional executable blocks use the `.embsec_` name. Their purpose is not yet established.

The loader resolved 275 import-reference labels and 260 thunk labels, accounting for the 535 header import records. Examples include console-language queries, XMA/audio output, controller state and content/file operations. API availability and correct runtime semantics remain unverified.

The loader created 19,249 function definitions from `.pdata`. This is a starting analysis database, not validated total function coverage. The symbol snapshot contains 162,978 entries: 275 imported, 19,510 analysis-generated and 143,193 default symbols. No manual names, comments or type annotations were added.

Every symbol's address, name, type and source is preserved in the private `loader-symbols.tsv` manifest. Its SHA-256 is `5f8d49b5161bb194b5abb0c2b519db4f7546373cf3ae376f72fe00eaf50ea55d`. Memory-block and summary exports are retained alongside it. These private manifests include generated analysis metadata and are not copied into the public project.

A loader limitation was observed: Ghidra's program-level image-base property is zero even though section addresses are correctly placed at their XEX-declared locations. Use the verified XEX base and explicit memory blocks for current research; do not infer relocated addresses from that zero property. This report does not claim the loader has been exhaustively validated.

## Initial language-query trace

A read-only follow-up found a reference to the loader-resolved `XGetLanguage` thunk at `0x828EC6B4`. The call site is `0x823BC9F4`, inside the function beginning at `0x823BC940`. The name corresponds to XAM ordinal `0x3CD` in [Xenia's export table](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/kernel/xam/xam_table.inc).

The caller's decompilation was saved privately. This initial reference does not establish the game's text/audio selection path. It must not be presented as proof that the seven resource groups are selected automatically from the console language. Trace the actual archive/text consumer and its language value before adopting that conclusion.

## Follow-up validation

A subsequent read-only pass validated every import record and every `.pdata` entry. The 535 records comprise 275 references (260 function imports and 15 variables) and 260 paired thunks, with no name/ordinal discrepancies against the pinned ReXGlue module export tables. Method, limits and candidate-runtime findings are in [the import report](runtime-imports.md).

All 19,249 `.pdata` entries are unique, four-byte-aligned addresses in executable memory, with a function defined at each address. This establishes complete coverage of that table by the loader. It does not establish complete function discovery, correct body boundaries, indirect-target coverage or safe static translation. A targeted reference pass also found keyboard and device-selector jumps with no containing function in the current inventory.

The loader changes the first two bytes of the first two words in each import thunk in the analysis image. The original XEX remains unchanged. Do not use the Ghidra image as an unmodified executable baseline; the import report links the responsible loader source.

A bounded scan of initialized blocks found none of six selected localization markers (`English`, `French`, `German`, `Swedish`, `common_`, `.txb`) in ASCII or UTF-16BE. This is not a general text inventory or evidence that localization is absent. Other representations, external resources and indirect selection remain unexamined.

After system-wide loader installation, using the earlier private settings directory produced a duplicate-module collision with its test extension. The successful follow-up used fresh private Ghidra settings and the installed system extension. No installed extension or live desktop service was changed. The failed and successful logs are retained privately.

No manual annotations were added. All follow-up processing used read-only mode with full auto-analysis disabled. Private exports preserve the import records, table coverage and critical-call references; no proprietary export was copied here.

## Next verification

The [instruction-level localization follow-up](localization-consumers.md) distinguishes the conditional startup-language dialog from the normal language/country mapper. It connects a bank-zero request to archive entries 22 through 28, validates their text tables, and traces one entry into a keyboard-title argument. Actual displayed captions remain unverified. The [Linux SDK trial](linux-toolchain-trial.md) records the separate tool build, baseline failures and verified candidate fixes; it did not translate or execute this game.

Trace critical import callers back to actual game operations, investigate function/control-flow coverage beyond `.pdata`, and trace language selection and the archive/text consumers before declaring runtime or localization behavior confirmed. The follow-up validated import identity and table-entry coverage; those checks do not complete semantic analysis.

No manual reverse-engineering annotations were added. Automatic loader annotations and the initial language-reference addresses are recorded above. No runtime behavior has been confirmed.
