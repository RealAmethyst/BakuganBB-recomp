// Original synthetic facts exercise speech policy without game or speech services.
#include "native_vs_editor_speech.h"
#include "native_vs_editor.h"

#include <cassert>
#include <utility>
#include <vector>

namespace {
using Kind = VsEditorEventKind;
std::function<void(VsEditorEvent&&)> capture;
std::vector<ScreenEvent> events;
std::vector<std::string> diagnostics;
VsEditorEvent Native(uint64_t id, Kind kind, uint32_t controller = 0) {
  VsEditorEvent event;
  event.id = id;
  event.kind = kind;
  event.controller = controller;
  event.locale = 1;
  return event;
}
void Send(VsEditorEvent event) { capture(std::move(event)); }
void Hints(uint64_t id, uint32_t controller = 0) {
  auto hint = Native(id, Kind::Hints, controller);
  hint.text = "Synthetic local hints";
  Send(hint);
  hint.kind = Kind::Footer;
  hint.text = "Synthetic shared footer";
  Send(hint);
}
VsEditorEvent Focus(uint64_t id, uint32_t row, uint32_t controller = 0) {
  auto event = Native(id, Kind::Focused, controller);
  VsEditorSelection selection;
  selection.row = row;
  selection.enabled_rows = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 8);
  selection.finish = row == 8;
  selection.caption = "Synthetic selected deck";
  event.selection = selection;
  return event;
}
}  // namespace

void SetVsEditorCapture(brawlers::ui::ReadBytes,
                        std::function<void(VsEditorEvent&&)> sink,
                        std::function<void(std::string_view)>, std::function<void()>) {
  capture = std::move(sink);
}

