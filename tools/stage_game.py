#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Prepare a private local installation from built code and user-owned game files."""

import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from generate_game import EXECUTABLE_SIZE, EXECUTABLE_SHA256, digest
from stage_launcher import ROOT, stage as stage_launcher


def external(path):
    path = path.resolve(strict=True)
    if path == ROOT or ROOT in path.parents:
        raise ValueError("Game inputs and build directories must be outside the repository")
    return path


def runpath(path):
    output = subprocess.check_output(["readelf", "-d", str(path)], text=True,
                                     env=dict(os.environ, LC_ALL="C"))
    values = re.findall(r"\((?:RUNPATH|RPATH)\).*?\[([^\]]*)\]", output)
    if len(values) != 1:
        raise ValueError(f"Expected one runtime search path in {path.name}")
    return values[0]


WINDOWS_BACKENDS = {"NVDA", "JAWS", "ZOOM_TEXT", "SENSE_READER", "PC_TALKER", "ZDSR", "BOY_PC_READER"}
LOAD_LIBRARY_SEARCH_SYSTEM32 = 0x00000800


def default_platform():
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "linux":
        return "linux"
    raise ValueError("Staging currently supports Linux and Windows only")


def check_prism_backends(cache, platform):
    backends = {}
    for name, kind, value in re.findall(
            r"^PRISM_ENABLE_(\w+)_BACKEND:([^=]+)=(.*)$", cache, re.MULTILINE):
        if kind not in {"BOOL", "STRING"} or value not in {"ON", "OFF"} or name in backends:
            raise ValueError("Prism backend configuration is invalid or ambiguous")
        backends[name] = value
    expected = WINDOWS_BACKENDS if platform == "windows" else {"ORCA"}
    enabled = {name for name, value in backends.items() if value == "ON"}
    if enabled != expected:
        raise ValueError("Prism must enable exactly the selected platform's screen-reader backends")


def check_windows_api_sets(unresolved):
    """Query Windows for virtual contracts; never load an inspected binary."""
    if not unresolved:
        return
    contracts = set()
    for name in unresolved:
        name = name.lower()
        if not re.fullmatch(r"(?:api|ext)-[a-z0-9-]+(?:~[a-z0-9-]+)?\.dll", name):
            raise ValueError(f"Unresolved Windows runtime import: {name}")
        contracts.add(name[:-4])
    try:
        # Microsoft apiquery2.h: BOOL APIENTRY IsApiSetImplemented(PCSTR).
        # The OS resolves this fixed API-set name; no build directory is searched.
        api = ctypes.WinDLL("api-ms-win-core-apiquery-l2-1-0.dll",
                            winmode=LOAD_LIBRARY_SEARCH_SYSTEM32)
        query = api.IsApiSetImplemented
        query.argtypes = [ctypes.c_char_p]
        query.restype = ctypes.c_int
        for contract in sorted(contracts):
            if not query(contract.encode("ascii")):
                raise ValueError(f"Windows API-set contract is unavailable: {contract}")
    except (OSError, AttributeError) as error:
        raise ValueError("Windows API-set availability query failed") from error


def audit_windows_runtime(native=None, prism_build=None):
    """Inspect PE files and OS contracts without loading game or speech libraries."""
    if sys.platform != "win32":
        raise ValueError("Windows dependency inspection must run on Windows")
    groups = []
    if native is not None:
        groups.append(("bin", native, native / "battle-brawlers.exe",
                       [native / "rexruntime.dll", native / "rexgpu-xenos.dll"]))
    if prism_build is not None:
        groups.append(("lib", prism_build, None, [prism_build / "prism.dll"]))
    if not groups:
        raise ValueError("At least one Windows build output is required for inspection")
    files = {}
    with tempfile.TemporaryDirectory(prefix="brawlers-runtime-audit-") as temporary:
        for destination, directory, executable, libraries in groups:
            if ";" in str(directory):
                raise ValueError("CMake runtime inspection does not support semicolons in build paths")
            report = Path(temporary) / (destination + ".txt")
            unresolved_report = Path(temporary) / (destination + "-unresolved.txt")
            command = ["cmake", "-DSEARCH_DIRECTORY=" + str(directory),
                       "-DLIBRARIES=" + ";".join(str(path) for path in libraries),
                       "-DOUTPUT=" + str(report),
                       "-DUNRESOLVED_OUTPUT=" + str(unresolved_report)]
            if executable is not None:
                command.append("-DEXECUTABLE=" + str(executable))
            command += ["-P", str(ROOT / "cmake/StageWindowsRuntime.cmake")]
            subprocess.run(command, check=True)
            check_windows_api_sets(unresolved_report.read_text(encoding="utf-8").splitlines())
            dependencies = [Path(line).resolve(strict=True)
                            for line in report.read_text(encoding="utf-8").splitlines()]
            binaries = libraries + ([executable] if executable is not None else [])
            for path in dict.fromkeys(binaries + dependencies):
                path = path.resolve(strict=True)
                if path.parent != directory.resolve() or not path.is_file():
                    raise ValueError("Runtime dependency resolved outside its verified build output")
                header = subprocess.check_output(["llvm-readobj", "--file-headers", str(path)],
                                                 text=True, encoding="utf-8", errors="replace")
                if not re.search(r"^Format: COFF-x86-64$", header, re.MULTILINE) or not re.search(
                        r"^\s*Machine: IMAGE_FILE_MACHINE_AMD64 \(0x8664\)$", header, re.MULTILINE) or not re.search(
                        r"^ImageOptionalHeader \{\s+Magic: 0x20B$", header, re.MULTILINE):
                    raise ValueError(f"Expected an x64 Windows PE binary: {path.name}")
                target = destination + "/" + path.name
                if target in files and files[target] != path:
                    raise ValueError("Runtime dependencies have conflicting destination names")
                files[target] = path
    return files


