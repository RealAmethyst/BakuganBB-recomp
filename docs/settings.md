# In-game settings redesign

## Scope and approved behavior

[Issue 33](https://github.com/RealAmethyst/bb-recomp/issues/33) targets the game's
settings screen reached from the main menu. On 2026-10-15 the owner corrected an
initial interpretation that had targeted the host F4 overlay. That separate
[source reference](host-settings.md) does not establish the native screen's
options, input behavior or persistence.

Approved categories are Display, Audio, Controls, Accessibility and Advanced.
Value changes must save automatically. Existing native settings and their effects
must be preserved. Accessibility, Audio must include separate enabled and volume
controls for friendly and enemy gate-card cues.

Restore Defaults affects only the current category, asks for confirmation, then
applies and automatically saves the reset.

PC settings are included in the new in-game menu. This approval includes the
scope discussed with fullscreen, resolution and VSync; it does not establish that
every SDK registry entry is suitable for exposure or safe to change live.
Window Size and Render Scale are approved as separate controls. Render Scale
requires a restart and must be identified as experimental.

The ordinary main-menu route is the requested entry. A new host-overlay controller
shortcut is not a requirement. Approved navigation uses Up/Down to move, A to
open or activate, Left/Right to adjust, and B to return while retaining changes.
Cue volume uses 0–100 percent in ten-percent steps, retaining enabled and
100-percent defaults for both cues.

The presentation must fit the native game: preserve its fonts, artwork,
transitions and interaction sounds. Native widget layout and draw contracts must
be verified before adapting them; the original spatial navigation slots are not
a vertical category list. On 2026-10-15 visual feedback confirmed the menu
opens and works, but identified clipped navigation help, category alignment and
missing visible focus. The approved help is now the short “A: Select. B: Back.”
line; category alignment must follow native menu conventions.

## Investigation and implementation sequence

1. Trace the main-menu transition into settings through the original executable.
2. Inventory actual displayed options, localized text/artwork, input, defaults,
   immediate effects and save/load consumers, including ownership and lifetime.
3. Resolve any remaining interaction decisions against that inventory. Keep
   native game content sourced from the game and new port-owned labels central.
4. Implement category presentation and automatic persistence through verified
   game and port-owned boundaries, preserving main-menu returns and speech.
5. Verify offline navigation, speech, value changes, defaults, save failures and
   affected existing behavior. Review consequential integration independently.
6. Deploy verified changes with backup for user-controlled testing.

## Evidence identity and limits

The following source investigation used the original executable identified in
[the baseline](executable-baseline.md), SHA-256
`7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b`, on
2026-10-15. Private read-only Ghidra exports are retained in collection
`native-settings-20261015`. Scalar instructions were decoded separately where
unsupported vector instructions truncated decompilation. No database annotations
were added. No game, speech backend or audio service was launched or controlled.

This is original-code and resource evidence, not interactive confirmation.
The implementation now connects a category model, native presentation adapter,
native save controller and port settings owner. Offline checks establish their
synthetic contracts. The owner accepted the settings foundation after testing
`diagnostic-20261015.5`, reporting saving when backing out, native-style Restore
Defaults confirmation and working interaction sounds. This is not a claim that
every display mode, gameplay effect, failure path or save timing was exercised.
Deployment identity and acceptance records belong to issue 33.

Earlier candidates passed synthetic checks but failed interactive entry: the
first rejected the wrong shared-owner field; the next rejected a valid sprite
edge and entered the original save-and-exit path. Those failures exposed gaps
in the original fixtures. The fresh audit below includes actual-resource CPU
geometry checks and a full adapter lifecycle fixture with a negative control.
Both Linux native build paths (with and without optional spatial audio), all
nine source CTest groups and 75 Python contracts pass after the corrections.
Issue 33 records deployment identity, acceptance and remaining verification limits.

## Main-menu entry and screen lifetime

Main-menu constructor `0x822CED20` installs table `0x8201205C`. Its result is
published through the completion pointer at task `+0x18`; selected result four
is Options. The root dispatcher `0x822D1BB0` receives that result in state
`0x0F`, selects state `0x1A`, and constructs the settings task at call
`0x822D21E8` using `0x82333C08`. The allocation is `0x58` bytes, and the
completion pointer is dispatcher `+0x18`. State `0x1B` waits for completion and
returns to state `0x0E`, which constructs the main menu again.

The main menu closes before Options opens. Its update `0x822CF7D0` and cleanup
`0x822CEB38` reach completion through `0x822CE988`, `0x822CE970` and task removal
`0x823B87A8`. Returning is a real reopening, rather than a child focus return
under a still-open main menu. The replacement must preserve that distinction.

Settings constructor `0x82333C08` installs task table `0x82019C40` and initializes
completion/result to minus one. Task `+0x14` is the state; `+0x18` is the borrowed
completion pointer; `+0x1C` is the result. The embedded model begins at `+0x20`,
and task `+0x54` holds the preview-audio handle. Update `0x82333CD0` has these
responsibilities:

- State zero: initialize resources, values and selectors through `0x82334718`.
- State one: wait for resources through `0x82334810`, build widgets through
  `0x82335320`, and start preview audio.
- State two: run the active model handler `0x823351E8`.
- State three: wait for closing animations through `0x82334920`, then stop preview
  audio.
- State four: apply exit/save cleanup through `0x82334E50`.
- State five: pump save completion through `0x822E6560`, then publish the result
  through the task completion slot.

Destructor `0x82333E38` calls model cleanup `0x82334A18` before the base task
destructor/free. New UI ownership must not outlive that native task or hold its
borrowed completion pointer after removal.

### Animation barriers and failed initialization

Fresh tracing after the initialization failure distinguishes task states from
the primary render record's byte at `+8`. The latter opens through `0→1→2`.
Closing requests `6→7`; `82334888` accepts seven, starts state three in the
primary record, shared title/hints and animation controller, and completes the
active task phase. `82334920` waits until all four owners reach state four.

These transitions depend on real callback completion. Dispatch `823CE010`
clears each bound record's pending field `+4`. The standalone animation path
`821E6AB8→821E4190→821D7EE8` resolves the same context-vector slot from widget
`+A` and marks that field while animation remains active. `82335E40` checks it
before advancing `1→2`, `6→7` or `3→4`. Hiding obsolete geometry must preserve
these callbacks; forcing a ready/closed value would bypass the native barrier.

Cleanup `82334E50` ordinarily calls final-save helper `822E6288` whenever exit
marker `model+30` is not `FF`, including when no option changed. Marker `FF`
instead reloads and reapplies stored options; it is not a neutral failure exit.
The replacement now identifies renderer failure before the managed screen
opens, including construction, initial publication and opening animation. For that unopened session, with no accepted host command or owned
save pending, it suppresses only the final-save request. Closing animation and
all remaining cleanup still run. Accepted changes retain normal save behavior.

The actual-integration lifecycle fixture injects native renderer outcomes and
checks this distinction through entry, readiness, input, closing and cleanup.
Restoring the old final-save predicate makes its no-save assertion fail. It
does not execute the game's renderer or establish visual or listening results.

## Displayed content and localization

The native title is bank-zero text ID `0x612`, English caption “Options”. Builder
`0x82335320` calls `0x821E8930` with title selector nine; the shared record stores
that selector at root `+0x58`. Widget constructor `0x821E9840` installs table
`0x82003EBC`. Its renderer `0x821E9908` reads the selector from the bound record
at `0x821E9A24`, indexes table `0x82003E40`, and calls `0x823A9908` at
`0x821E9A54`. Table entry nine is `0x612`. All seven bank-zero language resources
were checked for this caption. The uppercase main-menu artwork is not the title
source for this screen.

The record-binding hop was checked separately after review: at `0x821E8964`,
registration `0x823CA4C8` writes `commonRoot + 0x4C` into the render context's
`+0xC` vector at index `0x14`. Its range-checked store is `0x823CA4EC`. Widget
registration `0x823CE3F8` sets widget `+0xA` to that same index and appends the
widget to context `+0x8`. Dispatch `0x823CE010` retains its context in `r29` and
passes that context as callback `r4` at `0x823CE240..0x823CE24C`. Renderer
`0x821E9908` follows context `+0xC` to vector begin `+0xC`, bounds-checks widget
`+0xA` and obtains the stored record at `0x821E9958`. Record `+0xC` is therefore
the selector at `commonRoot + 0x58`, rather than an inferred adjacent field.

Setting-label constructor `0x82336B00` stores a row byte at widget `+0x100`.
Renderer `0x82336C28` uses that byte to index table `0x82019C94`, then calls
`0x823A9908` at `0x82336E0C` with bank zero. The five rows are:

- Throwing Practice: ID `0x928`, row zero.
- Vibration: ID `0xA0D`, row one.
- Music: ID `0x7BE`, row two.
- SFX: ID `0x7BF`, row three.
- Voice: ID `0x7C0`, row four.

The exact five captions were validated in all seven language resources.
`0x823A9908` loads provider `0x829CEB78`, calls its `+0x18` getter at
`0x823A9954` and passes the result through `0x82395428` for native expansion.
An implementation reusing the shared text reader still needs its applicable
provider/descriptor chain validated at the point of use.

Rows five and six are Default and OK. Their constructor `0x821CDAC0` stores
choice codes four and zero at widget `+0x120`; renderer
`0x82337CA8 → 0x821CC9D0` selects shared-art UV entry `code + 3`. English common
resource `0x8CC`, entries seven and three, was inspected to verify those actual
labels. This investigation did not transcribe Default from all seven common
atlases. No unrelated localized text ID may substitute for an artwork caption.

The options resource request is base `0x9AB` plus the runtime locale number,
through loader `0x821EB1A0`. Resources `0x9AC..0x9B2` cover locales one through
seven. The English options atlases show ON/OFF artwork and a ten-segment audio
bar. The bar does not provide an original numeric percentage caption. Central
project resources must own any new numeric value format and port-owned labels.

Native opening hint ID `0xA21` and adjustment hint `0xA1E` are separate from the
title. The direction glyph's precise spoken substitution has not been
established here. A rewritten control scheme needs its own accurate hints.

## Model, values and native effects

Model constructor `0x82334FC0` owns a loader at `+0x4`, animation controller at
`+0x8`, main selector at `+0xC`, secondary selector at `+0x10`, and primary and
secondary render/input records at `+0x14/+0x18`. Each record contains selector
pointers at `+0xC/+0x10`, player pointer at `+0x14`, current-settings pointer at
`+0x18`, preview-handle pointer at `+0x1C` and state at `+0x8`.

The model's current six-byte record is at `+0x24`, its backup at `+0x2A`, exit
marker at `+0x30`, player byte at `+0x1C` and interaction substate at `+0x23`.
Byte zero is retained metadata; its broader meaning is not established. Getter
`0x82333F08` reads byte `row + 1` through the render record's settings pointer.
The two toggles use zero/one. All three audio values use one through ten.

Guard `0x823340B0` checks whether the selected row can change in the requested
direction. Mutator `0x823341A0` changes the byte and performs these effects:

- Throwing Practice changes byte one. Its later gameplay consumer remains
  outside this traced UI path; do not infer extra behavior from the label.
- Vibration changes byte two, updates input manager `0x829CDA70` through slot
  `+0xCC`, and invokes the enabled preview through slot `+0x68`.
- Music, SFX and Voice change bytes three, four and five and call audio manager
  `0x829CDCE8` slots `+0x50`, `+0x58` and `+0x60`, respectively.

The audio conversion is `(value - 1) * constant(0x8201223C)`, whose float bits
are `0x3DE38E39`, approximately one ninth. Value one is silent and ten is full
gain. The inner input/audio implementations must be followed further if a
replacement calls them directly instead of retaining the native mutator.

Default handler `0x823344A8` restores bytes `00 01 01 0A 0A 0A` and applies the
three audio gains. Vibration is reapplied on exit. These are verified explicit
reset values; they are not proof of the complete no-save startup path.

## Original interaction and exit behavior

Setup `0x82334718` initializes the main selector with seven navigable entries.
`0x823351E8` dispatches main interaction to `0x82334AE8`, adjustment interaction
to `0x82334CE8`, Default processing to `0x82334838`, and exit to `0x82334888`.
The original main interaction toggles rows zero/one directly. Rows two through
four open a separate adjustment selector. Voice additionally starts a preview
selected from table `0x82019CD8`.

Entering adjustment copies the six current bytes to the backup. Accept retains
the current bytes; Back calls rollback `0x82333F70`, which restores the relevant
bytes and reapplies audio gains. This local cancel behavior differs from the
approved immediate-saving model and must not accidentally survive behind it.

Default starts a native animation before the reset. OK sets exit marker six.
Back from the main native list opens two-choice popup `0x821DFEC8` with bank-zero
body `0xA3C`, warning about losing unsaved progress. A confirmed discard uses
exit marker `0xFF`. Closing handler `0x82334888` waits for the input record's
native readiness, initiates widget/title/hint animations and disables its cursor.
`0x82334920` waits for those animation states to finish.

Cleanup `0x82334E50` reloads and restores saved values for marker `0xFF`;
otherwise it requests a save through `0x822E6288`. It reapplies vibration and
removes render layers `0x15`, `0x16` and shared overlay layer `0x19`, with the
associated shared title/hint state. The new menu cannot retain a discard warning
that falsely claims automatically saved edits are reversible.

## Native persistence and asynchronous ownership

Global `0x829CC34C` is the `0x94`-byte game save manager, constructed by
`0x822E6300`. Global `0x829CC350` is the separate `0x3C`-byte save-operation
service, constructed by `0x821C0330`. The manager's embedded settings adapter
at `+0x18` has table `0x82017A08`: write callback `0x822E5D38` at slot four and
read callback `0x822E5DA0` at slot eight. Adapter `+0x4` is a borrowed pointer to
six settings bytes. Requesting a save does not snapshot them.

Both callbacks select the slot through byte `0x829CA40C` at callback execution,
via `0x821BBA90`. The settings location is payload `+0x369C + slot * 0x3660`.
Singleton accessor `0x821BC7E0` returns `0x829CA610`; its first pointer is the
backing descriptor, whose `+0x0` is the serialized image and `+0x4` the payload
at image `+0x10`. Descriptor `+0x108` controls whether a disk-save context is
active. The image allocation is `0xD9EC` bytes. Initializer `0x821BD1F8` visits
four slot records, but surrounding initialization ordering is not yet a complete
missing-save/default-path proof.

Load helper `0x822E6490` points the adapter at its own stack buffer. With an
active context it reads through `0x821BBF68`; otherwise `0x821BBF40` obtains the
slot data and the helper copies the settings from offset `+0x3640`. Adapter
pointer ownership must therefore also account for native loads.

Save helper `0x822E6288` installs the borrowed buffer and invokes `0x821C0880`
with progress-UI flag one. The caller's incoming third argument is overwritten;
the observed value `0x5E` is not a copy length. With no active save context, the
fallback updates the backing RAM through the write callback without writing a
file. This must not be announced as durable persistence.

Service `0x821C0880` writes callback and popup fields at `+0x24/+0x28` before
checking whether operation `+0xC` is idle. A busy call can overwrite the active
request's fields even though it reports failure. The existing outer helper
ignores that result. Calling it for every input edge would be unsafe.

Accepted operation six proceeds through `0x821C12D8 → 0x821BC518` with operation
three. It invokes the adapter to consume the borrowed settings buffer, then
constructs a job through `0x821C3880` at call `0x821BC664`. Construction initializes
the job in state zero; it does not snapshot the image. A later pump through
`0x821BC8C0 → 0x821C35F8` calls preparation `0x821C3230` at `0x821C362C`.
Only then does the copy at `0x821C3268` snapshot the full image into job storage
at `+0x3C`. Generation tracking must account for that interval between adapter
consumption and image capture. The write path
`0x821BFFB0 → 0x821BF838 → 0x823BB338` reaches
`0x823BB3AC → 0x828EC774`, the imported `NtWriteFile` thunk.

Job state `+0xDA28` distinguishes preparation, pending, success and failure.
Poll `0x821C3358` uses `0x821BF788`; pending is minus one, success zero and
other outcomes become failure. `0x821BC8C0` records the job outcome in singleton
`+0xE` and frees the job. Service `0x821C1A08` reports success or enters native
device/error/retry handling. The normal save path also creates native progress
UI and the saving indicator `0x821BC308`.

`0x822E6560` pumps the service through `0x821C1DA8` and accepts result two. Idle
also returns two: this alone cannot prove a particular edit was written.
Operation flags can invalidate the active save context. File-completion evidence
also does not establish crash durability or a filesystem flush guarantee.

Automatic persistence therefore needs serialized requests, lifetime-owned
snapshots, validated slot ownership and explicit generation/outcome tracking.
It must distinguish queued, RAM-only, file-completed and failed states, retaining
native error handling. Closing the UI cannot release borrowed storage while an
operation still owns it. Rapid changes must preserve the latest value without
overwriting an in-flight request.

## Integration constraints and remaining decisions

The native Options task is the entry/lifetime boundary. Main-menu focus speech
is observation-only and must not become a hidden replacement dispatcher.
Existing observers hold the runtime global region briefly; disk operations,
blocking UI work and guest behavioral calls do not belong inside them. New
behavior needs its own component and an explicit architecture contract.

Settings speech must join the existing ordered game-event queue so opening
cannot overtake the main-menu close. The task-removal callback already fans out
to battle and versus capture and must preserve those subscribers. New controller
capture must preserve popup ownership, held-button quarantine and focus loss.

Navigation (Up/Down, A, Left/Right, B), ten-percent cue volume steps, enabled
and full-gain cue defaults, separate Window Size and Render Scale controls,
and category-local confirmed reset are approved above.
Host-source findings are
retained in [host-settings.md](host-settings.md); their registry is not a menu
design or proof of live effects.

Required verification includes original setting effects and ranges, slot changes,
busy saves, rapid edits, write failure and native retry behavior, closure with
pending writes, default-only persistence, controller focus/held inputs, localized
captions, title/section/item/hint ordering, and returning to the main menu.
Listening and physical-controller usability remain manual acceptance checks.

## PC display controls: baseline SDK boundaries

These findings use the pinned SDK revision
`c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. Paths in this section are relative
to that source. They were checked separately from the native game task.

Fullscreen is borderless desktop fullscreen. `src/ui/window.cpp` registers the
Boolean and `src/ui/rex_app.cpp` applies changes through `Window::SetFullscreen`.
`src/ui/window_sdl.cpp` calls `SDL_SetWindowFullscreen`, but currently ignores
its result. `IsFullscreen()` reports the desired state. A successful registry
assignment therefore is not evidence of a successful window-system transition.
Changes belong on the UI thread, outside drawing; `CallInUIThreadDeferred`
provides the dispatch boundary.

The SDK's `resolution` option combines startup window dimensions and guest
video-mode queries. `window_width/height` and `video_mode_width/height` have
different precedence in `src/ui/window_sdl.cpp` and
`src/kernel/xboxkrnl/xboxkrnl_video.cpp`. They are not a monitor-mode list.
`video_mode_refresh_rate` also describes guest timing, not monitor refresh.
The `Window` interface explicitly leaves live resize unimplemented, and the SDL
adapter does not provide exclusive display-mode enumeration/selection. A new
Window Size control needs a bounded host API; changing guest video dimensions
as a substitute would conflate separate behavior.

`resolution_scale` and per-axis `draw_resolution_scale_x/y` in
`src/graphics/pipeline/texture/cache.cpp` change renderer scale. They are
restart-required, range one through eight, with per-axis overrides taking
precedence over the shared value. `src/graphics/vulkan/command_processor.cpp`
explicitly marks scale above one experimental. Render Scale must be named and
described accordingly; it is not verified for this game or a silent replacement
for window resolution. Both controls were approved separately on 2026-10-15.

The registry's restart-required metadata does not stage a change: it writes the
value and records a restart marker. Guest queries can subsequently read it.
The integration distinguishes desired values from the renderer configuration
captured at initialization. The three scale flags are consumed by
`TextureCache::GetConfigDrawResolutionScale` only through Vulkan and D3D setup;
staging the desired registry values therefore preserves the active renderer and
lets the existing F4 Save action retain the pending restart choice.

The SDK flag `vsync` must not be used as an ordinary PC VSync control.
`src/graphics/graphics_system.cpp` uses it to switch guest vblank generation
between the configured guest refresh and approximately 1,000 Hz;
`src/graphics/command_processor.cpp` also changes guest GPU waiting behavior.
These are game-timing effects.

Host display synchronization is selected independently in
`src/ui/vulkan/vulkan_presenter.cpp`. The existing allow flags choose supported
modes in order Immediate, Mailbox, FIFO relaxed, then FIFO. Those flags are read
when constructing the swapchain; writing them alone does not reconfigure an
existing swapchain. Tear-free presentation must exclude Immediate and FIFO
relaxed. A new presentation preference needs an explicit deferred reconfiguration
operation, preserving guest timing and the presenter's render/UI ownership.
The existing code prohibits resize processing while UI drawers are executing.

The port-config save path is separate from the native save system. Its baseline
void `SaveConfig` directly truncates the destination, does not escape strings
correctly, does not check completed writes, and skips default-only saves.

A [checked atomic persistence prerequisite](../patches/rexglue/checked-config-save.patch)
is implemented independently of the UI. It retains unrelated configuration,
reports errors, and clears an old saved override when a control returns to its
default. Its explicit-value API also saves pending restart choices without
changing the live registry. Linux synthetic tests passed under ASan/UBSan,
including default-only saving, escaped strings, invalid input, and injected
write/sync/close/replacement failures. The unpatched SDK reproduced the final
override reset defect. The [patch record](../patches/rexglue/README.md) describes
coverage and platform limits. The host settings owner now uses this API.

The host settings owner serializes immutable save snapshots and coordinates
the existing F4 Save action through desired registry values. The checked API
is the persistence primitive; the owner supplies ordering and shutdown draining. Existing numeric getters/loaders also limit
double precision; the file tests do not establish arbitrary floating-point
runtime reload equivalence.


## Implemented ownership and failure behavior

`settings_menu` owns category focus, bounded choices, asynchronous transaction
identity and category-only confirmation. `settings_catalog` builds the approved
inventory from native captions and central port resources. Confirmations open
with No selected. `settings_events` publishes copied events through the existing
ordered queue; returning from a native child reconciles confirmation lifetime
and announces the current section and item without repeating Options.

`native_settings` wraps the existing Options builder, active handler and cleanup.
It keeps the game's task lifecycle and applies original options through
`823341A0`. UI sounds retain `821DCD78` IDs zero, one and five for confirm, back
and selection. Voice adjustment retains the native random preview table and
handle stop/start calls. Original cleanup still owns vibration, title and layer
teardown. A safely abandoned invalid save suppresses only its final unchecked
save helper, rather than retrying a rejected pointer chain. A task-scoped
completion token also bypasses only that skipped request's state-five wait after
rechecking idle service/job ownership: the original idle pump still dereferences
its common UI context and cannot be treated as an inert completion query.

`native_settings_save` owns a dedicated 16-byte guest callback and immutable
six-byte snapshot. It serializes operation six and coalesces pending edits.
Completion requires the native job and terminal service cleanup, not an idle
return code alone. Transient validation failures retry only after the complete
borrowed chain revalidates. Unexpected task removal retains borrowed callback
storage until the service releases it. Unrecoverable in-flight corruption fails
closed; it is not reported as successful persistence. No active save context
reports that native options cannot yet be saved to a file.

`settings_runtime` owns UI-thread display and cue changes plus a worker for
checked config writes. Desired commit, display application and disk completion
are separate outcomes. Normal window closure drains accepted config writes
before the SDK's hard-exit path. Failures while Options is closed remain latched
for a visible and spoken notice on its next opening. Catalog revisions synchronize
external F4 edits without resetting focus or polling registry values each frame.

The display patch adds host-only window dimensions and presentation VSync.
Fullscreen success follows matched SDL events. Window Size remains pending
while fullscreen and applies after exit. VSync changes swapchain presentation
without altering guest vblank timing. Render Scale offers one through seven,
matching the texture cache's actual supported maximum; configured asymmetric
values remain representable. The menu identifies this setting as experimental
and restart-required.


## Input and sound integration guards

The replacement preserves the original per-player enabled gate at the pointer
in `829CED20`, player stride 24, byte `+15`. Four logical player mappings do not
prove a four-element controller container: `82132558` bounds against its actual
`+C/+10` vector. The replacement uses that span and the mapped low-byte index,
then the connected/held/pressed/repeated fields at controller `+C/+14/+18/+20`.
It waits for held navigation buttons to release after native child focus returns.
A disconnected controller does not count as a release.

The audio manager at `829CDCE8` has derived table `82036260`. Constructor path
`823B2B28 → 823B35E0` installs child table `82036390` at manager `+40`.
Child fields `+4/+8/+C/+10/+14` must match manager `+58` (embedded engine),
`[+3C]` (voice pool), `+29C` (group container), `[+2AC]` (metadata), and
`[+2B0]` (metadata count), respectively. Base/destructor child table `82036348`
is not accepted. The pool count/vector/cursor contract comes from `823B2FC8`;
metadata records are 12 bytes and their low-byte group index selects a 16-byte
entry. Sound validation checks these owned relationships and consumed ranges.
The native engine retains actual voice allocation, stream routing and handles;
this is not an exhaustive safety proof of that subsystem.

Manager `+284`, returned by `823B2F08`, selects resident versus streaming sound
paths. Voice preview IDs alone do not establish their branch. SFX/voice gains
at manager `+4C/+50` and selected metadata gains are checked for finite values.
Fresh read-only evidence for these chains is retained in the private
`ui-render-audio-input-review`, `ui-render-input-container`,
`ui-render-child-init-scalar` and `ui-render-audio-child-consumers` collections.
No database annotations were added.


## Native presentation implementation

The shared title/hint owner comes from global `829CBC68`, with table
`82003EAC`. Model `+0` is the numeric context value two: `82334FC0` stores it
at `82334FDC`. It is not an object pointer. The first deployed replacement
mistook this field for the owner and rejected every entry with
`settings_render_common_range`, leaving the original screen active. The initial
synthetic fixture repeated that wrong assumption. The corrected fixture uses
the real numeric constructor value and separately validates the global owner;
the old implementation fails that fixture with the observed guard.

Fresh producer/consumer tracing confirms title record `common+4C` binds to
context slot `14` through `821E8930`, with title index nine at `common+58`.
Hints record `common+94` binds to slot `67` through `821E8CE8`, with its text
ID at `common+C4`. `823CA4C8` stores each record into the context vector and
`823CE3F8` sets the widget's slot at `+A`; the draw callbacks dereference that
same vector entry. Native accesses through `common+DF` establish the required
`E0`-byte extent. These findings were rechecked after the owner test; no database
annotations were added.

The renderer retains the native widget callbacks and builds five vertical rows
from owned copies of the first label, illustration and two neutral-panel
templates. Panel constructors retain logical slots zero through four, preserving
the native focus and pulse callbacks. The original illustrated tiles, toggle,
bar and action geometry are hidden only during final drawing; their callbacks
still advance. `823C64A0` restores visibility byte `+DA` afterward. Identity
checks include native widget tables and label/panel slot fields `+100/+F4`;
no foreign widget is rewritten.

The instruction hook at `823C65F4` follows native animation and receives the
widget's rebuilt final transform. It cannot accumulate an offset into an old
transform. Renderer context comes through `823A8CE0`; the backend is context
`+4` and its viewport is `+CC`, with dimensions at `+5C/+5E`.

### Resource and bounds contracts

Loader `823A8828` copies the animation section selected by the file header's
second offset and constructs its resource through `823C7EC8`. The constructor
relocates section offsets `+8/+C/+10`, sequence-table entries and per-sequence
track pointers. Resource `+8` supplies the sequence table; `+C` supplies the UV
table. The separate auxiliary object comes through `821D02D0/821D03D0`, whose
20-byte texture descriptors contain dimensions at `+4/+8`.

Bounds function `823C93F0` takes backend/resource/output/auxiliary in `r3..r6`,
sequence in `r8`, matrix/color pointers in `r9/r10`, frame in `f1` and track at
caller stack `+54`. It overwrites `r7` before reading it. Dispatch `823C8A60`
resolves sequence `(index+2)*4`, track `(index+4)*4`, then scans descending
keyframes by byte-length units until an eligible frame. No eligible frame or
a no-op record type can leave output untouched.

Output interpretation depends on record type. Type one, handled by `823D1CE0`,
writes a complete quad. The native text consumer uses its opposing corners at
`+8/+C` and `+18/+1C`. Type four, handled by `823D4700`, writes two points on the
right edge at those offsets; equal X coordinates are valid. Treating that edge
as a nonzero-width box caused `settings_render_bounds_empty` and the subsequent
save-and-exit. Type-four record `uint16(+6)` indexes the UV table, not another
animation sequence. UV components are stored as V0, U0, V1, U1.

The adapter validates the relocated sequence/track path and current/previous
record extents before a bounds call. Type-one records require `44` bytes;
type-four records require `48` and valid UV/texture descriptors. Curve tails
must not overlap fixed fields and must contain finite values. This deliberately
fails closed on unsupported or malformed data; it is not a general resource
decoder. Synthetic tests use distinct quad corners and equal-X sprite edges,
and check the complete call arguments independently of the renderer's reads.

### Authored artwork and layout

The original screen is a card arrangement: two upper settings and three lower
settings. Sequence six selects the setting-specific illustration, including the
Throwing Practice ball icon at frame zero. It is not a generic highlight; using
that frame for every rewritten row repeats a misleading icon.

Builder `82335320` also constructs two neutral panels per setting from sequences
ten and eleven through `82336E60`. Their table is `82019DBC`. The constructor
reads row mapping `82019CA8` and stores logical selection identity at `+F4`.
Its callback queries the bound selector's virtual `+14`, advances or reverses
the first animation callback, updates the selected flag and calls `823C64A0`.
The embedded callback at `+120`, table `82019D30`, follows the same selection
identity. These panels supply native focus feedback without the illustrated
tile. `821D05C0` installs that callback in per-widget ancestry state; it does
not mutate the source template's callback slots.

Full type-four quad generation is in `823D3EE8`. It combines UV-derived width
and height with four signed warp offsets at record `+24..+32`, then applies
local scale, rotation and translation through `823D0FD0`. Local scale fields
`+34/+36` divide by 1024 and also scale the warp offsets. Measuring only the
right-edge output cannot establish the union of a multi-track panel.

Ancestry sequences sixteen, fifteen and fourteen supply opening motion, card
placement and child placement. The first label's local offset is `(0,-49)`;
the neutral panels' local origin is `(0,0)`. Parent records use unit scale;
there is no implicit half-size reduction that makes five original cards fit a
vertical list. Native focus includes separate zoom/pulse tracks, whose maximum
extent must be allowed for when sizing compact panels.

Focus composition is also checked against the native helper. Call `823CEEC8`
passes the local focus matrix in `r4`, existing ancestry in `r5`, and output in
`r3` to `82171858`. Isolated execution of that original translated helper at
both sequence-eight scale endpoints preserves ancestry translation while
scaling its basis. A reversed-operand negative control changes translation.
This establishes the panel baseline correction's focus contract; it does not
replace an interactive animation check.

Native layering is retained. Tree traversal `823C8358` visits child tracks in
descending order; `821E2438` assigns their leaf priorities. The tree path through
`821E3318` and `821E3460` passes those priorities to widget `+C` in `823C7728`.
Dispatch `823CE010` sorts through `823CDA80/823CD440`; its comparator
`823CB3A8` orders the signed key formed from widget `+A` and `+C` ascending.
The sequence-fourteen panel/glow tracks precede the label in that order, so
copied templates preserve panels behind text and the native selection priority.

Bounded inspection of all seven requested Options resources found identical
relevant sequences, neutral-panel UV entries and texture dimensions. The two
textures are 512 by 256 and 1024 by 512. This verifies the shared geometry
contract across these resources, not localized typography or live visual fit.

Native text lookup `823B60B8` is overridden only inside the owned label/title/
hint draw scopes. Bounded UTF-16 caption storage belongs to this renderer;
localization tables remain unchanged. `823A9528` draws label, value, section and
footer with the native font/style without advancing the label callback again.
Part-three text bounds expand only in width. Buttons without values use the
full label/value area; settings keep separate columns. Full-width bounds retain
the native focus scale around the panel's center, while the column-center offset
remains a world-space translation. Font measurement `823958B8`
returns maximum newline-separated width/height. Each text operation shares one
fit factor across `82396140` measurement (`r9=0`) and drawing (`r9=1`), including
font scales and line advance. This matters because `823966F4..82396700` copies
font `+304/+308` metrics into layout data, and `823A9690..823A96DC` uses the
measured height for alignment before drawing. Fitting only the draw pass caused
inconsistent wrapping/alignment. Wide descriptions and confirmation text first
use the native word wrapper at normal scale, shrinking only when its measured
height exceeds the allocated row. Ordinary labels, values and hints fit a
single line. Empty owned text explicitly clears height/count: the original
`82396140` empty-input return precedes its metric reset and can retain an earlier
caption's height. Font-manager scale and line-limit fields are restored after
each pass. Manager anchor is `829CD808`, glyph table `+C`,
selected style `+314`, stride `58`.

Hints retain the native `821EA650` draw, parts `60` (glyph style) and `5F`
(text bounds), while fitting the complete central navigation caption into its
authored line. The title retains its native style. Confirmation displays the
actual question and two selectable choices; section/footer rows are not
included in spoken item counts.

A separate private CPU-only check executes 38 original translated functions,
including the real resource loader and bounds routines, against the actual
Options resource. It reproduces the valid equal-X sprite edge and validates
the adapter's resource contracts and layout measurements: 204 by 22 label
bounds, 275.12890625 by 150 neutral-panel bounds and 50.6 row spacing. It does
not execute the graphics-device path or establish final drawing and animation.

The retained renderer fixture compiles the actual adapter with synthetic memory
and native-call boundaries under ASan/UBSan. It checks template bounds, copied
views, scrolling, callback cadence, visibility restoration, confirmation focus,
font/hint fitting and teardown. These checks do not establish the visual quality
of the original resources in a running game.


## Visual feedback: shared header and button alignment

The right-edge header is authored in the original common resource, rather than
introduced by the replacement. Shared constructor `821E8930` uses root sequence
114 for Options and other native screens; caller `82379B88` supplies title
selector zero where Options supplies nine. In resource `8CC` at the fully opaque
keyframe, the title box is `[548,1048] × [77,102]`. The red panel and shadow span
approximately `[535,1300.98] × [-1,122]`, deliberately beyond the 1281-wide
canvas. Actual translated loader/bounds execution confirms the text box. The
replacement retains this shared header, including its opening movement.

The original Options label draw `82336C28` supplies horizontal alignment one in
its call data. The battle pause menu provides a separate selectable-menu
precedent: `822720A0 → 821DD878`, setup `821DD468`, label constructor `821DDE70`
and callback `821DDF60`. That callback supplies alignment one at `821DE268` and
draws through `823A9908` at `821DE29C`, using text-bounds part `4A`. `823A9528` places that mode in font manager `+14`; `82396140` at
`82396300..82396328` uses mode one to center the line within its measured box.
The rewrite initially kept a category caption in the left setting column even
though its neutral button spans both columns. Value-free button captions now
use that full area with the native centering mode; named settings and values
retain separate boxes. Synthetic regression checks cover both focused and
unfocused widths, stable centers and confirmation buttons.


### Native visual selection activation

The neutral panel callbacks query selector virtual `+14`, implemented by
`821D7588`. Its first gate, `821D7690 → 823AB178`, searches selector `+4`'s
registered widget list for widget `+F4` matching the row, then calls widget
virtual `+10 → 821CE4E0` to read eligibility at `+E0`. Builder `82335564..78`
still registers the illustration widgets; their restored `+F4` identities and
`823A8680`'s initial `+E0=1` remain intact. Hiding their geometry temporarily at
`+DA` does not remove them or change that eligibility gate.

The query then reaches the embedded list's `8226FB80`, which requires both the
selected index at child `+10` and an enabled byte at child `+18`. Updating the
selected index alone is insufficient.

Constructor `8226FC08` owns that list at selector `+40`. Setup `821DED48` binds
it through selector `+28`, initializes seven native slots with index zero, and
disables it through the child setter at `821DEDA0..821DEDB0`. Initializer `823B93C0` stores the supplied count at `+14` and calls child index
setter `8226FB18` before this disable, establishing the initial guard values.
The original input path
`821DEDC0 → 821DEF3C..821DEF50` subsequently enables it. The managed handler
replaced that input path but initially omitted its presentation activation,
leaving every native panel visually unfocused.

The adapter now validates selector table `8200FC30`, exact embedded-child
ownership, child table `8200FC10`, consumed range, count, index and enabled flag.
It keeps index updates through `821D7628` and enables the ready, opened screen
through `821D7670 → child virtual +10 → 8226FB58`. This setter consumes no input
and emits no sound. The adapter does not invent a close-time disable: the native
closing animation retains its selection, and suspended presentation remains
hidden through the existing renderer ownership.

The lifecycle fixture covers initial activation, category movement, reset
confirmation focus, native child return and closing. An index-only negative
control fails its focus assertion. These are native-state contracts; visible
animation and contrast are not established by these automated assertions.
The later acceptance above records the scope of the owner's in-game feedback.
