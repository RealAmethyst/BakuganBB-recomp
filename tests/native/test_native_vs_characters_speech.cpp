// Original presentation fixtures; no native image, game, or speech service.
#include "native_vs_characters_speech.h"
#include "native_vs_characters.h"

#include <cassert>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
brawlers::ui::ReadBytes reader;
std::function<void(VsCharactersCapture&&)> capture;
std::function<void(std::string_view)> report;
std::function<void()> fail;
std::vector<ScreenEvent> events;
std::vector<std::string> reasons;
unsigned reads = 0, failures = 0;
bool throw_output = false;

ScreenEvent Native(const char* kind, uint64_t id = 1) {
  ScreenEvent event;
  event.source = "vs_characters";
  event.kind = kind;
  event.id = id;
  event.locale = 2;
  return event;
}

template <typename Content>
void Send(Content value, uint32_t controller = 2, uint64_t id = 1, bool continuation = false) {
  capture(VsCharactersCapture{controller, id, 2, std::move(value), continuation});
}

VsCharactersPlayerSlot Slot() {
  VsCharactersPlayerSlot slot;
  slot.current_slot = 1;
  slot.target_slot = 3;
  slot.character = 4;
  slot.difficulty = 1;
  slot.position = 21;
  slot.total = 22;
  slot.computer = true;
  slot.section_key = "vs_identity_1";
  slot.character_name = "Synthetic selected character";
  return slot;
}

VsCharactersTeam Team(uint32_t choice = 0) {
  VsCharactersTeam team;
  team.choice = choice;
  team.position = choice + 1;
  team.total = 3;
  team.section = "Synthetic team section";
  team.hints = {"Synthetic team controls"};
  team.participants = {0, 2, 1, 3};
  team.identity_frames = {0, 5, 1, 6};
  team.entered = true;
  return team;
}
}  // namespace

void SetVsCharactersCapture(brawlers::ui::ReadBytes source,
                           std::function<void(VsCharactersCapture&&)> sink,
                           std::function<void(std::string_view)> diagnostic,
                           std::function<void()> failure) {
  reader = std::move(source);
  capture = std::move(sink);
  report = std::move(diagnostic);
  fail = std::move(failure);
}