def stage(native_build, prism_build, prism_source, rexglue_source, game_dir, output, *, platform=None):
    platform = default_platform() if platform is None else platform
    if platform not in {"linux", "windows"}:
        raise ValueError("Unsupported staging platform")
    native = external(native_build) / "native"
    prism_build = external(prism_build)
    prism_source = external(prism_source)
    sdk = external(rexglue_source)
    game = external(game_dir)
    original_output = output
    output = output.resolve()
    if output == ROOT or ROOT in output.parents or os.path.lexists(original_output) or output.exists():
        raise ValueError("Choose a new installation directory outside the repository")
    if any(parent in output.parents for parent in (native.parent, prism_build, prism_source, sdk, game)):
        raise ValueError("The installation must be separate from game inputs and dependency build/source directories")
    executable = game / "default.xex"
    if executable.stat().st_size != EXECUTABLE_SIZE or digest(executable) != EXECUTABLE_SHA256:
        raise ValueError("Game directory does not contain the supported default.xex")
    if platform == "windows":
        files = {
            "bin/battle-brawlers.exe": native / "battle-brawlers.exe",
            "bin/rexruntime.dll": native / "rexruntime.dll",
            "bin/rexgpu-xenos.dll": native / "rexgpu-xenos.dll",
            "lib/prism.dll": prism_build / "prism.dll",
        }
    else:
        files = {
            "bin/battle-brawlers": native / "battle-brawlers",
            "bin/librexruntime.so": native / "librexruntime.so",
            "bin/librexgpu-xenos.so": native / "librexgpu-xenos.so",
            "lib/libprism.so": prism_build / "libprism.so",
        }
    files.update({
        "licenses/rexglue/LICENSE": sdk / "LICENSE",
        "licenses/rexglue/NOTICE.md": ROOT / "patches/rexglue/NOTICE.md",
        "licenses/prism/LICENSE": prism_source / "LICENSE",
        "licenses/prism/NOTICE": prism_source / "NOTICE",
        "licenses/prism/local-notice.md": ROOT / "patches/prism/NOTICE.md",
        "licenses/prism/orca-existing-reader.patch": ROOT / "patches/prism/orca-existing-reader.patch",
        "licenses/prism/manifest.json": ROOT / "patches/prism/manifest.json",
    })
    for source in files.values():
        if not source.is_file():
            raise ValueError(f"Required input is missing: {source}")
    if platform == "linux" and not os.access(native / "battle-brawlers", os.X_OK):
        raise ValueError("The built native host must be executable")
    if not (prism_source / "LICENSES").is_dir():
        raise ValueError("Prism's complete LICENSES directory is required")
    cache = (prism_build / "CMakeCache.txt").read_text(encoding="utf-8")
    check_prism_backends(cache, platform)
    paths = {}
    if platform == "windows":
        files.update(audit_windows_runtime(native, prism_build))
    else:
        paths = {name: runpath(files[name]) for name in ("bin/librexruntime.so", "bin/librexgpu-xenos.so")}
        if runpath(files["bin/battle-brawlers"]) != "$ORIGIN":
            raise ValueError("The native host must use the verified relative runtime search path")
    # All preflight checks finish before creating the new installation.
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    stage_launcher(output, platform=platform)
    for name, source in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    shutil.copytree(prism_source / "LICENSES", output / "licenses/prism/LICENSES")
    for name, old in paths.items():
        if old != "$ORIGIN":
            subprocess.run(["cmake", "-DINPUT=" + str(output / name), "-DOLD_RPATH=" + old,
                            "-P", str(ROOT / "cmake/StageRuntime.cmake")], check=True)
        if runpath(output / name) != "$ORIGIN":
            raise ValueError("Staged runtime search path did not match")
    if platform == "windows":
        (output / "game-data.json").write_text(json.dumps({"path": str(game)}) + "\n", encoding="utf-8")
    else:
        (output / "data").symlink_to(game, target_is_directory=True)
    hashes = {str(p.relative_to(output)): digest(p) for p in output.rglob("*") if p.is_file()}
    (output / "local-build.json").write_text(json.dumps({
        "private_local_build": True, "game_executed": False,
        "game_executable_sha256": EXECUTABLE_SHA256, "files_sha256": hashes,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared local installation: {output}")
    print("Game data remains in its original directory. No game or speech service was started.")
    print("Keep this installation private; it contains compiled game code and references your game files.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("native-build", "prism-build", "prism-source", "rexglue-source", "game-dir", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--platform", choices=("linux", "windows"),
                        help="Installation platform (default: current operating system)")
    args = parser.parse_args()
    try:
        stage(args.native_build, args.prism_build, args.prism_source, args.rexglue_source,
              args.game_dir, args.output, platform=args.platform)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Staging failed: {error}\n")


if __name__ == "__main__":
    main()
