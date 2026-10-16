// Synthetic native setup events; no game, devices or speech service is executed.
#include "native_vs_mode.h"

#include <cassert>
#include <cfenv>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_822F70A8(PPCContext&, uint8_t*);
extern "C" void sub_822F7248(PPCContext&, uint8_t*);
extern "C" void sub_822F7B00(PPCContext&, uint8_t*);
void BrawlersVsModeReady(PPCRegister&);
void BrawlersVsModeSuspended(PPCRegister&);
void BrawlersVsModeResumed(PPCRegister&);
void BrawlersVsModeValueChanged(PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}

namespace {
constexpr uint32_t owner = 0x1000, parent = 0x2000, enabled = 0x3000;
constexpr uint32_t common = 0x4000, resources = 0x5000, loader = 0x6000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> diagnostics;
unsigned reads = 0, originals = 0, failures = 0;
bool throw_output = false;
std::function<void()> input;
uint8_t opaque[1]{};

void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[address + i] = static_cast<uint8_t>(value >> (24 - i * 8));
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = static_cast<uint8_t>(value >> 8);
  bytes[address + 1] = static_cast<uint8_t>(value);
}
void Text(uint32_t id, uint32_t offset, const char16_t* text) {
  Word(resource + 0x20 + id * 4, offset);
  Word(resource + 0x20 + (id + 1) * 4, offset + 0x100);
  uint32_t address = resource + offset;
  do { Unit(address, *text); address += 2; } while (*text++);
}
void Fixture() {
  bytes.clear();
  Word(owner, 0x8201802C);
  Word(owner + 0x18, parent + 0x42C);
  Word(owner + 0x20, parent + 0x20);
  Word(parent, 0x8201A078);
  Word(parent + 0x14, 3);
  Word(owner + 0x14, 2);
  bytes[owner + 0x129] = 0;
  Word(owner + 0xAC, 0x8200FC30);
  Word(owner + 0xD4, owner + 0xEC);
  Word(owner + 0xEC, 0x8200FC10);
  Word(owner + 0xF0, owner + 0x10C);
  Word(owner + 0xF4, enabled);
  Word(owner + 0xFC, 0);
  Word(owner + 0x100, 7);
  bytes[owner + 0x104] = 0;
  for (unsigned i = 0; i < 7; ++i) bytes[enabled + i] = i < 5;
  bytes[owner + 0x36] = 2;
  bytes[owner + 0x37] = 1;
  Word(owner + 0x38, 0);
  for (unsigned i = 0; i < 4; ++i) bytes[owner + 0x12F + i] = i < 2 ? i + 1 : 0;
  Word(owner + 0x44, 0x82018014);
  Word(owner + 0x6C, owner + 0x133);
  bytes[owner + 0x133] = 0;
  bytes[owner + 0x134] = 4;
  bytes[owner + 0x135] = bytes[owner + 0x136] = 0xFF;
  Word(0x829CBC68, common);
  Word(common, 0x82003EAC);
  Word(common + 0x58, 0);
  bytes[common + 0x54] = 2;
  Word(0x829CC358, resources);
  Word(resources + 0x18, loader);
  Word(loader, 0x82003F2C);
  Unit(loader + 0x20, 0x9FC);
  Word(0x829CEB78, provider);
  Word(provider, 0x82036508);
  Word(provider + 8, bank);
  bytes[provider + 0xC] = 1;
  bytes[bank + 0x60] = 1;
  Word(bank, resource);
  Word(bank + 4, resource + 0x3000);
  Word(bank + 8, resource + 0x20);
  Word(resource, 0x70617374);
  Word(resource + 4, 0x7274626C);
  bytes[resource + 0xB] = 1;
  Word(resource + 0xC, 0xA10);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x3000);
  Text(0x625, 0x5000, u"Synthetic setup title");
  Text(0xA08, 0x5100, u"Synthetic return hint");
}
void Original(PPCContext& ctx, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  ctx.r3.u64 = 0x123456789;
  ctx.r8.u64 = 0xABCDEF;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Construct(uint32_t mode = 0) {
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r4.u32 = parent + 0x42C;
  ctx.r5.u32 = parent + 0x20;
  ctx.r6.u32 = 1;
  ctx.r7.u32 = mode;
  ctx.r9.u64 = 0xABCDEF0123456789;
  ctx.f2.f64 = 0.125;
  sub_822F70A8(ctx, opaque);
  assert(ctx.r3.u64 == 0x123456789 && ctx.r8.u64 == 0xABCDEF);
  assert(ctx.r9.u64 == 0xABCDEF0123456789 && ctx.f2.f64 == 0.125);
  assert(std::fegetround() == FE_DOWNWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
  Word(parent + 0x14, 4);
}
void Hook(void (*hook)(PPCRegister&)) {
  PPCRegister task{};
  task.u32 = owner;
  const auto before = task.u64;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
  hook(task);
  assert(task.u64 == before && std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
void Select(uint32_t index) {
  Word(owner + 0xFC, index);
  bytes[owner + 0x104] = 1;
  NotifyVsModeSelection(owner + 0xAC, index);
}
void Input(std::function<void()> action = {}) {
  input = std::move(action);
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r9.u64 = 0xABCDEF0123456789;
  ctx.f2.f64 = 0.125;
  const auto before = originals;
  sub_822F7B00(ctx, opaque);
  assert(originals == before + 1);
  assert(ctx.r3.u64 == 0x123456789 && ctx.r8.u64 == 0xABCDEF);
  assert(ctx.r9.u64 == 0xABCDEF0123456789 && ctx.f2.f64 == 0.125);
  assert(std::fegetround() == FE_DOWNWARD && std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
  input = {};
}
void Open() { Fixture(); Construct(); Hook(BrawlersVsModeReady); Select(0); }
}  // namespace

extern "C" void __imp__sub_822F70A8(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822F7248(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822F7B00(PPCContext& ctx, uint8_t* base) {
  if (input) input();
  Original(ctx, base);
}

int main() {
  SetVsModeCapture([](uint32_t address, void* destination, size_t size) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < size; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(destination)[i] = found->second;
    }
    return true;
  }, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic sink failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { diagnostics.emplace_back(reason); }, [] { ++failures; });

  Fixture();
  Construct(1);
  NotifyVsModeSelection(0x7000, 0);
  assert(reads == 0 && events.empty());

  Open();
  assert(events.size() == 2 && events[0].kind == "opened" && events[1].kind == "focused");
  assert(events[0].title == "Synthetic setup title");
  assert(events[0].hints == std::vector<std::string>{"Synthetic return hint"});
  assert(events[0].source == "vs_mode" && events[0].locale == 1);
  assert(events[1].position == 1 && events[1].total == 5);
  assert(events[1].label_key == "vs_identity_0" && events[1].value_key == "vs_mode_slot_human");
  const auto first_id = events[0].id;
  const auto idle_reads = reads;
  Input();
  assert(reads == idle_reads && events.size() == 2);
  // 822F7E30 directly restores a slot after the shared selector event.
  Input([&] {
    Select(4);
    assert(events.size() == 2);
    Word(owner + 0xFC, 0);
  });
  assert(events.size() == 2);  // The transient confirmation never escapes.
  Input([&] {
    Select(4);
    Word(owner + 0xFC, 1);
    assert(events.size() == 2);
  });
  assert(events.size() == 3 && events.back().label_key == "vs_identity_4");
  Select(0);
  assert(events.size() == 4);
  // A native child transition discards the outgoing operation's pending item.
  Input([&] {
    Select(4);
    Hook(BrawlersVsModeSuspended);
    bytes[owner + 0x129] = 1;
  });
  assert(events.size() == 4);
  bytes[owner + 0x129] = 0;
  Word(owner + 0xFC, 0);
  Hook(BrawlersVsModeResumed);
  assert(events.size() == 5 && events.back().kind == "resumed");
  Select(0);
  assert(events.size() == 5);
  Select(4);
  assert(events.size() == 6);
  Input([&] {
    // The graph can remain on a button before 822F7E30 restores a slot.
    Word(owner + 0xFC, 0);
    Hook(BrawlersVsModeValueChanged);
    assert(events.size() == 6);
  });
  assert(events.size() == 7 && events.back().label_key == "vs_identity_0");
  Select(1);
  assert(events.back().value_key == "vs_mode_slot_computer" && events.back().position == 2);
  assert(events.back().label_key == "vs_identity_4");
  bytes[owner + 0x12F + 1] = 1;
  bytes[owner + 0x134] = 1;
  Hook(BrawlersVsModeValueChanged);
  assert(events.back().value_key == "vs_mode_slot_human");
  const auto before_empty = events.size();
  Select(2);
  Select(3);
  assert(events.size() == before_empty + 2);
  assert(events.back().label_key == "vs_mode_slot_empty" && events.back().value_key.empty());
  assert(events.back().position == 4);

  // Disabled mode rows do not appear in the navigable count.
  auto count = events.size();
  Select(5);
  assert(events.size() == count);
  bytes[owner + 0x36] = 4;
  bytes[enabled + 5] = bytes[enabled + 6] = 1;
  Select(5);
  assert(events.back().total == 7 && events.back().label_key == "vs_mode_kind_0");
  assert(events.back().selected);
  Word(owner + 0x38, 1);
  Hook(BrawlersVsModeValueChanged);
  assert(!events.back().selected);
  Select(6);
  assert(events.back().selected && events.back().position == 7);

  // A controller dialog suspends focus without ending the parent lifetime.
  Hook(BrawlersVsModeSuspended);
  count = events.size();
  Select(0);
  assert(events.size() == count);
  Hook(BrawlersVsModeResumed);
  assert(events.back().kind == "resumed" && events.back().id == first_id);
  assert(events.back().title.empty() && events.back().hints.empty());

  // Readable but wrong embedded pointers, lists and parents fail closed.
  count = events.size();
  Word(owner + 0xD4, owner + 0xE8);
  Select(1);
  assert(events.size() == count);
  Word(owner + 0xD4, owner + 0xEC);
  bytes[enabled + 4] = 0;
  Select(1);
  assert(events.size() == count);
  bytes[enabled + 4] = 1;
  Word(parent, 0xBAD);
  Select(1);
  assert(events.size() == count);
  Word(parent, 0x8201A078);

  NotifyVsModeTaskRemoved(owner);
  assert(events.back().kind == "closed" && events.back().id == first_id);
  count = events.size();
  NotifyVsModeTaskRemoved(owner);
  auto before_reads = reads;
  Select(0);
  assert(reads == before_reads && events.size() == count);

  // New native lifetime and actual locale, with no invented locale fallback.
  Fixture();
  Unit(loader + 0x20, 0xA02);
  Construct();
  Hook(BrawlersVsModeReady);
  Select(0);
  assert(events.back().id != first_id && events.back().locale == 7);
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  const auto calls = originals;
  sub_822F7248(ctx, opaque);
  assert(originals == calls + 1 && ctx.r3.u64 == 0x123456789);
  assert(events.back().kind == "closed");

  Fixture();
  Unit(loader + 0x20, 0x9FB);
  Construct();
  Hook(BrawlersVsModeReady);
  count = events.size();
  Select(0);
  assert(events.size() == count);
  NotifyVsModeTaskRemoved(owner);

  // Constructor reuse without removal cannot invent a second lifetime.
  Fixture();
  Construct();
  Word(parent + 0x14, 3);
  Construct();
  Hook(BrawlersVsModeReady);
  Select(0);
  assert(events.size() == count);
  NotifyVsModeTaskRemoved(owner);

  assert(!diagnostics.empty() && failures == 0);
  Fixture();
  Construct();
  Hook(BrawlersVsModeReady);
  throw_output = true;
  Select(0);
  assert(failures == 1);
  before_reads = reads;
  Select(1);
  assert(reads == before_reads && failures == 1);
}
