#!/usr/bin/env python3
"""Run VS capture fixtures without a game, display, audio or accessibility bus.

Usage: native_vs_regressions.py SDK SDK_BUILD THIRD_PARTY NEW_OUTPUT_DIRECTORY
Requires Linux, bubblewrap and clang++ with C++23 support.
"""
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    if sys.platform != "linux" or len(sys.argv) != 5:
        raise SystemExit(__doc__)
    bwrap = shutil.which("bwrap")
    compiler = shutil.which("clang++")
    if not bwrap or not compiler:
        raise SystemExit("bubblewrap and clang++ required; no unisolated fallback")
    sdk, built, third_party, output = [Path(value).resolve() for value in sys.argv[1:]]
    for path in (sdk / "include/rex/ppc/context.h", third_party / "simde/simde/x86/sse.h",
                 third_party / "fmt/include/fmt/format.h"):
        if not path.is_file():
            raise SystemExit(f"Required dependency missing: {path}")
    if not (built / "include").is_dir():
        raise SystemExit("SDK_BUILD must contain its generated include directory")
    output.mkdir(exist_ok=False)
    project = Path(__file__).resolve().parents[2]
    command = [bwrap, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists():
            command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file():
        command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += ["--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
                "--ro-bind", str(sdk), "/sdk", "--ro-bind", str(built), "/sdk-build",
                "--ro-bind", str(third_party), "/sdk/thirdparty",
                "--ro-bind", str(project), "/project", "--bind", str(output), "/output",
                "--clearenv", "--setenv", "PATH", "/usr/bin:/bin",
                "--setenv", "HOME", "/tmp/home",
                "--setenv", "XDG_RUNTIME_DIR", "/run/private",
                "--setenv", "XDG_CONFIG_HOME", "/tmp/config",
                "--setenv", "DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/private/session-bus",
                "--setenv", "AT_SPI_BUS_ADDRESS", "unix:path=/run/private/accessibility-bus",
                "--chdir", "/output"]
    includes = ["/project/src/core", "/project/src/game", "/sdk/include", "/sdk-build/include",
                "/sdk/thirdparty", "/sdk/thirdparty/simde", "/sdk/thirdparty/simde/simde",
                "/sdk/thirdparty/fmt/include", "/sdk/thirdparty/spdlog/include"]
    compile_args = [compiler, "-std=c++23", "-O1", "-g", "-ffp-model=strict", "-msse4.1",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-Wall", "-Wextra", "-Werror", "-pthread"]
    compile_args += [f"-I{path}" for path in includes]
    for name in ("mode", "rules", "arena", "viewer", "characters", "decks", "editor",
                 "characters_speech", "editor_speech"):
        binary = f"/output/test-native-vs-{name}"
        compile_command = compile_args + [f"/project/src/game/native_vs_{name}.cpp",
            f"/project/tests/native/test_native_vs_{name}.cpp", "-o", binary]
        for stage, arguments in (("compile", compile_command), ("run", [binary])):
            with (output / f"{name}-{stage}.log").open("x") as log:
                result = subprocess.run(command + arguments, stdout=log, stderr=subprocess.STDOUT)
            print(f"{name} {stage}: {result.returncode}", flush=True)
            if result.returncode:
                raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
