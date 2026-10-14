# Diagnostic host and launcher

These original integration sources accompany a private statically recompiled build. They contain no game executable, assets or generated game translation. Preparing the generated build is a separate private integration step; this directory is not a downloadable game or a complete build distribution.

## Components

- `native_host.cpp`: ReXApp hooks for startup phases, error events and a missing-audio-system guard. Compile privately with the generated image configuration and the pinned SDK's ReXApp/SDL entrypoint sources.
- `native_timing.cpp`: seven passive wrappers around verified generated entrypoints, forwarding their original implementations and recording update deltas, movie lifecycle, scheduled media deadlines and menu transition/sound requests. Compile alongside the host; requires the timing SDK patches recorded in the patch manifest.
- `native_dialogs.cpp`: two passive wrappers record keyboard dispatch and native result queries without reading guest pointers or text.
- `native_ui.cpp`: installs optional runtime keyboard and message-box observers before runtime threads start and transports actual UI events to the launcher. Compile with the pinned SDK's existing nlohmann JSON include directory and both dialog patches.
- `native_game_ui.cpp` and `game_events.toml`: observe verified menu/title/save-picker/popup transitions, native text consumers and readable startup movie pictures at successful submission. Include the TOML in the pinned private codegen configuration; compile the adapter with its generated hook calls. Constructor/removal wrappers preserve native lifetime. [Localization research](../research/localization-consumers.md) records the original behavior and exact contracts.
- `guest_ui_memory.h` and `game_speech_queue.cpp`: strict bounded text copying and value-only FIFO output outside guest/runtime locks. Requires the guarded-read SDK patch. Mapping safety does not establish game-object ownership; semantic hooks supply that context.
- `launch.py`: Linux process supervisor, explicit runtime paths, exclusive launch lock and private logs. It runs the game only through its main entrypoint, never when imported.
- `keyboard_speech.py` and `screen_speech.py`: validate native events and apply speech ordering through the existing Prism owner. `speech_text.py` supplies shared text preparation. Entered text and captions are excluded from persistent launch logs.
- `prism_speech.py`: Prism screen-reader allowlist; no standalone TTS fallback. The private Linux build requires a compatible `lib/libprism.so` beside the launcher.
- `messages.en.json`: central diagnostic, text-editing and position wording, plus directly verified artwork-only captions. Other game content comes from its actual localized consumers. The verified A/B/X/Start glyph substitutions apply only to game-rendered text.

The private deployment places the host and runtime libraries under `bin`, existing private runtime content under `data`, and writable configuration/log/cache files under `user-data`. These locations are relative to the launcher. Do not place any of those private game inputs or generated native artifacts in this source tree.

## Checks and limits

`launch.py --check` checks the expected installation files without loading Prism, speaking or running the game. It does not establish hardware, speech or gameplay readiness.

The original `test_launch.py` and `test_prism_speech.py` fixtures exercise process supervision and a mocked Prism API. Run them in an isolated environment with private runtime/configuration directories and no live desktop, accessibility bus or audio resources. Native Prism integration checks additionally require an isolated fake bus, never the active screen reader.

`test_native_timing.cpp` uses original fake guest functions to verify complete CPU-context forwarding, nested lifecycle calls and floating-point environment preservation. It does not execute the game. The host enables optional SDK presentation diagnostics; neither those queue observations nor movie deadlines establish physical display/audio synchronization. The [timing record](../research/timing-contracts.md) owns the verified hooks and measurement limits.

`test_keyboard_speech.py` covers original synthetic prompts, character edits, selections, positions, lifecycle ordering and malformed events. The supervisor fixture checks name/caption redaction. `test_native_dialogs.cpp` checks original-context forwarding and floating-point preservation without dereferencing its deliberately invalid text pointers. `test_native_ui.cpp` checks actual JSON serialization and invalid-UTF-8 failure. The [runtime keyboard fixture](../research/patches/rexglue/keyboard-dialog-regressions.py) uses pinned ImGui core with synthetic input and no platform backend. These checks cannot establish physical input routing or listening usability.

`test_native_game_ui.cpp` exercises original synthetic event/lifetime cases, native forwarding, argument and floating-point preservation, delayed text, ownership rejection, cached-draw read counts, replaced movie requests and exact picture ownership through lookahead, copying, reordering and queue reuse. `test_game_speech_queue.cpp` covers worker ownership, FIFO, concurrent producers and fail-closed faults. `test_event_hooks_codegen.cpp` and `test_event_hooks_runtime.cpp` emit, compile and execute original synthetic instructions to check hook order and live context-backed/localized register values. They contain no game instructions or generated game source.

`test_screen_speech.py` checks delayed first-item/hint queuing, native lifetime, exact locale lookup, indices and literal host text. Main-menu, title, save-picker and native popup state changes are event-driven; text callbacks still run as part of native drawing, but cached draws perform no guest-memory reads. Save details use the final native formatted text, including composite and scrolling hints. The separate SDL host-popup adapter supplies D-pad Left/Right and A, preserves the requested initial choice and quarantines held presses until release. Native game popup controls remain owned by the game. The native controller-battery warning remains outside this coverage.

The [diagnostic-launch record](../research/diagnostic-launch.md) owns build identities, verified evidence and remaining listening boundaries. Source and candidate changes were prepared with Codex assistance; no upstream contribution or publication is implied.
