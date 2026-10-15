#include "accessibility_audio.h"

#include <cstdio>

namespace brawlers::audio {
namespace {

void ReportDiagnostics() {
  const uint64_t bits = AccessibilityAudio().Diagnostics();
  for (unsigned i = 0; i < static_cast<unsigned>(Diagnostic::Count); ++i) {
    if (bits & (uint64_t{1} << i)) {
      const auto reason = DiagnosticName(static_cast<Diagnostic>(i));
      std::fprintf(stderr, "BRAWLERS_AUDIO:guard=%.*s\n", static_cast<int>(reason.size()), reason.data());
    }
  }
}

}  // namespace

SpatialAudioRuntime& AccessibilityAudio() {
  static SpatialAudioRuntime runtime;
  return runtime;
}

bool InitializeAccessibilityAudio(const std::filesystem::path& installation) {
  try {
    const RuntimeConfig config{
        (installation / "bin/libphonon.so").string(),
        (installation / "accessibility-audio/gate-friendly.wav").string(),
        (installation / "accessibility-audio/gate-enemy.wav").string()};
    const bool started = AccessibilityAudio().Start(config);
    if (!started)
      ReportDiagnostics();
    return started;
  } catch (...) {
    std::fprintf(stderr, "BRAWLERS_AUDIO:guard=initialization_paths\n");
    return false;
  }
}

void ShutdownAccessibilityAudio() {
  AccessibilityAudio().Stop();
  ReportDiagnostics();
}

}  // namespace brawlers::audio
