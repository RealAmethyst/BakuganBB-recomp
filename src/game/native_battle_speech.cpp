// Semantic capture for the pinned battle pickers; see docs/battle-speech.md.
#include "native_battle_speech.h"
#include "native_localization.h"

#include <cfenv>
#include <array>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_82271750(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82232010(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822718E0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8224C5D0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8224F880(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8224C948(PPCContext&, uint8_t*);

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

struct Picker {
  enum class Kind { Gate, Bakugan } kind = Kind::Gate;
  uint64_t id = 0;
  uint32_t locale = 0, common = 0;
  uint32_t selection = UINT32_MAX, card = UINT32_MAX;
  uint32_t pending_selection = UINT32_MAX, pending_slot = UINT32_MAX;
  bool active = false, opened = false, focused = false, hints_sent = false;
  bool resume_item = false;
  std::string hints;
  ScreenEvent item;
};
std::map<uint32_t, Picker> pickers;

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

bool Object(uint32_t owner, uint32_t size, uint32_t table, const char* reason) {
  return Guard(owner && owner <= UINT32_MAX - size && memory.U32BE(owner) == table, reason);
}

ScreenEvent Event(const Picker& picker, const char* kind) {
  ScreenEvent event;
  event.source = "battle_selection";
  event.kind = kind;
  event.id = picker.id;
  event.locale = picker.locale;
  return event;
}

void EmitHints(Picker& picker) {
  if (!picker.active || !picker.focused || picker.hints_sent || picker.hints.empty()) return;
  auto event = picker.item;
  event.kind = "focused";
  event.hints.push_back(picker.hints);
  output(std::move(event));
  picker.hints_sent = true;
}

void FocusGate(uint32_t owner, Picker& picker) {
  if (!picker.active || !picker.opened) return;
  if (!Object(owner, 0x100, 0x8200FD04, "battle_gate_task") ||
      !Object(owner + 0x20, 0x60, 0x8200F830, "battle_gate_selector") ||
      !Guard(memory.U32BE(owner + 0x14) == 2, "battle_gate_active_state")) return;
  const auto begin = memory.U32BE(owner + 0x38);
  const auto end = memory.U32BE(owner + 0x3C);
  const auto selection = memory.U32BE(owner + 0x64);
  if (!Guard(begin && end && *begin && *end >= *begin && (*end - *begin) % 2 == 0,
             "battle_gate_rows") ||
      !Guard(selection.has_value(), "battle_gate_selection_read")) return;
  const auto total = (*end - *begin) / 2;
  // Bound capture work by the byte-sized slot domain; this is not a game hand limit.
  if (!Guard(total > 0 && total <= 256 && *selection < total, "battle_gate_selection_bounds")) return;
  const auto slot = memory.U8(*begin + *selection * 2);
  if (!Guard(slot.has_value(), "battle_gate_slot_read")) return;
  if (!Guard(*slot != 0xFF, "battle_gate_slot_sentinel")) return;
  if (picker.focused && picker.selection == *selection && picker.card == *slot &&
      picker.item.total == total) return;

  const auto battle = memory.U32BE(0x829CBEC4);
  if (!Guard(battle.has_value(), "battle_gate_scene_anchor") ||
      !Object(*battle, 0x2600, 0x82009AB0, "battle_gate_scene_owner")) return;
  // 822142F0 receives the embedded scene at owner + 20, not the task itself.
  const auto cards = memory.U32BE(*battle + 0x1C94);
  const auto cards_end = memory.U32BE(*battle + 0x1C98);
  if (!Guard(cards && cards_end && *cards && *cards_end >= *cards &&
             (*cards_end - *cards) % 4 == 0 && (*cards_end - *cards) / 4 <= 256 &&
             *slot < (*cards_end - *cards) / 4, "battle_gate_card_vector")) return;
  const auto instance = memory.U32BE(*cards + uint32_t{*slot} * 4);
  if (!Guard(instance && *instance && *instance <= UINT32_MAX - 8,
             "battle_gate_card_instance")) return;
  const auto definition = memory.U32BE(*instance + 4);
  if (!Guard(definition && *definition && *definition <= UINT32_MAX - 0x1C,
             "battle_gate_card_definition")) return;
  const auto index = memory.U32BE(*definition + 0x18);
  if (!Guard(index && *index <= (UINT32_MAX - 0xA52) / 4,
             "battle_gate_card_text_index")) return;
  const uint32_t name_id = 0xA51 + *index * 4;
  const auto name = brawlers::ui::ReadMenuText(memory, name_id);
  if (!Guard(name.value.has_value(), name.reason)) return;
  const auto description = brawlers::ui::ReadMenuText(memory, name_id + 1);
  if (!Guard(description.value.has_value(), description.reason)) return;
  auto event = Event(picker, "focused");
  event.label = *name.value;
  event.description = *description.value;
  event.position = *selection + 1;
  event.total = total;
  picker.item = event;
  output(std::move(event));
  picker.selection = *selection;
  picker.card = *slot;
  picker.focused = true;
  EmitHints(picker);
}

