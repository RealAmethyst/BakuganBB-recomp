#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Assemble a distributable test package from built code; never run it.

The package holds the compiled program, launcher, notices and player guide.
It never contains game files: the launcher asks each player for their own.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

from generate_game import EXECUTABLE_SHA256, digest
from stage_game import (check_prism_backends, check_windows_x64, external, license_files,
                        normalize_runpaths, runpath, runtime_files)
from stage_launcher import ROOT, stage as stage_launcher


VERSION = re.compile(r"\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?")
LICENSE_NAME = re.compile(r"(?:licen[sc]e|copying|notice|unlicense)(?:[.-][\w.-]*)?", re.IGNORECASE)
# Vendored SDK components that are test-only, disabled or for other platforms.
UNUSED_COMPONENTS = {"catch2", "tracy", "moltenvk", "vulkan-loader", "dxc"}
# Windows components the import tables may name. A package cannot be resolved
# against a real system directory on another host, so every other import must
# be a packaged file, a C runtime API set or the documented compiler runtime.
WINDOWS_SYSTEM = {
    "advapi32.dll", "bcrypt.dll", "dxgi.dll", "gdi32.dll", "hid.dll", "imm32.dll",
    "kernel32.dll", "ole32.dll", "oleaut32.dll", "rpcrt4.dll", "setupapi.dll",
    "shell32.dll", "user32.dll", "version.dll", "winmm.dll", "ws2_32.dll",
    # WaitOnAddress contract, present since Windows 8; the Windows workflow
    # confirms it with the OS availability query when it audits Prism.
    "api-ms-win-core-synch-l1-2-0.dll",
}
VC_RUNTIME = {"msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"}
C_RUNTIME_API_SET = re.compile(r"api-ms-win-crt-[a-z0-9]+-l1-1-0\.dll")
PLAY_SCRIPT = ('@echo off\r\n'
               'rem Start the launcher with the bundled Python runtime.\r\n'
               '"%~dp0python\\python.exe" "%~dp0launch.py" %*\r\n'
               'if errorlevel 1 pause\r\n')
EXECUTABLES = {"launch.py", "bin/battle-brawlers"}
PLATFORM_LABEL = {"linux": "linux-x86_64", "windows": "windows-x64"}


def windows_imports(path):
    """Read a PE import table without loading the image."""
    output = subprocess.check_output(["llvm-readobj", "--coff-imports", str(path)],
                                     text=True, encoding="utf-8", errors="replace")
    return {name.lower() for name in re.findall(r"^\s*Name: (\S+)$", output, re.MULTILINE)}


def audit_windows_package(files):
    """Check architecture and that each import has a known provider."""
    for name, source in files.items():
        if not name.endswith((".exe", ".dll")):
            continue
        check_windows_x64(source)
        directory = name.rsplit("/", 1)[0]
        local = {other.rsplit("/", 1)[1].lower() for other in files if other.startswith(directory + "/")}
        for imported in sorted(windows_imports(source)):
            if not (imported in local or imported in WINDOWS_SYSTEM or imported in VC_RUNTIME
                    or C_RUNTIME_API_SET.fullmatch(imported)):
                raise ValueError(f"{name} imports an unreviewed library: {imported}")


def third_party_notices(sdk):
    """Return SDK component notices and the components that have none to copy."""
    notices, without = {}, []
    for component in sorted(path for path in (sdk / "thirdparty").iterdir() if path.is_dir()):
        if component.name in UNUSED_COMPONENTS or not any(component.iterdir()):
            continue
        found = False
        for path in sorted(component.rglob("*")):
            relative = path.relative_to(component)
            if len(relative.parts) > 3 or not path.is_file() or path.suffix == ".cfg":
                continue
            if LICENSE_NAME.fullmatch(path.name) or "LICENSES" in relative.parts[:-1]:
                notices[f"licenses/rexglue/thirdparty/{component.name}/{relative.as_posix()}"] = path
                found = True
        if not found:
            without.append(component.name)
    return notices, without


