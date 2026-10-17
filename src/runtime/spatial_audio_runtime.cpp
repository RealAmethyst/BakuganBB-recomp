#include "spatial_audio_runtime.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_loadso.h>
#include <phonon.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace brawlers::audio {
namespace {

static_assert(STEAMAUDIO_VERSION_MAJOR == 4 && STEAMAUDIO_VERSION_MINOR == 8 &&
              STEAMAUDIO_VERSION_PATCH == 1, "Revalidate before changing Steam Audio headers");
static_assert(sizeof(float) == 4, "WAV and SDL input require 32-bit float");

uint16_t U16(const unsigned char* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
uint32_t U32(const unsigned char* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

// Only startup calls this parser. Unknown, bounded RIFF chunks are skipped.
Diagnostic LoadClip(const std::filesystem::path& path, std::vector<float>& output) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return Diagnostic::AssetOpen;
  const auto end = file.tellg();
  if (end < 12 || end > 16 * 1024 * 1024) return Diagnostic::AssetBounds;
  std::vector<unsigned char> bytes(static_cast<size_t>(end));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return Diagnostic::AssetOpen;
  if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4) ||
      uint64_t(U32(bytes.data() + 4)) + 8 != bytes.size()) return Diagnostic::AssetBounds;
  const unsigned char* format = nullptr;
  const unsigned char* data = nullptr;
  size_t format_size = 0, data_size = 0;
  for (size_t offset = 12; offset < bytes.size();) {
    if (bytes.size() - offset < 8) return Diagnostic::AssetBounds;
    const size_t size = U32(bytes.data() + offset + 4);
    const size_t padded = size + (size & 1);
    if (padded > bytes.size() - offset - 8) return Diagnostic::AssetBounds;
    const auto* chunk = bytes.data() + offset;
    if (!std::memcmp(chunk, "fmt ", 4)) {
      if (format) return Diagnostic::AssetFormat;
      format = chunk + 8;
      format_size = size;
    } else if (!std::memcmp(chunk, "data", 4)) {
      if (data) return Diagnostic::AssetFormat;
      data = chunk + 8;
      data_size = size;
    }
    offset += 8 + padded;
  }
  if (!format || format_size < 16 || !data || !data_size || data_size % 4)
    return Diagnostic::AssetFormat;
  const uint32_t rate = U32(format + 4);
  if (U16(format) != 3 || U16(format + 2) != 1 || U16(format + 12) != 4 ||
      U16(format + 14) != 32 || rate != kSampleRate ||
      U32(format + 8) != rate * 4) return Diagnostic::AssetFormat;
  const size_t frames = data_size / 4;
  if (frames > size_t(rate) * 30) return Diagnostic::AssetBounds;
  std::vector<float> input(frames);
  for (size_t i = 0; i < frames; ++i) {
    const uint32_t bits = U32(data + i * 4);
    std::memcpy(&input[i], &bits, sizeof(float));
    if (!std::isfinite(input[i]) || std::abs(input[i]) > 1) return Diagnostic::AssetSamples;
  }
  output = std::move(input);
  return Diagnostic::Count;
}