void FocusBakugan(uint32_t owner, Picker& picker) {
  if (!picker.active || !picker.opened || picker.pending_selection == UINT32_MAX) return;
  const auto row = picker.pending_selection, slot = picker.pending_slot;
  // The native detail commit can repeat during drawing. Unchanged events do no reads.
  if (picker.focused && picker.selection == row && picker.card == slot) return;
  if (!Object(owner, 0xC00, 0x8200EAB0, "battle_bakugan_task") ||
      !Object(owner + 0x160, 0x60, 0x82003900, "battle_bakugan_selector") ||
      !Object(owner + 0x9C0, 0x40, 0x8200EA74, "battle_bakugan_detail") ||
      !Guard(memory.U32BE(owner + 0x14) == 5, "battle_bakugan_active_state") ||
      !Guard(memory.U32BE(owner + 0x9CC) == 3, "battle_bakugan_detail_mode") ||
      !Guard(slot < 0xFF && memory.U8(owner + 0x9D2) == slot,
             "battle_bakugan_detail_commit")) return;
  const auto locked_row = memory.U32BE(owner + 0x58);
  if (!Guard(locked_row && static_cast<int32_t>(*locked_row) < 0,
             "battle_bakugan_unlocked_selection")) return;
  const auto participant = memory.U32BE(owner + 0x34);
  if (!Guard(participant && *participant < 4, "battle_bakugan_participant")) return;
  const auto battle = memory.U32BE(0x829CBEC4);
  if (!Guard(battle.has_value(), "battle_bakugan_scene_anchor") ||
      !Object(*battle, 0x2600, 0x82009AB0, "battle_bakugan_scene_owner")) return;
  const auto per_participant = memory.U8(*battle + 0x24EA);
  // This detail consumer accepts rows 0..2. Larger native grids need another source.
  if (!Guard(per_participant && *per_participant > 0 && *per_participant <= 3,
             "battle_bakugan_unsupported_row_count")) return;
  if (!Guard(row < *per_participant &&
             (*participant + 1) * uint32_t{*per_participant} <= 255 &&
             slot == *participant * uint32_t{*per_participant} + row,
             "battle_bakugan_slot_mapping")) return;
  const uint32_t player = *battle + 0x1CA0 + *participant * 0xD8;
  const auto begin = memory.U32BE(player + 0xC), end = memory.U32BE(player + 0x10);
  if (!Guard(begin && end && *begin && *end >= *begin && *end - *begin <= 256,
             "battle_bakugan_available_vector")) return;
  std::array<bool, 256> present{};
  for (uint32_t index = 0; index < *end - *begin; ++index) {
    const auto available = memory.U8(*begin + index);
    if (!Guard(available.has_value(), "battle_bakugan_available_entry")) return;
    present[*available] = true;
  }
  if (!Guard(present[slot], "battle_bakugan_selected_available")) return;
  uint32_t position = 0, total = 0;
  for (uint32_t index = 0; index < *per_participant; ++index) {
    if (!present[*participant * *per_participant + index]) continue;
    ++total;
    if (index <= row) ++position;
  }
  if (!Guard(position && position <= total, "battle_bakugan_position")) return;
  const auto entries = memory.U32BE(*battle + 0x1C7C);
  const auto entries_end = memory.U32BE(*battle + 0x1C80);
  if (!Guard(entries && entries_end && *entries && *entries_end >= *entries &&
             (*entries_end - *entries) % 0x14 == 0 &&
             (*entries_end - *entries) / 0x14 <= 256 &&
             slot < (*entries_end - *entries) / 0x14,
             "battle_bakugan_definition_vector")) return;
  const auto name_index = memory.U32BE(*entries + slot * 0x14);
  if (!Guard(name_index && *name_index <= UINT32_MAX - 0xDB1,
             "battle_bakugan_name_index")) return;
  const auto name = brawlers::ui::ReadMenuText(memory, 0xDB1 + *name_index);
  if (!Guard(name.value.has_value(), name.reason)) return;
  auto event = Event(picker, picker.resume_item ? "resumed" : "focused");
  event.label = *name.value;
  event.position = position;
  event.total = total;
  picker.item = event;
  output(std::move(event));
  picker.selection = row;
  picker.card = slot;
  picker.focused = true;
  picker.resume_item = false;
  EmitHints(picker);
}

