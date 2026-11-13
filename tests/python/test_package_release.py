"""Original packaging fixtures; no game files, native libraries or processes."""

import hashlib
import json
from pathlib import Path
import sys
import tarfile
import unittest
from unittest.mock import patch
import zipfile

from test_build_tools import StagingFixture, TOOLS, stage_game

with patch.object(sys, "path", [str(TOOLS), *sys.path]):
    import package_release


REVISION = "0123456789abcdef0123456789abcdef01234567"


class PackagingFixture(StagingFixture):
    def setUp(self):
        super().setUp()
        del self.inputs["game-dir"]
        sdk = self.inputs["rexglue-source"] / "thirdparty"
        for relative in ("licensed/LICENSE.txt", "licensed/nested/COPYING.LIB",
                         "licensed/LICENSES/extra/terms.txt", "licensed/license-checker.cfg",
                         "licensed/source.c", "headers-only/header.h", "catch2/LICENSE.txt"):
            path = sdk / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"Original synthetic notice fixture: " + relative.encode())
        (sdk / "not-initialized").mkdir()
        self.stack.enter_context(patch.object(package_release, "runpath", return_value="$ORIGIN"))
        self.revision = self.stack.enter_context(
            patch.object(package_release, "source_revision", return_value=REVISION))
        self.output = self.root / "release output"

    def package(self, version="0.1.0-alpha.1", **extra):
        arguments = ["package_release.py", "--platform", self.platform, "--version", version,
                     "--output", str(self.output)]
        for name, directory in self.inputs.items():
            arguments.extend(["--" + name, str(directory)])
        for name, value in extra.items():
            arguments.extend(["--" + name.replace("_", "-"), str(value)])
        with patch.object(sys, "argv", arguments):
            package_release.main()

    def refused(self, message, **extra):
        before = self.snapshot_inputs()
        with self.assertRaises(SystemExit) as raised:
            self.package(**extra)
        self.assertEqual(raised.exception.code, 1)
        self.assertIn(message, self.errors.getvalue())
        self.assertFalse(self.output.exists())
        self.assertEqual(self.snapshot_inputs(), before)


