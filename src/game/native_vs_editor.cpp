#include "native_vs_editor.h"
#include "native_localization.h"

#include <algorithm>
#include <array>
#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_823111E0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82311528(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8231CDA8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8230FDA0(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8230F290(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82305978(PPCContext&, uint8_t*);
extern "C" void __imp__sub_82312E18(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8230F220(PPCContext&, uint8_t*);
extern "C" void __imp__sub_8231A578(PPCContext&, uint8_t*);
extern "C" void __imp__sub_823B9220(PPCContext&, uint8_t*);

namespace {
using brawlers::ui::GuestUiMemory;
using Kind = VsEditorEventKind;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;
std::function<void(VsEditorEvent&&)> output;
std::function<void(std::string_view)> diagnostic;
std::function<void()> failure;
std::set<std::string> reported;
uint64_t next_identity = 0;
bool failed = false;
thread_local uint32_t hint_context = 0;

struct Editor {
  uint64_t id = 0, child_id = 0;
  uint32_t summary = 0, root = 0, participant = 0, controller = 0, locale = 0;
  bool opened = false, active = false, pending = true, resumed = false;
  bool header_pending = true, hints_pending = true;
  bool status_pending = true, footer_pending = true;
  bool picker = false, confirmation = false, child_opened = false, child_focused = false;
  bool status_sent = false;
  uint32_t child_choice = 0;
  std::optional<VsEditorSelection> previous;
  std::string header, hints;
};
std::map<uint32_t, Editor> screens;

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
    // An unavailable lock never permits unprotected guest reads.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Owner(uint32_t owner, const Editor& editor, bool constructing = false) {
  return Guard(owner && owner <= UINT32_MAX - 0x4E0 &&
               memory.U32BE(owner) == 0x82018DE8 && memory.U32BE(owner + 0x18) == 0,
               "vs_editor_owner") &&
         Guard(memory.U32BE(owner + 0x2C) == editor.summary + 0x200 &&
               memory.U32BE(owner + 0x1C) == editor.participant &&
               memory.U32BE(editor.summary) == 0x82017DDC &&
               memory.U32BE(editor.summary + 0x18) == editor.root + 0x42C &&
               memory.U32BE(editor.summary + 0x1F4) == editor.root + 0x44 &&
               memory.U32BE(editor.root) == 0x8201A078 &&
               memory.U32BE(0x829CC54C) == editor.root &&
               memory.U32BE(editor.root + 0x14) == 8 &&
               memory.U32BE(editor.summary + 0x14) == (constructing ? 5U : 6U),
               "vs_editor_parent_binding");
}

bool Locale(uint32_t owner, Editor& editor) {
  const auto loader = memory.U32BE(owner + 0xA0);
  if (!Guard(loader && *loader && *loader <= UINT32_MAX - 0x24 &&
             memory.U32BE(*loader) == 0x82003F2C,
             "vs_editor_resource_loader")) return false;
  const auto resource = memory.U16BE(*loader + 0x20);
  if (!Guard(resource && *resource >= 0x9B4 && *resource <= 0x9BA,
             "vs_editor_resource_locale")) return false;
  editor.locale = *resource - 0x9B3;
  return true;
}

constexpr std::array<uint32_t, 6> attribute_names{0x656, 0x657, 0x65A, 0x658, 0x659, 0x65B};

bool Text(uint32_t id, std::string& destination) {
  const auto text = brawlers::ui::ReadMenuText(memory, id);
  if (!Guard(text.value.has_value(), text.reason)) return false;
  destination = *text.value;
  return true;
}

VsEditorEvent Event(const Editor& editor, Kind kind) {
  VsEditorEvent event;
  event.kind = kind;
  event.id = editor.id;
  event.child_id = editor.child_id;
  event.controller = editor.controller;
  event.locale = editor.locale;
  return event;
}

void Open(Editor& editor) {
  if (!editor.opened && output) {
    output(Event(editor, Kind::Opened));
    editor.opened = true;
  }
}

bool Graph(uint32_t selector, uint32_t count, uint32_t& selected, uint32_t& mask) {
  const uint32_t model = selector + 0x40;
  if (!Guard(memory.U32BE(selector) == 0x8200FC30 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x8200FC10 &&
             memory.U32BE(model + 4) == (count == 9 ? 0x82018A98U : 0x82018814U),
             "vs_editor_graph_owner") ||
      !Guard(memory.U32BE(model + 0x14) == count, "vs_editor_graph_count") ||
      !Guard(memory.U8(model + 0x18) == 1, "vs_editor_graph_inactive")) return false;
  const auto index = memory.U32BE(model + 0x10);
  const auto flags = memory.U32BE(model + 8);
  if (!Guard(index && *index < count, "vs_editor_graph_index") ||
      !Guard(flags && *flags && *flags <= UINT32_MAX - count,
             "vs_editor_graph_flags")) return false;
  mask = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const auto enabled = memory.U8(*flags + i);
    if (!Guard(enabled && *enabled <= 1, "vs_editor_graph_enabled")) return false;
    mask |= uint32_t{*enabled} << i;
  }
  if (!Guard(mask & (uint32_t{1} << *index), "vs_editor_disabled_selection")) return false;
  selected = *index;
  return true;
}

