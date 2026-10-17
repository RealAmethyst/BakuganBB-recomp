# Battle selection and match-entrance speech

Investigation date: 2026-10-15. Tracking: [issue 32](https://github.com/RealAmethyst/BakuganBB-recomp/issues/32).
Addresses refer to the [supported executable](executable-baseline.md). Capture
uses game-owned localized text and the existing shared speech queue. Original
execution, CPU registers and floating-point state are preserved.

## Gate-card picker

Constructor `82271750` creates task vtable `8200FD04`, with its carousel at
`+20` and detail widget at `+80`. Task update `82271518` reaches state two at
`82271638` only after the carousel, detail widget and common instruction widget
are ready. State three, written at `822716B4`, ends active selection; it does
not manufacture a new native screen lifetime.

The carousel stores two-byte rows at `[+18,+1C)` and its current row at `+44`.
Relative to the task these are `+38/+3C` and `+64`. Native input `82231E88`
wraps the complete list without skipping disabled rows; its change commit at
`82231FEC` follows the actual cursor write. Speech position is the selected row
plus one, with the actual row count. Row byte zero is a card-instance slot,
not a localized name index.

The exact text chain is global `829CBEC4` plus `20` to the scene, then the
pointer vector `[scene+1C74,scene+1C78)`. Helper `822142F0` loads the selected
instance and then its definition pointer at `+4`. Detail draw `82233CC0`
uses definition word `+18` in `A51 + 4*index + styleOffset`, reaching native
text draw `823A9908`. The verified style offsets zero and one select the card
name and effect description. Offset three is a printed card serial code;
it is not substituted for the effect description.

The common selection instructions use bank zero, ID `9E3`. After visibility
gating, actual draw `821E9C30` loads its record's bank and ID into `r28/r27` at
`821E9E98/9C`, with the record still in `r29`. Capture accepts only the current
common owner's record at `+60`. Late instructions are attached to the same
focused item, so the speech policy queues them once after the first readable
selection without repeating that selection.

No screen title is invented. The first item interrupts; subsequent selection
changes speak name, effect and list position. Native selector teardown
`82232010`, task destructor `822718E0` and generic task removal `823B87A8`
close the corresponding tracked lifetime. Reused addresses receive new IDs.

## Bakugan selection

Throw-preparation constructor `8224C5D0` installs task vtable `8200EAB0`.
The embedded selector is at `+160`, participant index at `+34`, locked row at
`+58`, and detail record at `+9C0` with mode three. The original update starts
at `822513F8`; its state-five write at `822515D8` follows selection setup and
marks interactive readiness. Accept/cancel writes at `82251610` and
`82251604` suspend selection capture. Returning from aiming reuses the same
native task and repeats the current item without repeating opening hints.

Native selection-to-detail function `8224F738` rejects locked or invalid
selection, obtains the selector's current row through its native virtual
getter, requires row zero through two, then calls `8224CC98`. The commit at
`8224F7F4` stores the resulting global Bakugan slot at task `+9D2`; live
registers `r31/r30/r3` provide task, row and slot. The rejection store at
`8224F860` clears focus without closing the screen. A subsequent valid commit
can therefore repeat the same item after focus returns. Unchanged committed
values are deduplicated before guest-memory reads.

Helper `8224CC98` uses scene byte `24CA` as the per-participant count and
forms the slot from participant times count plus row. It accepts that slot
only when present in the participant's byte vector at `+0C/+10`. Native input
`82250330` scans row order and skips absent candidates. Speech position is the
rank among available row numbers, independent of the byte vector's storage
order. Capture is limited to counts one through three because the verified
detail commit excludes higher rows; this is a capture limitation, not a claim
that every game configuration has at most three entries.

Actual name draw `82234A18` indexes the vector at global battle owner
`+1C7C/+1C80`, stride `14` hexadecimal. Unlike the card-definition chain, this
vector is relative to the owner itself, without adding the embedded-scene
offset. Entry word zero supplies the index in `DB1 + index`, passed to
`823A9908` at `82234B84`. No prose description is invented for this name field.
The task supplies common instruction ID `A39` or `9F2`, according to the
native controller-input condition. The shared visible-hint hook captures the
actual selected caption and queues it once after the initial item.

Teardown `8224F880`, destructor `8224C948` and generic task removal close the
tracked lifetime. Synthetic fixtures include a nonzero participant, reversed
available-slot storage, missing rows, deferred initial commits, unchanged
callbacks, focus return and return from aiming.

## Mode-two match entrance

Task constructor `823681D0` stores mode at `+24`, child pointer at `+2C` and
uses vtable `82032BC8`. The child has vtable `82032BB8` at `+40`, mode at `+50`,
battle type at `+58`, and three supplied character IDs at `+30/+34/+38`.
Task update `82368300` waits for the child's nonzero ready byte `+1C`, then
calls builder `8236A688` from state one. Speech captures the completed builder,
not a partially constructed panel.

During that builder only, portrait constructor `82368918` supplies the actual
displayed character ID in `r6`, presentation subtype zero in `r5`, and a
returned widget with vtable `82032BE4`. Its native atlas table `82032B24` has
16 entries. Capture validates these identities and collects native constructor
order; this is not asserted to be spatial left-to-right order.

Original instructions `8236A9E8` through `8236AA1C` establish the type-dependent
enumeration. Types zero through two use the supplied IDs in order. Types three
and five skip the first authored portrait but still start with the first
supplied ID. Type four skips the first authored portrait and starts with the
second supplied ID. Capture follows actual constructor calls and validates
their relationship to those input fields.

The portrait-to-name association is grounded in battle setup. Caller
`82344450` supplies fields `14/20/2C` from scenario records at `829A43A0`,
stride `44`. Setup `82343B38` uses those same fields for participants one
through three and calls `822DBB68` with bank zero and `D99 + characterID`.
The second route, caller `82354A48` and setup `823542C8`, uses fields
`1C/28/34` at `829AAF00`, stride `48`, with the same name lookup. Participant
zero uses the custom profile name through a different branch.

The announcement contains the localized names of the constructed portraits.
There is no invented VS heading, role label or menu index. Native close
`82368750` clears the child's renderer layer and context; destructor
`823688A8` and generic task removal also end the lifetime. Capture gaps and
repeated builder calls do not repeat the announcement.

## Shared localization and scope limits

The final bank-zero lookup follows provider global `829CEB78`, provider method
`823B60B8`, descriptor selection and relative-offset consumer `8212ECF0`.
`ReadMenuText` validates the provider/resource identities and bounds each
UTF-16 string by its following offset. Each feature establishes its exact
resource ID before using that shared reader. No extracted strings appear in
the implementation or synthetic fixtures.

The same entrance task's modes zero and one represent different tournament
overview/round panels, including defeated and masked future portraits. They
are excluded from this implementation. The separate `8220DC20` character
entrance/camera sequence is also not used as a VS text source. Matching the
verified mode-two panel to the reported brief VS moment still requires an
in-game check.

Turn-banner findings are research only: the native announcer already supplies
turn information. No turn speech or new audio settings menu is added.

## Offline verification

Original synthetic fixtures cover native forwarding, CPU/FPU preservation,
nonzero initial selections, row-to-instance mapping, delayed text/hints,
native close/reopen, malformed memory and text, and callback failure.
Entrance fixtures additionally cover actual portrait enumeration, type-four
skipping, unsupported modes, partial-build suppression and exception unwinding.
ASan/UBSan checks use isolated runtime/device namespaces and no game, audio
device or screen reader. Independent source reviews found no confirmed
correctness blockers in these scopes. The user confirms that the new speech spoke during testing of
`diagnostic-20261015.1`, but the gate cue made it difficult to hear. Detailed
per-screen ordering and gameplay acceptance remains incomplete. Audio masking
and activation follow-ups are recorded in [issue 31](https://github.com/RealAmethyst/BakuganBB-recomp/issues/31);
the next-session priority is the [settings rewrite](https://github.com/RealAmethyst/BakuganBB-recomp/issues/33).

Private original-instruction evidence is retained in `battle-speech-20261015`.
No RE database annotations were added. Raw game code and assets remain outside
the source tree.
