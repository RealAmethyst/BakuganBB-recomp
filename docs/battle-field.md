# Battle field and prebattle accessibility

Investigation date: 2026-10-15. Tracking: [gate-card audio](https://github.com/RealAmethyst/bb-recomp/issues/31) and [battle speech](https://github.com/RealAmethyst/bb-recomp/issues/32). Addresses refer to the [supported executable](executable-baseline.md). This record distinguishes original-code contracts from pending gameplay verification.

## Selected behavior

The approved gate-card cue is the edited `g_menu` stream 3 described in the [audio record](accessibility-audio.md). Own cards use its original pitch; other cards use the approved one-semitone-lower version. The approved repetition period includes silence equal to the sound's duration. The listener follows camera position and facing. The installed build uses the initial choice of playback whenever the battlefield is visible, with silence during native pause. Subsequent listening feedback supersedes that choice: play only while the player has gameplay control or the opponent is moving the ball, with silence in Bakugan selection and after landing on a card. Those native phase contracts remain untraced; the renderer gates below establish visibility only. Implementation is deferred until the next session, whose priority is the [settings UI rewrite](https://github.com/RealAmethyst/bb-recomp/issues/33).

Field speech prioritizes selected cards and Bakugan. Turn announcements are excluded because the native announcer already supplies them. The prebattle announcement identifies the displayed opponents using their verified game-owned names; a portrait need not contain a written caption to identify its character. A new audio settings menu is explicitly deferred to a planned settings rewrite for this temporary test milestone.

## Card scene and lifetime

Render task `82216F78` loads global `829CBEC4`, adds `20`, then invokes scene renderer `82216760`. Constructor `8220CE28` constructs that scene through `82218AF0`; destructor `8220D140` clears the global and destroys the scene through `82217C50`. Capture can use the renderer's actual incoming `r3` receiver without retaining a global pointer chain.

The renderer requires scene byte `24E0` nonzero and `24E1` zero. Card rendering additionally requires `24DF` nonzero. These are rendering conditions, not independently established aiming or rolling phases. Render-task state `1` suppresses rendering; state `2` requests task removal. Scene destruction and reset `822180A8` invalidate prior audio state.

The card vector is at scene `+1FE0`, with begin/end/capacity at `+1FEC/+1FF0/+1FF4` and record stride `40` hexadecimal. Insertion `8226D998` updates the cell-to-index mapping, but alternate setup insertion `822CDB30` calls lower-level append `8226D8C0` directly. Removal `82217A20` erases a record and rebuilds surviving mapped indices. Vector addresses and indices are not durable voice identities. Any capture implementation must also distinguish removal/replacement between snapshots, including identical cards placed in the same cell.

Capture observes append `8226D8C0`, shared insertion `82222AC0`, single erase
`82222638` and range erase `82222710`. Erase iterators carry the element address
in their low 32 bits and the container proxy in their high 32 bits. Guards check
both against the live vector. Source identities belong to occupied grid cells;
insertion/erasure retires affected identities, while surviving cells retain
their voice phase. Cached mutation snapshots retain their capture generation
and cannot resurrect audio following an external invalidation.

## Native pause path

Input predicate `82271FF8` examines active human participants and their assigned
controllers for the newly pressed Start mask, `800` hexadecimal. Pause task
`822720A0` then sets battle-owner freeze byte `2765` and clears scene draw byte
`24E0` at `822721A8`. Its resume path restores the draw flag at `822723D0` when
that task suspended the context, then clears the freeze flag.

The renderer is an independently registered task, vtable `820099C8` slot `4`
resolving to `82216F78`. It bypasses the gameplay freeze thunk `8220BBA0`.
Scheduler `823B80E0` continues dispatching linked, nondeleted tasks; ordinary
pause therefore reaches capture and invalidates it on the next render callback.
This does not prove that render-task state one represents ordinary pause.
A separate whole-dispatch early exit through `823B66F0` is not classified here.
Focus loss, system suspension and listening behavior still need validation.

## Card fields and consumers

Constructor `8226D700` stores participant input `r4` at record `+2D`, selected hand-card/model index `r5` at `+2C`, grid coordinates `r7/r8` at `+2E/+2F`, and rotation `r6` at `+30`. Placement callers `8226C150` and `8226C770` select the card from the same participant's hand. Native selection `82257D28` compares record `+2D` with the selecting participant; tutorial selection `822CCE40` also searches that owner field. Occupant ownership in nested records is distinct.

Record floats `+0/+4/+8` reach the final translation consumer. Renderer `82216760` selects a model from scene `[+224,+228)`, stride `190`, using record `+2C`, and installs it at wrapper `(scene+1A0)+90`. Wrapper vtable `8200D088`, slot `74`, resolves to `821F64B0`; that forwards through the selected model's slot `64`. Model construction `821F10C8` establishes vtable `82005508`, whose slot `64` is `821F0358`. That consumer loads model `+E8`, copies the input vector, sets its fourth component to one and calls `8215DA40`, which stores it at node `+40` and marks transform flag `800` dirty. The separate native spatial hit-test path `82258DF8` through `822141F8` corroborates the three position components.

Native grid spacing constants are `7.0` and `9.5`; their units are not established as metres. Directional rendering does not imply a distance-attenuation policy.

## Camera consumer

Scene context `+550` contains the camera at `+560`, constructed by `8239D008` through `82206F48` with vtable `820079B0`. The renderer passes that context to `821C39A0`, which uses its camera when no override is provided. Camera virtual slot `44` resolves to `82207098` and returns the view matrix at camera `+F0`.

Update `82207168` calls `821B8060` with eye at camera `+20`, target at `+30`, up at `+40`, and view output at `+F0`. The original vector operations compute normalized eye-minus-target, right from up crossed with backward, and corrected up from backward crossed with right. Scalar stores establish the matrix basis columns. `82207250` submits the same view matrix through `8217BB50`, selector one.

Accordingly, scene eye is at `+580` and view matrix at `+650`. Relative to that matrix, right occupies offsets `0/10/20`, corrected up `4/14/24`, and backward `8/18/28`, all hexadecimal. Audio forward is negative backward. Validate finite, approximately orthonormal vectors; matrix translation is not eye position.

## Participant identity limits

Scene byte `24C5` is the signed active-participant count, copied from setup byte `888`. Four records of stride `D8` are initialized at scene `+1C80`; capture must accept only counts one through four and examine only active rows.

Participant byte `D4`, at scene `+1D54+index*D8`, distinguishes native controller handling (zero) from AI handling. Native localized player/CPU turn-caption construction `82230890` independently uses the same discriminator. Byte `CF` is a controller index, and scene byte `24C7` is the current turn. Neither universally identifies the local player: multiple human participants may share a controller.

A first test supports the unambiguous single-human setup and fails closed with a named diagnostic otherwise. Owner equality identifies own cards. The team relation implemented by `8220D678` is separate: scene byte `24C9` equal to one enables its membership comparison. Current capture requires mode zero and rejects the relationship mode, rather than classifying AI allies as enemies without a verified design contract.

## Evidence limits

Private evidence collections `gate-card-audio-20261015/card-capture` and `battle-speech-20261015` retain original instruction exports and consumer traces. VMX128 decoder gaps were cross-checked against original words and the existing translation decoder. No RE database annotations were added. No game-derived code, audio or private machine paths are included in this record. The user confirms that audio and new speech operate in the installed build, but reports overly sharp onset, speech masking and the overly broad activation described above. Detailed direction and lifecycle acceptance remains incomplete.