bool MainState(uint32_t owner, Editor& editor, uint32_t& section) {
  if (!Owner(owner, editor)) return false;
  const auto phase = memory.U32BE(owner + 0x14);
  const auto current = memory.U8(owner + 0x3A);
  if (!phase || (*phase != 3 && *phase != 5 && *phase != 8) ||
      memory.U32BE(owner + 0x50) != 1) return false;
  if (!Guard(current && (*current == 0 || (*current >= 2 && *current <= 5)),
             "vs_editor_section")) return false;
  section = *current;
  return Locale(owner, editor);
}

bool Snapshot(uint32_t owner, Editor& editor, VsEditorSelection& item) {
  if (!MainState(owner, editor, item.section) ||
      !Graph(owner + 0x148, 9, item.row, item.enabled_rows)) return false;
  // Arrow controls immediately change the page/section and restore item focus.
  if (item.row == 0 || item.row == 7) return false;
  item.finish = item.row == 8;
  const auto empty = memory.U8(owner + 0x44);
  if (!Guard(empty && *empty <= 1, "vs_editor_empty_deck")) return false;
  item.empty = *empty != 0;
  uint32_t inventory_begin = 0, inventory_kind = 0, inventory_attribute = 0;
  if (item.section >= 4) {
    const auto kind = memory.U8(owner + 0x9A), attribute = memory.U8(owner + 0x56);
    const auto page = memory.U8(owner + 0x57), last = memory.U8(owner + 0x58);
    if (!Guard(kind && *kind <= 1 && item.section == *kind + 4 && attribute && *attribute < 6,
               "vs_editor_inventory_group") ||
        !Guard(page && last && *page <= *last, "vs_editor_inventory_page")) return false;
    const uint32_t group = (*kind * 6 + *attribute) * 0x18;
    const auto begin = memory.U32BE(owner + 0x3B8 + group);
    const auto end = memory.U32BE(owner + 0x3BC + group);
    if (!Guard(begin && end && *end >= *begin && !(*begin % 2) &&
               !((*end - *begin) % 2) && (*end - *begin) / 2 <= (*kind ? 0x117U : 0x53U),
               "vs_editor_inventory_vector")) return false;
    item.inventory_count = (*end - *begin) / 2;
    item.page = *page;
    item.last_page = *last;
    if (!Guard(*last == (item.inventory_count ? (item.inventory_count - 1) / 6 : 0),
               "vs_editor_inventory_last_page")) return false;
    inventory_begin = *begin;
    inventory_kind = *kind;
    inventory_attribute = *attribute;
  }
  if (item.section >= 2 && !item.empty) {
    const auto header = memory.Text(editor.participant + 0xA8, 22);
    if (!Guard(header.value.has_value(), "vs_editor_section_caption")) return false;
    item.section_caption = *header.value;
  }
  if (item.finish || item.section == 0) return true;
  const uint32_t slot = item.row - 1;
  if (!Guard(memory.U8(owner + 0x3C) == slot, "vs_editor_slot_selection")) return false;
  const auto type = memory.U8(owner + 0x5B + slot);
  const auto id = memory.U16BE(owner + 0x62 + slot * 2);
  const auto attribute = memory.U8(owner + 0x6E + slot);
  const auto replaceable = memory.U8(owner + 0x74 + slot);
  if (!Guard(type && *type <= 2 && id && attribute && replaceable && *replaceable <= 1,
             "vs_editor_item_fields")) return false;
  if (item.section >= 4) {
    const uint32_t absolute = item.page * 6 + slot;
    if (!Guard(absolute < item.inventory_count, "vs_editor_inventory_position") ||
        !Guard(memory.U16BE(inventory_begin + absolute * 2) == *id &&
               *attribute == inventory_attribute &&
               *type == (inventory_kind ? 2U - (inventory_attribute < 3) : 0U),
               "vs_editor_inventory_item")) return false;
  }
  item.item_type = *type;
  item.replaceable = *replaceable != 0;
  item.empty = (*id & 0x8000) != 0;
  if (!item.empty) {
    if (!Guard(*id < (*type == 0 ? 0x53U : 0x117U) && *attribute < 6,
               "vs_editor_item_domain")) return false;
    item.item_id = *id;
    item.attribute = *attribute;
    if (*type == 0) {
      if (!Text(attribute_names[*attribute], item.attribute_name)) return false;
      if (!Guard(memory.U32BE(owner + 0xE0) == 0x82018C1C &&
                 memory.U32BE(owner + 0xEC) == owner + 0x148 &&
                 memory.U32BE(owner + 0xF0) == editor.participant &&
                 memory.U32BE(owner + 0xF8) == owner + 0x38,
                 "vs_editor_numeric_model")) return false;
      uint32_t instance = 0;
      if (item.section == 2) {
        if (!Guard(slot < 3, "vs_editor_numeric_slot")) return false;
        instance = editor.participant + slot * 0x14;
      } else {
        const auto inventory = memory.U32BE(owner + 0x20);
        const uint32_t offset = (*id * 6 + *attribute) * 0x14;
        if (!Guard(inventory && *inventory && *inventory <= UINT32_MAX - offset - 0x14 &&
                   memory.U32BE(owner + 0xF4) == inventory, "vs_editor_numeric_inventory")) return false;
        instance = *inventory + offset;
      }
      const auto value = memory.U16BE(instance + 8);
      if (!Guard(value.has_value(), "vs_editor_numeric_value")) return false;
      // Three native digit widgets select hundreds, tens and units. Negative
      // signed values render zero; frames above nine are not verified digits.
      const uint32_t displayed = (*value & 0x8000) ? 0 : *value;
      if (Guard(displayed <= 999, "vs_editor_numeric_digit_range")) item.displayed_number = displayed;
      const auto level = memory.U8(instance + 0x10);
      if (!Guard(level.has_value(), "vs_editor_level_value")) return false;
      if (*level < 0x80 && Guard(*level >= 1 && *level <= 10, "vs_editor_level_frame"))
        item.level = *level;
    }
  }
  return true;
}

