// Diagnostic integration for the private statically recompiled build.
#include <cstdio>
#include <memory>
#include <mutex>

#include <spdlog/sinks/base_sink.h>

#include <rex/logging.h>
#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/filesystem.h>

#include "battle_brawlers_init.h"
#include "native_game_ui.h"
#include "native_battle_speech.h"
#include "native_versus_speech.h"
#include "native_settings.h"
#include "settings_runtime.h"
#ifdef BRAWLERS_SPATIAL_AUDIO
#include "accessibility_audio.h"
#include "native_gate_audio.h"
#endif

REXCVAR_DECLARE(bool, vulkan_timing_diagnostics);

void InstallDialogSpeech();

namespace {

void Report(const char* event) {
  // One stdio operation keeps events intact across runtime threads.
  std::fprintf(stderr, "BRAWLERS_STATUS:%s\n", event);
  std::fflush(stderr);
}

class DiagnosticSink final : public spdlog::sinks::base_sink<std::mutex> {
 protected:
  void sink_it_(const spdlog::details::log_msg& message) override {
    if (message.level >= spdlog::level::err && !reported_error_) {
      reported_error_ = true;
      Report("runtime_error");
    }
  }
  void flush_() override {}

 private:
  bool reported_error_ = false;
};

class BattleBrawlersHost final : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<BattleBrawlersHost>(new BattleBrawlersHost(context));
  }

 private:
  explicit BattleBrawlersHost(rex::ui::WindowedAppContext& context)
      : rex::ReXApp(context, "battle_brawlers", PPCImageConfig) {}

  void OnConfigurePaths(rex::PathConfig& paths) override {
    paths.config_path = paths.user_data_root / "battle_brawlers.toml";
    settings_path_ = paths.config_path;
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    InstallDialogSpeech();
    config.gpu_plugin = "xenos";
    REXCVAR_SET(vulkan_timing_diagnostics, true);
  }

  void OnPostInitLogging() override { rex::AddSink(std::make_shared<DiagnosticSink>()); }

  bool SetupEnvironment() override {
    Report("environment");
    bool result = rex::ReXApp::SetupEnvironment();
    if (!result)
      Report("environment_failed");
    return result;
  }

  bool SetupPresentation() override {
    Report("graphics");
    bool result = rex::ReXApp::SetupPresentation();
    if (!result)
      Report("graphics_failed");
    return result;
  }

  bool ConstructRuntime(const rex::PathConfig& paths) override {
    settings_path_ = paths.config_path;
    Report("runtime");
    bool result = rex::ReXApp::ConstructRuntime(paths);
    if (!result)
      Report("runtime_failed");
    // The imported XAudio client entrypoints require a non-null audio system.
    if (result && !runtime()->audio_system()) {
      Report("audio_unavailable");
      return false;
    }
    return result;
  }

  void OnPostLoadXexImage() override {
    auto* memory = runtime()->memory();
    SetNativeSettings(memory, &settings_);
    const auto reader = [memory](uint32_t address, void* output, size_t size) {
      return memory && memory->TryReadVirtual(address, output, size);
    };
    SetGameSpeechReader(reader);
    SetBattleSpeechCapture(reader, QueueGameEvent, QueueGameDiagnostic, FailGameCapture);
    SetVersusSpeechCapture(reader, QueueGameEvent, QueueGameDiagnostic, FailGameCapture);
    SetGameTaskRemovedObserver([](uint32_t owner) noexcept {
      NotifyBattleTaskRemoved(owner);
      NotifyVersusTaskRemoved(owner);
    });
#ifdef BRAWLERS_SPATIAL_AUDIO
    using brawlers::audio::SpatialAudioRuntime;
    brawlers::game::GateAudioOutput output;
    output.context = &brawlers::audio::AccessibilityAudio();
    output.capture_generation = [](void* context) noexcept {
      return static_cast<SpatialAudioRuntime*>(context)->CaptureGeneration();
    };
    output.publish = [](void* context, const brawlers::audio::Scene& scene,
                        uint64_t generation) noexcept {
      return static_cast<SpatialAudioRuntime*>(context)->Publish(scene, generation);
    };
    output.invalidate = [](void* context) noexcept {
      static_cast<SpatialAudioRuntime*>(context)->Invalidate();
    };
    output.diagnostic = [](void*, std::string_view reason) noexcept {
      QueueGameDiagnostic(reason);
    };
    brawlers::game::SetGateAudioCapture(reader, output);
#endif
    Report("image_loaded");
  }
  void OnPostSetup() override {
    const auto installation = rex::filesystem::GetExecutableFolder().parent_path();
#ifdef BRAWLERS_SPATIAL_AUDIO
    if (!brawlers::audio::InitializeAccessibilityAudio(installation)) {
      Report("spatial_audio_unavailable");
    } else {
      brawlers::game::ConfigureGateAudioCapture(
          brawlers::game::GateAudioActivation::NativeCardRendering);
    }
#endif
    auto* graphics = runtime()->graphics_system();
    std::string error;
    if (!settings_.Configure(runtime(), &app_context(), window(),
        graphics ? graphics->presenter() : nullptr, settings_path_, installation, &error)) {
      std::fprintf(stderr, "Settings initialization: %s\n", error.c_str());
      Report("settings_unavailable");
    }
  }
  void DrainSettings() {
    settings_.Shutdown();
    while (const auto event = settings_.Poll()) {
      if (event->kind == brawlers::settings::HostEventKind::SaveFailed)
        Report("settings_save_failed_on_exit");
      else if (event->kind == brawlers::settings::HostEventKind::ApplyFailed)
        Report("settings_apply_failed_on_exit");
    }
  }
  bool OnWindowCloseRequested() override {
    // ReXApp's normal window-close path hard-exits without OnShutdown.
    // Finish accepted config writes before allowing that existing path.
    DrainSettings();
    return true;
  }
  void OnShutdown() override {
    DrainSettings();
#ifdef BRAWLERS_SPATIAL_AUDIO
    brawlers::game::ConfigureGateAudioCapture(brawlers::game::GateAudioActivation::Disabled);
    brawlers::audio::ShutdownAccessibilityAudio();
#endif
  }
  void OnPreLaunchModule() override { Report("preparing_thread"); }
  void OnPostLaunchModule(rex::system::XThread*) override { Report("thread_created"); }
  void OnGuestThreadExit(rex::system::XThread*) override {
#ifdef BRAWLERS_SPATIAL_AUDIO
    brawlers::audio::AccessibilityAudio().Invalidate();
#endif
    Report("guest_exited");
  }

  std::filesystem::path settings_path_;
  brawlers::settings::HostSettingsRuntime settings_;
};

}  // namespace

REX_DEFINE_APP(battle_brawlers, BattleBrawlersHost::Create)
