#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Copy launcher sources into a new external flat package directory."""

import argparse
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    "src/platform/common/launch_support.py",
    "src/core/accessibility/prism_speech.py",
    "src/core/accessibility/keyboard_speech.py",
    "src/core/accessibility/screen_speech.py",
    "src/core/accessibility/speech_text.py",
    "resources/messages.en.json",
    "LICENSE",
    "NOTICE.md",
)


def stage(destination, platform="linux"):
    if platform not in {"linux", "windows"}:
        raise ValueError("Unsupported launcher platform")
    destination = destination.resolve()
    if destination == ROOT or ROOT in destination.parents:
        raise ValueError("The staging directory must be outside the repository")
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError("The staging directory must be empty or nonexistent")
    sources = [ROOT / "src/platform" / platform / "launch.py"]
    sources += [ROOT / name for name in SOURCES]
    for source in sources:
        if not source.is_file():
            raise ValueError(f"Missing launcher source: {source.relative_to(ROOT)}")
    destination.mkdir(parents=True, exist_ok=True)
    for source in sources:
        target = destination / source.name
        with source.open("rb") as original, target.open("xb") as copied:
            shutil.copyfileobj(original, copied)
        shutil.copymode(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, help="Empty or new directory outside the repository")
    parser.add_argument("--platform", choices=("linux", "windows"), default="linux")
    args = parser.parse_args()
    try:
        stage(args.destination, platform=args.platform)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Staging failed: {error}\n")
    print("Staged launcher sources, resources and license notices; native libraries and game data are not included.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
