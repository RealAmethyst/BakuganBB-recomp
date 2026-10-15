#pragma once

#include "settings_menu.h"

#include <array>
#include <map>

namespace brawlers::settings {

enum Setting : Id {
  ThrowingPractice = 101, Vibration, Music, Sfx, Voice,
  Fullscreen = 201, WindowSize, VSync, RenderScale,
  FriendlyEnabled = 301, FriendlyVolume, EnemyEnabled, EnemyVolume
};

// All captions originate in the actual game text consumer or central project
// resources. Choice labels and native audio percentages are supplied, not guessed
// by the menu model. Host values include defaults from their registered owners.
struct Catalog {
  std::string title;
  std::array<std::string, 5> native_labels;
  std::array<uint8_t, 6> native_values{};
  std::map<std::string, std::string> text;
  std::map<Id, int32_t> host_values;
  std::map<Id, int32_t> host_defaults;
  std::vector<Value> window_sizes;
  std::vector<Value> render_scales;
  std::vector<Value> native_volumes;
  std::vector<Value> cue_volumes;
};

// Missing captions or invalid values reject the entire menu before replacement.
std::optional<Definition> BuildDefinition(const Catalog& catalog);

}  // namespace brawlers::settings
