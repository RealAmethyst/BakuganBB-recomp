// Synthetic native boundaries; no game, screen reader, display or audio device.
#include "native_vs_arena.h"

#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_822F4530(PPCContext&, uint8_t*);
extern "C" void sub_822F5B40(PPCContext&, uint8_t*);
void BrawlersVsArenaReady(PPCRegister&);
void BrawlersVsArenaSuspended(PPCRegister&);
void BrawlersVsArenaNavigationChanged(PPCRegister&);
void BrawlersVsArenaUpdateFinished(PPCRegister&);
void BrawlersVsArenaValueChanged(PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}
namespace {
constexpr uint32_t task = 0x1000, parent = 0x3000, resources = 0x4000, loader = 0x5000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> diagnostics;
unsigned reads = 0, originals = 0, failures = 0;
bool throw_output = false;
uint8_t opaque[1]{};
void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[address + i] = value >> (24 - i * 8);
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = value >> 8;
  bytes[address + 1] = value;
}
void Text(uint32_t id, const char16_t* value) {
  uint32_t address = resource + 0x10000 + id * 0x100;
  do { Unit(address, *value); address += 2; } while (*value++);
}
void Fixture() {
  Word(task, 0x82017F2C);
  Word(task + 0x14, 3);
  Word(task + 0x18, parent + 0x42C);
  Word(task + 0x20, 0x82017F20);
  Word(task + 0x2C, task + 0x60);
  Word(task + 0x30, task + 0xC0);
  Word(task + 0x34, parent + 0x425);
  Word(task + 0x38, task + 0xC1);
  Word(task + 0x48, parent + 0x2C);
  Word(task + 0x4C, parent + 0x424);
  Word(task + 0x50, parent + 0x20);
  Word(task + 0x54, parent + 0x44);
  bytes[task + 0x5D] = 1;
  Word(task + 0x60, 0x8200FC30);
  Word(task + 0x88, task + 0xA0);
  Word(task + 0xA0, 0x8200FC10);
  Word(task + 0xA4, 0x82017EB8);
  Word(task + 0xB0, 3);
  Word(task + 0xB4, 9);
  bytes[task + 0xC0] = 3;
  bytes[task + 0xC1] = 2;
  // Holes precede the selected stage; native grid flags do not encode these.
  for (uint32_t i = 0; i < 9; ++i) bytes[task + 0x3C + i] = i != 1 && i != 2;
  Word(0x829CC54C, parent);
  Word(parent, 0x8201A078);
  Word(parent + 0x14, 11);
  bytes[parent + 0x425] = 2;
  Word(0x829CC358, resources);
  Word(resources + 0xC, loader);
  Word(loader, 0x82003F2C);
  Unit(loader + 0x20, 0x91C);
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
  for (uint32_t i = 0; i < 0xB00; ++i) Word(resource + 0x20 + i * 4, 0x10000 + i * 0x100);
  Text(0x629, u"Synthetic arena title");
  Text(0xA0A, u"Synthetic difficulty hint");
  Text(0xA02, u"Synthetic first shortcut");
  Text(0xA03, u"Synthetic second shortcut");
  Text(0x9EC, u"Synthetic selection hint");
}
bool Read(uint32_t address, void* target, size_t size) {
  ++reads;
  auto* destination = static_cast<uint8_t*>(target);
  for (size_t i = 0; i < size; ++i) {
    const auto found = bytes.find(address + i);
    if (found == bytes.end()) return false;
    destination[i] = found->second;
  }
  return true;
}
PPCRegister Owner() { PPCRegister result{}; result.u64 = task; return result; }
void Construct() {
  PPCContext context{};
  context.r3.u64 = task;
  context.r4.u64 = parent + 0x42C;
  context.r14.u64 = 0x123456789ABCDEF0;
  sub_822F4530(context, opaque);
  assert(context.r3.u64 == task && context.r12.u64 == 55);
  assert(context.r14.u64 == 0x123456789ABCDEF0);
  Word(parent + 0x14, 12);
}
}

extern "C" void __imp__sub_822F4530(PPCContext& context, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  context.r12.u64 = 55;
}
extern "C" void __imp__sub_822F5B40(PPCContext& context, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  context.r3.u64 = 77;
}

