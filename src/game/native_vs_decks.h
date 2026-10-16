#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

void SetVsDecksCapture(brawlers::ui::ReadBytes reader,
                       std::function<void(uint32_t, ScreenEvent&&)> output,
                       std::function<void(std::string_view)> diagnostic,
                       std::function<void()> failure);
void NotifyVsDecksTaskRemoved(uint32_t owner) noexcept;
void NotifyVsDecksSelection(uint32_t selector, uint32_t selected) noexcept;
