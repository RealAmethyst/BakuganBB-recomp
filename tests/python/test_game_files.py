"""Original game-directory fixtures; no game file, dialog or process is used."""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import game_files
from test_launch import RecordingReporter


SOURCE = Path(__file__).resolve().parents[2]
PLACEHOLDER = b"Original synthetic executable placeholder; never run."


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class GameDirectoryFixture(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.parent = Path(temporary.name).resolve()
        self.root = self.parent / "synthetic install"
        self.root.mkdir()
        self.game = self.game_directory("synthetic game \u00e6")
        for name, value in (("EXECUTABLE_SIZE", len(PLACEHOLDER)),
                            ("EXECUTABLE_SHA256", hashlib.sha256(PLACEHOLDER).hexdigest())):
            patcher = patch.object(game_files, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def game_directory(self, name, executable=PLACEHOLDER, media=True):
        directory = self.parent / name
        directory.mkdir()
        if executable is not None:
            (directory / "default.xex").write_bytes(executable)
        if media:
            (directory / "media").mkdir()
        return directory

    def events(self, reporter):
        return [key for key, _ in reporter.events]


class InspectionTests(GameDirectoryFixture):
    def test_supported_directory_is_accepted(self):
        self.assertEqual(game_files.inspect(self.game), "ok")

    def test_absent_executable_or_media_is_missing(self):
        self.assertEqual(game_files.inspect(self.game_directory("no executable", executable=None)), "missing")
        self.assertEqual(game_files.inspect(self.game_directory("no media", media=False)), "missing")
        self.assertEqual(game_files.inspect(self.parent / "absent"), "missing")
        self.assertEqual(game_files.inspect(Path("relative")), "missing")

    def test_other_size_or_content_is_unsupported(self):
        changed = PLACEHOLDER[:-1] + b"?"
        self.assertEqual(game_files.inspect(self.game_directory("changed", executable=changed)), "unsupported")
        self.assertEqual(game_files.inspect(self.game_directory("longer", executable=PLACEHOLDER + b"!")),
                         "unsupported")

    def test_launcher_and_generator_share_one_executable_identity(self):
        generator = load_module("generate_game_identity", SOURCE / "tools/generate_game.py")
        original = load_module("game_files_identity", SOURCE / "src/platform/common/game_files.py")
        self.assertEqual((original.EXECUTABLE_SIZE, original.EXECUTABLE_SHA256),
                         (generator.EXECUTABLE_SIZE, generator.EXECUTABLE_SHA256))


class RememberedDirectoryTests(GameDirectoryFixture):
    def test_accepted_directory_is_remembered_and_reported(self):
        reporter = RecordingReporter()
        self.assertEqual(game_files.accept(self.root, reporter, str(self.game)), self.game)
        self.assertEqual(self.events(reporter), ["game_files_verified"])
        self.assertEqual(game_files.saved_directory(self.root), self.game)
        self.assertEqual(json.loads((self.root / "game-data.json").read_text(encoding="utf-8")),
                         {"path": str(self.game)})
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), ["game-data.json"])

    def test_refused_directory_leaves_previous_choice(self):
        game_files.remember(self.root, self.game)
        for name, kwargs, event in (("other version", {"executable": b"different"}, "game_files_unsupported"),
                                    ("incomplete", {"media": False}, "game_files_missing")):
            with self.subTest(name=name):
                reporter = RecordingReporter()
                candidate = self.game_directory(name, **kwargs)
                self.assertIsNone(game_files.accept(self.root, reporter, str(candidate)))
                self.assertEqual(self.events(reporter), [event])
                self.assertEqual(game_files.saved_directory(self.root), self.game)

    def test_invalid_or_stale_configuration_is_not_used(self):
        configs = [None, "{", "[]", "null", "{}", '{"path":1}', '{"path":"relative"}',
                   '{"path":""}', '{"path":"\\u0000"}', json.dumps({"path": str(self.parent / "gone")})]
        for config in configs:
            with self.subTest(config=config):
                path = self.root / "game-data.json"
                if config is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_text(config, encoding="utf-8")
                self.assertIsNone(game_files.saved_directory(self.root))


