#!/usr/bin/env python3
"""Source-derived display boundary fixtures; never opens a window or GPU.

Usage: settings-display-regressions.py CLEAN_PINNED_SDK NEW_OUTPUT_DIRECTORY
Requires Linux, bubblewrap and clang++. Compilation and execution take place
inside a private mount/PID/network namespace with no host desktop, device,
accessibility, audio or bus sockets. There is no unisolated fallback.
"""

from pathlib import Path
import os
import io
import json
import tarfile
import shutil
import subprocess
import sys


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def inside():
    assert os.environ.get("DISPLAY_FIXTURE_ISOLATED") == "1"
    assert not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY")
    assert not Path("/run/user").exists() and not Path("/dev/dri").exists()
    for directory in ("/tmp/home", "/tmp/config", "/tmp/cache", "/tmp/data", "/run/test-runtime"):
        Path(directory).mkdir(mode=0o700, parents=True, exist_ok=True)
    sdk, output = Path("/sdk"), Path("/output")
    window = (sdk / "src/ui/window_sdl.cpp").read_text()
    presenter = (sdk / "src/ui/presenter.cpp").read_text()
    vulkan = (sdk / "src/ui/vulkan/vulkan_presenter.cpp").read_text()
    header = (sdk / "include/rex/ui/presenter.h").read_text()
    # Assert the tested methods are wired into the real lifecycle entry points.
    assert "RecordDisplayEvent(event);" in function(window, "void WindowSDL::HandleWindowEvent(")
    assert "ApplyPendingHostVSyncFromUIThread();" in function(presenter, "void Presenter::PaintFromUIThread(")
    assert "REXCVAR_GET(host_vsync)" in function(presenter, "bool Presenter::InitializeCommonSurfaceIndependent(")
    assert vulkan.count("IsHostVSyncRequestedFromUIThread(),") == 2
    assert "OnHostPresentModeFromUIThread" in function(
        vulkan, "VulkanPresenter::ConnectOrReconnectPaintingToSurfaceFromUIThread(")

    prelude = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <rex/ui/display_settings.h>
