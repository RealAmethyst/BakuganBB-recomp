// Original synthetic fixtures; no game, device, or speech service is executed.
#include "native_versus_speech.h"

#include <cassert>
#include <cfenv>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_823681D0(PPCContext&, uint8_t*);
extern "C" void sub_8236A688(PPCContext&, uint8_t*);
extern "C" void sub_82368918(PPCContext&, uint8_t*);
extern "C" void sub_82368750(PPCContext&, uint8_t*);
extern "C" void sub_823688A8(PPCContext&, uint8_t*);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}

namespace {
constexpr uint32_t task = 0x1000, child = 0x2000, widget = 0x3000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> guards;
std::vector<uint32_t> rendered;
unsigned originals = 0, reads = 0, failures = 0;
bool throw_output = false, throw_builder = false;
uint8_t opaque[1]{};

void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[address + i] = static_cast<uint8_t>(value >> (24 - i * 8));
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = static_cast<uint8_t>(value >> 8);
  bytes[address + 1] = static_cast<uint8_t>(value);
}
void Text(uint32_t character, const char16_t* text) {
  uint32_t address = resource + 0x6000 + character * 0x100;
  do { Unit(address, *text); address += 2; } while (*text++);
}
void Fixture(uint32_t type = 0) {
  Word(task, 0x82032BC8);
  Word(task + 0x14, 1);
  Word(task + 0x24, 2);
  Word(task + 0x2C, child);
  Word(child + 0x40, 0x82032BB8);
  Word(child + 0x50, 2);
  Word(child + 0x58, type);
  bytes[child + 0x1C] = 1;
  Word(child + 0x30, 0);
  Word(child + 0x34, 1);
  Word(child + 0x38, 2);
  Word(widget, 0x82032BE4);
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
  Word(resource + 0xC, 0xDB0);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x4000);
  for (uint32_t i = 0; i <= 16; ++i)
    Word(resource + 0x20 + (0xD99 + i) * 4, 0x6000 + i * 0x100);
  Text(0, u"Synthetic alpha");
  Text(1, u"Synthetic beta");
  Text(2, u"Synthetic gamma");
  rendered = {0, 1, 2};
}
void Original(PPCContext& ctx, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  ctx.r3.u64 = widget;
  ctx.r7.u64 = 0xABCDEF0123456789;
  ctx.f1.f64 = 0.5;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Call(void (*hook)(PPCContext&, uint8_t*), uint32_t owner,
          uint32_t mode = 2, uint32_t character = 0) {
  const auto count = originals;
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r5.u32 = mode;
  ctx.r6.u32 = character;
  ctx.r8.u64 = 0x9876543210;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  hook(ctx, opaque);
  assert(originals > count);
  assert(ctx.r3.u64 == widget && ctx.r7.u64 == 0xABCDEF0123456789);
  assert(ctx.r8.u64 == 0x9876543210 && ctx.f1.f64 == 0.5 && ctx.f2.f64 == 0.125);
  assert(ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
void Open() { Call(sub_823681D0, task); Call(sub_8236A688, child); }
void Close() { Call(sub_82368750, child); }
}  // namespace

extern "C" void __imp__sub_823681D0(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_82368918(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_82368750(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_823688A8(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_8236A688(PPCContext& ctx, uint8_t* base) {
  const auto before = events.size();
  for (auto character : rendered) {
    Call(sub_82368918, widget, 0, character);
    assert(events.size() == before);  // Nothing speaks during partial construction.
  }
  if (throw_builder) throw std::runtime_error("synthetic original failure");
  Original(ctx, base);
}

int main() {
  Fixture();
  SetVersusSpeechCapture([](uint32_t address, void* destination, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(destination)[i] = found->second;
    }
    return true;
  }, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic output failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });

  // Non-match panels and unscoped portraits never read guest memory.
  auto before_reads = reads;
  Call(sub_823681D0, task, 0);
  Call(sub_823681D0, task, 1);
  Call(sub_82368918, widget, 0, 0);
  assert(reads == before_reads && events.empty());

  Call(sub_823681D0, task);
  assert(events.empty());
  Call(sub_8236A688, child);
  assert(events.size() == 1 && events.back().kind == "opened");
  assert(events.back().source == "battle_versus");
  assert(events.back().body == "Synthetic alpha, Synthetic beta, Synthetic gamma");
  assert(events.back().title.empty() && events.back().position == 0 && events.back().total == 0);
  const auto first_id = events.back().id;
  Call(sub_8236A688, child);
  assert(events.size() == 1);  // A repeated capture is not a native reopening.
  Close();
  assert(events.size() == 2 && events.back().kind == "closed" && events.back().id == first_id);
  NotifyVersusTaskRemoved(task);
  assert(events.size() == 2);

  // Type 4's native portrait loop omits the first supplied NPC.
  Fixture(4);
  rendered = {1, 2};
  Open();
  assert(events.size() == 3 && events.back().id != first_id);
  assert(events.back().body == "Synthetic beta, Synthetic gamma");
  Call(sub_823688A8, task);
  assert(events.back().kind == "closed");

  // A mismatched or overlong native sequence suppresses the entire opening.
  auto before = events.size();
  Fixture();
  rendered = {0, 2};
  Open();
  assert(events.size() == before);
  Close();
  Fixture();
  rendered = {0, 1, 2, 0};
  Open();
  assert(events.size() == before);
  Close();
  Fixture();
  Word(widget, 0xBAD);
  Open();
  assert(events.size() == before);
  Close();

  // Readiness, provider bounds and constructor reuse fail closed.
  Fixture();
  bytes[child + 0x1C] = 0;
  Open();
  assert(events.size() == before);
  Close();
  Fixture();
  Word(resource + 0x20 + 0xD9B * 4, 1);
  Open();
  assert(events.size() == before);
  Close();
  Fixture();
  Call(sub_823681D0, task);
  Call(sub_823681D0, task);
  Call(sub_8236A688, child);
  assert(events.size() == before);
  Close();

  // Original exceptions propagate, while the temporary capture scope unwinds.
  Fixture();
  Call(sub_823681D0, task);
  throw_builder = true;
  try { Call(sub_8236A688, child); assert(false); } catch (const std::runtime_error&) {}
  throw_builder = false;
  before_reads = reads;
  Call(sub_82368918, widget, 0, 0);
  assert(reads == before_reads && events.size() == before);
  NotifyVersusTaskRemoved(task);

  // A task removal closes once, and a new lifetime can announce new text.
  Fixture(3);
  rendered = {0};
  Open();
  assert(events.size() == before + 1 && events.back().body == "Synthetic alpha");
  NotifyVersusTaskRemoved(task);
  assert(events.size() == before + 2 && events.back().kind == "closed");
  assert(!guards.empty() && failures == 0);

  // Sink failure is latched without preventing original guest execution.
  Fixture();
  throw_output = true;
  Open();
  assert(failures == 1);
  before_reads = reads;
  Call(sub_8236A688, child);
  assert(reads == before_reads && failures == 1);
}
