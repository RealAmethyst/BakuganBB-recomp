"""Prism speech through running screen readers, without a standalone TTS fallback."""

import ctypes
from pathlib import Path


# Stable identifiers from Prism 0.18.3 include/prism.h. Unknown backends fail closed.
SCREEN_READERS = frozenset({
    0xCB4897961A754BCB,  # VoiceOver
    0x89CC19C5C4AC1A56,  # NVDA
    0xAC3D60E9BD84B53E,  # JAWS
    0x10AA1FC05A17F96C,  # Orca
    0xD199C175AEEC494B,  # Android screen reader
    0x3D93C56C9E7F2A2E,  # ZDSR
    0xAE439D62DC7B1479,  # ZoomText
    0x285ABA1C16F3300F,  # Boy PC Reader
    0x344B951962E3B835,  # PC-Talker
    0xED4760890B55C2F2,  # Sense Reader
    0x8380F2A37B2C3EB6,  # System Access
    0x9120D89908785C13,  # Window-Eyes
})
REQUIRED_FEATURES = (1 << 0) | (1 << 2)  # Available now and supports speech.


class PrismLoadError(RuntimeError):
    """The explicit library could not provide a usable Prism context."""


class PrismSpeech:
    """Own one Prism context. Call from the launcher thread; close after use."""

    def __init__(self, library_path):
        self._context = None
        self._backend = None
        self._backend_id = None
        self.last_result = None
        try:
            path = Path(library_path).resolve(strict=True)
            self._lib = ctypes.CDLL(str(path))
            signatures = {
                "prism_init": ([ctypes.c_void_p], ctypes.c_void_p),
                "prism_shutdown": ([ctypes.c_void_p], None),
                "prism_registry_count": ([ctypes.c_void_p], ctypes.c_size_t),
                "prism_registry_id_at": ([ctypes.c_void_p, ctypes.c_size_t], ctypes.c_uint64),
                "prism_registry_create": ([ctypes.c_void_p, ctypes.c_uint64], ctypes.c_void_p),
                "prism_backend_get_features": ([ctypes.c_void_p], ctypes.c_uint64),
                "prism_backend_initialize": ([ctypes.c_void_p], ctypes.c_int),
                "prism_backend_speak": ([ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool], ctypes.c_int),
                "prism_backend_free": ([ctypes.c_void_p], None),
            }
            for name, (arguments, result) in signatures.items():
                function = getattr(self._lib, name)
                function.argtypes = arguments
                function.restype = result
        except (OSError, AttributeError) as error:
            raise PrismLoadError(str(error)) from error

        # A null config starts no availability monitor and initializes no backend.
        # Never use create_best/acquire_best: those may initialize standalone TTS.
        self._context = self._lib.prism_init(None)
        if not self._context:
            raise PrismLoadError("prism_init returned no context")

    def _select_backend(self):
        # Prism's frozen registry is sorted by descending backend priority.
        for index in range(self._lib.prism_registry_count(self._context)):
            identity = self._lib.prism_registry_id_at(self._context, index)
            if identity not in SCREEN_READERS:
                continue
            reused = identity == self._backend_id
            candidate = self._backend if reused else self._lib.prism_registry_create(
                self._context, identity)
            if not candidate:
                continue
            features = self._lib.prism_backend_get_features(candidate)
            if features & REQUIRED_FEATURES == REQUIRED_FEATURES:
                if reused or self._lib.prism_backend_initialize(candidate) == 0:
                    if not reused:
                        self._free_backend()
                        self._backend = candidate
                        self._backend_id = identity
                    return True
            if reused:
                self._free_backend()
            else:
                self._lib.prism_backend_free(candidate)
        self._free_backend()
        return False

    def speak(self, text, interrupt=False):
        """Return whether Prism accepted speech; silence when no reader is running."""
        self.last_result = None
        if not self._context or not text or "\0" in text:
            return False
        encoded = text.encode("utf-8")
        if not self._select_backend():
            return False
        self.last_result = self._lib.prism_backend_speak(self._backend, encoded, interrupt)
        if self.last_result != 0:
            self._free_backend()
            return False
        return True

    def _free_backend(self):
        if self._backend:
            self._lib.prism_backend_free(self._backend)
            self._backend = None
            self._backend_id = None

    def close(self):
        """Release this client's handles without stopping the screen reader."""
        self._free_backend()
        if self._context:
            self._lib.prism_shutdown(self._context)
            self._context = None