int main() {
  SetVsArenaCapture(Read, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic sink failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { diagnostics.emplace_back(reason); }, [] { ++failures; });
  Fixture();
  Construct();
  auto owner = Owner();
  BrawlersVsArenaReady(owner);
  assert(events.size() == 2 && events[0].title == "Synthetic arena title");
  assert(events[1].position == 2 && events[1].total == 7);
  assert(events[1].label_key == "vs_arena_name_3" && events[1].value_key == "vs_arena_level_2");
  assert(events[1].hints.size() == 4 && events[1].hints[0] == "Synthetic difficulty hint");
  assert(diagnostics.empty());
  const auto identity = events[0].id;
  const auto idle_reads = reads;
  for (int i = 0; i < 50; ++i) BrawlersVsArenaUpdateFinished(owner);
  assert(reads == idle_reads && events.size() == 2);
  // A moved-to arena restricts the difficulty. Do not emit the old difficulty
  // between selection and the native clamp at the end of the operation.
  bytes[task + 0xC0] = 5;
  Word(task + 0xB0, 5);
  BrawlersVsArenaNavigationChanged(owner);
  assert(events.size() == 2);
  bytes[task + 0xC1] = 0;
  bytes[parent + 0x425] = 0;
  BrawlersVsArenaUpdateFinished(owner);
  assert(events.size() == 3 && events.back().position == 4 && events.back().total == 7);
  assert(events.back().value_key == "vs_arena_level_0" && events.back().hints.empty());
  bytes[task + 0xC1] = 2;
  bytes[parent + 0x425] = 1;
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4 && events.back().value_key == "vs_arena_level_1");
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4);
  Word(task + 0x50, parent + 0x44);  // A readable participant pointer is not the config.
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4 && diagnostics.back() == "vs_arena_configuration_ownership");
  Word(task + 0x50, parent + 0x20);
  bytes[task + 0x3C + 5] = 0;
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4 && diagnostics.back() == "vs_arena_unavailable_selection");
  bytes[task + 0x3C + 5] = 1;
  Word(task + 0xB0, 9);
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4 && diagnostics.back() == "vs_arena_selector");
  Word(task + 0xB0, 5);
  bytes.erase(parent + 0x425);
  BrawlersVsArenaValueChanged(owner);
  assert(events.size() == 4 && diagnostics.back() == "vs_arena_difficulty");
  bytes[parent + 0x425] = 1;
  // Alternate input retains the displayed arena while its raw cursor rests on
  // a disabled choice. Difficulty changes still belong to the displayed arena.
  Word(task + 0xB0, 1);
  bytes[parent + 0x425] = 2;
  BrawlersVsArenaValueChanged(owner);
  assert(events.back().label_key == "vs_arena_name_5");
  assert(events.back().value_key == "vs_arena_level_2");
  events.pop_back();
  BrawlersVsArenaNavigationChanged(owner);
  BrawlersVsArenaSuspended(owner);
  const auto suspended_reads = reads;
  BrawlersVsArenaUpdateFinished(owner);
  BrawlersVsArenaValueChanged(owner);
  assert(reads == suspended_reads && events.size() == 4);
  NotifyVsArenaTaskRemoved(task);
  assert(events.size() == 5 && events.back().kind == "closed" && events.back().id == identity);
  NotifyVsArenaTaskRemoved(task);
  assert(events.size() == 5);
  Fixture();
  Unit(loader + 0x20, 0x922);
  Construct();
  BrawlersVsArenaReady(owner);
  assert(events.size() == 7 && events.back().locale == 7 && events.back().id != identity);
  PPCContext destructor{};
  destructor.r3.u64 = task;
  sub_822F5B40(destructor, opaque);
  assert(destructor.r3.u64 == 77 && events.back().kind == "closed");
  const auto closed_size = events.size();
  Fixture();
  Unit(loader + 0x20, 0x913);
  Construct();
  BrawlersVsArenaReady(owner);
  assert(events.size() == closed_size && diagnostics.back() == "vs_arena_locale");
  Fixture();
  Construct();
  throw_output = true;
  const auto rounding = std::fegetround();
  BrawlersVsArenaReady(owner);
  assert(failures == 1 && std::fegetround() == rounding);
  const auto failed_reads = reads;
  BrawlersVsArenaReady(owner);
  assert(reads == failed_reads && failures == 1);
  assert(originals == 5);
}