def source_revision():
    """Identify the packaged source; an uncommitted tree cannot be released."""
    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(ROOT), *arguments], text=True).strip()
    if git("status", "--porcelain"):
        raise ValueError("Commit or discard source changes before packaging a release")
    return git("rev-parse", "HEAD")


def forbidden_fragments(*paths):
    """Private locations that must not appear inside any packaged file."""
    candidates = {Path.home(), *paths}
    # A single top-level name such as a container mount point identifies nobody
    # and also occurs in ordinary relative paths.
    return [str(path).encode("utf-8") for path in candidates if len(path.parts) >= 3]


def audit_content(package, fragments):
    """Reject game inputs, mutable state and private build locations."""
    for path in sorted(package.rglob("*")):
        relative = path.relative_to(package).as_posix()
        if path.is_symlink():
            raise ValueError(f"Packages must not contain links: {relative}")
        if relative.split("/")[0] in {"data", "user-data"} or path.name in {"game-data.json", "default.xex"}:
            raise ValueError(f"Packages must not contain game files or player state: {relative}")
        if path.is_file():
            content = path.read_bytes()
            for fragment in fragments:
                if fragment in content:
                    raise ValueError(f"{relative} contains a private build location")


def write_archive(package, platform):
    """Write a tar.gz (Linux) or zip (Windows) archive beside the directory."""
    members = sorted(path for path in package.rglob("*"))
    if platform == "windows":
        archive = package.with_name(package.name + ".zip")
        with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as output:
            for path in members:
                if path.is_file():
                    output.write(path, f"{package.name}/{path.relative_to(package).as_posix()}")
        return archive
    archive = package.with_name(package.name + ".tar.gz")

    def normalize(info):
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        return info

    with tarfile.open(archive, "x:gz") as output:
        output.add(package, arcname=package.name, recursive=False, filter=normalize)
        for path in members:
            output.add(path, arcname=f"{package.name}/{path.relative_to(package).as_posix()}",
                       recursive=False, filter=normalize)
    return archive


def package(platform, native_build, prism_build, prism_source, rexglue_source, version, output,
            windows_python=None):
    if platform not in PLATFORM_LABEL:
        raise ValueError("Unsupported package platform")
    if not VERSION.fullmatch(version):
        raise ValueError("Use a version such as 0.1.0-alpha.1")
    if (platform == "windows") != (windows_python is not None):
        raise ValueError("The Windows package, and only it, needs the Python embeddable archive")
    native = external(native_build) / "native"
    prism_build = external(prism_build)
    prism_source = external(prism_source)
    sdk = external(rexglue_source)
    inputs = [native.parent, prism_build, prism_source, sdk]
    if windows_python is not None:
        windows_python = external(windows_python)
    original_output = output
    output = output.resolve()
    if output == ROOT or ROOT in output.parents or os.path.lexists(original_output) or output.exists():
        raise ValueError("Choose a new output directory outside the repository")
    if any(parent == output or parent in output.parents for parent in inputs):
        raise ValueError("The output must be separate from build and source directories")
    revision = source_revision()

    files = runtime_files(platform, native, prism_build)
    binaries = dict(files)
    files.update(license_files(sdk, prism_source))
    notices, without_notice = third_party_notices(sdk)
    files.update(notices)
    files["README.md"] = ROOT / "docs/playing.md"
    for source in files.values():
        if not source.is_file():
            raise ValueError(f"Required input is missing: {source}")
    if not (prism_source / "LICENSES").is_dir():
        raise ValueError("Prism's complete LICENSES directory is required")
    check_prism_backends((prism_build / "CMakeCache.txt").read_text(encoding="utf-8"), platform)
    paths = {}
    if platform == "windows":
        audit_windows_package(binaries)
    else:
        if not os.access(native / "battle-brawlers", os.X_OK):
            raise ValueError("The built native host must be executable")
        if runpath(files["bin/battle-brawlers"]) != "$ORIGIN":
            raise ValueError("The native host must use the verified relative runtime search path")
        paths = {name: runpath(files[name]) for name in ("bin/librexruntime.so", "bin/librexgpu-xenos.so")}
    sdk_manifest = json.loads((ROOT / "patches/rexglue/manifest.json").read_text(encoding="utf-8"))
    prism_manifest = json.loads((ROOT / "patches/prism/manifest.json").read_text(encoding="utf-8"))

    # All preflight checks finish before creating the package.
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    try:
        return assemble(output, platform, version, revision, files, paths, prism_source,
                        sdk_manifest["revision"], prism_manifest["revision"], without_notice,
                        windows_python, forbidden_fragments(ROOT, *inputs, output))
    except BaseException:
        # A refused package must not be mistaken for a finished one.
        shutil.rmtree(output)
        raise


