#include "settings_runtime.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>

#include <nlohmann/json.hpp>
#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>
#include <rex/ui/windowed_app_context.h>

#ifdef BRAWLERS_SPATIAL_AUDIO
#include "accessibility_audio.h"
#endif

REXCVAR_DEFINE_BOOL(accessibility_friendly_gate_enabled, true, "Accessibility/Audio",
                    "Enable the friendly gate sound");
REXCVAR_DEFINE_INT32(accessibility_friendly_gate_volume, 100, "Accessibility/Audio",
                     "Friendly gate sound volume percent").range(0, 100)
    .allowed({"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"});
REXCVAR_DEFINE_BOOL(accessibility_enemy_gate_enabled, true, "Accessibility/Audio",
                    "Enable the enemy gate sound");
REXCVAR_DEFINE_INT32(accessibility_enemy_gate_volume, 100, "Accessibility/Audio",
                     "Enemy gate sound volume percent").range(0, 100)
    .allowed({"0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"});

namespace brawlers::settings {
namespace {

using Values = std::map<Id, int32_t>;
using ConfigValues = std::vector<rex::cvar::ConfigSaveValue>;
const std::map<Id, const char*> kSingleFlags{
    {Fullscreen, "fullscreen"}, {VSync, "host_vsync"},
    {FriendlyEnabled, "accessibility_friendly_gate_enabled"},
    {FriendlyVolume, "accessibility_friendly_gate_volume"},
    {EnemyEnabled, "accessibility_enemy_gate_enabled"},
    {EnemyVolume, "accessibility_enemy_gate_volume"}};

bool Boolean(Id id) {
  return id == Fullscreen || id == VSync || id == FriendlyEnabled || id == EnemyEnabled;
}
std::string Replace(std::string text, const char* token, int32_t value) {
  size_t at = 0;
  const std::string replacement = std::to_string(value);
  while ((at = text.find(token, at)) != std::string::npos) {
    text.replace(at, std::char_traits<char>::length(token), replacement);
    at += replacement.size();
  }
  return text;
}
bool Contains(const std::vector<Value>& choices, int32_t value) {
  return std::any_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.value == value; });
}
ConfigValues Snapshot(const Values& values) {
  ConfigValues result;
  for (const auto& [id, name] : kSingleFlags) {
    const int32_t value = values.at(id);
    result.push_back({name, Boolean(id) ? (value ? "true" : "false") : std::to_string(value)});
  }
  const uint32_t size = static_cast<uint32_t>(values.at(WindowSize));
  result.push_back({"host_window_width", std::to_string(size >> 16)});
  result.push_back({"host_window_height", std::to_string(size & 65535)});
  const int32_t scale = values.at(RenderScale), x = scale / 16, y = scale % 16;
  result.push_back({"resolution_scale", std::to_string(x == y ? x : 1)});
  result.push_back({"draw_resolution_scale_x", std::to_string(x == y ? 1 : x)});
  result.push_back({"draw_resolution_scale_y", std::to_string(x == y ? 1 : y)});
  return result;
}

// Match the SDK startup fallback, excluding the new host override. ReXApp
// requests 1280 by 720. These reads do not mutate the guest video mode.
std::pair<uint32_t, uint32_t> AutomaticSize() {
  int32_t preset_x = 0, preset_y = 0;
  const bool preset = rex::graphics::video_mode_util::TryGetResolutionPresetFromCVar(preset_x, preset_y);
  const auto axis = [&](const char* window, const char* video, int32_t selected, int32_t fallback) {
    const int32_t configured = rex::cvar::Query<int32_t>(window);
    if (configured > 0) return configured;
    if (!rex::cvar::HasNonDefaultValue(window)) {
      const int32_t guest = rex::cvar::Query<int32_t>(video);
      if (rex::cvar::HasNonDefaultValue(video) && guest > 0) return std::clamp(guest, 1, 8192);
      if (preset) return std::clamp(selected, 1, 8192);
    }
    return fallback;
  };
  return {axis("window_width", "video_mode_width", preset_x, 1280),
          axis("window_height", "video_mode_height", preset_y, 720)};
}

