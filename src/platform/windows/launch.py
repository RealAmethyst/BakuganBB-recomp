#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Supervise a private Windows native build; importing never launches it."""

import argparse
import errno
import json
from pathlib import Path
import sys
import tempfile

from prism_speech import PrismSpeech
from launch_support import EVENTS, PREFIX, SPOKEN_EVENTS, Reporter, supervise


def game_directory(root):
    try:
        config = json.loads((root / "game-data.json").read_text(encoding="utf-8"))
        if not isinstance(config, dict) or not isinstance(config.get("path"), str):
            return None
        path = Path(config["path"])
        if path.is_absolute() and (path / "default.xex").is_file():
            return path
    except (OSError, ValueError):
        pass
    return None


def lock_installation(lock):
    import msvcrt

    # Lock the same byte even when the file is empty or was previously used.
    # The CRT permits locking beyond EOF; LK_NBLCK never waits for another launch.
    lock.seek(0)
    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)


def unlock_installation(lock):
    import msvcrt

    lock.seek(0)
    msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def launch(root, reporter, *, check=False):
    host = root / "bin" / "battle-brawlers.exe"
    required = [host, root / "bin/rexruntime.dll", root / "bin/rexgpu-xenos.dll"]
    data = game_directory(root)
    if data is None or any(not path.is_file() for path in required):
        reporter.report("missing_files")
        return 1
    if check:
        return 0
    user = root / "user-data"
    logs = user / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    with (user / "launch.lock").open("a+b") as lock:
        try:
            lock_installation(lock)
        except OSError as error:
            if error.errno != errno.EACCES:
                raise
            reporter.report("already_running")
            return 1
        try:
            session = Path(tempfile.mkdtemp(prefix="launch-", dir=logs))
            log = session / "console.log"
            reporter.report("starting")
            reporter.report("logs", speak=False, path=log)
            command = [str(host), f"--game_data_root={data}",
                       f"--user_data_root={user}", f"--cache_root={user / 'cache'}",
                       f"--log_file={session / 'runtime.log'}", "--log_level=info"]
            return supervise(command, root, log, reporter)
        finally:
            unlock_installation(lock)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-speech", action="store_true")
    parser.add_argument("--check", action="store_true", help="Check files without launching or speaking")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    messages = json.loads((root / "messages.en.json").read_text(encoding="utf-8"))
    speech = None
    if not (args.no_speech or args.check):
        try:
            speech = PrismSpeech(root / "lib" / "prism.dll")
        except (OSError, RuntimeError) as error:
            print(messages["speech_unavailable"], flush=True)
            print(str(error), file=sys.stderr)
    reporter = Reporter(messages, speech=speech)
    try:
        return launch(root, reporter, check=args.check)
    except OSError:
        reporter.report("storage_failed")
        return 1
    finally:
        if speech is not None:
            speech.close()


if __name__ == "__main__":
    sys.exit(main())