bool EmptyInventory(uint32_t owner, Editor& editor) {
  uint32_t section = 0;
  if (!editor.active || !editor.pending || !MainState(owner, editor, section) || section != 5)
    return false;
  const auto attribute = memory.U8(owner + 0x56);
  if (!Guard(memory.U8(owner + 0x9A) == 1 && attribute && *attribute < 6,
             "vs_editor_empty_inventory_group")) return false;
  const uint32_t group = (6 + *attribute) * 0x18;
  const auto begin = memory.U32BE(owner + 0x3B8 + group);
  const auto end = memory.U32BE(owner + 0x3BC + group);
  if (!Guard(begin && end && *end >= *begin && !(*begin % 2) && !((*end - *begin) % 2),
             "vs_editor_empty_inventory_vector") || *begin != *end) return false;
  if (!Guard(memory.U8(owner + 0x57) == 0 && memory.U8(owner + 0x58) == 0,
             "vs_editor_empty_inventory_page")) return false;
  const auto header = memory.Text(editor.participant + 0xA8, 22);
  if (!Guard(header.value.has_value(), "vs_editor_empty_inventory_header")) return false;
  auto event = Event(editor, Kind::EmptyInventory);
  event.section = section;
  event.section_caption = *header.value;
  output(std::move(event));
  editor.pending = false;
  editor.resumed = false;
  return true;
}

