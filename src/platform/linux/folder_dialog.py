# SPDX-License-Identifier: BSD-3-Clause
"""Ask for a directory through the desktop's own dialog program."""

import os
import shutil
import subprocess

from game_files import DialogUnavailable


def commands(title, start, desktop):
    """Return candidate dialog commands, preferring the running desktop's toolkit."""
    zenity = ["zenity", "--file-selection", "--directory", "--title=" + title,
              "--filename=" + start + "/"]
    kdialog = ["kdialog", "--title", title, "--getexistingdirectory", start]
    return [kdialog, zenity] if "KDE" in desktop.upper().split(":") else [zenity, kdialog]


def choose_directory(title):
    """Return the chosen path, None when cancelled, or raise DialogUnavailable."""
    if not (os.environ.get("WAYLAND_DISPLAY") or os.environ.get("DISPLAY")):
        raise DialogUnavailable("no graphical session")
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "")
    for command in commands(title, os.path.expanduser("~"), desktop):
        if shutil.which(command[0]) is None:
            continue
        try:
            result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL, text=True, check=False)
        except OSError:
            continue
        # Both programs print the directory and exit 0; 1 means cancelled.
        if result.returncode == 0:
            return result.stdout.rstrip("\n") or None
        if result.returncode == 1:
            return None
    raise DialogUnavailable("no working directory dialog program")
