# Host runtime settings reference

## Scope and baseline

This incidental investigation covers the ReXGlue F4 overlay. The owner clarified
on 2026-10-15 that issue 33 targets the original settings screen reached from the
game main menu. The F4 overlay is outside that rewrite. Native settings research
and approved decisions belong in [settings.md](settings.md).

Source inspection on 2026-10-15 used clean SDK revision
`c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`, the project's ordered SDK patches,
and project revision `618ab4f`. Read-only symbol inspection used the installed
`diagnostic-20261015.1` host, runtime and Xenos plugin. No game or speech backend
was loaded. No RE database annotations were added.

SDK references below are relative to that pinned source. Source inspection is
not listening, controller or gameplay verification.

## Current menu and ownership

`src/ui/rex_app.cpp:397–427` registers four system bindings: debug overlay F3,
console Backtick, settings F4 and achievements F7. `src/ui/keybinds.cpp:171–198`
registers their string values under `Input/Keybinds/System`, with live setters.

F4 creates or resets a `unique_ptr<SettingsDialog>`. Its constructor attaches
an ordinary ImGui dialog; it neither pauses the guest nor reports a system-UI
transition. The window has no close widget. Escape cancels key capture only.
Closing and reopening reconstructs the search text and category selection.

`src/ui/overlay/settings_overlay.cpp:183–512` builds an alphabetical category
tree from the entire registry every draw. The root selects all entries; selecting
a parent with direct entries also includes descendants. Search ignores category
selection. Names are lowercased for matching, but descriptions are not, despite
the case-insensitive-search comment.

Non-binding rows display internal setting names, with descriptions and lifecycle
explanations in mouse tooltips. Widgets are checkboxes, allowed-string dropdowns,
numeric editors, string editors and command buttons. Init-only entries are
disabled. Binding rows display descriptions, current binding, Rebind and Reset;
controller-emulation bindings are disabled when `mnk_mode` is false. Duplicate
bindings produce a visual warning. No general reset action exists.

## Inventory boundary

The three installed ELF files contain 156 distinct defined `FLAGS_*_storage_`
identities. Of these, 155 match static definitions in the pinned SDK; the remaining
`vulkan_timing_diagnostics` comes from the project's timing patch. Four system
bindings register dynamically without storage symbols. This is a static inventory
of definitions and registration paths, not a live enumeration of 160 visible rows.
Conditional compilation and registration lifetime remain relevant.

The existing categories and option families are:

- `Display`: resolution preset, guest width, height and refresh rate.
- `UI/Window`: fullscreen, startup width and height, monitor and video driver.
- `UI/Presenter`: presentation thread, letterboxing, safe areas, output effect,
  dithering, overscan cutoff and render-pass clearing. The compiled symbol
  inventory does not include optional FidelityFX-specific controls.
- `UI/Vulkan`: adapter selection, presentation modes, validation and device
  feature preferences/requirements.
- `GPU` and `GPU/Vulkan`: resolution scaling, VSync, filtering, shader compilation
  and caching, render-target/texture behavior, readbacks, primitive conversion,
  diagnostics and compatibility controls. These must remain reachable; their
  appearance in a registry is not proof that changing them live is supported.
- `Audio`: `audio_mute`, `audio_maxqframes` and `ffmpeg_verbose`.
- `Input`: backend, guide passthrough, controller mappings file, mouse/keyboard
  emulation, mouse input and sensitivity.
- `Input/Keybinds/Controller`: 25 keyboard/mouse-to-controller bindings: four face
  buttons, two triggers, two shoulders, both sticks' directions and presses,
  four D-pad directions, Back, Start and Guide.
- `Input/Keybinds/System`: the four host overlay bindings described above.
- `Kernel`: language/country, license mask, headless mode, thread scheduling
  overrides and kernel/debug behavior.
- `Clock`: time-scaling bypass and raw-clock selection.
- `Filesystem`: permission for writes relative to game data.
- `Memory` and `crt`: memory guards/diagnostics and guest C-runtime heap controls.
- `Runtime`: content, user-data, update, cache and metadata locations.
- `Log` and `Perf`: verbosity, output/rotation and performance CSV output.
- `Console`: the `echo` and `find` commands, rather than persistent user options.

The SDK's original defaults, constraints and setter identities remain the
baseline. A replacement presentation must not recreate that registry with a
separate set of inferred defaults or discard unfamiliar registered entries.

## Application, persistence and reset

