#!/usr/bin/env python3
"""Synthetic checked-save regressions against the patched SDK's actual cvar.cpp.

Usage: checked-config-save-regressions.py PATCHED_SDK NEW_OUTPUT_DIRECTORY
Linux only: uses linker syscall wrapping to inject filesystem failures. All
configuration files are created below the new output directory. No runtime,
desktop, screen-reader, audio, environment loading, or game code is invoked.
"""

from pathlib import Path
import os
import subprocess
import sys


def main():
    if sys.platform != "linux" or len(sys.argv) != 3:
        raise SystemExit(__doc__)
    sdk = Path(sys.argv[1]).resolve()
    root = Path(sys.argv[2]).resolve()
    root.mkdir(exist_ok=False)
    stubs = root / "stubs"
    (stubs / "rex/platform").mkdir(parents=True)
    # The full production registry, parser, serializer and filesystem code are
    # compiled. Only logging and environment access are isolated at the boundary.
    (stubs / "rex/logging.h").write_text("""#pragma once
#define REXLOG_DEBUG(...) ((void)0)
#define REXLOG_INFO(...) ((void)0)
#define REXLOG_WARN(...) ((void)0)
#define REXLOG_ERROR(...) ((void)0)
""")
    (stubs / "rex/platform/env.h").write_text("""#pragma once
#include <optional>
#include <string>
#include <string_view>
namespace rex::platform::env {
inline std::optional<std::string> get(std::string_view) { return std::nullopt; }
}
""")
    source = root / "checked-config-save.cpp"
    source.write_text(r'''
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <rex/cvar.h>
#include <toml++/toml.hpp>

REXCVAR_DEFINE_STRING(test_ui_name, "default", "Synthetic", "Synthetic string");
REXCVAR_DEFINE_BOOL(test_enabled, false, "Synthetic", "Synthetic boolean");
REXCVAR_DEFINE_INT32(test_count, 0, "Synthetic", "Synthetic count").range(0, 10);
REXCVAR_DEFINE_INT32(test_restart, 0, "Synthetic", "Pending value")
    .range(0, 10).lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_UINT64(test_unsigned, 0, "Synthetic", "Unsigned value");
REXCVAR_DEFINE_DOUBLE(test_number, 0.0, "Synthetic", "Number");
REXCVAR_DEFINE_COMMAND(test_command, [] {}, "Synthetic", "Command");

enum class Failure { None, Write, PartialWrite, Sync, Close, Replace, InterruptedWrite };
static Failure failure = Failure::None;
static int injected_calls = 0;
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_fsync(int);
extern "C" int __real_close(int);
extern "C" int __real_rename(const char*, const char*);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
  if (failure == Failure::Write) { ++injected_calls; errno = ENOSPC; return -1; }
  if (failure == Failure::PartialWrite) {
    if (++injected_calls == 1) return __real_write(fd, data, std::min(size, size_t(7)));
    errno = ENOSPC; return -1;
  }
  if (failure == Failure::InterruptedWrite && ++injected_calls == 1) {
    errno = EINTR; return -1;
  }
  return __real_write(fd, data, size);
}
extern "C" int __wrap_fsync(int fd) {
  if (failure == Failure::Sync) { ++injected_calls; errno = EIO; return -1; }
  return __real_fsync(fd);
}
extern "C" int __wrap_close(int fd) {
  int result = __real_close(fd);
  if (failure == Failure::Close) { ++injected_calls; errno = EIO; return -1; }
  return result;
}
extern "C" int __wrap_rename(const char* from, const char* to) {
  if (failure == Failure::Replace) { ++injected_calls; errno = EACCES; return -1; }
  return __real_rename(from, to);
}

namespace fs = std::filesystem;
using rex::cvar::ConfigSaveValue;
using rex::cvar::SaveConfigChecked;
static std::string Read(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
static void Write(const fs::path& path, const std::string& text) {
  std::ofstream stream(path, std::ios::binary); stream << text; stream.close(); assert(stream);
}
static void NoTemporary(const fs::path& root) {
  for (const auto& entry : fs::directory_iterator(root)) {
    assert(entry.path().filename().string().find(".tmp.") == std::string::npos);
  }
}
static void Reject(const fs::path& path, std::vector<ConfigSaveValue> values) {
  auto before = Read(path);
  auto result = SaveConfigChecked(path, values);
  assert(!result.saved && !result.error.empty() && Read(path) == before);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  fs::path root = argv[1], path = root / "settings.toml";
  const std::string original =
      "# synthetic configuration\ntest_ui_name = 'flat stale'\n"
      "test_count = 9\nunknown = [1, 2, 3]\n"
      "[test]\nui_name = 'nested stale'\nkeep = 'retained'\n"
      "[test.ui]\nname = 'deep stale'\nkeep = 42\n"
      "[future]\nflag = true\n";
  Write(path, original);
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);
  const fs::path collision = path.string() + ".tmp." + std::to_string(getpid()) + ".0";
  Write(collision, "preexisting temporary must survive");
  const std::string special = "quote\" slash\\ newline\n tab\t return\r "
                              "apostrophe' triple''" "' unicode \xE2\x98\x83 nul" +
                              std::string(1, '\0') + " end";
  auto result = SaveConfigChecked(path, {{"test_ui_name", special}});
  assert(result.saved && result.error.empty());
  assert(Read(collision) == "preexisting temporary must survive");
  fs::remove(collision);
  auto parsed = toml::parse_file(path.string());
  assert(parsed["test_ui_name"].value<std::string>() == special);
  assert(!parsed["test"]["ui_name"] && !parsed["test"]["ui"]["name"]);
  assert(parsed["test"]["keep"].value<std::string>() == "retained");
  assert(parsed["test"]["ui"]["keep"].value<int>() == 42);
  assert(parsed["test_count"].value<int>() == 9);
  assert(parsed["future"]["flag"].value<bool>() == true);
  assert(parsed["unknown"].as_array()->size() == 3);
  assert((fs::status(path).permissions() & fs::perms::all) ==
         (fs::perms::owner_read | fs::perms::owner_write));
  assert(rex::cvar::GetFlagByName("test_ui_name") == "default");

  // Pending restart values persist without changing live state or callbacks.
  int callbacks = 0;
  rex::cvar::RegisterChangeCallback("test_restart", [&](auto, auto) { ++callbacks; });
  assert(SaveConfigChecked(path, {{"test_restart", "4"}}).saved);
  assert(toml::parse_file(path.string())["test_restart"].value<int>() == 4);
  assert(rex::cvar::GetFlagByName("test_restart") == "0" && callbacks == 0);
  assert(rex::cvar::GetFlagSource("test_restart") == rex::cvar::Source::kDefault);
  assert(rex::cvar::GetPendingRestartFlags().empty());

  // Reset removes every recognized alias, retaining unrelated registered values.
  Write(path, original);
  assert(SaveConfigChecked(path, {{"test_ui_name", "default"}}).saved);
  parsed = toml::parse_file(path.string());
  assert(!parsed["test_ui_name"] && !parsed["test"]["ui_name"]);
  assert(!parsed["test"]["ui"]["name"] && parsed["test_count"].value<int>() == 9);
  rex::cvar::LoadConfig(path);
  assert(rex::cvar::GetFlagByName("test_ui_name") == "default");
  assert(rex::cvar::GetFlagByName("test_count") == "9");

  // Resetting the last override must update the file, including the legacy API.
  rex::cvar::ResetAllToDefaults();
  Write(path, "test_enabled = true\n");
  rex::cvar::SaveConfig(path);
  assert(toml::parse_file(path.string()).empty());
  rex::cvar::LoadConfig(path);
  assert(!REXCVAR_GET(test_enabled));
  fs::remove(path);
  assert(SaveConfigChecked(path).saved && toml::parse_file(path.string()).empty());
  assert(rex::cvar::SetFlagByName("test_ui_name", special));
  rex::cvar::SaveConfig(path);
  assert(toml::parse_file(path.string())["test_ui_name"].value<std::string>() == special);

  // Validate the entire batch before I/O; do not call permissive flag setters.
  Reject(path, {{"test_count", "11"}});
  Reject(path, {{"test_count", "2junk"}});
  Reject(path, {{"test_count", "2147483648"}});
  Reject(path, {{"test_enabled", "maybe"}});
  Reject(path, {{"unknown_flag", "2"}});
  Reject(path, {{"test_command", ""}});
  Reject(path, {{"test_count", "1"}, {"test_count", "2"}});
  Reject(path, {{"test_unsigned", "18446744073709551615"}});
  Reject(path, {{"test_unsigned", "-1"}});
  Reject(path, {{"test_number", std::string("1\0junk", 6)}});
  Reject(path, {{"test_number", "1e400"}});
  Reject(path, {{"test_number", "nan"}});
  Reject(path, {{"test_number", "inf"}});
  Reject(path, {{"test_number", "-inf"}});
  Reject(path, {{"test_count", "2"}, {"test_ui_name", std::string(1, char(0xff))}});
  assert(SaveConfigChecked(path, {{"test_unsigned", "9223372036854775807"},
                                  {"test_number", "1.25"}, {"test_enabled", "true"}}).saved);
  parsed = toml::parse_file(path.string());
  assert(parsed["test_unsigned"].value<int64_t>() == INT64_MAX);
  assert(parsed["test_number"].value<double>() == 1.25);
  assert(parsed["test_enabled"].value<bool>() == true);
  Write(path, "[test_count]\nunrelated = 3\n");
  Reject(path, {{"test_count", "2"}});
  Write(path, "invalid = [\n");
  Reject(path, {{"test_count", "2"}});

  // Failure after partial output, sync, close and replacement never truncates
  // the original, leaks a temporary, or reports success. Close is not retried.
  for (auto mode : {Failure::Write, Failure::PartialWrite, Failure::Sync,
                    Failure::Close, Failure::Replace}) {
    Write(path, original);
    failure = mode; injected_calls = 0;
    result = SaveConfigChecked(path, {{"test_count", "2"}});
    failure = Failure::None;
    assert(!result.saved && !result.error.empty() && injected_calls > 0);
    if (mode == Failure::Close) assert(injected_calls == 1);
    assert(Read(path) == original);
    NoTemporary(root);
  }
  failure = Failure::InterruptedWrite; injected_calls = 0;
  result = SaveConfigChecked(path, {{"test_count", "2"}});
  failure = Failure::None;
  assert(result.saved && injected_calls >= 2);
  assert(toml::parse_file(path.string())["test_count"].value<int>() == 2);

  // A failed first save leaves no destination either.
  fs::remove(path); failure = Failure::Replace;
  result = SaveConfigChecked(path, {{"test_count", "2"}});
  failure = Failure::None;
  assert(!result.saved && !fs::exists(path)); NoTemporary(root);
  result = SaveConfigChecked(root / "missing-parent/settings.toml", {{"test_count", "2"}});
  assert(!result.saved && !fs::exists(root / "missing-parent"));

  // Refuse symlinks (including dangling links), directories and special files.
  Write(path, original);
  fs::create_symlink(path.filename(), root / "link.toml");
  assert(!SaveConfigChecked(root / "link.toml", {{"test_count", "2"}}).saved);
  assert(fs::is_symlink(root / "link.toml") && Read(path) == original);
  fs::create_symlink("absent.toml", root / "dangling.toml");
  assert(!SaveConfigChecked(root / "dangling.toml", {{"test_count", "2"}}).saved);
  fs::create_directory(root / "directory.toml");
  assert(!SaveConfigChecked(root / "directory.toml", {{"test_count", "2"}}).saved);
  assert(mkfifo((root / "fifo.toml").c_str(), 0600) == 0);
  assert(!SaveConfigChecked(root / "fifo.toml", {{"test_count", "2"}}).saved);

  // Read/merge/write serialization retains disjoint in-process updates.
  bool first = false, second = false;
  std::thread a([&] { first = SaveConfigChecked(path, {{"test_count", "3"}}).saved; });
  std::thread b([&] { second = SaveConfigChecked(path, {{"test_enabled", "true"}}).saved; });
  a.join(); b.join(); assert(first && second);
  parsed = toml::parse_file(path.string());
  assert(parsed["test_count"].value<int>() == 3 && parsed["test_enabled"].value<bool>() == true);
  NoTemporary(root);
  std::cout << "PASS: checked config persistence, aliases/default resets, escaped strings, "
               "unrelated values, pending restart without mutation, validation, concurrent merge, "
               "write/sync/close/replace failures, cleanup and nonregular destinations.\n";
}
''')
    executable = root / "checked-config-save"
    command = [os.environ.get("CXX", "clang++"), "-std=c++23", "-O1", "-g",
               "-march=x86-64-v2", "-DFMT_HEADER_ONLY", "-pthread",
               "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
               "-I" + str(stubs), "-I" + str(sdk / "include")]
    command += ["-I" + str(sdk / "thirdparty" / directory) for directory in
                ["cli11/include", "fmt/include", "tomlplusplus/include", "simde"]]
    command += [str(sdk / "src/core/cvar.cpp"), str(source),
                "-Wl,--wrap=write,--wrap=fsync,--wrap=close,--wrap=rename", "-o", str(executable)]
    subprocess.run(command, check=True)
    data = root / "data"
    data.mkdir()
    subprocess.run([str(executable), str(data)], check=True)


if __name__ == "__main__":
    main()
