// Original synthetic offline integration fixture for the optional audio runtime.
// Compile this translation unit together with src/core/spatial_audio.cpp only:
// it includes the runtime implementation to exercise its internal Engine and
// deterministic controls-race seam. Do not also compile spatial_audio_runtime.cpp
// separately. Link shared SDL for its real library loader; the aborting audio
// stubs below prohibit output-device operations.
//
// Standalone build from the repository root (external SDK/header directories):
// clang++ -std=c++17 -O1 -g -fsanitize=address,undefined \
//   -fno-omit-frame-pointer -Wall -Wextra -Werror \
//   -I"$STEAM_AUDIO_SDK/include" -I"$SDL_INCLUDE_DIR" \
//   tests/native/test_spatial_audio_runtime.cpp src/core/spatial_audio.cpp \
//   -lSDL3 -pthread -o "$TEST_BINARY"
// Run in an isolated filesystem/network/process environment with private
// runtime, configuration, display, bus and device resources:
// "$TEST_BINARY" "$STEAM_AUDIO_SDK/lib/linux-x64/libphonon.so" "$NEW_SCRATCH_DIR"
// The scratch directory must not exist. It retains only generated test signals
// and malformed variants. This fixture neither plays audio nor launches a game.
#ifdef NDEBUG
#error "This fixture requires assertions; do not compile with NDEBUG."
#endif

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "../../src/runtime/spatial_audio_runtime.cpp"

// This file-only harness cannot initialize an output device, even accidentally.
extern "C" bool SDL_InitSubSystem(SDL_InitFlags) { std::abort(); }
extern "C" void SDL_QuitSubSystem(SDL_InitFlags) { std::abort(); }
extern "C" SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID, const SDL_AudioSpec*,
                                                      SDL_AudioStreamCallback, void*) {
  std::abort();
}
extern "C" bool SDL_ResumeAudioStreamDevice(SDL_AudioStream*) { std::abort(); }
extern "C" void SDL_DestroyAudioStream(SDL_AudioStream*) { std::abort(); }
extern "C" bool SDL_PutAudioStreamData(SDL_AudioStream*, const void*, int) { std::abort(); }

