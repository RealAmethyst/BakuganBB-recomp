# Repository instructions

## Direction and evidence

- Build an accurate native port of the Xbox 360 game, with accessibility accompanying each playable milestone. Linux/Vulkan is the first target; do not claim other platforms are validated.
- Follow [CONTRIBUTING.md](CONTRIBUTING.md), the [architecture](docs/architecture.md) and [build guide](docs/building.md). Use [GitHub Issues](https://github.com/RealAmethyst/bb-recomp/issues) as the authoritative tracker; do not add a competing Markdown backlog.
- Understanding the game is a core deliverable for future contributors and mod support. Decompile, trace and document every screen, feature, mechanic and supporting system as work reaches it. Preserve verified ownership, state transitions, data/text consumers and input contracts in the [documentation records](docs/README.md); distinguish evidence, hypotheses and unresolved behavior.
- Prefer hooks at verified semantic operations over repeated memory polling. Keep event capture separate from speech policy. Trace pointer chains through the actual consumer, validate ownership and lifetime, and fail closed. Never infer game facts from another project's addresses or behavior. Report every RE database annotation by address and name or comment.

## Publication boundary

- Do not download game assets. No game assets, game executables, raw decompilation, generated proprietary source, game shader caches, native game binaries or asset acquisition guidance in source, documentation, fixtures, CI artifacts or releases. Keep private inputs and outputs outside this tree.
- Use original synthetic fixtures. Audit actual files and proposed history, not just ignore patterns. Retain third-party attribution and licenses; keep shared content impersonal and free of private paths or identifying logs.
- Preserve working behavior and existing changes. Do not commit, push or rewrite history without an explicit request. Visibility and publication require owner authorization; do not infer it from ordinary development work.

## Accessibility and testing

- Accessibility behavior belongs to the project owner. Ask about unresolved design; do not guess. Use actual localized game text, with only verified artwork-only captions allowed in central project resources.
- Prism uses screen-reader backends only, with standalone TTS disabled. Absence of a screen reader remains silent. Do not reconfigure, stop, replace or restart live accessibility services without explicit permission for that specific action.
- Build and verify offline. Do not launch, close or control the game without an explicit request. Do not treat automated checks as listening or gameplay confirmation. Use isolated runtime/configuration/display/bus resources for accessibility tests.
- Communicate with short paragraphs and real Markdown headings for distinct sections. Give testing instructions directly in chat; do not add communication-only status or question files.
