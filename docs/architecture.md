# Architecture

## Dependency boundaries

`src/core` contains standard C++ memory/text validation, event values and the bounded speech queue. It does not depend on ReXGlue or generated game code. `src/core/accessibility` contains Python event validation, speech ordering, text preparation and the Prism adapter. These modules have no Linux launcher dependency; portability of the design is not proof of execution on every platform.

`src/game` contains hooks whose meaning comes from verified game code. They preserve original execution and capture events or copy bounded game-owned text. `config/game_events.toml` is the pinned translation's hook configuration. Game-event ownership and exact source identities are documented in the [localization record](localization-consumers.md).

`src/runtime` binds the host to ReXGlue and transports runtime UI events. It depends on the patched SDK and, for the host, private generated image configuration. Runtime integration is separate from OS-specific process supervision; it is not classified as portable core merely because the SDK abstracts some platforms.

`src/platform/linux/launch.py` owns Linux launch locking, child-process supervision and installation paths. There is no Windows or macOS launcher yet. New platforms should implement their own OS boundary and share the existing event/speech policy where its contracts remain valid.

## Sources and installed layout

Repository layout describes responsibilities. The private installed application retains its existing flat Python launcher/module layout, with `messages.en.json` beside the launcher, and `bin`, `lib`, `data` and `user-data` beneath the installation root. The [build guide](building.md) describes source staging. Moving repository files must not change that working installation contract.

`resources` contains central project wording and verified artwork-only transcriptions, not extracted game assets. Ordinary game captions are captured from the actual runtime text consumers. `tests` contains original synthetic fixtures; `patches/rexglue` retains SDK patches, provenance and their associated regression fixtures.

Game inputs, generated translation, runtime data, caches and compiled game binaries remain external to the source tree. No game execution is registered with CTest.

## Reference structures

[Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) separates its application from its recompilation library, with dedicated documentation and tools. [Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp) separates `src`, `include` and `docs`. [GoldenEye Recompiled](https://github.com/SunJaycy/GoldenEye-Recomp) uses a compact `src` tree with root build configuration.

This project follows those separation principles without adopting their content inventories or copying their implementation. A shared public header tree is not needed yet: current headers belong beside the small core or game component that owns them. Platform folders hold actual OS-specific code; they do not duplicate shared services.