struct SteamApi {
  SDL_SharedObject* library = nullptr;
#define STEAM_FUNCTION(name) decltype(&::name) name = nullptr
  STEAM_FUNCTION(iplContextCreate);
  STEAM_FUNCTION(iplContextRelease);
  STEAM_FUNCTION(iplHRTFCreate);
  STEAM_FUNCTION(iplHRTFRelease);
  STEAM_FUNCTION(iplBinauralEffectCreate);
  STEAM_FUNCTION(iplBinauralEffectRelease);
  STEAM_FUNCTION(iplBinauralEffectReset);
  STEAM_FUNCTION(iplBinauralEffectApply);
#undef STEAM_FUNCTION
  ~SteamApi() { if (library) SDL_UnloadObject(library); }
  Diagnostic Load(const std::filesystem::path& path) {
    if (path.empty()) return Diagnostic::SdkLoad;
    const auto utf8 = path.u8string();
    library = SDL_LoadObject(reinterpret_cast<const char*>(utf8.c_str()));
    if (!library) return Diagnostic::SdkLoad;
#define LOAD_STEAM(name) \
    name = reinterpret_cast<decltype(name)>(SDL_LoadFunction(library, #name)); \
    if (!name) return Diagnostic::SdkSymbol
    LOAD_STEAM(iplContextCreate);
    LOAD_STEAM(iplContextRelease);
    LOAD_STEAM(iplHRTFCreate);
    LOAD_STEAM(iplHRTFRelease);
    LOAD_STEAM(iplBinauralEffectCreate);
    LOAD_STEAM(iplBinauralEffectRelease);
    LOAD_STEAM(iplBinauralEffectReset);
    LOAD_STEAM(iplBinauralEffectApply);
#undef LOAD_STEAM
    return Diagnostic::Count;
  }
};

// Created completely before output starts; all storage and HRTFs stay fixed.
struct Engine {
  explicit Engine(SceneMailbox& mailbox) : mailbox(mailbox) {}
  SceneMailbox& mailbox;
  SteamApi api;
  IPLContext context = nullptr;
  IPLHRTF hrtf = nullptr;
  std::array<IPLBinauralEffect, kMaxSources> effects{};
  std::array<uint64_t, kMaxSources> effect_serials{};
  std::array<std::vector<float>, 2> clips;
  Scheduler scheduler;
  StereoBlockAdapter adapter;
  StereoLimiter limiter;
  Snapshot snapshot;
  uint64_t generation = 0, controls_revision = 0;
  std::array<float, kBlockFrames> mono{}, left{}, right{};

  ~Engine() {
    for (auto& effect : effects) if (effect) api.iplBinauralEffectRelease(&effect);
    if (hrtf) api.iplHRTFRelease(&hrtf);
    if (context) api.iplContextRelease(&context);
  }
  Diagnostic Prepare(const RuntimeConfig& config) {
    auto error = LoadClip(config.friendly_wav_path, clips[0]);
    if (error != Diagnostic::Count) return error;
    error = LoadClip(config.enemy_wav_path, clips[1]);
    if (error != Diagnostic::Count) return error;
    error = api.Load(config.library_path);
    if (error != Diagnostic::Count) return error;
    IPLContextSettings settings{};
    settings.version = STEAMAUDIO_VERSION;
    settings.simdLevel = IPL_SIMDLEVEL_SSE4;
    if (api.iplContextCreate(&settings, &context) != IPL_STATUS_SUCCESS) return Diagnostic::ContextCreate;
    IPLAudioSettings audio{kSampleRate, int(kBlockFrames)};
    IPLHRTFSettings hrtf_settings{};
    hrtf_settings.type = IPL_HRTFTYPE_DEFAULT;
    hrtf_settings.volume = 1;
    if (api.iplHRTFCreate(context, &audio, &hrtf_settings, &hrtf) != IPL_STATUS_SUCCESS)
      return Diagnostic::HrtfCreate;
    IPLBinauralEffectSettings effect_settings{};
    effect_settings.hrtf = hrtf;
    for (auto& effect : effects)
      if (api.iplBinauralEffectCreate(context, &audio, &effect_settings, &effect) != IPL_STATUS_SUCCESS)
        return Diagnostic::EffectCreate;
    return Diagnostic::Count;
  }
  // The optional barrier lets the offline fixture force a producer update
  // between the atomic reads and coherent snapshot acquisition.
  bool Synchronize(void (*before_snapshot)(SceneMailbox&) noexcept = nullptr) noexcept {
    const auto current_generation = mailbox.CaptureGeneration();
    const auto current_controls = mailbox.ControlsRevision();
    if (current_generation != generation || current_controls != controls_revision) adapter.Reset();
    if (current_generation != generation) {
      scheduler.Reset();
      limiter.Reset();
      snapshot.has_scene = false;
      generation = current_generation;
    }
    if (before_snapshot) before_snapshot(mailbox);
    Snapshot latest;
    if (mailbox.TryRead(latest)) {
      // A producer may publish new controls after current_controls was read.
      // Never label an old buffered mix with the newly acquired revision.
      if (latest.controls_revision != controls_revision) adapter.Reset();
      snapshot = latest;
      controls_revision = latest.controls_revision;
    } else if (current_controls != controls_revision) {
      // Never emit a buffered block using settings that were just disabled.
      return false;
    }
    if (!mailbox.IsOpen() || !snapshot.has_scene || snapshot.generation != generation) {
      scheduler.Reset();
      return false;
    }
    scheduler.Apply(snapshot.scene, snapshot.controls);
    return true;
  }
  static void RenderBlock(void* userdata, float* stereo) noexcept {
    static_cast<Engine*>(userdata)->RenderBlock(stereo);
  }
  void RenderBlock(float* stereo) noexcept {
    std::fill_n(stereo, 2 * kBlockFrames, 0);
    if (!Synchronize()) return;
    float* inputs[]{mono.data()};
    float* outputs[]{left.data(), right.data()};
    IPLAudioBuffer input{1, int(kBlockFrames), inputs};
    IPLAudioBuffer output{2, int(kBlockFrames), outputs};
    for (size_t slot = 0; slot < kMaxSources; ++slot) {
      const auto& voice = scheduler.voices()[slot];
      if (!voice.active) continue;
      if (effect_serials[slot] != voice.serial) {
        api.iplBinauralEffectReset(effects[slot]);
        effect_serials[slot] = voice.serial;
      }
      const size_t cue = size_t(voice.source.cue);
      scheduler.Fill(slot, clips[cue].data(), clips[cue].size(), mono.data(), kBlockFrames);
      Vec3 direction;
      if (!RelativeDirection(snapshot.scene, voice.source, direction)) {
        mailbox.Report(Diagnostic::CoincidentSource);
        api.iplBinauralEffectReset(effects[slot]);
        continue;
      }
      IPLBinauralEffectParams parameters{};
      parameters.direction = {direction.x, direction.y, direction.z};
      parameters.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
      parameters.spatialBlend = 1;
      parameters.hrtf = hrtf;
      api.iplBinauralEffectApply(effects[slot], &parameters, &input, &output);
      const float volume = snapshot.controls[cue].volume;
      for (size_t i = 0; i < kBlockFrames; ++i) {
        stereo[i * 2] += left[i] * volume;
        stereo[i * 2 + 1] += right[i] * volume;
      }
    }
    for (size_t i = 0; i < 2 * kBlockFrames; ++i) {
      if (!std::isfinite(stereo[i])) {
        mailbox.Report(Diagnostic::NonfiniteOutput);
        mailbox.Close();
        std::fill_n(stereo, 2 * kBlockFrames, 0);
        return;
      }
    }
    if (limiter.Process(stereo, kBlockFrames)) mailbox.Report(Diagnostic::MixLimited);
  }
  void Read(float* stereo, size_t frames) noexcept {
    if (!Synchronize()) {
      adapter.Reset();
      std::fill_n(stereo, frames * 2, 0);
      return;
    }
    const auto before_generation = generation;
    const auto before_controls = controls_revision;
    adapter.Read(stereo, frames, &Engine::RenderBlock, this);
    if (!mailbox.IsOpen() || mailbox.CaptureGeneration() != before_generation ||
        mailbox.ControlsRevision() != before_controls) {
      adapter.Reset();
      std::fill_n(stereo, frames * 2, 0);
    }
  }
};

}  // namespace

struct SpatialAudioRuntime::Impl {
  explicit Impl(SceneMailbox& mailbox) : engine(mailbox) {}
  Engine engine;
  SDL_AudioStream* stream = nullptr;
  bool audio_initialized = false;