Values RegistryValues() {
  Values values;
  for (const auto& [id, name] : kSingleFlags)
    values[id] = Boolean(id) ? int32_t(rex::cvar::Query<bool>(name)) : rex::cvar::Query<int32_t>(name);
  const auto automatic = AutomaticSize();
  int32_t width = rex::cvar::Query<int32_t>("host_window_width");
  int32_t height = rex::cvar::Query<int32_t>("host_window_height");
  if (width || height) { if (!width) width = automatic.first; if (!height) height = automatic.second; }
  if (width < 0 || width > 8192 || height < 0 || height > 8192)
    throw std::runtime_error("Window preference is out of range");
  values[WindowSize] = (width << 16) | height;
  const int32_t shared = std::max(1, rex::cvar::Query<int32_t>("resolution_scale"));
  int32_t x = rex::cvar::Query<int32_t>("draw_resolution_scale_x");
  int32_t y = rex::cvar::Query<int32_t>("draw_resolution_scale_y");
  if (shared != 1 && x == 1) x = shared;
  if (shared != 1 && y == 1) y = shared;
  values[RenderScale] = std::clamp(x, 1, 7) * 16 + std::clamp(y, 1, 7);
  return values;
}

void IncludeCurrentChoices(Catalog& catalog) {
  const int32_t size = catalog.host_values.at(WindowSize);
  if (!Contains(catalog.window_sizes, size)) {
    const auto label = Replace(catalog.text.at("dimensions"), "{width}", size >> 16);
    catalog.window_sizes.push_back({size, Replace(label, "{height}", size & 65535)});
  }
  const int32_t scale = catalog.host_values.at(RenderScale);
  if (!Contains(catalog.render_scales, scale)) {
    catalog.render_scales.push_back({scale,
        Replace(Replace(catalog.text.at("scale_axes"), "{x}", (scale / 16) * 100), "{y}", (scale % 16) * 100)});
  }
}

bool ConfigureCues(const Values& values) {
#ifdef BRAWLERS_SPATIAL_AUDIO
  auto& audio = brawlers::audio::AccessibilityAudio();
  const bool friendly = audio.Configure(brawlers::audio::Cue::Friendly,
      values.at(FriendlyEnabled) != 0, values.at(FriendlyVolume) / 100.0f);
  const bool enemy = audio.Configure(brawlers::audio::Cue::Enemy,
      values.at(EnemyEnabled) != 0, values.at(EnemyVolume) / 100.0f);
  return friendly && enemy;
#else
  (void)values;
  return true;
#endif
}

}  // namespace

