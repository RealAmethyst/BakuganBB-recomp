# Diagnostic host and launcher

These original integration sources accompany a private statically recompiled build. They contain no game executable, assets or generated game translation. Preparing the generated build is a separate private integration step; this directory is not a downloadable game or a complete build distribution.

## Components

- `native_host.cpp`: ReXApp hooks for startup phases, error events and a missing-audio-system guard. Compile privately with the generated image configuration and the pinned SDK's ReXApp/SDL entrypoint sources.
- `native_timing.cpp`: seven passive wrappers around verified generated entrypoints, forwarding their original implementations and recording update deltas, movie lifecycle, scheduled media deadlines and menu transition/sound requests. Compile alongside the host; requires the timing SDK patches recorded in the patch manifest.
- `launch.py`: Linux process supervisor, explicit runtime paths, exclusive launch lock and private logs. It runs the game only through its main entrypoint, never when imported.
- `prism_speech.py`: Prism screen-reader allowlist; no standalone TTS fallback. The private Linux build requires a compatible `lib/libprism.so` beside the launcher.
- `messages.en.json`: central diagnostic wording. Game captions and menu narration remain outside this milestone.

The private deployment places the host and runtime libraries under `bin`, existing private runtime content under `data`, and writable configuration/log/cache files under `user-data`. These locations are relative to the launcher. Do not place any of those private game inputs or generated native artifacts in this source tree.

## Checks and limits

`launch.py --check` checks the expected installation files without loading Prism, speaking or running the game. It does not establish hardware, speech or gameplay readiness.

The original `test_launch.py` and `test_prism_speech.py` fixtures exercise process supervision and a mocked Prism API. Run them in an isolated environment with private runtime/configuration directories and no live desktop, accessibility bus or audio resources. Native Prism integration checks additionally require an isolated fake bus, never the active screen reader.

`test_native_timing.cpp` uses original fake guest functions to verify complete CPU-context forwarding, nested lifecycle calls and floating-point environment preservation. It does not execute the game. The host enables optional SDK presentation diagnostics; neither those queue observations nor movie deadlines establish physical display/audio synchronization. The [timing record](../research/timing-contracts.md) owns the verified hooks and measurement limits.

The [diagnostic-launch record](../research/diagnostic-launch.md) owns build identities, verified evidence and remaining boundaries. No game menus are narrated yet. Source and candidate changes were prepared with Codex assistance; no upstream contribution or publication is implied.
