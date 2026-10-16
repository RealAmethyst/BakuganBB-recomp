// Captures final native character input results; speech policy owns controller order.
#include "native_vs_characters.h"
#include "native_localization.h"

#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_822E9370(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822ECCB0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822E89B8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822E8230(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822E76C8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822E7A40(PPCContext&, uint8_t*);

namespace {
using brawlers::ui::GuestUiMemory;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;
std::function<void(VsCharactersCapture&&)> output;
std::function<void(std::string_view)> diagnostic;
std::function<void()> failure;
std::set<std::string> reported;
uint64_t next_identity = 0;
bool failed = false;

struct Characters {
  uint64_t id = 0;
  uint32_t parent = 0, locale = 0, input_depth = 0;
  uint8_t dirty = 0, difficulty = 0, resume = 0;
  uint8_t pending_focus = 0, pending_difficulty = 0;
  std::array<uint32_t, 4> difficulty_slot{};
  bool active = false, opened = false, invalid = false;
  bool resume_started = false;
  bool team_active = false, team_dirty = false, team_pending = false, team_entered = false;
  std::array<std::string, 4> last;
  std::string last_team;
};
std::map<uint32_t, Characters> screens;

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
    // Failed locking never permits unprotected guest reads.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Owner(uint32_t owner, const Characters& screen) {
  return Guard(owner && owner <= UINT32_MAX - 0x33C &&
               memory.U32BE(owner) == 0x82017C50, "vs_characters_owner") &&
         Guard(memory.U32BE(0x829CC54C) == screen.parent &&
               memory.U32BE(screen.parent) == 0x8201A078,
               "vs_characters_parent") &&
         Guard(memory.U32BE(owner + 0x18) == screen.parent + 0x42C &&
               memory.U32BE(owner + 0x2E0) == screen.parent + 0x44 &&
               memory.U32BE(owner + 0x2E4) == screen.parent + 0x20 &&
               memory.U8(owner + 0x1F5) == 0, "vs_characters_configuration") &&
         Guard(memory.U32BE(owner + 0x1FC) == 0x82017C48 &&
               memory.U32BE(owner + 0x208) == owner + 0x20,
               "vs_characters_render_model");
}

ScreenEvent Event(const Characters& screen, const char* kind) {
  ScreenEvent event;
  event.source = "vs_characters";
  event.id = screen.id;
  event.locale = screen.locale;
  event.kind = kind;
  return event;
}

template <typename Content>
void Emit(const Characters& screen, uint32_t controller, Content content, bool continuation = false) {
  output(VsCharactersCapture{controller, screen.id, screen.locale, std::move(content), continuation});
}

bool Text(uint32_t id, std::string& text) {
  const auto result = brawlers::ui::ReadMenuText(memory, id);
  if (!Guard(result.value.has_value(), result.reason)) return false;
  text = *result.value;
  return true;
}

bool Open(Characters& screen, bool team = false) {
  if (screen.opened) return true;
  const auto common = memory.U32BE(0x829CBC68);
  if (!Guard(common && *common && *common <= UINT32_MAX - 0xE0 &&
             memory.U32BE(*common) == 0x82003EAC, "vs_characters_common") ||
      !Guard(memory.U32BE(*common + 0x58) == 1 && memory.U8(*common + 0x54) == 2,
             "vs_characters_title_record")) return false;
  auto opening = Event(screen, "opened");
  if (!Text(0x626, opening.title)) return false;
  for (auto id : {0xA04U, 0x9ECU}) {
    if (team && id == 0xA04) continue;
    std::string hint;
    if (!Text(id, hint)) return false;
    opening.hints.push_back(std::move(hint));
  }
  Emit(screen, UINT32_MAX, std::move(opening));
  screen.opened = true;
  return true;
}

struct Slot {
  uint32_t index = 0, character = 0, level = 0;
  bool computer = false;
};

bool ReadSlot(uint32_t owner, uint32_t index, Slot& slot) {
  const uint32_t record = owner + 0x20 + index * 0xC;
  const auto character = memory.U8(record + 1);
  const auto level = memory.U8(record + 2);
  const auto computer = memory.U8(record + 3);
  if (!Guard(index < 4 && character && *character <= 17 && level && *level <= 2 &&
             computer && *computer <= 1 && (index != 0 || !*computer),
             "vs_characters_slot")) return false;
  slot = {index, *character, *level, *computer != 0};
  return true;
}

