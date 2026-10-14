# Localization and console-dialog consumers

Inspection date: 2026-10-14. Owning investigation: [issue 3](https://github.com/RealAmethyst/bb-recomp/issues/3).

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

## Initial main-menu speech investigation

Fresh read-only instruction exports in private collection `menu-speech-20261014` establish useful lifecycle and selection evidence, but do not yet verify the displayed main-menu title, row captions or button hints. No annotations were added. Nearby localized candidates must not be substituted for those missing captions.

Constructor `0x822CED20` installs table `0x8201205C`; handler `0x822CF7D0` enters active input state 4 after its animation-state gate, then calls `0x822CEDF8`. Normal close state 10 calls `0x822CEB38`, which destroys owned widgets through `0x823CE5F8`, then calls `0x822CE988 → 0x822CE970`. This publishes the selected result through menu offset `0x18` and calls `0x823B87A8`, marking object offset `0x10` for removal. Alternate destruction paths still need coverage; focus loss is not a verified close.

Five rows are registered by `0x822CF1A8`, including the conditionally disabled row at index 3. The live selection chain differs from the previous-selection field: selector table slot `0x24` calls `0x821D7610`, dereferencing selector `0x28`, then nested table slot `0xC` calls `0x821D86D8`, reading nested `0x4`. Selector field `0x34` is previous selection. The exact main-menu setup caller and live count/bounds ownership are not yet complete enough for a safe new reader.

Description scheduling in `0x822CEC00` selects bank zero and identifier `0x613 + selection`, or `0xA43` for the disabled row. Widget callback `0x821E9C30` carries those fields to provider calls at `0x823AA36C` and `0x823AA6C0`. The latter path returns without drawing when the provider pointer is null. The final renderer beyond the VMX-truncated region remains untraced, so this establishes description lookup rather than exact visible row captions or a markup contract. Main-menu speech remains unfinished.

## Verified startup and menu consumers

The subsequent original-instruction and private artwork investigation under [issue 22](https://github.com/RealAmethyst/bb-recomp/issues/22) resolves the caption and renderer gaps above. Evidence collections `menu-speech-followup-20261014` and `startup-speech-20261014` retain original instruction exports, bounded resource hashes, UV mappings and inspected images. No reverse-engineering annotations were added. This establishes static sources and integration points, not listening or gameplay acceptance.

### Main-menu captions, locale and selection

Setup `0x822CF058` requests base resource `0x8E3` through the loader at menu `0xC4`. Loader constructor `0x821EB110` installs table `0x82003F2C`; request consumer `0x821EB1A0` loads shared state from `0x829CDD04`, adds selector `0xE38` to the base, and stores the actual requested resource ID as a halfword at loader `0x20`. IDs `0x8E4..0x8EA` identify the seven accepted language resources. The integration reads that actual request rather than sampling a separate language setting.

Caption constructor `0x822D0168` stores the row index at widget `0x108`. Callback `0x822D0268` assigns texture-part overrides `33 + row` and `38 + row` through `0x821CE218`. Renderer `0x823C8ED0` stores resource table-two entries at renderer `0xD8`; primitive consumer `0x823D3EE8` resolves override pairs and consumes the selected 20-byte entry's atlas index and four UV floats. All 70 normal/selected mappings across seven resources reach the inspected caption rectangles. The exact artwork transcriptions live in central mod localization resources; no game images or binary resources are included in source. No displayed main-menu heading was found, so none is supplied by speech.

The same setup calls selector slot `0x4C → 0x821DF130` with lower zero and exclusive upper five. This sets selector `0x28` to its embedded object at `0x40`, initializes nested current/lower/upper/valid fields at `0x4/0x8/0xC/0x18`, then restores the remembered selection through `0x821D7628 → 0x821D8690`. Initial selection need not be zero. Current-selection consumer `0x821D7610 → 0x821D86D8` follows that actual pointer; table identities are `0x82003900` and `0x820038DC`. The disabled fourth row remains navigable and retains its normal caption and position.

### Description and hint rendering

The final description bridge is now traced past the VMX decompiler gap. `0x823AA670` moves the provider's UTF-16 result into `r4` at `0x823AA6C4`, rejects null, and preserves it through the actual renderer call `0x823A9A38` at `0x823AA758`. A scoped observer of the existing provider call can copy its actual result without an extra guest call or an inferred bank-size field. All 42 strings for `0x613..0x617` and `0xA43` across the accepted resources contain ordinary localized text without dynamic placeholders or controller glyphs.

The main builder also creates the hints widget through `0x821E8CE8`. Its registered record is the object pointed to by global `0x829CBC68`, plus `0x94`. Callback `0x821EA650` follows owner `0xC` to its record vector (`0xC/0x10`), selects the record using widget halfword `0xA`, and loads its current text ID from record `0x30`. It draws through `0x823A9908` at `0x821EAC2C`, or `0x823AA9B0` on the alternate animation path. Actual IDs are `0x9D6` and `0x9EC`; they contain the displayed confirmation/back hints. All 14 localized variants contain only ordinary words and the verified A/B glyphs. Capture their actual rendering choice; do not reconstruct hints from controller bindings or pending state.

### Title and logo screens

Title constructor `0x822DA190` installs table `0x8201244C`; handler `0x822DA208` reaches active state three after animation byte `0x28` becomes two. Child system dialogs preserve this native lifetime. Teardown state five destroys the widgets and publishes its result through `0x822CE988 → 0x822CE970 → 0x823B87A8`. The title's resource animation chain `9 → 2 → 6 → UV11` reaches the inspected title-logo rectangle. All seven localized title resources (`0x8DC..0x8E2`) have identical hashes.

Title callback `0x822DADA0` renders bank-zero ID `0x9E5` through `0x823A9908` at `0x822DAF14`. The latter calls the provider, then placeholder expansion `0x82395428` and renderer `0x823A9528`. All seven selected prompts contain no dynamic placeholders and exactly the verified A glyph. This prompt is not an indexed list item.

The game's custom font lookup `0x823967A0` uses 16-byte glyph records. Renderer `0x82394F70` consumes their atlas coordinates and dimensions. Resource callback `0x823955C8` binds the custom atlas and table after the ordinary font styles; the inspected matching font table and pixels establish `U+2460` as A and `U+2461` as B. These substitutions apply to verified game-rendered text, while literal host-dialog and entered text remain literal. This is not a general markup parser.

The four startup logo captions were inspected directly in the indexed movie pixels. Manager table `0x8203608C` and dispatcher `0x823AF9A0` select setup `0x823AFB50`, active state-three update `0x823AFCB8`, and cleanup `0x823AFD98`. Within the active manager update, `0x82190C48` reaches video submission `0x8249A180`. Its zero return follows texture upload and draw call `0x8264C4B0`; the only alternative is `0x8000000A` for occupied buffers. This permits announcement on actual native draw submission rather than request or setup success. It still does not prove physical scanout. Cleanup establishes closure when manager `0x34` and state `0x4` are both zero; destructor `0x823AFF38` provides the shutdown close path.

### Native option messages

The original message service `0x823B75F8` passes title, body, button array, count and selected index through `0x823BC590` to `XamShowMessageBoxUI` at `0x828EC5C4`. Startup/title paths include actual localized messages. The runtime observer reports the displayed ImGui title/body, actual focused button, real count and native closure. The runtime's distinct `XamShowMessageBoxUIEx` remains a stub, so no displayed speech event is invented for it. Its conditional startup path and reachability limits remain documented above.

## Semantic events and recovered behavior

The implementation uses the pinned recompiler's `midasm_hook` support at original state transitions and native record loads. [game_events.toml](../config/game_events.toml) is the reproducible 48-site configuration. It observes register values after the original instruction without replacing instructions or changing registers. Function wrappers retain constructor/destructor ownership and nested rendering scopes; they do not poll menu or title state on every update.

These addresses apply only to the executable identity in [the baseline record](executable-baseline.md). Revalidate the original instructions and consumers before applying the configuration to any different input. Private collections `menu-speech-followup-20261014` and `startup-event-hooks-20261014` retain the fresh full instruction traces, dispatcher tables, register contracts and rejected alternatives. No database annotations were added.

### Main-menu transitions and navigation

- After `0x822CF970`, registers `r31/r9` carry menu owner/new state four. The animation-completion gate has passed; opening can wait for its first real selection.
- After `0x822CEC98`, `r30/r3/r9` carry menu owner/current index/final description ID. Native validity and current-versus-previous checks have already passed, including disabled-row substitution to `0xA43`. The description may still be awaiting animation/rendering; capture its corresponding actual text rather than invoking a second provider lookup.
- After `0x822CF038`, `r30/r11` carry owner/new state five. This begins exit after input disable and animation commands; it is not native destruction. Normal removal `0x823B87A8` and destructor entry `0x822CFDF8 → 0x822CEAA0` close the lifetime idempotently.

Setup validity needs an explicit correction: `0x821DF130` momentarily writes nested validity one, but its later virtual call `0x821D86E0` resets it to zero. The first active input pass `0x821DEDC0` changes it to one and sets previous selection to minus one when selector mode is two. This guarantees the initial selection-change branch without guessing that the initial selected index is zero.

Controller mutation `0x821D8750` increments/decrements nested current, then `0x823B9220` normalizes it. Main bounds are `[0,5)` with wrapping enabled, so four wraps to zero and zero to four. The normalizer returns one for in-range, two for clamping and three for wrapping, not a Boolean changed result. Disabled rows remain selectable; their confirmation does not take the exit transition. Direct setter `0x821D8690` clamps and returns whether the final value changed; it also serves restoration and pointer input, making it a weaker main-menu event boundary.

### Title activity and native lifetime

All title transition sites carry owner `r31`. Opening after `0x822DA594` stores state three from `r11`. Return after `0x822DA7D4` stores three from `r30`, shared by the state-eight and state-fourteen child-interaction branches. The three active-state exits are `0x822DA658` (`r11=6`), `0x822DA67C` (`r8=9/13`), and `0x822DA6A0` (`r10=4`, attract exit).

These inline transitions have no equivalent dedicated function boundary. They control activity independently of screen lifetime; returning resumes the prompt without repeating the title. Destructor `0x822DAD58` supplements normal removal. Main-menu state six has no traced return to four: its warning path proceeds toward closure and must not be treated as the title's child-return behavior.

### Text ownership without repeated pointer traversal

The game's bounds-checked native record loads supply description record `r29` after `0x821E9C84`, hint record `r29` after `0x821EA6A4`, and title record `r9` after `0x822DADE8`. Nested host scopes retain these tokens because the original registers are later reused. Menu setup binds the shared root's description/hint records at `0x60/0x94`; title records match a constructed owner plus `0x20`.

Only actual final-render provider calls qualify: return sites `0x823A9958` and `0x823AA6C4`, with incoming getter `r4=ID/r5=bank` and returned `r3=text`. The generated calls preserve the corresponding link register. Width measurements and composite-string assembly are excluded. Simple title/hint text uses the first renderer; descriptions and scrolling hints reach the second. Composite hints use a separate assembled-text consumer; the later save/popup integration below covers its normal and scrolling routes for those owners.

Owned, uncached text is copied with strict bounded UTF-16 decoding. Cached draws require no guest-memory reads. Runtime `TryReadVirtual` protects mapping/access lifetime across validation and copying; it does not establish game-object ownership or make concurrent game contents atomic. Event records leave the guest critical region through a bounded FIFO worker before serialization/output. Overflow or lost-event output fails closed. Synthetic tests verify these boundaries independently of speech playback.

### Movie request, drawing and closure

After `0x823AFB68`, `r3/r4` identify the manager and actual setup ID. After `0x823AFD7C`, `r31/r3` identify the manager/wrapper on the actual draw path, after replay/EOF branches. The initial `.6` adapter used a host-only update scope to associate successful `0x8249A180` submission with that manager and restore nested identity on return. It does not reread manager state.

Cleanup after `0x823AFE1C` reports owner `r31` only after clearing the wrapper and native state. Destructor `0x823AFF38` is the alternate close. The request API can overwrite an existing request; a differing setup invalidates a pending caption rather than inventing closure or announcing stale artwork. Native submission still does not prove physical scanout. Subsequent listening feedback establishes that first submission precedes readable lettering in some clips; caption-visible frame timing is a separate required correction.

### Readable logo pictures and decoder ownership

Private collection `logo-caption-timing-20261014` records original decoder instructions, visual inspection and offline packet-to-picture verification. All four supplied streams are 1280 by 720 WMV3 pictures. Their inclusive readable picture intervals are Activision 177–280, NOWPRO 48–175, Premium Agency 23–170, and Nelvana 0–210, numbered from zero. No extracted pictures or compressed game data are included in source.

Packet tags propagated through an isolated libavcodec decode establish each displayed picture's actual compressed input. Every tagged output matches an ordinary decode's picture hash; every compressed input matches a strictly reconstructed ASF media object. Activision display pictures 171/172/173 come from compressed objects 172/173/171 respectively; the readable interval begins afterward. The four readable intervals therefore also identify those exact compressed-object ordinal ranges. Encoded ASF object numbers start at one and wrap at 256, so they cannot serve as an unqualified global counter.

Native WMV output timestamps carry the latest compressed-input timestamp, which is not sufficient to identify a reordered picture. A plain produced-frame counter is also insufficient: the codec can retain or repeat pictures. The adapter instead assigns an identity to each actual compressed object and follows the native picture objects through copying, selection and queue publication.

Successful WMV initialization after `0x824E2070` supplies state `r11`; its `+0x44` is the codec. Header initialization uses the distinct state `+0x50` path. Real-object events after `0x824E17C8/0x824E189C` supply state `r31` once at the first fragment. These events number actual objects from zero within a fresh codec generation. Native picture start after `0x824F2300` snapshots the current object identity before body reads can fetch later input. Successful picture completion after `0x824F2F2C` associates that snapshot with codec `+0xE80`. Empty and one-byte-repeat paths bypass that new-picture commit.

Identity copies follow actual native picture copies, not generic metadata updates. After `0x8251B74C`, codec `r31` copies its current `+0xE80` picture into destination `r3`; after `0x825672B4`, the same source goes to destination `r28`. After `0x8251B964`, source `r11` and destination `r10` are live; after `0x824F774C`, source `r11` and destination `r8` are live. Their native metadata-copy instructions accompany the verified picture copying. Unknown sources invalidate destination identities, preventing stale reuse.

Actual YUV plane use after `0x824F5500` supplies codec `r31`, selected picture `r11`, and destination `r17`. A zero destination denotes an internal preparation/discard path and cannot arm a queue frame. Producer preparation after `0x824A9AA0` captures player `r30`, frame `r31`, output capacity `r4`, and byte-count pointer `r5`. Completion after `0x824A9AA4` requires nonnegative status and exactly one full verified YUV420 image. The value-only identity is installed before `0x824A9AC8` publishes the frame to the consumer thread.

The exact successful render event is after `0x824AA954`, with result `r3`, player `r25` and queued frame `r15`. `0x8000000A` retries the same frame; only zero qualifies. A known queued picture within the corresponding readable interval can open logo speech once. Skipped frames, decoder lookahead and render retries do not advance a speech counter. This proves native draw submission, not physical display scanout or speech-device timing.

Setup `0x823AFB68` precedes construction of the new player and decoder. A pending logo lifetime retains the generation floor established by its first setup event, rejecting old queued identities even if pointers are reused. Reset/destruction observations at `0x824E1994/0x824E19F0` invalidate the codec's picture and frame associations. A failed replacement initialization also invalidates the previous state before checking the new pointer. The normal startup path creates a fresh codec and does not subsequently reset it. Decoder constructor `0x8249FC58` sets `+0x200` to one, and normal mode `+0x1F4` is one; normal output therefore skips the selection and mode-five reset routes. The prepare callback uses status converter `0x824E15E8`, whose nonnegative outputs cannot trigger the separate `0x50000D` reset gate. Explicit seek, alternative stream selection and mode-five resets invalidate capture until a new decoder is initialized. Synthetic tests and original-source review pass; in-game listening remains unverified.

Final integration audit verifies all 48 hooks in 28 changed function bodies while preserving every original instruction and all 46,056 function identities. Native ASan/UBSan fixtures pass with 165 original forwarding calls and 451 bounded reads, including same-generation reordering, processed/buffered copies, failed replacement, lookahead, queue reuse, EOF, retries and lifetime floors. No database annotations were added. The private `exact-picture-events-and-reset.md` record owns the full original-code proof and corrects the earlier shorthand: `0x82496618` queries player metadata; it is not a play/seek operation.

### Native host-message input ownership

Fresh original-instruction evidence in private collection `popup-controller-20261014/native-input` traces title message states and their actual controller consumer. Title states 8, 12 and 14 wait on service `0x823B6980`; the active state 3 checks internal A and Start masks. No competing D-pad action occurs in those verified message-wait states. This does not establish that every game task stops during host UI.

Service `0x823B68C8` follows interface `+0x8` to its core, then `0x823B76B0 → 0x823B75F8 → 0x823BC590 → XamShowMessageBoxUI`. The core supplies an overlap at `+0x38`, choice result at `+0x60`, and two-button pointer array at `+0x68`. It becomes pending only on `IO_PENDING`. Completion `0x823B7BF8` distinguishes actual selected index 1 after the overlap completes. Notification handler `0x823B7A08` consumes `XN_SYS_UI` ID 9 into service busy/operation fields; this branch does not clear controller buffers.

The title's actual input anchor `0x829CDA70` holds table `0x82035B48`. Slot `0x20 → 0x823ACB08` gates through slot `0x78 → 0x823AD108`, which checks the mapped controller's connected byte `+0xC`. It then returns newly pressed bits at controller `+0x18`; this chain has no message-service or system-UI gate. Raw input consumer `0x82132978` maps XInput Up/Down/Left/Right to internal `0x1000/0x2000/0x4000/0x8000`, A to `0x4`, and Start to `0x800`. Its original edge calculation at `0x821330D8..0x82133104` derives newly pressed buttons from current and previous samples.

Consequently, host popup capture must own its actual modal lifetime and retain a release barrier for held digital controls. Restoring a still-held A immediately after neutral modal input can create a new game press. Notification tracking alone does not prove exclusive input ownership. The controller object's complete scheduler path remains unverified; no annotations were added.

## Save picker and native option popups

The subsequent user screenshots identify the game-rendered save picker and deletion confirmation, separate from host XAM dialogs. Private collection `save-screen-speech-20261014` retains fresh original instruction exports, resource identities, atlas inspection, layout records, and an independent integration review. No annotations were added. This evidence covers save mode zero and the native one-choice/two-choice popup families; other save modes and subclasses remain unverified.

### Save ownership, fields and activity

Constructor `0x822D38D0` installs table `0x820121DC` and stores incoming mode `r6` at owner `+0xA0`. Mode zero dispatches through `0x822D5298` to `0x822D44F0`. The embedded selector at owner `+0x20` is configured by `0x822D2F70 → 0x821DF130` with lower zero and exclusive upper one. Builder `0x822D3A50` registers one selectable card; decorative siblings are excluded from its position count.

Within that builder, date/time/name/empty constructors are respectively `0x822D6688`, `0x822D6238`, `0x822D5F48`, and `0x822D6AD8`. Each receives widget `r3`, row zero in `r5`, and the card's actual save record in `r9`; the name constructor additionally receives save data in `r10`. Their tables are respectively `0x82012224`, `0x8201221C`, `0x82012214`, and `0x8201222C`. Registering these actual constructor events binds callbacks to the owning screen without recurring widget-pointer traversal. Layer teardown `0x822D3830` and destructor `0x822D30D8` invalidate those associations. Normal task removal is `0x823B87A8`; deleting destructor `0x822D5678` reaches the same base cleanup.

Date callback `0x822D6798` formats save-record halfwords `+0x4/+0x6/+0xA` into year/month/day replacements and draws bank-zero ID `0xE75`. Time callback `0x822D6368` uses `0x821BBAB0` to convert stored milliseconds to seconds, formats hours/minutes/seconds with the native `999:59:59` cap, and draws `0xE76`. Both use the save-record occupied byte `+0x26`. Name callback `0x822D6050` draws its actual UTF-16 pointer directly; empty callback `0x822D6BD0` instead draws `0xA3F`.

The actual render chain is `0x823A9908 → 0x82395428 → 0x823A9528`. Both scratch accessor calls return the same buffer; incoming `r4` of the final draw contains the expanded text. Capturing that argument preserves localized field order, formatting and substitution without duplicating the game's date/time logic. The direct name draw reaches the same consumer. Entering state four after an operation invalidates cached fields so deleted or replaced content cannot reuse old details.

All seven save-layout resources `0x9F4..0x9FA` are byte-identical. Their actual layout places name, date and time at increasing vertical offsets 1, 31 and 61 under the same positively scaled parent. Position opcode three reaches `0x823D0468`, which consumes the signed coordinate halfwords. An empty drawn name is omitted. The inspected atlases supply no heading, so speech adds none.

The exact after-instruction state events are in `config/game_events.toml`: initial active stores at `0x822D46F0/0x822D47DC`, six return-to-active stores, and five active-input suspension stores. These change activity, not native lifetime. The shared selector event after `0x821DF0FC` supplies selector `r31` and current `r27` only after original branches reject unchanged and invalid selections. A runtime setup observer verifies the actual native bounds before emitting a list position.

### Native popup family and artwork

Factory `0x821DFEC8` creates the two-choice subtype, table `0x82003C80`; `0x821DFF80` creates the one-choice subtype, table `0x82003C90`. Shared constructor `0x821DFBE8` establishes selector `+0x18`, render record `+0x74`, bank `+0x88`, body ID `+0x8C`, and result byte `+0x92`. Two-choice setup reads the actual initial index at `+0xA0`; the deletion caller supplies index one. Accessibility preserves that native choice.

Handlers `0x821E0520/0x821E08B8` enter active state two and closing-animation state three at the four recorded instruction hooks. Shared removal `0x821DFAD8` publishes the result and removes the native task; destructor `0x821DF478` clears the singleton. A popup never closes its parent save screen. Body callback `0x821E1790` supplies the actual render record after `0x821E17D8`; matching it to owner `+0x74` establishes ownership before final expanded text capture.

Choice labels are raster artwork. Constructor `0x821CDAC0` stores the choice code at widget `+0x120`; callback `0x821CD6A8 → 0x821CC9D0` selects UV entry `code + 3`. Entry three is the one-choice label; entries four/five are the two choices. Shared resource anchor `0x829CBC68` holds derived loader table `0x82003EAC`; its actual request halfword `+0x20` ranges from `0x8CC..0x8D2` for the seven locales. The same object's `+0x4` resource supplies those exact UV entries. All seven atlases were inspected, and their verified labels reside in central mod localization resources. No nearby text-table label is substituted.

### Shared hints and verification

Existing-save hints use bank-zero `0x9F6`; deletion confirmation uses `0xA16`. The actual shared hint record is resource owner `+0x94`. Native popup activity takes precedence over parent hint ownership, including when its wording is already cached. Normal/composite final draw is `0x823A9528`; scrolling routes reach `0x823A9A38` with the complete owned string in `r4`, before glyph clipping. The custom font's actual entries and atlas establish U+2460 as A, U+2463 as X, and U+2465 as Start.

Original synthetic fixtures exercise owned versus unowned callbacks, expanded date/time text, optional names, occupied-to-empty changes, actual default choice, both popup types, cached draw reads, full scrolling hints, and parent return. A cached popup hint initially fell through to the underlying save owner; the regression reproduced this and passes after ownership is resolved before cache eligibility. Static review found the final save/popup integration consistent with the original contracts. Listening and real controller behavior remain user-controlled checks.

The provisional native integration builds and links with all 32 expected strong wrappers and their original implementations present. An independent generated-code audit verifies all 33 instruction hooks against original sites: all 46,056 function identities remain unchanged, and removing the observer calls restores every original body exactly. Registration and the 21 existing generator warnings are unchanged. The private `popup-controller-20261014/translation-save-audit` collection preserves the reproducer and hashes. These are offline integration checks, not an in-game result.

### Corrected message classifications

The disabled-confirm helper `0x8223D338` is not a popup: the constructed component's relevant slot resolves to the no-op `0x823B3550`. The separate rejection-sound path does not establish a message.

`0x822DB110` can dispatch a controller-battery warning through `0x821E72E8`. Its native widget renders bank-zero ID `0xE63`; no options list was found. This is a distinct informational warning, not an uncovered option selector. Its speech is not included in the host message-box observer.

## Native operation status messages

Issue [26](https://github.com/RealAmethyst/bb-recomp/issues/26) extends capture to the native popup subtype with no choices. Fresh read-only original evidence is retained privately in `status-message-speech-20261014`; no annotations were added. Factory `0x821E00D0 → 0x821DFDF8` supplies kind two to shared constructor `0x821DFBE8` and installs table `0x82003CB0`. The previous constructor observer rejected this kind. Its bank and body ID remain at owner `+0x88/+0x8C`, with render record `+0x74`; the traced subtype does not change them during its lifetime.

Handler `0x821E0F70` activates only after the opening animation reaches state two. The after-instruction event at `0x821E0FE4` supplies owner `r31` and state `r11`; closing event `0x821E102C` supplies owner `r31` and state `r10`. Completion follows native operation completion through `0x821C18E0 → 0x821C1408`, setting the flag consumed by `0x821DF9D8`. Removal `0x821DFB60` reaches existing task cleanup `0x823B87A8`; deleting destructor `0x821E16D0` reaches existing base cleanup `0x821DF478`. Animation, blank capture and parent focus changes do not create another lifetime.

Setup `0x821E0D78` registers the actual render record and creates the body widget through `0x821DF3F8`, table `0x82003C68`. Callback `0x821E1790` obtains record `r30` at `0x821E17D8`, then loads record `+0x18/+0x14` as ID/bank for `0x823A9908`. Provider text passes through native expansion `0x82395428`; final draw `0x823A9528` receives the expanded buffer in `r4`, with caller return address `0x823A9A24`. Capture requires this owned draw, nonempty text and active native state. Speech contains the displayed body alone, without an invented heading, choice label, hints or position.

Operation setup `0x821C12D8` selects bank-zero `0xE8A` for modes two, three, five and seven, `0xA24` for deletion mode four, and `0xE81` for the default saving message. Mode six with controller byte `+0x28` zero deliberately creates no popup. The ordinary load/check path `0x821C06D8 → 0x821C1158 → 0x821C0F98` also creates no status caption; a loading announcement must not be synthesized from that operation. Missing data can instead enter creation/save through `0x821C11E8`, which explicitly displays `0xE81`. Load/save errors remain existing interactive popup cases. These findings do not establish coverage of unrelated loading screens.

Native synthetic checks pass with strict warnings, AddressSanitizer and UndefinedBehaviorSanitizer; private collection `creator-status-integration-20261014` retains the results. A regression reproduced underlying hint capture when the no-choice modal had no artwork resource owner. Hint capture now stops while that modal is active. Original-source review found the constructor, state, final-text and cleanup integration consistent with these contracts. Verification is offline only; listening and real operation testing remain pending.
