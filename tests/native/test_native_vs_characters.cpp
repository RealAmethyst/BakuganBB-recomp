// Original synthetic native-operation fixtures; no game or speech services.
#include "native_vs_characters.h"

#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_822E9370(PPCContext&, uint8_t*);
extern "C" void sub_822ECCB0(PPCContext&, uint8_t*);
extern "C" void sub_822E89B8(PPCContext&, uint8_t*);
extern "C" void sub_822E8230(PPCContext&, uint8_t*);
extern "C" void sub_822E76C8(PPCContext&, uint8_t*);
extern "C" void sub_822E7A40(PPCContext&, uint8_t*);
void BrawlersVsCharactersReady(PPCRegister&);
void BrawlersVsCharactersResumed(PPCRegister&);
void BrawlersVsCharactersChanged(PPCRegister&);
void BrawlersVsCharactersSuspended(PPCRegister&);
void BrawlersVsCharactersDifficultyChanged(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersVsCharactersRendered(PPCRegister&, PPCRegister&);
void BrawlersVsCharactersTeamReady(PPCRegister&);
void BrawlersVsCharactersTeamRendered(PPCRegister&, PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}

namespace {
constexpr uint32_t task = 0x1000, parent = 0x3000, common = 0x4000;
constexpr uint32_t resources = 0x5000, loader = 0x5100, flags = 0x6000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
constexpr uint32_t widget = 0x7000;
constexpr uint32_t team_widget = 0x7010, team_loader = 0x5200;
std::map<uint32_t, uint8_t> bytes;
std::vector<VsCharactersCapture> events;
std::vector<std::string> guards;
std::function<void()> input;
std::function<void()> accepted;
std::function<void()> transfer;
uint32_t input_result = 0;
unsigned originals = 0, reads = 0, failures = 0;
bool throw_output = false;
uint8_t opaque[1]{};

void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[address + i] = value >> (24 - i * 8);
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = value >> 8;
  bytes[address + 1] = value;
}
void Text(uint32_t id, const char16_t* text) {
  uint32_t address = resource + 0x10000 + id * 0x100;
  do { Unit(address, *text); address += 2; } while (*text++);
}
uint32_t Selector(uint32_t controller) { return task + 0x74 + controller * 0x60; }
void Current(uint32_t controller, uint32_t index) {
  Word(Selector(controller) + 0x50, index);
  bytes[Selector(controller) + 0x58] = 1;
  NotifyVsCharactersSelection(Selector(controller), index);
}
void TeamCurrent(uint32_t index) {
  Word(task + 0x2B0, index);
  bytes[task + 0x2C4] = 1;
  NotifyVsCharactersSelection(task + 0x26C, index);
}
void Fixture() {
  Word(task, 0x82017C50);
  Word(task + 0x14, 3);
  Word(task + 0x18, parent + 0x42C);
  Word(task + 0x2E0, parent + 0x44);
  Word(task + 0x2E4, parent + 0x20);
  Word(task + 0x1FC, 0x82017C48);
  Word(task + 0x208, task + 0x20);
  Word(widget, 0x82017CCC);
  Word(team_widget, 0x82017D10);
  Word(task + 0x260, 0x82017C4C);
  Word(task + 0x26C, 0x82003900);
  Word(task + 0x294, task + 0x2AC);
  Word(task + 0x2AC, 0x820038DC);
  Word(task + 0x2B0, 0);
  Word(task + 0x2B4, 0);
  Word(task + 0x2B8, 3);
  Word(task + 0x2C0, 0);
  bytes[task + 0x2C4] = 0;
  const uint32_t permutations[3][4] = {{0, 1, 2, 3}, {0, 2, 1, 3}, {0, 3, 1, 2}};
  for (uint32_t row = 0; row < 3; ++row)
    for (uint32_t i = 0; i < 4; ++i)
      Word(0x82017B08 + (row * 4 + i) * 4, permutations[row][i]);
  bytes[task + 0x1F4] = 2;
  bytes[task + 0x1F5] = 0;
  bytes[task + 0x2FD] = 0;
  for (uint32_t i = 0; i < 4; ++i) {
    bytes[task + 0x21 + i * 0xC] = i;
    bytes[task + 0x22 + i * 0xC] = 1;
    bytes[task + 0x23 + i * 0xC] = i >= 2;
    bytes[task + 0x24 + i * 0xC] = i < 2 ? i : 0xFF;
    bytes[task + 0x2C8 + i] = i >= 2;
    const auto selector = Selector(i), model = selector + 0x40;
    Word(selector, 0x8200FC30);
    Word(selector + 0x28, model);
    Word(model, 0x8200FC10);
    Word(model + 4, 0x82017B80);
    Word(model + 8, flags + i * 0x100);
    Word(model + 0x10, i ? i : 22);
    Word(model + 0x14, 24);
    bytes[model + 0x18] = 0;
    for (uint32_t j = 0; j < 24; ++j) bytes[flags + i * 0x100 + j] = 1;
  }
  Word(0x829CC54C, parent);
  Word(parent, 0x8201A078);
  Word(parent + 0x14, 5);
  Word(parent + 0x20, 1);
  bytes[parent + 0x29] = 4;
  Word(0x829CBC68, common);
  Word(common, 0x82003EAC);
  Word(common + 0x58, 1);
  bytes[common + 0x54] = 2;
  Word(0x829CC358, resources);
  Word(resources + 8, loader);
  Word(loader, 0x82003F2C);
  Unit(loader + 0x20, 0x914);
  Word(resources + 0x14, team_loader);
  Word(team_loader, 0x82003F2C);
  Unit(team_loader + 0x20, 0x92C);
  Word(0x829CEB78, provider);
  Word(provider, 0x82036508);
  Word(provider + 8, bank);
  bytes[provider + 0xC] = 1;
  bytes[bank + 0x60] = 1;
  Word(bank, resource);
  Word(bank + 4, resource + 0x4000);
  Word(bank + 8, resource + 0x20);
  Word(resource, 0x70617374);
  Word(resource + 4, 0x7274626C);
  bytes[resource + 0xB] = 1;
  Word(resource + 0xC, 0xE00);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x4000);
  for (uint32_t id = 0; id < 0xE00; ++id)
    Word(resource + 0x20 + id * 4, 0x10000 + id * 0x100);
  Text(0x626, u"Synthetic character screen");
  Text(0x64A, u"Synthetic team section");
  Text(0xA04, u"Synthetic character controls");
  Text(0x9EC, u"Synthetic accept and return");
  Text(0xD99, u"Synthetic character zero");
  Text(0xD9A, u"Synthetic character one");
  Text(0xD9B, u"Synthetic character two");
  Text(0xD9C, u"Synthetic character three");
}
void Original(PPCContext& ctx, uint8_t* base, uint32_t result = 0) {
  assert(base == opaque);
  ++originals;
  ctx.r3.u64 = result;
  ctx.r7.u64 = 0xABCDEF0123456789;
  ctx.f1.f64 = 0.5;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Call(void (*hook)(PPCContext&, uint8_t*)) {
  PPCContext ctx{};
  ctx.r3.u32 = task;
  ctx.r4.u32 = parent + 0x42C;
  ctx.r5.u32 = parent + 0x44;
  ctx.r6.u32 = parent + 0x20;
  ctx.r7.u32 = 1;
  ctx.r9.u32 = 0;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  const auto before = originals;
  hook(ctx, opaque);
  assert(originals > before);
  assert(ctx.r7.u64 == 0xABCDEF0123456789 && ctx.f1.f64 == 0.5 && ctx.f2.f64 == 0.125);
  assert(ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
void Hook(void (*hook)(PPCRegister&)) {
  PPCRegister reg{};
  reg.u32 = task;
  const auto before = reg;
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  hook(reg);
  assert(std::memcmp(&reg, &before, sizeof(reg)) == 0);
  assert(std::fegetround() == FE_UPWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
void Input(std::function<void()> action = {}, uint32_t result = 0) {
  input = std::move(action);
  input_result = result;
  Call(sub_822E89B8);
  input = {};
}
void TeamInput(std::function<void()> action = {}, int32_t result = -1) {
  input = std::move(action);
  input_result = static_cast<uint32_t>(result);
  Call(sub_822E7A40);
  input = {};
}
void Render(uint32_t model = task + 0x1FC, uint32_t control = widget) {
  PPCRegister bound{}, drawn{};
  bound.u32 = model;
  drawn.u32 = control;
  const auto before_bound = bound, before_drawn = drawn;
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  BrawlersVsCharactersRendered(bound, drawn);
  assert(std::memcmp(&bound, &before_bound, sizeof(bound)) == 0);
  assert(std::memcmp(&drawn, &before_drawn, sizeof(drawn)) == 0);
  assert(std::fegetround() == FE_UPWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
void Difficulty(uint32_t controller, uint32_t slot) {
  PPCRegister owner{}, source{}, target{};
  owner.u32 = task;
  source.u32 = controller;
  target.u32 = slot;
  BrawlersVsCharactersDifficultyChanged(owner, source, target);
}
void TeamRender(uint32_t model = task + 0x260) {
  PPCRegister bound{}, drawn{};
  bound.u32 = model;
  drawn.u32 = team_widget;
  BrawlersVsCharactersTeamRendered(bound, drawn);
}
const ScreenEvent& LastScreen() { return std::get<ScreenEvent>(events.back().content); }
}  // namespace

extern "C" void __imp__sub_822E9370(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822ECCB0(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822E89B8(PPCContext& ctx, uint8_t* base) {
  if (input) input();
  Original(ctx, base, input_result);
}
extern "C" void __imp__sub_822E7A40(PPCContext& ctx, uint8_t* base) {
  if (input) input();
  Original(ctx, base, input_result);
}
extern "C" void __imp__sub_822E8230(PPCContext& ctx, uint8_t* base) {
  if (accepted) accepted();
  Original(ctx, base, 1);
}
extern "C" void __imp__sub_822E76C8(PPCContext& ctx, uint8_t* base) {
  if (transfer) transfer();
  Original(ctx, base);  // Return zero can still transfer focus to native confirmation.
}

int main() {
  Fixture();
  SetVsCharactersCapture([](uint32_t address, void* result, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(result)[i] = found->second;
    }
    return true;
  }, [](VsCharactersCapture&& event) {
    if (throw_output) throw std::runtime_error("synthetic callback failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });

  const auto unowned_reads = reads;
  Input([] { Current(0, 22); });
  assert(reads == unowned_reads && events.empty());
  bytes[Selector(0) + 0x58] = 0;
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  Hook(BrawlersVsCharactersReady);
  assert(events.empty());
  Input([] { Current(0, 22); Current(1, 1); });
  assert(events.size() == 3);
  assert(events[0].controller == UINT32_MAX && std::get<ScreenEvent>(events[0].content).kind == "opened");
  assert(std::get<ScreenEvent>(events[0].content).hints.size() == 2);
  assert(events[1].controller == 0 && std::get<ScreenEvent>(events[1].content).label_key == "vs_characters_confirm");
  assert(events[2].controller == 1 && LastScreen().label == "Synthetic character one");
  assert(LastScreen().position == 2 && LastScreen().total == 23 && LastScreen().selected);
  const auto identity = events[0].id;
  auto count = events.size();
  const auto idle_reads = reads;
  Input();
  NotifyVsCharactersSelection(Selector(0), 0);
  for (unsigned i = 0; i < 20; ++i) Render();
  assert(reads == idle_reads && events.size() == count);

  // Raw redirect23 and its intermediate assignment never escape the input scope.
  Input([&] {
    Current(0, 23);
    assert(events.size() == count);
    Word(Selector(0) + 0x50, 20);
    bytes[task + 0x24] = 0xFF;
    bytes[task + 0x24 + 2 * 0xC] = 0;
    Hook(BrawlersVsCharactersChanged);
  });
  assert(events.size() == count + 1);
  const auto& slot = std::get<VsCharactersPlayerSlot>(events.back().content);
  assert(slot.current_slot == 2 && slot.target_slot == 2 && slot.computer && slot.position == 21);
  assert(slot.section_key == "vs_identity_5");
  assert(slot.character_name == "Synthetic character two");

  count = events.size();
  bytes.erase(resource + 0x10000 + 0xD9C * 0x100);
  Input([] { Current(0, 21); });
  assert(events.size() == count);
  Text(0xD9C, u"Synthetic character three");
  Render();
  assert(events.size() == count + 1 &&
         std::get<VsCharactersPlayerSlot>(events.back().content).character_name == "Synthetic character three");
  Input([] { Current(0, 20); });

  // Acceptance and transfer are nested operations; snapshot uses the final player.
  count = events.size();
  accepted = [] {
    bytes[task + 0x21 + 2 * 0xC] = 0;
    bytes[task + 0x21] = 2;
  };
  transfer = [] {
    bytes[task + 0x24 + 2 * 0xC] = 0xFF;
    bytes[task + 0x24 + 3 * 0xC] = 0;
  };
  Input([&] {
    Current(0, 0);
    Call(sub_822E8230);
    assert(events.size() == count);
    Call(sub_822E76C8);
    assert(events.size() == count);
  });
  assert(events.size() == count + 1 && LastScreen().section_key == "vs_identity_6");
  assert(LastScreen().label == "Synthetic character zero" && !LastScreen().selected);

  // Committed CPU difficulty remains a typed fact, separate from hovered character0.
  Input([] {
    bytes[task + 0x22 + 3 * 0xC] = 2;
    Difficulty(0, 3);
  });
  const auto& level = std::get<VsCharactersDifficulty>(events.back().content);
  assert(level.slot == 3 && level.character == 3 && level.level == 2);
  assert(level.character_name == "Synthetic character three");

  count = events.size();
  bytes.erase(resource + 0x10000 + 0xD9C * 0x100);
  Input([] { bytes[task + 0x22 + 3 * 0xC] = 1; Difficulty(0, 3); });
  assert(events.size() == count);
  Text(0xD9C, u"Synthetic character three");
  Render();
  assert(events.size() == count + 1 && std::get<VsCharactersDifficulty>(events.back().content).level == 1);

  // Native handoff changes assignment before the same iteration writes the old
  // CPU slot's difficulty through retained r23/r19.
  Input([] {
    bytes[task + 0x24 + 3 * 0xC] = 0xFF;
    bytes[task + 0x24 + 2 * 0xC] = 0;
    Hook(BrawlersVsCharactersChanged);
    bytes[task + 0x22 + 3 * 0xC] = 0;
    Difficulty(0, 3);
  });
  const auto& old_slot = std::get<VsCharactersDifficulty>(events.back().content);
  assert(events.back().controller == 0 && old_slot.slot == 3 &&
         old_slot.character == 3 && old_slot.level == 0);
  assert(std::get<ScreenEvent>(events[events.size() - 2].content).section_key == "vs_identity_5");
  Input([] {
    bytes[task + 0x24 + 2 * 0xC] = 0xFF;
    bytes[task + 0x24 + 3 * 0xC] = 0;
    Hook(BrawlersVsCharactersChanged);
  });

  Input([] { Current(0, 16); });
  assert(LastScreen().label_key == "vs_characters_random" && !LastScreen().selected);
  Input([] { Current(0, 17); });
  assert(LastScreen().label_key == "vs_characters_custom");
  bytes[flags] = 0;
  Input([] { Current(0, 2); });
  assert(LastScreen().position == 2 && LastScreen().total == 22);
  count = events.size();
  Word(Selector(0) + 0x44, 0xBAD);
  Input([] { Current(0, 1); });
  assert(events.size() == count);  // Readable selector storage is not sufficient ownership.
  Word(Selector(0) + 0x44, 0x82017B80);
  bytes.erase(resource + 0x10000 + 0xD9A * 0x100);
  Input([] { Current(0, 1); });
  assert(events.size() == count);
  Text(0xD9A, u"Synthetic character one");
  const auto pending_reads = reads;
  Render(task + 0x200);  // The shared widget type does not identify a bound owner.
  assert(reads == pending_reads && events.size() == count);
  Word(widget, 0xBAD);
  Render();
  assert(events.size() == count);
  Word(widget, 0x82017CCC);
  Input([&] { Render(); assert(events.size() == count); });
  Render();  // Actual bound draw retries text without another selection event.
  assert(events.size() == count + 1 && LastScreen().label == "Synthetic character one");
  count = events.size();
  Input([] { Current(0, 0); });
  assert(events.size() == count);  // Disabled focus fails closed.
  bytes[flags] = 1;
  bytes[task + 0x24 + 2 * 0xC] = 0;
  Input([] { Current(0, 2); });
  assert(events.size() == count);  // Ambiguous controller ownership fails closed.
  bytes[task + 0x24 + 2 * 0xC] = 0xFF;

  Input([] { Current(0, 22); }, 2);
  assert(events.size() == count);  // Native transition discards outgoing dirty batch.
  Hook(BrawlersVsCharactersSuspended);
  Input([] { Current(0, 2); });
  assert(events.size() == count);
  Input([&] {
    Hook(BrawlersVsCharactersResumed);
    assert(events.size() == count);
    Current(0, 22);
  });
  assert(events.size() == count + 2 && LastScreen().kind == "resumed");
  assert(events.back().id == identity);
  assert(!events[count].continuation && events[count + 1].continuation);

  // A lower-numbered controller can be delayed; interruption belongs to the
  // first readable return, not controller zero or the current loop index.
  Input([] { Current(0, 0); });
  Hook(BrawlersVsCharactersSuspended);
  bytes.erase(resource + 0x10000 + 0xD99 * 0x100);
  count = events.size();
  Hook(BrawlersVsCharactersResumed);
  assert(events.size() == count + 1 && events.back().controller == 1 && !events.back().continuation);
  Text(0xD99, u"Synthetic character zero");
  Render();
  assert(events.size() == count + 2 && events.back().controller == 0 && events.back().continuation);
  Input([] { Current(0, 22); });

  // Team selection is a section of this same task. Its linear selector has
  // three rows, with actual participant order and identity artwork facts.
  Hook(BrawlersVsCharactersSuspended);
  Word(task + 0x14, 8);
  count = events.size();
  Hook(BrawlersVsCharactersTeamReady);
  assert(events.size() == count);  // Initial native selector is not yet valid.
  TeamInput([&] { TeamCurrent(0); assert(events.size() == count); });
  assert(events.size() == count + 1 && events.back().id == identity && events.back().controller == 0);
  const auto& first_team = std::get<VsCharactersTeam>(events.back().content);
  assert(first_team.choice == 0 && first_team.position == 1 && first_team.total == 3 && first_team.entered);
  assert(first_team.section == "Synthetic team section" && first_team.hints.size() == 1);
  assert(first_team.hints[0] == "Synthetic accept and return");
  assert((first_team.participants == std::array<uint32_t, 4>{0, 1, 2, 3}));
  assert((first_team.identity_frames == std::array<uint32_t, 4>{0, 1, 5, 6}));
  const auto team_idle_reads = reads;
  TeamInput(); TeamRender(); Render();
  assert(reads == team_idle_reads);
  count = events.size();
  TeamInput([&] { TeamCurrent(1); TeamCurrent(2); assert(events.size() == count); });
  const auto& third_team = std::get<VsCharactersTeam>(events.back().content);
  assert(events.size() == count + 1 && third_team.choice == 2 && third_team.position == 3 && !third_team.entered);
  assert((third_team.participants == std::array<uint32_t, 4>{0, 3, 1, 2}));
  assert((third_team.identity_frames == std::array<uint32_t, 4>{0, 6, 1, 5}));

  count = events.size();
  TeamInput([] { TeamCurrent(1); }, -2);
  assert(events.size() == count);  // Cancel discards that operation's outgoing row.
  Word(task + 0x14, 3);
  Hook(BrawlersVsCharactersResumed);
  assert(events.size() == count + 2 && LastScreen().kind == "resumed" && events.back().id == identity);
  Hook(BrawlersVsCharactersSuspended);
  Word(task + 0x14, 8);
  bytes[task + 0x1F7] = 0;  // Native cancellation resets only this obsolete cache.
  Hook(BrawlersVsCharactersTeamReady);
  const auto& reopened_team = std::get<VsCharactersTeam>(events.back().content);
  assert(reopened_team.choice == 1 && reopened_team.entered && reopened_team.position == 2);
  assert((reopened_team.identity_frames == std::array<uint32_t, 4>{0, 5, 1, 6}));

  // Structural corruption fails closed; a later semantic selection can recover.
  count = events.size();
  Word(task + 0x2B8, 4);
  TeamInput([] { TeamCurrent(2); });
  assert(events.size() == count);
  Word(task + 0x2B8, 3);
  Word(0x82017B08 + 2 * 16 + 4, 0);
  TeamInput([] { TeamCurrent(2); });
  assert(events.size() == count);
  Word(0x82017B08 + 2 * 16 + 4, 3);
  bytes[task + 0x2C8 + 3] = 2;
  TeamInput([] { TeamCurrent(2); });
  assert(events.size() == count);
  bytes[task + 0x2C8 + 3] = 1;
  Unit(team_loader + 0x20, 0x92D);
  TeamInput([] { TeamCurrent(2); });
  assert(events.size() == count);
  Unit(team_loader + 0x20, 0x92C);

  // Delayed section text retries through the bound team identity renderer only.
  bytes.erase(resource + 0x10000 + 0x64A * 0x100);
  TeamInput([] { TeamCurrent(2); });
  assert(events.size() == count);
  Text(0x64A, u"Synthetic team section");
  const auto wrong_team_reads = reads;
  TeamRender(task + 0x1FC);
  assert(reads == wrong_team_reads);
  TeamInput([&] { TeamRender(); assert(events.size() == count); });
  TeamRender();
  assert(events.size() == count + 1 && std::get<VsCharactersTeam>(events.back().content).choice == 2);
  count = events.size();
  TeamInput([] { TeamCurrent(0); }, 0);
  assert(events.size() == count);  // Confirm also discards the outgoing batch.
  const auto confirmed_reads = reads;
  TeamRender();
  assert(reads == confirmed_reads);

  Hook(BrawlersVsCharactersSuspended);
  Call(sub_822ECCB0);
  assert(events.back().controller == UINT32_MAX && LastScreen().kind == "closed");
  count = events.size();
  NotifyVsCharactersTaskRemoved(task);
  assert(events.size() == count);
  const auto closed_reads = reads;
  Input([] { Current(0, 1); });
  assert(reads == closed_reads);

  // Initial title/hints can become readable after the only initial selection
  // callback. Draw retry retains one lifetime and releases each pending item.
  Fixture();
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  bytes.erase(resource + 0x10000 + 0x626 * 0x100);
  Hook(BrawlersVsCharactersReady);
  count = events.size();
  Input([] { Current(0, 22); Current(1, 1); });
  assert(events.size() == count);
  Text(0x626, u"Synthetic character screen");
  bytes[task + 0x2FD] = 1;
  Render();
  assert(events.size() == count);
  bytes[task + 0x2FD] = 0;
  Render();
  assert(events.size() == count + 3 &&
         std::get<ScreenEvent>(events[count].content).kind == "opened");
  const auto settled_reads = reads;
  Render(); Input();
  assert(reads == settled_reads);
  NotifyVsCharactersTaskRemoved(task);

  // Suspension discards an unreadable pending focus rather than publishing a
  // stale outgoing item when its text appears later.
  Fixture();
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  Hook(BrawlersVsCharactersReady);
  bytes.erase(resource + 0x10000 + 0xD9A * 0x100);
  Input([] { Current(0, 1); });
  count = events.size();
  Hook(BrawlersVsCharactersSuspended);
  Text(0xD9A, u"Synthetic character one");
  const auto suspended_reads = reads;
  Render();
  assert(events.size() == count && reads == suspended_reads);
  NotifyVsCharactersTaskRemoved(task);

  // Reused address opens a new native lifetime; duplicate construction fails closed.
  Fixture();
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  Hook(BrawlersVsCharactersReady);
  Input([] { Current(0, 22); });
  assert(events.back().id != identity);
  Word(parent + 0x14, 5);
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  count = events.size();
  Input([] { Current(0, 1); });
  assert(events.size() == count);
  NotifyVsCharactersTaskRemoved(task);

  Fixture();
  Call(sub_822E9370);
  Word(parent + 0x14, 6);
  Hook(BrawlersVsCharactersReady);
  throw_output = true;
  Input([] { Current(0, 22); });
  assert(failures == 1);
  const auto failed_reads = reads;
  Input([] { Current(0, 1); });
  assert(failures == 1 && reads == failed_reads);
}