bool AssignedSlot(uint32_t owner, uint32_t controller, uint32_t players, Slot& slot) {
  uint32_t selected = 4;
  for (uint32_t i = 0; i < 4; ++i) {
    const auto assigned = memory.U8(owner + 0x24 + i * 0xC);
    if (!Guard(assigned && (*assigned < 4 || *assigned == 0xFF),
               "vs_characters_controller_assignment")) return false;
    if (*assigned != controller) continue;
    if (!Guard(i < players && selected == 4, "vs_characters_unique_assignment")) return false;
    selected = i;
  }
  if (selected == 4) return false;  // An unassigned controller has no focused slot.
  return ReadSlot(owner, selected, slot);
}

bool Selection(uint32_t owner, uint32_t controller, uint32_t& current,
               uint32_t& position, uint32_t& total) {
  const uint32_t selector = owner + 0x74 + controller * 0x60;
  const uint32_t model = selector + 0x40;
  if (!Guard(memory.U32BE(selector) == 0x8200FC30 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x8200FC10,
             "vs_characters_selector_owner") ||
      !Guard(memory.U32BE(model + 4) == 0x82017B80 &&
             memory.U32BE(model + 0x14) == 24, "vs_characters_selector_graph")) return false;
  const auto valid = memory.U8(model + 0x18);
  if (!Guard(valid && *valid <= 1, "vs_characters_selector_valid")) return false;
  if (!*valid) return false;
  const auto index = memory.U32BE(model + 0x10);
  const auto flags = memory.U32BE(model + 8);
  if (!Guard(index && *index < 23, "vs_characters_final_selection") ||
      !Guard(flags && *flags && *flags <= UINT32_MAX - 24,
             "vs_characters_enabled_storage")) return false;
  current = *index;
  for (uint32_t i = 0; i < 24; ++i) {
    const auto enabled = memory.U8(*flags + i);
    if (!Guard(enabled && *enabled <= 1, "vs_characters_enabled_flag") ||
        !Guard(i != current || *enabled != 0, "vs_characters_disabled_selection")) return false;
    // 23 redirects to a player-slot button inside 822E89B8; it is never a final item.
    if (i == 23) continue;
    total += *enabled;
    if (i <= current) position += *enabled;
  }
  return true;
}

void Focus(uint32_t owner, Characters& screen, uint32_t controller, uint32_t players) {
  screen.pending_focus &= ~(1U << controller);
  Slot slot;
  if (!AssignedSlot(owner, controller, players, slot)) return;
  uint32_t current = 0, position = 0, total = 0;
  if (!Selection(owner, controller, current, position, total)) return;
  auto event = Event(screen, (screen.resume & (1U << controller)) ? "resumed" : "focused");
  event.section_key = "vs_identity_" + std::to_string(slot.index + (slot.computer ? 3 : 0));
  event.position = position;
  event.total = total;
  Slot target;
  std::string target_name;
  if (current < 16) {
    // 822E7D58 -> participant+EA -> 822DBB68(D99+id), bank zero.
    if (!Text(0xD99 + current, event.label)) {
      screen.pending_focus |= 1U << controller;
      return;
    }
    event.selected = slot.character == current;
  } else if (current == 16 || current == 17) {
    event.label_key = current == 16 ? "vs_characters_random" : "vs_characters_custom";
    event.selected = current == 17 && slot.character == 17;
  } else if (current == 22) {
    event.label_key = "vs_characters_confirm";
  } else {
    if (!Guard(current - 18 < players, "vs_characters_target_slot") ||
        !ReadSlot(owner, current - 18, target)) return;
    if (target.character < 16 && !Text(0xD99 + target.character, target_name)) {
      screen.pending_focus |= 1U << controller;
      return;
    }
  }
  const auto fingerprint = std::to_string(current) + ":" + event.section_key + ":" +
      event.label + ":" + event.label_key + ":" + std::to_string(event.selected) + ":" +
      std::to_string(position) + ":" + std::to_string(total) + ":" +
      std::to_string(target.character) + ":" + std::to_string(target.level) + ":" +
      std::to_string(target.computer) + ":" + target_name;
  if (!(screen.resume & (1U << controller)) && screen.last[controller] == fingerprint) return;
  if (!Open(screen)) {
    screen.pending_focus |= 1U << controller;
    return;
  }
  const bool resumed = event.kind == "resumed";
  const bool continuation = resumed && screen.resume_started;
  if (current >= 18 && current <= 21) {
    Emit(screen, controller, VsCharactersPlayerSlot{slot.index, target.index, target.character,
         target.level, position, total, target.computer, event.kind == "resumed",
         event.section_key, std::move(target_name)}, continuation);
  } else {
    Emit(screen, controller, std::move(event), continuation);
  }
  if (resumed) screen.resume_started = true;
  screen.last[controller] = fingerprint;
  screen.resume &= ~(1U << controller);
}

