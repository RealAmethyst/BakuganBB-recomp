# Localization and console-dialog consumers

Inspection date: 2026-10-14. Owning investigation: [issue 3](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/3).

## Findings and limits

Static tracing now connects language/country settings, a request for bank zero, archive entries 22 through 28, resource descriptors and a keyboard dialog's title argument. Private bounded inspection validated 4,269 nonempty UTF-16BE entries in each of seven language-labelled resources. Actual loading, displayed captions, announcement timing and complete translation coverage remain unverified.

This is static evidence for executable SHA-256 `7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b`. Text was decoded privately; no game was launched and no manual analysis names, types, comments or function definitions were added. Read-only Ghidra inspection retained private instruction/reference exports. Addresses identify this baseline; they are not stable integration anchors.

## Conditional startup language use

Entry `0x823BCB28` calls `0x823BC940` at `0x823BCB68`, before the later call to `0x821BAF60` at `0x823BCCB4`. The language query inside `0x823BC940`, at `0x823BC9F4`, is conditional on an executable-privilege check, AV-pack branches and two successful configuration reads with additional bit tests. Its real hardware trigger and displayed warning remain unverified.

The returned language selects table entries; values at least 10 are replaced with 1. Copy helper `0x823BC8C0` reads length-prefixed 16-bit units from a sequence beginning at `0x820367A0`, performs a bounded copy and terminates it. Calls at `0x823BCAD8` and `0x823BCAEC` prepare buffers passed to `0x823BC808` at `0x823BCAFC`.

That helper creates an event, calls `XamShowMessageBoxUIEx` at `0x823BC890`, then closes the event handle. The outer entry can subsequently call `XamLoaderTerminateTitle` at `0x823BCB74`. This supports a startup-dialog/termination path, not the previously unproven assumption that this query selects normal game text and audio.

Import identities used here: `XGetLanguage` at `0x828EC6B4`, `XamShowMessageBoxUIEx` at `0x828EC6A4`, `XGetAVPack` at `0x828EC6C4`, `XamLoaderTerminateTitle` at `0x828EC6D4`, `NtCreateEvent` at `0x828EC784`, `ExGetXConfigSetting` at `0x828EC7F4`, `NtClose` at `0x828EC804`, and `XexCheckExecutablePrivilege` at `0x828EC814`. Import validation is described in [runtime coverage](runtime-imports.md).

## Provider result reaches the keyboard title argument

The instruction-level chain is:

1. In `0x82300C98`, the zero branch of the object's state word at offset `0x14` reaches a provider lookup. At `0x82300D58`, it loads the object stored at global `0x829CEB78`.
2. It loads that object's table at offset zero, selects table slot `0x18`, and calls the target at `0x82300D68` with arguments `r4 = 0x765` and `r5 = 0`.
3. The returned `r3` is stored at stack offset `0x5C` at `0x82300D6C`. The argument block beginning at stack offset `0x50` is passed in `r4` to `0x823B6A10` at `0x82300D78`.
4. `0x823B6A10` loads the pointer stored at its first object's offset `0x8`, preserves `r4`, and calls `0x823B7820` at `0x823B6A20`. This is a pointer dereference, not object-base arithmetic.
5. At `0x823B7888`, that function loads the argument block's offset `0xC` into `r6`, then calls `0x823BC4F0` at `0x823B7894`. That leaf jumps to `XamShowKeyboardUI`, thunk `0x828EC524`.