struct HostSettingsRuntime::State : rex::ui::WindowListener,
                                    std::enable_shared_from_this<State> {
  struct SaveJob { uint64_t generation; ConfigValues values; };
  struct Pending { uint64_t generation; int32_t value; bool requested = false; };
  struct FlagScope {
    std::atomic<bool>& flag;
    explicit FlagScope(std::atomic<bool>& value) : flag(value) { flag.store(true); }
    ~FlagScope() { flag.store(false); }
  };
  mutable std::mutex mutex;
  std::condition_variable wake;
  bool configured = false, stopping = false, scheduled = false;
  bool saving = false, display_pending = false;
  std::atomic<bool> registry_dirty{false}, applying{false}, refreshing{false};
  std::atomic<uint64_t> catalog_revision{0};
  uint64_t last_generation = 0;
  Catalog catalog;
  std::deque<Command> commands;
  std::deque<HostEvent> events;
  std::deque<SaveJob> saves;
  std::thread worker;
  rex::ui::WindowedAppContext* context = nullptr;
  rex::ui::Window* window = nullptr;
  rex::ui::Presenter* presenter = nullptr;
  std::filesystem::path path;
  // UI-thread-owned, never inspected by the worker or the guest thread.
  std::map<Id, Pending> pending;
  ConfigValues desired_config;

  void Emit(HostEvent event) {
    std::lock_guard lock(mutex);
    events.push_back(std::move(event));
  }
  bool ScheduleLocked() {
    if (scheduled) return true;
    const std::weak_ptr<State> weak = shared_from_this();
    scheduled = context->CallInUIThreadDeferred([weak] {
      if (auto state = weak.lock()) state->Update();
    });
    return scheduled;
  }
  void Schedule() {
    std::lock_guard lock(mutex);
    if (configured && !stopping) ScheduleLocked();
  }
  void OnClosing(rex::ui::UIEvent&) override {
    // SDK calls may dispatch close events synchronously. Detach while the
    // window still exists, then make subsequent work in this stack a no-op.
    if (window) window->RemoveListener(this);
    window = nullptr; presenter = nullptr;
    std::lock_guard lock(mutex);
    configured = false;
  }
  void OnResize(rex::ui::UISetupEvent&) override { Schedule(); }
  void OnRestored(rex::ui::UIEvent&) override { Schedule(); }
  void OnGotFocus(rex::ui::UISetupEvent&) override { Schedule(); }

  void SaveLoop() {
    for (;;) {
      SaveJob job;
      {
        std::unique_lock lock(mutex);
        wake.wait(lock, [&] { return stopping || !saves.empty(); });
        if (saves.empty()) return;
        job = std::move(saves.front());
        saves.pop_front();
        saving = true;
      }
      rex::cvar::ConfigSaveResult result;
      try { result = rex::cvar::SaveConfigChecked(path, job.values); }
      catch (const std::exception& error) { result.error = error.what(); }
      {
        std::lock_guard lock(mutex);
        events.push_back({job.generation, result.saved ? HostEventKind::Saved : HostEventKind::SaveFailed,
                          0, false, std::move(result.error)});
        saving = false;
      }
    }
  }

  bool Validate(const Command& command, const Catalog& current, Values& next) {
    if (!command.generation || command.changes.empty() ||
        (command.kind != CommandKind::Adjust && command.kind != CommandKind::Reset)) return false;
    if (command.kind == CommandKind::Adjust &&
        (command.changes.size() != 1 || command.row != command.changes.front().row)) return false;
    next = current.host_values;
    std::set<Id> changed;
    for (const auto& change : command.changes) {
      const auto it = next.find(change.row);
      if (it == next.end() || !changed.insert(change.row).second || it->second != change.before) return false;
      const int32_t value = change.after;
      if (Boolean(change.row)) { if (value != 0 && value != 1) return false; }
      else if (change.row == WindowSize) { if (!Contains(current.window_sizes, value)) return false; }
      else if (change.row == RenderScale) { if (!Contains(current.render_scales, value)) return false; }
      else if (!Contains(current.cue_volumes, value)) return false;
      if (command.kind == CommandKind::Reset && value != current.host_defaults.at(change.row)) return false;
      it->second = value;
    }
    return true;
  }

  void Commit(const Command& command) {
    Catalog current;
    { std::lock_guard lock(mutex); current = catalog; }
    Values next;
    if (!Validate(command, current, next)) {
      Emit({command.generation, HostEventKind::Rejected, 0, false, "Invalid or stale host settings command"});
      return;
    }
    auto snapshot = Snapshot(next);
    std::set<std::string> selected;
    for (const auto& change : command.changes) {
      if (change.row == WindowSize) {
        selected.insert("host_window_width"); selected.insert("host_window_height");
      } else if (change.row == RenderScale) {
        selected.insert("resolution_scale"); selected.insert("draw_resolution_scale_x");
        selected.insert("draw_resolution_scale_y");
      } else selected.insert(kSingleFlags.at(change.row));
    }
    std::erase_if(snapshot, [&](const auto& item) { return !selected.count(item.name); });
    bool restart = false;
    for (const auto& change : command.changes) restart |= change.row == RenderScale;
    // Carry every owned desired value forward, including previous failed saves.
    // Initial raw values are retained until their setting is actually edited.
    auto save_values = desired_config;
    for (const auto& value : snapshot)
      for (auto& saved : save_values) if (saved.name == value.name) saved.value = value.value;
    FlagScope guard(applying);
    // Publish the desired transaction before any platform callback. A rejected
    // OS request is an application failure of this accepted preference.
    {
      std::lock_guard lock(mutex);
      saves.push_back({command.generation, save_values});
      catalog.host_values = next;
      catalog_revision.fetch_add(1);
      events.push_back({command.generation, HostEventKind::Committed, 0, restart, {}});
    }
    desired_config = std::move(save_values);
    wake.notify_one();
    // Fullscreen uses the existing registered callback as its only apply
    // path. The checked SDK state below exposes its result without retrying.
    for (const auto& change : command.changes) {
      if (change.row == Fullscreen || change.row == WindowSize || change.row == VSync) {
        pending[change.row] = {command.generation, change.after, false};
      }
    }
    // Reapply the saved window preference after a confirmed fullscreen exit,
    // including a size loaded at startup or applied before entering fullscreen.
    if (pending.count(Fullscreen)) {
      if (!pending.at(Fullscreen).value)
        pending[WindowSize] = {command.generation, next.at(WindowSize), false};
      else if (pending.count(WindowSize)) pending.at(WindowSize).requested = false;
    }
    if (!window) return;
    for (const auto& value : snapshot) {
      if (!rex::cvar::SetFlagByName(value.name, value.value)) {
        Emit({command.generation, HostEventKind::ApplyFailed, 0, restart,
              "Registered setting rejected its validated value: " + value.name});
      }
    }
    const bool cues_changed = std::any_of(command.changes.begin(), command.changes.end(),
        [](const Change& change) { return change.row >= FriendlyEnabled && change.row <= EnemyVolume; });
    if (cues_changed && !ConfigureCues(next)) {
      Emit({command.generation, HostEventKind::ApplyFailed, 0, false, "Gate sound controls rejected values"});
    }
    wake.notify_one();
  }

  void Finish(Id row, bool success, std::string error = {}) {
    auto it = pending.find(row);
    if (it == pending.end()) return;
    Emit({it->second.generation, success ? HostEventKind::Applied : HostEventKind::ApplyFailed,
          row, false, std::move(error)});
    pending.erase(it);
  }
  void ObserveFullscreen() {
    const auto request = pending.at(Fullscreen);
    const auto state = window->GetDisplayStateFromUIThread();
    if (!state.available) Finish(Fullscreen, false, state.error);
    else if (!state.fullscreen_error.empty()) Finish(Fullscreen, false, state.fullscreen_error);
    else if (!state.fullscreen_pending && state.fullscreen == (request.value != 0))
      Finish(Fullscreen, true);
    else if (!state.fullscreen_pending || state.requested_fullscreen != (request.value != 0))
      Finish(Fullscreen, false, "Fullscreen callback did not accept the requested preference");
  }
  void CheckDisplay() {
    if (!window || !presenter) return;
    if (pending.count(Fullscreen)) ObserveFullscreen();
    if (!window || !presenter) return;
    auto state = window->GetDisplayStateFromUIThread();
    if (!state.available) {
      for (Id id : {Id(Fullscreen), Id(WindowSize)}) if (pending.count(id)) Finish(id, false, state.error);
    } else {
      if (pending.count(WindowSize) && !state.fullscreen && !state.fullscreen_pending) {
        auto& request = pending.at(WindowSize);
        const auto automatic = AutomaticSize();
        const uint32_t size = static_cast<uint32_t>(request.value);
        const uint32_t width = size ? size >> 16 : automatic.first;
        const uint32_t height = size ? size & 65535 : automatic.second;
        if (!request.requested) {
          const auto result = window->RequestWindowSizeFromUIThread(width, height);
          request.requested = true;
          if (!result.accepted || !result.error.empty()) Finish(WindowSize, false, result.error);
          else if (result.applied) Finish(WindowSize, true);
        } else if (!state.size_pending && state.logical_width == width && state.logical_height == height) {
          Finish(WindowSize, true);
        }
      }
    }
    if (!window || !presenter) return;
    if (pending.count(VSync)) {
      auto& request = pending.at(VSync);
      if (!request.requested) {
        const auto result = presenter->RequestHostVSyncFromUIThread(request.value != 0);
        request.requested = true;
        if (!result.accepted) Finish(VSync, false, result.error);
        else if (result.applied) Finish(VSync, true);
      } else {
        const auto presentation = presenter->GetHostPresentationStateFromUIThread();
        if (!presentation.error.empty()) Finish(VSync, false, presentation.error);
        else if (presentation.has_applied_preference && !presentation.pending &&
                 presentation.applied_vsync_preference == (request.value != 0)) Finish(VSync, true);
      }
    }
  }
  void RefreshRegistry() {
    if (!registry_dirty.load()) return;
    FlagScope guard(refreshing);
    if (!registry_dirty.exchange(false)) return;
    Catalog next;
    { std::lock_guard lock(mutex); next = catalog; }
    const auto before = next.host_values;
    next.host_values = RegistryValues();
    IncludeCurrentChoices(next);
    for (auto& value : desired_config) value.value = rex::cvar::GetFlagByName(value.name);
    // The raw host menu may change these flags too. Cancel superseded display
    // requests and apply cue controls from the complete current registry state.
    for (const auto& [id, value] : next.host_values)
      if (value != before.at(id)) pending.erase(id);
    const bool cues_changed = before.at(FriendlyEnabled) != next.host_values.at(FriendlyEnabled) ||
        before.at(FriendlyVolume) != next.host_values.at(FriendlyVolume) ||
        before.at(EnemyEnabled) != next.host_values.at(EnemyEnabled) ||
        before.at(EnemyVolume) != next.host_values.at(EnemyVolume);
    if (cues_changed && !ConfigureCues(next.host_values))
      Emit({0, HostEventKind::ApplyFailed, 0, false, "Gate sound controls rejected registry preferences"});
    {
      std::lock_guard lock(mutex);
      catalog = std::move(next);
      catalog_revision.fetch_add(1);
    }
  }
  void Update() {
    {
      std::lock_guard lock(mutex);
      scheduled = false;
      if (!configured || stopping || applying.load() || refreshing.load() ||
          !context->IsInUIThread()) return;
    }
    // A queued function can be drained during drawing. Leave work queued for
    // the next external Poll rather than recursively scheduling paint work.
    if (!presenter || !presenter->CanChangeSurfaceFromUIThread()) return;
    RefreshRegistry();
    std::deque<Command> work;
    { std::lock_guard lock(mutex); work.swap(commands); }
    for (const auto& command : work) Commit(command);
    CheckDisplay();
    { std::lock_guard lock(mutex); display_pending = !pending.empty(); }
  }
};

