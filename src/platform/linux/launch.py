#!/usr/bin/env python3
"""Supervise a private native build; importing this module never launches it."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import sys
import tempfile

from folder_dialog import choose_directory
from game_files import accept, request, saved_directory
from prism_speech import PrismSpeech
from launch_support import EVENTS, PREFIX, SPOKEN_EVENTS, Reporter, supervise


def game_directory(root):
    """Prefer the remembered directory, then a locally staged data link."""
    saved = saved_directory(root)
    if saved is not None:
        return saved
    staged = root / "data"
    return staged if (staged / "default.xex").is_file() else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-speech", action="store_true")
    parser.add_argument("--check", action="store_true", help="Check files without launching or speaking")
    parser.add_argument("--game-dir", help="Directory containing your own game files; verified and remembered")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    messages = json.loads((root / "messages.en.json").read_text(encoding="utf-8"))
    speech = None
    if not (args.no_speech or args.check):
        try:
            speech = PrismSpeech(root / "lib" / "libprism.so")
        except (OSError, RuntimeError) as error:
            print(messages["speech_unavailable"], flush=True)
            print(str(error), file=sys.stderr)
    reporter = Reporter(messages, speech=speech)
    try:
        return launch(root, reporter, check=args.check, game_dir=args.game_dir)
    except OSError:
        reporter.report("storage_failed")
        return 1
    finally:
        if speech is not None:
            speech.close()


def launch(root, reporter, *, check=False, game_dir=None):
    host = root / "bin" / "battle-brawlers"
    required = [host, root / "bin/librexruntime.so", root / "bin/librexgpu-xenos.so"]
    if any(not path.is_file() for path in required) or not os.access(host, os.X_OK):
        reporter.report("missing_files")
        return 1
    if game_dir is not None:
        data = accept(root, reporter, game_dir)
        if data is None:
            return 1
    else:
        data = game_directory(root)
    if data is None and check:
        reporter.report("game_files_not_chosen")
        return 1
    if check:
        return 0
    if data is None:
        data = request(root, reporter, choose_directory)
        if data is None:
            return 1
    user = root / "user-data"
    logs = user / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    with (user / "launch.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            reporter.report("already_running")
            return 1
        session = Path(tempfile.mkdtemp(prefix="launch-", dir=logs))
        log = session / "console.log"
        reporter.report("starting")
        reporter.report("logs", speak=False, path=log)
        command = [str(host), f"--game_data_root={data}",
                   f"--user_data_root={user}", f"--cache_root={user / 'cache'}",
                   f"--log_file={session / 'runtime.log'}", "--log_level=info"]
        return supervise(command, root, log, reporter)


if __name__ == "__main__":
    sys.exit(main())