void Focus(uint32_t owner, Editor& editor, const std::optional<std::string>& caption = {}) {
  if (!editor.active || !editor.pending || !output) return;
  if (EmptyInventory(owner, editor)) return;
  VsEditorSelection item;
  if (!Snapshot(owner, editor, item)) return;
  if (!item.finish) {
    if (!caption) return;
    if (!Guard(item.empty || !caption->empty(), "vs_editor_selected_caption")) return;
    item.caption = *caption;
  }
  Open(editor);
  if (!editor.resumed && editor.previous && *editor.previous == item) {
    editor.pending = false;
    return;
  }
  auto event = Event(editor, editor.resumed ? Kind::Resumed : Kind::Focused);
  event.section = item.section;
  event.selection = item;
  output(std::move(event));
  editor.previous = std::move(item);
  editor.pending = false;
  editor.resumed = false;
}

void Dirty(Editor& editor) {
  editor.pending = true;
  editor.header_pending = true;
  editor.hints_pending = true;
  editor.status_pending = true;
  editor.footer_pending = true;
}

void Suspend(Editor& editor) {
  if (editor.active && editor.opened && output) output(Event(editor, Kind::Suspended));
  editor.active = false;
  editor.resumed = editor.opened;
  Dirty(editor);
}

void CloseChild(Editor& editor) {
  if (editor.child_opened && output)
    output(Event(editor, editor.picker ? Kind::PickerClosed : Kind::ConfirmationClosed));
  editor.picker = false;
  editor.confirmation = false;
  editor.child_opened = false;
  editor.child_focused = false;
  editor.status_sent = false;
  editor.child_id = 0;
}

void Close(uint32_t owner) {
  const auto found = screens.find(owner);
  if (found == screens.end()) return;
  CloseChild(found->second);
  if (found->second.opened && output) output(Event(found->second, Kind::Closed));
  screens.erase(found);
}

bool Model(uint32_t owner, uint32_t model, uint32_t offset) {
  return Guard(model == owner + offset && memory.U32BE(model) == 0x82018C1C &&
               memory.U32BE(model + 0xC) == owner + 0x148 &&
               memory.U32BE(model + 0x18) == owner + 0x38 &&
               memory.U32BE(model + 0x10) == memory.U32BE(owner + 0x1C),
               "vs_editor_render_model");
}

std::optional<uint32_t> BoundModel(uint32_t widget, uint32_t context) {
  if (!Guard(widget && widget <= UINT32_MAX - 0xC && context && context <= UINT32_MAX - 0x10,
             "vs_editor_render_addresses")) return {};
  const auto list = memory.U32BE(context + 0xC);
  const auto index = memory.U16BE(widget + 0xA);
  if (!Guard(list && *list && *list <= UINT32_MAX - 0x14 && index,
             "vs_editor_render_list")) return {};
  const auto begin = memory.U32BE(*list + 0xC), end = memory.U32BE(*list + 0x10);
  if (!Guard(begin && end && *begin && *end >= *begin && !(*begin % 4) &&
             !((*end - *begin) % 4) && *index < (*end - *begin) / 4,
             "vs_editor_render_index")) return {};
  const auto model = memory.U32BE(*begin + *index * 4);
  if (!Guard(model && *model, "vs_editor_render_binding")) return {};
  return model;
}

void PickerFocus(uint32_t owner, Editor& editor) {
  if (!editor.picker || !output || !Owner(owner, editor) ||
      memory.U32BE(owner + 0x14) != 7 || memory.U32BE(owner + 0x278) != 1 ||
      !Locale(owner, editor)) return;
  const auto kind = memory.U32BE(owner + 0x274);
  uint32_t selected = 0, mask = 0;
  if (!Guard(kind && *kind <= 1, "vs_editor_picker_kind") ||
      !Graph(owner + 0x210, 6, selected, mask) ||
      !Guard(mask == 0x3F, "vs_editor_picker_enabled")) return;
  if (!editor.child_opened) {
    auto opening = Event(editor, Kind::PickerOpened);
    opening.picker_kind = *kind;
    if (!Text(*kind ? 0x6BC : 0x6B9, opening.text)) return;
    output(std::move(opening));
    editor.child_opened = true;
  }
  if (editor.child_focused && editor.child_choice == selected) return;
  auto event = Event(editor, Kind::PickerFocused);
  event.picker_kind = *kind;
  event.choice = selected;
  event.choices = 6;
  if (*kind == 0 && !Text(attribute_names[selected], event.label)) return;
  // These are real descriptions; the six selectable captions are icons only.
  if (!Text(*kind ? 0x563 + selected : 0x98B, event.text)) return;
  output(std::move(event));
  editor.child_choice = selected;
  editor.child_focused = true;
}