  ~Impl() {
    // SDL synchronizes destruction with its callback before DSP is destroyed.
    if (stream) SDL_DestroyAudioStream(stream);
    if (audio_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
  static void SDLCALL Callback(void* userdata, SDL_AudioStream* stream,
                              int additional_bytes, int) noexcept {
    auto& self = *static_cast<Impl*>(userdata);
    if (additional_bytes <= 0) return;
    // SDL requests bytes in the stream's input format. Round to complete stereo
    // frames; the adapter retains partially consumed DSP blocks between calls.
    size_t frames = (size_t(additional_bytes) + 2 * sizeof(float) - 1) / (2 * sizeof(float));
    std::array<float, 2 * kBlockFrames> stereo{};
    while (frames) {
      const size_t count = std::min(frames, kBlockFrames);
      self.engine.Read(stereo.data(), count);
      if (!SDL_PutAudioStreamData(stream, stereo.data(), int(count * 2 * sizeof(float)))) {
        self.engine.mailbox.Report(Diagnostic::OutputWrite);
        self.engine.mailbox.Close();
        return;
      }
      frames -= count;
    }
  }
};

SpatialAudioRuntime::SpatialAudioRuntime() = default;
SpatialAudioRuntime::~SpatialAudioRuntime() { Stop(); }
bool SpatialAudioRuntime::Start(const RuntimeConfig& config) noexcept {
  if (started_) return false;
  started_ = true;
  try {
    impl_ = std::make_unique<Impl>(mailbox_);
    auto error = impl_->engine.Prepare(config);
    if (error == Diagnostic::Count) {
      if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) error = Diagnostic::OutputInit;
      else impl_->audio_initialized = true;
    }
    if (error == Diagnostic::Count) {
      const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kSampleRate};
      impl_->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                              &Impl::Callback, impl_.get());
      if (!impl_->stream) error = Diagnostic::OutputOpen;
    }
    if (error == Diagnostic::Count) {
      mailbox_.Open();
      if (!SDL_ResumeAudioStreamDevice(impl_->stream)) error = Diagnostic::OutputResume;
    }
    if (error == Diagnostic::Count) return true;
    mailbox_.Report(error);
  } catch (...) {
    mailbox_.Report(Diagnostic::StartException);
  }
  Stop();
  return false;
}
void SpatialAudioRuntime::Stop() noexcept {
  mailbox_.Close();
  impl_.reset();
}
uint64_t SpatialAudioRuntime::CaptureGeneration() const noexcept { return mailbox_.CaptureGeneration(); }
bool SpatialAudioRuntime::Publish(const Scene& scene, uint64_t generation) noexcept {
  return mailbox_.Publish(scene, generation);
}
void SpatialAudioRuntime::Invalidate() noexcept { mailbox_.Invalidate(); }
bool SpatialAudioRuntime::Configure(Cue cue, bool enabled, float volume) noexcept {
  return mailbox_.Configure(cue, enabled, volume);
}
bool SpatialAudioRuntime::Running() const noexcept { return mailbox_.IsOpen(); }
uint64_t SpatialAudioRuntime::Diagnostics() const noexcept { return mailbox_.Diagnostics(); }

}  // namespace brawlers::audio
