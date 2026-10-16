// Original synthetic fixtures; no game or accessibility services are used.
#include "native_vs_rules.h"

#include <array>
#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_82339BD0(PPCContext&, uint8_t*);
extern "C" void sub_82339910(PPCContext&, uint8_t*);
extern "C" void sub_82339878(PPCContext&, uint8_t*);
void BrawlersVsRulesReady(PPCRegister&);
void BrawlersVsRulesSuspended(PPCRegister&);
void BrawlersVsRulesLabelDraw(PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}
namespace {
constexpr uint32_t task = 0x1000, common = 0x2000, parent = 0x3000, flags = 0x4000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> guards;
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
void Fixture() {
  Word(task, 0x82019EFC);
  Word(task + 0x14, 3);
  Word(task + 0x18, parent + 0x42C);
  Word(task + 0x1C, parent + 0x2C);
  Word(task + 0x20, parent + 0x424);
  Word(task + 0x44, 0x82019EE4);
  Word(task + 0x54, task + 0x2C);
  Word(task + 0x58, task + 0x38);
  Word(task + 0x7C, 0x8200FC30);
  Word(task + 0xA4, task + 0xBC);
  Word(task + 0xBC, 0x8200FC10);
  Word(task + 0xC4, flags);
  Word(task + 0xCC, 14);
  Word(task + 0xD0, 15);
  for (uint32_t i = 0; i < 7; ++i) bytes[task + 0x5E + i] = 1;
  for (uint32_t i = 0; i < 15; ++i) bytes[flags + i] = 1;
  bytes[task + 0x2C] = 1;
  bytes[task + 0x2D] = 1;
  bytes[task + 0x2E] = 0;
  Word(task + 0x30, 4);
  bytes[task + 0x34] = 1;
  bytes[task + 0x35] = 0;
  bytes[task + 0x36] = 1;
  bytes[task + 0x5D] = 1;
  Word(parent, 0x8201A078);
  Word(parent + 0x14, 9);
  Word(0x829CBC68, common);
  Word(common, 0x82003EAC);
  Unit(common + 0x20, 0x8CC);
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
  Word(resource + 0xC, 0xB00);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x4000);
  for (uint32_t id = 0; id < 0xB00; ++id)
    Word(resource + 0x20 + id * 4, 0x10000 + id * 0x100);
  Text(0x627, u"Synthetic rule title");
  Text(0x639, u"Synthetic explanation");
  Text(0xA1D, u"Synthetic button hints");
  Text(0xA28, u"Synthetic difficulty");
  Text(0x95D, u"Synthetic gate setting");
  Text(0x960, u"Synthetic ability setting");
  Text(0x827, u"Synthetic level setting");
  Text(0x9A7, u"Synthetic attribute setting");
  Text(0x644, u"Synthetic shots setting");
  Text(0x7EE, u"Synthetic interference setting");
  Text(0x95A, u"Synthetic easy");
  Text(0x95B, u"Synthetic medium");
  Text(0x95C, u"Synthetic hard");
  Text(0x95E, u"Synthetic gate automatic");
  Text(0x95F, u"Synthetic gate manual");
  Text(0x961, u"Synthetic ability automatic");
  Text(0x962, u"Synthetic ability manual");
  Text(0x82C, u"Synthetic dynamic level");
  Text(0x64C, u"Synthetic enabled");
  Text(0x64D, u"Synthetic disabled");
}
void Original(PPCContext& ctx, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  ctx.r3.u64 = 0xABCDEF;
  ctx.r7.u64 = 0xABCDEF0123456789;
  ctx.f1.f64 = 0.5;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Call(void (*hook)(PPCContext&, uint8_t*), bool valid_parent = true) {
  PPCContext ctx{};
  ctx.r3.u32 = task;
  ctx.r4.u32 = parent + 0x42C;
  ctx.r5.u32 = parent + 0x2C;
  ctx.r6.u32 = parent + 0x424;
  ctx.r7.u32 = valid_parent ? parent + 0x20 : 0;
  ctx.r8.u32 = 1;
  ctx.r9.u32 = 1;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  const auto before = originals;
  hook(ctx, opaque);
  assert(originals == before + 1);
  assert(ctx.r3.u64 == 0xABCDEF && ctx.r7.u64 == 0xABCDEF0123456789);
  assert(ctx.r8.u32 == 1 && ctx.f1.f64 == 0.5 && ctx.f2.f64 == 0.125);
  assert(ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
void Hook(void (*hook)(PPCRegister&), uint32_t value = task) {
  PPCRegister reg{};
  reg.u32 = value;
  const auto before = reg;
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  hook(reg);
  assert(std::memcmp(&reg, &before, sizeof(reg)) == 0);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
void Select(uint32_t index) {
  Word(task + 0xCC, index);
  NotifyVsRulesSelection(task + 0x7C, index);
}
}  // namespace
extern "C" void __imp__sub_82339BD0(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_82339910(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_82339878(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }

int main() {
  Fixture();
  SetVsRulesCapture([](uint32_t address, void* result, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(result)[i] = found->second;
    }
    return true;
  }, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic callback failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });

  const auto idle_reads = reads;
  NotifyVsRulesSelection(0xDEAD, 0);
  Hook(BrawlersVsRulesLabelDraw, 0xDEAD);
  assert(reads == idle_reads && events.empty());
  Call(sub_82339BD0, false);
  Hook(BrawlersVsRulesReady);
  assert(events.empty());
  Call(sub_82339BD0);
  assert(events.empty());
  Hook(BrawlersVsRulesReady);
  assert(events.size() == 2 && events[0].kind == "opened");
  assert(events[0].title == "Synthetic rule title" && events[0].body == "Synthetic explanation");
  assert(events[1].label_key == "vs_rules_confirm" && events[1].position == 8 && events[1].total == 8);
  assert(events[1].hints.size() == 1);
  const auto identity = events[0].id;
  const auto settled_reads = reads;
  Hook(BrawlersVsRulesLabelDraw, 0xDEAD);
  assert(reads == settled_reads);

  // The native arrows are paired controls for one row. Only its changed value speaks.
  Select(0);
  assert(events.back().label == "Synthetic difficulty" && events.back().value == "Synthetic medium");
  assert(events.back().position == 1 && events.back().total == 8 && events.back().hints.empty());
  const auto prior = events.size();
  Select(1);
  assert(events.size() == prior);
  bytes[task + 0x2C] = 2;
  Call(sub_82339910);
  assert(events.size() == prior + 1 && events.back().value == "Synthetic hard");
  bytes[task + 0x5D] = 0;
  Call(sub_82339910);
  assert(events.size() == prior + 1);

  Select(2);
  assert(events.back().value == "Synthetic gate automatic");
  Select(4);
  assert(events.back().value == "Synthetic ability manual");
  Select(6);
  assert(events.back().value == "Synthetic dynamic level");
  Select(8);
  assert(events.back().value == "Synthetic enabled");
  Select(10);
  assert(events.back().value == "Synthetic disabled");
  Select(12);
  assert(events.back().value == "Synthetic enabled" && events.back().position == 7);

  // A stale event, mismatched navigation flags, and unreadable values fail closed.
  auto count = events.size();
  NotifyVsRulesSelection(task + 0x7C, 0);
  assert(events.size() == count);
  bytes[flags] = 0;
  Select(0);
  assert(events.size() == count);
  bytes[flags] = 1;
  bytes.erase(task + 0x2C);
  Select(0);
  assert(events.size() == count);
  bytes[task + 0x2C] = 1;

  // Disabled rows are excluded from positions and cannot become readable focus.
  bytes[task + 0x5E] = 0;
  bytes[flags] = bytes[flags + 1] = 0;
  Select(0);
  assert(events.size() == count);
  Select(2);
  assert(events.back().position == 1 && events.back().total == 7);
  Hook(BrawlersVsRulesSuspended);
  count = events.size();
  Select(6);
  assert(events.size() == count);
  Call(sub_82339878);
  assert(events.back().kind == "closed" && events.back().id == identity);
  count = events.size();
  NotifyVsRulesTaskRemoved(task);
  assert(events.size() == count);

  // A missing initial caption is retried at its actual draw, preserving identity.
  Fixture();
  Word(task + 0xCC, 0);
  bytes.erase(resource + 0x10000 + 0xA28 * 0x100);
  Call(sub_82339BD0);
  Hook(BrawlersVsRulesReady);
  assert(events.size() == count);
  Text(0xA28, u"Synthetic difficulty");
  Word(0x6000, 0x82019F0C);
  Hook(BrawlersVsRulesLabelDraw, 0x6000);
  assert(events.size() == count + 2 && events[count].id != identity);
  assert(events.back().label == "Synthetic difficulty" && events.back().hints.size() == 1);

  // A delayed hint can arrive after the title. Retry the first item without a
  // second opening event, preserving the speech pipeline's pending queue.
  NotifyVsRulesTaskRemoved(task);
  count = events.size();
  Fixture();
  Word(task + 0xCC, 0);
  bytes.erase(resource + 0x10000 + 0xA1D * 0x100);
  Call(sub_82339BD0);
  Hook(BrawlersVsRulesReady);
  assert(events.size() == count + 1 && events.back().kind == "opened");
  const auto delayed_identity = events.back().id;
  Text(0xA1D, u"Synthetic delayed hint");
  Hook(BrawlersVsRulesLabelDraw, 0x6000);
  assert(events.size() == count + 2 && events.back().kind == "focused");
  assert(events.back().id == delayed_identity && events.back().hints.size() == 1);
  assert(events.back().hints[0] == "Synthetic delayed hint");

  throw_output = true;
  Select(2);
  assert(failures == 1);
  const auto failed_reads = reads;
  Select(6);
  assert(failures == 1 && reads == failed_reads);
}