void ConfirmationFocus(uint32_t owner, Editor& editor) {
  if (!editor.confirmation || !output || !Owner(owner, editor) ||
      memory.U32BE(owner + 0x14) != 11 || !Locale(owner, editor)) return;
  const uint32_t selector = owner + 0x1A8, model = owner + 0x1E8;
  if (!Guard(memory.U32BE(selector) == 0x82003900 &&
             memory.U32BE(selector + 0x28) == model &&
             memory.U32BE(model) == 0x820038DC, "vs_editor_confirmation_selector") ||
      !Guard(memory.U32BE(model + 8) == 0 && memory.U32BE(model + 0xC) == 2,
             "vs_editor_confirmation_count")) return;
  const auto selected = memory.U32BE(model + 4);
  const auto kind = memory.U8(owner + 0x204);
  if (!Guard(selected && *selected < 2 && kind && *kind < 5,
             "vs_editor_confirmation_choice")) return;
  if (!editor.child_opened) {
    constexpr std::array<uint32_t, 5> prompts{0x914, 0x918, 0x7F6, 0x7F6, 0x9A4};
    auto opening = Event(editor, Kind::ConfirmationOpened);
    if (!Text(prompts[*kind], opening.text)) return;
    output(std::move(opening));
    editor.child_opened = true;
  }
  if (editor.child_focused && editor.child_choice == *selected) return;
  auto event = Event(editor, Kind::ConfirmationFocused);
  event.choice = *selected;
  event.choices = 2;
  output(std::move(event));
  editor.child_choice = *selected;
  editor.child_focused = true;
}

void Register(uint32_t owner, uint32_t completion, uint32_t mode, uint32_t participant,
              uint32_t player, uint32_t layer) {
  if (!output || completion < 0x200 || player >= 4) return;
  const uint32_t summary = completion - 0x200;
  const auto result = memory.U32BE(summary + 0x18);
  if (!Guard(summary && summary <= UINT32_MAX - 0x208 &&
             memory.U32BE(summary) == 0x82017DDC && memory.U32BE(summary + 0x14) == 5 &&
             result && *result >= 0x42C, "vs_editor_parent")) return;
  const uint32_t root = *result - 0x42C;
  if (!Guard(root && root <= UINT32_MAX - 0x438 && memory.U32BE(root) == 0x8201A078 &&
             memory.U32BE(root + 0x14) == 8 && mode == root + 0x20 &&
             participant == root + 0x44 + player * 0xF8 &&
             memory.U32BE(summary + 0x1FC) == layer, "vs_editor_root_scope")) return;
  Editor editor;
  editor.summary = summary;
  editor.root = root;
  editor.participant = participant;
  if (!Owner(owner, editor, true)) return;
  const auto controller = memory.U8(owner + 0x38);
  if (!Guard(controller && *controller < 4, "vs_editor_controller") ||
      !Guard(!screens.contains(owner), "vs_editor_duplicate_lifetime")) return;
  editor.controller = *controller;
  editor.id = ++next_identity;
  screens.emplace(owner, std::move(editor));
}
}  // namespace

void SetVsEditorCapture(brawlers::ui::ReadBytes reader,
                        std::function<void(VsEditorEvent&&)> sink,
                        std::function<void(std::string_view)> report,
                        std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsEditorTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

void NotifyVsEditorSelection(uint32_t selector, uint32_t selected) noexcept {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (selector == owner + 0x148 && editor.active) {
        uint32_t actual = 0, mask = 0;
        if (!Graph(selector, 9, actual, mask) ||
            !Guard(actual == selected, "vs_editor_selection_event_mismatch")) return;
        Dirty(editor);
        return;
      }
      if (selector == owner + 0x210 && editor.picker) {
        uint32_t actual = 0, mask = 0;
        if (!Graph(selector, 6, actual, mask) ||
            !Guard(actual == selected, "vs_editor_picker_event_mismatch")) return;
        PickerFocus(owner, editor);
        return;
      }
    }
  });
}

extern "C" void sub_823111E0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32, mode = ctx.r5.u32;
  const auto participant = ctx.r6.u32, player = ctx.r7.u32, layer = ctx.r8.u32;
  __imp__sub_823111E0(ctx, base);
  Observe([&] { Register(owner, completion, mode, participant, player, layer); });
}

