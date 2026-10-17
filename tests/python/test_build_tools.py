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


class StagingFixture(unittest.TestCase):
    platform = "linux"

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
        self.stack = stack
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
        arguments.extend(["--output", str(output), "--platform", self.platform])
        with patch.object(sys, "argv", arguments):
            stage_game.main()


class StagingTests(StagingFixture):
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

    @unittest.skipUnless(sys.platform == "linux", "Linux permissions and symlink installation")
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


class WindowsStagingTests(StagingTests):
    platform = "windows"

    def setUp(self):
        super().setUp()
        self.native = self.inputs["native-build"] / "native"
        self.windows_files = {
            "bin/battle-brawlers.exe": self.native / "battle-brawlers.exe",
            "bin/rexruntime.dll": self.native / "rexruntime.dll",
            "bin/rexgpu-xenos.dll": self.native / "rexgpu-xenos.dll",
            "lib/prism.dll": self.inputs["prism-build"] / "prism.dll",
        }
        for path in self.windows_files.values():
            path.write_bytes(b"Original synthetic Windows binary placeholder; never loaded.")
        self.cache = self.inputs["prism-build"] / "CMakeCache.txt"
        self.cache.write_text("".join(f"PRISM_ENABLE_{name}_BACKEND:STRING=ON\n"
                                      for name in sorted(stage_game.WINDOWS_BACKENDS)) +
                              "PRISM_ENABLE_ORCA_BACKEND:BOOL=OFF\n"
                              "PRISM_ENABLE_SAPI_BACKEND:STRING=OFF\n", encoding="utf-8")
        self.audit = self.stack.enter_context(patch.object(
            stage_game, "audit_windows_runtime", return_value=dict(self.windows_files)))

    def test_new_installation_is_private_and_preserves_inputs(self):
        before = self.snapshot_inputs()
        output = self.root / "installation with spaces \u00e6"
        with patch.object(Path, "symlink_to", side_effect=AssertionError("No Windows symlinks")):
            self.stage(output)
        self.audit.assert_called_once_with(self.native, self.inputs["prism-build"])
        self.assertEqual(self.snapshot_inputs(), before)
        self.assertFalse((output / "data").exists())
        self.assertEqual(json.loads((output / "game-data.json").read_text(encoding="utf-8")),
                         {"path": str(self.inputs["game-dir"])})
        self.assertEqual((output / "launch.py").read_bytes(),
                         (stage_game.ROOT / "src/platform/windows/launch.py").read_bytes())
        for target, source in self.windows_files.items():
            self.assertEqual((output / target).read_bytes(), source.read_bytes())
        self.assertEqual((output / "licenses/prism/LICENSES/nested/license.txt").read_bytes(),
                         (self.inputs["prism-source"] / "LICENSES/nested/license.txt").read_bytes())
        manifest = json.loads((output / "local-build.json").read_text(encoding="utf-8"))
        self.assertFalse(manifest["game_executed"])
        self.assertTrue(manifest["private_local_build"])
        self.assertEqual(manifest["game_executable_sha256"], hashlib.sha256(self.game_bytes).hexdigest())
        for relative, expected in manifest["files_sha256"].items():
            self.assertEqual(hashlib.sha256((output / relative).read_bytes()).hexdigest(), expected)
        self.assertIn("game-data.json", manifest["files_sha256"])

    def test_only_audited_runtime_dependencies_are_copied(self):
        dependency = self.native / "verified-dependency.dll"
        dependency.write_bytes(b"Original audited dependency fixture.")
        (self.native / "unrelated.dll").write_bytes(b"Original unrelated fixture.")
        self.audit.return_value["bin/verified-dependency.dll"] = dependency
        output = self.root / "installation"
        self.stage(output)
        self.assertEqual((output / "bin/verified-dependency.dll").read_bytes(), dependency.read_bytes())
        self.assertFalse((output / "bin/unrelated.dll").exists())

    def test_missing_reader_or_enabled_tts_fails_before_output_creation(self):
        original = self.cache.read_text(encoding="utf-8")
        variants = [original.replace("PRISM_ENABLE_NVDA_BACKEND:STRING=ON\n", ""),
                    original.replace("PRISM_ENABLE_SAPI_BACKEND:STRING=OFF", "PRISM_ENABLE_SAPI_BACKEND:STRING=ON"),
                    original.replace("PRISM_ENABLE_SAPI_BACKEND:STRING=OFF", "PRISM_ENABLE_SAPI_BACKEND:STRING=AUTO"),
                    original + "PRISM_ENABLE_UNKNOWN_BACKEND:BOOL=ON\n"]
        for cache in variants:
            with self.subTest(cache=cache):
                self.cache.write_text(cache, encoding="utf-8")
                output = self.root / "installation"
                with self.assertRaises(SystemExit):
                    self.stage(output)
                self.assertFalse(output.exists())
        self.audit.assert_not_called()

    def test_failed_runtime_audit_does_not_create_installation(self):
        self.audit.side_effect = ValueError("Synthetic unresolved runtime dependency")
        output = self.root / "installation"
        with self.assertRaises(SystemExit):
            self.stage(output)
        self.assertFalse(output.exists())


