#include "native_vs_rules.h"
#include "native_localization.h"

#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_82339BD0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82339910(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82339878(PPCContext&, uint8_t*);

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

struct Rules {
  uint64_t id = 0;
  uint32_t locale = 0, parent = 0;
  bool active = false, opened = false, focused = false;
  ScreenEvent item;
};
std::map<uint32_t, Rules> screens;

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
  return Guard(owner && owner <= UINT32_MAX - 0xDC &&
               memory.U32BE(owner) == 0x82019EFC &&
               memory.U32BE(owner + 0x44) == 0x82019EE4,
               "vs_rules_owner");
}

ScreenEvent Event(const Rules& rules, const char* kind) {
  ScreenEvent event;
  event.source = "vs_rules";
  event.kind = kind;
  event.id = rules.id;
  event.locale = rules.locale;
  return event;
}

bool Text(uint32_t id, std::string& destination) {
  const auto text = brawlers::ui::ReadMenuText(memory, id);
  if (!Guard(text.value.has_value(), text.reason)) return false;
  destination = *text.value;
  return true;
}

bool Selection(uint32_t owner, uint32_t& selected) {
  // 821D7610 follows selector+28; model slot C (8226FB50) reads +10.
  const uint32_t selector = owner + 0x7C, model = owner + 0xBC;
  if (!Guard(memory.U32BE(selector) == 0x8200FC30 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x8200FC10,
             "vs_rules_selector_owner") ||
      !Guard(memory.U32BE(model + 0x14) == 15, "vs_rules_selector_count")) return false;
  const auto index = memory.U32BE(model + 0x10);
  if (!Guard(index && *index < 15, "vs_rules_selector_index")) return false;
  selected = *index;
  return true;
}

bool Value(uint32_t owner, uint32_t row, std::string& value) {
  // 8233B650 follows the bound model's +10 pointer to current values.
  // The +14 pointer is the previous value used only by its transition animation.
  if (!Guard(memory.U32BE(owner + 0x54) == owner + 0x2C &&
             memory.U32BE(owner + 0x58) == owner + 0x38,
             "vs_rules_value_ownership")) return false;
  if (row == 3) {
    const auto level = memory.U32BE(owner + 0x30);
    return Guard(level && *level < 5, "vs_rules_level") && Text(0x828 + *level, value);
  }
  constexpr std::array<uint32_t, 7> offsets{0x2C, 0x2D, 0x2E, 0, 0x34, 0x35, 0x36};
  const auto current = memory.U8(owner + offsets[row]);
  if (!Guard(current && *current <= (row == 0 ? 2 : 1), "vs_rules_value")) return false;
  const uint32_t id = row == 0 ? 0x95A + *current :
                      row == 1 ? 0x95E + (*current == 0) :
                      row == 2 ? 0x961 + (*current == 0) : 0x64C + (*current == 0);
  return Text(id, value);
}

void Focus(uint32_t owner, Rules& rules) {
  if (!rules.active || !output || !Owner(owner) ||
      !Guard(memory.U32BE(owner + 0x14) == 3, "vs_rules_active_state")) return;
  uint32_t selection = 0;
  if (!Selection(owner, selection)) return;
  if (!Guard(memory.U32BE(owner + 0x18) == rules.parent + 0x42C &&
             memory.U32BE(owner + 0x1C) == rules.parent + 0x2C &&
             memory.U32BE(owner + 0x20) == rules.parent + 0x424,
             "vs_rules_parent_binding")) return;
  const auto flags = memory.U32BE(owner + 0xC4);
  if (!Guard(flags && *flags && *flags <= UINT32_MAX - 15,
             "vs_rules_navigation_flags")) return;
  const auto row = selection / 2;
  auto event = Event(rules, "focused");
  // 82339D98 enables both arrows of each logical row from these flags.
  uint32_t preceding = 0, total = 1;
  for (uint32_t i = 0; i < 7; ++i) {
    const auto enabled = memory.U8(owner + 0x5E + i);
    if (!Guard(enabled && *enabled <= 1, "vs_rules_row_enabled")) return;
    if (!Guard(memory.U8(*flags + i * 2) == *enabled &&
               memory.U8(*flags + i * 2 + 1) == *enabled,
               "vs_rules_navigation_enabled")) return;
    if (!Guard(i != row || *enabled != 0, "vs_rules_disabled_selection")) return;
    total += *enabled;
    if (i < row) preceding += *enabled;
  }
  if (!Guard(memory.U8(*flags + 14) == 1, "vs_rules_confirm_enabled")) return;
  event.position = preceding + 1;
  event.total = total;
  if (row == 7) {
    event.label_key = "vs_rules_confirm";
  } else {
    constexpr std::array<uint32_t, 7> labels{0xA28, 0x95D, 0x960, 0x827, 0x9A7, 0x644, 0x7EE};
    if (!Text(labels[row], event.label) || !Value(owner, row, event.value)) return;
  }
  if (!rules.opened) {
    auto opening = Event(rules, "opened");
    if (!Text(0x627, opening.title) || !Text(0x639, opening.body)) return;
    output(std::move(opening));
    rules.opened = true;
  }
  if (rules.focused && rules.item.label == event.label &&
      rules.item.label_key == event.label_key && rules.item.value == event.value &&
      rules.item.position == event.position && rules.item.total == event.total) return;
  if (!rules.focused) {
    std::string hints;
    if (!Text(0xA1D, hints)) return;
    event.hints.push_back(std::move(hints));
  }
  rules.item = event;
  output(std::move(event));
  rules.focused = true;
}