void Flush(uint32_t owner, Characters& screen) {
  const auto dirty = std::exchange(screen.dirty, 0);
  const auto difficulty = std::exchange(screen.difficulty, 0);
  if (!(dirty | difficulty) || !screen.active || screen.invalid || !output) return;
  if (!Owner(owner, screen) ||
      !Guard(memory.U32BE(owner + 0x14) == 3 && memory.U32BE(screen.parent + 0x14) == 6,
             "vs_characters_active_state") ||
      !Guard(memory.U8(owner + 0x2FD) == 0, "vs_characters_input_section")) return;
  const auto controllers = memory.U8(owner + 0x1F4);
  const auto players = memory.U8(screen.parent + 0x29);
  if (!Guard(controllers && *controllers >= 1 && *controllers <= 4 &&
             players && *players >= 2 && *players <= 4,
             "vs_characters_player_counts")) return;
  for (uint32_t controller = 0; controller < *controllers; ++controller) {
    if (dirty & (1U << controller)) Focus(owner, screen, controller, *players);
    if (!(difficulty & (1U << controller))) continue;
    screen.pending_difficulty &= ~(1U << controller);
    Slot slot;
    if (!Guard(screen.difficulty_slot[controller] < *players,
               "vs_characters_difficulty_target") ||
        !ReadSlot(owner, screen.difficulty_slot[controller], slot) ||
        !Guard(slot.computer, "vs_characters_difficulty_slot")) continue;
    VsCharactersDifficulty value{slot.index, slot.character, slot.level, {}};
    if ((slot.character < 16 && !Text(0xD99 + slot.character, value.character_name)) || !Open(screen)) {
      screen.pending_difficulty |= 1U << controller;
      continue;
    }
    Emit(screen, controller, std::move(value));
  }
}

void FlushTeam(uint32_t owner, Characters& screen) {
  if (!std::exchange(screen.team_dirty, false) || !screen.team_active ||
      screen.invalid || !output) return;
  if (!Owner(owner, screen) ||
      !Guard(memory.U32BE(owner + 0x14) == 8 && memory.U32BE(screen.parent + 0x14) == 6 &&
             memory.U32BE(screen.parent + 0x20) == 1, "vs_characters_team_state") ||
      !Guard(memory.U32BE(owner + 0x260) == 0x82017C4C &&
             memory.U32BE(owner + 0x26C) == 0x82003900 &&
             memory.U32BE(owner + 0x294) == owner + 0x2AC &&
             memory.U32BE(owner + 0x2AC) == 0x820038DC,
             "vs_characters_team_selector") ||
      !Guard(memory.U32BE(owner + 0x2B4) == 0 && memory.U32BE(owner + 0x2B8) == 3 &&
             memory.U32BE(owner + 0x2C0) == 0, "vs_characters_team_bounds")) return;
  const auto valid = memory.U8(owner + 0x2C4);
  if (!Guard(valid && *valid <= 1, "vs_characters_team_valid") || !*valid) return;
  const auto current = memory.U32BE(owner + 0x2B0);
  if (!Guard(current && *current < 3, "vs_characters_team_current")) return;
  const auto resources = memory.U32BE(0x829CC358);
  if (!Guard(resources && *resources && *resources <= UINT32_MAX - 0x18,
             "vs_characters_team_resources")) return;
  const auto loader = memory.U32BE(*resources + 0x14);
  if (!Guard(loader && *loader && *loader <= UINT32_MAX - 0x24 &&
             memory.U32BE(*loader) == 0x82003F2C &&
             memory.U16BE(*loader + 0x20) == 0x92B + screen.locale,
             "vs_characters_team_locale")) return;

  VsCharactersTeam team;
  team.choice = *current;
  team.position = *current + 1;
  team.total = 3;
  team.entered = screen.team_entered;
  uint32_t seen = 0;
  std::string fingerprint = std::to_string(*current);
  for (uint32_t i = 0; i < 4; ++i) {
    const auto slot = memory.U32BE(0x82017B08 + (*current * 4 + i) * 4);
    if (!Guard(slot && *slot < 4 && !(seen & (1U << *slot)),
               "vs_characters_team_permutation")) return;
    const auto computer = memory.U8(owner + 0x2C8 + *slot);
    if (!Guard(computer && *computer <= 1 && (*slot != 0 || !*computer),
               "vs_characters_team_identity")) return;
    seen |= 1U << *slot;
    team.participants[i] = *slot;
    team.identity_frames[i] = *slot + (*computer ? 3 : 0);
    fingerprint += ":" + std::to_string(team.identity_frames[i]);
  }
  if (!screen.team_entered && screen.last_team == fingerprint) {
    screen.team_pending = false;
    return;
  }
  std::string hint;
  if (!Text(0x64A, team.section) || !Text(0x9EC, hint) || !Open(screen, true)) {
    screen.team_pending = true;
    return;
  }
  team.hints.push_back(std::move(hint));
  Emit(screen, 0, std::move(team));
  screen.last_team = std::move(fingerprint);
  screen.team_pending = screen.team_entered = false;
}

