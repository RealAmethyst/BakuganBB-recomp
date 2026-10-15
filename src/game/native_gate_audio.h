#pragma once

#include "guest_ui_memory.h"
#include "spatial_audio.h"

#include <cstdint>
#include <string_view>

namespace brawlers::game {

// This describes the verified native draw gate, not an inferred battle phase.
enum class GateAudioActivation { Disabled, NativeCardRendering };

struct GateAudioOutput {
  void* context = nullptr;
  uint64_t (*capture_generation)(void*) noexcept = nullptr;
  bool (*publish)(void*, const audio::Scene&, uint64_t) noexcept = nullptr;
  void (*invalidate)(void*) noexcept = nullptr;
  void (*diagnostic)(void*, std::string_view) noexcept = nullptr;
};

// Install before guest threads start. The reader validates the entire range.
// Output callbacks only update/queue values: no devices, IO, or guest reentry.
// The diagnostic callback must enqueue diagnostics for delivery outside guest
// critical locks. The runtime owns output lifetime until guest threads stop.
void SetGateAudioCapture(ui::ReadBytes reader, GateAudioOutput output);

// Camera listener; unique active human participant; own cards versus other cards.
// Team and unverified relationship modes are unsupported and remain silent.
// Capture remains disabled until the host explicitly selects an activation.
void ConfigureGateAudioCapture(GateAudioActivation activation) noexcept;

}  // namespace brawlers::game