void Focus(uint32_t owner, Picker& picker) {
  if (picker.kind == Picker::Kind::Gate) FocusGate(owner, picker);
  else FocusBakugan(owner, picker);
}

void Close(uint32_t owner) {
  const auto found = pickers.find(owner);
  if (found == pickers.end()) return;
  if (found->second.opened) output(Event(found->second, "closed"));
  pickers.erase(found);
}

void Activate(uint32_t owner) {
  const auto found = pickers.find(owner);
  if (found == pickers.end()) return;
  auto& picker = found->second;
  const bool bakugan = picker.kind == Picker::Kind::Bakugan;
  if (!Object(owner, bakugan ? 0xC00 : 0x100, bakugan ? 0x8200EAB0 : 0x8200FD04,
              "battle_picker_activate_task") ||
      !Guard(memory.U32BE(owner + 0x14) == (bakugan ? 5U : 2U),
             "battle_picker_activate_state")) return;
  const auto common = memory.U32BE(0x829CBC68);
  if (!Guard(common.has_value(), "battle_gate_common_anchor") ||
      !Object(*common, 0xC8, 0x82003EAC, "battle_gate_common_owner")) return;
  const auto resource = memory.U16BE(*common + 0x20);
  if (!Guard(resource && *resource >= 0x8CC && *resource <= 0x8D2,
             "battle_gate_common_locale")) return;
  picker.common = *common;
  picker.locale = *resource - 0x8CB;
  const bool resumed = picker.opened && !picker.active;
  picker.active = true;
  if (!picker.opened) {
    output(Event(picker, "opened"));
    picker.opened = true;
  }
  if (resumed) {
    picker.focused = false;
    picker.resume_item = false;
    picker.pending_selection = UINT32_MAX;
    picker.pending_slot = UINT32_MAX;
    output(Event(picker, "resumed"));
  }
  Focus(owner, picker);
}

}  // namespace

void SetBattleSpeechCapture(brawlers::ui::ReadBytes reader,
                           std::function<void(ScreenEvent&&)> sink,
                           std::function<void(std::string_view)> report,
                           std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyBattleTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

extern "C" void sub_82271750(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_82271750(ctx, base);
  Observe([&] {
    if (!output || !Object(owner, 0x100, 0x8200FD04, "battle_gate_constructor") ||
        !Guard(!pickers.contains(owner), "battle_gate_duplicate_lifetime")) return;
    Picker picker;
    picker.id = ++next_identity;
    pickers.emplace(owner, std::move(picker));
  });
}

extern "C" void sub_82232010(PPCContext& ctx, uint8_t* base) {
  const auto selector = ctx.r3.u32;
  Observe([&] { if (selector >= 0x20) Close(selector - 0x20); });
  __imp__sub_82232010(ctx, base);
}

extern "C" void sub_822718E0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { Close(owner); });
  __imp__sub_822718E0(ctx, base);
}

