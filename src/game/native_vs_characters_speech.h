#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

// Installs native character capture and converts its facts into queued speech
// events. Controller prefixes and translated templates remain central policy.
void SetVsCharactersSpeech(brawlers::ui::ReadBytes reader,
                           std::function<void(ScreenEvent&&)> output,
                           std::function<void(std::string_view)> diagnostic,
                           std::function<void()> failure);