extern "C" void sub_82311528(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32, mode = ctx.r5.u32;
  const auto participant = ctx.r6.u32, player = ctx.r7.u32, layer = ctx.r8.u32;
  __imp__sub_82311528(ctx, base);
  Observe([&] { Register(owner, completion, mode, participant, player, layer); });
}

extern "C" void sub_8231CDA8(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_8231CDA8(ctx, base);
}

extern "C" void sub_8230FDA0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_8230FDA0(ctx, base);
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end()) Focus(owner, found->second);
  });
}

extern "C" void sub_8230F290(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_8230F290(ctx, base);
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end()) Dirty(found->second);
  });
}

extern "C" void sub_82305978(PPCContext& ctx, uint8_t* base) {
  const auto picker = ctx.r3.u32;
  __imp__sub_82305978(ctx, base);
  Observe([&] {
    if (picker < 0x20C) return;
    const auto found = screens.find(picker - 0x20C);
    if (found == screens.end() || !Owner(found->first, found->second)) return;
    Suspend(found->second);
    CloseChild(found->second);
    found->second.picker = true;
    found->second.child_id = ++next_identity;
  });
}

extern "C" void sub_82312E18(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_82312E18(ctx, base);
  Observe([&] {
    const auto found = screens.find(owner);
    if (found == screens.end() || !Owner(owner, found->second)) return;
    Suspend(found->second);
    CloseChild(found->second);
    found->second.confirmation = true;
    found->second.child_id = ++next_identity;
  });
}

extern "C" void sub_8230F220(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_8230F220(ctx, base);
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end() && found->second.confirmation) CloseChild(found->second);
  });
}

void NotifyVsEditorViewerOpened(uint32_t owner) noexcept {
  Observe([&] {
    const auto found = screens.find(owner);
    if (found != screens.end() && Owner(owner, found->second) && memory.U32BE(owner + 0x14) == 16)
      Suspend(found->second);
  });
}

extern "C" void sub_823B9220(PPCContext& ctx, uint8_t* base) {
  const auto model = ctx.r3.u32;
  __imp__sub_823B9220(ctx, base);
  Observe([&] {
    if (model < 0x1E8) return;
    const auto found = screens.find(model - 0x1E8);
    if (found != screens.end()) ConfirmationFocus(found->first, found->second);
  });
}

void BrawlersVsEditorReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found == screens.end()) return;
    uint32_t section = 0;
    if (!MainState(owner.u32, found->second, section)) return;
    found->second.active = true;
    Open(found->second);
    Dirty(found->second);
    Focus(owner.u32, found->second);
  });
}

void BrawlersVsEditorCaption(PPCRegister& widget, PPCRegister& model) {
  Observe([&] {
    if (model.u32 < 0xC4) return;
    const auto found = screens.find(model.u32 - 0xC4);
    if (found == screens.end() || !found->second.active || !found->second.pending) return;
    if (!Model(found->first, model.u32, 0xC4) ||
        !Guard(widget.u32 && widget.u32 <= UINT32_MAX - 0x1A0 &&
               memory.U32BE(widget.u32) == 0x82018D88, "vs_editor_caption_widget")) return;
    const auto caption = memory.Text(widget.u32 + 0x104, 35);
    if (!Guard(caption.value.has_value(), "vs_editor_caption_text")) return;
    Focus(found->first, found->second, caption.value);
  });
}

void BrawlersVsEditorHeader(PPCRegister& widget, PPCRegister& model, PPCRegister& text) {
  Observe([&] {
    if (model.u32 < 0xC4) return;
    const auto found = screens.find(model.u32 - 0xC4);
    if (found == screens.end() || !found->second.active || !found->second.header_pending) return;
    auto& editor = found->second;
    uint32_t section = 0;
    if (!MainState(found->first, editor, section) || !Model(found->first, model.u32, 0xC4) ||
        !Guard(widget.u32 && memory.U32BE(widget.u32) == 0x82018D9C &&
               text.u32 == editor.participant + 0xA8, "vs_editor_header_consumer")) return;
    const auto caption = memory.Text(text.u32, 22);
    if (!Guard(caption.value.has_value(), "vs_editor_header_text")) return;
    if (editor.header != *caption.value) {
      auto event = Event(editor, Kind::Header);
      event.section = section;
      event.text = *caption.value;
      output(std::move(event));
      editor.header = *caption.value;
    }
    editor.header_pending = false;
  });
}