void BrawlersGateSelectionReady(PPCRegister& owner) {
  Observe([&] { Activate(owner.u32); });
}

void BrawlersGateSelectionSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = pickers.find(owner.u32);
    if (found == pickers.end() || !found->second.active) return;
    found->second.active = false;
  });
}

void BrawlersBattleSelectionChanged(PPCRegister& selector) {
  Observe([&] {
    if (selector.u32 < 0x20) return;
    const auto found = pickers.find(selector.u32 - 0x20);
    if (found != pickers.end() && found->second.kind == Picker::Kind::Gate)
      Focus(found->first, found->second);
  });
}

void BrawlersBattleHintDraw(PPCRegister& record, PPCRegister& bank, PPCRegister& id) {
  if (bank.u32 != 0 || (id.u32 != 0x9E3 && id.u32 != 0xA39 && id.u32 != 0x9F2)) return;
  Observe([&] {
    for (auto& [owner, picker] : pickers) {
      if (!picker.active || !picker.common || record.u32 != picker.common + 0x60 ||
          picker.hints_sent) continue;
      if ((picker.kind == Picker::Kind::Gate) != (id.u32 == 0x9E3)) continue;
      const auto text = brawlers::ui::ReadMenuText(memory, id.u32);
      if (!Guard(text.value.has_value(), text.reason)) return;
      picker.hints = *text.value;
      Focus(owner, picker);
      EmitHints(picker);
      return;
    }
  });
}

extern "C" void sub_8224C5D0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_8224C5D0(ctx, base);
  Observe([&] {
    if (!output || !Object(owner, 0xC00, 0x8200EAB0, "battle_bakugan_constructor") ||
        !Guard(!pickers.contains(owner), "battle_bakugan_duplicate_lifetime")) return;
    Picker picker;
    picker.kind = Picker::Kind::Bakugan;
    picker.id = ++next_identity;
    pickers.emplace(owner, std::move(picker));
  });
}

extern "C" void sub_8224F880(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { Close(owner); });
  __imp__sub_8224F880(ctx, base);
}

extern "C" void sub_8224C948(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { Close(owner); });
  __imp__sub_8224C948(ctx, base);
}

void BrawlersBakuganSelectionReady(PPCRegister& owner) {
  Observe([&] { Activate(owner.u32); });
}

void BrawlersBakuganSelectionSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = pickers.find(owner.u32);
    if (found != pickers.end() && found->second.kind == Picker::Kind::Bakugan)
      found->second.active = false;
  });
}

void BrawlersBakuganSelectionCommitted(PPCRegister& owner, PPCRegister& row, PPCRegister& slot) {
  Observe([&] {
    const auto found = pickers.find(owner.u32);
    if (found == pickers.end() || found->second.kind != Picker::Kind::Bakugan ||
        !found->second.active) return;
    auto& picker = found->second;
    picker.pending_selection = row.u32;
    picker.pending_slot = slot.u32;
    FocusBakugan(owner.u32, picker);
  });
}

void BrawlersBakuganSelectionCleared(PPCRegister& owner) {
  Observe([&] {
    const auto found = pickers.find(owner.u32);
    if (found == pickers.end() || found->second.kind != Picker::Kind::Bakugan) return;
    auto& picker = found->second;
    if (picker.focused) picker.resume_item = true;
    picker.focused = false;
    picker.pending_selection = UINT32_MAX;
    picker.pending_slot = UINT32_MAX;
  });
}