void Dirty(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found != screens.end() && found->second.active && !found->second.invalid)
    found->second.dirty = 0xF;
}

void Close(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found == screens.end()) return;
  if (found->second.opened && output) Emit(found->second, UINT32_MAX, Event(found->second, "closed"));
  screens.erase(found);
}

void Ready(uint32_t owner, bool resumed) {
  const auto found = screens.find(owner);
  if (found == screens.end() || found->second.invalid) return;
  auto& screen = found->second;
  screen.active = true;
  screen.resume_started = false;
  screen.team_active = screen.team_dirty = screen.team_pending = false;
  screen.dirty = 0xF;
  if (resumed && screen.opened) screen.resume = 0xF;
  if (!screen.input_depth) Flush(owner, screen);
}
}  // namespace

void SetVsCharactersCapture(brawlers::ui::ReadBytes reader,
                           std::function<void(VsCharactersCapture&&)> sink,
                           std::function<void(std::string_view)> report,
                           std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsCharactersTaskRemoved(uint32_t owner) noexcept { Observe([&] { Close(owner); }); }

void NotifyVsCharactersSelection(uint32_t selector, uint32_t index) noexcept {
  Observe([&] {
    if (selector >= 0x26C) {
      const auto team = screens.find(selector - 0x26C);
      if (team != screens.end() && team->second.team_active && team->second.input_depth) {
        if (Guard(index < 3, "vs_characters_team_selection_event")) team->second.team_dirty = true;
        return;
      }
    }
    for (uint32_t controller = 0; controller < 4; ++controller) {
      const uint32_t offset = 0x74 + controller * 0x60;
      if (selector < offset) continue;
      const auto found = screens.find(selector - offset);
      if (found == screens.end() || !found->second.active || !found->second.input_depth) continue;
      if (Guard(index < 24, "vs_characters_selection_event"))
        found->second.dirty |= 1U << controller;
    }
  });
}

extern "C" void sub_822E9370(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32;
  const auto participants = ctx.r5.u32, config = ctx.r6.u32, layer = ctx.r7.u32;
  const auto mode = ctx.r9.u8;
  __imp__sub_822E9370(ctx, base);
  Observe([&] {
    if (!output || completion < 0x42C || layer != 1 || mode != 0) return;
    Characters screen;
    screen.parent = completion - 0x42C;
    if (!Guard(screen.parent && screen.parent <= UINT32_MAX - 0x440 &&
               participants == screen.parent + 0x44 && config == screen.parent + 0x20,
               "vs_characters_constructor_arguments") || !Owner(owner, screen) ||
        !Guard(memory.U32BE(screen.parent + 0x14) == 5, "vs_characters_constructor_state")) return;
    const auto duplicate = screens.find(owner);
    if (duplicate != screens.end()) {
      duplicate->second.invalid = true;
      Guard(false, "vs_characters_duplicate_lifetime");
      return;
    }
    const auto resources = memory.U32BE(0x829CC358);
    if (!Guard(resources && *resources && *resources <= UINT32_MAX - 0x1C,
               "vs_characters_resources")) return;
    const auto loader = memory.U32BE(*resources + 8);
    if (!Guard(loader && *loader && *loader <= UINT32_MAX - 0x24 &&
               memory.U32BE(*loader) == 0x82003F2C, "vs_characters_loader")) return;
    const auto resource = memory.U16BE(*loader + 0x20);
    if (!Guard(resource && *resource >= 0x914 && *resource <= 0x91A,
               "vs_characters_locale")) return;
    screen.locale = *resource - 0x913;
    screen.id = ++next_identity;
    screens.emplace(owner, std::move(screen));
  });
}

extern "C" void sub_822ECCB0(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_822ECCB0(ctx, base);
}

extern "C" void sub_822E89B8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end()) ++found->second.input_depth;
  });
  __imp__sub_822E89B8(ctx, base);
  const auto result = ctx.r3.u8;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found == screens.end()) return;
    auto& screen = found->second;
    if (screen.input_depth) --screen.input_depth;
    if (result != 0) {
      screen.dirty = screen.difficulty = 0;
      screen.pending_focus = screen.pending_difficulty = 0;
      return;
    }
    if (!screen.input_depth) Flush(owner, screen);
  });
}