class RequestTests(GameDirectoryFixture):
    def test_title_comes_from_central_messages_and_choice_is_remembered(self):
        reporter = RecordingReporter()
        titles = []

        def choose(title):
            titles.append(title)
            return str(self.game)

        self.assertEqual(game_files.request(self.root, reporter, choose), self.game)
        self.assertEqual(titles, [reporter.messages["game_files_dialog_title"]])
        self.assertEqual(self.events(reporter), ["game_files_needed", "game_files_verified"])

    def test_refused_choice_explains_and_asks_again(self):
        reporter = RecordingReporter()
        choices = iter([str(self.game_directory("other", executable=b"different")),
                        str(self.game_directory("empty", executable=None)), str(self.game)])
        self.assertEqual(game_files.request(self.root, reporter, lambda title: next(choices)), self.game)
        self.assertEqual(self.events(reporter), ["game_files_needed", "game_files_unsupported",
                                                 "game_files_missing", "game_files_verified"])

    def test_cancel_reports_and_remembers_nothing(self):
        reporter = RecordingReporter()
        self.assertIsNone(game_files.request(self.root, reporter, lambda title: None))
        self.assertEqual(self.events(reporter), ["game_files_needed", "game_files_cancelled"])
        self.assertFalse((self.root / "game-data.json").exists())

    def unavailable(self, title):
        raise game_files.DialogUnavailable("synthetic")

    def console(self, text, interactive=True):
        stream = io.StringIO(text)
        stream.isatty = lambda: interactive
        return patch.object(sys, "stdin", stream)

    def test_console_replaces_unavailable_dialog_and_accepts_quoted_path(self):
        reporter = RecordingReporter()
        with self.console(f'  "{self.game}"  \n'):
            self.assertEqual(game_files.request(self.root, reporter, self.unavailable), self.game)
        self.assertEqual(self.events(reporter), ["game_files_needed", "game_files_prompt",
                                                 "game_files_verified"])

    def test_console_refusal_asks_again_and_empty_line_cancels(self):
        reporter = RecordingReporter()
        with self.console(str(self.parent / "absent") + "\n\n"):
            self.assertIsNone(game_files.request(self.root, reporter, self.unavailable))
        self.assertEqual(self.events(reporter), ["game_files_needed", "game_files_prompt",
                                                 "game_files_missing", "game_files_prompt",
                                                 "game_files_cancelled"])

    def test_closed_console_cancels(self):
        reporter = RecordingReporter()
        with self.console(""):
            self.assertIsNone(game_files.request(self.root, reporter, self.unavailable))
        self.assertEqual(self.events(reporter)[-1], "game_files_cancelled")

    def test_no_dialog_and_no_console_names_the_command_line_option(self):
        for stream in ("redirected", None):
            with self.subTest(stream=stream):
                reporter = RecordingReporter()
                context = self.console("", interactive=False) if stream else patch.object(sys, "stdin", None)
                with context:
                    self.assertIsNone(game_files.request(self.root, reporter, self.unavailable))
                self.assertEqual(self.events(reporter), ["game_files_needed",
                                                         "game_files_dialog_unavailable"])

    def test_every_reported_key_has_central_wording(self):
        messages = RecordingReporter().messages
        for key in ("needed", "dialog_title", "prompt", "missing", "unsupported", "verified",
                    "cancelled", "dialog_unavailable", "not_chosen"):
            self.assertIn("game_files_" + key, messages)


