#pragma once

#include <filesystem>
#include <memory>

#include "../core/spatial_audio.h"

namespace brawlers::audio {

struct RuntimeConfig {
  std::filesystem::path library_path;
  // Mono IEEE float32 RIFF WAV, 48 kHz, <=30 seconds/16 MiB, finite peak<=1.
  // Single approved cycles: runtime supplies equally long silent intervals.
  std::filesystem::path friendly_wav_path;
  std::filesystem::path enemy_wav_path;
};

// Native SDL output, independent of the guest audio device/stream. The owner
// serializes Start/Stop/destruction outside guest locks. Keep this object alive
// until all capture producers have joined; Stop rejects them before freeing DSP.
// Single-use: initialization and restart are never attempted in a callback.
class SpatialAudioRuntime {
 public:
  SpatialAudioRuntime();
  ~SpatialAudioRuntime();
  SpatialAudioRuntime(const SpatialAudioRuntime&) = delete;
  SpatialAudioRuntime& operator=(const SpatialAudioRuntime&) = delete;

  bool Start(const RuntimeConfig& config) noexcept;
  void Stop() noexcept;
  uint64_t CaptureGeneration() const noexcept;
  bool Publish(const Scene& scene, uint64_t generation) noexcept;
  void Invalidate() noexcept;
  bool Configure(Cue cue, bool enabled, float volume) noexcept;
  bool Running() const noexcept;
  // Latched bits (1 << Diagnostic); report newly observed keys on a host thread.
  uint64_t Diagnostics() const noexcept;

 private:
  struct Impl;
  SceneMailbox mailbox_;
  std::unique_ptr<Impl> impl_;
  bool started_ = false;
};

}  // namespace brawlers::audio