extern "C" void sub_822E8230(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822E8230(ctx, base);
  if (ctx.r3.u8 == 1) Observe([&] { Dirty(owner); });
}

extern "C" void sub_822E7A40(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end()) ++found->second.input_depth;
  });
  __imp__sub_822E7A40(ctx, base);
  const auto result = ctx.r3.s32;
  Observe([&] {
    const auto found = screens.find(owner);
    if (found == screens.end()) return;
    auto& screen = found->second;
    if (screen.input_depth) --screen.input_depth;
    // Native -1 keeps the section; 0..2 confirm a composition and -2 cancels.
    if (result != -1) {
      screen.team_active = screen.team_dirty = screen.team_pending = false;
      return;
    }
    if (!screen.input_depth) FlushTeam(owner, screen);
  });
}

extern "C" void sub_822E76C8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822E76C8(ctx, base);
  Observe([&] { Dirty(owner); });
}

void BrawlersVsCharactersReady(PPCRegister& owner) { Observe([&] { Ready(owner.u32, false); }); }
void BrawlersVsCharactersResumed(PPCRegister& owner) { Observe([&] { Ready(owner.u32, true); }); }
void BrawlersVsCharactersChanged(PPCRegister& owner) { Observe([&] { Dirty(owner.u32); }); }

void BrawlersVsCharactersSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    found->second.active = false;
    found->second.resume_started = false;
    found->second.dirty = found->second.difficulty = 0;
    found->second.pending_focus = found->second.pending_difficulty = 0;
    found->second.team_active = found->second.team_dirty = found->second.team_pending = false;
  });
}

void BrawlersVsCharactersDifficultyChanged(PPCRegister& owner, PPCRegister& controller,
                                          PPCRegister& slot) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end() || !found->second.active || !found->second.input_depth) return;
    if (!Guard(controller.u32 < 4, "vs_characters_difficulty_controller") ||
        !Guard(slot.u32 < 4, "vs_characters_difficulty_target")) return;
    // r19 retains the changed slot even if this operation handed its controller
    // to another slot before the shoulder-button write through r23.
    found->second.difficulty_slot[controller.u32] = slot.u32;
    found->second.difficulty |= 1U << controller.u32;
  });
}

void BrawlersVsCharactersRendered(PPCRegister& model, PPCRegister& widget) {
  Observe([&] {
    if (model.u32 < 0x1FC) return;
    const auto owner = model.u32 - 0x1FC;
    const auto found = screens.find(owner);
    if (found == screens.end()) return;
    auto& screen = found->second;
    if (!(screen.pending_focus | screen.pending_difficulty) || !screen.active ||
        screen.invalid || screen.input_depth) return;
    // The visible renderer retains the exact bound model. Team widgets share
    // this type, so the widget alone cannot establish primary ownership.
    if (!Guard(widget.u32 && memory.U32BE(widget.u32) == 0x82017CCC,
               "vs_characters_retry_widget")) return;
    screen.dirty |= screen.pending_focus;
    screen.difficulty |= screen.pending_difficulty;
    Flush(owner, screen);
  });
}

void BrawlersVsCharactersTeamReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end() || found->second.invalid) return;
    auto& screen = found->second;
    screen.active = false;
    screen.dirty = screen.difficulty = screen.pending_focus = screen.pending_difficulty = 0;
    screen.team_active = screen.team_dirty = screen.team_entered = true;
    if (!screen.input_depth) FlushTeam(owner.u32, screen);
  });
}

void BrawlersVsCharactersTeamRendered(PPCRegister& model, PPCRegister& widget) {
  Observe([&] {
    if (model.u32 < 0x260) return;
    const auto owner = model.u32 - 0x260;
    const auto found = screens.find(owner);
    if (found == screens.end()) return;
    auto& screen = found->second;
    if (!screen.team_pending || !screen.team_active || screen.invalid || screen.input_depth) return;
    if (!Guard(widget.u32 && memory.U32BE(widget.u32) == 0x82017D10,
               "vs_characters_team_retry_widget")) return;
    screen.team_dirty = true;
    FlushTeam(owner, screen);
  });
}
