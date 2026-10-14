"""Original staging fixtures; no game files, native libraries or processes."""

from contextlib import ExitStack
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


TOOLS = Path(__file__).resolve().parents[2] / "tools"
with patch.object(sys, "path", [str(TOOLS), *sys.path]):
    import stage_game


class StagingTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.inputs = {name: self.root / name for name in (
            "native-build", "prism-build", "prism-source", "rexglue-source", "game-dir")}
        for directory in self.inputs.values():
            directory.mkdir()
        self.game_bytes = b"Original synthetic executable placeholder; never run."
        fixtures = {
            "native-build/native/battle-brawlers": b"Synthetic host; never run.",
            "native-build/native/librexruntime.so": b"Synthetic runtime; never load.",
            "native-build/native/librexgpu-xenos.so": b"Synthetic plugin; never load.",
            "prism-build/libprism.so": b"Synthetic speech library; never load.",
            "prism-build/CMakeCache.txt": (
                b"PRISM_ENABLE_ORCA_BACKEND:BOOL=ON\n"
                b"PRISM_ENABLE_SPEECH_DISPATCHER_BACKEND:BOOL=OFF\n"),
            "prism-source/LICENSE": b"Original synthetic license fixture.",
            "prism-source/NOTICE": b"Original synthetic notice fixture.",
            "prism-source/LICENSES/nested/license.txt": b"Original nested license fixture.",
            "rexglue-source/LICENSE": b"Original synthetic SDK license fixture.",
            "game-dir/default.xex": self.game_bytes,
        }
        for relative, data in fixtures.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (self.inputs["native-build"] / "native/battle-brawlers").chmod(0o700)

        stack = ExitStack()
        self.addCleanup(stack.close)
        stack.enter_context(patch.object(stage_game, "EXECUTABLE_SIZE", len(self.game_bytes)))
        stack.enter_context(patch.object(stage_game, "EXECUTABLE_SHA256",
                                        hashlib.sha256(self.game_bytes).hexdigest()))
        stack.enter_context(patch.object(stage_game, "runpath", return_value="$ORIGIN"))
        for function in ("run", "check_output", "Popen"):
            stack.enter_context(patch.object(stage_game.subprocess, function,
                                            side_effect=AssertionError("Process execution forbidden")))
        stack.enter_context(patch("ctypes.CDLL", side_effect=AssertionError("Library loading forbidden")))
        stack.enter_context(patch.object(sys, "stdout", io.StringIO()))
        self.errors = stack.enter_context(patch.object(sys, "stderr", io.StringIO()))

    def snapshot_inputs(self):
        return {str(path.relative_to(self.root)): (
            path.stat().st_mode, path.read_bytes() if path.is_file() else None)
            for directory in self.inputs.values() for path in directory.rglob("*")}

    def stage(self, output):
        arguments = ["stage_game.py"]
        for name, directory in self.inputs.items():
            arguments.extend(["--" + name, str(directory)])
        arguments.extend(["--output", str(output)])
        with patch.object(sys, "argv", arguments):
            stage_game.main()

    def test_output_under_any_input_is_rejected_without_changes(self):
        before = self.snapshot_inputs()
        for name, directory in self.inputs.items():
            with self.subTest(input=name):
                output = directory / "installation"
                with self.assertRaises(SystemExit) as raised:
                    self.stage(output)
                self.assertEqual(raised.exception.code, 1)
                self.assertIn("must be separate", self.errors.getvalue())
                self.assertFalse(output.exists())
                self.assertEqual(self.snapshot_inputs(), before)

    def test_new_installation_is_private_and_preserves_inputs(self):
        before = self.snapshot_inputs()
        output = self.root / "installation"
        previous_mask = os.umask(0o022)
        try:
            self.stage(output)
        finally:
            os.umask(previous_mask)
        self.assertEqual(output.stat().st_mode & 0o777, 0o700)
        self.assertEqual(self.snapshot_inputs(), before)
        self.assertTrue((output / "data").is_symlink())
        self.assertEqual((output / "data").resolve(), self.inputs["game-dir"])
        self.assertEqual((output / "licenses/prism/LICENSES/nested/license.txt").read_bytes(),
                         (self.inputs["prism-source"] / "LICENSES/nested/license.txt").read_bytes())
        manifest = json.loads((output / "local-build.json").read_text())
        self.assertFalse(manifest["game_executed"])
        self.assertTrue(manifest["private_local_build"])

    def test_existing_output_is_not_overwritten(self):
        output = self.root / "installation"
        output.mkdir()
        original = output / "keep.txt"
        original.write_bytes(b"Existing original fixture.")
        before = self.snapshot_inputs()
        with self.assertRaises(SystemExit) as raised:
            self.stage(output)
        self.assertEqual(raised.exception.code, 1)
        self.assertEqual(list(output.iterdir()), [original])
        self.assertEqual(original.read_bytes(), b"Existing original fixture.")
        self.assertEqual(self.snapshot_inputs(), before)


if __name__ == "__main__":
    unittest.main()
