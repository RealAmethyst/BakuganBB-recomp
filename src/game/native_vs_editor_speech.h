#pragma once

#include "guest_ui_memory.h"
#include "native_speech.h"

#include <functional>
#include <string_view>

// Configure before guest execution. Native capture owns synchronization and
// exception isolation; this bridge translates its copied facts into speech.
void SetVsEditorSpeech(brawlers::ui::ReadBytes reader,
                       std::function<void(ScreenEvent&&)> output,
                       std::function<void(std::string_view)> diagnostic,
                       std::function<void()> failure);
