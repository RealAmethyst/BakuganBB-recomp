#include "native_vs_decks.h"
#include "native_localization.h"

#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_822F1138(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822F2820(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822F0D18(PPCContext&, uint8_t*);

namespace {
using brawlers::ui::GuestUiMemory;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;
std::function<void(uint32_t, ScreenEvent&&)> output;
std::function<void(std::string_view)> diagnostic;
std::function<void()> failure;
std::set<std::string> reported;
uint64_t next_identity = 0;
bool failed = false;

struct Decks {
  uint64_t id = 0;
  uint32_t parent = 0, locale = 0, controllers = 1;
  uint32_t input_depth = 0, dirty_controllers = 0;
  bool active = false, opened = false, hints_sent = false, resume_started = false;
  std::array<bool, 4> focused{};
  std::array<bool, 4> resumed{};
  std::array<ScreenEvent, 4> items;
};
std::map<uint32_t, Decks> screens;

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
    try { function(); }
    catch (...) {
      failed = true;
      if (failure) failure();
    }
  } catch (...) {
    // An unavailable lock never permits unprotected guest reads.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Owner(uint32_t owner) {
  return Guard(owner && owner <= UINT32_MAX - 0x208 &&
               memory.U32BE(owner) == 0x82017DDC &&
               memory.U32BE(owner + 0x20) == 0x82017DD8,
               "vs_decks_owner");
}

ScreenEvent Event(const Decks& decks, const char* kind) {
  ScreenEvent event;
  event.source = "vs_decks";
  event.kind = kind;
  event.id = decks.id;
  event.locale = decks.locale;
  return event;
}

bool Text(uint32_t id, std::string& destination) {
  const auto text = brawlers::ui::ReadMenuText(memory, id);
  if (!Guard(text.value.has_value(), text.reason)) return false;
  destination = *text.value;
  return true;
}

bool Participants(uint32_t owner, const Decks& decks, uint32_t& count, uint32_t& controllers) {
  if (!Guard(memory.U32BE(owner + 0x18) == decks.parent + 0x42C &&
             memory.U32BE(owner + 0x1F4) == decks.parent + 0x44 &&
             memory.U32BE(owner + 0x1F8) == decks.parent + 0x20 &&
             memory.U32BE(owner + 0x204) == decks.parent + 0x432,
             "vs_decks_parent_binding")) return false;
  const auto participants = memory.U8(decks.parent + 0x29);
  const auto humans = memory.U8(owner + 0x1DC);
  if (!Guard(participants && *participants >= 2 && *participants <= 4,
             "vs_decks_participant_count") ||
      !Guard(humans && *humans >= 1 && *humans <= *participants,
             "vs_decks_controller_count")) return false;
  count = *participants;
  controllers = *humans;
  return true;
}

bool Selection(uint32_t owner, uint32_t controller, uint32_t& selected, uint32_t& flags) {
  const uint32_t selector = owner + 0x2C + controller * 0x60;
  const uint32_t model = selector + 0x40;
  const uint32_t count = controller == 0 ? 5 : 1;
  if (!Guard(memory.U32BE(selector) == 0x8200FC30 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x8200FC10, "vs_decks_selector_owner") ||
      !Guard(memory.U32BE(model + 0x14) == count, "vs_decks_selector_count")) return false;
  const auto index = memory.U32BE(model + 0x10);
  const auto enabled = memory.U32BE(model + 8);
  if (!Guard(index && *index < count, "vs_decks_selector_index") ||
      !Guard(enabled && *enabled && *enabled <= UINT32_MAX - count,
             "vs_decks_navigation_flags")) return false;
  selected = *index;
  flags = *enabled;
  return true;
}

void Focus(uint32_t owner, Decks& decks, uint32_t controller) {
  if (!decks.active || !output || controller >= 4 || !Owner(owner) ||
      !Guard(memory.U32BE(owner + 0x14) == 3, "vs_decks_active_state")) return;
  uint32_t participants = 0, controllers = 0;
  if (!Participants(owner, decks, participants, controllers) || controller >= controllers) return;
  decks.controllers = controllers;
  uint32_t selected = 0, flags = 0;
  if (!Selection(owner, controller, selected, flags)) return;
  auto item = Event(decks, decks.resumed[controller] ? "resumed" : "focused");
  const uint32_t count = controller == 0 ? 5 : 1;
  for (uint32_t i = 0; i < count; ++i) {
    const auto enabled = memory.U8(flags + i);
    if (!Guard(enabled && *enabled <= 1, "vs_decks_navigation_enabled") ||
        !Guard(i != selected || *enabled != 0, "vs_decks_disabled_selection")) return;
    item.total += *enabled;
    if (i <= selected) item.position += *enabled;
  }
  if (controller == 0 && selected == 4) {
    item.label_key = "vs_decks_confirm";
  } else {
    const uint32_t participant = controller == 0 ? selected : controller;
    if (!Guard(participant < participants, "vs_decks_selected_participant")) return;
    // 822F11C8 binds participant+D4 to widget+100; 822F3318 passes it
    // directly to 823A9528. The deck-name widget at +A8 draws no text.
    const auto name = memory.Text(decks.parent + 0x44 + participant * 0xF8 + 0xD4, 11);
    if (!Guard(name.value && !name.value->empty(), "vs_decks_participant_name")) return;
    item.label = *name.value;
  }
  if (!decks.opened) {
    if (controller != 0) return;
    auto opening = Event(decks, "opened");
    if (!Text(0x628, opening.title)) return;
    output(0, std::move(opening));
    decks.opened = true;
  }
  const auto& previous = decks.items[controller];
  if (decks.focused[controller] && previous.label == item.label &&
      previous.label_key == item.label_key && previous.position == item.position &&
      previous.total == item.total) return;
  if (!decks.hints_sent && controller == 0) {
    std::string hints;
    if (!Text(0x9EC, hints)) return;
    item.hints.push_back(std::move(hints));
    decks.hints_sent = true;
  }
  const bool resumed = decks.resumed[controller];
  if (resumed) item.continuation = decks.resume_started;
  decks.items[controller] = item;
  output(controller, std::move(item));
  if (resumed) decks.resume_started = true;
  decks.focused[controller] = true;
  decks.resumed[controller] = false;
}

