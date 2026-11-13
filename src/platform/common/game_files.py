# SPDX-License-Identifier: BSD-3-Clause
"""Locate, verify and remember the player's own game directory.

No game file is copied, changed or distributed. The installation records only
the directory's location after its executable matches the supported identity.
"""

import hashlib
import json
from pathlib import Path
import sys


CONFIG = "game-data.json"
# Identity of the only executable the compiled translation corresponds to.
EXECUTABLE_SIZE = 11055104
EXECUTABLE_SHA256 = "7f47ff62b744239fb3c1b98bb9ca26ab38b3d33cd148bba6d14c03739f7ab29b"


class DialogUnavailable(Exception):
    """The platform cannot present a directory dialog in this session."""


def saved_directory(root):
    """Return the remembered game directory, or None unless it still verifies."""
    try:
        config = json.loads((root / CONFIG).read_text(encoding="utf-8"))
        if not isinstance(config, dict) or not isinstance(config.get("path"), str):
            return None
        path = Path(config["path"])
    except (OSError, ValueError):
        return None
    # The files may have changed since the directory was chosen.
    return path if inspect(path) == "ok" else None


def inspect(path):
    """Classify a candidate directory as "ok", "missing" or "unsupported"."""
    try:
        executable = path / "default.xex"
        if not path.is_absolute() or not executable.is_file() or not (path / "media").is_dir():
            return "missing"
        if executable.stat().st_size != EXECUTABLE_SIZE:
            return "unsupported"
        digest = hashlib.sha256()
        with executable.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except (OSError, ValueError):
        return "missing"
    return "ok" if digest.hexdigest() == EXECUTABLE_SHA256 else "unsupported"


def remember(root, path):
    """Record a verified directory; the write replaces the file atomically."""
    temporary = root / (CONFIG + ".new")
    temporary.write_text(json.dumps({"path": str(path)}) + "\n", encoding="utf-8")
    temporary.replace(root / CONFIG)


def accept(root, reporter, candidate):
    """Verify and remember one candidate; report why it was refused."""
    try:
        path = Path(candidate).expanduser().resolve()
    except (OSError, ValueError, RuntimeError):
        reporter.report("game_files_missing")
        return None
    result = inspect(path)
    if result != "ok":
        reporter.report("game_files_" + result)
        return None
    remember(root, path)
    reporter.report("game_files_verified")
    return path


def typed_directory(reporter):
    """Read a path from an interactive console; None means cancelled."""
    if sys.stdin is None or not sys.stdin.isatty():
        raise DialogUnavailable("standard input is not an interactive console")
    reporter.report("game_files_prompt")
    try:
        text = input().strip()
    except EOFError:
        return None
    # File managers and consoles commonly quote a pasted path.
    if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
        text = text[1:-1]
    return text or None


def request(root, reporter, choose_directory):
    """Ask until a supported directory is chosen or the player cancels.

    `choose_directory(title)` returns a path, returns None when cancelled, or
    raises DialogUnavailable. A console prompt replaces an unavailable dialog.
    """
    reporter.report("game_files_needed")
    title = reporter.messages["game_files_dialog_title"]
    use_dialog = True
    while True:
        try:
            if use_dialog:
                candidate = choose_directory(title)
            else:
                candidate = typed_directory(reporter)
        except DialogUnavailable:
            if use_dialog:
                use_dialog = False
                continue
            reporter.report("game_files_dialog_unavailable")
            return None
        if candidate is None:
            reporter.report("game_files_cancelled")
            return None
        path = accept(root, reporter, candidate)
        if path is not None:
            return path