void Close(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found == screens.end()) return;
  if (found->second.opened && output) output(Event(found->second, "closed"));
  screens.erase(found);
}
}  // namespace

void SetVsRulesCapture(brawlers::ui::ReadBytes reader,
                       std::function<void(ScreenEvent&&)> sink,
                       std::function<void(std::string_view)> report,
                       std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsRulesTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

void NotifyVsRulesSelection(uint32_t selector, uint32_t selected) noexcept {
  Observe([&] {
    if (selector < 0x7C) return;
    const auto found = screens.find(selector - 0x7C);
    if (found == screens.end() || !found->second.active) return;
    uint32_t actual = 0;
    if (!Guard(selected < 15, "vs_rules_selection_event") ||
        !Owner(found->first) || !Selection(found->first, actual) ||
        !Guard(actual == selected, "vs_rules_selection_event_mismatch")) return;
    Focus(found->first, found->second);
  });
}

extern "C" void sub_82339BD0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32;
  const auto config = ctx.r5.u32, arena = ctx.r6.u32, mode = ctx.r7.u32;
  const auto layer = ctx.r8.u32;
  const auto enabled = ctx.r9.u8;
  __imp__sub_82339BD0(ctx, base);
  Observe([&] {
    if (!output || completion < 0x42C) return;
    const uint32_t parent = completion - 0x42C;
    if (!Guard(parent && parent <= UINT32_MAX - 0x430 &&
               memory.U32BE(parent) == 0x8201A078 &&
               memory.U32BE(parent + 0x14) == 9 && config == parent + 0x2C &&
               arena == parent + 0x424 && mode == parent + 0x20 &&
               layer == 1 && enabled == 1, "vs_rules_parent") || !Owner(owner)) return;
    if (!Guard(!screens.contains(owner), "vs_rules_duplicate_lifetime")) return;
    const auto common = memory.U32BE(0x829CBC68);
    if (!Guard(common && *common && *common <= UINT32_MAX - 0xE0 &&
               memory.U32BE(*common) == 0x82003EAC, "vs_rules_common_owner")) return;
    const auto resource = memory.U16BE(*common + 0x20);
    if (!Guard(resource && *resource >= 0x8CC && *resource <= 0x8D2,
               "vs_rules_common_locale")) return;
    Rules rules;
    rules.id = ++next_identity;
    rules.parent = parent;
    rules.locale = *resource - 0x8CB;
    screens.emplace(owner, std::move(rules));
  });
}

extern "C" void sub_82339910(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_82339910(ctx, base);
  Observe([&] {
    const auto found = screens.find(owner);
    if (found == screens.end() || !found->second.active) return;
    if (!Owner(owner)) return;
    const auto changed = memory.U8(owner + 0x5D);
    if (!Guard(changed && *changed <= 1, "vs_rules_changed_flag") || !*changed) return;
    Focus(owner, found->second);
  });
}

extern "C" void sub_82339878(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_82339878(ctx, base);
}

void BrawlersVsRulesReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    found->second.active = true;
    Focus(owner.u32, found->second);
  });
}

void BrawlersVsRulesSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found != screens.end()) found->second.active = false;
  });
}

void BrawlersVsRulesLabelDraw(PPCRegister& widget) {
  Observe([&] {
    // Retry a delayed first item without re-reading settled screens every frame.
    for (auto& [owner, rules] : screens) {
      if (!rules.active || rules.focused) continue;
      if (!Guard(widget.u32 && widget.u32 <= UINT32_MAX - 0x104 &&
                 memory.U32BE(widget.u32) == 0x82019F0C, "vs_rules_label_widget")) return;
      Focus(owner, rules);
    }
  });
}
