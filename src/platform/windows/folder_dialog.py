# SPDX-License-Identifier: BSD-3-Clause
"""Ask for a directory with the Windows shell's folder browser."""

import ctypes
import threading

from game_files import DialogUnavailable


# Microsoft shlobj_core.h: BROWSEINFOW flags and SHGetPathFromIDListEx.
BIF_RETURNONLYFSDIRS = 0x0001
BIF_EDITBOX = 0x0010
BIF_NEWDIALOGSTYLE = 0x0040
BIF_NONEWFOLDERBUTTON = 0x0200
PATH_CAPACITY = 32768


def browse(title):
    """Show the dialog on the calling thread; None means cancelled."""
    from ctypes import wintypes

    class BrowseInfo(ctypes.Structure):
        _fields_ = [("hwndOwner", wintypes.HWND), ("pidlRoot", ctypes.c_void_p),
                    ("pszDisplayName", wintypes.LPWSTR), ("lpszTitle", wintypes.LPCWSTR),
                    ("ulFlags", wintypes.UINT), ("lpfn", ctypes.c_void_p),
                    ("lParam", wintypes.LPARAM), ("iImage", ctypes.c_int)]

    shell = ctypes.WinDLL("shell32", use_last_error=True)
    ole = ctypes.WinDLL("ole32", use_last_error=True)
    shell.SHBrowseForFolderW.argtypes = [ctypes.POINTER(BrowseInfo)]
    shell.SHBrowseForFolderW.restype = ctypes.c_void_p
    shell.SHGetPathFromIDListEx.argtypes = [ctypes.c_void_p, wintypes.LPWSTR,
                                            wintypes.DWORD, ctypes.c_int]
    shell.SHGetPathFromIDListEx.restype = wintypes.BOOL
    ole.OleInitialize.argtypes = [ctypes.c_void_p]
    ole.OleInitialize.restype = ctypes.c_long
    ole.OleUninitialize.argtypes = []
    ole.OleUninitialize.restype = None
    ole.CoTaskMemFree.argtypes = [ctypes.c_void_p]
    ole.CoTaskMemFree.restype = None

    # The new dialog style requires a single-threaded apartment on this thread.
    if ole.OleInitialize(None) < 0:
        raise DialogUnavailable("OLE initialization failed")
    try:
        display_name = ctypes.create_unicode_buffer(260)
        info = BrowseInfo(None, None, ctypes.cast(display_name, wintypes.LPWSTR), title,
                          BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE
                          | BIF_NONEWFOLDERBUTTON, None, 0, 0)
        item = shell.SHBrowseForFolderW(ctypes.byref(info))
        if not item:
            return None
        try:
            path = ctypes.create_unicode_buffer(PATH_CAPACITY)
            if not shell.SHGetPathFromIDListEx(item, path, PATH_CAPACITY, 0):
                # A chosen item without a file-system path, such as a network
                # server, is a refused choice and not a cancellation.
                return display_name.value or "?"
            return path.value or None
        finally:
            ole.CoTaskMemFree(item)
    finally:
        ole.OleUninitialize()


def choose_directory(title):
    """Return the chosen path, None when cancelled, or raise DialogUnavailable."""
    outcome = {}

    def run():
        try:
            outcome["path"] = browse(title)
        except Exception as error:  # Any dialog failure must reach the console prompt.
            outcome["error"] = error

    # A dedicated thread owns the apartment, whatever COM state speech libraries
    # established on the launcher's main thread.
    thread = threading.Thread(target=run, name="game-directory-dialog")
    thread.start()
    thread.join()
    if "error" in outcome:
        raise DialogUnavailable(str(outcome["error"]))
    return outcome.get("path")
