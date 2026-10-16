#include "native_vs_characters_speech.h"
#include "native_vs_characters.h"

#include <memory>
#include <set>
#include <utility>

namespace {
class Presentation {
 public:
  Presentation(std::function<void(ScreenEvent&&)> output,
               std::function<void(std::string_view)> diagnostic)
      : output_(std::move(output)), diagnostic_(std::move(diagnostic)) {}

  void Capture(VsCharactersCapture&& capture) {
    if (!output_ || !Guard(capture.id && capture.locale >= 1 && capture.locale <= 7 &&
                          (capture.controller < 4 || capture.controller == UINT32_MAX),
                          "vs_characters_speech_metadata")) return;
    std::visit([&](auto&& value) { Present(capture, std::move(value)); }, std::move(capture.content));
  }

 private:
  bool Guard(bool valid, const char* reason) {
    if (!valid && diagnostic_ && reported_.emplace(reason).second) diagnostic_(reason);
    return valid;
  }

  ScreenEvent Event(const VsCharactersCapture& capture, const char* kind) {
    ScreenEvent event;
    event.source = "vs_characters";
    event.id = capture.id;
    event.locale = capture.locale;
    event.controller = capture.controller;
    event.kind = kind;
    return event;
  }

  ScreenTextArgument Identity(uint32_t frame) {
    return {{}, "vs_identity_" + std::to_string(frame)};
  }

  bool Character(uint32_t id, std::string name, ScreenTextArgument& argument) {
    if (!Guard(id <= 17 && (id >= 16 || !name.empty()), "vs_characters_speech_name")) return false;
    if (id < 16) argument.text = std::move(name);
    else argument.key = id == 16 ? "vs_characters_random" : "vs_characters_custom";
    return true;
  }

  void Present(const VsCharactersCapture& capture, ScreenEvent event) {
    if (!Guard(event.source == "vs_characters" && event.id == capture.id &&
               event.locale == capture.locale, "vs_characters_speech_event")) return;
    const bool lifecycle = event.kind == "opened" || event.kind == "closed";
    if (!Guard(lifecycle ? capture.controller == UINT32_MAX : capture.controller < 4,
               "vs_characters_speech_controller")) return;
    event.controller = capture.controller;
    event.continuation = event.kind == "resumed" && capture.continuation;
    if (!lifecycle && !event.label.empty()) event.description_key = "vs_character_portrait_description";
    const bool closed = event.kind == "closed";
    output_(std::move(event));
    if (!lifecycle) focused_.insert(capture.id);
    if (closed) focused_.erase(capture.id);
    if (closed && statuses_.erase(capture.id)) {
      auto status = Event(capture, "closed");
      status.source = "vs_status";
      output_(std::move(status));
    }
  }

  void Present(const VsCharactersCapture& capture, VsCharactersPlayerSlot slot) {
    if (!Guard(capture.controller < 4 && slot.current_slot < 4 && slot.target_slot < 4 &&
               slot.difficulty <= 2 && (!slot.computer || slot.target_slot > 0) &&
               slot.position && slot.position <= slot.total && slot.total <= 23 &&
               !slot.section_key.empty(), "vs_characters_speech_slot")) return;
    ScreenTextArgument character;
    if (!Character(slot.character, std::move(slot.character_name), character)) return;
    auto event = Event(capture, slot.resumed ? "resumed" : "focused");
    event.continuation = slot.resumed && capture.continuation;
    event.section_key = std::move(slot.section_key);
    event.position = slot.position;
    event.total = slot.total;
    event.label_template = "vs_character_slot";
    event.label_arguments = {Identity(slot.target_slot + (slot.computer ? 3 : 0)), std::move(character)};
    event.description_key = slot.computer ? "vs_cpu_slot_description" : "vs_character_slot_description";
    if (slot.computer) event.value_key = "vs_characters_level_" + std::to_string(slot.difficulty);
    output_(std::move(event));
    focused_.insert(capture.id);
  }

  void Present(const VsCharactersCapture& capture, VsCharactersDifficulty difficulty) {
    if (!Guard(capture.controller < 4 && difficulty.slot > 0 && difficulty.slot < 4 &&
               difficulty.level <= 2, "vs_characters_speech_difficulty")) return;
    ScreenTextArgument character;
    if (!Character(difficulty.character, std::move(difficulty.character_name), character)) return;
    auto event = Event(capture, statuses_.contains(capture.id) ? "focused" : "opened");
    event.source = "vs_status";
    event.label_template = "vs_cpu_difficulty";
    event.label_arguments = {Identity(difficulty.slot + 3), std::move(character),
                            {{}, "vs_characters_level_" + std::to_string(difficulty.level)}};
    output_(std::move(event));
    statuses_.insert(capture.id);
  }

  void Present(const VsCharactersCapture& capture, VsCharactersTeam team) {
    if (!Guard(capture.controller == 0 && team.choice < 3 && team.position == team.choice + 1 &&
               team.total == 3 && !team.section.empty() && !team.hints.empty(),
               "vs_characters_speech_team")) return;
    uint32_t seen = 0;
    auto event = Event(capture, team.entered && focused_.contains(capture.id) ? "resumed" : "focused");
    event.section = std::move(team.section);
    event.position = team.position;
    event.total = team.total;
    event.hints = std::move(team.hints);
    event.label_template = "vs_team";
    for (size_t i = 0; i < team.participants.size(); ++i) {
      const auto slot = team.participants[i], frame = team.identity_frames[i];
      if (!Guard(slot < 4 && !(seen & (1U << slot)) &&
                 (frame == slot || (slot > 0 && frame == slot + 3)),
                 "vs_characters_speech_team_identity")) return;
      seen |= 1U << slot;
      event.label_arguments.push_back(Identity(frame));
    }
    output_(std::move(event));
    focused_.insert(capture.id);
  }

  std::function<void(ScreenEvent&&)> output_;
  std::function<void(std::string_view)> diagnostic_;
  std::set<std::string> reported_;
  std::set<uint64_t> statuses_;
  std::set<uint64_t> focused_;
};
}  // namespace

void SetVsCharactersSpeech(brawlers::ui::ReadBytes reader,
                           std::function<void(ScreenEvent&&)> output,
                           std::function<void(std::string_view)> diagnostic,
                           std::function<void()> failure) {
  auto presentation = std::make_shared<Presentation>(std::move(output), diagnostic);
  // Native capture owns synchronization, floating-point preservation and its
  // exception latch, including execution of this presentation callback.
  SetVsCharactersCapture(std::move(reader), [presentation](VsCharactersCapture&& capture) {
    presentation->Capture(std::move(capture));
  }, std::move(diagnostic), std::move(failure));
}
