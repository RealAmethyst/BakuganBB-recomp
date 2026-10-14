"""Original process fixtures; never execute or load game code."""

import json
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import launch


class RecordingReporter:
    def __init__(self):
        self.events = []

    def report(self, key, **kwargs):
        self.events.append((key, kwargs))


class SupervisorTests(unittest.TestCase):
    def installation(self, root):
        (root / "bin").mkdir()
        (root / "data").mkdir()
        for name in ["bin/battle-brawlers", "bin/librexruntime.so", "bin/librexgpu-xenos.so",
                     "data/default.xex"]:
            (root / name).touch(mode=0o700)

    def run_fixture(self, script):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reporter = RecordingReporter()
            code = launch.supervise([sys.executable, "-c", script], root,
                                    root / "log.txt", reporter)
            return code, [key for key, _ in reporter.events], (root / "log.txt").read_text()

    def test_successful_process_is_not_reported_as_playable(self):
        code, events, _ = self.run_fixture(
            "print('BRAWLERS_STATUS:thread_created'); print('BRAWLERS_STATUS:guest_exited')")
        self.assertEqual((code, events), (0, ["thread_created", "guest_exited", "closed"]))

    def test_zero_exit_without_launch_is_failure(self):
        code, events, _ = self.run_fixture("pass")
        self.assertEqual((code, events), (1, ["early_exit"]))

    def test_runtime_error_is_failure_even_with_zero_exit(self):
        code, events, _ = self.run_fixture(
            "print('BRAWLERS_STATUS:thread_created'); print('BRAWLERS_STATUS:runtime_error')")
        self.assertEqual(code, 1)
        self.assertIn("runtime_error", events)

    def test_failure_before_host_hooks_is_reported(self):
        code, events, log = self.run_fixture("import sys; print('SDL failed', file=sys.stderr); sys.exit(2)")
        self.assertEqual((code, events), (1, ["process_failed"]))
        self.assertIn("SDL failed", log)

    def test_signal_exit_is_reported(self):
        code, events, log = self.run_fixture("import os, signal; os.kill(os.getpid(), signal.SIGTERM)")
        self.assertEqual((code, events), (1, ["process_failed"]))
        self.assertIn("Host process exit: -15", log)

    def test_error_deduplicates_and_unknown_events_are_not_announced(self):
        code, events, log = self.run_fixture(
            "print('BRAWLERS_STATUS:graphics_failed\\n' * 2, end=''); "
            "print('BRAWLERS_STATUS:invented'); print('prefix BRAWLERS_STATUS:thread_created')")
        self.assertEqual((code, events), (1, ["graphics_failed", "early_exit"]))
        self.assertIn("invented", log)

    def test_non_utf8_logs_are_retained_without_crashing(self):
        code, events, log = self.run_fixture("import os; os.write(2, b'bad byte: \\xff\\n')")
        self.assertEqual(code, 1)
        self.assertIn("bad byte:", log)

    def test_spawn_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reporter = RecordingReporter()
            code = launch.supervise([str(root / "missing")], root, root / "log", reporter)
            self.assertEqual(code, 1)
            self.assertEqual(reporter.events[0][0], "launch_failed")

    def test_unwritable_log_never_spawns(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(subprocess, "Popen") as spawn:
            root = Path(directory)
            reporter = RecordingReporter()
            # An existing directory cannot be opened as a new log file.
            code = launch.supervise(["unused"], root, root, reporter)
            self.assertEqual(code, 1)
            self.assertEqual(reporter.events[0][0], "storage_failed")
            spawn.assert_not_called()

    def test_full_log_keeps_draining_and_waits_for_child(self):
        class FullLog:
            def write(self, text):
                raise OSError("synthetic disk full")

            def close(self):
                pass

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reporter = RecordingReporter()
            script = ("from pathlib import Path; "
                      "print('BRAWLERS_STATUS:thread_created', flush=True); "
                      "print('x' * 200000); Path('finished').touch()")
            with patch.object(Path, "open", return_value=FullLog()):
                code = launch.supervise([sys.executable, "-c", script], root, root / "log", reporter)
            self.assertTrue((root / "finished").exists())
            self.assertEqual(code, 1)
            events = [key for key, _ in reporter.events]
            self.assertEqual(events, ["log_failed", "thread_created", "closed"])

    def test_incomplete_installation_never_spawns(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(subprocess, "Popen") as spawn:
            reporter = RecordingReporter()
            self.assertEqual(launch.launch(Path(directory), reporter), 1)
            spawn.assert_not_called()
            self.assertEqual(reporter.events[0][0], "missing_files")

    def test_check_never_spawns_or_creates_user_data(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(subprocess, "Popen") as spawn:
            root = Path(directory)
            self.installation(root)
            self.assertEqual(launch.launch(root, RecordingReporter(), check=True), 0)
            spawn.assert_not_called()
            self.assertFalse((root / "user-data").exists())

    def test_storage_setup_failure_is_reported_without_spawning(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(subprocess, "Popen") as spawn:
            root = Path(directory)
            self.installation(root)
            (root / "user-data").touch()
            (root / "messages.en.json").write_text(
                Path(__file__).with_name("messages.en.json").read_text())
            with patch.object(launch, "__file__", str(root / "launch.py")), \
                    patch.object(sys, "argv", ["launch.py", "--no-speech"]), \
                    patch.object(sys, "stdout", new_callable=io.StringIO) as output:
                self.assertEqual(launch.main(), 1)
                self.assertIn("writable storage", output.getvalue())
            spawn.assert_not_called()

    def test_existing_launch_lock_prevents_second_process(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(subprocess, "Popen") as spawn:
            root = Path(directory)
            self.installation(root)
            (root / "user-data").mkdir()
            reporter = RecordingReporter()
            with (root / "user-data/launch.lock").open("a") as lock:
                launch.fcntl.flock(lock, launch.fcntl.LOCK_EX | launch.fcntl.LOCK_NB)
                self.assertEqual(launch.launch(root, reporter), 1)
            self.assertEqual(reporter.events[0][0], "already_running")
            spawn.assert_not_called()

    def test_all_events_have_localized_messages(self):
        messages = json.loads(Path(__file__).with_name("messages.en.json").read_text())
        self.assertTrue(launch.EVENTS <= messages.keys())


if __name__ == "__main__":
    unittest.main()