extern "C" void sub_8231A578(PPCContext& ctx, uint8_t* base) {
  struct Scope {
    uint32_t previous = hint_context;
    explicit Scope(uint32_t context) { hint_context = context; }
    ~Scope() { hint_context = previous; }
  } scope(ctx.r4.u32);
  __imp__sub_8231A578(ctx, base);
}

void BrawlersVsEditorHints(PPCRegister& widget, PPCRegister& text) {
  Observe([&] {
    if (!hint_context || std::none_of(screens.begin(), screens.end(), [](const auto& entry) {
          return entry.second.active && entry.second.hints_pending;
        })) return;
    const auto model = BoundModel(widget.u32, hint_context);
    if (!model || *model < 0xA8) return;
    const auto found = screens.find(*model - 0xA8);
    if (found == screens.end() || !found->second.active || !found->second.hints_pending) return;
    auto& editor = found->second;
    uint32_t section = 0;
    if (!MainState(found->first, editor, section) || !Model(found->first, *model, 0xA8) ||
        !Guard(widget.u32 && memory.U32BE(widget.u32) == 0x82018D94,
               "vs_editor_hint_widget")) return;
    const auto hints = memory.Text(text.u32);
    if (!Guard(hints.value.has_value(), "vs_editor_hint_text")) return;
    {
      auto event = Event(editor, Kind::Hints);
      event.section = section;
      event.text = *hints.value;
      output(std::move(event));
      editor.hints = *hints.value;
    }
    editor.hints_pending = false;
  });
}

void BrawlersVsEditorPickerReady(PPCRegister& picker) {
  Observe([&] {
    if (picker.u32 < 0x20C) return;
    const auto found = screens.find(picker.u32 - 0x20C);
    if (found != screens.end()) PickerFocus(found->first, found->second);
  });
}

void BrawlersVsEditorPickerClosed(PPCRegister& picker) {
  Observe([&] {
    if (picker.u32 < 0x20C) return;
    const auto found = screens.find(picker.u32 - 0x20C);
    if (found != screens.end() && found->second.picker &&
        memory.U32BE(picker.u32 + 0x6C) == 3) CloseChild(found->second);
  });
}

void BrawlersVsEditorConfirmationReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = screens.find(owner.u32);
    if (found != screens.end()) ConfirmationFocus(owner.u32, found->second);
  });
}

void BrawlersVsEditorConfirmationStatus(PPCRegister& widget, PPCRegister& context,
                                        PPCRegister& text_id) {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (!editor.confirmation || !editor.child_opened || editor.status_sent) continue;
      if (!Owner(owner, editor) || memory.U32BE(owner + 0x14) != 12) continue;
      if (BoundModel(widget.u32, context.u32) != owner + 0xFC) continue;
      const auto kind = memory.U8(owner + 0x204);
      if (!Guard(kind && *kind < 2 && text_id.u32 == (*kind ? 0x92D : 0x915),
                 "vs_editor_confirmation_status_id") ||
          !Guard(widget.u32 && memory.U32BE(widget.u32) == 0x82018DE0,
                 "vs_editor_confirmation_status_widget")) return;
      auto event = Event(editor, Kind::ConfirmationStatus);
      if (!Text(text_id.u32, event.text)) return;
      output(std::move(event));
      editor.status_sent = true;
    }
  });
}

void BrawlersVsEditorStatus(PPCRegister& widget, PPCRegister& context, PPCRegister& text_id) {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (!editor.active || !editor.status_pending) continue;
      uint32_t section = 0;
      if (!MainState(owner, editor, section)) continue;
      const auto model = BoundModel(widget.u32, context.u32);
      if (!model || *model != owner + 0xC4) continue;
      if (!Model(owner, *model, 0xC4) ||
          !Guard(memory.U32BE(widget.u32) == 0x82018CF4,
                 "vs_editor_status_widget") ||
          !Guard((text_id.u32 == 0x996 && section == 0 && memory.U8(owner + 0x44) == 1) ||
                 (text_id.u32 == 0x51A && section == 4), "vs_editor_status_id")) return;
      auto event = Event(editor, Kind::Status);
      event.section = section;
      VsEditorSelection selection;
      if (section == 0 && Snapshot(owner, editor, selection)) event.selection = selection;
      if (!Text(text_id.u32, event.text)) return;
      if (section == 4) {
        const auto header = memory.Text(editor.participant + 0xA8, 22);
        if (!Guard(header.value.has_value(), "vs_editor_status_header")) return;
        event.section_caption = *header.value;
      }
      output(std::move(event));
      editor.status_pending = false;
    }
  });
}

