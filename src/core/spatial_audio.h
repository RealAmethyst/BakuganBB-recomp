#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace brawlers::audio {

inline constexpr size_t kMaxSources = 32;
inline constexpr size_t kBlockFrames = 256;
inline constexpr int kSampleRate = 48000;

struct Vec3 { float x = 0, y = 0, z = 0; };
enum class Cue : uint8_t { Friendly, Enemy };
struct Source {
  uint64_t id = 0;
  Cue cue = Cue::Friendly;
  Vec3 position;
  float level = 1;
};
struct Scene {
  Vec3 listener_position;
  Vec3 listener_ahead{0, 0, -1};
  Vec3 listener_up{0, 1, 0};
  std::array<Source, kMaxSources> sources{};
  size_t count = 0;
};
enum class Diagnostic : uint8_t {
  Capacity, InvalidListener, InvalidSource, DuplicateIdentity, InvalidSettings,
  CoincidentSource, MixLimited, NonfiniteOutput, AssetOpen, AssetBounds,
  AssetFormat, AssetSamples, SdkLoad, SdkSymbol, ContextCreate, HrtfCreate,
  EffectCreate, OutputInit, OutputOpen, OutputResume, OutputWrite, StartException,
  Count
};
std::string_view DiagnosticName(Diagnostic diagnostic) noexcept;
// Returns Count for a valid scene. Coincident positions are valid, silent voices.
Diagnostic ValidateScene(const Scene& scene) noexcept;
bool RelativeDirection(const Scene& scene, const Source& source, Vec3& result) noexcept;

struct Control { bool enabled = true; float volume = 1; uint64_t epoch = 0; };
struct Snapshot {
  Scene scene;
  std::array<Control, 2> controls{};
  uint64_t generation = 0;
  uint64_t controls_revision = 0;
  bool has_scene = false;
};

// Producer calls can block briefly on another producer, but never on rendering.
// The renderer only tries the mutex. A capture token must precede guest reads.
class SceneMailbox {
 public:
  uint64_t CaptureGeneration() const noexcept;
  bool Publish(const Scene& scene, uint64_t generation) noexcept;
  bool Configure(Cue cue, bool enabled, float volume) noexcept;
  void Invalidate() noexcept;
  void Open() noexcept;
  void Close() noexcept;
  bool IsOpen() const noexcept;
  uint64_t ControlsRevision() const noexcept;
  bool TryRead(Snapshot& snapshot) noexcept;
  uint64_t Diagnostics() const noexcept;
  void Report(Diagnostic diagnostic) noexcept;

 private:
  std::mutex mutex_;
  Snapshot latest_;
  std::atomic<uint64_t> generation_{1};
  std::atomic<uint64_t> controls_revision_{0};
  std::atomic<uint64_t> diagnostics_{0};
  std::atomic<bool> open_{false};
};

struct Voice {
  Source source;
  size_t phase = 0;
  uint64_t serial = 0;
  bool active = false;
};
// No ownership of samples. A voice plays N samples followed by N silent samples.
// Survivors retain slots, phase and serial across reorder/transform/level changes.
class Scheduler {
 public:
  void Reset() noexcept;
  // Scene and controls must have passed mailbox validation.
  void Apply(const Scene& scene, const std::array<Control, 2>& controls) noexcept;
  void Fill(size_t slot, const float* clip, size_t frames, float* output,
            size_t count) noexcept;
  const std::array<Voice, kMaxSources>& voices() const noexcept { return voices_; }

 private:
  std::array<Voice, kMaxSources> voices_{};
  std::array<uint64_t, 2> epochs_{};
  uint64_t serial_ = 0;
};

// Fixed-block stereo rendering adapted to arbitrary whole stereo frame requests.
class StereoBlockAdapter {
 public:
  using Renderer = void (*)(void*, float*) noexcept;
  void Reset() noexcept;
  void Read(float* output, size_t frames, Renderer renderer, void* context) noexcept;

 private:
  std::array<float, 2 * kBlockFrames> block_{};
  size_t cursor_ = kBlockFrames;
};

// Stereo-linked gain preserves interaural level differences. Only overload
// reduces gain; recovery follows a 50 ms exponential toward unity.
class StereoLimiter {
 public:
  void Reset() noexcept { gain_ = 1; }
  bool Process(float* stereo, size_t frames) noexcept;
  float gain() const noexcept { return gain_; }

 private:
  float gain_ = 1;
};

}  // namespace brawlers::audio
