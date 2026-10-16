#include "native_vs_editor_speech.h"
#include "native_vs_editor.h"

#include <bit>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace {
using Kind = VsEditorEventKind;
std::function<void(ScreenEvent&&)> output;
std::function<void(std::string_view)> diagnostic;
std::set<std::string> reported;
uint64_t next_status = 0;

struct Editor {
  uint32_t controller = UINT32_MAX, locale = 0;
  bool first = true, suspended = false;
  std::optional<VsEditorEvent> pending;
  std::optional<VsEditorSelection> current;
  std::map<uint32_t, std::string> hints;
  std::string footer, status_text, status_section;
  uint64_t child = 0, status = 0;
  bool picker = false;
  std::optional<VsEditorEvent> child_focus;
  std::optional<VsEditorEvent> native_status;
};
std::map<uint64_t, Editor> editors;

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

ScreenEvent Event(uint64_t id, const Editor& editor, const char* source, const char* kind) {
  ScreenEvent event;
  event.id = id;
  event.source = source;
  event.kind = kind;
  event.controller = editor.controller;
  event.locale = editor.locale;
  return event;
}

void CloseStatus(Editor& editor) {
  if (!editor.status) return;
  output(Event(editor.status, editor, "vs_editor_status", "closed"));
  editor.status = 0;
  editor.status_text.clear();
  editor.status_section.clear();
}

void Status(Editor& editor, const std::string& text, const std::string& section = {},
            const std::string& key = {}) {
  const auto identity = text + key;
  if (identity.empty() || (editor.status && editor.status_text == identity &&
                           editor.status_section == section)) return;
  CloseStatus(editor);
  editor.status = ++next_status;
  auto event = Event(editor.status, editor, "vs_editor_status", "opened");
  event.body = text;
  event.label_key = key;
  event.section = section;
  editor.status_text = identity;
  editor.status_section = section;
  if (!editor.footer.empty()) event.hints.push_back(editor.footer);
  output(std::move(event));
}

void CloseChild(Editor& editor) {
  if (!editor.child) return;
  CloseStatus(editor);
  output(Event(editor.child, editor,
               editor.picker ? "vs_editor_picker" : "vs_editor_confirmation", "closed"));
  editor.child = 0;
  editor.child_focus.reset();
  editor.footer.clear();
}

void EmitFocus(uint64_t id, Editor& editor) {
  if (!editor.pending || editor.suspended || editor.child) return;
  const auto& native = *editor.pending;
  if (!Guard(native.selection.has_value(), "vs_editor_speech_selection")) return;
  const auto& item = *native.selection;
  if (!Guard(item.row >= 1 && item.row <= 8 && item.row != 7 &&
             (item.enabled_rows & (1U << item.row)), "vs_editor_speech_row")) return;
  // The initial deck-list hint is a separate native widget from the common
  // footer. Wait for both so neither is lost after the first hint announcement.
  if (editor.first && (editor.footer.empty() ||
                      (item.section == 0 && !editor.hints.contains(0)))) return;
  const bool locked = item.section == 0 && item.empty && !item.finish;
  if (locked && (!editor.native_status || !editor.native_status->selection ||
                 editor.native_status->selection->row != item.row)) return;

  auto event = Event(id, editor, "vs_editor",
                     native.kind == Kind::Resumed ? "resumed" : "focused");
  event.section = item.section_caption;
  if (item.section >= 4) {
    const uint32_t position = item.page * 6 + item.row;
    if (!Guard(!item.finish && position <= item.inventory_count && item.inventory_count,
               "vs_editor_speech_inventory_position")) return;
    event.position = position;
    event.total = item.inventory_count;
  } else {
    const auto content = item.enabled_rows & 0x17EU;
    event.position = std::popcount(content & ((1U << (item.row + 1)) - 1));
    event.total = std::popcount(content);
  }
  if (item.finish) event.label_key = "vs_editor_finish";
  else if (locked) {
    event.label_key = "vs_question_mark";
    event.description = editor.native_status->text;
  } else if (item.empty && item.caption.empty()) event.label_key = "vs_empty_slot";
  else {
    if (!Guard(!item.caption.empty(), "vs_editor_speech_caption")) return;
    event.label = item.caption;
    if (item.item_type == 0) {
      if (item.level) {
        if (!Guard(*item.level >= 1 && *item.level <= 10, "vs_editor_speech_level")) return;
        event.label.clear();
        event.label_template = "vs_editor_creature_level";
        event.label_arguments = {{item.caption, ""}, {"", "vs_editor_level"},
                                 *item.level == 10 ? ScreenTextArgument{"", "vs_editor_max"} :
                                                    ScreenTextArgument{std::to_string(*item.level), ""}};
      }
      event.value = item.attribute_name;
      if (item.displayed_number) {
        if (!event.value.empty()) event.value += ", ";
        event.value += std::to_string(*item.displayed_number);
      }
    }
    else if (item.attribute) {
      if (!Guard(*item.attribute < 6, "vs_editor_speech_card_color")) return;
      event.value_key = "vs_card_color_icon_" + std::to_string(*item.attribute);
    }
    if (!item.replaceable) event.description_key = "vs_dimmed_item";
  }
  if (!editor.footer.empty()) event.hints.push_back(editor.footer);
  const auto hint = editor.hints.find(item.section);
  if (hint != editor.hints.end()) event.hints.push_back(hint->second);
  CloseStatus(editor);
  output(std::move(event));
  editor.current = item;
  editor.pending.reset();
  editor.first = false;
}

