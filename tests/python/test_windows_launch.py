"""Original Windows installation fixtures; never load game or speech libraries."""

import errno
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_launch import MESSAGES, RecordingReporter, synthetic_game, synthetic_identity


SOURCE = Path(__file__).resolve().parents[2]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# The installed layout is flat; keep this platform's dialog module out of other tests.
with patch.dict(sys.modules), patch.object(
        sys, "path", [str(SOURCE / "src/platform/windows"), *sys.path]):
    sys.modules.pop("folder_dialog", None)
    launch = load_module("windows_launch", SOURCE / "src/platform/windows/launch.py")
staging = load_module("stage_launcher", SOURCE / "tools/stage_launcher.py")


class WindowsLauncherTests(unittest.TestCase):
    def setUp(self):
        synthetic_identity(self)

    def installation(self, parent):
        parent = parent.resolve()
        root = parent / "synthetic install"
        data = parent / "synthetic game \u00e6"
        (root / "bin").mkdir(parents=True)
        synthetic_game(data)
        for name in ("battle-brawlers.exe", "rexruntime.dll", "rexgpu-xenos.dll", *launch.VC_RUNTIME):
            (root / "bin" / name).touch()
        (root / "game-data.json").write_text(json.dumps({"path": str(data)}), encoding="utf-8")
        (root / "messages.en.json").write_text(MESSAGES.read_text(encoding="utf-8"), encoding="utf-8")
        return root, data

    def test_check_never_loads_speech_spawns_or_creates_user_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            with patch.object(launch, "__file__", str(root / "launch.py")), \
                    patch.object(sys, "argv", ["launch.py", "--check"]), \
                    patch.object(launch, "PrismSpeech") as speech, \
                    patch.object(subprocess, "Popen") as spawn, \
                    patch.object(launch, "lock_installation") as lock:
                self.assertEqual(launch.main(), 0)
            speech.assert_not_called()
            spawn.assert_not_called()
            lock.assert_not_called()
            self.assertFalse((root / "user-data").exists())

    def test_missing_or_invalid_game_config_fails_before_launch(self):
        configs = [None, "{", "[]", "null", "{}", '{"path":1}',
                   '{"path":"relative"}', '{"path":""}', '{"path":"\\u0000"}']
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            for config in configs:
                with self.subTest(config=config):
                    path = root / "game-data.json"
                    if config is None:
                        path.unlink()
                    else:
                        path.write_text(config, encoding="utf-8")
                    reporter = RecordingReporter()
                    with patch.object(launch, "supervise") as supervise, \
                            patch.object(launch, "request") as request:
                        self.assertEqual(launch.launch(root, reporter, check=True), 1)
                    supervise.assert_not_called()
                    request.assert_not_called()
                    self.assertEqual([key for key, _ in reporter.events], ["game_files_not_chosen"])
                    self.assertFalse((root / "user-data").exists())

    def test_first_launch_asks_and_uses_the_chosen_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self.installation(Path(directory))
            (root / "game-data.json").unlink()
            reporter = RecordingReporter()
            with patch.object(launch, "request", return_value=data) as request, \
                    patch.object(launch, "lock_installation"), \
                    patch.object(launch, "unlock_installation"), \
                    patch.object(launch, "supervise", return_value=0) as supervise:
                self.assertEqual(launch.launch(root, reporter), 0)
            request.assert_called_once_with(root, reporter, launch.choose_directory)
            self.assertIn(f"--game_data_root={data}", supervise.call_args.args[0])

    def test_cancelled_request_or_refused_option_never_launches(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            (root / "game-data.json").unlink()
            with patch.object(launch, "supervise") as supervise:
                with patch.object(launch, "request", return_value=None):
                    self.assertEqual(launch.launch(root, RecordingReporter()), 1)
                reporter = RecordingReporter()
                with patch.object(launch, "accept", return_value=None) as accept:
                    self.assertEqual(launch.launch(root, reporter, game_dir="synthetic choice"), 1)
                accept.assert_called_once_with(root, reporter, "synthetic choice")
            supervise.assert_not_called()
            self.assertFalse((root / "user-data").exists())

    def test_missing_native_file_or_executable_fails_check(self):
        for name in ("bin/battle-brawlers.exe", "bin/rexruntime.dll", "bin/rexgpu-xenos.dll"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root, _ = self.installation(Path(directory))
                (root / name).unlink()
                reporter = RecordingReporter()
                self.assertEqual(launch.launch(root, reporter, check=True), 1)
                self.assertEqual(reporter.events[0][0], "missing_files")

    def test_missing_compiler_runtime_is_reported_before_any_request(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            system = Path(directory) / "synthetic windows"
            (system / "System32").mkdir(parents=True)
            with patch.dict(launch.os.environ, {"SystemRoot": str(system)}):
                self.assertTrue(launch.vc_runtime_present(root))
                (root / "bin" / launch.VC_RUNTIME[1]).unlink()
                reporter = RecordingReporter()
                with patch.object(launch, "request") as request, \
                        patch.object(launch, "supervise") as supervise:
                    self.assertEqual(launch.launch(root, reporter), 1)
                request.assert_not_called()
                supervise.assert_not_called()
                self.assertEqual([key for key, _ in reporter.events], ["vc_runtime_missing"])
                self.assertIn("vc_runtime_missing", reporter.messages)
                # The redistributable's system copy satisfies the same import.
                (system / "System32" / launch.VC_RUNTIME[1]).touch()
                self.assertTrue(launch.vc_runtime_present(root))

    def test_missing_game_executable_fails_check(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self.installation(Path(directory))
            (data / "default.xex").unlink()
            self.assertEqual(launch.launch(root, RecordingReporter(), check=True), 1)

    def test_command_preserves_external_game_path_and_private_user_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self.installation(Path(directory))
            reporter = RecordingReporter()
            with patch.object(launch, "lock_installation") as lock, \
                    patch.object(launch, "unlock_installation") as unlock, \
                    patch.object(launch, "supervise", return_value=7) as supervise:
                self.assertEqual(launch.launch(root, reporter), 7)
            lock.assert_called_once()
            unlock.assert_called_once_with(lock.call_args.args[0])
            command, cwd, log, actual_reporter = supervise.call_args.args
            self.assertEqual(command, [str(root / "bin/battle-brawlers.exe"),
                                      f"--game_data_root={data}",
                                      f"--user_data_root={root / 'user-data'}",
                                      f"--cache_root={root / 'user-data/cache'}",
                                      f"--log_file={log.parent / 'runtime.log'}",
                                      "--log_level=info"])
            self.assertEqual(cwd, root)
            self.assertEqual(log.parent.parent, root / "user-data/logs")
            self.assertIs(actual_reporter, reporter)

    def test_lock_storage_error_is_not_reported_as_another_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            with patch.object(launch, "__file__", str(root / "launch.py")), \
                    patch.object(sys, "argv", ["launch.py", "--no-speech"]), \
                    patch.object(launch, "lock_installation", side_effect=OSError(errno.EIO, "synthetic")), \
                    patch.object(launch, "supervise") as supervise, \
                    patch.object(sys, "stdout", new_callable=io.StringIO) as output:
                self.assertEqual(launch.main(), 1)
            self.assertIn("writable storage", output.getvalue())
            supervise.assert_not_called()

    def test_main_uses_windows_prism_path_and_closes_mocked_speech(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            with patch.object(launch, "__file__", str(root / "launch.py")), \
                    patch.object(sys, "argv", ["launch.py"]), \
                    patch.object(launch, "PrismSpeech") as speech, \
                    patch.object(launch, "launch", return_value=0):
                self.assertEqual(launch.main(), 0)
            speech.assert_called_once_with(root / "lib/prism.dll")
            speech.return_value.close.assert_called_once()

    @unittest.skipUnless(sys.platform == "win32", "Requires real Windows CRT file locking")
    def test_other_process_lock_prevents_launch_and_closing_releases_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.installation(Path(directory))
            (root / "user-data").mkdir()
            script = ("import msvcrt, sys; "
                      "lock = open(sys.argv[1], 'a+b'); lock.seek(0); "
                      "msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1); "
                      "print('ready', flush=True); sys.stdin.readline(); "
                      "lock.seek(0); msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1); lock.close()")
            with subprocess.Popen([sys.executable, "-c", script, str(root / "user-data/launch.lock")],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True) as holder:
                try:
                    self.assertEqual(holder.stdout.readline().strip(), "ready")
                    reporter = RecordingReporter()
                    with patch.object(launch, "supervise") as supervise:
                        self.assertEqual(launch.launch(root, reporter), 1)
                    self.assertEqual(reporter.events[0][0], "already_running")
                    supervise.assert_not_called()
                finally:
                    holder.communicate("release\n", timeout=10)
                self.assertEqual(holder.returncode, 0)
            # Each normal launch closes its handle, including failed supervision.
            for result in (1, 0):
                with patch.object(launch, "supervise", return_value=result) as supervise:
                    self.assertEqual(launch.launch(root, RecordingReporter()), result)
                supervise.assert_called_once()


class LauncherStagingTests(unittest.TestCase):
    def test_each_platform_stages_selected_entrypoint_and_shared_policy(self):
        for platform in ("linux", "windows"):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as directory:
                destination = Path(directory) / "install"
                staging.stage(destination, platform=platform)
                source = SOURCE / "src/platform" / platform / "launch.py"
                self.assertEqual((destination / "launch.py").read_bytes(), source.read_bytes())
                self.assertEqual((destination / "folder_dialog.py").read_bytes(),
                                 (source.parent / "folder_dialog.py").read_bytes())
                for shared in ("launch_support.py", "game_files.py"):
                    self.assertEqual((destination / shared).read_bytes(),
                                     (SOURCE / "src/platform/common" / shared).read_bytes())
                self.assertTrue((destination / "prism_speech.py").is_file())
                self.assertTrue((destination / "messages.en.json").is_file())
                self.assertTrue((destination / "NOTICE.md").is_file())

    def test_default_remains_linux_and_nonempty_destination_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "install"
            staging.stage(destination)
            expected = (SOURCE / "src/platform/linux/launch.py").read_bytes()
            self.assertEqual((destination / "launch.py").read_bytes(), expected)
            with self.assertRaises(ValueError):
                staging.stage(destination, platform="windows")
            self.assertEqual((destination / "launch.py").read_bytes(), expected)

    def test_invalid_platform_does_not_create_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "install"
            with self.assertRaises(ValueError):
                staging.stage(destination, platform="unsupported")
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