class WindowsAuditTests(StagingFixture):
    def setUp(self):
        super().setUp()
        self.native = self.inputs["native-build"] / "native"
        self.prism = self.inputs["prism-build"]
        for path in (self.native / "battle-brawlers.exe", self.native / "rexruntime.dll",
                     self.native / "rexgpu-xenos.dll", self.prism / "prism.dll"):
            path.write_bytes(b"Synthetic file, inspected through mocked tools only.")
        self.stack.enter_context(patch.object(stage_game.sys, "platform", "win32"))
        self.headers = self.stack.enter_context(patch.object(stage_game.subprocess, "check_output",
            return_value="Format: COFF-x86-64\nMachine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n"
                         "ImageOptionalHeader {\n  Magic: 0x20B\n}"))
        self.dependencies = []

        def report(command, **kwargs):
            output = next(argument.removeprefix("-DOUTPUT=") for argument in command
                          if argument.startswith("-DOUTPUT="))
            Path(output).write_text("\n".join(str(path) for path in self.dependencies), encoding="utf-8")

        self.inspect = self.stack.enter_context(patch.object(stage_game.subprocess, "run", side_effect=report))

    def test_native_and_prism_audits_can_run_independently(self):
        native = stage_game.audit_windows_runtime(native=self.native)
        self.assertEqual(set(native), {"bin/battle-brawlers.exe", "bin/rexruntime.dll", "bin/rexgpu-xenos.dll"})
        prism = stage_game.audit_windows_runtime(prism_build=self.prism)
        self.assertEqual(set(prism), {"lib/prism.dll"})
        self.assertEqual(self.inspect.call_count, 2)
        self.assertEqual(self.headers.call_count, 4)

    def test_dependency_outside_verified_output_is_rejected(self):
        foreign = self.root / "foreign.dll"
        foreign.write_bytes(b"Synthetic foreign dependency.")
        self.dependencies = [foreign]
        with self.assertRaisesRegex(ValueError, "outside"):
            stage_game.audit_windows_runtime(prism_build=self.prism)

    def test_wrong_architecture_or_object_without_pe_header_is_rejected(self):
        for header in ("Format: COFF-i386\nMachine: IMAGE_FILE_MACHINE_I386 (0x14C)\n",
                       "Format: COFF-x86-64\nMachine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n"):
            with self.subTest(header=header):
                self.headers.return_value = header
                with self.assertRaisesRegex(ValueError, "x64 Windows PE"):
                    stage_game.audit_windows_runtime(prism_build=self.prism)


if __name__ == "__main__":
    unittest.main()