void EmitChildFocus(Editor& editor) {
  if (!editor.child || !editor.child_focus) return;
  const auto& native = *editor.child_focus;
  if (!Guard(native.choice < native.choices &&
             native.choices == (editor.picker ? 6U : 2U), "vs_editor_speech_child_choice")) return;
  auto event = Event(editor.child, editor,
                     editor.picker ? "vs_editor_picker" : "vs_editor_confirmation", "focused");
  event.position = native.choice + 1;
  event.total = native.choices;
  if (editor.picker) {
    if (!Guard(native.picker_kind <= 1, "vs_editor_speech_picker_kind")) return;
    if (native.picker_kind == 0) {
      if (!Guard(!native.label.empty(), "vs_editor_speech_attribute_name")) return;
      event.label = native.label;
      event.description_key = "vs_attribute_icon_" + std::to_string(native.choice);
    } else event.label_key = "vs_card_color_icon_" + std::to_string(native.choice);
    event.description = native.text;
  } else event.label_key = native.choice == 0 ? "vs_editor_yes" : "vs_editor_no";
  if (!editor.footer.empty()) event.hints.push_back(editor.footer);
  output(std::move(event));
  editor.child_focus.reset();
}

void Capture(VsEditorEvent&& native) {
  if (!output || !Guard(native.id && native.controller < 4 && native.locale >= 1 && native.locale <= 7,
                       "vs_editor_speech_identity")) return;
  if (native.kind == Kind::Opened) {
    if (!Guard(!editors.contains(native.id), "vs_editor_speech_duplicate_open")) return;
    Editor editor;
    editor.controller = native.controller;
    editor.locale = native.locale;
    editors.emplace(native.id, editor);
    // The deck list has no displayed screen title. Its selected caption and
    // subsection's real deck-name header arrive through their own consumers.
    output(Event(native.id, editor, "vs_editor", "opened"));
    return;
  }
  const auto found = editors.find(native.id);
  if (found == editors.end()) return;
  auto& editor = found->second;
  if (!Guard(editor.controller == native.controller && editor.locale == native.locale,
             "vs_editor_speech_lifetime_metadata")) return;
  switch (native.kind) {
    case Kind::Closed:
      CloseChild(editor);
      CloseStatus(editor);
      output(Event(native.id, editor, "vs_editor", "closed"));
      editors.erase(found);
      return;
    case Kind::Suspended:
      editor.suspended = true;
      editor.pending.reset();
      editor.native_status.reset();
      editor.footer.clear();
      CloseStatus(editor);
      return;
    case Kind::Focused:
    case Kind::Resumed:
      editor.suspended = false;
      editor.pending = std::move(native);
      EmitFocus(found->first, editor);
      return;
    case Kind::Header:
      // Snapshot captures this same verified participant+A8 consumer as the
      // section caption, so a later header draw cannot reorder the first item.
      return;
    case Kind::Hints:
      editor.hints[native.section] = std::move(native.text);
      EmitFocus(native.id, editor);
      return;
    case Kind::Footer: {
      editor.footer = std::move(native.text);
      if (editor.status) {
        auto event = Event(editor.status, editor, "vs_editor_status", "focused");
        event.hints.push_back(editor.footer);
        output(std::move(event));
      } else if (editor.child) {
        auto event = Event(editor.child, editor,
                           editor.picker ? "vs_editor_picker" : "vs_editor_confirmation", "focused");
        event.hints.push_back(editor.footer);
        output(std::move(event));
      } else EmitFocus(native.id, editor);
      return;
    }
    case Kind::Status:
      editor.native_status = native;
      if (native.section == 0) EmitFocus(native.id, editor);
      else Status(editor, native.text, native.section_caption);
      return;
    case Kind::EmptyInventory:
      Status(editor, {}, native.section_caption, "vs_empty_inventory");
      return;
    case Kind::PickerOpened:
    case Kind::ConfirmationOpened: {
      if (!Guard(native.child_id && !native.text.empty() && !editor.child,
                 "vs_editor_speech_child_open")) return;
      editor.child = native.child_id;
      editor.picker = native.kind == Kind::PickerOpened;
      editor.footer.clear();
      auto event = Event(editor.child, editor,
                         editor.picker ? "vs_editor_picker" : "vs_editor_confirmation", "opened");
      if (editor.picker) event.title = std::move(native.text);
      else event.body = std::move(native.text);
      output(std::move(event));
      return;
    }
    case Kind::PickerFocused:
    case Kind::ConfirmationFocused:
      if (!Guard(editor.child == native.child_id && editor.child &&
                 editor.picker == (native.kind == Kind::PickerFocused),
                 "vs_editor_speech_child_binding")) return;
      editor.child_focus = std::move(native);
      EmitChildFocus(editor);
      return;
    case Kind::PickerClosed:
    case Kind::ConfirmationClosed:
      if (editor.child == native.child_id) CloseChild(editor);
      return;
    case Kind::ConfirmationStatus:
      if (editor.child == native.child_id && !editor.picker) Status(editor, native.text);
      return;
    case Kind::Opened: return;
  }
}
}  // namespace

void SetVsEditorSpeech(brawlers::ui::ReadBytes reader,
                       std::function<void(ScreenEvent&&)> sink,
                       std::function<void(std::string_view)> report,
                       std::function<void()> failure) {
  output = std::move(sink);
  diagnostic = report;
  SetVsEditorCapture(std::move(reader), Capture, std::move(report), std::move(failure));
}
