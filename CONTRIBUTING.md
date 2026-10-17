# Contributing

## Scope and tracking

Use [GitHub Issues](https://github.com/RealAmethyst/BakuganBB-recomp/issues) for work planning and evidence. Issue 1 groups the original investigations. Keep changes focused and preserve working behavior. The current priority is accurate, accessible playability; enhancements follow that foundation.

Read the [architecture](docs/architecture.md), [build guide](docs/building.md) and relevant [game records](docs/README.md) before changing an integration contract. Propose independent work in separate feature branches and focused pull requests. Explain the user-visible problem, actual cause, validation and remaining limits. Disclose AI assistance; never represent AI-written code as human-authored. Check a dependency's own contribution policy before proposing changes upstream.

## Content boundary

Do not submit game executables, extracted assets, raw decompilation, generated proprietary code, shaders or shader caches derived from the game, native game binaries, private logs, personal machine paths or acquisition instructions. Keep private inputs and build outputs outside the checkout. Do not attach them to issues, pull requests, Actions artifacts or releases.

Use independently authored synthetic fixtures. Describe verified behavior and cite executable identity and code locations without copying proprietary implementations. Inspect the actual proposed files and history; `.gitignore` is not a content audit. Preserve third-party notices and review licenses before reuse. Original contributions use the project's [BSD-3-Clause license](LICENSE).

## Reverse engineering and accessibility

Understanding the game is a deliverable. Trace each screen, feature, mechanic and service as work reaches it. Record verified function responsibilities, object ownership, state transitions, localization consumers and input behavior in `docs`, separating observations, hypotheses and unknowns. Keep raw evidence private. Report any RE database annotation with its exact address and label or comment.

Prefer verified semantic hooks. When polling is necessary, explain why and bound its cost. Trace pointer chains through the actual consumer; readable memory alone is not proof of object identity. Validate ownership and lifetime, fail closed and log distinct failure reasons without recording private text. Keep speech policy separate from game-event capture.

Use the game's actual localized text for game content. Only verified artwork-only wording without a text source may enter the central project resources. Do not invent screen titles or substitute a nearby string. User-facing accessibility decisions belong to the accessibility designer; ask about unresolved behavior.

## Verification

Run the relevant [offline checks](docs/building.md), including affected existing behavior. Preserve synthetic fixture assertions and check failures as well as successful paths. State precisely what passed and what was not tested. Automated checks do not establish audible speech, controller usability or complete gameplay.

Do not use the active screen reader, desktop accessibility bus or audio services as a test fixture. Run integration tests in an isolated environment with separate runtime, configuration, display and service activation. A private bus alone is insufficient isolation. Listening and in-game testing are performed by a tester.
