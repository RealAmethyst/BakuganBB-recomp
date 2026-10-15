#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

// Install before starting the guest thread. Only the verified mode-2 match
// entrance is captured; tournament overview and round panels are excluded.
void SetVersusSpeechCapture(brawlers::ui::ReadBytes reader,
                           std::function<void(ScreenEvent&&)> output,
                           std::function<void(std::string_view)> diagnostic,
                           std::function<void()> failure);
void NotifyVersusTaskRemoved(uint32_t owner) noexcept;
