#!/usr/bin/env python3
"""Supervise a private native build; importing this module never launches it."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from prism_speech import PrismSpeech


PREFIX = "BRAWLERS_STATUS:"
EVENTS = {
    "environment", "graphics", "runtime", "image_loaded", "preparing_thread",
    "thread_created", "guest_exited", "environment_failed", "graphics_failed",
    "runtime_failed", "runtime_error", "audio_unavailable",
}
SPOKEN_EVENTS = {"environment_failed", "graphics_failed", "runtime_failed", "runtime_error",
                 "audio_unavailable"}


class Reporter:
    def __init__(self, messages, speech=None):
        self.messages = messages
        self.speech = speech

    def report(self, key, *, speak=True, **values):
        text = self.messages[key].format(**values)
        print(text, flush=True)
        if not speak or self.speech is None:
            return
        self.speech.speak(text, interrupt=False)


def supervise(command, cwd, log_path, reporter):
    """Return failure for explicit runtime errors even with a zero child exit."""
    seen = set()
    try:
        log = log_path.open("x", encoding="utf-8")
    except OSError:
        reporter.report("storage_failed")
        return 1
    storage_failed = False

    def record(text):
        nonlocal storage_failed
        if storage_failed:
            return
        try:
            log.write(text)
            log.flush()
        except OSError:
            storage_failed = True
            reporter.report("log_failed")

    try:
        try:
            child = subprocess.Popen(
                command, cwd=cwd, stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace", bufsize=1,
            )
        except OSError as error:
            record(f"Process creation failed: {error}\n")
            reporter.report("launch_failed")
            return 1
        with child.stdout:
            for line in child.stdout:
                # Continue draining and owning the process if the disk fills.
                record(line)
                if not line.startswith(PREFIX):
                    continue
                event = line[len(PREFIX):].strip()
                if event in EVENTS and event not in seen:
                    seen.add(event)
                    reporter.report(event, speak=event in SPOKEN_EVENTS)
        code = child.wait()
        record(f"Host process exit: {code}\n")
    finally:
        try:
            log.close()
        except OSError:
            if not storage_failed:
                storage_failed = True
                reporter.report("log_failed")
    if code != 0:
        reporter.report("process_failed")
        return 1
    if "thread_created" not in seen:
        reporter.report("early_exit")
        return 1
    reporter.report("closed")
    return int(storage_failed or bool(seen & SPOKEN_EVENTS))


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
            speech = PrismSpeech(root / "lib" / "libprism.so")
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


def launch(root, reporter, *, check=False):
    host = root / "bin" / "battle-brawlers"
    required = [host, root / "bin/librexruntime.so", root / "bin/librexgpu-xenos.so",
                root / "data/default.xex"]
    if any(not path.is_file() for path in required) or not os.access(host, os.X_OK):
        reporter.report("missing_files")
        return 1
    if check:
        return 0
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
        command = [str(host), f"--game_data_root={root / 'data'}",
                   f"--user_data_root={user}", f"--cache_root={user / 'cache'}",
                   f"--log_file={session / 'runtime.log'}", "--log_level=info"]
        return supervise(command, root, log, reporter)


if __name__ == "__main__":
    sys.exit(main())
