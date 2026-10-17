# Character creation

## Evidence and scope

These contracts were traced on 2026-10-14 against the [pinned executable](executable-baseline.md), using fresh original instructions, resource consumers and privately inspected previews. No reverse-engineering database annotations were added. Raw exports and game resources remain private. [Issue 26](https://github.com/RealAmethyst/BakuganBB-recomp/issues/26) tracks implementation and listening acceptance.

The attribute-selection parent and detailed appearance editor are separate native tasks. Name entry is a child of the parent; its physical-keyboard contract is documented in [Story entry](story-entry-freeze.md). Native confirmation dialogs retain their independent lifetimes. Operation-status messages are documented in the [localization record](localization-consumers.md#native-operation-status-messages).

## Attribute-selection lifetime and input

Constructor `82349F58` creates the parent with table `8201A8D0`. Its selector is at owner `+48`; the selector table is `8200FC30`, and `+28` points to the embedded graph selector at `+40`, with table `8200FC10`. The nested current index is `+10`, count `+14`, and validity byte `+18`. Setup `823486B8` and builder `82348EF0` register six attribute rows and one artwork confirmation row. Decorations and text widgets do not contribute to the seven-item total.

Active state 7 is stored at `82349B24`. Confirmation and Back suspend interaction through state 8 at `82349B8C` and `82349C7C`. Builder `82348EF0` and layer teardown `82348978` rebuild presentation without ending the task. Returning from the name or appearance child reaches the same active state. Only actual task removal through `823B87A8`, or destructor `82348548`, ends the parent lifetime. Speech must preserve its opening title across child screens.

Changed selection uses the shared event at `821DF0FC`, carrying selector `r31` and current row `r27`. The initial selection is restored from the committed attribute through inverse table `8201A89C`; it must not be assumed to be zero.

## Focused attribute and localized text

The focused icon and applied attribute are distinct. State 7 writes the applied attribute at owner `+848` only when a row is confirmed, at `82349BD4`. The row-to-attribute map is `8201A894`; name and description IDs are in `8201A930`. Preview callback `8234BA78` reads the applied value through its context record, with old and current values retained for crossfade. Reading that old preview during cursor movement would announce the wrong option.

Focused speech therefore resolves the verified row's own name and description at the selection event. `native_localization.h` follows the actual bank-zero provider chain without invoking an extra guest function. Provider global `829CEB78` has table `82036508`, banks at `+8` and byte count at `+C`. Bank stride is `70`, with up to eight descriptors of stride `C` and descriptor count at `+60`. Getter `823B60B8` subtracts preceding descriptor counts; `8212ECF0` resolves a resource-relative string offset.

Descriptor assignment `8212ED40` validates the resource marker and flag and binds offset tables. Header `+10` identifies the 32-bit offset table; header `+14` identifies an auxiliary 16-bit table, **not a text base or allocation size**. The host validates those pointer bindings, table arithmetic and alignment, then bounds the selected UTF-16 string by the following entry's offset and a finite copying budget. Final entries are rejected because their allocation bound is not established. All twelve needed name/description IDs in the seven inspected language resources are nonfinal, terminate within that bound, and contain plain text. Invalid data produces a named guard and no substituted caption.

The displayed title is bank 0, ID `6B9`, consumed by `8234A890`; instruction ID `96A` is consumed by `8234C0D0`. They are separate opening text. Both use their actual expanded final draw output. The parent uses common hint ID `9EC`. Confirmation row 6 uses shared artwork UV 3: constructor `821CDAC0` substitutes the common resource, and renderer `821CC9D0` selects code plus 3. Its existing verified artwork localization applies.

## Detailed appearance editor

Constructor `8232C3E0` creates table `820196BC`. Verified fields are result pointer `+18`, actor `+1C`, name buffer `+20`, layer `+28`, mode `+2C`, category `+9C`, pending category `+A0`, root row `+A4`, option row `+A8`, accepted option `+AC`, transition `+B0`, and selector `+B8`.

Configuration `8232BC00` establishes enabled rows from mode/root tables `82019468` and `829A2004`, or category count table `820194B4`. Root mode 0 has eight categories plus confirmation; mode 1 has seven categories plus confirmation. Confirmation uses selector row 8 in both modes, so its spoken position must be the enabled-item ordinal, not raw row plus one. Category submenus exclude row 8. The graph selector has nine storage slots, which is not always the navigable total.

Interaction requires task state 3 and transition 1. The shared selection event reports the actual cursor; the caption mirror at `+A8` alone misses row 8. Category acceptance and cancellation use `8232BCF8` and the verified forward/back category tables. Randomization creates its own native confirmation popup; returning must resume the editor without inventing another opening title. Normal exit `8232C290`, task removal, and destructor `8232BB28` provide actual close evidence.

Callback `8232EB88` consumes root label IDs from `820194FC`, or category/row IDs from `82019530`. Its old/new widgets crossfade, so capture must accept only the current expected ID. No persistent title or submenu heading was established in the actual consumers. Nearby strings with suggestive names do not justify spoken headings.

Callback `8232E830` displays marker ID `927` only when the current option equals the accepted option and the editor is not closing. The marker belongs to that item's description. Custom hints use `94A` for root randomization and `A23` for rotation; common hints use `9EC`. The verified Y and neutral right-stick glyphs are expanded centrally. The right-stick icon does not mean pressing the stick.

Following listening feedback, speech shortens this verified marker to the centrally localized word "Selected". Capture sends an explicit selection flag; it does not replace matching words in arbitrary game text. The marker's native visibility condition is retained.

The editor requests an actual loading caption, ID `91F`, through `82330070` when actor readiness `821F99A8` fails or the pending byte reached through record `+30` is nonzero, after its layer and widget gates pass. The text request at `823301D0` reaches layout `823A9528`, line drawing `82396140` in draw mode, glyph construction `82395D18` and render-packet finalization `82396128`. This is game rendering, not a diagnostic label. Opacity, clipping and physical visibility during the reported run remain unverified; a draw submission alone does not establish visible pixels.

Following listening feedback, appearance-preview loading stays silent. Its speech-only wrapper, record/readiness hooks and status lifetime have been removed; the original game rendering remains intact. Preview loading therefore neither interrupts nor suspends option speech. Native saving, deleting and other operation-status popups retain their separate capture and speech.

## Visual descriptions

Most appearance options have native localized names. Supplementary descriptions explain verified visual details where those names alone are insufficient. They live in the central `creator_descriptions` resource, keyed by category and zero-based option. Named colors and absence options do not need redundant supplements. The five generically numbered skin tones receive direct color descriptions, without the introductory phrase "The preview shows", following listening feedback. Their evidence remains the displayed previews; the character model's tint pipeline has not been traced here. These descriptions are original accessibility wording; no preview images are distributed.

Preview update `8232D940` selects animation from `8201946C` and uses the option as its frame. Native animation/key consumers `823C8050`, `823C8358`, `823D4510` and `823D3EE8` establish the frame-to-UV mapping used for private visual inspection. Descriptions must follow that mapping rather than atlas order or a guessed association.

The drawing context also matters. Wrapper constructors `823A8B70`/`823A8C30` store a 40-byte context at `+18`; context constructors `823CDFB0`/`823CDF50` reach `823CDC68`/`823CDB20`, which allocate a 320-byte renderer and store it at context `+4`. Renderer constructors `823CA388`/`823CA430` reach initializer `823C8198`, which clears direct UV override `+DC` at `823C81C8`. Dispatcher `823CE010` preserves that context in `r29` and supplies it in `r4` to widget slot zero at `823CE24C`. Preview table `820196D8` dispatches to `8232D940`; its call into `823C64A0` uses the same context's renderer at `+4`.

The preview widget's override vector is empty, and the traced creator drawing path does not call the standalone direct-UV setters. This verifies the initialized baseline path; it is not a claim that the engine resets direct UV overrides for every widget. The earlier possible per-widget-reset interpretation was rejected against the original code. Future changes to context ownership or overrides need to recheck this mapping.

## Verification boundary

Original synthetic fixtures check resource bounds, speech ordering, native ownership and lifecycle. They do not execute the game or establish audible usability. Current build and listening results belong in the issue and [diagnostic record](diagnostic-launch.md).
