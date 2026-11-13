# SPDX-License-Identifier: BSD-3-Clause
"""Shared process supervision and speech policy for private native builds."""

import subprocess

from keyboard_speech import KeyboardSpeech, PREFIX as UI_PREFIX, parse_event
from screen_speech import ScreenSpeech, PREFIX as SCREEN_PREFIX, parse_event as parse_screen


PREFIX = "BRAWLERS_STATUS:"
EVENTS = {
    "environment", "graphics", "runtime", "image_loaded", "preparing_thread",
    "thread_created", "guest_exited", "environment_failed", "graphics_failed",
    "runtime_failed", "runtime_error", "audio_unavailable", "spatial_audio_unavailable",
    "settings_unavailable", "settings_save_failed_on_exit", "settings_apply_failed_on_exit",
}
SPOKEN_EVENTS = {"environment_failed", "graphics_failed", "runtime_failed", "runtime_error",
                 "audio_unavailable", "spatial_audio_unavailable", "settings_unavailable",
                 "settings_save_failed_on_exit", "settings_apply_failed_on_exit"}


class Reporter:
    def __init__(self, messages, speech=None):
        self.messages = messages
        self.speech = speech
        self.dialogs = KeyboardSpeech(messages, speech)
        self.screens = ScreenSpeech(messages, speech)

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
                if line.startswith(SCREEN_PREFIX):
                    event = parse_screen(line[len(SCREEN_PREFIX):])
                    if event is None:
                        record("BRAWLERS_SCREEN:invalid event; text omitted\n")
                    else:
                        record(f"BRAWLERS_SCREEN:{event['source']} {event['kind']} "
                               f"id={event['id']}; text omitted\n")
                        screens = getattr(reporter, "screens", None)
                        if screens is not None:
                            screens.handle(event)
                    continue
                if line.startswith(UI_PREFIX):
                    event = parse_event(line[len(UI_PREFIX):])
                    if event is None:
                        record("BRAWLERS_UI:invalid event; text omitted\n")
                    else:
                        record(f"BRAWLERS_UI:{event['kind']} id={event['id']}; text omitted\n")
                        dialogs = getattr(reporter, "dialogs", None)
                        if dialogs is not None:
                            dialogs.handle(event)
                    continue
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
