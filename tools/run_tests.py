#!/usr/bin/env python3
"""Run original source-only fixtures in an isolated Linux environment.

Default: configure, build and run CTest in a fresh temporary directory.
--build-dir retains output in a new directory outside the repository.
No game, SDK, real speech backend or desktop service is loaded.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[1]
SANDBOX_SOURCE = Path("/source")
SANDBOX_OUTPUT = Path("/output")
MARKER = "BRAWLERS_TEST_SANDBOX"


def inside_sandbox():
    return SOURCE == SANDBOX_SOURCE and os.environ.get(MARKER) == "1"


def python_tests():
    paths = [
        SOURCE / "src/platform/linux",
        SOURCE / "src/platform/common",
        SOURCE / "src/core/accessibility",
        SOURCE / "tests/python",
    ]
    sys.path[:0] = [str(path) for path in paths]
    suite = unittest.defaultTestLoader.discover(str(paths[-1]), pattern="test_*.py")
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


def sandbox_work(python_only):
    if not inside_sandbox():
        raise RuntimeError("Internal test execution requires the isolated runner")
    for directory in ("/tmp/home", "/tmp/config", "/tmp/cache", "/tmp/data", "/run/test-runtime"):
        Path(directory).mkdir(mode=0o700, parents=True, exist_ok=True)
        Path(directory).chmod(0o700)
    if python_only:
        return python_tests()
    commands = [
        ["cmake", "-S", str(SOURCE), "-B", str(SANDBOX_OUTPUT),
         "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON",
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
        ["cmake", "--build", str(SANDBOX_OUTPUT), "--parallel", "2"],
        ["ctest", "--test-dir", str(SANDBOX_OUTPUT), "--output-on-failure"],
    ]
    for command in commands:
        print("Running:", " ".join(command), flush=True)
        completed = subprocess.run(command, check=False)
        if completed.returncode:
            return completed.returncode
    return 0


def sandbox_command(output, python_only):
    executable = shutil.which("bwrap")
    if not executable:
        raise RuntimeError("bubblewrap (bwrap) is required; no unisolated fallback is provided")
    interpreter = Path(sys.executable).resolve()
    if not any(interpreter.is_relative_to(path) for path in (Path("/usr"), Path("/bin"))):
        raise RuntimeError("Use a system Python under /usr or /bin; external environments are not mounted")
    command = [executable, "--unshare-all", "--die-with-parent", "--new-session"]
    for directory in ("/usr", "/bin", "/lib", "/lib64"):
        if Path(directory).exists():
            command += ["--ro-bind", directory, directory]
    if Path("/etc/ld.so.cache").is_file():
        command += ["--ro-bind", "/etc/ld.so.cache", "/etc/ld.so.cache"]
    command += [
        "--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp", "--tmpfs", "/run",
        "--ro-bind", str(SOURCE), str(SANDBOX_SOURCE),
        "--bind", str(output), str(SANDBOX_OUTPUT), "--clearenv",
    ]
    environment = {
        "PATH": "/usr/bin:/bin",
        "HOME": "/tmp/home",
        "XDG_CONFIG_HOME": "/tmp/config",
        "XDG_CACHE_HOME": "/tmp/cache",
        "XDG_DATA_HOME": "/tmp/data",
        "XDG_RUNTIME_DIR": "/run/test-runtime",
        "TMPDIR": "/tmp",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/run/test-runtime/session-bus",
        "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/run/test-runtime/system-bus",
        "AT_SPI_BUS_ADDRESS": "unix:path=/run/test-runtime/accessibility-bus",
        "NO_AT_BRIDGE": "1",
        "PYTHONDONTWRITEBYTECODE": "1",
        "PYTHONNOUSERSITE": "1",
        MARKER: "1",
    }
    for name, value in environment.items():
        command += ["--setenv", name, value]
    command += ["--chdir", str(SANDBOX_OUTPUT), str(interpreter),
                "/source/tools/run_tests.py", "--inside"]
    if python_only:
        command.append("--python-only")
    return command


def run_isolated(output, python_only):
    print(f"Isolated test output: {output}", flush=True)
    result = subprocess.run(sandbox_command(output, python_only), check=False)
    if result.returncode:
        print("Isolated checks failed; no unisolated retry was attempted.", file=sys.stderr)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path,
                        help="Retain output in a new directory outside the repository")
    parser.add_argument("--python-only", action="store_true",
                        help="Run only the isolated Python contract fixtures")
    parser.add_argument("--inside", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if sys.version_info < (3, 9):
        parser.error("Python 3.9 or newer is required")
    if sys.platform != "linux":
        parser.error("The isolated test runner currently supports Linux only")
    if args.inside or inside_sandbox():
        if args.build_dir:
            parser.error("An internal invocation cannot select another build directory")
        return sandbox_work(args.python_only)
    if args.build_dir:
        output = args.build_dir.expanduser().resolve()
        if output == SOURCE or output.is_relative_to(SOURCE):
            parser.error("The build directory must be outside the repository")
        if output.exists():
            parser.error("The build directory must not already exist")
        output.mkdir(mode=0o700)
        return run_isolated(output, args.python_only)
    temporary_root = Path(tempfile.gettempdir()).resolve()
    if temporary_root == SOURCE or temporary_root.is_relative_to(SOURCE):
        parser.error("The temporary directory must be outside the repository")
    with tempfile.TemporaryDirectory(prefix="brawlers-source-tests-", dir=temporary_root) as output:
        return run_isolated(Path(output), args.python_only)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"Test runner error: {error}", file=sys.stderr)
        sys.exit(1)
