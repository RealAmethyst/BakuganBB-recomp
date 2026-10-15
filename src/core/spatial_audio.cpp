#include "spatial_audio.h"

#include <algorithm>
#include <cmath>

namespace brawlers::audio {
namespace {
bool Finite(Vec3 v) noexcept {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
double Dot(Vec3 a, Vec3 b) noexcept {
  return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
}
bool ValidCue(Cue cue) noexcept { return cue == Cue::Friendly || cue == Cue::Enemy; }
}  // namespace

std::string_view DiagnosticName(Diagnostic diagnostic) noexcept {
  constexpr std::array<std::string_view, size_t(Diagnostic::Count)> names{
      "capacity", "invalid_listener", "invalid_source", "duplicate_identity",
      "invalid_settings", "coincident_source", "mix_limited", "nonfinite_output",
      "asset_open", "asset_bounds", "asset_format", "asset_samples", "sdk_load",
      "sdk_symbol", "context_create", "hrtf_create", "effect_create", "output_init",
      "output_open", "output_resume", "output_write", "start_exception"};
  return size_t(diagnostic) < names.size() ? names[size_t(diagnostic)] : "none";
}

Diagnostic ValidateScene(const Scene& scene) noexcept {
  if (scene.count > kMaxSources) return Diagnostic::Capacity;
  if (!Finite(scene.listener_position) || !Finite(scene.listener_ahead) ||
      !Finite(scene.listener_up) ||
      std::abs(Dot(scene.listener_ahead, scene.listener_ahead) - 1) > 0.001 ||
      std::abs(Dot(scene.listener_up, scene.listener_up) - 1) > 0.001 ||
      std::abs(Dot(scene.listener_ahead, scene.listener_up)) > 0.001)
    return Diagnostic::InvalidListener;
  for (size_t i = 0; i < scene.count; ++i) {
    const auto& source = scene.sources[i];
    if (!source.id || !ValidCue(source.cue) || !Finite(source.position) ||
        !std::isfinite(source.level) || source.level < 0 || source.level > 1)
      return Diagnostic::InvalidSource;
    for (size_t j = 0; j < i; ++j)
      if (scene.sources[j].id == source.id) return Diagnostic::DuplicateIdentity;
  }
  return Diagnostic::Count;
}

bool RelativeDirection(const Scene& scene, const Source& source, Vec3& result) noexcept {
  const double dx = double(source.position.x) - scene.listener_position.x;
  const double dy = double(source.position.y) - scene.listener_position.y;
  const double dz = double(source.position.z) - scene.listener_position.z;
  const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (!(length > 0) || !std::isfinite(length)) return false;
  const auto a = scene.listener_ahead;
  const auto u = scene.listener_up;
  const Vec3 r{a.y * u.z - a.z * u.y, a.z * u.x - a.x * u.z,
               a.x * u.y - a.y * u.x};
  const double x = (dx * r.x + dy * r.y + dz * r.z) / length;
  const double y = (dx * u.x + dy * u.y + dz * u.z) / length;
  const double z = -(dx * a.x + dy * a.y + dz * a.z) / length;
  const double local_length = std::sqrt(x * x + y * y + z * z);
  if (!(local_length > 0) || !std::isfinite(local_length)) return false;
  result = {float(x / local_length), float(y / local_length), float(z / local_length)};
  return true;
}

uint64_t SceneMailbox::CaptureGeneration() const noexcept { return generation_.load(); }
bool SceneMailbox::Publish(const Scene& scene, uint64_t generation) noexcept {
  if (!open_.load() || generation != generation_.load()) return false;
  const auto error = ValidateScene(scene);
  if (error != Diagnostic::Count) {
    Report(error);
    Invalidate();
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!open_.load() || generation != generation_.load()) return false;
  latest_.scene = scene;
  latest_.generation = generation;
  latest_.has_scene = true;
  return true;
}
bool SceneMailbox::Configure(Cue cue, bool enabled, float volume) noexcept {
  if (!ValidCue(cue) || !std::isfinite(volume) || volume < 0 || volume > 1) {
    Report(Diagnostic::InvalidSettings);
    Invalidate();
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto& control = latest_.controls[size_t(cue)];
  if (control.enabled != enabled || control.volume != volume) {
    if (control.enabled != enabled) ++control.epoch;
    control.enabled = enabled;
    control.volume = volume;
    latest_.controls_revision = controls_revision_.fetch_add(1) + 1;
  }
  return true;
}
void SceneMailbox::Invalidate() noexcept { generation_.fetch_add(1); }
void SceneMailbox::Open() noexcept { Invalidate(); open_.store(true); }
void SceneMailbox::Close() noexcept { open_.store(false); Invalidate(); }
bool SceneMailbox::IsOpen() const noexcept { return open_.load(); }
uint64_t SceneMailbox::ControlsRevision() const noexcept { return controls_revision_.load(); }
bool SceneMailbox::TryRead(Snapshot& snapshot) noexcept {
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) return false;
  snapshot = latest_;
  if (!open_.load() || snapshot.generation != generation_.load()) snapshot.has_scene = false;
  return true;
}
uint64_t SceneMailbox::Diagnostics() const noexcept { return diagnostics_.load(); }
void SceneMailbox::Report(Diagnostic diagnostic) noexcept {
  if (diagnostic != Diagnostic::Count) diagnostics_.fetch_or(uint64_t{1} << uint8_t(diagnostic));
}

void Scheduler::Reset() noexcept {
  for (auto& voice : voices_) voice.active = false;
}
void Scheduler::Apply(const Scene& scene, const std::array<Control, 2>& controls) noexcept {
  std::array<bool, kMaxSources> retained{};
  // Reserve all surviving slots before admitting new sources.
  for (auto& voice : voices_) {
    if (!voice.active) continue;
    if (epochs_[size_t(voice.source.cue)] != controls[size_t(voice.source.cue)].epoch) {
      voice.active = false;
      continue;
    }
    bool found = false;
    for (size_t i = 0; i < scene.count; ++i) {
      const auto& source = scene.sources[i];
      if (controls[size_t(source.cue)].enabled && source.id == voice.source.id &&
          source.cue == voice.source.cue) {
        voice.source = source;
        retained[i] = found = true;
        break;
      }
    }
    if (!found) voice.active = false;
  }
  for (size_t i = 0; i < epochs_.size(); ++i) epochs_[i] = controls[i].epoch;
  for (size_t i = 0; i < scene.count; ++i) {
    if (retained[i] || !controls[size_t(scene.sources[i].cue)].enabled) continue;
    for (auto& voice : voices_) {
      if (!voice.active) {
        voice = {scene.sources[i], 0, ++serial_, true};
        break;
      }
    }
  }
}
void Scheduler::Fill(size_t slot, const float* clip, size_t frames, float* output,
                     size_t count) noexcept {
  auto& voice = voices_[slot];
  if (!voice.active || !frames) {
    std::fill_n(output, count, 0);
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    output[i] = voice.phase < frames ? clip[voice.phase] * voice.source.level : 0;
    if (++voice.phase == frames * 2) voice.phase = 0;
  }
}

void StereoBlockAdapter::Reset() noexcept { cursor_ = kBlockFrames; }
void StereoBlockAdapter::Read(float* output, size_t frames, Renderer renderer,
                              void* context) noexcept {
  while (frames) {
    if (cursor_ == kBlockFrames) { renderer(context, block_.data()); cursor_ = 0; }
    const size_t take = std::min(frames, kBlockFrames - cursor_);
    std::copy_n(block_.data() + cursor_ * 2, take * 2, output);
    cursor_ += take;
    output += take * 2;
    frames -= take;
  }
}
bool StereoLimiter::Process(float* stereo, size_t frames) noexcept {
  constexpr float recovery = 0.9995834201f;  // exp(-1 / (48000 * .050))
  bool limited = false;
  for (size_t i = 0; i < frames; ++i) {
    const float peak = std::max(std::abs(stereo[2 * i]), std::abs(stereo[2 * i + 1]));
    const float target = peak > 0.95f ? 0.95f / peak : 1;
    gain_ = std::min(target, 1 - (1 - gain_) * recovery);
    limited |= target < 1;
    stereo[2 * i] *= gain_;
    stereo[2 * i + 1] *= gain_;
  }
  return limited;
}

}  // namespace brawlers::audio