@unittest.skipUnless(sys.platform == "linux", "Packaging runs on a Linux host")
class LinuxPackageTests(PackagingFixture):
    name = "bb-recomp-0.1.0-alpha.1-linux-x86_64"

    def test_package_holds_program_guide_and_notices_but_no_game_files(self):
        before = self.snapshot_inputs()
        self.package()
        self.assertEqual(self.snapshot_inputs(), before)
        root = self.output / self.name
        for relative in ("launch.py", "launch_support.py", "game_files.py", "folder_dialog.py",
                         "messages.en.json", "bin/battle-brawlers", "bin/librexruntime.so",
                         "bin/librexgpu-xenos.so", "lib/libprism.so", "LICENSE", "NOTICE.md",
                         "licenses/prism/LICENSES/nested/license.txt",
                         "licenses/rexglue/thirdparty/licensed/LICENSE.txt",
                         "licenses/rexglue/thirdparty/licensed/nested/COPYING.LIB",
                         "licenses/rexglue/thirdparty/licensed/LICENSES/extra/terms.txt"):
            self.assertTrue((root / relative).is_file(), relative)
        self.assertEqual((root / "README.md").read_bytes(),
                         (stage_game.ROOT / "docs/playing.md").read_bytes())
        for relative in ("data", "user-data", "game-data.json", "local-build.json", "Play.bat", "python",
                         "licenses/rexglue/thirdparty/catch2", "licenses/rexglue/thirdparty/not-initialized",
                         "licenses/rexglue/thirdparty/licensed/license-checker.cfg",
                         "licenses/rexglue/thirdparty/licensed/source.c"):
            self.assertFalse((root / relative).exists(), relative)
        listed = (root / "licenses/rexglue/thirdparty/README.txt").read_text(encoding="utf-8")
        self.assertIn("- headers-only\n", listed)
        self.assertNotIn("- licensed\n", listed)

    def test_manifest_identifies_source_and_every_file(self):
        self.package()
        root = self.output / self.name
        manifest = json.loads((root / "release.json").read_text(encoding="utf-8"))
        self.assertEqual((manifest["version"], manifest["platform"], manifest["source_revision"]),
                         ("0.1.0-alpha.1", "linux-x86_64", REVISION))
        self.assertIs(manifest["contains_game_files"], False)
        self.assertEqual(manifest["required_game_executable_sha256"], package_release.EXECUTABLE_SHA256)
        packaged = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
        self.assertEqual(set(manifest["files_sha256"]), packaged - {"release.json"})
        for relative, expected in manifest["files_sha256"].items():
            self.assertEqual(hashlib.sha256((root / relative).read_bytes()).hexdigest(), expected)

    def test_archive_has_one_root_fixed_modes_and_a_matching_checksum(self):
        self.package()
        archive = self.output / (self.name + ".tar.gz")
        with tarfile.open(archive) as contents:
            members = {member.name: member for member in contents.getmembers()}
        self.assertTrue(all(name == self.name or name.startswith(self.name + "/") for name in members))
        for relative, mode in (("launch.py", 0o755), ("bin/battle-brawlers", 0o755),
                               ("bin/librexruntime.so", 0o644), ("README.md", 0o644), ("bin", 0o755)):
            self.assertEqual(members[f"{self.name}/{relative}"].mode, mode, relative)
        self.assertTrue(all(member.uid == 0 and member.uname == "" and not member.issym()
                            for member in members.values()))
        checksum = (self.output / (archive.name + ".sha256")).read_text(encoding="ascii")
        self.assertEqual(checksum, f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n")

    def test_uncommitted_source_or_invalid_version_is_refused(self):
        self.revision.side_effect = ValueError("Commit or discard source changes")
        self.refused("Commit or discard")
        self.revision.side_effect = None
        for version in ("alpha", "1.0", "0.1.0-", "0.1.0 alpha", "../0.1.0"):
            with self.subTest(version=version), self.assertRaises(SystemExit):
                self.package(version=version)
            self.assertFalse(self.output.exists())

    def test_output_inside_an_input_or_already_present_is_refused(self):
        self.output = self.inputs["prism-build"] / "release"
        self.refused("must be separate")
        self.output = self.root / "existing"
        self.output.mkdir()
        with self.assertRaises(SystemExit):
            self.package()
        self.assertEqual(list(self.output.iterdir()), [])

    def test_private_build_location_inside_a_binary_removes_the_package(self):
        runtime = self.inputs["native-build"] / "native/librexruntime.so"
        runtime.write_bytes(b"Synthetic runtime with " + str(self.inputs["native-build"]).encode() + b"/x.cpp")
        self.refused("private build location")

    def test_absolute_runtime_search_path_or_wrong_speech_backends_are_refused(self):
        with patch.object(package_release, "runpath", return_value="/synthetic/build"):
            self.refused("relative runtime search path")
        (self.inputs["prism-build"] / "CMakeCache.txt").write_text(
            "PRISM_ENABLE_ORCA_BACKEND:BOOL=ON\nPRISM_ENABLE_SPEECH_DISPATCHER_BACKEND:BOOL=ON\n")
        self.refused("screen-reader backends")

    def test_python_archive_is_refused_for_linux(self):
        archive = self.root / "python.zip"
        archive.write_bytes(b"unused")
        self.refused("only it", windows_python=archive)


@unittest.skipUnless(sys.platform == "linux", "Packaging runs on a Linux host")
class WindowsPackageTests(PackagingFixture):
    platform = "windows"
    name = "bb-recomp-0.1.0-alpha.1-windows-x64"

    def setUp(self):
        super().setUp()
        native = self.inputs["native-build"] / "native"
        for path in (native / "battle-brawlers.exe", native / "rexruntime.dll",
                     native / "rexgpu-xenos.dll", self.inputs["prism-build"] / "prism.dll"):
            path.write_bytes(b"Original synthetic Windows binary placeholder; never loaded.")
        (self.inputs["prism-build"] / "CMakeCache.txt").write_text(
            "".join(f"PRISM_ENABLE_{name}_BACKEND:STRING=ON\n" for name in sorted(stage_game.WINDOWS_BACKENDS))
            + "PRISM_ENABLE_SAPI_BACKEND:STRING=OFF\n", encoding="utf-8")
        self.python = self.root / "python-embed.zip"
        with zipfile.ZipFile(self.python, "w") as archive:
            archive.writestr("python.exe", "Original synthetic interpreter placeholder; never run.")
            archive.writestr("python399._pth", "python399.zip\n.\n")
            archive.writestr("LICENSE.txt", "Original synthetic interpreter license.")
        self.imports = {"battle-brawlers.exe": {"kernel32.dll", "rexruntime.dll", "msvcp140.dll",
                                                "api-ms-win-crt-heap-l1-1-0.dll"},
                        "rexruntime.dll": {"user32.dll", "vcruntime140_1.dll"},
                        "rexgpu-xenos.dll": {"rexruntime.dll"},
                        "prism.dll": {"rpcrt4.dll", "api-ms-win-core-synch-l1-2-0.dll"}}
        self.stack.enter_context(patch.object(
            package_release, "windows_imports", side_effect=lambda path: self.imports[path.name]))
        self.headers = self.stack.enter_context(patch.object(package_release, "check_windows_x64"))

    def test_package_bundles_interpreter_start_script_and_no_game_files(self):
        self.package(windows_python=self.python)
        root = self.output / self.name
        self.assertEqual(self.headers.call_count, 4)
        self.assertEqual((root / "Play.bat").read_bytes(), package_release.PLAY_SCRIPT.encode("ascii"))
        self.assertIn(b'"%~dp0python\\python.exe" "%~dp0launch.py" %*\r\n', (root / "Play.bat").read_bytes())
        # The launcher's own directory must be importable by the embedded runtime.
        self.assertEqual((root / "python/python399._pth").read_text(), "python399.zip\n.\n..\n")
        self.assertTrue((root / "python/LICENSE.txt").is_file())
        self.assertEqual((root / "launch.py").read_bytes(),
                         (stage_game.ROOT / "src/platform/windows/launch.py").read_bytes())
        for relative in ("data", "user-data", "game-data.json"):
            self.assertFalse((root / relative).exists(), relative)
        manifest = json.loads((root / "release.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["python_archive_sha256"], hashlib.sha256(self.python.read_bytes()).hexdigest())
        with zipfile.ZipFile(self.output / (self.name + ".zip")) as archive:
            names = archive.namelist()
        self.assertTrue(all(name.startswith(self.name + "/") for name in names))
        self.assertIn(self.name + "/bin/battle-brawlers.exe", names)

    def test_unreviewed_or_misplaced_import_is_refused_before_output(self):
        for binary, imported in (("rexruntime.dll", "unknown.dll"),
                                 ("battle-brawlers.exe", "api-ms-win-core-synthetic-l1-1-0.dll"),
                                 # A library packaged in another directory is not on the search path.
                                 ("prism.dll", "rexruntime.dll")):
            with self.subTest(imported=imported):
                original = set(self.imports[binary])
                self.imports[binary].add(imported)
                self.refused("unreviewed library: " + imported, windows_python=self.python)
                self.imports[binary] = original

    def test_wrong_architecture_or_missing_interpreter_is_refused(self):
        self.headers.side_effect = ValueError("Expected an x64 Windows PE binary: synthetic")
        self.refused("x64 Windows PE", windows_python=self.python)
        self.headers.side_effect = None
        self.refused("needs the Python embeddable archive")
        with zipfile.ZipFile(self.python, "w") as archive:
            archive.writestr("readme.txt", "Not an interpreter.")
        self.refused("not a Python embeddable package", windows_python=self.python)


if __name__ == "__main__":
    unittest.main()
