// Native player and match-mode setup; see the VS setup research record.
#include "native_vs_mode.h"
#include "native_localization.h"

#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_822F70A8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822F7248(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822F7B00(PPCContext&, uint8_t*);

namespace {

using brawlers::ui::GuestUiMemory;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;
std::function<void(ScreenEvent&&)> output;
std::function<void(std::string_view)> diagnostic;
std::function<void()> failure;
std::set<std::string> reported;
uint64_t next_identity = 0;
bool failed = false;

struct Menu {
  uint64_t id = 0;
  uint32_t locale = 0, parent = 0, input_depth = 0;
  bool active = false, opened = false, invalid = false, resume = false, dirty = false;
  std::string last_item;
};
std::map<uint32_t, Menu> menus;

class FloatEnvironment {
 public:
  FloatEnvironment() : valid_(std::fegetenv(&value_) == 0) {}
  ~FloatEnvironment() { if (valid_) std::fesetenv(&value_); }
 private:
  std::fenv_t value_;
  bool valid_;
};

template <typename Function>
void Observe(Function function) noexcept {
  FloatEnvironment preserve;
  try {
    auto lock = state_region.Acquire();
    if (failed) return;
    try {
      function();
    } catch (...) {
      failed = true;
      if (failure) failure();
    }
  } catch (...) {
    // Never replace a failed guest-region lock with an unprotected memory read.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Task(uint32_t owner) {
  return Guard(owner && owner <= UINT32_MAX - 0x13C &&
               memory.U32BE(owner) == 0x8201802C, "vs_mode_task_owner");
}

ScreenEvent Event(const Menu& menu, const char* kind) {
  ScreenEvent event;
  event.source = "vs_mode";
  event.kind = kind;
  event.id = menu.id;
  event.locale = menu.locale;
  return event;
}

void Close(uint32_t owner) {
  const auto found = menus.find(owner);
  if (found == menus.end()) return;
  if (found->second.opened) output(Event(found->second, "closed"));
  menus.erase(found);
}

bool Selection(uint32_t owner, uint32_t selected, uint32_t& total) {
  const auto selector = owner + 0xAC;
  const auto nested = memory.U32BE(selector + 0x28);
  if (!Guard(memory.U32BE(selector) == 0x8200FC30, "vs_mode_selector_table") ||
      !Guard(nested == selector + 0x40, "vs_mode_selector_nested") ||
      !Guard(memory.U32BE(*nested) == 0x8200FC10, "vs_mode_nested_table") ||
      !Guard(memory.U32BE(*nested + 4) == owner + 0x10C, "vs_mode_navigation_graph") ||
      !Guard(memory.U32BE(*nested + 0x14) == 7, "vs_mode_selector_count") ||
      !Guard(selected < 7 && memory.U32BE(*nested + 0x10) == selected,
             "vs_mode_selector_current")) return false;
  const auto active = memory.U8(*nested + 0x18);
  if (!Guard(active && *active <= 1, "vs_mode_selector_active")) return false;
  // Native setup leaves the graph inactive until its first input pass.
  if (!*active) return false;
  const auto enabled = memory.U32BE(*nested + 8);
  const auto players = memory.U8(owner + 0x36);
  if (!Guard(enabled && *enabled && *enabled <= UINT32_MAX - 7,
             "vs_mode_enabled_storage") ||
      !Guard(players && *players >= 2 && *players <= 4, "vs_mode_player_count")) return false;
  total = *players == 4 ? 7 : 5;
  for (uint32_t row = 0; row < 7; ++row) {
    if (!Guard(memory.U8(*enabled + row) == (row < total ? 1 : 0),
               "vs_mode_enabled_rows")) return false;
  }
  return Guard(selected < total, "vs_mode_enabled_selection");
}

void Focus(uint32_t owner, Menu& menu, uint32_t selected) {
  if (!menu.active || menu.invalid || !Task(owner)) return;
  if (!Guard(memory.U32BE(menu.parent) == 0x8201A078 &&
             memory.U32BE(menu.parent + 0x14) == 4 &&
             memory.U32BE(owner + 0x20) == menu.parent + 0x20 &&
             memory.U32BE(owner + 0x18) == menu.parent + 0x42C,
             "vs_mode_parent_owner")) return;
  if (!Guard(memory.U32BE(owner + 0x14) == 2, "vs_mode_active_state") ||
      !Guard(memory.U8(owner + 0x129) == 0, "vs_mode_input_section")) return;
  uint32_t total = 0;
  if (!Selection(owner, selected, total)) return;
  auto event = Event(menu, menu.resume ? "resumed" : "focused");
  event.position = selected + 1;
  event.total = total;
  if (selected < 4) {
    const auto state = memory.U8(owner + 0x12F + selected);
    if (!Guard(state && *state <= 2, "vs_mode_slot_state")) return;
    if (*state) {
      // 822FA008 follows model+28 to the actual identity-frame array. Slot
      // order alone does not establish the displayed participant identifier.
      if (!Guard(memory.U32BE(owner + 0x44) == 0x82018014 &&
                 memory.U32BE(owner + 0x6C) == owner + 0x133,
                 "vs_mode_identity_model")) return;
      const auto identity = memory.U8(owner + 0x133 + selected);
      if (!Guard(identity && *identity < 7, "vs_mode_identity_frame")) return;
      event.label_key = "vs_identity_" + std::to_string(*identity);
    } else {
      event.label_key = "vs_mode_slot_empty";
    }
    constexpr std::array<const char*, 3> values{
        "", "vs_mode_slot_human", "vs_mode_slot_computer"};
    event.value_key = values[*state];
  } else if (selected == 4) {
    event.label_key = "vs_mode_confirm";
  } else {
    const auto mode = memory.U32BE(owner + 0x38);
    if (!Guard(mode && *mode < 2, "vs_mode_match_kind")) return;
    event.label_key = "vs_mode_kind_" + std::to_string(selected - 5);
    event.selected = *mode == selected - 5;
  }
  const auto fingerprint = event.label_key + ":" + event.value_key + ":" +
      std::to_string(event.selected) + ":" + std::to_string(event.position) + ":" + std::to_string(total);
  if (!menu.resume && fingerprint == menu.last_item) return;
  if (!menu.opened) {
    const auto common = memory.U32BE(0x829CBC68);
    if (!Guard(common && *common && *common <= UINT32_MAX - 0xE0,
               "vs_mode_common_anchor") ||
        !Guard(memory.U32BE(*common) == 0x82003EAC, "vs_mode_common_owner") ||
        !Guard(memory.U32BE(*common + 0x58) == 0 &&
               memory.U8(*common + 0x54) == 2, "vs_mode_title_record")) return;
    const auto resources = memory.U32BE(0x829CC358);
    if (!Guard(resources && *resources && *resources <= UINT32_MAX - 0x1C,
               "vs_mode_resources_anchor")) return;
    const auto loader = memory.U32BE(*resources + 0x18);
    if (!Guard(loader && *loader && *loader <= UINT32_MAX - 0x2C,
               "vs_mode_resource_loader") ||
        !Guard(memory.U32BE(*loader) == 0x82003F2C, "vs_mode_resource_loader_table")) return;
    const auto resource = memory.U16BE(*loader + 0x20);
    if (!Guard(resource && *resource >= 0x9FC && *resource <= 0xA02,
               "vs_mode_resource_locale")) return;
    menu.locale = *resource - 0x9FB;
    event.locale = menu.locale;
    auto opening = Event(menu, "opened");
    const auto title = brawlers::ui::ReadMenuText(memory, 0x625);
    const auto hint = brawlers::ui::ReadMenuText(memory, 0xA08);
    if (!Guard(title.value.has_value(), title.reason) ||
        !Guard(hint.value.has_value(), hint.reason)) return;
    opening.title = *title.value;
    opening.hints.push_back(*hint.value);
    output(std::move(opening));
    menu.opened = true;
  }
  output(std::move(event));
  menu.last_item = fingerprint;
  menu.resume = false;
}

void Current(uint32_t owner) {
  const auto found = menus.find(owner);
  if (found == menus.end() || !found->second.active || found->second.invalid) return;
  if (!Task(owner)) return;
  const auto selected = memory.U32BE(owner + 0xFC);
  if (!Guard(selected.has_value(), "vs_mode_current_read")) return;
  Focus(owner, found->second, *selected);
}

void Changed(uint32_t owner, Menu& menu) {
  if (!menu.active || menu.invalid) return;
  menu.dirty = true;
  if (!menu.input_depth) {
    menu.dirty = false;
    Current(owner);
  }
}

}  // namespace

void SetVsModeCapture(brawlers::ui::ReadBytes reader,
                      std::function<void(ScreenEvent&&)> sink,
                      std::function<void(std::string_view)> report,
                      std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsModeTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

void NotifyVsModeSelection(uint32_t selector, uint32_t index) noexcept {
  Observe([&] {
    if (selector < 0xAC) return;
    const auto found = menus.find(selector - 0xAC);
    if (found == menus.end() || !Guard(index < 7, "vs_mode_selection_event")) return;
    if (found->second.input_depth) Changed(found->first, found->second);
    else Focus(found->first, found->second, index);
  });
}

void BrawlersVsModeReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = menus.find(owner.u32);
    if (found == menus.end() || found->second.invalid) return;
    found->second.active = true;
    Changed(owner.u32, found->second);
  });
}

void BrawlersVsModeSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = menus.find(owner.u32);
    if (found == menus.end()) return;
    found->second.active = false;
    found->second.dirty = false;
  });
}

void BrawlersVsModeResumed(PPCRegister& owner) {
  Observe([&] {
    const auto found = menus.find(owner.u32);
    if (found == menus.end() || found->second.invalid) return;
    found->second.active = true;
    found->second.resume = found->second.opened;
    Changed(owner.u32, found->second);
  });
}

void BrawlersVsModeValueChanged(PPCRegister& owner) {
  Observe([&] {
    const auto found = menus.find(owner.u32);
    if (found != menus.end()) Changed(owner.u32, found->second);
  });
}

extern "C" void sub_822F7B00(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] {
    const auto found = menus.find(owner);
    if (found != menus.end()) ++found->second.input_depth;
  });
  __imp__sub_822F7B00(ctx, base);
  Observe([&] {
    const auto found = menus.find(owner);
    if (found == menus.end()) return;
    auto& menu = found->second;
    if (menu.input_depth) --menu.input_depth;
    // Native input can redirect through selector+28 after its changed event.
    if (!menu.input_depth && std::exchange(menu.dirty, false)) Current(owner);
  });
}

extern "C" void sub_822F70A8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  const auto result = ctx.r4.u32, config = ctx.r5.u32;
  const auto layer = ctx.r6.u32;
  const auto mode = ctx.r7.u8;
  __imp__sub_822F70A8(ctx, base);
  if (mode != 0 || layer != 1 || config < 0x20) return;
  Observe([&] {
    if (!output || !Task(owner)) return;
    const auto parent = config - 0x20;
    if (!Guard(parent && parent <= UINT32_MAX - 0x43C &&
               memory.U32BE(parent) == 0x8201A078 &&
               memory.U32BE(parent + 0x14) == 3 &&
               result == parent + 0x42C &&
               memory.U32BE(owner + 0x20) == config &&
               memory.U32BE(owner + 0x18) == result,
               "vs_mode_constructor_parent")) return;
    if (const auto previous = menus.find(owner); previous != menus.end()) {
      Guard(false, "vs_mode_duplicate_lifetime");
      previous->second.invalid = true;
      return;
    }
    Menu menu;
    menu.id = ++next_identity;
    menu.parent = parent;
    menus.emplace(owner, std::move(menu));
  });
}

extern "C" void sub_822F7248(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { Close(owner); });
  __imp__sub_822F7248(ctx, base);
}
