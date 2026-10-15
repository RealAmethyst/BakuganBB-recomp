#pragma once

#include "guest_ui_memory.h"
#include "settings_menu.h"

#include <cstddef>
#include <cstdint>
#include <functional>

struct PPCContext;

namespace brawlers::settings {

// Install before guest execution. Both callbacks validate the entire range.
void SetNativeSettingsRenderAccess(
    ui::ReadBytes read,
    std::function<bool(uint32_t, const void*, size_t)> write);

// One guest-thread owner. Begin/Built bracket the original Options builder.
// A false Built result requires closing the native task without accepting
// managed input; construction may already have installed repeated templates.
bool NativeSettingsRenderBegin(uint32_t model, PPCContext& ctx, uint8_t* base);
bool NativeSettingsRenderBuilt(uint32_t model, PPCContext& ctx, uint8_t* base);
bool NativeSettingsRenderPublish(const View& view);
// Zero-based selector identity in the five physical native display slots.
uint32_t NativeSettingsRenderSelection();
bool NativeSettingsRenderHealthy();
// Suspend presentation while a native child dialog owns focus. Animation and
// closing callbacks still run. Resume republishes the current copied view.
void NativeSettingsRenderSuspend(bool suspended);
void NativeSettingsRenderEnd(uint32_t model, PPCContext& ctx, uint8_t* base);

// Existing 823A9528 wrapper calls this before its original. True means this
// function handled the scoped native text draw (including hidden rows).
bool NativeSettingsRenderText(PPCContext& ctx, uint8_t* base);
// Existing 821EA650 wrapper delegates before its original. The provider wrapper
// delegates after its original, retaining the request arguments before calling.
bool NativeSettingsRenderHints(PPCContext& ctx, uint8_t* base);
void NativeSettingsRenderProvider(PPCContext& ctx, uint32_t bank, uint32_t id,
                                  uint64_t caller);

}  // namespace brawlers::settings

// The implementation also provides the generator's C++ instruction hook
// NativeSettingsRowTransform(PPCRegister&): 823C65F4, before the instruction,
// registers=["r31"]. Native animation has assembled the final matrix there.
