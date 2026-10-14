// Original serialization fixtures. No runtime, game or screen reader is loaded.
#include <cassert>
#include <cstdio>
#include <string>
#include <utility>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <rex/ui/keyboard_dialog.h>
#include <rex/ui/message_box_dialog.h>

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
    assert(json.at("position") == (i == 1 ? 2 : 0));
    assert(json.at("total") == (i == 1 ? 2 : 0));
  }
  assert(!std::fgets(line, sizeof(line), output));
  std::fclose(output);
  std::puts("Native UI JSON round trip, lifecycle fields and invalid UTF-8 checks passed.");
}
