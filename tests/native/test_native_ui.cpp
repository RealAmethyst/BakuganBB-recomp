// Original serialization fixtures. No runtime, game or screen reader is loaded.
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <rex/ui/keyboard_dialog.h>
#include <rex/ui/message_box_dialog.h>

#include "native_speech.h"

namespace rex::ui {
KeyboardDialogObserver observer;
void SetKeyboardDialogObserver(KeyboardDialogObserver value) { observer = std::move(value); }
MessageBoxDialogObserver message_observer;
void SetMessageBoxDialogObserver(MessageBoxDialogObserver value) {
  message_observer = std::move(value);
}
}  // namespace rex::ui

void InstallDialogSpeech();

int main() {
  auto* output = std::tmpfile();
  assert(output);
  assert(dup2(fileno(output), STDERR_FILENO) >= 0);
  InstallDialogSpeech();
  assert(rex::ui::observer);
  rex::ui::KeyboardDialogEvent event;
  event.kind = rex::ui::KeyboardDialogEvent::Kind::Opened;
  event.id = 17;
  event.title = "Synthetic \"prompt\"\nsecond line";
  event.description = "Literal backslash \\ and tab\t";
  event.text = "é";
  event.cursor = event.selection_start = event.selection_end = 2;
  event.capacity = 11;
  rex::ui::observer(event);
  event.kind = rex::ui::KeyboardDialogEvent::Kind::Focused;
  event.label = "OK";
  event.position = 1;
  event.total = 2;
  rex::ui::observer(event);
  event.kind = rex::ui::KeyboardDialogEvent::Kind::Closed;
  event.cancelled = true;
  rex::ui::observer(event);
  event.title = "\xff";  // Invalid UTF-8 fails without leaking text or throwing.
  rex::ui::observer(event);
  std::fflush(stderr);
  std::rewind(output);
  char line[4096];
  const char* kinds[] = {"opened", "focused", "closed"};
  for (int i = 0; i < 3; ++i) {
    assert(std::fgets(line, sizeof(line), output));
    std::string value(line);
    assert(value.starts_with("BRAWLERS_UI:"));
    const auto json = nlohmann::json::parse(value.substr(12));
    assert(json.at("kind") == kinds[i] && json.at("id") == 17);
    assert(json.at("title") == "Synthetic \"prompt\"\nsecond line");
    assert(json.at("description") == "Literal backslash \\ and tab\t");
    assert(json.at("text") == "é" && json.at("cursor") == 2);
    assert(json.at("capacity") == 11 && json.at("total") == (i ? 2 : 0));
    assert(json.at("cancelled") == (i == 2));
  }
  assert(std::fgets(line, sizeof(line), output));
  assert(std::string(line) == "BRAWLERS_UI:serialization failed\n");
  assert(!std::fgets(line, sizeof(line), output));
  std::clearerr(output);
  assert(rex::ui::message_observer);
  rex::ui::MessageBoxDialogEvent popup;
  popup.kind = rex::ui::MessageBoxDialogEvent::Kind::Opened;
  popup.id = 42;
  popup.title = "Synthetic question";
  popup.description = "Choose a synthetic option.";
  popup.total = 2;
  rex::ui::message_observer(popup);
  popup.kind = rex::ui::MessageBoxDialogEvent::Kind::Focused;
  popup.label = "Second option";
  popup.position = 2;
  rex::ui::message_observer(popup);
  popup.kind = rex::ui::MessageBoxDialogEvent::Kind::Closed;
  rex::ui::message_observer(popup);
  std::fflush(stderr);
  // stdio writes through stderr share the file descriptor's offset; reopen for reading.
  std::rewind(output);
  for (int i = 0; i < 4; ++i)
    assert(std::fgets(line, sizeof(line), output));
  for (int i = 0; i < 3; ++i) {
    assert(std::fgets(line, sizeof(line), output));
    std::string value(line);
    assert(value.starts_with("BRAWLERS_SCREEN:"));
    const auto json = nlohmann::json::parse(value.substr(16));
    assert(json.at("source") == "message" && json.at("id") == 42);
    assert(json.at("kind") == kinds[i]);
    assert(json.at("body") == (i == 0 ? popup.description : ""));
    assert(json.at("label") == (i == 1 ? popup.label : ""));
    assert(json.at("value") == "");
    assert(json.at("position") == (i == 1 ? 2 : 0));
    assert(json.at("total") == (i == 1 ? 2 : 0));
  }
  assert(!std::fgets(line, sizeof(line), output));
  std::clearerr(output);
  ScreenEvent creator;
  creator.source = "creator";
  creator.kind = "focused";
  creator.id = 43;
  creator.label = "Synthetic setting";
  creator.value = "Value \"é\"\nsecond line";
  creator.description = "Synthetic description";
  creator.description_key = "synthetic_shape";
  creator.section_key = "synthetic_section";
  creator.value_key = "synthetic_value";
  creator.selected = true;
  creator.position = 1;
  creator.total = 2;
  assert(SendScreenEvent(creator));
  ScreenEvent status;
  status.source = "status";
  status.kind = "opened";
  status.id = 44;
  status.body = "Synthetic status body";
  assert(SendScreenEvent(status));
  creator.value = "\xff";
  assert(!SendScreenEvent(creator));
  std::fflush(stderr);
  std::rewind(output);
  for (int i = 0; i < 7; ++i)
    assert(std::fgets(line, sizeof(line), output));
  assert(std::fgets(line, sizeof(line), output));
  auto json = nlohmann::json::parse(std::string(line).substr(16));
  assert(json.at("source") == "creator" && json.at("id") == 43);
  assert(json.at("value") == "Value \"é\"\nsecond line");
  assert(json.at("label") == "Synthetic setting");
  assert(json.at("description") == "Synthetic description");
  assert(json.at("description_key") == "synthetic_shape");
  assert(json.at("section_key") == "synthetic_section");
  assert(json.at("value_key") == "synthetic_value");
  assert(json.at("selected") == true);
  assert(json.at("position") == 1 && json.at("total") == 2);
  assert(std::fgets(line, sizeof(line), output));
  json = nlohmann::json::parse(std::string(line).substr(16));
  assert(json.at("source") == "status" && json.at("id") == 44);
  assert(json.at("body") == "Synthetic status body");
  assert(json.at("selected") == false);
  assert(json.at("value") == "" && json.at("position") == 0 && json.at("total") == 0);
  assert(std::fgets(line, sizeof(line), output));
  assert(std::string(line) == "BRAWLERS_SCREEN:serialization failed\n");
  assert(!std::fgets(line, sizeof(line), output));
  std::clearerr(output);
  const auto final_offset = std::ftell(output);
  ScreenEvent team;
  team.source = "vs_characters";
  team.kind = "resumed";
  team.id = 45;
  team.controller = 2;
  team.continuation = true;
  team.label_template = "synthetic_pair";
  team.label_arguments = {{"Synthetic name", ""}, {"", "synthetic_identity"}};
  team.position = 2;
  team.total = 3;
  assert(SendScreenEvent(team));
  std::fflush(stderr);
  assert(std::fseek(output, final_offset, SEEK_SET) == 0);
  assert(std::fgets(line, sizeof(line), output));
  json = nlohmann::json::parse(std::string(line).substr(16));
  assert(json.at("controller") == 2 && json.at("label_template") == "synthetic_pair");
  assert(json.at("continuation") == true && json.at("kind") == "resumed");
  assert(json.at("label_arguments").size() == 2);
  assert(json.at("label_arguments")[0].at("text") == "Synthetic name");
  assert(json.at("label_arguments")[1].at("key") == "synthetic_identity");
  std::fclose(output);
  std::puts("Native UI JSON round trip, lifecycle fields and invalid UTF-8 checks passed.");
}
