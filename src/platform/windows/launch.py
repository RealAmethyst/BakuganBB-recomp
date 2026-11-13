#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Supervise a private Windows native build; importing never launches it."""

import argparse
import errno
import json
import os
from pathlib import Path
import sys
import tempfile

from folder_dialog import choose_directory
from game_files import accept, request, saved_directory
from prism_speech import PrismSpeech
from launch_support import EVENTS, PREFIX, SPOKEN_EVENTS, Reporter, supervise


# Imported by the native binaries and installed by Microsoft's Visual C++
# Redistributable; Windows itself does not include them.
VC_RUNTIME = ("msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll")


def vc_runtime_present(root):
    system = Path(os.environ.get("SystemRoot", "")) / "System32"
    return all((root / "bin" / name).is_file() or (system / name).is_file() for name in VC_RUNTIME)


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


def launch(root, reporter, *, check=False, game_dir=None):
    host = root / "bin" / "battle-brawlers.exe"
    required = [host, root / "bin/rexruntime.dll", root / "bin/rexgpu-xenos.dll"]
    if any(not path.is_file() for path in required):
        reporter.report("missing_files")
        return 1
    if not vc_runtime_present(root):
        reporter.report("vc_runtime_missing")
        return 1
    if game_dir is not None:
        data = accept(root, reporter, game_dir)
        if data is None:
            return 1
    else:
        data = saved_directory(root)
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
    parser.add_argument("--game-dir", help="Directory containing your own game files; verified and remembered")
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
        return launch(root, reporter, check=args.check, game_dir=args.game_dir)
    except OSError:
        reporter.report("storage_failed")
        return 1
    finally:
        if speech is not None:
            speech.close()


if __name__ == "__main__":
    sys.exit(main())
