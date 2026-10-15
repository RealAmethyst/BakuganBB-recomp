// Match-entrance portrait identities are captured at their native construction.
#include "native_versus_speech.h"
#include "native_localization.h"

#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_823681D0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8236A688(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82368918(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82368750(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823688A8(PPCContext&, uint8_t*);

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

struct Entrance {
  uint64_t id = 0;
  uint32_t child = 0, type = 0;
  std::array<uint32_t, 3> characters{};
  size_t count = 0;
  bool building = false, invalid = false, opened = false;
};
std::map<uint32_t, Entrance> entrances;
thread_local uint32_t building_owner = 0;

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
    // A failed runtime lock cannot be bypassed with an unprotected guest read.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Task(uint32_t owner) {
  return Guard(owner && owner <= UINT32_MAX - 0x34 &&
               memory.U32BE(owner) == 0x82032BC8, "versus_task_owner");
}

bool Child(uint32_t child) {
  return Guard(child && child <= UINT32_MAX - 0x5C &&
               memory.U32BE(child + 0x40) == 0x82032BB8 &&
               memory.U32BE(child + 0x50) == 2, "versus_child_owner");
}

ScreenEvent Event(const Entrance& entrance, const char* kind) {
  ScreenEvent event;
  event.source = "battle_versus";
  event.kind = kind;
  event.id = entrance.id;
  return event;
}

void Close(uint32_t owner) {
  const auto found = entrances.find(owner);
  if (found == entrances.end()) return;
  if (found->second.opened) output(Event(found->second, "closed"));
  entrances.erase(found);
}

uint32_t Begin(uint32_t child) {
  for (auto& [owner, entrance] : entrances) {
    if (entrance.child != child) continue;
    if (entrance.opened || entrance.invalid) return 0;
    const auto ready = memory.U8(child + 0x1C);
    if (!Guard(!building_owner && !entrance.building, "versus_nested_builder") ||
        !Task(owner) || !Child(child) ||
        !Guard(memory.U32BE(owner + 0x2C) == child &&
               memory.U32BE(owner + 0x14) == 1 &&
               memory.U32BE(child + 0x58) == entrance.type &&
               ready && *ready != 0, "versus_builder_ready")) {
      entrance.invalid = true;
      return 0;
    }
    entrance.building = true;
    entrance.count = 0;
    return owner;
  }
  return 0;
}

void Portrait(uint32_t owner, uint32_t widget, uint32_t style, uint32_t character) {
  const auto found = entrances.find(owner);
  if (found == entrances.end()) return;
  auto& entrance = found->second;
  if (!entrance.building || entrance.invalid) return;
  const size_t first = entrance.type == 4 ? 1 : 0;
  if (!Guard(entrance.count < entrance.characters.size() - first,
             "versus_portrait_capacity") ||
      !Guard(style == 0 && character < 16, "versus_portrait_identity") ||
      !Guard(widget && widget <= UINT32_MAX - 0x110 &&
             memory.U32BE(widget) == 0x82032BE4, "versus_portrait_owner") ||
      !Guard(memory.U32BE(entrance.child + 0x30 +
                         static_cast<uint32_t>(first + entrance.count) * 4) == character,
             "versus_portrait_participant")) {
    entrance.invalid = true;
    return;
  }
  entrance.characters[entrance.count++] = character;
}

void Finish(uint32_t owner) {
  const auto found = entrances.find(owner);
  if (found == entrances.end()) return;
  auto& entrance = found->second;
  if (!entrance.building) return;
  entrance.building = false;
  if (entrance.invalid || entrance.opened ||
      !Guard(entrance.count != 0, "versus_empty_portraits") ||
      !Task(owner) || !Child(entrance.child) ||
      !Guard(memory.U32BE(owner + 0x2C) == entrance.child,
             "versus_completed_owner")) return;
  auto event = Event(entrance, "opened");
  for (size_t i = 0; i < entrance.count; ++i) {
    // The same scenario fields feed NPC records and 822DBB68(D99 + ID).
    const auto name = brawlers::ui::ReadMenuText(memory, 0xD99 + entrance.characters[i]);
    if (!Guard(name.value.has_value(), name.reason)) return;
    if (!event.body.empty()) event.body += ", ";
    event.body += *name.value;
  }
  output(std::move(event));
  entrance.opened = true;
}

}  // namespace

void SetVersusSpeechCapture(brawlers::ui::ReadBytes reader,
                           std::function<void(ScreenEvent&&)> sink,
                           std::function<void(std::string_view)> report,
                           std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVersusTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

extern "C" void sub_823681D0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  const auto mode = ctx.r5.u32;
  __imp__sub_823681D0(ctx, base);
  if (mode != 2) return;
  Observe([&] {
    if (!output || !Task(owner) ||
        !Guard(memory.U32BE(owner + 0x24) == 2, "versus_task_mode")) return;
    if (const auto previous = entrances.find(owner); previous != entrances.end()) {
      Guard(false, "versus_duplicate_lifetime");
      previous->second.invalid = true;
      return;
    }
    const auto child = memory.U32BE(owner + 0x2C);
    if (!Guard(child.has_value(), "versus_child_pointer") || !Child(*child)) return;
    const auto type = memory.U32BE(*child + 0x58);
    if (!Guard(type && *type < 6, "versus_battle_type")) return;
    Entrance entrance;
    entrance.id = ++next_identity;
    entrance.child = *child;
    entrance.type = *type;
    entrances.emplace(owner, std::move(entrance));
  });
}

extern "C" void sub_8236A688(PPCContext& ctx, uint8_t* base) {
  uint32_t owner = 0;
  Observe([&] { owner = Begin(ctx.r3.u32); });
  {
    struct Scope {
      uint32_t previous = building_owner;
      explicit Scope(uint32_t current) { building_owner = current; }
      ~Scope() { building_owner = previous; }
    } scope(owner);
    __imp__sub_8236A688(ctx, base);
  }
  if (owner) Observe([&] { Finish(owner); });
}

extern "C" void sub_82368918(PPCContext& ctx, uint8_t* base) {
  const auto owner = building_owner;
  const auto style = ctx.r5.u32;
  const auto character = ctx.r6.u32;
  __imp__sub_82368918(ctx, base);
  if (owner) Observe([&] { Portrait(owner, ctx.r3.u32, style, character); });
}

extern "C" void sub_82368750(PPCContext& ctx, uint8_t* base) {
  const auto child = ctx.r3.u32;
  Observe([&] {
    for (const auto& [owner, entrance] : entrances) {
      if (entrance.child == child) { Close(owner); break; }
    }
  });
  __imp__sub_82368750(ctx, base);
}

extern "C" void sub_823688A8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { Close(owner); });
  __imp__sub_823688A8(ctx, base);
}