int main() {
  SetVsCharactersSpeech([](uint32_t, void*, size_t) { ++reads; return false; },
    [](ScreenEvent&& event) {
      if (throw_output) throw std::runtime_error("synthetic output failure");
      events.push_back(std::move(event));
    }, [](std::string_view reason) { reasons.emplace_back(reason); }, [] { ++failures; });
  assert(reader && capture && report && fail);

  auto opened = Native("opened");
  opened.title = "Synthetic title";
  opened.hints = {"Synthetic controls"};
  Send(opened, UINT32_MAX);
  assert(events.size() == 1 && events.back().controller == UINT32_MAX);
  assert(events.back().title == opened.title && events.back().hints == opened.hints);
  for (uint32_t controller = 0; controller < 4; ++controller) {
    auto focused = Native("focused");
    focused.label = "Synthetic portrait name";
    focused.position = controller + 1;
    focused.total = 20;
    focused.selected = controller == 1;
    focused.section_key = "vs_identity_5";
    Send(focused, controller);
    assert(events.back().controller == controller && events.back().section_key == "vs_identity_5");
    assert(events.back().label == focused.label && events.back().selected == focused.selected);
    assert(events.back().position == controller + 1 && events.back().total == 20);
    assert(events.back().description_key == "vs_character_portrait_description");
    assert(events.back().title.empty());
  }

  Send(Slot());
  const auto& slot = events.back();
  assert(slot.kind == "focused" && slot.label_template == "vs_character_slot");
  assert(slot.label.empty() && slot.label_key.empty() && slot.label_arguments.size() == 2);
  assert(slot.label_arguments[0].key == "vs_identity_6");
  assert(slot.label_arguments[1].text == "Synthetic selected character");
  assert(slot.value_key == "vs_characters_level_1" && slot.description_key == "vs_cpu_slot_description");
  assert(slot.controller == 2 && slot.section_key == "vs_identity_1" && slot.position == 21 && slot.total == 22);

  auto human = Slot();
  human.computer = false;
  human.target_slot = 0;
  human.resumed = true;
  human.character = 17;
  human.character_name.clear();
  Send(human, 2, 1, true);
  assert(events.back().kind == "resumed" && events.back().value_key.empty());
  assert(events.back().continuation);
  assert(events.back().description_key == "vs_character_slot_description");
  assert(events.back().label_arguments[0].key == "vs_identity_0");
  assert(events.back().label_arguments[1].key == "vs_characters_custom");
  human.character = 16;
  Send(human);
  assert(events.back().label_arguments[1].key == "vs_characters_random");
  auto returning = Native("resumed");
  returning.label = "Synthetic return";
  returning.position = 1;
  returning.total = 3;
  Send(returning, 3, 1, true);
  assert(events.back().continuation && events.back().controller == 3);

  Send(VsCharactersDifficulty{3, 2, 2, "Synthetic CPU character"}, 0);
  assert(events.back().source == "vs_status" && events.back().kind == "opened");
  assert(events.back().controller == 0 && events.back().label_template == "vs_cpu_difficulty");
  assert(events.back().label_arguments[0].key == "vs_identity_6");
  assert(events.back().label_arguments[1].text == "Synthetic CPU character");
  assert(events.back().label_arguments[2].key == "vs_characters_level_2");
  assert(events.back().position == 0 && events.back().total == 0 && events.back().section_key.empty());
  Send(VsCharactersDifficulty{2, 16, 0, {}}, 3);
  assert(events.back().kind == "focused" && events.back().controller == 3);
  assert(events.back().label_arguments[0].key == "vs_identity_5");
  assert(events.back().label_arguments[1].key == "vs_characters_random");

  Send(Team(), 0);
  assert(events.back().kind == "resumed" && events.back().section == "Synthetic team section");
  assert(events.back().label_template == "vs_team" && events.back().label_arguments.size() == 4);
  assert(events.back().label_arguments[0].key == "vs_identity_0");
  assert(events.back().label_arguments[1].key == "vs_identity_5");
  assert(events.back().label_arguments[2].key == "vs_identity_1");
  assert(events.back().label_arguments[3].key == "vs_identity_6");
  assert(events.back().position == 1 && events.back().total == 3 && events.back().title.empty());
  assert(!events.back().continuation);
  auto team = Team(2);
  team.entered = false;
  Send(team, 0);
  assert(events.back().kind == "focused" && events.back().position == 3);

  // A title first captured in the team section must retain initial queue intent.
  Send(Native("opened", 2), UINT32_MAX, 2);
  Send(Team(), 0, 2);
  assert(events.back().kind == "focused");
  Send(Team(), 0, 2);
  assert(events.back().kind == "resumed");

  auto count = events.size();
  Send(Native("closed"), UINT32_MAX);
  assert(events.size() == count + 2 && events.back().source == "vs_status" && events.back().kind == "closed");
  assert(events.back().controller == UINT32_MAX);
  count = events.size();
  Send(Native("closed", 2), UINT32_MAX, 2);
  assert(events.size() == count + 1);  // No status lifetime was opened for this id.

  // Invalid facts never become plausible fallback wording or malformed rows.
  count = events.size();
  auto missing_name = Slot();
  missing_name.character_name.clear();
  Send(missing_name);
  auto bad_slot = Slot();
  bad_slot.target_slot = 0;
  Send(bad_slot);
  team = Team();
  team.identity_frames[0] = 3;
  Send(team, 0);
  team = Team();
  team.participants[3] = 2;
  Send(team, 0);
  Send(Team(), 1);
  Send(VsCharactersDifficulty{0, 0, 0, "Synthetic invalid CPU"});
  auto wrong_source = Native("focused");
  wrong_source.source = "other";
  Send(wrong_source);
  assert(events.size() == count && !reasons.empty());
  assert(reads == 0);  // Presentation uses copied facts, never guest memory.

  // Sink exceptions propagate to the existing native observer's failure latch.
  throw_output = true;
  bool propagated = false;
  try { Send(Slot()); } catch (const std::runtime_error&) { propagated = true; }
  assert(propagated);
  fail();
  assert(failures == 1);
}
