# First private native build: evidence and testing gates

Inspection date: 2026-10-14. Owning investigation: [issue 4](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/4), under [epic 1](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/1).

## Result and boundaries

The first private native x86-64 Linux executable now compiles and links. This clears a build gate, not a boot or playability gate. It has never been launched. Rendering, input, valid audio playback, saves and accessibility remain untested.

The original executable identity is recorded in [executable-baseline.md](executable-baseline.md). All game inputs, translated C++, binaries and disassembly stay outside the publishable workspace. This record contains measurements and open-source tool references only. No assets, acquisition guidance, commits, pushes or dependency adoption are included.

## Control-flow correction

Pinned ReXGlue revision: `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. The first ordinary codegen attempt against the original XEX stopped with **110 unresolved branch errors**. The loader-patched Ghidra analysis image was not used for translation.

In `src/codegen/phase_gapfill.cpp`, an unconditional relative branch splits a linear gap segment only when its target is already a registered callable. A backward exit to an earlier unregistered target can absorb a following leaf into that segment; discovery stops at the preceding exit and misses the leaf. If the segment begins inside an already registered owner, it may instead be skipped wholesale. Later branches into the missing leaf fail validation.

The [candidate patch](../patches/rexglue/gapfill-backward-exit.patch) also splits when the branch target precedes the current linear segment, while retaining the own-entry exception. This is a partitioning heuristic, not proof that every backward exit is a semantic tail call. No game addresses, forced validation, silent no-ops or error suppression were added. [Local candidate verification, issue 9](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/9).

Verification:

- Ten original synthetic cases: **baseline 8/10; candidate 10/10**. Two defects fail before/pass after. Eight controls preserve existing owners, own-entry recursion, interior backward loops, forward branch behavior, call fallthrough, conditional fallthrough and ordinary leaves.
- Ownership checks use actual blocks and instruction lists, not just a function's numeric extent. The [original fixture](../patches/rexglue/gapfill-regressions.cpp) contains no game input.
- The existing SDK unit binary, relinked with the candidate phase, passes with four unchanged upstream BitStream skips. The full PowerPC suite was not rerun for this analysis-only change; its earlier result is in [the SDK trial](linux-toolchain-trial.md).
- Corrected private analysis ends with **46,135 functions and zero validation errors**. All 110 formerly unresolved targets now belong to actual blocks; all 228 watched addresses have block coverage. Ordinary codegen exits successfully without force.
- The two previously missing Ghidra switch listings are covered by the translator already: handler `0x821BE538`, table `0x821BE56C`, 27 targets; handler `0x822D79C0`, table `0x822D7A00`, 12 targets. The verified request arms and calls are present. These cases did not need game-specific configuration.

The candidate phase was compiled separately and linked before the pinned archive. Neither SDK source checkout was modified during this experiment. The patch applies cleanly to the pinned baseline. Inline data, arbitrary function boundaries, exceptions and whole-game behavior remain outside this proof.

## Native compilation and linking

Codegen emitted **149 C++ units**, totaling 157,089,012 bytes: 147 translated partitions plus initialization and registration. All units and their generated precompiled header compiled successfully in 150 Ninja actions. No compiler errors or warnings occurred.

A private host then compiled and linked the actual ReXApp and SDL entrypoint, generated PPCImageConfig and all generated objects. It explicitly selects the Xenos GPU plugin. The matching candidate runtime and plugin were staged beside the executable. This consumer build uses the previously verified clock/depfile runtime candidate; it does not rebuild or install the SDK.

Initial configuration: Clang C++23, x86-64-v2/SSE4.1, large code model, strict floating point, no strict aliasing, O0 with line-table debug information. Release runtime macros remain enabled. This establishes compile/link viability, not optimized performance. One unused CMake package-registry variable warning occurred during configuration; no package-registry operation was performed.

The linked x86-64 ELF is **162,898,768 bytes**, SHA-256 `c8ccb229a4c48a7a3a956e29d596e1c2e10fc21e7a4797d9e4ca9a0c6d7251a2`. Readelf confirms the runtime dependency and executable `$ORIGIN` RUNPATH. It was inspected, never executed. The staged plugin still has a development-tree RUNPATH; this is not a finished portable package.

## Instruction warnings and traps

Generation reported 20 unexpected float16_4 pack warnings. Independent decoding of the original input found the same mode at every site: `vpkd3d128`, type 5, mask 3, shift 0, with distinct source/destination registers. All destination halfword indices are within bounds.

The [Xenia instruction emitter](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/ppc/ppc_emit_altivec.cc#L2054) uses identical word insertion for mask 3/shift 0 and mask 2/shift 0. The current SDK's four-halfword insertion matches that layout for these nonaliasing sites. Thus these warnings alone are overconservative, not demonstrated target corruption. An original one-instruction fixture generated by the unmodified SDK also passes for this exact nonaliasing mode. This comparison does not establish every numeric conversion edge case.

A strict inventory of all 68 emitted pack occurrences found 44 type-5/mask-2/shift-2, 20 type-5/mask-3/shift-0, and two each of type-0/mask-1/shift-3 and type-3/mask-1/shift-3. None aliases source and destination. No emitted type-5 instruction uses mask 1 or shift 3. Generic defects in those combinations must not be presented as reached target failures.

A separate original 24-case experiment exercises masks 1/2/3, shifts 0–3, and aliased/distinct registers through the actual SDK generator and sanitized native execution. Results: **8 pass, 10 value/preservation failures, 6 bounds violations stopped by UBSan**. This confirms ignored mask-1 preservation, source clobbering under aliasing and shift-3 array overflow. Both distinct-register type-5 forms emitted by this target pass their ordinary-value controls. [Issue 11](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/11) records these generic SDK defects; no fix was implemented. NaN, infinity, signed zero, subnormals and general rounding boundaries remain untested.

All 216 emitted builtin traps are switch-range guards. There are no generated null-target traps, debugtraps or REX_UNIMPLEMENTED markers in the 149 C++ units. These observations do not prove guards are unreachable or establish all instruction semantics. The unusually large function partition warning also did not prevent compilation.

## Host startup and private file layout

The default SDK host has no selected graphics plugin; a port must select and stage Xenos deliberately. Missing graphics initialization is fatal. Audio and input setup failures can instead disable those subsystems with warnings, so a running process would not prove either works.

The stock host mounts a directory through HostPathDevice. A raw image is not a drop-in value for that directory option. A complete verified private runtime file layout remains to be prepared. Writable user/cache paths must be explicit before setup; late-loaded configuration must not be assumed to move already established paths. This is runtime integration guidance, not asset acquisition guidance.

A separate original synthetic host was compiled and linked first. A copy-only packaging experiment gave its executable, runtime and Xenos plugin `$ORIGIN` RUNPATHs and checked direct shared-library filenames statically. That does not prove loader ABI compatibility or portability, and it did not modify the SDK binaries. The real generated host package has not yet incorporated that packaging step.

The conditional startup language/message path is narrowed by actual source: `XGetAVPack` returns 6 in the pinned runtime, and the inspected startup function returns before that dialog for this value. This makes that particular dialog an unlikely first blocker under the present configuration. It does not establish general dialog accessibility or all startup behavior. [Runtime import evidence](runtime-imports.md) owns the remaining semantic gaps.

## Vulkan layout defect

An original CPU-only probe generated eight SDK-authored depth helper shaders, without guest microcode. **120 validator runs** covered valid Vulkan environments and layout options: **24 scalar-layout cases passed; 96 non-scalar cases failed**, including relaxed layout and uniform-buffer-standard layout.

`SystemConstants.user_clip_planes` begins at byte 28. The Uniform Block uses host offsets directly, despite a std140 comment. The device code enables scalarBlockLayout only when available on the Vulkan 1.2 path, without requiring it for accepted devices; its earlier-version path does not enable the extension. Default render-target initialization uses these helpers. This is an undeclared portable-device requirement, **not an observed failure on the development GPU**. No Vulkan instance or physical device was queried.

[Issue 10](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/10) tracks the choice between consistently requiring/enabling scalar layout and changing the complete uniform ABI. An ABI change must account for host offsets and shader-cache compatibility. No policy or fix has been adopted.

Pinned source: `include/rex/graphics/pipeline/shader/spirv_translator.h:204`; `src/graphics/pipeline/shader/spirv_translator.cpp:259-329`; `src/ui/vulkan/vulkan_device.cpp:53-59,172-244,303-341,648-652`; `src/graphics/vulkan/pipeline_cache.cpp:287-340`.

Actual target shaders, GPU commands, resources, synchronization, memory exports/readbacks, first-frame accuracy and presentation remain untested. Fragment interlock is optional with a fallback; it is not a universal device prerequisite. The SDK already has an integrated Xenos-to-SPIR-V translator, so a separate XenosRecomp integration is not required just to evaluate this runtime.

## Audio and movie evidence

The actual built FFmpeg registry contains five audio decoders, including XMAFRAMES, and zero video decoders. All eight XMA initialization combinations pass: mono/stereo at 24, 32, 44.1 and 48 kHz. All eight reject an original two-byte invalid packet with the expected invalid-data error.

This checks decoder availability, initialization and malformed-input rejection only. No valid audio stream, timing, looping, flush/drain, output device or speech was tested. SDL audio initialization was not called.

Thirteen privately inventoried `.pam` resources do not identify a codec by extension alone. The target may contain a translated decoder. Absence of host video codecs is not evidence that movie support is definitely missing; the consumer requires tracing.

Pinned source: `thirdparty/ffmpeg-overlay/codec_list.c`; `src/audio/xma_context.cpp:69,471`; `include/rex/audio/xma/context.h:173`; `thirdparty/FFmpeg/libavcodec/wmaprodec.c:2059`.

## Isolation and reproducibility

Executed analysis and synthetic probes used Bubblewrap with isolated namespaces, private devices/runtime/temp directories and a cleared environment. They had no host desktop, accessibility bus, audio output or GPU devices. Generated-source compilation had read-only inputs and two workers. No game code, host launcher or native port was run. No Ghidra annotations were added.

Private evidence collections are named `first-build-gates-20261014`, `startup-runtime-gates-20261014` and `graphics-audio-gates-20261014`. They preserve exact commands, tool/source identities, logs, numeric analysis snapshots, original fixture outputs, build manifests and hashes. Relevant files include candidate `phase-output/Validate.json`, `codegen-candidate-exit.txt`, `unit-results-resolved.xml`, gap-fill `baseline.json`/`candidate.json`, generated-build `REPORT.md`/`elf-and-artifact-metadata.json`, shader validation matrices and codec probe results. Personal paths and proprietary contents remain private.

## Test readiness

Follow-up: [the diagnostic-launch preparation](diagnostic-launch.md) records the verified private file layout, scalar-layout contract fix, valid synthetic XMA decoding and startup diagnostics. This report preserves the original link-trial evidence below; subsequent preparation is not proof of a successful game launch.

The build gate is cleared. The next bounded test needs a verified private file layout, a reproducible host package with explicit writable paths, a decision/check for required Vulkan features, and resolution or qualification of instruction semantics. Startup failures must be diagnosable without relying on vision. Running the host starts the game and remains a user-controlled action.

A useful accessible interaction additionally needs verified native screen/focus events, localized captions, deliberate controller interaction and a speech adapter. Existing headless auto-accept behavior is not an accessibility implementation. Saving/profile behavior and complete gameplay are later gates, not prerequisites for every diagnostic boot, but neither can be claimed from a successful boot. GitHub Issues owns priorities and status; this document owns the measurements.