Edits call `SetFlagByName`; Save to config calls `SaveConfig` separately. Closing
the overlay neither saves nor undoes edits. Binding Reset immediately uses the
entry's declared default and still requires saving for persistence.

`src/core/cvar.cpp:298–343` validates constraints, rejects init-only changes
after finalization, calls the setter, tracks restart-required names and invokes
callbacks. A successful setter is not proof that the consumer rereads its value.
For example, `audio_maxqframes` is declared with the default live lifecycle, but
`src/audio/audio_system.cpp:49–67` reads it while constructing the audio system
and semaphores. A restart is needed for that consumer. The new presentation must
not promise a live effect from the annotation alone.

Guest mute is read during audio submission in
`src/audio/sdl/sdl_audio_driver.cpp:198`. Fullscreen has a host change callback
in `src/ui/rex_app.cpp:353`. Letterbox and safe-area values are read during
presentation in `src/ui/presenter.cpp:908–960`. These are directly inspected
consumers; the investigation has not traced every advanced GPU setting's effect.

The application selects `user-data/battle_brawlers.toml` relative to the
installation. `src/core/cvar.cpp:538–568,639–711` serializes non-default values
and loads TOML. Source inspection identifies these persistence limitations:

- Saving an entirely default registry returns without writing; an older file can
  therefore retain values that were just reset.
- String serialization surrounds values with quotes without escaping embedded
  quotes, backslashes or line breaks.
- Saving truncates the target directly, returns no success result and does not
  check the completed stream write. The UI cannot truthfully announce success
  from this API alone.
- Rewriting serializes registered non-default values, not a merge retaining
  unrecognized configuration keys or comments.

These are source findings, not reproduced disk-failure tests. Persistence work
must cover default-only saves, string round trips, preservation of unrelated
configuration and failure reporting before claiming reliable saving/reset.

## Input and speech contracts

Settings has no speech observer and does not enable keyboard navigation.
`src/runtime/native_ui.cpp` observes only keyboard and message dialogs.
The baseline speech adapter rejected a `settings` source; the in-game rewrite
now accepts its copied settings and confirmation events.
Its persistent initial-item/hint queue and label/value/description/position
format can be extended after defining the settings event contract.

The host's active-input callback in `src/ui/rex_app.cpp:251–258` gates guest
input by ImGui mouse capture whenever a built-in overlay exists. It is not
exclusive settings ownership and does not stop guest simulation.

The existing `popup-controller.patch` maps only D-pad Left/Right and South/A
for focused native message boxes. It does not supply settings entry, Up/Down,
Back or stick navigation. Its capture tokens, focus checks, child-dialog
ownership and held-button release quarantine must survive any generalization.

The drawer forwards keyboard input without marking it handled; system bindings
remain reachable. `src/ui/keybinds.cpp:211–223` checks neither modifiers nor
text-edit/capture state. Rebinding to an existing overlay hotkey can therefore
invoke that overlay before the settings draw captures it. This is a source-derived
risk, not an interactive reproduction.

`OnCreateDialogs` is available after system binding registration, but there is
no settings factory override. `SetupOverlays`, `settings_overlay_` and its config
path are private. A focused SDK extension seam is preferable to replacing the
whole application setup. Ownership must remain explicit: `ImGuiDialog::Close()`
schedules `delete this`, which is incompatible with leaving the existing
settings `unique_ptr` owning the same instance.

## Existing accessibility-audio integration

`SpatialAudioRuntime::Configure` already accepts a cue, enabled state and volume.
`SceneMailbox::Configure` rejects non-finite values and volumes outside zero to
one, publishes coherent controls and advances reset epochs for enabled changes.
The renderer applies independent friendly/enemy gains. Both current defaults are
enabled with gain one. The in-game settings owner now persists and applies those four controls.

These controls belong under Accessibility, Audio as required by issue 33. Guest
mute, cue gains and screen-reader speech have separate owners. Connecting cue
settings must preserve that separation. Volume increments and any new default
gain are design decisions, not implied by the engine's numeric range.

Softer onset and narrower gameplay-phase activation remain in issue 31. They
require their own approved sound behavior and verified native phase consumers;
this host-menu investigation does not establish those consumers.

## Scope limit

No host-overlay redesign is planned under issue 33. The source findings above
may inform port-owned settings integration, but are not evidence for the game's
own options, save path or input/lifetime behavior. Runtime integration and its verification limits are recorded in [settings.md](settings.md).
