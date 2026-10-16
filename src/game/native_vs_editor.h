#pragma once

#include "guest_ui_memory.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

enum class VsEditorEventKind {
  Opened, Focused, Resumed, Header, Hints, Footer, Status, EmptyInventory, Suspended, Closed,
  PickerOpened, PickerFocused, PickerClosed,
  ConfirmationOpened, ConfirmationFocused, ConfirmationStatus, ConfirmationClosed,
};

struct VsEditorSelection {
  uint32_t section = 0;
  uint32_t row = 0;
  uint32_t enabled_rows = 0;
  uint32_t page = 0;
  uint32_t last_page = 0;
  uint32_t inventory_count = 0;
  std::optional<uint32_t> item_type;
  std::optional<uint32_t> item_id;
  std::optional<uint32_t> attribute;
  std::optional<uint32_t> displayed_number;
  std::optional<uint32_t> level;
  bool finish = false;
  bool empty = false;
  bool replaceable = true;
  std::string caption;
  std::string section_caption;
  std::string attribute_name;
  bool operator==(const VsEditorSelection&) const = default;
};

// Captured native facts. Speech policy supplies labels for icon-only controls
// and decides how section/page controls contribute to a navigable item count.
struct VsEditorEvent {
  VsEditorEventKind kind = VsEditorEventKind::Opened;
  uint64_t id = 0;
  uint64_t child_id = 0;
  uint32_t controller = 0;
  uint32_t locale = 0;
  uint32_t section = 0;
  uint32_t picker_kind = 0;
  uint32_t choice = 0;
  uint32_t choices = 0;
  std::optional<VsEditorSelection> selection;
  std::string text;
  std::string label;
  std::string section_caption;
};

void SetVsEditorCapture(brawlers::ui::ReadBytes reader,
                        std::function<void(VsEditorEvent&&)> output,
                        std::function<void(std::string_view)> diagnostic,
                        std::function<void()> failure);
void NotifyVsEditorSelection(uint32_t selector, uint32_t selected) noexcept;
void NotifyVsEditorTaskRemoved(uint32_t owner) noexcept;
void NotifyVsEditorViewerOpened(uint32_t owner) noexcept;
