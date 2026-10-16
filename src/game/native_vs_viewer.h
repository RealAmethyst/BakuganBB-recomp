#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

void SetVsViewerCapture(brawlers::ui::ReadBytes reader,
                        std::function<void(ScreenEvent&&)> output,
                        std::function<void(std::string_view)> diagnostic,
                        std::function<void()> failure);
void NotifyVsViewerTaskRemoved(uint32_t owner) noexcept;
