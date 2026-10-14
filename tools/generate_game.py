#!/usr/bin/env python3
"""Generate private translation from the verified executable; never run the game."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


SOURCE = Path(__file__).resolve().parents[1]
EXECUTABLE_SIZE = 11055104
EXECUTABLE_SHA256 = "7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rexglue", required=True, type=Path,
                        help="patched ReXGlue generator executable")
    parser.add_argument("--game-executable", required=True, type=Path,
                        help="privately supplied verified XEX")
    parser.add_argument("--output", required=True, type=Path,
                        help="new directory outside this repository")
    args = parser.parse_args(argv)

    try:
        executable = args.game_executable.resolve(strict=True)
        if not executable.is_file() or executable.stat().st_size != EXECUTABLE_SIZE:
            parser.error("game executable does not match the supported baseline size")
        if digest(executable) != EXECUTABLE_SHA256:
            parser.error("game executable does not match the supported baseline SHA-256")
        generator = args.rexglue.resolve(strict=True)
        if not generator.is_file() or not os.access(generator, os.X_OK):
            parser.error("ReXGlue must be an executable file")
        output = args.output.resolve()
        if output == SOURCE or SOURCE in output.parents:
            parser.error("output must be outside the repository")
        if os.path.lexists(args.output) or output.exists():
            parser.error("output must not already exist")
        hooks = SOURCE / "config" / "game_events.toml"
        hooks_data = hooks.read_bytes()
        generator_sha256 = digest(generator)
        output.mkdir(mode=0o700, parents=True, exist_ok=False)
    except OSError as error:
        parser.error(str(error))

    # Snapshot the public configuration so later edits cannot change this run.
    (output / "game_events.toml").write_bytes(hooks_data)
    manifest = output / "battle_brawlers_manifest.toml"
    manifest.write_text(
        '[project]\nsdk_version = "0.10.0"\nname = "battle_brawlers"\n\n'
        '[entrypoint]\nincludes = ["game_events.toml"]\n'
        + "file_path = " + json.dumps(str(executable), ensure_ascii=False) + "\n"
        + 'out_directory_path = "generated"\n', encoding="utf-8")
    command = [str(generator), "--log-level", "info", "codegen", str(manifest)]
    record = {
        "executable_size": EXECUTABLE_SIZE,
        "executable_sha256": EXECUTABLE_SHA256,
        "generator_sha256": generator_sha256,
        "hooks_sha256": hashlib.sha256(hooks_data).hexdigest(),
        "command": command,
        "game_executed": False,
    }
    try:
        with (output / "codegen.log").open("wb") as log:
            result = subprocess.run(command, cwd=output, stdout=log,
                                    stderr=subprocess.STDOUT, check=False)
        record["returncode"] = result.returncode
        if result.returncode:
            raise RuntimeError("ReXGlue generation failed")
        if executable.stat().st_size != EXECUTABLE_SIZE or digest(executable) != EXECUTABLE_SHA256:
            raise RuntimeError("game executable changed during generation; discard this output")
        required = ("sources.cmake", "battle_brawlers_init.h", "battle_brawlers_pch.h")
        if any(not (output / "generated" / name).is_file() for name in required):
            raise RuntimeError("ReXGlue did not produce the required generated files")
        record["status"] = "generated"
    except (OSError, RuntimeError) as error:
        record["status"] = "failed"
        record["error"] = str(error)
    (output / "generation.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if record["status"] != "generated":
        print(f"Generation failed; preserved evidence in {output}", file=sys.stderr)
        return 1
    print(f"Private translation generated in {output / 'generated'}")
    print("This does not build, launch or validate gameplay. Keep all output private.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