def assemble(output, platform, version, revision, files, paths, prism_source, sdk_revision,
             prism_revision, without_notice, windows_python, fragments):
    root = output / f"bb-recomp-{version}-{PLATFORM_LABEL[platform]}"
    stage_launcher(root, platform=platform)
    for name, source in files.items():
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    shutil.copytree(prism_source / "LICENSES", root / "licenses/prism/LICENSES",
                    copy_function=shutil.copyfile)
    (root / "licenses/rexglue/thirdparty").mkdir(parents=True, exist_ok=True)
    (root / "licenses/rexglue/thirdparty/README.txt").write_text(
        "License files copied from the vendored components of the pinned ReXGlue SDK source:\n"
        f"https://github.com/rexglue/rexglue-sdk/tree/{sdk_revision}/thirdparty\n\n"
        "These components state their terms in their source file headers at that location\n"
        "and have no separate license file to copy:\n"
        + "".join(f"- {name}\n" for name in without_notice), encoding="utf-8")
    normalize_runpaths(root, paths)
    record = {
        "name": "BB Recomp", "version": version, "platform": PLATFORM_LABEL[platform],
        "source_revision": revision,
        "rexglue_revision": sdk_revision, "prism_revision": prism_revision,
        "required_game_executable_sha256": EXECUTABLE_SHA256,
        "contains_game_files": False,
    }
    if platform == "windows":
        with zipfile.ZipFile(windows_python) as archive:
            archive.extractall(root / "python")
        runtime_paths = list((root / "python").glob("python*._pth"))
        if len(runtime_paths) != 1 or not (root / "python/python.exe").is_file():
            raise ValueError("The archive is not a Python embeddable package")
        # The embedded runtime ignores the script directory; name it explicitly.
        runtime_paths[0].write_text(runtime_paths[0].stem + ".zip\n.\n..\n", encoding="ascii")
        (root / "Play.bat").write_bytes(PLAY_SCRIPT.encode("ascii"))
        record["python_archive_sha256"] = digest(windows_python)
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        path.chmod(0o755 if path.is_dir() or relative in EXECUTABLES else 0o644)
    audit_content(root, fragments)
    record["files_sha256"] = {path.relative_to(root).as_posix(): digest(path)
                              for path in sorted(root.rglob("*")) if path.is_file()}
    (root / "release.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    (root / "release.json").chmod(0o644)
    archive = write_archive(root, platform)
    checksum = archive.with_name(archive.name + ".sha256")
    checksum.write_text(f"{digest(archive)}  {archive.name}\n", encoding="ascii")
    print(f"Packaged {archive}")
    print("Nothing was executed. The package contains compiled game code and no game files.")
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=sorted(PLATFORM_LABEL))
    parser.add_argument("--version", required=True, help="Release version, for example 0.1.0-alpha.1")
    for name in ("native-build", "prism-build", "prism-source", "rexglue-source", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--windows-python", type=Path,
                        help="Official python-<version>-embed-amd64.zip (Windows packages only)")
    args = parser.parse_args()
    try:
        package(args.platform, args.native_build, args.prism_build, args.prism_source,
                args.rexglue_source, args.version, args.output, windows_python=args.windows_python)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Packaging failed: {error}\n")


if __name__ == "__main__":
    main()
