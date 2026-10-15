#include "settings_catalog.h"

#include <cassert>
#include <string>

using namespace brawlers::settings;

namespace {

Catalog Fixture() {
  Catalog catalog;
  catalog.title = "Synthetic settings";
  catalog.native_labels = {"Synthetic practice", "Synthetic vibration", "Synthetic music",
                           "Synthetic effects", "Synthetic voice"};
  catalog.native_values = {0, 1, 1, 1, 5, 10};
  for (const char* key : {"hints", "restore_defaults", "restore_question", "yes", "no",
      "off", "on", "display", "audio", "controls", "accessibility", "advanced",
      "fullscreen", "window_size", "window_size_description", "vsync", "vsync_description",
      "render_scale", "render_scale_description", "accessibility_audio", "friendly_enabled",
      "friendly_volume", "enemy_enabled", "enemy_volume"})
    catalog.text[key] = std::string("Synthetic ") + key;
  catalog.host_values = {{Fullscreen, 0}, {WindowSize, 720}, {VSync, 1}, {RenderScale, 1},
      {FriendlyEnabled, 1}, {FriendlyVolume, 100}, {EnemyEnabled, 1}, {EnemyVolume, 100}};
  catalog.host_defaults = catalog.host_values;
  catalog.window_sizes = {{720, "Synthetic small window"}, {1080, "Synthetic large window"}};
  catalog.render_scales = {{1, "Synthetic native scale"}, {2, "Synthetic double scale"}};
  const std::array<int, 10> percentages{0, 11, 22, 33, 44, 56, 67, 78, 89, 100};
  for (size_t i = 0; i < percentages.size(); ++i)
    catalog.native_volumes.push_back({static_cast<int32_t>(i + 1), std::to_string(percentages[i]) + "%"});
  for (int32_t value = 0; value <= 100; value += 10)
    catalog.cue_volumes.push_back({value, std::to_string(value) + "%"});
  return catalog;
}

const Row& Find(const Definition& definition, Id id) {
  for (const auto& page : definition.pages)
    for (const auto& row : page.rows)
      if (row.id == id) return row;
  assert(false);
  return definition.pages[0].rows[0];
}

void InventoryAndResetScopes() {
  const auto catalog = Fixture();
  const auto definition = BuildDefinition(catalog);
  assert(definition && definition->root == 1 && definition->pages.size() == 7);
  const auto& root = definition->pages[0];
  assert(root.label.empty() && root.rows.size() == 5);
  const std::array<const char*, 5> categories{"display", "audio", "controls", "accessibility", "advanced"};
  for (size_t i = 0; i < categories.size(); ++i) {
    assert(root.rows[i].kind == RowKind::Page && root.rows[i].page == i + 2);
    assert(root.rows[i].label == catalog.text.at(categories[i]));
  }
  const std::array<std::vector<Id>, 6> resets{{
      {Fullscreen, WindowSize, VSync}, {Music, Sfx, Voice}, {ThrowingPractice, Vibration},
      {FriendlyEnabled, FriendlyVolume, EnemyEnabled, EnemyVolume}, {RenderScale},
      {FriendlyEnabled, FriendlyVolume, EnemyEnabled, EnemyVolume}}};
  for (size_t i = 0; i < resets.size(); ++i) {
    const auto& row = Find(*definition, 401 + i);
    assert(row.kind == RowKind::Reset && row.reset_rows == resets[i]);
    assert(row.confirmation == catalog.text.at("restore_question"));
    assert(row.yes == catalog.text.at("yes") && row.no == catalog.text.at("no"));
  }
  for (Id id : {Music, Sfx, Voice}) {
    const auto& row = Find(*definition, id);
    assert(row.kind == RowKind::Range && row.minimum == 1 && row.maximum == 10 && row.step == 1);
    assert(row.default_value == 10 && row.values.size() == 10);
    assert(row.values.front().value == 1 && row.values.front().label == "0%");
    assert(row.values.back().value == 10 && row.values.back().label == "100%");
    for (size_t i = 0; i < row.values.size(); ++i)
      assert(row.values[i].label == catalog.native_volumes[i].label);
  }
  for (Id id : {FriendlyVolume, EnemyVolume}) {
    const auto& row = Find(*definition, id);
    assert(row.minimum == 0 && row.maximum == 100 && row.step == 10 && row.default_value == 100);
  }
  assert(Find(*definition, FriendlyEnabled).default_value == 1);
  assert(Find(*definition, EnemyEnabled).default_value == 1);
  assert(Find(*definition, RenderScale).description == catalog.text.at("render_scale_description"));
  Menu menu;
  assert(menu.Configure(*definition));
  menu.Open();
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  assert(menu.Read().rows[0].value == "0%" && menu.Read().rows[2].value == "100%");
  const auto command = menu.Handle(Input::Right);
  assert(command.command->changes[0].before == 1 && command.command->changes[0].after == 2);
}

void ExternalCustomChoices() {
  auto catalog = Fixture();
  const auto definition = BuildDefinition(catalog);
  Menu menu;
  assert(definition && menu.Configure(*definition));
  menu.Open();
  menu.Handle(Input::Activate);
  menu.Handle(Input::Down);
  catalog.window_sizes.push_back({913, "Synthetic custom window"});
  catalog.render_scales.push_back({23, "Synthetic asymmetric scale"});
  catalog.host_values[WindowSize] = 913;
  catalog.host_values[RenderScale] = 23;
  auto refreshed = BuildDefinition(catalog);
  assert(refreshed);
  const std::vector<ChoiceUpdate> choices{
      {WindowSize, Find(*refreshed, WindowSize).values},
      {RenderScale, Find(*refreshed, RenderScale).values}};
  const std::vector<std::pair<Id, int32_t>> values{{WindowSize, 913}, {RenderScale, 23}};
  assert(menu.SynchronizeValues(values) == Status::Invalid);
  assert(menu.SynchronizeValues(values, choices) == Status::Ok);
  assert(menu.Read().page == 2 && menu.Read().position == 2);
  assert(menu.Read().rows[1].numeric_value == 913 && menu.Read().rows[1].value == "Synthetic custom window");
  const auto command = menu.Handle(Input::Left);
  assert(command.command->changes[0].before == 913 && command.command->changes[0].after == 1080);
  assert(menu.SynchronizeValues(values, choices) == Status::Busy);
  assert(menu.Resolve(command.command->generation, false).status == Status::Rejected);
  for (const auto& invalid : std::vector<std::vector<ChoiceUpdate>>{
      {{WindowSize, {{913, "Synthetic current only"}}}},
      {{WindowSize, {{720, "Synthetic default only"}}}},
      {{WindowSize, {{720, "Synthetic default"}, {913, ""}}}},
      {{WindowSize, {{720, "Synthetic default"}, {913, "Synthetic current"}, {913, "Duplicate"}}}},
      {{Fullscreen, {{0, "Synthetic off"}, {1, "Synthetic on"}}}},
      {{999, {{0, "Synthetic unknown"}}}}, {choices[0], choices[0]}}) {
    assert(menu.SynchronizeValues({{Fullscreen, 1}}, invalid) == Status::Invalid);
    assert(menu.Read().rows[0].numeric_value == 0 && menu.Read().rows[1].numeric_value == 913);
    assert(menu.Read().rows[1].value == "Synthetic custom window");
  }
  // Replacing the current choice and its value together need not retain the old choice.
  assert(menu.SynchronizeValues({{WindowSize, 777}},
      {{WindowSize, {{720, "Synthetic default"}, {777, "Synthetic replacement"}}}}) == Status::Ok);
  menu.Handle(Input::Down);
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  assert(menu.Read().confirming && !menu.Read().confirm_yes);
  menu.Handle(Input::Left);
  const auto reset = menu.Handle(Input::Activate);
  assert(reset.command->changes.size() == 1);
  assert(reset.command->changes[0].row == WindowSize && reset.command->changes[0].after == 720);
  menu.Resolve(reset.command->generation, true);
  menu.Handle(Input::Back);
  for (int i = 0; i < 4; ++i) menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  assert(menu.Read().rows[0].numeric_value == 23 && menu.Read().rows[0].value == "Synthetic asymmetric scale");
}

void InvalidCatalogs() {
  auto catalog = Fixture();
  catalog.text.erase("window_size");
  assert(!BuildDefinition(catalog));
  catalog = Fixture(); catalog.native_values[3] = 0;
  assert(!BuildDefinition(catalog));
  catalog = Fixture(); catalog.host_defaults[WindowSize] = 999;
  assert(!BuildDefinition(catalog));
  catalog = Fixture(); catalog.host_values[FriendlyVolume] = 35;
  assert(!BuildDefinition(catalog));
}
}

int main() {
  InventoryAndResetScopes();
  ExternalCustomChoices();
  InvalidCatalogs();
}
