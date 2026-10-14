# Bakugan Battle Brawlers: recompilation feasibility research

Research date: 2026-10-14.

Follow-up: [Executable baseline and localization inventory](executable-baseline.md) records the subsequent private image/header inspection. [Import and runtime coverage](runtime-imports.md) records the later identity checks and concrete candidate-runtime gaps. Statements below about the target not yet being inspected describe the initial upstream research stage.

Latest measured evidence: the [Linux SDK trial](linux-toolchain-trial.md) built the pinned candidate and identified five failing tests. Two isolated fixes now yield 1,671 passes, zero failures and four unchanged upstream skips, including all 1,458 synthetic PowerPC cases passing. [Localization consumers](localization-consumers.md) connects language/country configuration to seven privately validated text resources and a keyboard-title entry. Recovered switch arms missing from the initial analysis listing provide a concrete control-flow case for the translator trial. The subsequent [first-build investigation](first-build-gates.md) completed private translation, compiled all 149 generated units and linked a native Linux executable after correcting a gap-fill defect. Subsequent user tests now establish title/main-menu input and music, with smooth opening playback in diagnostic-20261014.4. Story reaches an unresponsive character creator. Current runtime measurements are in [timing contracts](timing-contracts.md), and the remaining blocker is recorded in [Story entry](story-entry-freeze.md). Complete gameplay and game-content speech remain unverified.

## Conclusion and evidence limits

A native x86-64 Linux port using static CPU recompilation and Vulkan is a technically credible direction. Existing ports demonstrate the architecture. They do not establish that Bakugan Battle Brawlers can already be translated correctly, or that its runtime, rendering, audio and accessibility requirements are understood.

The recommended next milestone is a bounded feasibility investigation, followed by a small, verified execution milestone. Do not select a permanent runtime or promise a completion date before examining the actual target executable.

ReXGlue deserves the first integrated-runtime evaluation. XenonRecomp with a game-specific runtime/rendering layer remains an alternative. This is an engineering recommendation for investigation, not an adopted dependency or a claim of compatibility.

Evidence in this document has four levels:

- **Source verified:** observed in the linked upstream source or build files at the stated revision.
- **Upstream documented:** described by maintainers; not independently tested here.
- **Compatibility report:** another user's observation; not reproduced here and not a proven diagnosis.
- **Proposed:** a project recommendation or experiment, not an implemented result.

No Bakugan executable was analyzed during this review. No game, emulator, recompilation build or live accessibility integration was run. No game assets or report attachments were downloaded. Runtime performance, full playability and accessibility remain unverified.

## Project constraints and tracking

The target is the Xbox 360 version of the 2009 game. Linux, x86-64, Vulkan, faithful gameplay and complete accessibility are primary requirements. Higher resolutions, altered frame rates and other enhancements follow a correct baseline.

Game assets, proprietary executable content, decompilation dumps and acquisition guidance must never enter source, documentation, public tests, CI artifacts or releases. Generated files require the same scrutiny as their inputs. This document contains technical source references only; it does not describe obtaining game content.

