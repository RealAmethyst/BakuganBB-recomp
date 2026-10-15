#pragma once

#include <string_view>

#include "guest_ui_memory.h"
#include "native_speech.h"

// Set before the guest thread starts; the reader must validate the whole range.
void SetGameSpeechReader(brawlers::ui::ReadBytes reader);

// Fence transport output outside guest/global critical locks; not speech playback.
bool FlushGameSpeech();

// Queue a bounded diagnostic key without doing IO under the guest region.
bool QueueGameDiagnostic(std::string_view reason) noexcept;

// Other verified game observers share this FIFO and its fail-closed lifetime.
bool QueueGameEvent(ScreenEvent&& event) noexcept;
void FailGameCapture() noexcept;

using GameTaskRemovedObserver = void (*)(uint32_t) noexcept;
// Register before guest execution; observers must preserve native execution.
void SetGameTaskRemovedObserver(GameTaskRemovedObserver observer);
