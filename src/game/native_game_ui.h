#pragma once

#include "guest_ui_memory.h"

// Set before the guest thread starts; the reader must validate the whole range.
void SetGameSpeechReader(brawlers::ui::ReadBytes reader);

// Fence transport output outside guest/global critical locks; not speech playback.
bool FlushGameSpeech();