void BrawlersVsEditorPickerTitle(PPCRegister& widget, PPCRegister& context) {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (!editor.picker || (editor.child_opened && editor.child_focused)) continue;
      if (BoundModel(widget.u32, context.u32) != owner + 0x298) continue;
      if (!Guard(memory.U32BE(widget.u32) == 0x82018950 &&
                 memory.U32BE(owner + 0x298) == 0x82018880 &&
                 memory.U32BE(owner + 0x2A4) == owner + 0x210 &&
                 memory.U32BE(owner + 0x2A8) == owner + 0x274,
                 "vs_editor_picker_title_binding")) return;
      PickerFocus(owner, editor);
    }
  });
}

void BrawlersVsEditorConfirmationPrompt(PPCRegister& widget, PPCRegister& context) {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (!editor.confirmation || (editor.child_opened && editor.child_focused)) continue;
      if (BoundModel(widget.u32, context.u32) != owner + 0xFC) continue;
      if (!Guard(memory.U32BE(widget.u32) == 0x82018DBC &&
                 memory.U32BE(owner + 0xFC) == 0x82018C20 &&
                 memory.U32BE(owner + 0x108) == owner + 0x1A8,
                 "vs_editor_confirmation_prompt_binding")) return;
      ConfirmationFocus(owner, editor);
    }
  });
}

void BrawlersVsEditorPreviewHint(PPCRegister& widget, PPCRegister& context) {
  Observe([&] {
    if (std::none_of(screens.begin(), screens.end(), [](const auto& entry) {
          return entry.second.active && entry.second.hints_pending;
        })) return;
    const auto model = BoundModel(widget.u32, context.u32);
    if (!model || *model < 0xC4) return;
    const auto found = screens.find(*model - 0xC4);
    if (found == screens.end() || !found->second.active || !found->second.hints_pending) return;
    uint32_t section = 0;
    auto& editor = found->second;
    if (!MainState(found->first, editor, section) || !Model(found->first, *model, 0xC4) ||
        !Guard(memory.U32BE(widget.u32) == 0x82018DAC && (section == 2 || section == 4),
               "vs_editor_preview_hint_widget")) return;
    auto event = Event(editor, Kind::Hints);
    event.section = section;
    if (!Text(0x5E0, event.text)) return;
    output(std::move(event));
    editor.hints_pending = false;
  });
}

void BrawlersVsEditorFooter(PPCRegister& widget, PPCRegister& context, PPCRegister& record) {
  Observe([&] {
    for (auto& [owner, editor] : screens) {
      if (!editor.footer_pending || !editor.opened) continue;
      const auto phase = memory.U32BE(owner + 0x14);
      if (!phase || (!editor.active && !(editor.picker && editor.child_opened && *phase == 7) &&
                     !(editor.confirmation && editor.child_opened && *phase == 11))) continue;
      if (!Owner(owner, editor)) continue;
      const auto common = memory.U32BE(0x829CBC68);
      if (!Guard(common && *common && *common <= UINT32_MAX - 0xE0 &&
                 memory.U32BE(*common) == 0x82003EAC, "vs_editor_footer_owner")) return;
      if (record.u32 != *common + 0x94 || BoundModel(widget.u32, context.u32) != record.u32)
        continue;
      const auto id = memory.U32BE(record.u32 + 0x30);
      if (!Guard(id && (*id == 0x9D6 || *id == 0x9EC), "vs_editor_footer_id")) return;
      auto event = Event(editor, Kind::Footer);
      if (!Text(*id, event.text)) return;
      for (uint32_t offset : {0x38U, 0x40U}) {
        const auto extra = memory.U32BE(record.u32 + offset);
        if (!Guard(extra.has_value(), "vs_editor_footer_extra")) return;
        if (*extra == UINT32_MAX) break;
        std::string text;
        if (!Text(*extra, text)) return;
        event.text += " " + text;
      }
      output(std::move(event));
      editor.footer_pending = false;
    }
  });
}
