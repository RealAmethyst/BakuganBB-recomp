#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

void SetVsModeCapture(brawlers::ui::ReadBytes reader,
                      std::function<void(ScreenEvent&&)> output,
                      std::function<void(std::string_view)> diagnostic,
                      std::function<void()> failure);
void NotifyVsModeTaskRemoved(uint32_t owner) noexcept;
void NotifyVsModeSelection(uint32_t selector, uint32_t index) noexcept;
