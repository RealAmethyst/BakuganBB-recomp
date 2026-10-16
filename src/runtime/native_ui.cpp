// Transport UI-thread events to the launcher's existing Prism owner.
#include <cstdio>
#include <exception>

#include <nlohmann/json.hpp>
#include <rex/ui/keyboard_dialog.h>
#include <rex/ui/message_box_dialog.h>

#include "native_speech.h"

bool SendScreenEvent(const ScreenEvent& event) noexcept {
  try {
    nlohmann::json arguments = nlohmann::json::array();
    for (const auto& argument : event.label_arguments)
      arguments.push_back({{"text", argument.text}, {"key", argument.key}});
    const nlohmann::json message = {
        {"source", event.source}, {"kind", event.kind}, {"id", event.id},
        {"title", event.title}, {"body", event.body}, {"section", event.section},
        {"label", event.label}, {"value", event.value},
        {"description", event.description}, {"description_key", event.description_key},
        {"hints", event.hints},
        {"title_key", event.title_key}, {"section_key", event.section_key},
        {"label_key", event.label_key}, {"value_key", event.value_key}, {"locale", event.locale},
        {"label_template", event.label_template}, {"label_arguments", arguments},
        {"controller", event.controller},
        {"continuation", event.continuation},
        {"position", event.position}, {"total", event.total}, {"selected", event.selected}};
    const auto encoded = message.dump();
    return std::fprintf(stderr, "BRAWLERS_SCREEN:%s\n", encoded.c_str()) >= 0;
  } catch (...) {
    std::fprintf(stderr, "BRAWLERS_SCREEN:serialization failed\n");
    return false;
  }
}

void InstallDialogSpeech() {
  rex::ui::SetMessageBoxDialogObserver([](const rex::ui::MessageBoxDialogEvent& event) {
    using Kind = rex::ui::MessageBoxDialogEvent::Kind;
    ScreenEvent message;
    message.source = "message";
    message.id = event.id;
    switch (event.kind) {
      case Kind::Opened:
        message.kind = "opened";
        message.title = event.title;
        message.body = event.description;
        break;
      case Kind::Focused:
        message.kind = "focused";
        message.label = event.label;
        message.position = event.position;
        message.total = event.total;
        break;
      case Kind::Closed:
        message.kind = "closed";
        break;
    }
    if (!message.kind.empty())
      SendScreenEvent(message);
  });
  rex::ui::SetKeyboardDialogObserver([](const rex::ui::KeyboardDialogEvent& event) {
    using Kind = rex::ui::KeyboardDialogEvent::Kind;
    const char* kind = nullptr;
    switch (event.kind) {
      case Kind::Opened: kind = "opened"; break;
      case Kind::Focused: kind = "focused"; break;
      case Kind::Edited: kind = "edited"; break;
      case Kind::Closed: kind = "closed"; break;
    }
    if (!kind)
      return;
    try {
      const nlohmann::json message = {
          {"kind", kind}, {"id", event.id}, {"title", event.title},
          {"description", event.description}, {"text", event.text}, {"label", event.label},
          {"cursor", event.cursor}, {"selection_start", event.selection_start},
          {"selection_end", event.selection_end}, {"cancelled", event.cancelled},
          {"capacity", event.capacity}, {"position", event.position}, {"total", event.total}};
      const auto encoded = message.dump();
      // One write prevents other runtime threads from splitting the event line.
      std::fprintf(stderr, "BRAWLERS_UI:%s\n", encoded.c_str());
    } catch (const std::exception&) {
      // No caption or entered text in failure diagnostics.
      std::fprintf(stderr, "BRAWLERS_UI:serialization failed\n");
    }
  });
}
