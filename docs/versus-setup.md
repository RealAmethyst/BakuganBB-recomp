# Main-menu battle setup

## Scope and evidence

This record covers the native setup screens reached from the main menu's battle
entry. The portrait introduction immediately before a battle is a separate
[capture](battle-speech.md). Work and listening acceptance are tracked in
[issue 36](https://github.com/RealAmethyst/bb-recomp/issues/36).

The addresses below were traced on 2026-10-15 against the supported executable
in the [baseline](executable-baseline.md), using read-only Ghidra decompilation
and PowerPC instructions. Switch arms omitted by the ordinary function listing
were checked with bounded disassembly. Captions present only in artwork were
inspected in all seven resource variants. Raw exports and images remain private;
no RE database annotations were added.

Captures observe completed native operations. They do not replace input,
rendering, confirmation, sound or save behavior. A constructor and actual task
removal delimit a screen's lifetime; a child or popup only suspends its parent.
Ownership, active state and text-resource checks fail closed.

The approved speech policy, confirmed on 2026-10-16, announces all controllers
and describes picture-only controls. Controller identification is separate from
the native participant label because a controller can change player slots.
Supplementary wording lives in central translation resources. Native names,
values and hints retain their actual text or verified artwork sources.

## Root flow

The main-menu dispatcher `822D1BB0` handles result 1 through state 12 and
`822D1AA8`, which constructs the `0x440`-byte root at `8233E618`. Its global is
`829CC54C`, vtable `8201A078`, update function `8233E780`. The root owns match
configuration at `+20`, four participant records at `+44` with stride `F8`, arena
and difficulty bytes at `+424/+425`, and child completion at `+42C`.

- State 3 constructs player options at `822F70A8`; state 4 waits.
- State 5 constructs character selection at `822E9370`; state 6 waits.
- State 7 constructs the deck summary at `822F1138`; state 8 waits.
- State 9 constructs game rules at `82339BD0`; state A waits.
- State B constructs arena selection at `822F4530`; state C waits.
- Arena result 0 advances to battle preparation, 1 opens decks, 2 opens rules,
  and -2 returns to character selection. Character result 1 returns to player
  options. The root retains the configuration across these transitions.

## Shared selectors and resources

The custom grid selector has vtable `8200FC30`. Its `+28` points to an embedded
model at selector `+40`, vtable `8200FC10`. The model contains a graph pointer at
`+4`, enabled-byte storage at `+8`, current index at `+10`, count at `+14` and
validity byte at `+18`. Getter `821D7610` dispatches to `8226FB50`, which reads
that current index. The existing callback after `821DF0FC` observes a changed
index before the previous-index cache is updated. This is not necessarily the
completed screen operation: individual screens can subsequently redirect it.

Resource global `829CC358` points to a plain `0x1C`-byte structure, without a
vtable. Its fields point to `821EB110` loaders with vtable `82003F2C`. Loader
`+20` is the actual requested resource ID, including the locale applied by
`821EB1A0`. Native readiness checks inspect loader `+18`. Character resources
are at owner `+8`, arena at `+C`, and player options at `+18`.

Common title renderer `821E9908` reads the header's selector at `+58`, looks up
`82003E40`, and draws through `823A9908` at `821E9A54`. Selectors 0 through 4
use localized IDs `625`, `626`, `627`, `628`, `629`. These are actual displayed
titles, not headings inferred from class or task names.

The rules dispatcher `8233A4D8` and arena selector input `822F4DC0` pass
controller 0 to their native selector/input operations. Their speech identifies
that controller; character and participant-deck screens retain each controller's
own native focus instead.

## Player options

Constructor `822F70A8` creates a `0x13C`-byte task with vtable `8201802C`.
Completion `+18` and configuration `+20` must belong to the VS root. The grid
at `+AC` has seven logical rows: four participant slots, confirmation, and two
match kinds. The final two rows are enabled only with four occupied slots.
Occupied and human counts are bytes `+36/+37`; match kind is word `+38`.
Slot states at `+12F..132` encode empty, human and computer, while identity
frames at `+133..136` select the displayed participant identifiers.

`822FA008` consumes those identity frames through its bound model and widget
index. `822FA5A0` uses artwork UV 13/14 for the two match kinds. Resource IDs
`9FC..A02` select the seven locales. The title is text `625`, with hint `A08`.

Ready is after `822F8EEC`; value operations finish at `822F7CF8`, `822F7D18`,
`822F7F5C`, `822F7EBC`. Controller dialogs use input substate `+129` and suspend
the parent. Return at `822F8450` resumes its existing identity. Deleting
destructor `822F7248` and native task removal close it.

An initially inactive selector is expected. Constructor `821DE760` initializes
its previous-input state to 2 and previous choice to -1. On the first usable
controller input, `821DEDC0` makes the nested model valid and reaches the shared
selection event even if the cursor has not moved. Without usable input, native
focus remains invalid. This supplies the delayed initial item without polling.

Empty, human and computer slot descriptions use the approved central wording.
This screen accepts native input from controller 0 only; the all-controller
speech policy does not change native control ownership.

The wrapper around `822F7B00` coalesces changes until input completes. The native
operation can restore the previous slot at `822F7E30` after the shared selector
event, including cases with no fresh shared event. That store marks the final
snapshot dirty. Child and exit transitions discard outgoing focus. Idle input
does not read guest memory for capture.

## Game rules

Constructor `82339BD0` creates a `0xDC`-byte task with vtable `82019EFC`. Its
model at `+44` has vtable `82019EE4`, a current-values pointer at model `+10`
to task `+2C`, and an animation-history pointer at model `+14` to task `+38`.
The selector at `+7C` has 15 indices: paired left/right arrows for seven rows,
then confirmation. Speech positions count enabled logical rows, not arrows.
The native enable bytes at `+5E..64` must agree with the selector enable array.

Verified label/value consumers use these IDs and fields:

- Difficulty: label `A28`, byte `+2C`, value `95A + value` for 0 through 2.
- Gate cards: label `95D`, byte `+2D`, value `95E + (value == 0)`.
- Ability cards: label `960`, byte `+2E`, value `961 + (value == 0)`.
- Battle level: label `827`, word `+30`, value `828 + value` for 0 through 4.
- Attribute effect: label `9A7`, byte `+34`, value `64C + (value == 0)`.
- Special shots: label `644`, byte `+35`, the same boolean value mapping.
- Interference: label `7EE`, byte `+36`, the same boolean value mapping.

Title `627`, body `639` and hint `A1D` are separate native content. Confirmation
is common artwork UV 3. Ready is after `8233A578`; the label draw at `8233AB28`
allows a bounded retry while initial text is unavailable. Value operation
`82339910` is observed after completion. Suspend points are `8233A5F0` and
`8233A63C`; destructor `82339878` closes the lifetime.

Values are edited in a native local copy. `82339E48` writes back only for result
0; cancelling retains the original configuration. Capture must preserve this.

## Arena selection

Constructor `822F4530` creates a `0xC4`-byte task, vtable `82017F2C`, with model
`82017F20` at `+20`. The actual root call at `8233EC24..3C` and constructor
stores establish these bindings:

- Task `+18` points to root completion `+42C`.
- Task `+48` points to root battle rules `+2C`.
- Task `+4C` points to root arena `+424`.
- Task `+50` points to root match mode `+20`.
- Task `+54` points to root participants `+44`.
- Model pointers at task `+2C/+30/+34/+38` bind the selector at `+60`, accepted
  arena byte `+C0`, root difficulty `+425`, and maximum difficulty byte `+C1`.

The grid graph is `82017EB8` with nine indices. Actual navigable choices come
from task bytes `+3C..44`; `822F4DC0` skips unavailable arenas independently of
the generic grid's enable flags. In the alternate-input branch at
`822F4E20..38`, an unavailable raw cursor can coexist with the previously
accepted arena. The caption consumer follows the accepted byte, not that cursor.

Renderer `822F5CE8` follows bound model `+10` to the accepted byte, converts it
to the widget frame at `+18`, and calls `823C64A0` at `822F5F64`. Sequence 4,
track 0 maps logical indices to UVs `9,10,13,16,15,8,11,12,14` in texture 3.
These are the name captions. The thumbnail table `82017F74` is not a text source.
Difficulty renderer `822F6A18` follows model `+14` to root `+425` and selects
UVs 5 through 7 in texture 2. Resource IDs `91C..922` establish locale.

Ready occurs after `822F5528`. Navigation marks a pending change after
`822F58AC`, but the difficulty clamp and dependent flags finish later. Both
return paths, before `822F5908` and `822F59BC`, flush that pending change.
Unchanged updates perform no guest reads. Difficulty-only operations are final
after `822F577C` and `822F5850`. Transition stores at `822F559C`, `822F55EC`,
`822F5640`, `822F5694` suspend capture. Destructor `822F5B40` and actual task
removal close it.

Title `629` is separate from the displayed difficulty instruction `A0A`.
The latter is a hint, followed by `A02/A03` when shortcuts are enabled and
selection hint `9EC`. It is not a second heading.

## Character selection findings

Constructor `822E9370` creates a `0x33C`-byte task, vtable `82017C50`.
Participant records bind at `+2E0` and match configuration at `+2E4`. Four grids
start at `+74`, stride `60`. Controller count is byte `+1F4`.
Four slot records start at `+20`, stride `C`: state, committed character,
computer difficulty, computer flag, controller index, then animation and
custom-load flags. Controller `FF` means unassigned.

The grid has 24 raw indices: portraits 0 through 15, Random 16, Custom 17,
participant controls 18 through 21, confirmation 22, and a transient redirect
23. The redirect is not an independent navigable item. Locked portraits can
remain focusable even when native confirmation rejects them.

Input operation `822E89B8` can move a controller to another participant after
confirmation. The shared selector callback therefore cannot immediately speak
its intermediate value. Capture must defer until the operation completes and
derive the participant from the record's controller assignment. Likewise,
computer difficulty belongs to the committed character, not a hovered portrait.
The changed difficulty slot must be retained from native `r19`: a same-operation
controller handoff can occur before the write through the original slot base
in `r23`. Hooks after `822E8CF4/822E8D90` therefore carry owner, controller and
changed slot separately. Resolving the controller's final assignment would
announce the wrong value.

The primary render model is task `+1FC`, table `82017C48`; its `+C` points to
the slot records at task `+20`. The owned portrait renderer after `822EEC9C`
retains this model in `r30` and widget `82017CCC` in `r31`. It provides a retry
for explicitly pending text without requiring cursor movement. Once capture
succeeds, repeated draws perform no guest reads. Team widgets sharing a widget
type cannot establish primary ownership on their own.

Character identity transfer `822E7D58` copies the slot's character to participant
`+EA`. For ordinary character IDs below 16 it calls `822DBB68` with text ID
`D99 + character`, then copies the result into participant `+D4`. The text
provider chain reaches `823B60B8` and `8212ECF0`. Portrait artwork contains no
printed names; announcing these verified identities is part of the approved
picture-description policy.

CPU difficulty renderer `822EE418` installs pairs at widget `+28` with
destinations 6 through 8. These are UV replacements, not sequence numbers:
`823D3EE8` matches the track and indexes the replacement UV record with stride
`20` before consuming its texture index and coordinates. The inspected records
provide the seven-locale Easy/Normal/Hard captions. Committed character names are
copied in native capture before deduplication, so unavailable text can retry.
Difficulty status uses its actual changed slot independently of hovered focus.

The team section retains this same native task and common title. Renderer
`822EF3E8` draws actual localized section `64A` at `822EF4F4`. Three composition
rows map participant slots through table `82017B08`: `[0,1,2,3]`, `[0,2,1,3]`,
`[0,3,1,2]`. The first two positions form one team and the last two the other.
There is no separate confirmation row; the current composition is confirmed.

This section uses a different selector type: task `+26C`, table `82003900`,
embedded model at `+2AC`, table `820038DC`. Getter `821D86D8` reads model `+4`;
model `+8/+C` are lower and exclusive upper bounds, 0 and 3. A grid-style count
or current offset is wrong here. Cancellation resets task cache `+1F7` without
resetting the actual selector, so the cache cannot identify focus on reopening.
Ready is after `822EB30C`; cancel/accept suspend at `822EB33C/822EB34C`.
Character focus resumes after `822EB4E8`. Native team input uses controller 0.
Selector virtual `+4` is the shared `821DEDC0` update, also used by the grid.
It dispatches the current-item getter through the model's virtual `+C`, so
the existing event at `821DF0FC` observes this linear selector too.
The wrapper around `822E7A40` coalesces changes until input completes; confirmation
or cancellation suspends the section. The bound identity renderer after
`822EFC28` retries only pending localized text. Captured team facts retain all
four participant positions and their displayed identity frames; composing their
spoken description remains separate from capture.

## Nested deck screens

The root deck summary is `822F1138`, vtable `82017DDC`, size `208`. Its update
`822F2208` uses title `628` and hint `9EC`. Selecting a participant enters a
nested editor through `823111E0` or `82311528`; returning rebuilds the summary
without destroying its lifetime.

The nested editor is a `4E0`-byte task with vtable `82018DE8`, updated by
`823132E8`. It has deck, current-item, replacement, filter and confirmation
states. Its header cannot be inferred from the parent: text `9CB` is an
equipped-deck status badge, not an editor title. Header renderer `8231AA00`
draws the participant's current deck name at `+A8` through `8231AC0C`.

Both VS constructors set immutable mode `+18` to 0. The renaming branch requires
mode 1, so it is outside this route. Current-item and replacement branches remain
reachable. Main selector `+148` has previous/next page controls at 0/7, content
at 1 through 6, and finish at 8. Page navigation can immediately redirect focus;
capture must wait for the rebuilt content and native animation readiness.

Deck-summary input `822F0D18` updates secondary controllers before controller 0.
Capture coalesces its changed selections and emits final snapshots in controller
order only for result 5, which retains the screen. Results 0 through 4 and 6
transition away and discard pending focus. On a parent return, the first actual
resumed item interrupts; later controllers queue behind it, even if controller
0's text becomes readable later.

Selected-caption consumer `82319F88` resolves its model through render context
`+C`, that object's vector at `+C`, and the widget's `+A` index. The model at
editor `+C4` binds participant through `+10` and section data through `+18`.
Sections 2/4 use creature name ID `DB1 + item`; sections 3/5 use card name ID
`A51 + 4 * item`. Current caption is widget `+104`; `+14A` is animation history.
The deck-list update `82310498` copies the selected deck name to participant
`+A8` before rendering. Capture runs before the common join at `8231A2E4`,
including the branch where the caption has not changed. The preceding copy
operation alone would miss same-caption focus changes and child returns.

The six-choice picker embedded at editor `+20C` is constructed by `82305978`.
Its title is `6B9` for kind 0 or `6BC` for kind 1. Renderer `8230A540` selects
artwork through table `82018920[kind * 6 + row]`. All seven inspected locales
show attribute icons or colored card frames, without printed choice captions.
The applied attribute enum reaches text table `8201A930` through `8234BA78`;
the table maps to IDs `656,657,65A,658,659,65B`. The picker stores its accepted
row directly as the inventory attribute, so these names identify the icons.
Card-frame colors use approved central descriptions. Confirmation builder
`82312E18` uses mode-zero prompt IDs
`914,918,7F6,7F6,9A4`, indexed by its native kind, and common artwork Yes/No.
Success messages `915/92D` are later status content, not part of the prompt.

The current item cache uses type 0 for creatures and types 1/2 for the two
card groups. Item domains are 83 creature species and 279 cards, with six
attributes. Inventory groups use `(kind * 6 + attribute) * 0x18`; vector begin
and end are at editor `+3B8/+3BC` within that group. Page reconstruction
`8230F990` computes the last page from the item count with six entries per page.
The selected content row maps to cache slot `row - 1` after `8230FDA0`.

Authored artwork contains collection captions, but their visibility is not
established for this route. Background renderer `82315310` draws track 3,
excluding the track containing those captions. Image inspection alone does
not justify announcing them as sections.

Dynamic hint renderer `8231A578` repurposes `r31` before its string draw at
`8231A840`. Its original `r4` render context is preserved by a scoped wrapper;
the draw callback validates that context with widget `r29` and text `r4`.
Using the later `r31` as a model would follow the text provider instead. The
preview hint at `8231B190` and common footer at `821EA9E4` have separate verified
bindings. Late footer text queues after the first item; it does not require
cursor movement. Empty lists announce status and the actual Back hint without
inventing a selectable row. Empty card inventory uses a central factual zero
count because its renderer supplies no native empty-list caption.

The creature detail widgets consume the selected instance, from the participant
for section 2 or the species/attribute inventory for section 4. `82318F68` reads
signed instance `+8` for its three digits; negatives display zero. Captures omit
unverified digit frames above 999 with a named diagnostic. `82316898` consumes
instance `+10` for level artwork: sequence 95 supplies the localized LEVEL
label, sequence 96 supplies levels 1 through 9 and its maximum frame. These
verified details accompany the actual localized attribute and selected name.

Card-image effects remain outside text capture. `82317898` requests the small
card texture through `82330F10`; cache `82330AD0`, pump `82330C90` and loader
`82207F18` load resource `1033 + cardID`. Callback `82207F70` and decoder
`821C63A0` handle an image archive, without a localized effect-text consumer in
that chain. Its MCT pixels remain uninspected. Adjacent name-table IDs are not
evidence of displayed effects and are not announced as substitutes.

The summary's participant-name renderer `822F3318` draws the bound UTF-16 name
through `823A9528` at `822F3428`. Its apparent deck-name renderer `822F3538`
validates its model but never draws text. A plausible participant field is not
sufficient evidence to announce a visible summary caption.

## Bakugan detail viewer

The deck preview helper `8230E9F8` reaches `822E0440`, which constructs a
`B4`-byte task at `822DC058`, vtable `82017294`. Scope follows the live VS root
in state 8, its summary in state 6, and the editor's `+2C` completion pointer to
summary `+200`. The editor's participant at `+1C` must be one of the root's four
`F8`-byte records. Only construction inside that verified preview operation is
captured; the viewer has other native callers.

Global `829CC330` owns a `30`-byte resource loader with vtable `82003F2C` and
completion at `+2C`. Viewer task `+18` points to that completion. The constructor
copies the selected `14`-byte instance to task `+2C`; model `+20` has table
`82017278`. The manager requests resource IDs `9D4..9DA` for the seven locales.

The actual layout 20 shows the creature name above the model, then its biography
and Back hint. It has no separate title or selectable rows. Name renderer
`822DCBC8` and biography renderer `822DCDF8` follow render context `+C`, vector
begin/end at `+C/+10`, and widget index `+A` to the bound model. At their actual
draw calls `822DCCE0` and `822DCF34`, `r29` still holds that model and `r5` holds
the exact localized text ID. Capture validates the registered model and uses
those IDs, rather than reconstructing a biography-table index.

For reference, native name lookup is `DB1 + species`, and the biography table is
`82016AB0[species * 6 + attribute]`. Inventory initializer `823105A8` establishes
83 species and six attributes; the complete table ends at `82017278`.
Hint renderer `822DC998` draws text `A08` at `822DCAAC`.

The preview helper passes editor controller `+38` through `822E0440` to the
viewer constructor's `r7`. The constructor stores it at viewer `+B0`; update
`822DC460` uses that byte for native input. Capture checks the constructor value,
stored byte and editor binding before attaching controller identity to speech.

Update `822DC460` becomes active after `822DC528`; close input advances it after
`822DC57C`. Capture waits for both actual content draws before opening speech,
then avoids further guest reads on repeated draws. Destructor `822DD1E0` and
native task removal close it. Normal cleanup writes completion through
`822CE970`; the editor waits through `822E0428` and resumes its earlier section.

Speech presents name and biography as content, with Back queued afterward. It
does not add an index or infer a title from the creature's identity.

## Verification boundary

Synthetic arena checks pass under AddressSanitizer and UndefinedBehaviorSanitizer
with isolated runtime, display, bus, configuration and device resources. They
cover availability-based positions, deferred clamping, difficulty changes,
alternate-input cursor divergence, ownership rejection, lifetime cleanup,
locale bounds, idle reads and exception isolation. Source-only speech checks
cover artwork section/value resolution and announcement ordering.

The player-options and rules fixtures also pass under the same isolation and
sanitizers. All nine ordinary source-check groups pass, along with separate
shared-menu and JSON serialization fixtures. Rules additionally exercise an
initial hint arriving after the opening title, retaining the same screen ID.
Viewer fixtures cover nested ownership, actual text-ID capture, delayed
biography, repeated-draw silence, unrelated callers, suspension, removal,
destructor forwarding and failure latching. Its speech test checks body-before-
hint order and absence of invented rows.

Character fixtures exercise controller reassignment, transient focus redirection,
delayed text, child returns, ownership rejection and a difficulty change belonging
to the previously focused slot. Team cases check actual selector position after
cancel/reopen, composition changes, delayed text and the retained parent lifetime.
Deck-summary fixtures check controller-specific focus and parent returns.

All seven VS capture fixtures and both presentation-bridge fixtures pass together
under the isolated sanitizer runner.
Editor cases cover native formed captions, inventory consistency, late picker
and confirmation text, viewer returns, stale parent/resource/model rejection,
malformed text and floating-point/register preservation. The shared menu fixture
also passes with all new selector notifications linked.

All capture and presentation sinks are connected to the shared event transport.
The approved policy announces all controllers and describes picture-only
controls. Speech tests cover independent controller focus, delayed initial items,
queued parent returns, localized templates, section-before-status ordering and
late hints, while preserving ordinary popup and status behavior.

Fresh generation contains 67 VS observer calls across 40 callback names. Native
builds with and without Steam Audio compile and link without warnings. All 30 VS
wrappers link as strong symbols alongside their original implementations. The
configuration matches the generated snapshot. The nine source-check groups,
nine isolated VS fixtures and separate event-serialization checks pass. These
checks use synthetic fixtures and do not execute the game.

Private build `diagnostic-20261016.1` is deployed with the accepted
`diagnostic-20261015.5` files backed up. Installation verified previous and new
file digests, native dependencies, the launch lock and isolated check-only
preflight. The runtime and GPU library identities are unchanged.

Listening feedback on 2026-10-16 confirms that VS menu speech works in the
tested flow. Individual screens and controllers were not enumerated; graphics,
complete VS-route coverage and regression acceptance remain unconfirmed.
Card names and color descriptions are captured, but effect wording
inside compressed card artwork has no verified text source on this route and is
not announced. No RE database annotations were added.
