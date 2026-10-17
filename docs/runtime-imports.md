# Executable imports and candidate runtime coverage

Inspection date: 2026-10-14. Authoritative work item: [runtime investigation, issue 2](https://github.com/RealAmethyst/BakuganBB-recomp/issues/2).

## Conclusion

The selected executable's 535 import records are accounted for without ordinal/name or thunk-pairing discrepancies against the pinned ReXGlue export tables. This establishes an import inventory, not runtime compatibility.

ReXGlue remains a candidate for evaluation. Forty-five imported names intersect explicit stub registrations at revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. Ordinary, nonstub registrations also include incomplete behavior. Static call sites exist for several important incomplete operations, but their reachability from actual gameplay and save flows remains unproven.

No game or native port was run. The subsequent [Linux SDK trial](linux-toolchain-trial.md) built successfully and identified five test failures; isolated candidate fixes now produce zero failures with four upstream skips unchanged. All proprietary inputs and decompilations remain private.

## Inventory and verification method

Target executable SHA-256: `7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b`.

- `xam.xex`: 106 import references and 106 function thunks.
- `xboxkrnl.exe`: 169 import references and 154 function thunks.
- Total: 275 import references, comprising 260 functions and 15 variables, plus 260 thunks.

The original XEX optional-header import table was parsed with extent, library-size, name-index and total-record checks. Its record addresses were joined to words and symbols read from the private Ghidra analysis image. Reference words retain their type-zero import encoding. Each reference's module and low-16-bit ordinal was checked against the separate SDK export table, including function/variable type. Every paired thunk matched its reference's ordinal and symbol name.

All 535 records were inspected; there were no mismatches. This is a cross-check against a separate implementation's table, not an authoritative console conformance test or a claim that the two projects have independent historical provenance. [XAM export table](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/export_table.inc), [kernel export table](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xboxkrnl/export_table.inc).

The loader patches the first two bytes of each of the first two thunk words in its analysis image. It does not change the original privately supplied executable. Its analysis image must therefore not be treated as an unmodified executable or used directly as a translation baseline. Reference-word ordinals, header addresses and original executable identity are separate evidence. [Loader import processing](https://github.com/zeroKilo/XEXLoaderWV/blob/d0af801aee083c86950b90c3db78b2e1c642067f/XEXLoaderWV/src/main/java/xexloaderwv/XEXHeader.java#L204).

Private evidence includes `import-header-records.tsv`, `import-records.tsv`, `verified-imports.json`, `import-validation.json`, and the read-only inspection scripts. These records are not repository content.

## Critical semantic gaps

### File and content durability

`XamContentFlush` (XAM ordinal `0x267`) returns or completes success without flushing content. `NtFlushBuffersFile` (kernel `0xDB`) reports success without validating or flushing its file handle. A POSIX file-handle implementation elsewhere calls `fsync`, but these entry points do not call it. This is an implementation gap; it is not evidence of an observed corrupt save. [Content flush](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_content.cpp#L271), [file flush](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xboxkrnl/xboxkrnl_io.cpp#L600), [POSIX file implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/core/filesystem_posix.cpp#L182).

`NtWriteFileGather` (kernel `0x100`) is an explicit `REX_EXPORT_STUB`. The macro logs a warning but does not assign a defined result to the guest return register. It must not be assumed to return a usable unsupported-operation error. [Gather-write registration](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xboxkrnl/xboxkrnl_io.cpp#L813), [stub macro](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/hook.h#L50).

### Profile persistence

`XamUserWriteProfileSettings` (XAM `0x21A`) accepts binary/content settings for user zero. Several other setting types, including strings and numeric values, log an unimplemented-type error but still reach successful completion. The actual setting types used by this game require tracing. [Profile-write implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_user.cpp#L304).

### Language selection

`XGetLanguage` (XAM `0x3CD`) uses a hardcoded NTSC-U region value and returns English. The selected game has seven populated language resource groups, but neither those filenames nor this runtime function establishes the game's actual localization path. Runtime language policy must be designed only after the consumer is traced. [Language implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_info.cpp#L185).

Follow-up now distinguishes the normal resource-selector initialization: it calls `ExGetXConfigSetting` for language and country, which the pinned runtime serves from configurable values. Thus the hardcoded `XGetLanguage` result does not establish an English-only normal game. The verified numeric mapping and request/callback chain are in [localization consumers](localization-consumers.md); the seven private bank validations and exact resource-backed title entry are now recorded there; actual display/speech behavior remains untested.

### Accessible console dialogs

Message-box and keyboard exports contain ImGui-based dialog paths. Headless message boxes automatically select the focused answer; headless keyboards complete using the default text. Device selection returns a dummy device. These are not evidence of screen-reader access, deliberate player choice or usable controller interaction. Automatic acceptance must not substitute for an accessible dialog. User-facing behavior remains a design decision. [Console UI implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/kernel/xam/xam_ui.cpp#L268).

## Static caller evidence

Read-only Ghidra references identify the following call sites. Addresses are for the exact target above, not stable integration anchors or verified runtime execution paths.

- `NtFlushBuffersFile`: thunk `0x828ECE74`, call at `0x826380F0`, containing function `0x826380E0`. Private decompilation shows the returned status is tested.
- `NtWriteFileGather`: thunk `0x828ECE64`, calls at `0x82637EEC` and `0x82637F30`, containing function `0x82637E78`. Private decompilation includes synchronous/overlapped-style branches and status handling; exact calling semantics still require an instruction-level trace.
- `XamUserWriteProfileSettings`: thunk `0x828ECAD4`, call at `0x823C3BAC`, containing function `0x823C3B70`.
- `XamShowMessageBoxUI`: thunk `0x828EC5C4`, call at `0x823BC5A4`, containing function `0x823BC590`.
- `XGetLanguage`: thunk `0x828EC6B4`, call at `0x823BC9F4`, containing function `0x823BC940`.
- `XamShowKeyboardUI`: thunk `0x828EC524`, jump at `0x823BC4F0`; no containing function resolved by the existing function inventory.
- `XamShowDeviceSelectorUI`: thunk `0x828EC5B4`, jump at `0x823BC588`; no containing function resolved by the existing function inventory.
- `XamContentFlush`: thunk `0x828EC334`; no incoming reference returned by the current database. This is not proof that it is unused.

All five requested caller decompilations completed. No manual names, comments, types or function definitions were added. Full auto-analysis remains disabled. Existing references can omit indirect or undiscovered control flow; these may be SDK wrappers rather than game-owned callers.

## Instruction-level caller follow-up

The file-flush call is reached through this static direct-call chain: `0x823DDF18` calls `0x823E5868` at `0x823DDF68`; that function calls `0x826380E0` at `0x823E5938`; the latter calls `NtFlushBuffersFile` at `0x826380F0`. The first function exits on failure of `0x823DDE80`, and bit mask `0x4000` in its stream/object field at offset `0xC` gates the flush. The wrapper preserves its incoming first argument for the import and tests negative returned status.

`0x823E5868` performs table/flag validation and calls `0x823F15E0` before passing that result to the flush wrapper. The full bodies of `0x823E5328` and `0x823F15E0` have not been traced, so the exact handle transformations remain unverified. Higher references include stream iteration/output paths, not an established game-save operation. This is evidence of a general runtime/stream path to the incomplete flush, not proof of save corruption or of save-time reachability.

The bounded existing-reference graph found no incoming references to gather-write wrapper `0x82637E78` or profile-write wrapper `0x823C3B70`. Depth limits, indirect calls and missing analysis prevent treating that result as proof of nonuse.

The system-language query belongs to a conditional startup dialog/termination path. A separate resource-backed provider result reaches the keyboard dialog's title argument. The exact instruction chains, structure offsets and limitations are recorded in [localization consumers](localization-consumers.md). The subsequent localization record establishes the normal numeric language mapping and one resource-backed caption; actual display and gameplay behavior remain untested.

No manual annotations were added. Compiler register-save helpers confuse decompiler signatures, so critical argument flow was checked against instructions rather than inferred pseudocode.

## What the stub count means

The 45 count is the intersection of the verified imported names with explicit `REX_EXPORT_STUB` invocations in the pinned kernel sources. Both ordinary exports and explicit stubs register callable names. Successful import lookup does not distinguish useful implementation from a stub. [Registration macros](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/hook.h#L71).

This is not a count of mandatory fixes or all incomplete functions. Unused SDK paths may import stubs; nonstub functions can still be incomplete, as the flush examples demonstrate. Source existence also does not prove a configuration builds or includes that registration.

## First-build follow-up

The [first-build investigation](first-build-gates.md) now owns native host compilation, plugin/file-layout requirements and isolated graphics/audio checks. The inspected startup branch exits before its conditional message dialog when the runtime's `XGetAVPack` returns 6. This narrows that particular path without establishing general startup compatibility. `XamShowMessageBoxUIEx` also has incomplete behavior despite an ordinary export registration; explicit stub counts remain an undercount of semantic gaps.

## Next evidence required

Trace the callers above back to actual game operations, preserving the distinction between SDK wrappers and reached gameplay. Prioritize save completion/error propagation, console dialogs and language selection. Evaluate those paths with original synthetic runtime tests before choosing the dependency or implementing game-specific hooks.

The [Linux trial](linux-toolchain-trial.md) establishes a successful SDK build and 1,458 passing synthetic PowerPC cases. Two isolated candidate patches resolve its five original failures; four upstream skips remain. Target control-flow coverage and graphics/audio feasibility remain unresolved. No runtime has been selected by this report.
