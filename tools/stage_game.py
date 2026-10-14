#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Prepare a private local installation from built code and user-owned game files."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from generate_game import EXECUTABLE_SIZE, EXECUTABLE_SHA256, digest
from stage_launcher import ROOT, stage


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("native-build", "prism-build", "prism-source", "rexglue-source", "game-dir", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    try:
        native = external(args.native_build) / "native"
        prism_build = external(args.prism_build)
        prism_source = external(args.prism_source)
        sdk = external(args.rexglue_source)
        game = external(args.game_dir)
        output = args.output.resolve()
        if output == ROOT or ROOT in output.parents or os.path.lexists(args.output) or output.exists():
            raise ValueError("Choose a new installation directory outside the repository")
        if any(parent in output.parents for parent in (native.parent, prism_build, prism_source, sdk, game)):
            raise ValueError("The installation must be separate from game inputs and dependency build/source directories")
        executable = game / "default.xex"
        if executable.stat().st_size != EXECUTABLE_SIZE or digest(executable) != EXECUTABLE_SHA256:
            raise ValueError("Game directory does not contain the supported default.xex")
        files = {
            "bin/battle-brawlers": native / "battle-brawlers",
            "bin/librexruntime.so": native / "librexruntime.so",
            "bin/librexgpu-xenos.so": native / "librexgpu-xenos.so",
            "lib/libprism.so": prism_build / "libprism.so",
            "licenses/rexglue/LICENSE": sdk / "LICENSE",
            "licenses/rexglue/NOTICE.md": ROOT / "patches/rexglue/NOTICE.md",
            "licenses/prism/LICENSE": prism_source / "LICENSE",
            "licenses/prism/NOTICE": prism_source / "NOTICE",
            "licenses/prism/local-notice.md": ROOT / "patches/prism/NOTICE.md",
            "licenses/prism/orca-existing-reader.patch": ROOT / "patches/prism/orca-existing-reader.patch",
            "licenses/prism/manifest.json": ROOT / "patches/prism/manifest.json",
        }
        for source in files.values():
            if not source.is_file():
                raise ValueError(f"Required input is missing: {source}")
        if not os.access(native / "battle-brawlers", os.X_OK):
            raise ValueError("The built native host must be executable")
        if not (prism_source / "LICENSES").is_dir():
            raise ValueError("Prism's complete LICENSES directory is required")
        cache = (prism_build / "CMakeCache.txt").read_text()
        backends = dict(re.findall(r"^(PRISM_ENABLE_\w+_BACKEND):\w+=(\w+)$", cache, re.MULTILINE))
        if backends.get("PRISM_ENABLE_ORCA_BACKEND") != "ON" or any(
                value != "OFF" for name, value in backends.items() if name != "PRISM_ENABLE_ORCA_BACKEND"):
            raise ValueError("Prism must be built with only the Orca backend enabled")
        paths = {name: runpath(files[name]) for name in ("bin/librexruntime.so", "bin/librexgpu-xenos.so")}
        if runpath(files["bin/battle-brawlers"]) != "$ORIGIN":
            raise ValueError("The native host must use the verified relative runtime search path")
        # All preflight checks finish before creating the new installation.
        output.mkdir(mode=0o700, parents=True, exist_ok=False)
        stage(output)
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
        (output / "data").symlink_to(game, target_is_directory=True)
        hashes = {str(p.relative_to(output)): digest(p) for p in output.rglob("*") if p.is_file()}
        (output / "local-build.json").write_text(json.dumps({
            "private_local_build": True, "game_executed": False,
            "game_executable_sha256": EXECUTABLE_SHA256, "files_sha256": hashes,
        }, indent=2) + "\n")
        print(f"Prepared local installation: {output}")
        print("Game data remains in its original directory. No game or speech service was started.")
        print("Keep this installation private; it contains compiled game code and a link to your game files.")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Staging failed: {error}\n")


if __name__ == "__main__":
    main()
