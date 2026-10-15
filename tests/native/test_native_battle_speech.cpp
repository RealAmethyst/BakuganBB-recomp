// Original synthetic fixtures. No game, audio device, or accessibility service runs.
#include "native_battle_speech.h"

#include <array>
#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_82271750(PPCContext&, uint8_t*);
extern "C" void sub_82232010(PPCContext&, uint8_t*);
extern "C" void sub_822718E0(PPCContext&, uint8_t*);
extern "C" void sub_8224C5D0(PPCContext&, uint8_t*);
extern "C" void sub_8224F880(PPCContext&, uint8_t*);
extern "C" void sub_8224C948(PPCContext&, uint8_t*);
void BrawlersGateSelectionReady(PPCRegister&);
void BrawlersGateSelectionSuspended(PPCRegister&);
void BrawlersBattleSelectionChanged(PPCRegister&);
void BrawlersBattleHintDraw(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersBakuganSelectionReady(PPCRegister&);
void BrawlersBakuganSelectionSuspended(PPCRegister&);
void BrawlersBakuganSelectionCommitted(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersBakuganSelectionCleared(PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}

namespace {
constexpr uint32_t task = 0x1000, common = 0x2000, battle = 0x3000;
constexpr uint32_t rows = 0x6000, cards = 0x7000, provider = 0x8000, bank = 0x9000;
constexpr uint32_t resource = 0x10000;
constexpr uint32_t bakugan_task = 0x90000, bakugan_entries = 0xA0000, available = 0xB0000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> guards;
unsigned originals = 0, reads = 0, failures = 0;
bool throw_output = false;
uint8_t opaque[1]{};

void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[address + i] = static_cast<uint8_t>(value >> (24 - i * 8));
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = static_cast<uint8_t>(value >> 8);
  bytes[address + 1] = static_cast<uint8_t>(value);
}
uint32_t StringOffset(uint32_t id) { return 0x8000 + (id - 0x9E3) * 0x100; }
void Text(uint32_t id, const char16_t* text) {
  const auto offset = StringOffset(id);
  Word(resource + 0x20 + id * 4, offset);
  Word(resource + 0x24 + id * 4, offset + 0x100);
  uint32_t address = resource + offset;
  do {
    Unit(address, *text);
    address += 2;
  } while (*text++);
}
void Fixture() {
  Word(task, 0x8200FD04);
  Word(task + 0x20, 0x8200F830);
  Word(task + 0x14, 2);
  Word(task + 0x38, rows);
  Word(task + 0x3C, rows + 4);
  Word(task + 0x64, 1);
  bytes[rows] = 1;
  bytes[rows + 1] = 1;
  bytes[rows + 2] = 0;
  bytes[rows + 3] = 0;  // Disabled for acceptance, but still navigable.
  Word(0x829CBC68, common);
  Word(common, 0x82003EAC);
  Unit(common + 0x20, 0x8CC);
  Word(0x829CBEC4, battle);
  Word(battle, 0x82009AB0);
  Word(battle + 0x1C94, cards);
  Word(battle + 0x1C98, cards + 8);
  Word(cards, 0x7100);
  Word(cards + 4, 0x7200);
  Word(0x7104, 0x7300);
  Word(0x7204, 0x7400);
  Word(0x7318, 0);
  Word(0x7418, 1);
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
  Text(0x9E3, u"Synthetic selection instructions");
  Text(0xA51, u"Synthetic card alpha");
  Text(0xA52, u"Synthetic effect alpha");
  Text(0xA55, u"Synthetic card beta");
  Text(0xA56, u"Synthetic effect beta");
}
void BakuganFixture() {
  Fixture();
  Word(bakugan_task, 0x8200EAB0);
  Word(bakugan_task + 0x160, 0x82003900);
  Word(bakugan_task + 0x14, 5);
  Word(bakugan_task + 0x34, 1);
  Word(bakugan_task + 0x58, UINT32_MAX);
  Word(bakugan_task + 0x9C0, 0x8200EA74);
  Word(bakugan_task + 0x9CC, 3);
  bytes[bakugan_task + 0x9D2] = 5;
  bytes[battle + 0x24EA] = 3;
  Word(battle + 0x1CA0 + 0xD8 + 0xC, available);
  Word(battle + 0x1CA0 + 0xD8 + 0x10, available + 2);
  // Native navigation orders rows, not this deliberately reversed availability vector.
  bytes[available] = 5;
  bytes[available + 1] = 3;
  Word(battle + 0x1C7C, bakugan_entries);
  Word(battle + 0x1C80, bakugan_entries + 6 * 0x14);
  Word(bakugan_entries + 3 * 0x14, 0);
  Word(bakugan_entries + 5 * 0x14, 1);
  Text(0xDB1, u"Synthetic creature alpha");
  Text(0xDB2, u"Synthetic creature beta");
  Text(0xA39, u"Synthetic creature selection instructions");
  Text(0x9F2, u"Synthetic alternate creature instructions");
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
void Call(void (*hook)(PPCContext&, uint8_t*), uint32_t owner) {
  const auto count = originals;
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r8.u64 = 0x9876543210;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  hook(ctx, opaque);
  assert(originals == count + 1);
  assert(ctx.r3.u64 == 0xABCDEF && ctx.r7.u64 == 0xABCDEF0123456789);
  assert(ctx.r8.u64 == 0x9876543210 && ctx.f1.f64 == 0.5 && ctx.f2.f64 == 0.125);
  assert(ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
template <typename Function, typename... Values>
void Event(Function function, Values... values) {
  std::array<PPCRegister, sizeof...(values)> args{};
  size_t i = 0;
  ((args[i++].u64 = values), ...);
  const auto before = args;
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  [&]<size_t... N>(std::index_sequence<N...>) { function(args[N]...); }
    (std::make_index_sequence<sizeof...(values)>{});
  assert(std::memcmp(args.data(), before.data(), sizeof(args)) == 0);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
void Hint(uint32_t record = common + 0x60, uint32_t id = 0x9E3) {
  Event(BrawlersBattleHintDraw, record, 0, id);
}
}  // namespace

extern "C" void __imp__sub_82271750(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_82232010(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_822718E0(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_8224C5D0(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_8224F880(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }
extern "C" void __imp__sub_8224C948(PPCContext& ctx, uint8_t* base) { Original(ctx, base); }

int main() {
  Fixture();
  SetBattleSpeechCapture([](uint32_t address, void* output, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(output)[i] = found->second;
    }
    return true;
  }, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic output failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });

  // Unknown native carousels are ignored without touching guest memory.
  auto read_count = reads;
  Event(BrawlersBattleSelectionChanged, 0xDEAD);
  assert(reads == read_count && events.empty());
  Call(sub_82271750, task);
  assert(events.empty());
  Event(BrawlersGateSelectionReady, task);
  assert(events.size() == 2);
  assert(events[0].kind == "opened" && events[0].title.empty());
  assert(events[1].kind == "focused" && events[1].label == "Synthetic card alpha");
  assert(events[1].description == "Synthetic effect alpha");
  assert(events[1].position == 2 && events[1].total == 2 && events[1].locale == 1);
  const auto identity = events[0].id;
  Hint(common + 0x94);
  Hint(common + 0x60, 0x9EC);
  assert(events.size() == 2);
  Hint();
  assert(events.size() == 3 && events.back().kind == "focused");
  assert(events.back().label == "Synthetic card alpha" && events.back().position == 2);
  assert(events.back().hints == std::vector<std::string>{"Synthetic selection instructions"});
  read_count = reads;
  Hint();
  assert(reads == read_count && events.size() == 3);
  Event(BrawlersGateSelectionReady, task);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 3);
  Word(task + 0x64, 0);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 4 && events.back().label == "Synthetic card beta");
  assert(events.back().position == 1 && events.back().total == 2);

  // Native navigation uses the whole vector, not just the three displayed slots.
  Word(task + 0x3C, rows + 8);
  bytes[rows + 4] = 0;
  bytes[rows + 5] = 1;
  bytes[rows + 6] = 1;
  bytes[rows + 7] = 1;
  Word(task + 0x64, 3);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 5 && events.back().position == 4 && events.back().total == 4);
  Word(task + 0x3C, rows + 4);

  // Invalid rows and broken definition chains never substitute another card.
  Word(task + 0x64, 2);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 5 && guards.back() == "battle_gate_selection_bounds");
  Word(task + 0x64, 1);
  Word(0x7104, 0);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 5 && guards.back() == "battle_gate_card_definition");
  Word(0x7104, 0x7300);
  Unit(resource + StringOffset(0xA51), 0xDC00);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 5 && guards.back() == "text_read");
  Text(0xA51, u"Synthetic card alpha");
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == 6 && events.back().label == "Synthetic card alpha");
  Word(task + 0x14, 3);
  Event(BrawlersGateSelectionSuspended, task);
  assert(events.size() == 6);
  auto size = events.size();
  Word(task + 0x64, 0);
  Event(BrawlersBattleSelectionChanged, task + 0x20);
  assert(events.size() == size);
  Call(sub_82232010, task + 0x20);
  assert(events.back().kind == "closed" && events.back().id == identity);
  size = events.size();
  NotifyBattleTaskRemoved(task);
  Call(sub_822718E0, task);
  Event(BrawlersGateSelectionReady, task);
  assert(events.size() == size);

  // A delayed first item keeps opening lifetime and queues hints after that item.
  Fixture();
  Call(sub_82271750, task);
  Word(0x829CEB78, 0);
  Event(BrawlersGateSelectionReady, task);
  assert(events.size() == size + 1 && events.back().kind == "opened");
  assert(events.back().id != identity);
  Word(0x829CEB78, provider);
  Hint();
  assert(events.size() == size + 3);
  assert(events[size + 1].kind == "focused" && events[size + 2].kind == "focused");
  assert(events[size + 1].hints.empty() && !events[size + 2].hints.empty());
  NotifyBattleTaskRemoved(task);
  assert(events.back().kind == "closed");

  BakuganFixture();
  size = events.size();
  Call(sub_8224C5D0, bakugan_task);
  Event(BrawlersBakuganSelectionReady, bakugan_task);
  assert(events.size() == size + 1 && events.back().kind == "opened");
  const auto bakugan_identity = events.back().id;
  Hint();  // A card hint cannot attach to the creature picker.
  Hint(common + 0x60, 0xA39);
  assert(events.size() == size + 1);
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 2, 5);
  assert(events.size() == size + 3);
  assert(events[size + 1].label == "Synthetic creature beta");
  assert(events[size + 1].description.empty());
  assert(events[size + 1].position == 2 && events[size + 1].total == 2);
  assert(events[size + 1].hints.empty() && !events[size + 2].hints.empty());
  assert(events[size + 2].kind == "focused");
  read_count = reads;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 2, 5);
  assert(events.size() == size + 3 && reads == read_count);
  bytes[bakugan_task + 0x9D2] = 3;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 0, 3);
  assert(events.back().label == "Synthetic creature alpha" && events.back().position == 1);

  // Leaving and returning to the same item repeats that item, without a new opening.
  bytes[bakugan_task + 0x9D2] = 0xFF;
  Word(bakugan_task + 0x58, 0);
  Event(BrawlersBakuganSelectionCleared, bakugan_task);
  size = events.size();
  Event(BrawlersBakuganSelectionCleared, bakugan_task);
  assert(events.size() == size);
  Word(bakugan_task + 0x58, UINT32_MAX);
  bytes[bakugan_task + 0x9D2] = 3;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 0, 3);
  assert(events.size() == size + 1 && events.back().kind == "resumed");
  assert(events.back().id == bakugan_identity && events.back().position == 1);
  assert(events.back().hints.empty());
  Word(bakugan_task + 0x14, 6);
  Event(BrawlersBakuganSelectionSuspended, bakugan_task);
  size = events.size();
  read_count = reads;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 0, 3);
  assert(events.size() == size && reads == read_count);
  Word(bakugan_task + 0x14, 5);
  Event(BrawlersBakuganSelectionReady, bakugan_task);
  assert(events.back().kind == "resumed" && events.back().label.empty());
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 0, 3);
  assert(events.back().kind == "focused" && events.back().label == "Synthetic creature alpha");
  assert(events.back().id == bakugan_identity);
  size = events.size();
  bytes[battle + 0x24EA] = 4;
  bytes[bakugan_task + 0x9D2] = 6;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 2, 6);
  assert(events.size() == size && guards.back() == "battle_bakugan_unsupported_row_count");
  bytes[battle + 0x24EA] = 3;
  bytes[bakugan_task + 0x9D2] = 4;
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 1, 4);
  assert(events.size() == size && guards.back() == "battle_bakugan_selected_available");
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 2, 4);
  assert(events.size() == size && guards.back() == "battle_bakugan_slot_mapping");
  Call(sub_8224F880, bakugan_task);
  assert(events.back().kind == "closed" && events.back().id == bakugan_identity);
  size = events.size();
  Call(sub_8224C948, bakugan_task);
  NotifyBattleTaskRemoved(bakugan_task);
  Event(BrawlersBakuganSelectionCommitted, bakugan_task, 0, 3);
  assert(events.size() == size);

  // Task type failures stay silent, and capture failures cannot produce stale events.
  size = events.size();
  Word(task, 0x12345678);
  Call(sub_82271750, task);
  Event(BrawlersGateSelectionReady, task);
  assert(events.size() == size && guards.back() == "battle_gate_constructor");
  Fixture();
  Call(sub_82271750, task);
  throw_output = true;
  Event(BrawlersGateSelectionReady, task);
  assert(failures == 1);
  throw_output = false;
  Event(BrawlersGateSelectionReady, task);
  assert(events.size() == size);
}
