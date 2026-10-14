#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ScreenEvent {
  std::string source;
  std::string kind;
  uint64_t id = 0;
  std::string title;
  std::string body;
  std::string section;
  std::string label;
  std::string value;
  std::string description;
  std::string description_key;
  std::string title_key;
  std::string label_key;
  uint32_t locale = 0;
  std::vector<std::string> hints;
  uint32_t position = 0;
  uint32_t total = 0;
  bool selected = false;
};

// Copies only text captured at a verified native UI consumer. No persistent text log.
bool SendScreenEvent(const ScreenEvent& event) noexcept;