GitHub Issues is the authoritative tracker in the private [Battle Brawlers repository](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp), created with owner approval on 2026-10-14. The [research epic](https://github.com/RealAmethyst/BakuganBattleBrawlersRecomp/issues/1) owns the investigations. The research gates below describe acceptance scope, not a second Markdown ticket backlog. The repository for a different Bakugan game is unrelated.

## What native static recompilation means here

The CPU translator converts PowerPC instructions into C++, which a host compiler turns into native machine code. The result still needs implementations of the console services the game expects: memory, threads, synchronization, files, input, audio and graphics.

A native CPU executable can still use a GPU compatibility layer. ReXGlue's graphics build explicitly defines a Xenia-derived Xenos GPU plugin with Vulkan and D3D12 implementations. Conversely, XenosRecomp describes Unleashed Recompiled as using a renderer translation layer. These are different architectural choices, even though both can execute statically recompiled CPU code and present through Vulkan. [ReXGlue graphics sources](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/graphics/CMakeLists.txt), [XenosRecomp architecture](https://github.com/hedge-dev/XenosRecomp/blob/990d03b28a27b50277ee5d8d942e1c5f873869d1/README.md).

The deliverable must run independently of a traditional Xbox 360 emulator application. That does not automatically prohibit implementing console behavior inside the port. Document the chosen boundary honestly; do not describe a GPU compatibility implementation as a wholly rewritten game renderer.

Static recompilation is also different from recovering the original source. Generated C++ retains machine-level state and control flow. Maintainable improvements should use verified integration points and separate native components rather than hand-editing large generated files.

## XenonRecomp: CPU translation candidate

Reviewed revision: `ddd128bcca99fe8bfbb99bea583c972351fa6ace`.

Upstream documents that XenonRecomp supplies CPU translation, **not a runtime**. Its output carries PowerPC register state and a guest memory base. Loads and stores account for endianness, while vector instructions have additional ordering and floating-point considerations.

Important documented gaps include unsupported instruction cases, unimplemented MMIO and unsupported exceptions. Function-boundary and jump-table analysis can need game-specific corrections. These are research questions for Bakugan, not permission to skip instructions or assume unsupported paths are unused.

Function overrides and instruction-level hooks provide potential integration points for native behavior and accessibility. Optimizations that rely on ABI assumptions should follow a correct baseline. [XenonRecomp implementation and limitations](https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/README.md).

Proposed acceptance evidence: an instruction/control-flow inventory for the target, reproducible function discovery, explicit unresolved targets, and synthetic tests for every translator change needed by the game. A generated C++ file or successful link is insufficient.

## XenosRecomp: shader translation candidate

Reviewed revision: `990d03b28a27b50277ee5d8d942e1c5f873869d1`.

XenosRecomp converts Xbox 360 shaders to HLSL, which DXC can compile to SPIR-V for Vulkan. It does not supply the full rendering backend.

Its documentation identifies Unleashed-specific assumptions about shader containers, reflection, vertex locations and instancing. Dynamic register indexing and integer constants are among the unsupported features. Complex shader control flow also needs verification. The shader-cache mode can embed translated results in generated C++; that does not make those results suitable for this project's public repository. [Shader translator documentation](https://github.com/hedge-dev/XenosRecomp/blob/990d03b28a27b50277ee5d8d942e1c5f873869d1/README.md).

Proposed acceptance evidence: a private inventory of actual shader features and containers, plus original synthetic public fixtures for needed translator behavior. Inventory actual vertex formats, resource bindings and render passes before designing a renderer around another game's assumptions.

## ReXGlue: integrated runtime candidate

Reviewed revision: `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`.

### Execution and subsystem boundaries

The SDK combines code generation with a Xenia-derived runtime. The reviewed dispatcher maps guest function addresses to compiled host functions. Missing entries do not trigger a CPU JIT fallback in that dispatch path. Module registration, callbacks and dynamically resolved targets still need to be covered by generated or native implementations. [Dispatcher interface](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/system/function_dispatcher.h), [Dispatcher implementation](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/system/function_dispatcher.cpp).

The runtime architecture documents memory, kernel objects, virtual filesystems, input, audio and graphics as distinct subsystems. It is explicitly experimental and not a turnkey game converter. [Runtime architecture](https://github.com/rexglue/rexglue-sdk/wiki/Runtime-Architecture-Overview), [SDK status](https://github.com/rexglue/rexglue-sdk/wiki).

The GPU plugin contains command processing, texture/render-target caches and a SPIR-V translator, including a memory-export translation source. A source file's presence does not establish correct support for every shader case. The plugin ABI checks versions and loads a backend beside the executable. [Graphics build](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/graphics/CMakeLists.txt), [GPU plugin ABI](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/system/gpu_plugin.h).

Audio sources include XMA processing, SDL output and FFmpeg codec dependencies. Linux input uses an SDL controller backend. These are useful starting components, not proof that Bakugan's audio or controls work. [Audio build](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/audio/CMakeLists.txt), [Input build](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/input/CMakeLists.txt).

### Linux build and verification

Root CMake requires CMake 3.25, Clang, C++23 and a 64-bit target; its non-Apple Clang minimum check is 18. Vulkan defaults on for non-Windows platforms. The reviewed Linux release workflow uses Clang 20 and an Ubuntu 22.04 container. The latter is a more useful starting toolchain reference than treating the minimum compiler check as a validated configuration. [Root build](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/CMakeLists.txt), [Platform workflow](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/.github/workflows/_build-platform.yaml).

The inspected release workflow builds and packages the SDK but contains no test execution step. The existence of Linux CI configuration is evidence of a supported build path, not a locally verified successful build or proof of game correctness.

Tests are opt-in in root CMake. The suite includes core/runtime unit tests and a PowerPC assembly-to-generated-C++ pipeline using Catch2. Public validation can therefore begin without game content. Coverage still needs review: the presence of an instruction suite does not prove every instruction variant or game path is covered. [Unit test targets](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/tests/unit/CMakeLists.txt), [Instruction test pipeline](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/tests/ppc/CMakeLists.txt).

Documentation drift is visible: the wiki home describes v0.3.x while reviewed root CMake declares a 0.10.0 version floor. Pin source revisions and verify interfaces in code before implementing against wiki descriptions.

### Evaluation recommendation

Investigate ReXGlue first because it already has boundaries and implementations for several services a raw translator does not provide. Measure that potential saving against GPU correctness, unsupported imports, instruction semantics, Linux integration and maintenance churn. Do not assume that inheriting Xenia-derived components inherits Xenia's exact compatibility or fixes.

## Lessons from three existing ports

### Unleashed Recompiled

Reviewed revision: `cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c`.

The host build separates runtime, platform code, game integration and generated translation. Rendering source creates Vulkan interfaces while also containing substantial game-specific integration and pipeline handling. This demonstrates why successful shader translation alone is not a port. [Host build](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/CMakeLists.txt), [Rendering integration](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/gpu/video.cpp).

Transfer the modular architecture and evidence-driven fixes. Do not transfer its addresses, shader assumptions, game state layouts or executable-specific patches to Bakugan.

### Marathon Recompiled

Reviewed revision: `368d210d0cb278163cd74d02342ce047d1e85b22`.

The host build has separate audio, runtime, rendering, input and game integration components. Its XMA decoder retains attribution to Xenia-derived work. It provides another example of the infrastructure surrounding statically translated CPU code. [Host build](https://github.com/sonicnext-dev/MarathonRecomp/blob/368d210d0cb278163cd74d02342ce047d1e85b22/MarathonRecomp/CMakeLists.txt), [Audio decoder](https://github.com/sonicnext-dev/MarathonRecomp/blob/368d210d0cb278163cd74d02342ce047d1e85b22/MarathonRecomp/apu/xma_decoder.cpp).

Its contribution policy prohibits AI-generated contributions, including issue reports and communication with maintainers. No such contribution or communication is part of this research. [Contribution policy](https://github.com/sonicnext-dev/MarathonRecomp/blob/368d210d0cb278163cd74d02342ce047d1e85b22/CONTRIBUTING.md).

### Zelda64Recomp

Reviewed revision: `b65c482ed672258eb684ab94a4e8c8aa662615ee`.

The build separates recompiled functions, native patches, runtime and rendering. Application startup supplies graphics, audio, input and threading callbacks. This is useful architectural precedent for clear interfaces and targeted patches. Its N64-specific implementation is not an Xbox 360 runtime candidate. [Build architecture](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/b65c482ed672258eb684ab94a4e8c8aa662615ee/CMakeLists.txt), [Application integration](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/b65c482ed672258eb684ab94a4e8c8aa662615ee/src/main/main.cpp).

Its contribution policy prohibits AI-generated code contributions. [Contribution policy](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/b65c482ed672258eb684ab94a4e8c8aa662615ee/CONTRIBUTING.md).

## Bakugan-specific compatibility evidence

The Xenia Canary tracker identifies Battle Brawlers with title ID `41560857`. This is a useful cross-reference, not verification of the exact executable version intended for this port. The issue has a gameplay label, not evidence of a completed campaign. [Bakugan compatibility report](https://github.com/xenia-canary/game-compatibility/issues/320).

The opening report and subsequent comments describe rendering and music problems. Reports from 2025 include loading hangs or lost audio; rendering differences involving readback, resolution scaling and Vulkan; and a severe final-battle visibility problem. An April 2026 comment asks whether crashes were fixed. These observations come from different configurations and dates and have not been reproduced here.

Investigation implications, rather than established diagnoses:

- Validate render-target readback and synchronization before changing render scale.
- Include music continuity and scene transitions in audio acceptance evidence.
- Cover cutscenes and late-game scenes, not just an early arena.
- Treat Vulkan correctness as an explicit requirement; success with D3D12 would not satisfy the primary platform goal.
- Record the exact build, backend and configuration for each future observation.

Do not transplant emulator configuration workarounds as native fixes. Locate the actual cause in the chosen runtime and target game. The older [Xenia issue](https://github.com/xenia-project/game-compatibility/issues/310) provides historical context only.

## Architecture alternatives and decision criteria

### Integrated runtime with a Xenos compatibility backend

Proposed candidate: ReXGlue CPU translation, runtime and Vulkan GPU plugin.

Potential advantage: reuse of existing kernel, memory, graphics and audio implementations. Main uncertainty: the actual behavior and completeness needed by Bakugan, including interactions between CPU execution and GPU readback. Native accessibility integration still needs verified game semantics.

Select this approach only after a reproducible Linux build, synthetic validation and a documented match between the game's required subsystems and the runtime. Keep game-specific fixes outside the SDK where practical; upstream-quality shared fixes need their own tests and policy review.

### Game-specific runtime and rendering translation

Proposed candidate: XenonRecomp, a verified runtime foundation, and a dedicated Vulkan renderer with suitable shader translation.

Potential advantage: direct control over the game's render integration and future enhancements. Main cost: identifying the actual graphics API boundary, resource formats, synchronization and platform calls. Reusing another port's renderer wholesale is not justified without that evidence.

Select this approach only if reverse engineering exposes a tractable boundary and the required behavior can be implemented and tested incrementally. A hybrid is possible, but every additional boundary needs a clear owner and a correctness test.

### Criteria shared by both alternatives

- Accurate CPU execution, floating-point behavior and indirect control flow.
- Vulkan correctness at the baseline resolution and timing.
- Required runtime, audio, file and save behavior.
- Verified semantic hooks for accessibility, independent of draw calls.
- Reproducible builds, diagnosable failures and practical maintenance cost.
- A publication model that satisfies the strict content boundary.

Do not estimate performance from the word native. Profile measured bottlenecks after correctness; changing simulation timing or frame caps prematurely can obscure defects.

## Accessibility architecture and research

Proposed separation: verified game state and localized text feed semantic events; an accessibility layer applies announcement and interaction rules; platform adapters deliver speech and accessible UI. Renderer availability must not be the source of truth for focus, menu lifetime or selected items.

Standard desktop UI accessibility and in-game speech are separate needs. Qt documents platform accessibility support including AT-SPI on Linux. Speech Dispatcher documents an application-to-synthesis interface. These are candidates for different layers, not a choice of toolkit or a claim that direct speech supplies a complete accessibility tree. [Qt accessibility](https://doc.qt.io/qt-6/accessible.html), [Speech Dispatcher architecture](https://github.com/brailcom/speechd/blob/master/README.overview.md).

Research must establish:

- Accessible setup, validation failures, settings, controller configuration and recovery.
- Actual menu/subscreen lifetimes, displayed titles, localized text consumers and navigable lists.
- Every information channel and interaction needed to finish gameplay without vision.
- Timing, interruption and queued speech behavior under loading and rapid interaction.
- Saving, loading, progression and error recovery without sighted assistance.
- Appropriate output abstraction for Linux first and later platforms.

Gameplay cue design, navigation assistance, bindings and ambiguous announcement behavior remain accessibility-design decisions. Do not infer those designs from unrelated mods. Apply the established speech rules where applicable, and verify the game's real captions and events before speaking them.

Synthetic event sequences can validate announcement ordering offline. A successful API call does not prove speech is usable. Final listening and gameplay confirmation belongs to the accessibility designer. No research test may reconfigure or interfere with the running desktop's accessibility services.

## Feasibility research gates

These are proposed acceptance gates for future GitHub research issues. Each investigation should end with evidence, a decision and remaining uncertainties; a spike may finish while concluding that an approach is unsuitable.

### Target identity and architecture

Record the exact privately supplied executable's hash, version, entry point, sections, imports and module requirements. Determine architecture facts from the target, including middleware only when identifiable. Keep binary content and decompilation outside publishable materials.

Completion evidence: a reproducible identity check, an import/module inventory, and explicit unknowns. An engine name inferred from another platform's version is not enough.

### CPU and control-flow coverage

Evaluate function boundaries, switch tables, indirect calls, exceptions, unusual instructions, dynamically loaded modules and any code generation or self-modification. Determine whether all executable paths can be represented by the static toolchain.

Completion evidence: analysis coverage, unresolved targets and instruction cases, with each claimed fix backed by original synthetic tests. Never convert an unsupported operation into a silent no-op to reach the next screen.

### Runtime and persistence

Map required imports to implementations. Classify each as verified, incomplete or missing. Investigate threading, synchronization, clocks, memory mapping, file semantics, profiles and save behavior according to actual calls.

Completion evidence: documented semantics and minimal tests for required behavior. A stub returning success is not implementation evidence. Save tests must preserve existing data and include failed/interrupted writes and recovery.

### Graphics and audio

Inventory shader features, resource formats, render targets, readback, synchronization and timing. Identify audio formats, decoder calls and stream lifecycle. Use these findings to compare runtime alternatives.

Completion evidence: a written architecture decision, original synthetic correctness cases, and a private validation plan for observed game paths. Baseline image/audio accuracy precedes resolution and frame-rate enhancements.

### First execution and accessible interaction

Choose a small execution target after analysis exposes the startup dependencies. Record every unsupported operation encountered rather than hiding it. Progress from deterministic native execution to rendering/input/audio and one real accessible interaction.

Completion evidence: reproducible build identity and logs, correct behavior for the scoped interaction, and listening confirmation. The game is launched only by the project owner unless explicit permission is given for that launch.

### Complete playability

Expand coverage across all mandatory progression, save/reload, interruptions, transitions and the ending. Include the problem classes identified in dated compatibility reports. Distinguish port defects, inherited behavior and accessibility barriers with evidence.

Completion evidence: an agreed end-to-end playthrough and accessibility coverage, with unresolved defects recorded. Reaching menus or a single battle does not close this gate.

## Public and private verification boundaries

Proposed public CI builds independently authored host components and original synthetic fixtures. It must succeed without private game inputs and must not upload generated game translations, shaders, dumps, media or diagnostic attachments containing game content.

Private integration checks cover the actual target. Store their inputs and generated outputs outside the publishable source tree. Publish only reviewed findings that do not contain restricted content or personal information. Inspect archives, caches, logs and debug outputs as well as tracked files.

The intended native executable and the allowed public release contents require a separate packaging decision. Do not assume that translating or compiling proprietary input makes every resulting byte suitable for release. A source-only milestone and private native build can be evaluated before settling distribution. This is an unresolved engineering/publication requirement, not a legal conclusion.

## Dependency provenance and contribution constraints

Record the license, revision and origin of every adopted component, including its transitive dependencies. Preserve required notices and legitimate authorship. Impersonal project prose must not remove attribution.

ReXGlue's root license is BSD-3-Clause and identifies Xenia-derived portions. Unleashed Recompiled uses GPL-3.0. Those labels do not replace a dependency-by-dependency review before copying or distributing code. Ten focused [ReXGlue evaluation patches](patches/rexglue/README.md) are now retained with upstream license notices; permanent dependency adoption and distribution remain undecided. [ReXGlue license](https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/LICENSE), [Unleashed license](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/COPYING).

ReXGlue's contribution guide requires associated issues for pull requests. Other reference projects have the AI restrictions noted above. Recheck applicable policies before any future contribution; researching public source does not authorize posting upstream. [ReXGlue contribution guide](https://github.com/rexglue/rexglue-sdk/wiki/Contributing).

## Remaining decisions

- Runtime call reachability and full CPU/control-flow coverage for the now-identified target.
- Future public visibility and source publication; the new repository is currently private.
- Runtime and graphics approach, after the feasibility comparison.
- Accessibility interaction designs as verified game behavior becomes available.
- Public packaging boundaries and the native-build delivery model.

No deadline, completion estimate, middleware claim or guaranteed tool compatibility is established by this document.