using namespace brawlers::audio;
static void Put32(std::vector<unsigned char>& b, size_t p, uint32_t value) {
  for (unsigned n = 0; n < 4; ++n) b[p + n] = (value >> (8 * n)) & 255;
}
static void Save(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write((const char*)bytes.data(), bytes.size());
  assert(file.good());
}
static std::vector<unsigned char> Wave(size_t frames, bool loud = false) {
  std::vector<unsigned char> b(44 + frames * 4);
  std::memcpy(b.data(), "RIFF", 4);
  Put32(b, 4, b.size() - 8);
  std::memcpy(b.data() + 8, "WAVEfmt ", 8);
  Put32(b, 16, 16);
  b[20] = 3;
  b[22] = 1;
  Put32(b, 24, 48000);
  Put32(b, 28, 192000);
  b[32] = 4;
  b[34] = 32;
  std::memcpy(b.data() + 36, "data", 4);
  Put32(b, 40, frames * 4);
  for (size_t i = 0; i < frames; ++i) {
    float v = float((loud ? 0.8 : 0.03) * std::sin(i * 0.12));
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    Put32(b, 44 + i * 4, bits);
  }
  return b;
}
static Scene SceneWith(size_t count) {
  Scene s;
  s.count = count;
  for (size_t i = 0; i < count; ++i) s.sources[i] = {i + 1, Cue::Friendly, {-1, 0, 0}, 1};
  return s;
}
static void Publish(SceneMailbox& m, const Scene& s) {
  assert(m.Publish(s, m.CaptureGeneration()));
}
static double Energy(const std::vector<float>& b, size_t channel) {
  double sum = 0;
  for (size_t i = channel; i < b.size(); i += 2) sum += double(b[i]) * b[i];
  return sum;
}
static void Same(const std::vector<float>& a, const std::vector<float>& b) {
  assert(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) assert(std::abs(a[i] - b[i]) < 1e-7);
}
int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "Usage: %s STEAM_AUDIO_LIBRARY NEW_SCRATCH_DIRECTORY\n", argv[0]);
    return 2;
  }
  const std::filesystem::path scratch(argv[2]);
  const auto path = [&](const char* name) { return scratch / name; };
  assert(!std::getenv("DISPLAY") && !std::getenv("DBUS_SESSION_BUS_ADDRESS"));
  assert(!std::filesystem::exists("/dev/snd") && !std::filesystem::exists("/run/user"));
  if (!std::filesystem::create_directory(scratch)) {
    std::fprintf(stderr, "Scratch directory must be new.\n");
    return 2;
  }
  auto bytes = Wave(997);
  Save(path("synthetic.wav"), bytes);
  RuntimeConfig config{argv[1], path("synthetic.wav"), path("synthetic.wav")};
  std::vector<float> clip;
  assert(LoadClip(config.friendly_wav_path, clip) == Diagnostic::Count && clip.size() == 997);
  const auto unicode_wav = scratch / std::filesystem::path(u"synthetic-\u97f3\u58f0.wav");
  Save(unicode_wav, bytes);
  assert(LoadClip(unicode_wav, clip) == Diagnostic::Count && clip.size() == 997);
  config.friendly_wav_path = unicode_wav;
  auto unicode_library = scratch / std::filesystem::path(u"sdk-\u97f3\u58f0");
  unicode_library += config.library_path.extension();
  std::filesystem::copy_file(config.library_path, unicode_library);
  config.library_path = unicode_library;
  assert(LoadClip(path("missing.wav"), clip) == Diagnostic::AssetOpen);
  auto bad = bytes;
  Put32(bad, 4, 0xffffffff);
  Save(path("bad-size.wav"), bad);
  assert(LoadClip(path("bad-size.wav"), clip) == Diagnostic::AssetBounds);
  bad = bytes;
  Put32(bad, 24, 44100);
  Put32(bad, 28, 176400);
  Save(path("bad-rate.wav"), bad);
  assert(LoadClip(path("bad-rate.wav"), clip) == Diagnostic::AssetFormat);
  bad = bytes;
  Put32(bad, 44, 0x7fc00000);
  Save(path("bad-nan.wav"), bad);
  assert(LoadClip(path("bad-nan.wav"), clip) == Diagnostic::AssetSamples);
  bad = bytes;
  Put32(bad, 44, 0x40000000);
  Save(path("bad-headroom.wav"), bad);
  assert(LoadClip(path("bad-headroom.wav"), clip) == Diagnostic::AssetSamples);
  bad = bytes;
  Put32(bad, 40, 400000);
  Save(path("bad-data.wav"), bad);
  assert(LoadClip(path("bad-data.wav"), clip) == Diagnostic::AssetBounds);
  bad = bytes;
  bad.insert(bad.end(), bytes.begin() + 12, bytes.begin() + 36);
  Put32(bad, 4, bad.size() - 8);
  Save(path("bad-duplicate.wav"), bad);
  assert(LoadClip(path("bad-duplicate.wav"), clip) == Diagnostic::AssetFormat);
  {
    SpatialAudioRuntime failed;
    auto unavailable = config;
    unavailable.library_path = path("missing.so");
    assert(!failed.Start(unavailable));
    assert(failed.Diagnostics() & (uint64_t{1} << uint8_t(Diagnostic::SdkLoad)));
    assert(!failed.Running());
  }
  SceneMailbox a, b;
  a.Open();
  b.Open();
  Engine engine(a), reference(b);
  assert(engine.Prepare(config) == Diagnostic::Count &&
         reference.Prepare(config) == Diagnostic::Count);
  auto scene = SceneWith(2);
  scene.sources[1].cue = Cue::Enemy;
  scene.sources[1].position = {1, 0, 0};
  Publish(a, scene);
  Publish(b, scene);
  std::vector<float> actual(20000), expected(20000);
  size_t offset = 0;
  for (size_t n : {1, 317, 19, 511, 9152}) {
    engine.Read(actual.data() + offset * 2, n);
    offset += n;
  }
  reference.Read(expected.data(), 10000);
  Same(actual, expected);
  assert(Energy(actual, 0) > 0 && Energy(actual, 1) > 0);
  const auto phase = engine.scheduler.voices()[0].phase;
  const auto serial = engine.scheduler.voices()[0].serial;
  std::swap(scene.sources[0], scene.sources[1]);
  scene.sources[1].position = {-2, 1, 0};
  Publish(a, scene);
  assert(engine.Synchronize());
  assert(engine.scheduler.voices()[0].phase == phase &&
         engine.scheduler.voices()[0].serial == serial);
  a.Invalidate();
  engine.Read(actual.data(), 17);
  assert(std::all_of(actual.begin(), actual.begin() + 34, [](float v) { return v == 0; }));
  Publish(a, scene);
  engine.Read(actual.data(), 1);
  assert(engine.scheduler.voices()[0].serial != serial);

  // Both transient controls changes occur before the next callback.
  auto active = SceneWith(1);
  Publish(a, active);
  engine.Read(actual.data(), 256);
  const auto previous_serial = engine.scheduler.voices()[0].serial;
  a.Configure(Cue::Friendly, false, 1);
  a.Configure(Cue::Friendly, true, 1);
  engine.Read(actual.data(), 256);
  assert(engine.scheduler.voices()[0].serial != previous_serial);
  a.Configure(Cue::Friendly, false, 1);
  engine.Read(actual.data(), 256);
  assert(std::all_of(actual.begin(), actual.begin() + 512, [](float v) { return v == 0; }));
  a.Configure(Cue::Friendly, true, 1);

  // Force old revision read / new disabled snapshot acquisition while a
  // nonzero mixed block still has 255 buffered frames.
  a.Invalidate();
  Publish(a, SceneWith(1));
  engine.Read(actual.data(), 1);
  assert(engine.Synchronize(
      [](SceneMailbox& mailbox) noexcept { mailbox.Configure(Cue::Friendly, false, 1); }));
  engine.Read(actual.data(), 32);
  assert(std::all_of(actual.begin(), actual.begin() + 64, [](float v) { return v == 0; }));
  a.Configure(Cue::Friendly, true, 1);

  // A coincident voice consumes time silently and does not suppress its neighbor.
  a.Invalidate();
  b.Invalidate();
  auto coincident = SceneWith(2);
  coincident.sources[0].position = {0, 0, 0};
  auto neighbor = SceneWith(1);
  neighbor.sources[0] = coincident.sources[1];
  Publish(a, coincident);
  Publish(b, neighbor);
  actual.resize(2048);
  expected.resize(2048);
  engine.Read(actual.data(), 1024);
  reference.Read(expected.data(), 1024);
  Same(actual, expected);
  assert(a.Diagnostics() & (uint64_t{1} << uint8_t(Diagnostic::CoincidentSource)));
  assert(engine.scheduler.voices()[0].phase == 1024);

  double ild[3]{};
  for (size_t direction = 0; direction < 3; ++direction) {
    a.Invalidate();
    auto single = SceneWith(1);
    single.sources[0].position = direction == 0   ? Vec3{-1, 0, 0}
                                 : direction == 1 ? Vec3{1, 0, 0}
                                                  : Vec3{0, 0, -1};
    Publish(a, single);
    actual.resize(8192);
    engine.Read(actual.data(), 4096);
    ild[direction] = 10 * std::log10(Energy(actual, 0) / Energy(actual, 1));
  }
  assert(ild[0] > 3 && ild[1] < -3 && std::abs(ild[2]) < 3);
  Save(path("synthetic-loud.wav"), Wave(997, true));
  SceneMailbox many;
  many.Open();
  Engine loud(many);
  auto loud_config = config;
  loud_config.friendly_wav_path = path("synthetic-loud.wav");
  assert(loud.Prepare(loud_config) == Diagnostic::Count);
  Publish(many, SceneWith(32));
  actual.resize(20000);
  loud.Read(actual.data(), 10000);
  float peak = 0;
  for (float value : actual) {
    assert(std::isfinite(value));
    peak = std::max(peak, std::abs(value));
  }
  assert(peak <= 0.950001f && peak > 0.9f);
  assert(many.Diagnostics() & (uint64_t{1} << uint8_t(Diagnostic::MixLimited)));
  std::printf(
      "PASS: real SDK engine, malformed assets, independent voices, arbitrary reads, settings "
      "epochs, invalidation, coincidence, linked limiter; left/right/front ILD %.5f %.5f %.5f dB; "
      "32-voice peak %.8f; zero output-device calls\n",
      ild[0], ild[1], ild[2], peak);
}
