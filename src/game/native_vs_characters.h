#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <array>
#include <functional>
#include <string_view>
#include <variant>
#include <vector>

// Captured facts are converted by the character speech bridge before queuing.
struct VsCharactersPlayerSlot {
  uint32_t current_slot = 0, target_slot = 0, character = 0, difficulty = 0;
  uint32_t position = 0, total = 0;
  bool computer = false, resumed = false;
  std::string section_key;
  std::string character_name;
};

struct VsCharactersDifficulty {
  uint32_t slot = 0, character = 0, level = 0;
  std::string character_name;
};

struct VsCharactersTeam {
  uint32_t choice = 0, position = 0, total = 0;
  std::string section;
  std::vector<std::string> hints;
  // Native display order: first two participants oppose the final two.
  std::array<uint32_t, 4> participants{}, identity_frames{};
  // True for the first captured row of each actual team-section entry.
  bool entered = false;
};

struct VsCharactersCapture {
  // UINT32_MAX denotes the single native screen's opened/closed events.
  uint32_t controller = UINT32_MAX;
  uint64_t id = 0;
  uint32_t locale = 0;
  std::variant<ScreenEvent, VsCharactersPlayerSlot, VsCharactersDifficulty,
               VsCharactersTeam> content;
  bool continuation = false;
};

void SetVsCharactersCapture(brawlers::ui::ReadBytes reader,
                           std::function<void(VsCharactersCapture&&)> output,
                           std::function<void(std::string_view)> diagnostic,
                           std::function<void()> failure);
void NotifyVsCharactersTaskRemoved(uint32_t owner) noexcept;
void NotifyVsCharactersSelection(uint32_t selector, uint32_t index) noexcept;