#define REX_PLATFORM_MAC 0
#define assert_true(v) assert(v)
#define assert_false(v) assert(!(v))
#define assert_not_null(v) assert((v) != nullptr)
#define REXLOG_WARN(...) ((void)0)
#define REXCVAR_GET(name) flag_##name
static bool flag_host_present_from_non_ui_thread = true;
static bool flag_vulkan_allow_present_mode_immediate = true;
static bool flag_vulkan_allow_present_mode_mailbox = true;
static bool flag_vulkan_allow_present_mode_fifo_relaxed = true;
static int flag_host_window_width = 0, flag_host_window_height = 0;
static int flag_window_width = 0, flag_window_height = 0;
static int flag_video_mode_width = 1280, flag_video_mode_height = 720;
namespace rex::cvar {
bool HasNonDefaultValue(const std::string& name) {
  if (name == "window_width") return flag_window_width != 0;
  if (name == "window_height") return flag_window_height != 0;
  if (name == "video_mode_width") return flag_video_mode_width != 1280;
  if (name == "video_mode_height") return flag_video_mode_height != 720;
  assert(false); return false;
}
}
namespace rex::graphics::video_mode_util {
bool TryGetResolutionPresetFromCVar(int32_t&, int32_t&) { return false; }
}
struct App {
  std::thread::id ui = std::this_thread::get_id();
  bool IsInUIThread() const { return std::this_thread::get_id() == ui; }
};
struct PaintGuard {
  bool drawing = false;
  bool CanChangeSurfaceFromUIThread() const { return !drawing; }
};
struct SDL_Window { SDL_WindowFlags flags = 0; };
static bool fail_fullscreen = false, fail_mode = false, fail_size = false;
static int fullscreen_calls = 0, mode_calls = 0, size_calls = 0;
static int requested_width = 0, requested_height = 0;
extern "C" const char* SDL_GetError() { return "synthetic OS rejection"; }
extern "C" SDL_WindowFlags SDL_GetWindowFlags(SDL_Window* window) { return window->flags; }
extern "C" bool SDL_SetWindowFullscreenMode(SDL_Window*, const SDL_DisplayMode* mode) {
  ++mode_calls; assert(!mode); return !fail_mode;
}
extern "C" bool SDL_SetWindowFullscreen(SDL_Window* window, bool enabled) {
  ++fullscreen_calls;
  if (fail_fullscreen) return false;
  // Model SDL's flags including a pending request, not confirmed native state.
  if (enabled) window->flags |= SDL_WINDOW_FULLSCREEN;
  else window->flags &= ~SDL_WINDOW_FULLSCREEN;
  return true;
}
extern "C" bool SDL_SetWindowSize(SDL_Window*, int width, int height) {
  ++size_calls; requested_width = width; requested_height = height; return !fail_size;
}
namespace rex::ui {
class WindowSDL {
 public:
  enum class Phase { kClosed, kOpen };
  struct WindowDestructionReceiver {
    explicit WindowDestructionReceiver(WindowSDL* window) : window(window) {}
    bool IsWindowDestroyedOrClosed() const { return window->phase() != Phase::kOpen; }
    WindowSDL* window;
  };
  App context;
  PaintGuard guard;
  SDL_Window native;
  SDL_Window* sdl_window_ = &native;
  WindowDisplayState display_state_;
  Phase phase_ = Phase::kOpen;
  bool desired_fullscreen = false;
  uint32_t desired_width = 0, desired_height = 0;
  uint32_t dpi_scale = 1;
  App& app_context() const { return const_cast<App&>(context); }
  Phase phase() const { return phase_; }
  PaintGuard* presenter() { return &guard; }
  bool IsFullscreen() const { return desired_fullscreen; }
  void OnFullscreenUpdate(bool enabled) { desired_fullscreen = enabled; }
  void OnDesiredLogicalSizeUpdate(uint32_t width, uint32_t height) {
    desired_width = width; desired_height = height;
  }
  uint32_t SizeToPhysical(uint32_t n) const { return n * dpi_scale; }
  uint32_t SizeToLogical(uint32_t n) const { return (n + dpi_scale - 1) / dpi_scale; }
  WindowDisplayState GetDisplayStateFromUIThread() const;
  DisplayChangeResult RequestFullscreenFromUIThread(bool);
  DisplayChangeResult RequestWindowSizeFromUIThread(uint32_t, uint32_t);
  void RecordDisplaySize(int, int);
  void RecordDisplayEvent(const SDL_Event&);
  void ApplyNewFullscreen();
};
'''
    harness = prelude
    for name in ["WindowDisplayState WindowSDL::GetDisplayStateFromUIThread(",
                 "DisplayChangeResult WindowSDL::RequestFullscreenFromUIThread(",
                 "DisplayChangeResult WindowSDL::RequestWindowSizeFromUIThread(",
                 "void WindowSDL::RecordDisplaySize(", "void WindowSDL::RecordDisplayEvent(",
                 "void WindowSDL::ApplyNewFullscreen("]:
        harness += function(window, name)
    harness += r'''
struct Window {
  App context;
  App& app_context() const { return const_cast<App&>(context); }
};
struct Surface {
  uint32_t width = 1280, height = 720;
  bool GetSize(uint32_t& w, uint32_t& h) { w = width; h = height; return w && h; }
};
class Presenter {
 public:
'''
    for name in ["enum class PaintMode", "enum class SurfacePaintConnectionState",
                 "enum class SurfacePaintConnectResult"]:
        harness += function(header, name).rstrip() + ";\n"
    harness += r'''
  Window window;
  Surface surface;
  Window* window_ = &window;
  Surface* surface_ = &surface;
  HostPresentationState host_presentation_state_;
  bool supports = true;
  bool is_executing_ui_drawers_ = false, is_in_ui_thread_paint_ = false;
  std::mutex paint_mode_mutex_;
  PaintMode paint_mode_ = PaintMode::kGuestOutputThreadImmediately;
  SurfacePaintConnectionState surface_paint_connection_state_ =
      SurfacePaintConnectionState::kConnectedPaintable;
  bool surface_paint_connection_was_optimal_at_successful_paint_ = false;
  bool surface_paint_connection_has_implicit_vsync_ = false;
  uint32_t surface_width_in_paint_connection_ = 1280, surface_height_in_paint_connection_ = 720;
  std::vector<int> ui_drawers_;
  int reconnects = 0, paints_requested = 0, disconnects = 0;
  SurfacePaintConnectResult next_result = SurfacePaintConnectResult::kSuccess;
  bool SupportsHostVSync() const { return supports; }
  static bool IsConnectedSurfacePaintConnectionState(SurfacePaintConnectionState s) {
    return s == SurfacePaintConnectionState::kConnectedPaintable ||
           s == SurfacePaintConnectionState::kConnectedOutdated;
  }
  bool RequestPaintOrConnectionRecoveryViaWindow(bool) { ++paints_requested; return true; }
  void UpdateUITicksNeededFromUIThread() {}
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() { ++disconnects; }
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      Surface&, uint32_t, uint32_t, bool, bool& implicit_vsync) {
    assert(paint_mode_ != PaintMode::kGuestOutputThreadImmediately);
    assert(!is_executing_ui_drawers_ && !is_in_ui_thread_paint_);
    ++reconnects;
    implicit_vsync = host_presentation_state_.requested_vsync;
    host_presentation_state_.actual_mode = implicit_vsync ? HostPresentMode::kFifo
                                                        : HostPresentMode::kImmediate;
    return next_result;
  }
  HostPresentationState GetHostPresentationStateFromUIThread() const;
  DisplayChangeResult RequestHostVSyncFromUIThread(bool);
  void ApplyPendingHostVSyncFromUIThread();
  void OnSurfaceResizeFromUIThread();
  void UpdateSurfacePaintConnectionFromUIThread(bool*, bool);
  void DisconnectPaintingFromSurfaceFromUIThread(SurfacePaintConnectionState);
  void SetPaintModeFromUIThread(PaintMode);
  PaintMode GetDesiredPaintModeFromUIThread(bool) const;
};
'''
    for name in ["HostPresentationState Presenter::GetHostPresentationStateFromUIThread(",
                 "DisplayChangeResult Presenter::RequestHostVSyncFromUIThread(",
                 "void Presenter::ApplyPendingHostVSyncFromUIThread(",
                 "void Presenter::OnSurfaceResizeFromUIThread(",
                 "void Presenter::UpdateSurfacePaintConnectionFromUIThread(",
                 "void Presenter::DisconnectPaintingFromSurfaceFromUIThread(",
                 "void Presenter::SetPaintModeFromUIThread(",
                 "Presenter::PaintMode Presenter::GetDesiredPaintModeFromUIThread("]:
        harness += function(presenter, name)
    harness += "}\n"
    harness += function(window, "uint32_t ResolveWindowWidth(")
    harness += function(window, "uint32_t ResolveWindowHeight(")
    guest = (sdk / "src/kernel/xboxkrnl/xboxkrnl_video.cpp").read_text()
    harness += function(guest, "uint32_t GetConfiguredVideoModeWidth(")
    harness += function(guest, "uint32_t GetConfiguredVideoModeHeight(")
    start = vulkan.index("  if (require_vsync) {")
    end = vulkan.index("  swapchain_create_info.clipped", start)
    harness += "VkPresentModeKHR Select(bool require_vsync, std::vector<VkPresentModeKHR> present_modes) {\n"
    harness += "VkSwapchainCreateInfoKHR swapchain_create_info{};\n" + vulkan[start:end]
    harness += "return swapchain_create_info.presentMode;\n}\n"
    harness += r'''
