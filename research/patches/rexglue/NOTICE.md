# Third-party notices for ReXGlue evaluation patches

These patches modify source from ReXGlue SDK revision `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`. The following notices are retained from the affected source-file headers. Paths below are relative to that upstream repository.

The upstream BSD-3-Clause terms and root notices are reproduced in [LICENSE.rexglue](LICENSE.rexglue). This notice supplements that unchanged license file; it does not assign a license to unrelated project content.

## Affected source group 1

Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
All rights reserved.

- `include/rex/codegen/function_graph.h`
- `src/codegen/builders/context.cpp`
- `src/codegen/builders/control_flow.cpp`
- `src/codegen/phase_gapfill.cpp`
- `src/core/clock_posix.cpp`
- `src/core/threading_posix.cpp`
- `src/ui/rex_app.cpp`
- `tests/unit/codegen/output_stamp_test.cpp`
- `tests/unit/core/chrono_test.cpp`

## Affected source group 2

Copyright 2022 Ben Vanik. All rights reserved.

The same headers credit: Tom Clay, 2026 — Adapted for ReXGlue runtime.

- `include/rex/chrono/chrono.h`
- `include/rex/chrono/clock.h`
- `include/rex/ui/imgui_dialog.h`
- `include/rex/ui/imgui_drawer.h`
- `include/rex/ui/window.h`
- `src/graphics/vulkan/command_processor.cpp`
- `src/kernel/xboxkrnl/xboxkrnl_rtl.cpp`
- `src/kernel/xam/xam_ui.cpp`
- `src/ui/imgui_dialog.cpp`
- `src/ui/imgui_drawer.cpp`
- `src/ui/vulkan/vulkan_presenter.cpp`

## Affected source group 3

Copyright 2019 Ben Vanik. All rights reserved.

The same headers credit: Tom Clay, 2026 — Adapted for ReXGlue runtime.

- `src/core/clock.cpp`

## Affected source group 4

Copyright (c) 2026 Rien Gupta <rgupta9@scu.edu>
All rights reserved.

- `include/rex/ui/overlay/achievement_toast.h`
- `src/ui/overlay/achievement_toast.cpp`

## Affected source group 5

Copyright 2025 Ben Vanik. All rights reserved.

The same headers credit: Tom Clay, 2026 — Adapted for ReXGlue runtime.

- `src/ui/vulkan/vulkan_device.cpp`

## Affected source group 6

Copyright 2020 Ben Vanik. All rights reserved.

The same headers credit: Tom Clay, 2026 — Adapted for ReXGlue runtime.

- `include/rex/system/xmemory.h`
- `src/system/xmemory.cpp`
- `include/rex/input/sdl/sdl_input_driver.h`
- `src/input/sdl/sdl_input_driver.cpp`

## Local changes

The evaluation changes were prepared with Codex assistance. Original authorship and copyright notices above refer to the upstream source, not authorship of these changes.

The new `include/rex/ui/modal_gamepad_input.h` is original local integration code with an explicit BSD-3-Clause SPDX identifier; it does not inherit a claim of authorship from the upstream notices.