void Close(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found == screens.end()) return;
  if (found->second.opened && output) output(0, Event(found->second, "closed"));
  screens.erase(found);
}
}  // namespace

void SetVsDecksCapture(brawlers::ui::ReadBytes reader,
                       std::function<void(uint32_t, ScreenEvent&&)> sink,
                       std::function<void(std::string_view)> report,
                       std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsDecksTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

void NotifyVsDecksSelection(uint32_t selector, uint32_t selected) noexcept {
  Observe([&] {
    for (auto& [owner, decks] : screens) {
      if (!decks.active || selector < owner + 0x2C) continue;
      const uint32_t relative = selector - owner - 0x2C;
      if (relative % 0x60 || relative / 0x60 >= 4) continue;
      const uint32_t controller = relative / 0x60;
      if (decks.input_depth) {
        if (Guard(selected < (controller ? 1u : 5u), "vs_decks_selection_event_index"))
          decks.dirty_controllers |= 1u << controller;
        return;
      }
      uint32_t actual = 0, flags = 0;
      if (!Owner(owner) || !Selection(owner, controller, actual, flags) ||
          !Guard(actual == selected, "vs_decks_selection_event_mismatch")) return;
      Focus(owner, decks, controller);
      return;
    }
  });
}

extern "C" void sub_822F0D18(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end()) ++found->second.input_depth;
  });
  __imp__sub_822F0D18(ctx, base);
  const auto result = ctx.r3.u32;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found == screens.end()) return;
    auto& decks = found->second;
    if (decks.input_depth) --decks.input_depth;
    // Results 0..4 open a child or confirm; 6 leaves this screen.
    if (result != 5) decks.dirty_controllers = 0;
    if (decks.input_depth) return;
    const auto dirty = std::exchange(decks.dirty_controllers, 0);
    for (uint32_t controller = 0; controller < 4; ++controller)
      if (dirty & (1u << controller)) Focus(owner, decks, controller);
  });
}

extern "C" void sub_822F1138(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32;
  const auto participants = ctx.r5.u32, mode = ctx.r6.u32;
  const auto flags = ctx.r7.u32, layer = ctx.r8.u32;
  __imp__sub_822F1138(ctx, base);
  Observe([&] {
    if (!output || completion < 0x42C) return;
    const uint32_t parent = completion - 0x42C;
    if (!Guard(parent && parent <= UINT32_MAX - 0x438 &&
               memory.U32BE(parent) == 0x8201A078 &&
               memory.U32BE(parent + 0x14) == 7 && participants == parent + 0x44 &&
               mode == parent + 0x20 && flags == parent + 0x432 && layer == 1,
               "vs_decks_parent") || !Owner(owner)) return;
    if (!Guard(!screens.contains(owner), "vs_decks_duplicate_lifetime")) return;
    const auto common = memory.U32BE(0x829CBC68);
    if (!Guard(common && *common && *common <= UINT32_MAX - 0xE0 &&
               memory.U32BE(*common) == 0x82003EAC, "vs_decks_common_owner")) return;
    const auto resource = memory.U16BE(*common + 0x20);
    if (!Guard(resource && *resource >= 0x8CC && *resource <= 0x8D2,
               "vs_decks_common_locale")) return;
    Decks decks;
    decks.id = ++next_identity;
    decks.parent = parent;
    decks.locale = *resource - 0x8CB;
    screens.emplace(owner, std::move(decks));
  });
}

extern "C" void sub_822F2820(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_822F2820(ctx, base);
}

void BrawlersVsDecksReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    found->second.active = true;
    for (uint32_t controller = 0; controller < 4; ++controller)
      Focus(owner.u32, found->second, controller);
  });
}

void BrawlersVsDecksSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    found->second.active = false;
    found->second.dirty_controllers = 0;
    found->second.resume_started = false;
    found->second.focused.fill(false);
    if (found->second.opened) found->second.resumed.fill(true);
  });
}

void BrawlersVsDecksNameDraw(PPCRegister& widget) {
  Observe([&] {
    for (auto& [owner, decks] : screens) {
      if (!decks.active) continue;
      for (uint32_t controller = 0; controller < decks.controllers; ++controller) {
        if (decks.focused[controller]) continue;
        // Retry unreadable first captions at their real consumer only.
        if (!Guard(widget.u32 && widget.u32 <= UINT32_MAX - 0x104 &&
                   memory.U32BE(widget.u32) == 0x82017E18, "vs_decks_name_widget")) return;
        Focus(owner, decks, controller);
      }
    }
  });
}
