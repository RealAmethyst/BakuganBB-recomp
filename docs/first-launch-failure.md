# First launch: missing indirect-call function

Inspection date: 2026-10-14. Owning work: [issue 13](https://github.com/RealAmethyst/bb-recomp/issues/13), with the related emission defect in [issue 14](https://github.com/RealAmethyst/bb-recomp/issues/14). The preceding build and its boundaries are in [diagnostic-launch.md](diagnostic-launch.md).

## Observed result

The tester launched `diagnostic-20261014.1` and heard a menu cursor sound identified as the language-selection screen, followed by an error. Audio output is listening-confirmed. The screen identity is the tester's observation, not a verified native screen/caption hook; the exact scope of spoken diagnostic feedback was not separately confirmed.

Private logs record graphics-device and swapchain initialization, shader work, controller recognition and audio-client setup before the fatal error. These observations do not establish rendered-frame correctness or playable navigation.

The process terminated with SIGABRT after attempting to dispatch guest address `0x8240F738`. That address is absent from the generated function maps in build `diagnostic-20261014.1`. The failed executable identity is SHA-256 `6584f5585ae9809c514f8f77aebc93c3a95b6f0507d347580f3d7318bd621abb`.

## Original code and crash evidence

Read-only analysis of the original executable and the saved crash core confirms a valid callable function, not an arbitrary pointer:

- Target `0x8240F738` is a six-instruction forwarding thunk ending with an unconditional CTR branch at `0x8240F74C`. Its preceding function starts at `0x8240F720` and ends with the same kind of branch at `0x8240F734`.
- Function table `0x8203A940` contains the predecessor at offset `0x40` and the missing target at offset `0x44` (`0x8203A984`). Constructor `0x82410D60` forms that table address at `0x82410D78`/`0x82410D8C` and stores it at object offset zero at `0x82410DA0`.
- The actual failing indirect call is at `0x823FA194`, returning to `0x823FA198`. It follows the object pointer at caller-object offset `0xB0`, that object's table at offset zero, then table offset `0x44`. The saved core contains this exact chain and target.
- The missing thunk follows its argument's offset `0x48`, adjusts the resulting object by `0x40`, then calls table slot `0x14`. The core-derived final target is `0x823F9B80` through table `0x82039F28`.
- That consumer adds four to the forwarded argument and branches to the original import thunk at `0x828EC9E4`, named `RtlLeaveCriticalSection` by executable metadata, kernel ordinal `0x130`. Thus the missing function forwards a lock-release operation; skipping it would not preserve behavior.

All 38 selected original instruction words across the caller, constructor, forwarding thunk and final consumer match the separate read-only Ghidra trace. Concrete heap addresses, registers, proprietary disassembly and the core remain private. No annotations were added. No game code was executed during this investigation.

## Discovery defect and synthetic regression

The pinned SDK's gap-fill partitioner splits on returns and selected direct branches but omits unconditional CTR branches. Its block discovery already stops at those branches. A following unseeded leaf can therefore remain inside a skipped segment and never become a registered function. The earlier backward-exit correction does not handle this case.

The bounded candidate adds the same unconditional-CTR termination predicate used by block discovery. Conditional CTR branches and linked calls retain fallthrough. Existing discovery/cleanup retain owned switch cases.

An original 21-case fixture gives 17/21 passes with the earlier candidate and 21/21 with the added correction. Four missing-leaf regressions fail before and pass after; all ten earlier gap-fill cases and seven additional controls remain passing. Three failing baseline cases still report zero validation errors because no direct branch references the missing target. Successful validation alone does not establish indirect-call coverage.

The switch controls use original manual table hints. Automatic switch recognition, arbitrary embedded data and every indirect-branch encoding are outside their proof.

## Emission defect exposed during integration

The gap-fill-only target regeneration registers the missing function, but reports 22 unresolved conditional branches while also emitting their destination labels. That intermediate candidate was rejected despite codegen returning zero. A broader inventory also found four pre-existing unresolved-branch fatal guards in the failed first build, missed by its earlier output checks: three persist in the intermediate candidate, nineteen are additional, and one is resolved by improved gap ownership.

The emitter passes an instruction address to `FunctionGraph::classifyTarget`. Its nearest-entry lookup can select a different function introduced into a hole between the real owner's discontiguous blocks. The emitter already has the exact `FunctionNode`; the correction passes its entry address at four classification sites while call-edge lookup retains the instruction site. The focused correction and output regression checks are tracked separately in issue 14.

An original fixture exercises actual `FunctionNode::emitCpp`: baseline 8/12, candidate 12/12. Four regressions cover conditional internal jumps, own-entry loops, internal linked branches and switch labels. Eight controls preserve calls, tails, imports and a genuinely unresolved target's error. The fixture explicitly constructs ownership and manual switch metadata; it does not prove automatic discovery. Independent source review verified that the writer passes the graph-owned node and that classification precedence, recursion and instruction-site call metadata remain intact.

## Registration audit

Normal regeneration changes 46,135 registered functions to 46,316: 186 additions and five removed accidental entries. The removed addresses remain emitted internal conditional blocks:

- `0x823E9820` belongs to `0x823E9810`, reached from `0x823E9818`.
- `0x823E9844` belongs to `0x823E9830`, reached from `0x823E9838`.
- `0x82400F80` belongs to `0x82400F70`, reached from `0x82400F78`.
- `0x824089B4` belongs to `0x824089A0`, reached from `0x824089A8`.
- `0x8283D840` belongs to `0x8283D828`, reached from `0x8283D834`.

Original executable analysis found one local unlinked conditional reference to each, no direct call/tail references and no aligned word pointers across all initialized blocks. Another 163 selected instruction words match independent original-input decoding. Runtime-computed aliases cannot be excluded by static scans, but no positive callable-root evidence was lost. The formerly standalone `0x824089B4` also contained the unresolved backward branch at `0x824089D0`; its proper owner now preserves that branch.

## Integrated verification and deployment

Both corrections were integrated with the earlier backward-exit candidate. Normal target generation completed with the missing thunk's declaration, six-instruction body and registration present, zero unresolved diagnostics and zero `REX_FATAL` markers across all 149 C++ translation units. Twenty existing float16 warnings remain; this is not complete instruction-semantic coverage.

The integrated upstream unit binary passed: 282 test-case records, four existing skips, zero errors/failures. Catch's JUnit summary reports 967 tests, counting below the case level. All 154 native build steps completed without compiler warnings or errors. All six retained SDK patches apply sequentially to the pinned revision; applied implementation files match the compiled candidate copies.

Installed replacement: `diagnostic-20261014.2`, native executable SHA-256 `84d93b47a035a198adbc3e4061c11cf5d7d130e964b1aaa3906414fe14a1cd84`. The original executable and manifest were backed up before replacement. The launcher lock was held and no installed game process was running. Runtime libraries, Prism and launcher are unchanged. ELF dependencies match the previous executable and RUNPATH remains `$ORIGIN`; deployed check-only preflight passes without game execution or speech initialization.

The subsequent user-controlled test reached the title screen with working input and music, and the tester closed normally. Logs confirm process exit zero with no fatal runtime error. This startup path passes the earlier failure; complete gameplay remains unverified. Reported opening audio/video drift is tracked in issue 15. The tester confirms the game stayed focused during that opening.

## Evidence and boundaries

The private `first-launch-crash-20261014` collection preserves the failed build manifest, logs, crash core, original-input comparisons, exact commands, synthetic graphs and candidate output. Its `target-trace` and `gapfill-probe` directories own the independent investigations. No game assets, dumps, binaries or generated proprietary source are included here.

The user controls the next launch. No live screen-reader services, game process, commits or pushes were controlled by the coding agent.
