# Accessibility audio research

Research dates: 2026-10-14 to 2026-10-15. Tracking: [research issue 29](https://github.com/RealAmethyst/BakuganBB-recomp/issues/29) and [implementation issue 31](https://github.com/RealAmethyst/BakuganBB-recomp/issues/31). The original proposal below is followed by source implementation and offline validation. Initial gameplay listening feedback is recorded below; detailed directional and lifecycle acceptance remains incomplete.

## Test implementation

Private build `diagnostic-20261015.1` includes this implementation. The
[deployment record](diagnostic-launch.md#battle-accessibility-test-build)
identifies the installed binary, checks, backup and remaining listening limits.

`src/core/spatial_audio` owns bounded scene snapshots, independent source identities, loop cursors and control generations. `src/runtime/spatial_audio_runtime` loads the external Steam Audio 4.8.1 library and validated mono float WAV files before opening a separate 48 kHz stereo SDL stream. Its retained pool supports 32 independent binaural effects, processing 256-frame blocks and retaining partial output between SDL requests. Invalidated captures cannot republish an old scene. Disable/re-enable generations reset the affected voice even when both changes happen between callbacks.

The approved single-cycle clips play at their original pitch, followed by silence of the same duration. Enemy clips retain the approved one-semitone pitch reduction. Private preparation resamples the approved edits to 48 kHz; it does not add another pitch or cadence change. No game-derived audio is stored in the repository. The eventual asset importer remains deferred.

`src/game/native_gate_audio` captures the verified camera and placed-card data described in [battle-field evidence](battle-field.md). It preserves source identity across vector compaction and retires removed sources before storage reuse. The installed activation policy is playback while the battlefield is visible, with silence during native pause. Listening feedback supersedes this broad policy with the narrower target below. A unique human participant is required; ambiguous participant/team configurations fail closed with named diagnostics.

The test-build settings menu is explicitly deferred to the planned settings rewrite. No extra key binding is added. Separate cue enable/volume controls exist internally for lifecycle handling and later integration; this does not claim a completed user-facing settings feature.

Offline fixtures cover stable cadence, reordered/replaced sources, stale capture rejection, buffered-output invalidation, independent effects, malformed WAV rejection and linked-stereo overload limiting. A deterministic concurrency fixture reproduces the old-controls/new-snapshot race and verifies its correction. Tests use synthetic audio, private runtime/device namespaces and no playback. SDL's own stream buffering may allocate; the implementation does not claim that the entire callback is allocation-free. Live device behavior, latency, masking and direction still require listening tests.

## Scope and boundaries

Steam Audio is intended for added accessibility sounds, including positional humming loops and short battle cues. Existing game audio remains on its current playback path. Prism retains screen-reader speech ownership. Sound selection, permitted modifications and gameplay meaning belong to the accessibility designer.

The immediate asset task is a private, listenable catalogue of game sound effects and ambience, excluding music and dialogue. A future release application must reproduce required extraction from the user's supplied game assets during asset preparation. Installer integration is explicitly deferred. No game-derived audio, generated game code or private evidence belongs in this repository or a release.

## Current output path

The inspected SDK baseline is ReXGlue `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` with the project's [ordered patches](../patches/rexglue/README.md). Source was reread for this investigation; the [timing record](timing-contracts.md) retains the existing playback investigation and latency limits.

- `include/rex/audio/sdl/sdl_audio_driver.h`, `SDLAudioDriver`: guest blocks contain six channels at 48,000 Hz and 256 samples per channel. The SDL stream is protected driver-owned state; this class has no public host-PCM mixing hook.
- `src/audio/sdl/sdl_audio_driver.cpp`, `Initialize`: opens default playback using `SDL_OpenAudioDeviceStream`. When the initially queried endpoint has at most two channels, it reopens a stereo stream for the SDK's stereo fold.
- The same file's `SDLCallback` converts already-mixed big-endian planar guest PCM into interleaved float stereo or six-channel output. It submits through `SDL_PutAudioStreamData` and returns producer credits after successful submission. Individual game sound identities are unavailable here.
- `src/runtime/native_host.cpp`, `ConstructRuntime`: the existing game path checks that a runtime audio system exists. The optional accessibility engine initializes separately in `OnPostSetup`.
- SDK `include/rex/rex_app.h` exposes `OnPostSetup`, `OnPreLaunchModule` and `OnShutdown`. `src/ui/rex_app.cpp` calls `OnPostSetup` at the end of runtime construction; `OnShutdown` runs before normal window/runtime cleanup and before its module-thread join. Cue shutdown must therefore reject concurrent guest events before releasing callback state.

These facts describe initialization and callback code, not tested device-switch behavior. In particular, the SDK's initial endpoint inspection does not establish that its own surround/stereo policy is recomputed after a device change.

### Installed ELF inspection

Read-only `readelf` inspection covered these installed artifacts:

- Host SHA-256: `26f0b8a0ac4521a0f3db26ca37deb6aaae37668d7db7d80b9c83f41c0a4e609c`.
- Runtime SHA-256: `8e1b62f05e66f1b3f5fbddd1f0a0c377a676e96a3a6d31c19eb5eb256cee987d`.

The runtime defines and exports `SDL_InitSubSystem`, `SDL_QuitSubSystem`, `SDL_OpenAudioDeviceStream`, `SDL_GetAudioStreamDevice` and `SDL_PutAudioStreamData`, including their SDL `_REAL` implementations. The host's symbol tables contain none of those names; its dynamic symbol table contains no `SDL_` names. The host requires `librexruntime.so`; neither artifact lists a shared SDL library as a dependency. No `SYMBOLIC` dynamic tag was reported.

This supports runtime ownership of the inspected SDL entry points in this build. It does not establish future linker behavior after adding host SDL calls, or behavior under runtime symbol interposition. [NativeGame.cmake](../cmake/NativeGame.cmake) also lists `libSDL3.a`, so repeat symbol/dependency inspection on the actual integration binary before relying on a common SDL instance. No executable or dynamic loader was run for this inspection.

## Proposed native integration

Follow the existing [architecture boundaries](architecture.md): verified game hooks capture semantic events and copied transforms in `src/game`; SDK-independent voice commands and lifecycle policy belong in `src/core`; Steam Audio and SDL ownership belong in `src/runtime`. Do not route PCM through Python speech transport or the guest XAudio producer-credit queue.

The bounded first option is a project-owned Steam Audio mixer feeding a separate stereo SDL logical playback stream. SDL explicitly permits multiple logical opens on one physical device and can follow default-device changes. This avoids changing the guest conversion/mixing path, but does not promise sample-exact synchronization with game sound. Source references: [SDL device contract](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDevice), [stream creation](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream). The pinned SDK SDL headers contain these APIs; its submodule identity is `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4`.

A shared output bus is an alternative if later requirements demand unified device selection or output timing. That requires an explicit SDK lifetime/mixing interface; reaching through the current driver's protected stream is not such an interface. The current implementation uses the separate-stream option.

Steam Audio 4.8.1 was the latest official release observed on the research date. Pin a matching SDK/header/library identity; Linux x86-64 uses `phonon.h` and `libphonon.so`. An external source build is also possible, but its selected FFT backend, MySOFA and FlatBuffers dependencies still need reproducible build/staging work. Optional GPU/ray-tracing components are unnecessary for basic binaural cues. See the [4.8.1 release](https://github.com/ValveSoftware/steam-audio/releases/tag/v4.8.1), [C API setup](https://valvesoftware.github.io/steam-audio/doc/capi/getting-started.html) and [pinned build configuration](https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/core/CMakeLists.txt).

### Voice processing

Use a retained context/HRTF and a bounded pool of independent voice states. Each playing voice needs its own sample cursor and binaural-effect history. A source's mono or stereo PCM enters `iplBinauralEffectApply`; both stereo input channels represent the same spatial location. The output is stereo and cannot alias the input. Sum processed voices into an accessibility bus, then interleave for SDL. Steam Audio buffers use deinterleaved 32-bit floats. See the [binaural API](https://valvesoftware.github.io/steam-audio/doc/capi/binaural-effect.html) and [buffer API](https://valvesoftware.github.io/steam-audio/doc/capi/audio-buffers.html).

The application owns decoding, resampling, clip scheduling and loop metadata. Preserve DSP state across loop wraps; reset a recycled voice and drain effect tails when finite input ends. Preserve an unmodified audition copy before applying any approved edit or loop recipe. Bilinear HRTF interpolation is a candidate for smoothly moving loops, with a CPU tradeoff to measure against selected sounds and voice counts. [Valve's guide](https://valvesoftware.github.io/steam-audio/doc/capi/guide.html) explains interpolation and direct effects.

HRTF creation is expensive and cannot run concurrently. Load it outside processing callbacks. A default HRTF and custom SOFA data are supported. Additionally, the pinned binaural implementation can reallocate its convolution state inside `apply` if a replacement HRTF has a different impulse-response length. Prepare replacements outside the processing path instead of assuming live swaps are allocation-free. See the [HRTF API](https://valvesoftware.github.io/steam-audio/doc/capi/hrtf.html) and [4.8.1 implementation](https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/core/src/core/binaural_effect.cpp).

### Timing and ownership

Use verified semantic start/stop events and bounded copied position updates. Lifetime identities must prevent reused game objects or stale events from reviving old loops. Separate event delivery from latest transform snapshots, with explicit overflow/invalidation handling; losing a stop command must not leave a persistent hum.

Keep game-memory access, decoding, file I/O and resource creation outside audio processing. One processing owner controls mutable voice/effect state. SDL callback requests have variable byte counts and may overestimate because of conversion/buffering; adapt them to fixed DSP blocks with retained remainder. Do not destroy a stream from its callback. See the [SDL callback contract](https://wiki.libsdl.org/SDL3/SDL_AudioStreamCallback) and [Steam Audio integration guidance](https://valvesoftware.github.io/steam-audio/doc/capi/integration.html).

At 48 kHz, 256 and 512 samples represent 5.33 and 10.67 ms respectively. They are candidate processing sizes, not latency guarantees. Event capture, buffering, device output and HRTF response contribute additional delay. Measure bounded processing time and queue depth before choosing a size; listening tests must establish whether battle cues arrive usefully.

## Transforms and optional acoustics

Steam Audio uses right-handed coordinates: positive X right, positive Y up and negative Z forward. `iplCalculateRelativeDirection` accepts source/listener world positions and listener ahead/up vectors. Transform and validate game data before using it; coordinate conventions and valid listener orientation cannot be inferred from readable memory. See the [geometry API](https://valvesoftware.github.io/steam-audio/doc/capi/geometry.html).

Basic binaural direction requires no arena mesh, simulator, probes or baking. Distance attenuation is a separate effect/policy. Physically simulated occlusion and reflections require verified scene geometry/materials and additional ownership/threading contracts. Defer those dependencies unless the accessibility design calls for them; direction alone does not imply physical distance attenuation or obstacle behavior.

The initial audio investigation established no battle capture anchors. Subsequent original-code tracing is recorded in [battle-field evidence](battle-field.md); existing menu sound-request diagnostics are not battle anchors. World units have no verified metre conversion, so directional processing does not infer a physical attenuation model.

## Listening feedback and next-session priority

User-controlled testing of `diagnostic-20261015.1` on 2026-10-15 confirms the
cue and new battle speech operate. The onset still feels too sharp despite the
earlier entrance fade, and the volume makes speech difficult to hear. A softer
fade-in and per-cue volume controls are requested for the next session; no new
fade duration or default volume has been chosen.

The revised activation target is only while the player has gameplay control or
the opponent is moving the ball. Bakugan selection and the state after landing
on a card must be silent. Native control/ball-motion phase consumers still need
to be traced; field visibility alone does not establish those states.

The next-session priority is the [settings UI rewrite, issue 33](https://github.com/RealAmethyst/BakuganBB-recomp/issues/33):
inventory existing options, navigation and persistence/reset behavior, then
replace the presentation with extensible categories including Accessibility.
The designer will settle unresolved category and navigation choices. Per-cue
enabled/volume settings belong in Accessibility, Audio. Onset and phase changes
remain in issue 31. These adjustments were deferred; the installed build is
unchanged. General listening confirmation does not establish every directional,
pause, removal or speech-ordering case.

## Design and verification gates

The designer has selected the clips, edits, slower repetition and camera listener. Initial visible-field activation is superseded by the listening feedback below. Distance mapping and later cue priorities must not be inferred from the implementation. Each spatial sound ultimately needs its own enabled and volume settings under Accessibility, Audio, with normal persistence/reset behavior; the owner explicitly deferred that menu work for this test build.

Remaining integration checks must cover source invalidation, event overflow, finite transforms, resource teardown and bounded callback work. Inspect final linked dependencies and preserve existing guest audio paths. Automated rendering does not establish directional clarity, masking, audible onset or gameplay usefulness; those require user-controlled listening tests.

### Offline SDK validation

The official Steam Audio 4.8.1 archive was verified against its published SHA-256, `4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449`. Its Linux x86-64 library has SHA-256 `faf7c4d69e0f104366f6553cda38d3f946f761f7db223baf12663f6fbf2387da`. An original synthetic harness compiled with warnings as errors and rendered into memory in isolated namespaces with private device/runtime directories, no sound devices and a cleared environment.

Both 256- and 512-sample blocks passed at 48 kHz. Checks establish left/right ear-energy ordering, independent retained voice histories across loop wraps, partial final blocks, tail completion and reset silence. Fixed-HRTF processing made no calls to the supplied allocation callback after setup; this does not cover every third-party allocation path or HRTF replacement. The default front HRTF was not exactly symmetric, with approximately minus 2.085 dB left/right energy ratio. Basic binaural processing needed only the SDK library and standard system libraries in the isolated runtime.

These are offline DSP results on the current Linux x86-64 host. SDL output, device lifetime, realtime deadlines, listening suitability and gameplay behavior remain unverified. Private evidence collection `gate-card-audio-20261014/steam-audio` retains the original synthetic harness, exact commands, logs and notices.

### Gate-card audition edit

[Issue 31](https://github.com/RealAmethyst/BakuganBB-recomp/issues/31) tracks the selected `g_menu` bank, zero-based stream 3, for a repeated gate-card cue. The original mono 44,100 Hz source has 15,636 frames and no native loop metadata. Its SHA-256 is `360b25242f13b0c48c895cba40cd6585593548a6a63e696c7d9de05595b7101c`. The designer reported an opening click and requested an edit shared by friendly and enemy cards. The earlier enemy pitch choice is minus one semitone.

The first edited audition candidate removes 1,544 opening frames (about 35 ms), then applies raised-cosine entrance/exit fades of 353/220 frames. Enemy playback uses libsamplerate 0.2.2 best-quality sinc resampling at the inverse of `2^(-1/12)`, with a 44-frame endpoint taper to remove resampler edge ringing. This lengthens the enemy cycle by approximately 5.95 percent. Both outputs use common gain `0.7247231356142394` to retain headroom. A separate synthetic 440 Hz check of this pitch path measured a 0.0242-cent deviation from the target.

Private float WAV previews repeat each edited cycle twenty times. File checks establish finite samples, peaks below full scale, zero boundary steps and exact repetitions. Original audio and previous previews remain preserved outside the repository. The designer confirmed that this edit sounds much better on 2026-10-15, then selected repetition half as often with unchanged pitch. The revised preview adds silence equal to each edited clip length: 14,092 frames for friendly cards and 14,929 for enemy cards. Onset intervals are therefore 28,184 frames (about 639 ms) and 29,858 frames (about 677 ms), respectively. Twenty repetitions retain the approved sound samples byte for byte, with verified zero-filled gaps, finite samples and peaks below full scale. The designer approved both slower previews by listening on 2026-10-15. This approves the edited sound, pitch distinction and repetition cadence; spatialized gameplay had not yet been tested at that preview milestone. Subsequent integration and listening feedback are recorded above.

## Loop scheduling reference

Read-only inspection of the local Time Stranger mod on 2026-10-15 provides a scheduling reference, not a Bakugan game contract. In `DSTS.Accessibility/Core/SpatialAudioEngine.cs`, `FillMonoFrame` (line 695) treats a positive interval as total onset-to-onset time. For intervals longer than a clip, it inserts interval minus clip length as silence (line 765). Its continuous wall cues instead use an overlapping endpoint path (lines 717–740). Thus doubling the total cycle, rather than merely doubling an existing gap, halves repetition frequency without changing sample playback or pitch.

Useful ownership practices are independent of its particular sounds: `SubmitFrame` publishes source identity, position, interval and listener data together (line 373); the processing thread captures one coherent snapshot (line 518). `Core/SpatialAudioSources.cs` preserves surviving owners' slots (line 24) and assigns new voice identities on replacement (line 48). A changed voice or sound resets cursor and DSP history (`SpatialAudioEngine.cs`, lines 551 and 673), while ordinary updates retain cadence. A reset generation also carries a stop/resume through a gap between audio callbacks (lines 447 and 528).

The reference's `tests/SpatialAudio/Program.cs` contains fixtures for slot reorder/replacement, stop/resume between callbacks, stable onset cadence, map transitions, arbitrary read sizes and continuous seams (lines 75, 96, 118, 143, 182 and 198). These tests were inspected, not executed. Its Windows output, distance/front-back treatment, fixed timing and clip-truncation rules are not adopted as Bakugan behavior. No reference code was copied or modified.

## Licenses and reproducible assets

Steam Audio code uses Apache 2.0. Retain its license and applicable notices; audit the selected SDK build's separate dependencies and HRTF notices before distribution. Do not describe all bundled binaries as Apache-licensed. Sources: [pinned license](https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/LICENSE.md), [third-party notices](https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/core/THIRDPARTY.md).

Keep extracted audio and private manifests outside source control. Preserve stable source identity, decoder version, original format/rate/channels and loop metadata where verified. Future selected cues should derive from those identities and explicit transformation recipes. The eventual release app must reproduce extraction from locally supplied game assets; shipping the audition files is not the deferred installer plan.

## Extraction findings

The private conversion covers all 55 non-language resident sound banks in the main archive: 768 streams, comprising 767 mono sources and one stereo source. Eight separate ambience streams were also converted. The catalogue contains 776 single-pass 32-bit float WAV files, retaining original rates and channels. It does not normalize, resample, crossfade or otherwise select an accessibility sound design.

Filenames retain the bank name and zero-based substream index. The engine's resident sound table has 499 records, but each record refers to a context-dependent loaded bank slot and substream. The extraction does not invent globally stable sound IDs from bank order. Private manifests retain source hashes, archive extents, original format bytes, encoded-payload hashes, decoder identity, decoded sample counts and corrected loop metadata.

### Verified container consumers

The [archive record](localization-consumers.md#requester-and-archive-entry-selection) describes the main index. Its parser and positional-read evidence were reread; all 6,373 archive entry extents were validated before bounded reads of the sound records. Original inputs remain unchanged.

Fresh original-instruction tracing establishes the resident bank path: manager global `829CDCE8`, derived constructor `823B20D8`, completion `823B2D18`, adapter table `82036390` slot `+8`, then `823B4390`, `82120D00`, `8212BB78` and parser `8212B498`. These are evidence anchors on the supported executable, not an implemented accessibility capture chain.

Parser `8212B498` checks `PASD`, reads the big-endian stream count at bank `+8`, and walks a directory starting at `+20` with stride `20` (hexadecimal). Each row supplies bank-relative format/data offsets at `+0/+4` and corresponding byte sizes at `+10/+14`. These values reach `8212AF70` as format pointer, payload pointer, format size and payload size. That consumer recognizes XMA2 tag `0166` and requires the complete `34`-byte format. All selected streams match that layout. Other row fields are retained privately without assigning unverified meanings.

For offline decoding, an original utility wraps each unchanged format and payload in the PASX layout accepted by [vgmstream r2117](https://github.com/vgmstream/vgmstream/blob/r2117/src/meta/pasx.c). The official Linux decoder archive was checked against its published SHA-256, `2f98c77f756079f63fbd119939067f1ed461d77e70993bc4cc372736d859c84a`. Decoder `-L` retains loop metadata while disabling repeated playback; `-W 4` preserves float samples. The [CLI source](https://github.com/vgmstream/vgmstream/blob/r2117/cli/vgmstream_cli.c) establishes those options.

### Verification and sample limits

All 776 conversions completed with zero decoder failures or stderr warnings. Checks cover RIFF/chunk bounds, finite float samples, rate/channel/frame agreement with decoder metadata, and loop extents. All 768 wrappers preserve their original format and payload bytes exactly. Original synthetic fixtures accept valid archive/bank layouts and reject malformed signature, count, path, bounds, format and overlap cases. Decoder processes have isolated namespaces, private device/runtime directories, cleared environments and file output only.

There are 295 distinct non-silent resident-effect WAVs after exact file-hash deduplication. Twelve bank entries decode to the same silent content; they remain in the complete catalogue, but the unique-effects playlist skips them. Fifteen effects and all eight ambience tracks retain loop metadata. No sound was played by the coding agent.

An initial PCM16 audition pass saturated decoded peaks above full scale. It was preserved privately as a backup and replaced by float WAV output: 650 of the 776 decoded files have peaks above unity, with a maximum of approximately 1.529. No normalization was applied. Separate system-FFmpeg decoding confirmed the shared silent payload and non-silent mono/stereo controls; this is a second wrapper/tool check using the same codec family, not an independent codec-conformance result.

Twenty-nine effect sample counts differ from raw XMA PlayLength by between minus 352 and plus 445 samples; every WAV agrees with the decoder's corrected count. [vgmstream's XMA correction](https://github.com/vgmstream/vgmstream/blob/r2117/src/coding/coding_utils.c) accounts for encoded sample/skip information. Keep raw and corrected values separate. Native `8212AF70` also masks certain playback/loop lengths to 128-sample multiples and clamps loop bounds. The catalogue is not a claim of console-exact trimming, looping or audible equivalence.

### Classification boundary

The streaming lookup `823B28F0` uses root filenames below index `43` hexadecimal and localized filenames thereafter. `823B2B28` obtains the resident table, 16-byte streaming-name records and 12-byte parameter records from separate container children. The parameter's concurrency-group field is not a verified music/effect label.

Thirty-two remaining root streams need content classification: five `j_` entries, one `b_powbtl` entry and 26 `vmc` entries. A discrete request for one `j_` entry is verified at `82368394`, but event-triggered playback does not establish whether its content is musical. No verified consumer establishes that all `vmc` entries are dialogue or that `b_powbtl` is an effect. They are excluded from the effects/ambience catalogue; do not describe that catalogue as every possible nonmusical sound in the game.

Private evidence collection `accessibility-audio-20261014` retains the original exports, format audits, scripts, conversion logs and manifests. No RE database annotations were added. Extracted banks, wrappers, WAVs, playlists and manifests remain outside the repository; the public research record contains no audio payloads.

## Windows portability evidence, 2026-10-17

The optional runtime uses SDL3's shared-library loader and native filesystem
paths. The pinned SDL revision `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4`
converts UTF-8 library paths to UTF-16 on Windows; its Linux implementation
retains `dlopen` with `RTLD_NOW | RTLD_LOCAL`. Steam Audio function pointers
retain the header's Windows calling convention.

The official Steam Audio 4.8.1 archive's SHA-256 is
`4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449`.
Read-only inspection of `lib/windows-x64/phonon.dll`, SHA-256
`ca3dbc01dbc24492717011e80f6a51404ca143ae344ca660971d2c983f1e058d`,
confirmed AMD64 and all eight API exports consumed by this engine. Normal
imports are KERNEL32, ole32 and ADVAPI32. Delay imports include OpenCL,
GPUUtilities and TrueAudioNext; the current binaural engine does not request
GPU devices. This is binary/source inspection, not Windows runtime validation.

The SDK archive includes `THIRDPARTY.md`; retain it and the
[upstream license](https://github.com/ValveSoftware/steam-audio/blob/v4.8.1/LICENSE.md)
with any private installation. Do not describe all bundled code as Apache-only.
The [Windows build guide](building-windows.md) describes the optional build
boundary; automatic staging of private cue resources remains deferred.

An isolated Linux ASan/UBSan run using actual Steam Audio and the pinned SDL
loader passed the existing synthetic DSP/failure checks plus Unicode library
and WAV filenames. The fixture linked shared SDL for loading and replaced audio
device operations with aborting stubs; no output-device call occurred. This
preserves offline coverage without establishing Windows speech or audio output.
