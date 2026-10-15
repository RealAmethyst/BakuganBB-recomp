#!/usr/bin/env python3
"""Run the native settings renderer with synthetic guest memory and callbacks.

Usage: native_settings_render_regressions.py SDK SIMDE_INCLUDE NEW_OUTPUT_DIRECTORY
Requires Linux, bubblewrap and a C++23 compiler. No game or service is loaded.
"""
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    if sys.platform != "linux" or len(sys.argv) != 4:
        raise SystemExit(__doc__)
    bwrap = shutil.which("bwrap")
    if not bwrap:
        raise SystemExit("bubblewrap required; no unisolated fallback")
    sdk, simde, output = [Path(value).resolve() for value in sys.argv[1:]]
    if not (sdk / "include/rex/ppc/context.h").is_file():
        raise SystemExit("SDK must contain include/rex/ppc/context.h")
    if not (simde / "simde").is_dir():
        raise SystemExit("SIMDE_INCLUDE must contain the simde directory")
    output.mkdir(exist_ok=False)
    project = Path(__file__).resolve().parents[2]
    command = [bwrap, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists():
            command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file():
        command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += ["--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
                "--ro-bind", str(sdk), "/sdk", "--ro-bind", str(simde), "/simde",
                "--ro-bind", str(project), "/project", "--bind", str(output), "/output",
                "--clearenv", "--setenv", "PATH", "/usr/bin:/bin",
                "--setenv", "HOME", "/tmp/home", "--chdir", "/output"]
    compile_command = ["/usr/bin/c++", "-std=c++23", "-O0", "-g",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                       "-I/project/src/core", "-I/project/src/game",
                       "-isystem", "/sdk/include", "-isystem", "/simde",
                       "/project/tests/runtime/native_settings_render_regressions.cpp",
                       "-o", "/output/native-settings-render"]
    subprocess.run(command + compile_command, check=True)
    subprocess.run(command + ["/output/native-settings-render"], check=True)


if __name__ == "__main__":
    main()
