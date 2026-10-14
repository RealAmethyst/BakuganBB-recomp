#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Apply a verified patch bundle to a clean, pinned external dependency checkout."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def git(source, *arguments, **kwargs):
    return subprocess.run(["git", "-C", str(source), *arguments], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs).stdout


def apply(dependency, source, check_only=False):
    source = source.resolve(strict=True)
    if source == ROOT or ROOT in source.parents:
        raise ValueError("The dependency checkout must be outside the project repository")
    top = Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve()
    if top != source:
        raise ValueError("Specify the dependency checkout root")
    bundle = ROOT / "patches" / dependency
    manifest = json.loads((bundle / "manifest.json").read_text())
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    if revision != manifest["revision"]:
        raise ValueError(f"Expected dependency revision {manifest['revision']}, found {revision}")
    if git(source, "status", "--porcelain", "--untracked-files=normal", "--ignore-submodules=all").strip():
        raise ValueError("Use a clean dependency checkout; existing changes will not be overwritten")
    for name, expected in manifest["sha256"].items():
        path = (bundle / name).resolve(strict=True)
        if bundle.resolve() not in path.parents or not path.is_file():
            raise ValueError(f"Invalid patch manifest path: {name}")
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Patch bundle checksum mismatch: {name}")
    patches = manifest["application_order"]
    if not patches or any(name not in manifest["sha256"] or not name.endswith(".patch") for name in patches):
        raise ValueError("Patch application order must contain checksummed patch files")
    # Validate the entire ordered series in a temporary index before touching files.
    with tempfile.TemporaryDirectory(prefix="brawlers-patch-check-") as temporary:
        environment = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / "index"))
        git(source, "read-tree", "HEAD", env=environment)
        for name in patches:
            git(source, "apply", "--cached", "--whitespace=nowarn", str(bundle / name), env=environment)
        combined = git(source, "diff", "--cached", "--binary", env=environment)
        git(source, "apply", "--check", "--whitespace=nowarn", "-", input=combined)
        if not check_only:
            git(source, "apply", "--whitespace=nowarn", "-", input=combined)
    print(f"{'Validated' if check_only else 'Applied'} {len(patches)} {dependency} patches at {revision}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dependency", choices=("rexglue", "prism"))
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="Validate without changing source files")
    args = parser.parse_args()
    try:
        apply(args.dependency, args.source, args.check)
    except subprocess.CalledProcessError as error:
        parser.exit(1, "Dependency patching failed: " + error.stderr.decode(errors="replace") + "\n")
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Dependency patching failed: {error}\n")


if __name__ == "__main__":
    main()
