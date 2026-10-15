#!/usr/bin/env python3
"""Compile the actual host settings adapter with synthetic SDK/audio boundaries.

Usage: settings_runtime_regressions.py PATCHED_SDK DEPENDENCIES NEW_OUTPUT_DIRECTORY
Linux/bubblewrap required. No window, GPU, audio library, bus or game is loaded.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def inside():
    assert os.environ.get("SETTINGS_TEST_ISOLATED") == "1"
    assert not Path("/run/user").exists() and not Path("/dev/dri").exists()
    assert not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY")
    output = Path("/output")
    stubs = output / "stubs"
    for directory in ("rex/ui", "rex/graphics"):
        (stubs / directory).mkdir(parents=True)
    (stubs / "rex/cvar.h").write_text(r'''
#pragma once
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
namespace rex { class Runtime {}; }
namespace rex::cvar {
struct FlagEntry { std::string default_value, value; };
inline std::map<std::string, FlagEntry> registry;
inline std::mutex save_mutex;
inline std::condition_variable save_gate;
inline bool block_save=false, save_entered=false;
inline std::atomic<unsigned> registry_reads{0};
struct ConfigSaveValue { std::string name, value; };
struct ConfigSaveResult { bool saved = false; std::string error; };
inline std::vector<std::vector<ConfigSaveValue>> saved;
inline bool fail_save = false;
inline bool fail_set = false;
inline std::map<std::string,std::vector<std::function<void(std::string_view,std::string_view)>>> callbacks;
inline void RegisterChangeCallback(std::string_view name,std::function<void(std::string_view,std::string_view)> fn) {callbacks[std::string(name)].push_back(std::move(fn));}
inline auto fullscreen_callback = std::function<void(bool)>();
struct Registrar {
 Registrar(const char* name, const char* value) { registry.emplace(name, FlagEntry{value,value}); }
 Registrar& range(int,int) { return *this; }
 Registrar& allowed(std::initializer_list<std::string>) { return *this; }
};
inline const FlagEntry* GetFlagInfo(std::string_view name) {
 ++registry_reads; auto it=registry.find(std::string(name)); return it==registry.end()?nullptr:&it->second;
}
inline std::string GetFlagByName(std::string_view name) {++registry_reads;return registry.at(std::string(name)).value;}
template<class T> T Query(std::string_view name) {
 const auto* entry=GetFlagInfo(name); if(!entry) return {};
 if constexpr(std::is_same_v<T,bool>) return entry->value=="true";
 else return std::stoi(entry->value);
}
inline bool HasNonDefaultValue(std::string_view name) {
 const auto* entry=GetFlagInfo(name); return entry && entry->value!=entry->default_value;
}
inline bool SetFlagByName(std::string_view name,std::string_view value) {
 if(fail_set) return false;
 auto it=registry.find(std::string(name)); if(it==registry.end()) return false;
 it->second.value=value;
 if(name=="fullscreen" && fullscreen_callback) fullscreen_callback(value=="true");
 for(auto& callback:callbacks[std::string(name)]) callback(name,value);
 return true;
}
inline ConfigSaveResult SaveConfigChecked(const std::filesystem::path&, const std::vector<ConfigSaveValue>& values) {
 std::unique_lock lock(save_mutex); save_entered=true;save_gate.notify_all();
 save_gate.wait(lock,[]{return !block_save;});saved.push_back(values);
 return {!fail_save, fail_save?"synthetic replace failure":""};
}
}
#define REXCVAR_DEFINE_BOOL(n,d,c,e) static rex::cvar::Registrar reg_##n(#n,d?"true":"false")
#define REXCVAR_DEFINE_INT32(n,d,c,e) static auto reg_##n=rex::cvar::Registrar(#n,#d)
''')
    (stubs / "rex/ui/windowed_app_context.h").write_text(r'''
#pragma once
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
namespace rex::ui {
class WindowedAppContext {
 std::mutex mutex; std::deque<std::function<void()>> queue;
 public:
 std::thread::id owner=std::this_thread::get_id(); bool accepts=true;
 bool IsInUIThread() const { return owner==std::this_thread::get_id(); }
 bool CallInUIThreadDeferred(std::function<void()> fn) {
  std::lock_guard lock(mutex); if(!accepts) return false; queue.push_back(std::move(fn)); return true;
 }
 void Pump() {
  std::deque<std::function<void()>> work;
  { std::lock_guard lock(mutex); work.swap(queue); }
  for(auto& fn:work) fn();
 }
};
}
''')
    (stubs / "rex/ui/window_listener.h").write_text(r'''
#pragma once
namespace rex::ui {
struct UIEvent {}; struct UISetupEvent : UIEvent {};
struct WindowListener {
 virtual ~WindowListener()=default;
 virtual void OnClosing(UIEvent&) {};
 virtual void OnResize(UISetupEvent&) {};
 virtual void OnRestored(UIEvent&) {};
 virtual void OnGotFocus(UISetupEvent&) {};
};
}
''')
    (stubs / "rex/ui/window.h").write_text(r'''
#pragma once
#include <rex/ui/display_settings.h>
#include <rex/ui/window_listener.h>
namespace rex::ui {
class Window {
 public:
 WindowDisplayState state{true};
 WindowListener* listener=nullptr;
 bool reject_fullscreen=false, reject_size=false, desired=false;
 bool reject_first_only=false;
 int fullscreen_calls=0,size_calls=0;
 WindowDisplayState GetDisplayStateFromUIThread() const { return state; }
 DisplayChangeResult RequestFullscreenFromUIThread(bool value) {
  ++fullscreen_calls;
  if(reject_fullscreen || (reject_first_only && fullscreen_calls==1)) {
   state.fullscreen_error="synthetic fullscreen rejection";
   return {false,false,state.fullscreen_error};
  }
  state.fullscreen_error.clear();
  desired=value; state.requested_fullscreen=value;
  state.fullscreen_pending=state.fullscreen!=value;
  return {true,!state.fullscreen_pending,{}};
 }
 void SetFullscreen(bool value) {
  if(desired==value) return;
  desired=value;
  auto result=RequestFullscreenFromUIThread(value);
  if(!result.accepted) desired=state.fullscreen;
 }
 DisplayChangeResult RequestWindowSizeFromUIThread(uint32_t width,uint32_t height) {
  ++size_calls;
  if(reject_size) return {false,false,"synthetic window size rejection"};
  state.requested_logical_width=width; state.requested_logical_height=height;
  state.size_pending=state.logical_width!=width || state.logical_height!=height;
  return {true,!state.size_pending,{}};
 }
 void AddListener(WindowListener* value) {listener=value;}
 void RemoveListener(WindowListener* value) {assert(listener==value);listener=nullptr;}
};
}
''')
    (stubs / "rex/ui/presenter.h").write_text(r'''
#pragma once
#include <rex/ui/display_settings.h>
namespace rex::ui {
class Presenter {
 public:
 HostPresentationState state{true};
 bool painting=false,reject=false; int calls=0;
 bool CanChangeSurfaceFromUIThread() const {return !painting;}
 DisplayChangeResult RequestHostVSyncFromUIThread(bool value) {
  ++calls;
  if(reject) return {false,false,"synthetic presentation rejection"};
  state.requested_vsync=value; state.pending=true; state.error.clear(); return {true,false,{}};
 }
 HostPresentationState GetHostPresentationStateFromUIThread() const {return state;}
};
}
''')
    (stubs / "rex/graphics/video_mode_util.h").write_text(r'''
#pragma once
namespace rex::graphics::video_mode_util {
inline bool TryGetResolutionPresetFromCVar(int32_t&,int32_t&) {return false;}
}
''')
    harness = output / "test.cpp"
    native_host = Path("/project/src/runtime/native_host.cpp").read_text()
    close_methods = "\n".join(function(native_host, signature).replace(" override", "") for signature in
                              ("void DrainSettings()", "bool OnWindowCloseRequested()"))
    harness.write_text(r'''
#include <cassert>
#include <functional>
#include <chrono>
#include <iostream>
#include "/project/src/runtime/settings_runtime.cpp"
#ifdef BRAWLERS_SPATIAL_AUDIO
namespace brawlers::audio {
struct SpatialAudioRuntime::Impl {};
static float friendly_volume=1,enemy_volume=1;
static bool friendly_enabled=true,enemy_enabled=true;
SpatialAudioRuntime::SpatialAudioRuntime()=default;
SpatialAudioRuntime::~SpatialAudioRuntime()=default;
bool SpatialAudioRuntime::Running() const noexcept {return true;}
bool SpatialAudioRuntime::Configure(Cue cue,bool enabled,float volume) noexcept {
 if(cue==Cue::Friendly) {friendly_volume=volume;friendly_enabled=enabled;}
 else {enemy_volume=volume;enemy_enabled=enabled;}
 return volume>=0 && volume<=1;
}
SpatialAudioRuntime& AccessibilityAudio() {static SpatialAudioRuntime instance;return instance;}
}
#endif
using namespace brawlers::settings;
using rex::cvar::registry;
static std::vector<std::string> close_reports;
void Report(const char* value) { close_reports.emplace_back(value); }
struct CloseOwner {
 HostSettingsRuntime settings_;
 @CLOSE_METHODS@
};
std::vector<HostEvent> Drain(HostSettingsRuntime& runtime) {
 std::vector<HostEvent> result;
 while(auto event=runtime.Poll()) result.push_back(*event);
 return result;
}
bool Has(const std::vector<HostEvent>& events,HostEventKind kind,Id row=0) {
 return std::any_of(events.begin(),events.end(),[&](auto& event){return event.kind==kind && event.row==row;});
}
HostEvent AwaitSave(HostSettingsRuntime& runtime,uint64_t generation) {
 for(int i=0;i<1000;++i) {
  while(auto event=runtime.Poll()) if(event->generation==generation &&
   (event->kind==HostEventKind::Saved || event->kind==HostEventKind::SaveFailed)) return *event;
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 assert(false); return {};
}
int main() {
 const auto add=[](const char* name,const char* value){registry.emplace(name,rex::cvar::FlagEntry{value,value});};
 add("fullscreen","true");add("host_vsync","false");
 for(auto name:{"host_window_width","host_window_height","window_width","window_height"}) add(name,"0");
 for(auto name:{"resolution_scale","draw_resolution_scale_x","draw_resolution_scale_y"}) add(name,"1");
 add("video_mode_width","1280");add("video_mode_height","720");
 registry.at("resolution_scale").value="2"; registry.at("draw_resolution_scale_y").value="3";
 rex::Runtime runtime;rex::ui::WindowedAppContext context;rex::ui::Window window;rex::ui::Presenter presenter;
 window.state.fullscreen=true;window.desired=true;window.state.logical_width=1280;window.state.logical_height=720;
 rex::cvar::fullscreen_callback=[&](bool value){window.SetFullscreen(value);};
 HostSettingsRuntime host;
 assert(!host.ReadCatalog() && !host.ReadCatalogSnapshot() && !host.CatalogRevision());std::string error;
 assert(host.Configure(&runtime,&context,&window,&presenter,"/output/config.toml","/project/resources",&error));
 auto catalog=*host.ReadCatalog();
 const auto initial_snapshot=host.ReadCatalogSnapshot();
 assert(initial_snapshot && initial_snapshot->revision==host.CatalogRevision() && initial_snapshot->revision!=0);
 const auto reads=rex::cvar::registry_reads.load();
 for(int i=0;i<1000;++i) assert(host.CatalogRevision()==initial_snapshot->revision);
 assert(rex::cvar::registry_reads.load()==reads);
 assert(catalog.host_values.at(RenderScale)==35 && catalog.host_defaults.at(RenderScale)==17);
 assert(catalog.native_volumes.front().label=="0%" && catalog.native_volumes[4].label=="44%" && catalog.native_volumes[5].label=="56%" && catalog.native_volumes.back().label=="100%");
#ifdef BRAWLERS_SPATIAL_AUDIO
 assert(!catalog.text.count("cue_unavailable"));
#else
 assert(catalog.text.count("cue_unavailable"));
#endif
 // Native changes and mixed batches are rejected without mutation or saving.
 assert(host.Submit({1,CommandKind::Adjust,Music,{{Music,10,9}}}));context.Pump();
 assert(Has(Drain(host),HostEventKind::Rejected));
 assert(host.Submit({2,CommandKind::Reset,0,{{FriendlyVolume,100,0},{Music,10,9}}}));context.Pump();
 assert(Has(Drain(host),HostEventKind::Rejected));assert(host.ReadCatalog()->host_values.at(FriendlyVolume)==100);
 // Fullscreen window-size changes persist without forcing any OS state change.
 const int32_t size=(1920<<16)|1080;
 std::thread guest([&]{assert(host.Submit({3,CommandKind::Adjust,WindowSize,{{WindowSize,0,size}}}));});guest.join();
 context.Pump();assert(window.size_calls==0 && window.fullscreen_calls==0);
 assert(host.ReadCatalog()->host_values.at(WindowSize)==size);
 assert(AwaitSave(host,3).kind==HostEventKind::Saved);
 {std::lock_guard lock(rex::cvar::save_mutex);assert(rex::cvar::saved.back().size()==11);}
 // Leaving fullscreen waits for its event before requesting the stored size.
 assert(host.Submit({4,CommandKind::Adjust,Fullscreen,{{Fullscreen,1,0}}}));context.Pump();
 assert(window.fullscreen_calls==1 && window.size_calls==0);
 assert(AwaitSave(host,4).kind==HostEventKind::Saved);
 window.state.fullscreen=false;window.state.fullscreen_pending=false;
 host.UpdateFromUIThread();assert(window.size_calls==1);
 auto events=Drain(host);assert(Has(events,HostEventKind::Applied,Fullscreen));assert(!Has(events,HostEventKind::Applied,WindowSize));
 window.state.size_pending=false;window.state.logical_width=1920;window.state.logical_height=1080;
 host.UpdateFromUIThread();assert(Has(Drain(host),HostEventKind::Applied,WindowSize));
 // Presenter acceptance is not proof of an applied preference. Reconnect errors
 // remain explicit; a later command can retry and succeed.
 assert(host.Submit({5,CommandKind::Adjust,VSync,{{VSync,0,1}}}));context.Pump();
 assert(AwaitSave(host,5).kind==HostEventKind::Saved);
 presenter.state.error="synthetic reconnect failure";presenter.state.pending=false;
 host.UpdateFromUIThread();assert(Has(Drain(host),HostEventKind::ApplyFailed,VSync));
 assert(host.Submit({6,CommandKind::Adjust,VSync,{{VSync,1,0}}}));context.Pump();
 assert(AwaitSave(host,6).kind==HostEventKind::Saved);
 presenter.state.pending=false;presenter.state.has_applied_preference=true;presenter.state.applied_vsync_preference=false;
 host.UpdateFromUIThread();assert(Has(Drain(host),HostEventKind::Applied,VSync));
 // Restart values coherently update all three registry flags; unrelated changes
 // do not canonicalize an existing asymmetric scale.
 assert(registry.at("resolution_scale").value=="2" && registry.at("draw_resolution_scale_y").value=="3");
 assert(host.Submit({7,CommandKind::Reset,0,{{RenderScale,35,17}}}));context.Pump();
 assert(registry.at("resolution_scale").value=="1" && registry.at("draw_resolution_scale_x").value=="1" && registry.at("draw_resolution_scale_y").value=="1");
 assert(AwaitSave(host,7).kind==HostEventKind::Saved);
 {std::lock_guard lock(rex::cvar::save_mutex);assert(rex::cvar::saved.back().size()==11);rex::cvar::fail_save=true;}
 assert(host.Submit({8,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,100,50}}}));context.Pump();
 assert(host.ReadCatalog()->host_values.at(FriendlyVolume)==50);
 assert(AwaitSave(host,8).kind==HostEventKind::SaveFailed);
#ifdef BRAWLERS_SPATIAL_AUDIO
 assert(brawlers::audio::friendly_volume==0.5f);
#endif
 {std::lock_guard lock(rex::cvar::save_mutex);rex::cvar::fail_save=false;}
 // Stale before-values and unsupported cue increments are rejected as a batch.
 assert(host.Submit({9,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,100,40}}}));context.Pump();assert(Has(Drain(host),HostEventKind::Rejected));
 assert(host.Submit({10,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,50,55}}}));context.Pump();assert(Has(Drain(host),HostEventKind::Rejected));
 assert(!host.Submit({10,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,50,40}}}));
 // Drawing defers the complete operation. Pending work survives menu closure
 // and resumes through the owner's Poll/Update entry point.
 presenter.painting=true;
 assert(host.Submit({11,CommandKind::Adjust,EnemyVolume,{{EnemyVolume,100,40}}}));context.Pump();
 assert(host.ReadCatalog()->host_values.at(EnemyVolume)==100 && host.HasPendingWork());
 presenter.painting=false;host.UpdateFromUIThread();assert(AwaitSave(host,11).kind==HostEventKind::Saved);
 {std::lock_guard lock(rex::cvar::save_mutex);
  auto& values=rex::cvar::saved.back();
  assert(std::any_of(values.begin(),values.end(),[](auto& value){return value.name=="accessibility_friendly_gate_volume" && value.value=="50";}));
 }
 // Reset commits the complete category batch and persists default values.
 assert(host.Submit({12,CommandKind::Reset,406,{{FriendlyVolume,50,100},{EnemyVolume,40,100}}}));context.Pump();
 assert(host.ReadCatalog()->host_values.at(FriendlyVolume)==100 && host.ReadCatalog()->host_values.at(EnemyVolume)==100);
 assert(AwaitSave(host,12).kind==HostEventKind::Saved);
 // The first OS request fails; a hidden second request would succeed. The
 // existing cvar callback must remain the sole apply path.
 window.reject_first_only=true;window.fullscreen_calls=0;
 assert(host.Submit({13,CommandKind::Adjust,Fullscreen,{{Fullscreen,0,1}}}));context.Pump();
 bool failed=false,applied=false,saved=false;
 for(int i=0;i<1000 && !saved;++i) {
  for(auto& event:Drain(host)) if(event.generation==13) {
   failed|=event.kind==HostEventKind::ApplyFailed && event.row==Fullscreen;
   applied|=event.kind==HostEventKind::Applied && event.row==Fullscreen;
   saved|=event.kind==HostEventKind::Saved;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 assert(failed && !applied && saved && window.fullscreen_calls==1);
 const auto before_external_revision=host.CatalogRevision();
 rex::cvar::SetFlagByName("accessibility_enemy_gate_volume","20");
 assert(host.CatalogRevision()>before_external_revision);
 const auto external_snapshot=host.ReadCatalogSnapshot();
 assert(external_snapshot && external_snapshot->revision==host.CatalogRevision() &&
        external_snapshot->catalog.host_values.at(EnemyVolume)==20);
 assert(host.ReadCatalog()->host_values.at(EnemyVolume)==20);
 assert(host.Submit({14,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,100,30}}}));context.Pump();
 assert(AwaitSave(host,14).kind==HostEventKind::Saved);
 assert(host.ReadCatalog()->host_values.at(EnemyVolume)==20);
#ifdef BRAWLERS_SPATIAL_AUDIO
 assert(brawlers::audio::enemy_volume==0.2f && brawlers::audio::friendly_volume==0.3f);
#endif
 assert(host.Submit({15,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,30,10}}}));
 host.Shutdown();context.Pump();assert(!host.ReadCatalog() && !host.ReadCatalogSnapshot() && !host.CatalogRevision() && !window.listener);
 assert(Has(Drain(host),HostEventKind::Rejected));
 assert(!host.Submit({16,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,30,10}}}));
 // Hold one accepted write in the synthetic filesystem boundary and queue
 // another. Invoke the actual extracted close hook, releasing the first only
 // after Shutdown starts. Both accepted jobs must finish before close returns.
 CloseOwner closing;
 assert(closing.settings_.Configure(&runtime,&context,&window,&presenter,"/output/config.toml","/project/resources",&error));
 size_t writes_before;
 {std::lock_guard lock(rex::cvar::save_mutex);
  writes_before=rex::cvar::saved.size();rex::cvar::block_save=true;rex::cvar::save_entered=false;
 }
 assert(closing.settings_.Submit({1,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,30,20}}}));context.Pump();
 {std::unique_lock lock(rex::cvar::save_mutex);
  rex::cvar::save_gate.wait(lock,[]{return rex::cvar::save_entered;});
 }
 assert(closing.settings_.Submit({2,CommandKind::Adjust,EnemyVolume,{{EnemyVolume,20,40}}}));context.Pump();
 std::thread release([&] {
  while(closing.settings_.CatalogRevision()!=0) std::this_thread::yield();
  {std::lock_guard lock(rex::cvar::save_mutex);rex::cvar::block_save=false;}
  rex::cvar::save_gate.notify_all();
 });
 assert(closing.OnWindowCloseRequested());release.join();context.Pump();
 {std::lock_guard lock(rex::cvar::save_mutex);assert(rex::cvar::saved.size()==writes_before+2);}
 assert(!closing.settings_.HasPendingWork() && close_reports.empty());
 CloseOwner failing_close;
 assert(failing_close.settings_.Configure(&runtime,&context,&window,&presenter,"/output/config.toml","/project/resources",&error));
 {std::lock_guard lock(rex::cvar::save_mutex);rex::cvar::fail_save=true;}
 assert(failing_close.settings_.Submit({1,CommandKind::Adjust,FriendlyVolume,{{FriendlyVolume,20,10}}}));context.Pump();
 assert(failing_close.OnWindowCloseRequested());context.Pump();
 assert(close_reports==std::vector<std::string>{"settings_save_failed_on_exit"});
 {std::lock_guard lock(rex::cvar::save_mutex);rex::cvar::fail_save=false;}
 std::cout<<"PASS: actual close hook drains accepted writes and reports failures; catalog revisions avoid unchanged registry reads.\n";
 std::cout<<"PASS: actual host adapter; desired/applied/saved separation, fullscreen deferred size, restart scale, native/mixed/stale rejection, save failure, queued lifetime and cue preferences.\n";
}
'''.replace('@CLOSE_METHODS@', close_methods))
    includes = [stubs, Path("/sdk/include"), Path("/dependencies/inja/third_party/include")]
    command = ["clang++", "-std=c++23", "-O1", "-g", "-pthread", "-fsanitize=address,undefined",
               "-fno-omit-frame-pointer"] + ["-I" + str(path) for path in includes]
    for audio in (False, True):
        target = output / ("with-audio" if audio else "without-audio")
        subprocess.run(command + (["-DBRAWLERS_SPATIAL_AUDIO=1"] if audio else []) +
                       [str(harness), "-o", str(target)], check=True)
        subprocess.run([str(target)], check=True)
    syntax = ["clang++", "-std=c++23", "-march=x86-64-v2", "-fsyntax-only", "-DREX_HAS_VULKAN=1",
              "-DREX_HAS_D3D12=0", "-DFMT_HEADER_ONLY", "-I/sdk/include"]
    syntax += ["-I/dependencies/" + directory for directory in
               ["sdl3/include", "vulkan-headers/include", "fmt/include", "spdlog/include",
                "simde", "imgui", "inja/third_party/include"]]
    for audio in (False, True):
        subprocess.run(syntax + (["-DBRAWLERS_SPATIAL_AUDIO=1"] if audio else []) +
                       ["/project/src/runtime/settings_runtime.cpp"], check=True)
    print("PASS: complete adapter compiles against real patched SDK headers, with and without audio support.")


def main():
    if sys.argv[1:] == ["--inside"]:
        inside()
        return
    if sys.platform != "linux" or len(sys.argv) != 4:
        raise SystemExit(__doc__)
    bwrap = shutil.which("bwrap")
    if not bwrap:
        raise SystemExit("bubblewrap required; no unisolated fallback")
    sdk, dependencies, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
    output.mkdir(exist_ok=False)
    project = Path(__file__).resolve().parents[2]
    command = [bwrap, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists(): command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file(): command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += ["--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
                "--ro-bind", str(sdk), "/sdk", "--ro-bind", str(dependencies), "/dependencies",
                "--ro-bind", str(project), "/project", "--bind", str(output), "/output", "--clearenv"]
    for key, value in {
        "PATH": "/usr/bin:/bin", "HOME": "/tmp/home", "XDG_CONFIG_HOME": "/tmp/config",
        "XDG_CACHE_HOME": "/tmp/cache", "XDG_DATA_HOME": "/tmp/data", "XDG_RUNTIME_DIR": "/run/test-runtime",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/run/test-runtime/session-bus",
        "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/run/test-runtime/system-bus",
        "AT_SPI_BUS_ADDRESS": "unix:path=/run/test-runtime/accessibility-bus", "NO_AT_BRIDGE": "1",
        "PYTHONDONTWRITEBYTECODE": "1", "PYTHONNOUSERSITE": "1", "SETTINGS_TEST_ISOLATED": "1",
    }.items(): command += ["--setenv", key, value]
    command += ["--chdir", "/output", "/usr/bin/python3", "/project/tests/runtime/settings_runtime_regressions.py", "--inside"]
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