Thus the provider result reaches the fourth import argument. The pinned runtime calls that argument `title` in its reconstructed prototype; this is checked implementation evidence, not a verified vendor SDK declaration. [Keyboard prototype](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_ui.cpp#L421).

Identifier `0x765` now resolves to a bounded, nonempty entry in all seven privately inspected resources, as detailed below. Its exact text stays private. A title argument reaching a dialog does not establish its actual displayed wording, when the player encounters it or how it should be announced.

## Provider construction and lookup

Global `0x829CEB78` is initially zero in the analysis image. Constructor path `0x823B6200` allocates `0x10` bytes, stores the object to the global at `0x823B6248`, installs table `0x82036508` at object offset zero at `0x823B624C`, and initializes through `0x823B5E10`. The table word at `0x82036520`, slot `0x18`, is `0x823B60B8`.

Initialization allocates bank storage with `0x70`-byte elements and stores the bank-array pointer at object offset `0x8` at `0x823B5EA8`; object offset `0xC` holds a count byte. This verifies a construction path, not that it is the sole writer or executes in the relevant game state. An operand-based candidate scan also found reset/destruction paths `0x823B61B0` and `0x823B6278`; that scan is not exhaustive reference discovery.

Getter `0x823B60B8` loads object offset `0x8` at `0x823B60C8`, selects a bank using the low eight bits of `r5` multiplied by `0x70`, and reads the bank's descriptor-count byte at offset `0x60` at `0x823B60E0`. It traverses descriptors with stride `0xC`, subtracting each descriptor's entry count from the requested identifier until one contains the remainder. No match returns zero at `0x823B6124`.

Helper `0x8212EE18` returns zero for a null descriptor resource pointer; otherwise it reads the entry count at resource offset `0xC`. When the identifier falls within that count, the getter calls `0x8212ECF0` at `0x823B6144`. This helper reads the resource base at descriptor offset zero, reads an offset table through descriptor offset `0x8`, indexes its 32-bit entries and returns resource base plus the selected offset. A null resource base returns zero.

Assignment helper `0x8212ED40` checks an eight-byte format marker, checks 16-byte alignment of the resource-relative offsets at `0x10` and `0x14`, and requires the low bit of the byte at resource offset `0xB`. On success it stores the resource base at descriptor offset zero (`0x8212EDB8`), base plus the resource's `0x10` value at descriptor offset `0x8` (`0x8212EDC8`), and base plus its `0x14` value at descriptor offset `0x4` (`0x8212EDD4`). The meaning of that latter pointer is not established here.

This traces the selected lookup through the fields its consumer actually reads. The private resource checks below add file bounds and encoding evidence, but do not establish live resource ownership or safe hook use. In particular, no bank-index range check was identified in this getter. Readable pointers and these header checks are not substitutes for verifying resource ownership and bounds.

## Language and country settings select a resource offset

The later entry path `0x821BAF60` calls constructor `0x821BE020` at `0x821BAFBC`. That constructor calls `0x823B6AE0` at `0x821BE084`, stores its result at offset `0x10` of the object obtained from shared-state offset `0xE34`, and writes the same result to shared-state offsets `0xE38` and `0xE3C` at `0x821BE098`/`0x821BE0A0`. The shared-state pointer is loaded from global `0x829CDD04`.

`0x823B6AE0` calls wrapper `0x823BC438`, which calls `ExGetXConfigSetting` at `0x823BC460` with category 3, setting 9 and a four-byte result buffer. It accepts language values 1 through 12; invalid/error results use `XGetGameRegion` via thunk `0x828EC4C4` to choose a fallback. This is distinct from the earlier startup call to `XGetLanguage`.

The game then maps language input 1 to resource selector 1, 3 to 2, 4 to 3, 5 to 4, and 6 to 5. The pinned runtime names those language inputs English, German, French, Spanish and Italian respectively. Other language values returned by the wrapper use the country/locale fallback below. This does not establish support for Japanese, Polish or Russian merely because the wrapper accepts their language numbers. [Runtime language definitions](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/system/xcontent.h#L23).

Fallback wrapper `0x823BC238` calls `ExGetXConfigSetting` at `0x823BC260` with category 3, setting `0xE` and a one-byte result buffer. Its private byte switch table at `0x82036710` converts country IDs to locale IDs. Relevant verified mappings are country 8 to locale 3, 74 to 23, 32 to 11, and 90 to 32. The mapper selects resource value 6 for locales 3/23, 7 for locales 11/32, and otherwise 1. The runtime names those countries Belgium, Netherlands, Finland and Sweden. Failed or unsupported country conversion also has a region-derived fallback; the country table is not a direct language list. [Runtime country and locale definitions](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_locale.cpp).

Country fallback only applies when the language takes the mapper's fallback branch. For example, accepted English input selects value 1 regardless of a Swedish country setting. The archive mapping below associates selector 6 with the Dutch-labelled resource and 7 with the Swedish-labelled resource; country names alone did not establish that correspondence.

The pinned candidate runtime serves category 3 setting 9 from configurable `user_language` and setting `0xE` from configurable `user_country`. Their defaults are 1 and 103. Consequently, the separately hardcoded `XGetLanguage` implementation is not evidence that the normal resource-selection path is limited to English. No language setting was changed or tested in a running game. [Configuration implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xboxkrnl/xboxkrnl_xconfig.cpp#L49), [language default](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_user.cpp#L31), [country default](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_locale.cpp#L24).

An additional initialization path `0x823B6598` writes selector 1 at `0x823B65B8`. The operand candidate search is not exhaustive write discovery. Do not assume the selector remains immutable or that every load follows the same initialization sequence.

## Request and completion paths into the provider

Provider table slot `0x10` points to `0x823B5FB8`. This method preserves the incoming bank selector and resource-base identifier, invokes slot `0x14` to release the selected bank, then constructs a request whose first 16-bit field is shared-state `0xE38` plus incoming `r6` minus one. It marks the selected bank at offset `0x6C` and clears the provider completion byte at offset `0x4`.

The request is passed through `0x8238E7B0` at `0x823B603C` to manager `0x8238E6C0`. The latter copies the three 16-bit request fields into a `0x24`-byte queue record. The provider pointer is stored at record offset `0xC`. The low eight bits of the original bank argument are put in `r7` at `0x823B5FE8`, moved to `r9` by the wrapper at `0x8238E7B8`, and stored at record offset `0x1C` at `0x8238E710`.

At completion, `0x8238E5C8` puts the current queue record in manager offset `0x12A0` at `0x8238E5E8`. In its object-callback branch, it passes the manager in `r4` and calls provider table slot zero at `0x8238E630`. Adapter `0x823B5EE8` forwards those arguments to table slot `0x8`, whose recorded target is `0x823B5F10`. A successful callback permits the manager to set provider offset `0x4` to one at `0x8238E648`.

Callback `0x823B5F10` follows its incoming manager's `0x12A0` pointer. It takes the low byte of record offset `0x1C` as the bank index, record offset `0x10` as the resource pointer, and offset `0x14` as associated metadata. It stores the latter two at selected-bank offsets `0x64`/`0x68`, sets bank offset `0x6C`, and initializes the container through `0x8212EE88` at `0x823B5F5C`. Manager consumers suggest that the metadata is a size, but its full producer contract remains untraced.

The callback takes the container count's low byte, stores it at bank offset `0x60`, and iterates descriptors with stride `0xC`. Helper `0x8212F198` bounds the item index against container offset `0x14`, reads a relative offset from the table at container offset `0x4`, and adds the base at container offset zero. The callback passes that pointer to the previously traced descriptor assignment helper at `0x823B5F90`.

This connects request language offset, bank metadata, completion callback and the getter's descriptor layout at the instruction level. The callback does not visibly use the associated size-like metadata for bounds checks or check the descriptor-assignment result. These facts are limitations for a future safe reader, not observed crashes or a reason to execute unvalidated input.

## Requester and archive entry selection

Constructor `0x821BE020` installs table `0x82000DA8`; its slot `0x10` points to state handler `0x821BE538`. The handler indexes its switch table at `0x821BE56C` using object offset `0x14`. State 18 reaches `0x821BE958`, which requests provider slot `0x10` with bank zero and base resource ID 22 at `0x821BE97C`. State 19 waits on the provider's completion byte. Another handler, `0x822D79C0`, requests the same bank/base at `0x822D7E28` in state 7 and waits in state 8. Static state transitions do not establish that either path has run.

The provider's selector-plus-base-minus-one calculation therefore produces record IDs 22 through 28 for selectors 1 through 7. Queue consumer `0x8238E2F8` reads the first request halfword, sign-extends it at `0x8238E314` and calls `0x82131288`. That leaf checks the index against the archive count and stores it at archive-manager offset `0x8`. Sign extension does not affect these seven IDs. The read path rounds the record size up to a 2,048-byte boundary before issuing I/O; the private validator uses the exact indexed file size.

Archive parser `0x82130D80` establishes a big-endian header with record count at `0x8`, record-table offset at `0xC`, archive-offset-table offset at `0x10` and lookup-tree root index at `0x14`. Records have stride `0x10`: name offset, byte size, left child and right child. It relocates table and name offsets in memory. Helpers `0x82130EF8` and `0x82130F10` read the byte size and indexed archive offset. `0x82131370` uses the selected record and passes its archive offset plus the requested intra-file offset to `0x82131C38`; these archive offsets are bytes, not sectors.

Independent index inspection validated all 6,373 record/name bounds, the complete acyclic lookup tree and non-overlapping file ranges inside the 2,510,774,272-byte archive. The disc's bounded index read matched the previously inspected index hash. The seven selected records are:

- 22: English-labelled, 316,160 bytes.
- 23: German-labelled, 344,832 bytes.
- 24: French-labelled, 344,832 bytes.
- 25: Spanish-labelled, 333,184 bytes.
- 26: Italian-labelled, 311,424 bytes.
- 27: Dutch-labelled, 306,560 bytes.
- 28: Swedish-labelled, 301,696 bytes.

The Japanese-labelled record is 21, outside this selector range. Its existence remains insufficient evidence of supported Japanese gameplay.

## Private text-resource validation

Seven bounded archive reads totalled 2,258,688 bytes. Each resource has the observed container version 2, byte-order marker 2, flags byte 3 and eight children. Parser `0x8212EF78` establishes the child-offset table at `0x10`, the auxiliary table at `0x130` and the flag-dependent table at `0x150` for this count/version. All child headers match the descriptor checks described above. The observed headers differ from the compressed-wrapper signature checked by `0x8212F3A0`, supporting the direct-container branch for these inputs.

The flag-dependent table supplies consistent candidate child lengths. Helper `0x8212F1C8` returns its indexed values; another consumer, `0x823B2B28`, divides two such values by element strides 16 and 12. That supports a byte-size interpretation, but the complete downstream contract has not been established. The private validator treats these values as observed bounds, additionally requiring every extent to fit before the next child and within the exact archive entry. It is a validator for this observed subset, not a general format implementation.

Each resource contains the same eight entry counts: 1,529, 1,112, 840, 24, 67, 101, 94 and 502, totalling 4,269. Every offset table fits its child bound; every string offset is aligned and within the text area; every terminator occurs before the next entry or child end. All entries are nonempty and strictly decode as UTF-16BE. The provider-result consumer `0x822DBB68` independently reads 16-bit units until zero and returns twice their count, consistent with that representation.

Identifier `0x765` is decimal 1,893. Subtracting the first child's 1,529 entries selects zero-based child 1, local entry 364 in every resource. The selected text was retained only in private evidence. Equal counts and valid encoding do not prove translation quality, coverage of every game screen, voice/subtitle correspondence or the absence of unused entries.

The original private validator accepted one synthetic fixture containing a supplementary Unicode character and rejected 13 malformed cases covering header, capacity, extent, table, alignment, termination, surrogate and missing-entry errors. No game text was used in those fixtures. These checks establish the observed files' structural validity, not the safety of calling the original getter on arbitrary live pointers.

## Analysis reliability and next evidence

The decompiler treats compiler register-save helpers, including `0x823D8E88`, `0x823D8E94` and `0x823D8E9C`, as ordinary value-returning functions. This introduces misleading inferred parameters and return-value chains. The register flow above was checked against decoded instructions; inferred decompiler signatures must not become API contracts.

Private evidence is grouped in `startup-inspection`, `provider-initialization`, `provider-methods`, `resource-selection-functions`, `config-query-wrappers`, and bounded pointer/instruction exports. Separate read-only reviews checked the keyboard argument chain, language mapping and callback chain against instructions and the runtime source. Targeted non-persistent Ghidra decoding recovered the callback adapter and country-switch arms missing from the existing listing; no database annotations were added. These missing bodies also reinforce that `.pdata` coverage is not complete code coverage.

The base-ID request sites were absent from the existing instruction listing. A raw aligned-word candidate scan followed by non-persistent decoding recovered the relevant switch arms. Raw matches also included inline table data; only candidates with verified register flow and switch ownership were accepted. This is a concrete control-flow coverage concern, not evidence that the recompilation tool itself has failed.

Additional private evidence lives in `provider-raw-candidates`, `provider-request-state`, `archive-index-consumers`, `archive-positional-read` and `text-index-inspection/resource-validation`, with bounded switch, parser and consumer exports alongside them. Exact resource bytes and captions remain private. Independent review confirmed the request-to-index bridge, replayed all seven resource checks and the synthetic cases, and corrected the object-pointer dereference described above. A separate direct disc read using the index fields matched all seven validated resource hashes. These checks do not replace runtime evidence.

Next evidence is live resource ownership and release lifetime, actual dialog context/caption verification, and broader localization coverage including audio. Separately, the CPU trial needs to check these recovered switch arms against the translator's own control-flow analysis. Accessibility design follows those facts; the current findings do not authorize invented titles or a speech implementation.
