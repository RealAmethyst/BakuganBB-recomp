#!/usr/bin/env python3
"""Test actual native settings guards with synthetic memory and call boundaries.

Usage: native_settings_guards.py NEW_OUTPUT_DIRECTORY
Linux/bubblewrap required. No SDK, game inputs, display, audio or bus is loaded.
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
    assert os.environ.get("SETTINGS_GUARDS_ISOLATED") == "1"
    assert not Path("/run/user").exists() and not Path("/dev/dri").exists()
    assert not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY")
    source = Path("/project/src/game/native_settings.cpp").read_text()
    extracted = "\n".join(function(source, signature) for signature in (
        "bool Object(", "bool ReadableRange(", "bool FiniteFloat(", "bool AudioContext(",
        "std::optional<Input> ReadInput(", 'extern "C" void sub_82333CD0(',
        'extern "C" void sub_822E6560('))
    harness = Path("/output/guards.cpp")
    harness.write_text(r'''
#include "guest_ui_memory.h"
#include "settings_menu.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace brawlers::settings;
std::map<uint32_t, uint8_t> bytes;
std::vector<std::string> diagnostics;
bool Guard(bool condition, const char* reason) {
  if (!condition) diagnostics.emplace_back(reason);
  return condition;
}
bool Read(uint32_t address, void* target, size_t size) {
  auto* output = static_cast<uint8_t*>(target);
  for (size_t i = 0; i < size; ++i) {
    auto found = bytes.find(address + i);
    if (found == bytes.end()) return false;
    output[i] = found->second;
  }
  return true;
}
brawlers::ui::GuestUiMemory memory(Read);
void Word(uint32_t address, uint32_t value) {
  for (int i = 3; i >= 0; --i) { bytes[address + i] = value & 255; value >>= 8; }
}
void Range(uint32_t address, uint32_t size) {
  for (uint32_t i = 0; i < size; ++i) bytes[address + i] = 0;
}
// The extracted code reads r3.u32 and writes r3.u64. Separate fields avoid
// depending on host endianness or synthetic union aliasing in this fixture.
struct Register { uint32_t u32 = 0; uint64_t u64 = 0; };
struct PPCContext { Register r3; };
struct Session { uint32_t model = 0x9000; } owner;
Session* session = &owner;
thread_local uint32_t updating_task = 0;
std::map<uint32_t, uint32_t> skipped_final_saves;
unsigned original_wait_calls = 0, original_update_calls = 0;
std::function<void(PPCContext&, uint8_t*)> update_body;
extern "C" void __imp__sub_82333CD0(PPCContext& ctx, uint8_t* base) {
  ++original_update_calls;
  if (update_body) update_body(ctx, base);
}
extern "C" void __imp__sub_822E6560(PPCContext& ctx, uint8_t*) {
  ++original_wait_calls;
  ctx.r3.u64 = 77;
}
@EXTRACTED@
constexpr uint32_t audio = 0x10000, child = 0x11000, pool = 0x12000;
constexpr uint32_t entries = 0x13000, metadata = 0x14000, groups = 0x15000;
void ValidAudio() {
  bytes.clear(); diagnostics.clear();
  Range(audio, 0x2B4); Range(child, 0x18); Range(pool, 0x28);
  Range(entries, 12); Range(metadata, 24); Range(groups, 32);
  Word(0x829CDCE8, audio); Word(audio, 0x82036260);
  Word(audio + 0x40, child); Word(audio + 0x3C, pool);
  Word(audio + 0x2AC, metadata); Word(audio + 0x2B0, 2);
  Word(audio + 0x29C, groups);
  Word(child, 0x82036390); Word(child + 4, audio + 0x58);
  Word(child + 8, pool); Word(child + 12, audio + 0x29C);
  Word(child + 16, metadata); Word(child + 20, 2);
  Word(pool + 4, 4); Word(pool + 0x14, entries);
  Word(pool + 0x18, entries + 12); Word(pool + 0x24, 3);
  Word(audio + 0x4C, 0x3F800000); Word(audio + 0x50, 0x3F800000);
  Word(metadata, 0x3F800000); Word(metadata + 4, 0x3F800000);
  Word(metadata + 8, 1);
}
void AudioGuards() {
  ValidAudio(); assert(AudioContext() && AudioContext(0) && AudioContext(1));
  // Every anchored owner and child relationship must match independently.
  for (auto address : {0x829CDCE8u, audio, audio + 0x40, child, child + 4,
       child + 8, child + 12, child + 16, child + 20}) {
    ValidAudio(); Word(address, 0); assert(!AudioContext(0)); assert(!diagnostics.empty());
  }
  for (auto address : {audio + 0x3C, audio + 0x2AC, audio + 0x2B0}) {
    ValidAudio(); Word(address, 0); assert(!AudioContext(0));
  }
  for (const auto& [address, value] : std::vector<std::pair<uint32_t,uint32_t>>{
       {pool + 4, 0}, {pool + 4, 5}, {pool + 0x18, entries - 4},
       {pool + 0x18, entries + 11}, {pool + 0x24, 4},
       {pool + 0x18, entries + 65540}}) {
    ValidAudio(); Word(address, value); assert(!AudioContext(0));
  }
  for (auto address : {audio + 0x4C, audio + 0x50, metadata, metadata + 4}) {
    for (auto nonfinite : {0x7F800000u, 0xFF800000u, 0x7FC00000u}) {
      ValidAudio(); Word(address, nonfinite); assert(!AudioContext(0));
    }
  }
  for (auto address : {audio + 0x200, pool + 0x20, entries + 8, metadata + 11, groups + 31}) {
    ValidAudio(); bytes.erase(address); assert(!AudioContext(0));
  }
  ValidAudio(); Word(metadata + 8, 2); assert(!AudioContext(0));
  ValidAudio(); Word(audio + 0x29C, 0); assert(!AudioContext(0));
  ValidAudio(); Word(audio + 0x29C, UINT32_MAX - 4095); assert(!AudioContext(0));
  ValidAudio(); Word(audio + 0x2AC, UINT32_MAX - 3); Word(child + 16, UINT32_MAX - 3);
  assert(!AudioContext(1));
  ValidAudio(); assert(!ReadableRange(0, 1, "null"));
  assert(!ReadableRange(audio, 0, "empty"));
  assert(!ReadableRange(audio, 65537, "budget"));
  assert(!ReadableRange(UINT32_MAX - 2, 4, "overflow"));
  assert(!FiniteFloat(0x70000, "unreadable"));
  assert(ReadableRange(audio, 0x2B4, "complete"));
}
constexpr uint32_t task = 0x30000, service = 0x31000;
void ValidWait() {
  bytes.clear(); diagnostics.clear(); skipped_final_saves.clear();
  Word(task, 0x82019C40); Word(task + 0x14, 5);
  Word(0x829CC350, service); Word(service, 0); Word(service + 12, 0);
  Word(service + 0x24, 0); Word(0x829CA614, 0); Word(0x829CA618, 0);
  skipped_final_saves[task] = service; updating_task = task;
  original_wait_calls = original_update_calls = 0;
}
void WaitGuards() {
  PPCContext ctx;
  ValidWait(); sub_822E6560(ctx, nullptr);
  assert(ctx.r3.u64 == 1 && original_wait_calls == 0 && skipped_final_saves.empty());
  sub_822E6560(ctx, nullptr); assert(ctx.r3.u64 == 77 && original_wait_calls == 1);
  for (auto address : {task, task + 0x14, 0x829CC350u, service, service + 12,
       service + 0x24, 0x829CA614u, 0x829CA618u}) {
    ValidWait(); Word(address, 99); sub_822E6560(ctx, nullptr);
    assert(ctx.r3.u64 == 0 && original_wait_calls == 0 && skipped_final_saves.count(task));
    assert(!diagnostics.empty());
    ValidWait(); bytes.erase(address); sub_822E6560(ctx, nullptr);
    assert(ctx.r3.u64 == 0 && original_wait_calls == 0 && skipped_final_saves.count(task));
  }
  ValidWait(); skipped_final_saves[task] = 0; sub_822E6560(ctx, nullptr);
  assert(ctx.r3.u64 == 0 && original_wait_calls == 0);
  ValidWait(); updating_task = task + 0x100; sub_822E6560(ctx, nullptr);
  assert(ctx.r3.u64 == 77 && original_wait_calls == 1 && skipped_final_saves.count(task));
  // Nested unrelated task updates must restore the outer task scope before its wait.
  ValidWait(); updating_task = 123;
  update_body = [&](PPCContext& current, uint8_t* base) {
    assert(updating_task == current.r3.u32);
    if (current.r3.u32 == task) {
      PPCContext nested; nested.r3.u32 = task + 0x100;
      sub_82333CD0(nested, base);
      assert(updating_task == task && nested.r3.u64 == 77);
    }
    sub_822E6560(current, base);
  };
  ctx.r3.u32 = task; sub_82333CD0(ctx, nullptr);
  assert(updating_task == 123 && ctx.r3.u64 == 1);
  assert(original_update_calls == 2 && original_wait_calls == 1 && skipped_final_saves.empty());
  update_body = {};
}
void InputGuards() {
  bytes.clear(); diagnostics.clear();
  constexpr uint32_t manager = 0x40000, cursors = 0x50000, container = 0x60000;
  constexpr uint32_t begin = 0x61000, controller = 0x62000;
  Word(0x829CDA70, manager); Word(manager, 0x82035B48);
  bytes[session->model + 0x1C] = 2;
  Word(0x829CED20, cursors); bytes[cursors + 2 * 24 + 0x15] = 1;
  Word(manager + (0x1107 + 2) * 4, 5);
  Word(0x829E7960, container); Word(container + 12, begin); Word(container + 16, begin + 24);
  Word(begin + 5 * 4, controller); bytes[controller + 12] = 1;
  Word(controller + 0x14, 4); Word(controller + 0x18, 4); Word(controller + 0x20, 0);
  uint32_t held = 0;
  assert(ReadInput(&held) == Input::Activate && held == 4); // Six-entry vector is valid.
  bytes[cursors + 2 * 24 + 0x15] = 0;
  assert(!ReadInput(&held) && held == UINT32_MAX);
  bytes.erase(cursors + 2 * 24 + 0x15);
  assert(!ReadInput(&held) && held == UINT32_MAX);
  bytes[cursors + 2 * 24 + 0x15] = 1;
  Word(manager + (0x1107 + 2) * 4, 6); assert(!ReadInput());
  Word(manager + (0x1107 + 2) * 4, 5);
  Word(container + 16, begin + 23); assert(!ReadInput());
  Word(container + 16, begin + 24);
  // Disconnection is not evidence that held navigation was released.
  bytes[controller + 12] = 0; assert(!ReadInput(&held) && held == UINT32_MAX);
  bytes[controller + 12] = 1;
  assert(ReadInput(&held) == Input::Activate && held == 4);
  Word(controller + 0x14, 0); Word(controller + 0x18, 0);
  assert(!ReadInput(&held) && held == 0);
}
int main() {
  AudioGuards(); WaitGuards(); InputGuards();
  std::cout << "PASS: actual audio ownership/ranges, scoped final-save waits and controller enabled gate.\n";
}
'''.replace("@EXTRACTED@", extracted))
    subprocess.run(["clang++", "-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I/project/src/core",
                    str(harness), "-o", "/output/guards"], check=True)
    subprocess.run(["/output/guards"], check=True)


def main():
    if sys.argv[1:] == ["--inside"]:
        inside()
        return
    if sys.platform != "linux" or len(sys.argv) != 2:
        raise SystemExit(__doc__)
    bwrap = shutil.which("bwrap")
    if not bwrap:
        raise SystemExit("bubblewrap required; no unisolated fallback")
    output = Path(sys.argv[1]).resolve()
    project = Path(__file__).resolve().parents[2]
    if output == project or project in output.parents:
        raise SystemExit("Output directory must be outside the source tree")
    output.mkdir(exist_ok=False)
    command = [bwrap, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists(): command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file(): command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += ["--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
                "--ro-bind", str(project), "/project", "--bind", str(output), "/output", "--clearenv"]
    for key, value in {
        "PATH": "/usr/bin:/bin", "HOME": "/tmp/home", "XDG_CONFIG_HOME": "/tmp/config",
        "XDG_CACHE_HOME": "/tmp/cache", "XDG_DATA_HOME": "/tmp/data", "XDG_RUNTIME_DIR": "/run/test-runtime",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/run/test-runtime/session-bus",
        "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/run/test-runtime/system-bus",
        "AT_SPI_BUS_ADDRESS": "unix:path=/run/test-runtime/accessibility-bus", "NO_AT_BRIDGE": "1",
        "PYTHONDONTWRITEBYTECODE": "1", "PYTHONNOUSERSITE": "1", "SETTINGS_GUARDS_ISOLATED": "1",
    }.items(): command += ["--setenv", key, value]
    command += ["--chdir", "/output", "/usr/bin/python3", "/project/tests/runtime/native_settings_guards.py", "--inside"]
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