int main() {
  SetVsEditorSpeech({}, [](ScreenEvent&& event) { events.push_back(std::move(event)); },
                    [](std::string_view reason) { diagnostics.emplace_back(reason); }, [] {});
  Send(Focus(100, 1));
  assert(events.empty());
  for (uint32_t controller = 0; controller < 4; ++controller) {
    const uint64_t id = 100 + controller;
    Send(Native(id, Kind::Opened, controller));
    assert(events.back().kind == "opened" && events.back().title.empty());
    assert(events.back().controller == controller);
    const auto opened = events.size();
    Send(Focus(id, 1, controller));
    assert(events.size() == opened);  // A late footer must not lose local hints.
    Hints(id, controller);
    assert(events.back().position == 1 && events.back().total == 4);
    assert(events.back().label == "Synthetic selected deck");
    assert(events.back().hints.size() == 2 && events.back().controller == controller);
    Send(Focus(id, 8, controller));
    assert(events.back().label_key == "vs_editor_finish");
    assert(events.back().position == 4 && events.back().total == 4);
  }
  auto item = Focus(100, 2);
  item.selection->section = 2;
  item.selection->section_caption = "Synthetic actual deck header";
  item.selection->caption = "Synthetic creature";
  item.selection->item_type = 0;
  item.selection->attribute_name = "Synthetic native attribute";
  item.selection->displayed_number = 432;
  item.selection->level = 7;
  Send(item);
  assert(events.back().section == "Synthetic actual deck header");
  assert(events.back().value == "Synthetic native attribute, 432");
  assert(events.back().label_template == "vs_editor_creature_level");
  assert(events.back().label_arguments[0].text == "Synthetic creature");
  assert(events.back().label_arguments[1].key == "vs_editor_level");
  assert(events.back().label_arguments[2].text == "7");
  item.selection->level = 10;
  Send(item);
  assert(events.back().label_arguments[2].key == "vs_editor_max");
  Send(Native(100, Kind::Suspended));
  const auto suspended = events.size();
  auto header = Native(100, Kind::Header);
  header.text = "Stale hidden header";
  Send(header);
  assert(events.size() == suspended);
  item.kind = Kind::Resumed;
  Send(item);
  assert(events.back().kind == "resumed" && events.back().id == 100);
  assert(events.back().title.empty() && events.back().section == "Synthetic actual deck header");

  Send(Native(100, Kind::Suspended));
  auto child = Native(100, Kind::PickerOpened);
  child.child_id = 700;
  child.text = "Synthetic actual picker title";
  Send(child);
  assert(events.back().source == "vs_editor_picker" && events.back().title == child.text);
  child.kind = Kind::PickerFocused;
  child.choices = 6;
  child.choice = 4;
  child.label = "Synthetic localized attribute identity";
  child.text = "Synthetic actual picker description";
  Send(child);
  assert(events.back().position == 5 && events.back().total == 6);
  assert(events.back().label == child.label && events.back().description == child.text);
  assert(events.back().description_key == "vs_attribute_icon_4");
  child.picker_kind = 1;
  child.choice = 2;
  Send(child);
  assert(events.back().label_key == "vs_card_color_icon_2");
  auto footer = child;
  footer.kind = Kind::Footer;
  footer.text = "Synthetic child footer";
  Send(footer);
  assert(events.back().source == "vs_editor_picker" && events.back().id == 700);
  assert(events.back().label.empty() && events.back().hints.size() == 1);
  child.kind = Kind::PickerClosed;
  Send(child);
  assert(events.back().kind == "closed" && events.back().id == 700);

  item.kind = Kind::Resumed;
  item.selection->section = 5;
  item.selection->page = 1;
  item.selection->inventory_count = 8;
  item.selection->item_type = 2;
  item.selection->attribute = 5;
  item.selection->replaceable = false;
  item.selection->enabled_rows = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7);
  item.selection->caption = "Synthetic card";
  Send(item);
  assert(events.back().position == 8 && events.back().total == 8);
  assert(events.back().value_key == "vs_card_color_icon_5");
  assert(events.back().description_key == "vs_dimmed_item");
  const auto valid = events.size();
  item.selection->inventory_count = 7;
  Send(item);
  assert(events.size() == valid && !diagnostics.empty());

  auto locked = Focus(100, 3);
  locked.selection->empty = true;
  locked.selection->caption.clear();
  Send(locked);
  assert(events.size() == valid);
  auto status = Native(100, Kind::Status);
  status.selection = locked.selection;
  status.text = "Synthetic native unlock condition";
  Send(status);
  assert(events.back().label_key == "vs_question_mark");
  assert(events.back().description == status.text && events.back().position == 3);
  status.selection.reset();
  status.section = 4;
  status.text = "Synthetic native empty collection message";
  Send(status);
  assert(events.back().source == "vs_editor_status" && events.back().body == status.text);
  assert(events.back().position == 0 && events.back().total == 0);
  const auto status_id = events.back().id;
  footer = Native(100, Kind::Footer);
  footer.text = "Synthetic empty-list back hint";
  Send(footer);
  assert(events.back().source == "vs_editor_status" && events.back().id == status_id);
  assert(events.back().kind == "focused" && events.back().hints[0] == footer.text);
  const auto duplicate_status = events.size();
  Send(status);
  assert(events.size() == duplicate_status);

  auto empty = Native(100, Kind::EmptyInventory);
  empty.section = 5;
  empty.section_caption = "Synthetic actual empty-list header";
  Send(empty);
  assert(events.back().source == "vs_editor_status");
  assert(events.back().label_key == "vs_empty_inventory");
  assert(events.back().section == empty.section_caption && events.back().position == 0);
  Send(Native(100, Kind::Suspended));
  assert(events.back().source == "vs_editor_status" && events.back().kind == "closed");
  child = Native(100, Kind::ConfirmationOpened);
  child.child_id = 701;
  child.text = "Synthetic native confirmation question";
  Send(child);
  assert(events.back().body == child.text && events.back().title.empty());
  child.kind = Kind::ConfirmationFocused;
  child.choices = 2;
  child.choice = 1;
  Send(child);
  assert(events.back().label_key == "vs_editor_no" && events.back().position == 2);
  child.kind = Kind::ConfirmationStatus;
  child.text = "Synthetic native action result";
  Send(child);
  assert(events.back().source == "vs_editor_status" && events.back().body == child.text);
  child.kind = Kind::ConfirmationClosed;
  Send(child);
  assert(events.back().source == "vs_editor_confirmation" && events.back().kind == "closed");
  Send(Native(100, Kind::Closed));
  assert(events.back().source == "vs_editor" && events.back().kind == "closed");
  const auto closed = events.size();
  Send(Native(100, Kind::Closed));
  Send(Focus(100, 1));
  assert(events.size() == closed);
  for (uint32_t controller = 1; controller < 4; ++controller)
    Send(Native(100 + controller, Kind::Closed, controller));
}
