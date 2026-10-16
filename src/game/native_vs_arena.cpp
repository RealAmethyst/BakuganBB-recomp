// Observes the native VS arena selection after navigation and difficulty settle.
#include "native_vs_arena.h"
#include "native_localization.h"

#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_822F4530(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822F5B40(PPCContext&, uint8_t*);

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
struct Arena {
  uint64_t id = 0;
  uint32_t parent = 0, locale = 0;
  bool active = false, opened = false, pending = false;
  ScreenEvent last;
};
std::map<uint32_t, Arena> screens;

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
    // Never replace a failed runtime lock with an unprotected guest read.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Owner(uint32_t owner, const Arena& arena) {
  return Guard(owner && owner <= UINT32_MAX - 0xC4 &&
               memory.U32BE(owner) == 0x82017F2C &&
               memory.U32BE(owner + 0x20) == 0x82017F20,
               "vs_arena_owner") &&
         Guard(memory.U32BE(0x829CC54C) == arena.parent &&
               memory.U32BE(arena.parent) == 0x8201A078,
               "vs_arena_parent") &&
         Guard(memory.U32BE(owner + 0x18) == arena.parent + 0x42C &&
               memory.U32BE(owner + 0x48) == arena.parent + 0x2C &&
               memory.U32BE(owner + 0x4C) == arena.parent + 0x424 &&
               memory.U32BE(owner + 0x50) == arena.parent + 0x20 &&
               memory.U32BE(owner + 0x54) == arena.parent + 0x44,
               "vs_arena_configuration_ownership") &&
         Guard(memory.U32BE(owner + 0x2C) == owner + 0x60 &&
               memory.U32BE(owner + 0x30) == owner + 0xC0 &&
               memory.U32BE(owner + 0x34) == arena.parent + 0x425 &&
               memory.U32BE(owner + 0x38) == owner + 0xC1,
               "vs_arena_render_model");
}

ScreenEvent Event(const Arena& arena, const char* kind) {
  ScreenEvent event;
  event.source = "vs_arena";
  event.kind = kind;
  event.id = arena.id;
  event.locale = arena.locale;
  return event;
}

bool Text(uint32_t id, std::string& value) {
  const auto result = brawlers::ui::ReadMenuText(memory, id);
  if (!Guard(result.value.has_value(), result.reason)) return false;
  value = *result.value;
  return true;
}

void Focus(uint32_t owner, Arena& arena) {
  if (!arena.active || !output || !Owner(owner, arena) ||
      !Guard(memory.U32BE(owner + 0x14) == 3 &&
             memory.U32BE(arena.parent + 0x14) == 12, "vs_arena_active_state")) return;
  const auto selected = memory.U8(owner + 0xC0);
  const auto level = memory.U8(arena.parent + 0x425);
  const auto maximum = memory.U8(owner + 0xC1);
  if (!Guard(selected && *selected < 9, "vs_arena_selection") ||
      !Guard(level && maximum && *maximum <= 2 && *level <= *maximum,
             "vs_arena_difficulty")) return;
  const uint32_t selector = owner + 0x60, model = owner + 0xA0;
  const auto cursor = memory.U32BE(model + 0x10);
  if (!Guard(memory.U32BE(selector) == 0x8200FC30 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x8200FC10 &&
             memory.U32BE(model + 4) == 0x82017EB8 &&
             memory.U32BE(model + 0x14) == 9 &&
             cursor && *cursor < 9, "vs_arena_selector")) return;
  // Alternate input can leave the raw cursor on an unavailable arena. The
  // caption and difficulty consumers retain the accepted selection at C0.
  auto event = Event(arena, "focused");
  // Native navigation skips the arena availability flags, not grid enable bits.
  for (uint32_t i = 0; i < 9; ++i) {
    const auto enabled = memory.U8(owner + 0x3C + i);
    if (!Guard(enabled && *enabled <= 1, "vs_arena_availability") ||
        !Guard(i != *selected || *enabled != 0, "vs_arena_unavailable_selection")) return;
    event.total += *enabled;
    if (i <= *selected) event.position += *enabled;
  }
  event.label_key = "vs_arena_name_" + std::to_string(*selected);
  event.value_key = "vs_arena_level_" + std::to_string(*level);
  if (!arena.opened) {
    auto opening = Event(arena, "opened");
    if (!Text(0x629, opening.title)) return;
    // A0A is the displayed difficulty instruction, not a second screen heading.
    std::string hint;
    if (!Text(0xA0A, hint)) return;
    event.hints.push_back(std::move(hint));
    const auto shortcuts = memory.U8(owner + 0x5D);
    if (!Guard(shortcuts && *shortcuts <= 1, "vs_arena_shortcuts")) return;
    if (*shortcuts) {
      for (const auto id : {0xA02U, 0xA03U}) {
        if (!Text(id, hint)) return;
        event.hints.push_back(hint);
      }
    }
    if (!Text(0x9EC, hint)) return;
    event.hints.push_back(std::move(hint));
    output(std::move(opening));
    arena.opened = true;
  }
  if (arena.last.label_key == event.label_key && arena.last.value_key == event.value_key &&
      arena.last.position == event.position && arena.last.total == event.total) return;
  arena.last = event;
  output(std::move(event));
}

void Close(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found == screens.end()) return;
  if (found->second.opened && output) output(Event(found->second, "closed"));
  screens.erase(found);
}
}  // namespace

void SetVsArenaCapture(brawlers::ui::ReadBytes reader,
                       std::function<void(ScreenEvent&&)> sink,
                       std::function<void(std::string_view)> report,
                       std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsArenaTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

extern "C" void sub_822F5B40(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_822F5B40(ctx, base);
}

extern "C" void sub_822F4530(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32;
  __imp__sub_822F4530(ctx, base);
  Observe([&] {
    if (!output) return;
    const auto parent = memory.U32BE(0x829CC54C);
    if (!parent || !*parent || *parent > UINT32_MAX - 0x440 || completion != *parent + 0x42C)
      return;  // The same screen also has unrelated native callers.
    Arena arena;
    arena.parent = *parent;
    if (!Owner(owner, arena) ||
        !Guard(memory.U32BE(*parent + 0x14) == 11, "vs_arena_constructor_state") ||
        !Guard(!screens.contains(owner), "vs_arena_duplicate_lifetime")) return;
    const auto resources = memory.U32BE(0x829CC358);
    if (!Guard(resources && *resources && *resources <= UINT32_MAX - 0x1C,
               "vs_arena_resources")) return;
    const auto loader = memory.U32BE(*resources + 0xC);
    if (!Guard(loader && *loader && *loader <= UINT32_MAX - 0x24 &&
               memory.U32BE(*loader) == 0x82003F2C, "vs_arena_loader")) return;
    const auto request = memory.U16BE(*loader + 0x20);
    if (!Guard(request && *request >= 0x91C && *request <= 0x922,
               "vs_arena_locale")) return;
    arena.locale = *request - 0x91B;
    arena.id = ++next_identity;
    screens.emplace(owner, std::move(arena));
  });
}

void BrawlersVsArenaReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    found->second.active = true;
    Focus(owner.u32, found->second);
  });
}

void BrawlersVsArenaSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found != screens.end()) found->second.active = found->second.pending = false;
  });
}

void BrawlersVsArenaNavigationChanged(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found != screens.end() && found->second.active) found->second.pending = true;
  });
}

void BrawlersVsArenaUpdateFinished(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end() || !found->second.pending) return;
    found->second.pending = false;
    Focus(owner.u32, found->second);
  });
}

void BrawlersVsArenaValueChanged(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found != screens.end()) Focus(owner.u32, found->second);
  });
}