using namespace rex::ui;
static void Event(WindowSDL& window, uint32_t type, int w = 0, int h = 0) {
  SDL_Event event{}; event.type = type; event.window.data1 = w; event.window.data2 = h;
  window.RecordDisplayEvent(event);
}
int main() {
  WindowSDL w;
  w.RecordDisplaySize(1280, 720);
  auto result = w.RequestFullscreenFromUIThread(true);
  assert(result.accepted && !result.applied && result.error.empty());
  assert((w.native.flags & SDL_WINDOW_FULLSCREEN) && !w.GetDisplayStateFromUIThread().fullscreen);
  assert(w.GetDisplayStateFromUIThread().fullscreen_pending);
  Event(w, SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
  assert(w.GetDisplayStateFromUIThread().fullscreen && !w.GetDisplayStateFromUIThread().fullscreen_pending);
  int before = fullscreen_calls;
  result = w.RequestFullscreenFromUIThread(true);
  assert(result.accepted && result.applied && fullscreen_calls == before);
  assert(!w.RequestWindowSizeFromUIThread(1920, 1080).accepted && size_calls == 0);
  fail_fullscreen = true;
  result = w.RequestFullscreenFromUIThread(false);
  assert(!result.accepted && !result.error.empty() && w.desired_fullscreen);
  assert(!w.GetDisplayStateFromUIThread().fullscreen_error.empty());
  fail_fullscreen = false;
  result = w.RequestFullscreenFromUIThread(false);
  assert(w.GetDisplayStateFromUIThread().fullscreen_error.empty());
  assert(result.accepted && !result.applied && w.GetDisplayStateFromUIThread().fullscreen);
  Event(w, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
  assert(!w.GetDisplayStateFromUIThread().fullscreen && !w.GetDisplayStateFromUIThread().fullscreen_pending);
  fail_mode = true; before = fullscreen_calls;
  assert(!w.RequestFullscreenFromUIThread(true).accepted && fullscreen_calls == before);
  fail_mode = false;

  w.dpi_scale = 2;
  result = w.RequestWindowSizeFromUIThread(1920, 1080);
  assert(result.accepted && !result.applied && requested_width == 3840 && requested_height == 2160);
  assert(w.GetDisplayStateFromUIThread().logical_width == 1280);
  Event(w, SDL_EVENT_WINDOW_RESIZED, 3000, 1800);
  assert(w.GetDisplayStateFromUIThread().logical_width == 1500);
  assert(w.GetDisplayStateFromUIThread().size_pending); // clamped state is not fabricated success
  Event(w, SDL_EVENT_WINDOW_RESIZED, 3840, 2160);
  assert(!w.GetDisplayStateFromUIThread().size_pending);
  assert(w.RequestWindowSizeFromUIThread(1920, 1080).applied);
  before = size_calls;
  assert(!w.RequestWindowSizeFromUIThread(0, 720).accepted);
  assert(!w.RequestWindowSizeFromUIThread(8193, 720).accepted && size_calls == before);
  for (auto flags : {SDL_WINDOW_MAXIMIZED, SDL_WINDOW_MINIMIZED}) {
    w.native.flags = flags;
    assert(!w.RequestWindowSizeFromUIThread(800, 600).accepted);
  }
  w.native.flags = 0; fail_size = true;
  result = w.RequestWindowSizeFromUIThread(800, 600);
  assert(!result.accepted && !result.error.empty() && w.desired_width == 1920);
  fail_size = false;
  w.guard.drawing = true; before = fullscreen_calls;
  assert(!w.RequestFullscreenFromUIThread(true).accepted && fullscreen_calls == before);
  assert(!w.RequestWindowSizeFromUIThread(800, 600).accepted);
  w.guard.drawing = false;
  std::thread wrong([&] {
    assert(!w.GetDisplayStateFromUIThread().available);
    assert(!w.RequestFullscreenFromUIThread(true).accepted);
    assert(!w.RequestWindowSizeFromUIThread(800, 600).accepted);
  }); wrong.join();
  w.phase_ = WindowSDL::Phase::kClosed;
  assert(!w.RequestFullscreenFromUIThread(true).accepted);
  assert(!w.GetDisplayStateFromUIThread().available);

  flag_host_window_width = 1600; flag_host_window_height = 900;
  assert(ResolveWindowWidth(1280) == 1600 && ResolveWindowHeight(720) == 900);
  assert(GetConfiguredVideoModeWidth() == 1280 && GetConfiguredVideoModeHeight() == 720);
  flag_window_width = 1920; flag_window_height = 1080;
  assert(ResolveWindowWidth(1280) == 1600 && ResolveWindowHeight(720) == 900);
  assert(GetConfiguredVideoModeWidth() == 1920 && GetConfiguredVideoModeHeight() == 1080);
  flag_host_window_width = 0; flag_host_window_height = 0;
  assert(ResolveWindowWidth(1280) == 1920 && ResolveWindowHeight(720) == 1080);

  std::vector<VkPresentModeKHR> modes{VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
                                     VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR};
  assert(Select(false, modes) == VK_PRESENT_MODE_IMMEDIATE_KHR);
  assert(Select(true, modes) == VK_PRESENT_MODE_FIFO_KHR);
  flag_vulkan_allow_present_mode_immediate = false;
  assert(Select(false, modes) == VK_PRESENT_MODE_MAILBOX_KHR);
  assert(Select(true, modes) == VK_PRESENT_MODE_FIFO_KHR);
  flag_vulkan_allow_present_mode_mailbox = false;
  assert(Select(false, modes) == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
  assert(Select(true, modes) == VK_PRESENT_MODE_FIFO_KHR);
  flag_vulkan_allow_present_mode_fifo_relaxed = false;
  assert(Select(false, modes) == VK_PRESENT_MODE_FIFO_KHR);
  assert(Select(false, {}) == VK_PRESENT_MODE_FIFO_KHR);

  Presenter p;
  p.is_executing_ui_drawers_ = p.is_in_ui_thread_paint_ = true;
  result = p.RequestHostVSyncFromUIThread(true);
  assert(result.accepted && !result.applied && p.reconnects == 0 && p.paints_requested == 1);
  p.is_executing_ui_drawers_ = p.is_in_ui_thread_paint_ = false;
  p.ApplyPendingHostVSyncFromUIThread();
  auto state = p.GetHostPresentationStateFromUIThread();
  assert(state.has_applied_preference && state.applied_vsync_preference && !state.pending);
  assert(state.actual_mode == HostPresentMode::kFifo && p.reconnects == 1);
  assert(p.RequestHostVSyncFromUIThread(true).applied);
  assert(p.reconnects == 1);
  p.RequestHostVSyncFromUIThread(false);
  p.next_result = Presenter::SurfacePaintConnectResult::kFailure;
  p.ApplyPendingHostVSyncFromUIThread();
  state = p.GetHostPresentationStateFromUIThread();
  assert(!state.has_applied_preference && !state.pending && !state.error.empty());
  assert(state.actual_mode == HostPresentMode::kUnknown);
  p.next_result = Presenter::SurfacePaintConnectResult::kSuccess;
  p.RequestHostVSyncFromUIThread(false); p.ApplyPendingHostVSyncFromUIThread();
  assert(p.GetHostPresentationStateFromUIThread().has_applied_preference);
  p.surface.width = 0;
  p.RequestHostVSyncFromUIThread(true); p.ApplyPendingHostVSyncFromUIThread();
  state = p.GetHostPresentationStateFromUIThread();
  assert(state.pending && !state.has_applied_preference && state.actual_mode == HostPresentMode::kUnknown);
  p.surface.width = 1280; p.ApplyPendingHostVSyncFromUIThread();
  assert(p.GetHostPresentationStateFromUIThread().applied_vsync_preference);
  p.supports = false;
  assert(!p.RequestHostVSyncFromUIThread(false).accepted);
  p.supports = true;
  std::thread bad_thread([&] { assert(!p.RequestHostVSyncFromUIThread(false).accepted); });
  bad_thread.join();
  p.window_ = nullptr;
  assert(!p.RequestHostVSyncFromUIThread(false).accepted);

  // Exercise the real paint-mode mutex handoff while a synthetic guest painter
  // owns it. A helper releases that owner once the UI is about to refresh.
  Presenter handoff;
  std::mutex mutex; std::condition_variable cv;
  bool owner_ready = false, ui_ready = false;
  std::thread guest_thread([&] {
    std::unique_lock owned(handoff.paint_mode_mutex_);
    std::unique_lock coordination(mutex); owner_ready = true; cv.notify_all();
    cv.wait(coordination, [&] { return ui_ready; });
  });
  { std::unique_lock lock(mutex); cv.wait(lock, [&] { return owner_ready; }); }
  handoff.RequestHostVSyncFromUIThread(true);
  { std::lock_guard lock(mutex); ui_ready = true; cv.notify_all(); }
  handoff.ApplyPendingHostVSyncFromUIThread(); guest_thread.join();
  assert(handoff.reconnects == 1 && handoff.GetHostPresentationStateFromUIThread().has_applied_preference);
  std::cout << "PASS: source-derived window requests/events/errors/DPI, host/guest dimensions, "
               "Vulkan mode selection, deferred presenter refresh, failed/zero-area reconnect, "
               "paint ownership and thread/draw guards. No native window or GPU was created.\n";
}
'''
    generated = output / "settings-display.cpp"
    generated.write_text(harness)
    includes = [sdk / "include", Path("/dependencies/sdl3/include"),
                Path("/dependencies/vulkan-headers/include")]
    command = ["clang++", "-std=c++23", "-O1", "-g", "-pthread",
               "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command += ["-I" + str(path) for path in includes]
    command += [str(generated), "-o", str(output / "settings-display")]
    subprocess.run(command, check=True)
    subprocess.run([str(output / "settings-display")], check=True)
    # Compile the complete changed translation units against actual SDK/SDL/
    # Vulkan headers as well. This catches integration errors hidden by boundary
    # doubles; -fsyntax-only neither links nor initializes a platform backend.
    syntax = ["clang++", "-std=c++23", "-march=x86-64-v2", "-fsyntax-only",
              "-DREX_HAS_VULKAN=1", "-DREX_HAS_D3D12=0", "-DFMT_HEADER_ONLY",
              "-I/sdk", "-I/sdk/include"]
    syntax += ["-I/dependencies/" + directory for directory in
               ["sdl3/include", "vulkan-headers/include", "fmt/include", "spdlog/include",
                "simde", "imgui", "utfcpp/source", "renderdoc", "vulkan-memory-allocator/include"]]
    for source in ("src/ui/window.cpp", "src/ui/window_sdl.cpp", "src/ui/presenter.cpp",
                   "src/ui/vulkan/vulkan_presenter.cpp"):
        subprocess.run(syntax + [str(sdk / source)], check=True)
    print("PASS: complete changed SDK translation units compile against real dependency headers.")


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--inside":
        inside()
        return
    if sys.platform != "linux" or len(sys.argv) != 3:
        raise SystemExit(__doc__)
    bubblewrap = shutil.which("bwrap")
    if not bubblewrap:
        raise SystemExit("bubblewrap is required; no unisolated fallback")
    source = Path(sys.argv[1]).resolve()
    dependencies = (source / "thirdparty").resolve(strict=True)
    output = Path(sys.argv[2]).resolve()
    output.mkdir(exist_ok=False)
    bundle = Path(__file__).resolve().parent
    manifest = json.loads((bundle / "manifest.json").read_text())
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if revision != manifest["revision"]:
        raise SystemExit("SDK revision does not match the patch manifest")
    sdk = output / "sdk"
    sdk.mkdir()
    archive = subprocess.check_output(["git", "-C", str(source), "archive", "HEAD"])
    with tarfile.open(fileobj=io.BytesIO(archive)) as archive_file:
        archive_file.extractall(sdk, filter="data")
    for name in manifest["application_order"]:
        subprocess.run(["git", "apply", "--whitespace=error", str(bundle / name)], cwd=sdk, check=True)
        if name == "settings-display.patch":
            break
    else:
        raise SystemExit("Display patch is missing from the ordered manifest")
    assert "modal_gamepad_input.h" in (sdk / "include/rex/ui/window.h").read_text()
    command = [bubblewrap, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists():
            command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file():
        command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += ["--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
                "--ro-bind", str(sdk), "/sdk", "--ro-bind", str(dependencies), "/dependencies",
                "--ro-bind", str(Path(__file__).resolve()), "/fixture.py",
                "--bind", str(output), "/output", "--clearenv"]
    environment = {
        "PATH": "/usr/bin:/bin", "HOME": "/tmp/home", "XDG_CONFIG_HOME": "/tmp/config",
        "XDG_CACHE_HOME": "/tmp/cache", "XDG_DATA_HOME": "/tmp/data",
        "XDG_RUNTIME_DIR": "/run/test-runtime", "TMPDIR": "/tmp",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/run/test-runtime/session-bus",
        "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/run/test-runtime/system-bus",
        "AT_SPI_BUS_ADDRESS": "unix:path=/run/test-runtime/accessibility-bus",
        "NO_AT_BRIDGE": "1", "PYTHONDONTWRITEBYTECODE": "1", "PYTHONNOUSERSITE": "1",
        "DISPLAY_FIXTURE_ISOLATED": "1",
    }
    for name, value in environment.items():
        command += ["--setenv", name, value]
    command += ["--chdir", "/output", "/usr/bin/python3", "/fixture.py", "--inside"]
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
