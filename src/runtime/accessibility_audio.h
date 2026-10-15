#pragma once

#include <filesystem>

#include "spatial_audio_runtime.h"

namespace brawlers::audio {

// Process lifetime covers the guest threads, including captures rejected during
// host shutdown. Initialize/shutdown are serialized on the host UI thread.
SpatialAudioRuntime& AccessibilityAudio();
bool InitializeAccessibilityAudio(const std::filesystem::path& installation);
void ShutdownAccessibilityAudio();

}  // namespace brawlers::audio