HostSettingsRuntime::HostSettingsRuntime() : state_(std::make_shared<State>()) {}
HostSettingsRuntime::~HostSettingsRuntime() { Shutdown(); }

bool HostSettingsRuntime::Configure(rex::Runtime* runtime, rex::ui::WindowedAppContext* context,
                                   rex::ui::Window* window, rex::ui::Presenter* presenter,
                                   const std::filesystem::path& config_path,
                                   const std::filesystem::path& installation, std::string* error) {
  auto& state = *state_;
  const auto fail = [&](const std::string& detail) { if (error) *error = detail; return false; };
  if (!runtime || !context || !window || !presenter || !context->IsInUIThread() || config_path.empty())
    return fail("Missing host settings owners or incorrect thread");
  { std::lock_guard lock(state.mutex); if (state.configured || state.stopping) return fail("Settings already configured or stopped"); }
  try {
    Catalog catalog;
    std::ifstream input(installation / "messages.en.json");
    const auto document = nlohmann::json::parse(input);
    for (auto it = document.at("settings").begin(); it != document.at("settings").end(); ++it)
      catalog.text.emplace(it.key(), it.value().get<std::string>());
    for (const auto& [id, name] : kSingleFlags) {
      const auto* entry = rex::cvar::GetFlagInfo(name);
      if (!entry) return fail(std::string("Missing registered setting: ") + name);
      catalog.host_values[id] = Boolean(id) ? int32_t(rex::cvar::Query<bool>(name)) : rex::cvar::Query<int32_t>(name);
      catalog.host_defaults[id] = Boolean(id) ? int32_t(entry->default_value == "true") : std::stoi(entry->default_value);
    }
    for (const char* name : {"host_window_width", "host_window_height", "resolution_scale",
                             "draw_resolution_scale_x", "draw_resolution_scale_y"}) {
      if (!rex::cvar::GetFlagInfo(name)) return fail(std::string("Missing registered setting: ") + name);
    }
    catalog.host_values = RegistryValues();
    catalog.host_defaults[WindowSize] = 0;
    catalog.window_sizes.push_back({0, catalog.text.at("automatic")});
    const auto add_size = [&](int32_t encoded) {
      if (Contains(catalog.window_sizes, encoded)) return;
      auto label = Replace(catalog.text.at("dimensions"), "{width}", encoded >> 16);
      catalog.window_sizes.push_back({encoded, Replace(label, "{height}", encoded & 65535)});
    };
    for (const auto& size : std::vector<std::pair<int32_t, int32_t>>{
             {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}})
      add_size((size.first << 16) | size.second);
    add_size(catalog.host_values.at(WindowSize));
    catalog.host_defaults[RenderScale] = 17;
    for (int32_t scale = 1; scale <= 7; ++scale)
      catalog.render_scales.push_back({scale * 17, Replace(catalog.text.at("percent"), "{value}", scale * 100)});
    IncludeCurrentChoices(catalog);
    for (int32_t value = 1; value <= 10; ++value)
      catalog.native_volumes.push_back({value, Replace(catalog.text.at("percent"), "{value}", ((value - 1) * 100 + 4) / 9)});
    for (int32_t value = 0; value <= 100; value += 10)
      catalog.cue_volumes.push_back({value, Replace(catalog.text.at("percent"), "{value}", value)});
    if (!Contains(catalog.cue_volumes, catalog.host_values.at(FriendlyVolume)) ||
        !Contains(catalog.cue_volumes, catalog.host_values.at(EnemyVolume))) return fail("Gate volume is not a supported increment");
#ifdef BRAWLERS_SPATIAL_AUDIO
    if (brawlers::audio::AccessibilityAudio().Running()) catalog.text.erase("cue_unavailable");
#endif
    if (!ConfigureCues(catalog.host_values)) return fail("Gate sound controls rejected initial preferences");
    state.desired_config = Snapshot(catalog.host_values);
    for (auto& value : state.desired_config) value.value = rex::cvar::GetFlagByName(value.name);
    state.context = context; state.window = window; state.presenter = presenter; state.path = config_path;
    { std::lock_guard lock(state.mutex); state.catalog = std::move(catalog); }
    state.worker = std::thread([shared_state = state_] { shared_state->SaveLoop(); });
    window->AddListener(&state);
    {
      std::lock_guard lock(state.mutex);
      state.catalog_revision.store(1);
      state.configured = true;
    }
    const std::weak_ptr<State> weak = state_;
    for (const auto& value : state.desired_config) {
      rex::cvar::RegisterChangeCallback(value.name, [weak](std::string_view, std::string_view) {
        if (auto shared = weak.lock()) {
          shared->registry_dirty.store(true);
          shared->catalog_revision.fetch_add(1);
          shared->Schedule();
        }
      });
    }
    return true;
  } catch (const std::exception& exception) {
    Shutdown();
    return fail(exception.what());
  }
}