@unittest.skipUnless(sys.platform == "linux", "Linux desktop dialog programs")
class LinuxDialogTests(unittest.TestCase):
    def setUp(self):
        self.dialog = load_module("linux_folder_dialog", SOURCE / "src/platform/linux/folder_dialog.py")

    def run_dialog(self, *, environment, installed, results):
        calls = []

        def run(command, **kwargs):
            calls.append(command)
            self.assertIs(kwargs["stdin"], self.dialog.subprocess.DEVNULL)
            code, output = results[command[0]]
            return self.dialog.subprocess.CompletedProcess(command, code, output)

        with patch.dict(self.dialog.os.environ, environment, clear=True), \
                patch.object(self.dialog.shutil, "which", side_effect=lambda name: name if name in installed else None), \
                patch.object(self.dialog.subprocess, "run", side_effect=run):
            try:
                return self.dialog.choose_directory("Synthetic title"), calls
            except game_files.DialogUnavailable:
                return game_files.DialogUnavailable, calls

    def test_session_without_display_never_starts_a_program(self):
        result, calls = self.run_dialog(environment={}, installed={"zenity", "kdialog"}, results={})
        self.assertIs(result, game_files.DialogUnavailable)
        self.assertEqual(calls, [])

    def test_desktop_toolkit_is_preferred_and_title_is_passed(self):
        for desktop, expected in (("KDE", "kdialog"), ("ubuntu:GNOME", "zenity"), ("", "zenity")):
            with self.subTest(desktop=desktop):
                result, calls = self.run_dialog(
                    environment={"WAYLAND_DISPLAY": "synthetic", "XDG_CURRENT_DESKTOP": desktop},
                    installed={"zenity", "kdialog"},
                    results={"zenity": (0, "/synthetic/zenity\n"), "kdialog": (0, "/synthetic/kdialog\n")})
                self.assertEqual(result, "/synthetic/" + expected)
                self.assertEqual([command[0] for command in calls], [expected])
                self.assertTrue(any("Synthetic title" in argument for argument in calls[0]))

    def test_cancel_is_not_retried_with_another_program(self):
        result, calls = self.run_dialog(environment={"DISPLAY": ":synthetic"}, installed={"zenity", "kdialog"},
                                        results={"zenity": (1, ""), "kdialog": (0, "/unexpected\n")})
        self.assertIsNone(result)
        self.assertEqual(len(calls), 1)

    def test_failed_or_absent_programs_fall_through_to_unavailable(self):
        result, calls = self.run_dialog(environment={"DISPLAY": ":synthetic"}, installed={"zenity", "kdialog"},
                                        results={"zenity": (255, ""), "kdialog": (0, "/synthetic\n")})
        self.assertEqual(result, "/synthetic")
        self.assertEqual([command[0] for command in calls], ["zenity", "kdialog"])
        result, calls = self.run_dialog(environment={"DISPLAY": ":synthetic"}, installed=set(), results={})
        self.assertIs(result, game_files.DialogUnavailable)
        self.assertEqual(calls, [])


class WindowsDialogTests(unittest.TestCase):
    """The shell call itself needs Windows; only its thread boundary is checked."""

    def setUp(self):
        self.dialog = load_module("windows_folder_dialog", SOURCE / "src/platform/windows/folder_dialog.py")

    def test_result_and_cancel_cross_the_dialog_thread(self):
        for value in ("C:/synthetic game", None):
            with self.subTest(value=value), patch.object(self.dialog, "browse", return_value=value) as browse:
                self.assertEqual(self.dialog.choose_directory("Synthetic title"), value)
                browse.assert_called_once_with("Synthetic title")

    def test_shell_failure_becomes_unavailable(self):
        for error in (OSError("synthetic"), AttributeError("synthetic"), TypeError("synthetic"),
                      game_files.DialogUnavailable("synthetic")):
            with self.subTest(error=error), patch.object(self.dialog, "browse", side_effect=error):
                with self.assertRaises(game_files.DialogUnavailable):
                    self.dialog.choose_directory("Synthetic title")


if __name__ == "__main__":
    unittest.main()
