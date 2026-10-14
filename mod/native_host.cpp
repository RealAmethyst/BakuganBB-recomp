// Diagnostic integration for the private statically recompiled build.
#include <cstdio>
#include <memory>
#include <mutex>

#include <spdlog/sinks/base_sink.h>

#include <rex/logging.h>
#include <rex/cvar.h>
#include <rex/rex_app.h>

#include "battle_brawlers_init.h"
#include "native_game_ui.h"

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
    SetGameSpeechReader([memory](uint32_t address, void* output, size_t size) {
      return memory && memory->TryReadVirtual(address, output, size);
    });
    Report("image_loaded");
  }
  void OnPreLaunchModule() override { Report("preparing_thread"); }
  void OnPostLaunchModule(rex::system::XThread*) override { Report("thread_created"); }
  void OnGuestThreadExit(rex::system::XThread*) override { Report("guest_exited"); }
};

}  // namespace

REX_DEFINE_APP(battle_brawlers, BattleBrawlersHost::Create)