std::optional<Catalog> HostSettingsRuntime::ReadCatalog() const {
  Catalog catalog;
  {
    std::lock_guard lock(state_->mutex);
    if (!state_->configured || state_->stopping) return std::nullopt;
    catalog = state_->catalog;
  }
  // Read-only registry access is synchronized by the SDK. This covers an
  // external flag change whose deferred UI refresh has not executed yet.
  if ((state_->registry_dirty.load() || state_->refreshing.load()) && !state_->applying.load()) {
    try { catalog.host_values = RegistryValues(); IncludeCurrentChoices(catalog); }
    catch (const std::exception&) { return std::nullopt; }
  }
  return catalog;
}
uint64_t HostSettingsRuntime::CatalogRevision() const {
  std::lock_guard lock(state_->mutex);
  if (!state_->configured || state_->stopping) return 0;
  return state_->catalog_revision.load();
}
std::optional<HostCatalogSnapshot> HostSettingsRuntime::ReadCatalogSnapshot() const {
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const uint64_t before = CatalogRevision();
    if (!before) return std::nullopt;
    auto catalog = ReadCatalog();
    if (!catalog) return std::nullopt;
    if (CatalogRevision() == before)
      return HostCatalogSnapshot{std::move(*catalog), before};
  }
  return std::nullopt;
}
bool HostSettingsRuntime::Submit(const Command& command) {
  std::lock_guard lock(state_->mutex);
  if (!state_->configured || state_->stopping || !command.generation ||
      command.generation <= state_->last_generation || !state_->commands.empty()) return false;
  state_->commands.push_back(command);
  if (!state_->ScheduleLocked()) { state_->commands.pop_back(); return false; }
  state_->last_generation = command.generation;
  return true;
}
std::optional<HostEvent> HostSettingsRuntime::Poll() {
  std::lock_guard lock(state_->mutex);
  if (state_->configured && !state_->stopping &&
      (!state_->commands.empty() || state_->display_pending || state_->registry_dirty.load())) state_->ScheduleLocked();
  if (state_->events.empty()) return std::nullopt;
  auto event = std::move(state_->events.front()); state_->events.pop_front(); return event;
}
bool HostSettingsRuntime::HasPendingWork() const {
  std::lock_guard lock(state_->mutex);
  return !state_->commands.empty() || !state_->events.empty() || !state_->saves.empty() ||
         state_->saving || state_->display_pending ||
         (state_->configured && state_->registry_dirty.load());
}
void HostSettingsRuntime::UpdateFromUIThread() { state_->Update(); }
void HostSettingsRuntime::Shutdown() {
  auto& state = *state_;
  bool configured;
  {
    std::lock_guard lock(state.mutex);
    configured = state.configured;
    state.configured = false; state.stopping = true; state.display_pending = false;
    for (const auto& command : state.commands)
      state.events.push_back({command.generation, HostEventKind::Rejected, 0, false, "Settings owner is stopping"});
    state.commands.clear();
  }
  if (configured && state.window) state.window->RemoveListener(&state);
  state.wake.notify_one();
  if (state.worker.joinable()) state.worker.join();
}

}  // namespace brawlers::settings
