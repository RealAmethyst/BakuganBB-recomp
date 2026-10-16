// Original synthetic fixtures; no game or accessibility services are used.
#include "native_vs_decks.h"

#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_822F1138(PPCContext&, uint8_t*);
extern "C" void sub_822F2820(PPCContext&, uint8_t*);
extern "C" void sub_822F0D18(PPCContext&, uint8_t*);
void BrawlersVsDecksReady(PPCRegister&);
void BrawlersVsDecksSuspended(PPCRegister&);
void BrawlersVsDecksNameDraw(PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}
namespace {
constexpr uint32_t task = 0x1000, common = 0x2000, parent = 0x3000, flags = 0x4000;
constexpr uint32_t widget = 0x5000, provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
struct Captured { uint32_t controller; ScreenEvent event; };
std::vector<Captured> events;
std::vector<std::string> guards;
unsigned originals = 0, reads = 0, failures = 0;
bool throw_output = false;
std::function<void()> input;
uint32_t input_result = 5;
uint8_t opaque[1]{};
void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[address + i] = value >> (24 - i * 8);
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = value >> 8;
  bytes[address + 1] = value;
}
void String(uint32_t address, const char16_t* text) {
  do { Unit(address, *text); address += 2; } while (*text++);
}
void Text(uint32_t id, const char16_t* text) {
  String(resource + 0x10000 + id * 0x100, text);
}
void Fixture() {
  Word(task, 0x82017DDC);
  Word(task + 0x14, 3);
  Word(task + 0x18, parent + 0x42C);
  Word(task + 0x20, 0x82017DD8);
  Word(task + 0x1F4, parent + 0x44);
  Word(task + 0x1F8, parent + 0x20);
  Word(task + 0x204, parent + 0x432);
  bytes[task + 0x1DC] = 1;
  bytes[parent + 0x29] = 2;
  for (uint32_t c = 0; c < 4; ++c) {
    const uint32_t selector = task + 0x2C + c * 0x60, model = selector + 0x40;
    Word(selector, 0x8200FC30);
    Word(selector + 0x28, model);
    Word(model, 0x8200FC10);
    Word(model + 8, flags + c * 0x10);
    Word(model + 0x10, 0);
    Word(model + 0x14, c ? 1 : 5);
    bytes[flags + c * 0x10] = 1;
  }
  bytes[flags + 1] = 1;
  bytes[flags + 2] = 0;
  bytes[flags + 3] = 0;
  bytes[flags + 4] = 1;
  String(parent + 0x44 + 0xD4, u"Unit Alpha");
  String(parent + 0x44 + 0xF8 + 0xD4, u"Unit Beta");
  String(parent + 0x44 + 2 * 0xF8 + 0xD4, u"Unit Gamma");
  String(parent + 0x44 + 3 * 0xF8 + 0xD4, u"Unit Delta");
  String(parent + 0x44 + 0xA8, u"Hidden deck name");
  Word(parent, 0x8201A078);
  Word(parent + 0x14, 7);
  Word(0x829CBC68, common);
  Word(common, 0x82003EAC);
  Unit(common + 0x20, 0x8CC);
  Word(widget, 0x82017E18);
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
  Text(0x628, u"Synthetic summary title");
  Text(0x9EC, u"Synthetic summary hints");
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
  ctx.r5.u32 = parent + 0x44;
  ctx.r6.u32 = valid_parent ? parent + 0x20 : 0;
  ctx.r7.u32 = parent + 0x432;
  ctx.r8.u32 = 1;
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
void Select(uint32_t controller, uint32_t index) {
  const uint32_t selector = task + 0x2C + controller * 0x60;
  Word(selector + 0x50, index);
  NotifyVsDecksSelection(selector, index);
}
void Input(uint32_t result = 5, std::function<void()> action = {}) {
  input = std::move(action);
  input_result = result;
  PPCContext ctx{};
  ctx.r3.u32 = task;
  ctx.r8.u32 = 0x87654321;
  ctx.lr = 0x12345678;
  const auto before = originals;
  sub_822F0D18(ctx, opaque);
  assert(originals == before + 1 && ctx.r3.u64 == result);
  assert(ctx.r7.u64 == 0xABCDEF0123456789 && ctx.f1.f64 == 0.5);
  assert(ctx.r8.u32 == 0x87654321 && ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
}  // namespace
extern "C" void __imp__sub_822F1138(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822F2820(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822F0D18(PPCContext& ctx, uint8_t* base) {
  assert(ctx.r3.u32 == task);
  if (input) input();
  Original(ctx, base);
  ctx.r3.u64 = input_result;
}

int main() {
  Fixture();
  SetVsDecksCapture([](uint32_t address, void* result, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(result)[i] = found->second;
    }
    return true;
  }, [](uint32_t controller, ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic callback failure");
    events.push_back({controller, std::move(event)});
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });

  const auto idle_reads = reads;
  NotifyVsDecksSelection(0xDEAD, 0);
  Hook(BrawlersVsDecksNameDraw, 0xDEAD);
  assert(reads == idle_reads && events.empty());
  Call(sub_822F1138, false);
  Hook(BrawlersVsDecksReady);
  assert(events.empty());
  Call(sub_822F1138);
  Hook(BrawlersVsDecksReady);
  assert(events.size() == 2 && events[0].event.kind == "opened");
  assert(events[0].event.title == "Synthetic summary title");
  assert(events[1].event.label == "Unit Alpha" && events[1].event.position == 1);
  assert(events[1].event.total == 3 && events[1].event.hints.size() == 1);
  const auto identity = events[0].event.id;
  const auto input_idle_reads = reads;
  Input();
  assert(reads == input_idle_reads && events.size() == 2);
  for (const auto result : {0u, 1u, 2u, 3u, 4u, 6u}) {
    Input(result, [&] {
      Select(0, 1);
      assert(events.size() == 2);
    });
    assert(events.size() == 2);
  }
  Input(5, [&] {
    Select(0, 1);
    Select(0, 0);
    assert(events.size() == 2);
  });
  assert(events.size() == 2);
  const auto settled_reads = reads;
  Hook(BrawlersVsDecksNameDraw, 0xDEAD);
  assert(reads == settled_reads);
  Select(0, 1);
  assert(events.back().event.label == "Unit Beta" && events.back().event.position == 2);
  assert(events.back().event.hints.empty());
  Select(0, 4);
  assert(events.back().event.label_key == "vs_decks_confirm");
  assert(events.back().event.position == 3 && events.back().event.total == 3);
  const auto unchanged = events.size();
  Select(0, 4);
  NotifyVsDecksSelection(task + 0x2C, 1);
  Select(0, 2);
  assert(events.size() == unchanged);
  Select(0, 0);
  Hook(BrawlersVsDecksSuspended);
  const auto suspended = events.size();
  Select(0, 1);
  assert(events.size() == suspended);
  Hook(BrawlersVsDecksReady);
  assert(events.size() == suspended + 1 && events.back().event.id == identity);
  assert(events.back().event.kind == "resumed" && events.back().event.hints.empty());

  // Distinct native selectors retain controller identity without speech policy.
  Hook(BrawlersVsDecksSuspended);
  bytes[parent + 0x29] = 4;
  bytes[task + 0x1DC] = 2;
  bytes[flags + 1] = 0;
  bytes[flags + 2] = 1;
  bytes[flags + 3] = 1;
  Word(task + 0x7C, 0);
  Hook(BrawlersVsDecksReady);
  assert(events[events.size() - 2].controller == 0);
  assert(events[events.size() - 2].event.kind == "resumed");
  assert(!events[events.size() - 2].event.continuation);
  assert(events.back().controller == 1 && events.back().event.label == "Unit Beta");
  assert(events.back().event.kind == "resumed" && events.back().event.continuation);
  assert(events.back().event.position == 1 && events.back().event.total == 1);
  assert(events.back().event.id == identity);
  const auto simultaneous = events.size();
  Input(5, [&] {
    String(parent + 0x44 + 0xF8 + 0xD4, u"Unit Echo");
    Select(1, 0);  // Native input processes this controller first.
    Select(0, 2);
    assert(events.size() == simultaneous);
  });
  assert(events.size() == simultaneous + 2);
  assert(events[simultaneous].controller == 0);
  assert(events[simultaneous].event.label == "Unit Gamma");
  assert(events[simultaneous + 1].controller == 1);
  assert(events[simultaneous + 1].event.label == "Unit Echo");
  // A delayed first controller must queue after the first actual resumed item.
  Hook(BrawlersVsDecksSuspended);
  Unit(parent + 0x44 + 2 * 0xF8 + 0xD4, 0xDC00);
  const auto delayed_resume = events.size();
  Hook(BrawlersVsDecksReady);
  assert(events.size() == delayed_resume + 1);
  assert(events.back().controller == 1 && events.back().event.kind == "resumed");
  assert(!events.back().event.continuation);
  String(parent + 0x44 + 2 * 0xF8 + 0xD4, u"Unit Gamma");
  Hook(BrawlersVsDecksNameDraw, widget);
  assert(events.size() == delayed_resume + 2);
  assert(events.back().controller == 0 && events.back().event.kind == "resumed");
  assert(events.back().event.continuation);
  Select(0, 3);
  assert(events.back().controller == 0 && events.back().event.label == "Unit Delta");
  assert(events.back().event.position == 3 && events.back().event.total == 4);
  Call(sub_822F2820);
  assert(events.back().event.kind == "closed");
  const auto closed = events.size();
  NotifyVsDecksTaskRemoved(task);
  assert(events.size() == closed);

  // A malformed initial string is retried at the verified name consumer.
  Fixture();
  Unit(parent + 0x44 + 0xD4, 0xDC00);
  Call(sub_822F1138);
  Hook(BrawlersVsDecksReady);
  assert(events.size() == closed);
  String(parent + 0x44 + 0xD4, u"Unit Alpha");
  Hook(BrawlersVsDecksNameDraw, widget);
  assert(events.size() == closed + 2 && events.back().event.id != identity);
  assert(events.back().event.hints.size() == 1);
  const auto valid = events.size();
  Word(task + 0x54, 0);
  Select(0, 1);
  assert(events.size() == valid);
  NotifyVsDecksTaskRemoved(task);
  Fixture();
  Call(sub_822F1138);
  throw_output = true;
  Hook(BrawlersVsDecksReady);
  assert(failures == 1);
  const auto failed_reads = reads;
  Hook(BrawlersVsDecksReady);
  NotifyVsDecksSelection(task + 0x2C, 0);
  assert(reads == failed_reads && failures == 1);
  assert(!guards.empty());
}
