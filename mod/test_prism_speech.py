"""Offline adapter contract tests; every native call is replaced by a fake."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from prism_speech import PrismLoadError, PrismSpeech, REQUIRED_FEATURES


ORCA = 0x10AA1FC05A17F96C
NVDA = 0x89CC19C5C4AC1A56
SAPI = 0x1D6DF72422CEEE66
SPEECH_DISPATCHER = 0xE3D6F895D949EBFE
UNKNOWN = 123


class FakePrism:
    def __init__(self, identities, available=()):
        self.available = set(available)
        self.prism_init = Mock(return_value=100)
        self.prism_shutdown = Mock()
        self.prism_registry_count = Mock(return_value=len(identities))
        self.prism_registry_id_at = Mock(side_effect=lambda context, index: identities[index])
        self.prism_registry_create = Mock(side_effect=lambda context, identity: identity)
        self.prism_backend_get_features = Mock(
            side_effect=lambda backend: REQUIRED_FEATURES if backend in self.available else 4)
        self.prism_backend_initialize = Mock(return_value=0)
        self.prism_backend_speak = Mock(return_value=0)
        self.prism_backend_free = Mock()


class PrismSpeechTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.library = Path(self.directory.name) / "libprism.so"
        self.library.touch()

    def adapter(self, fake):
        loader = patch("prism_speech.ctypes.CDLL", return_value=fake)
        self.addCleanup(loader.stop)
        loader.start()
        adapter = PrismSpeech(self.library)
        self.addCleanup(adapter.close)
        fake.prism_init.assert_called_once_with(None)
        return adapter

    def test_no_reader_never_initializes_standalone_or_unknown_backend(self):
        fake = FakePrism([SAPI, SPEECH_DISPATCHER, UNKNOWN, ORCA], [SAPI, SPEECH_DISPATCHER])
        adapter = self.adapter(fake)
        self.assertFalse(adapter.speak("Diagnostic"))
        fake.prism_registry_create.assert_called_once_with(100, ORCA)
        fake.prism_backend_initialize.assert_not_called()
        fake.prism_backend_speak.assert_not_called()
        fake.prism_backend_free.assert_called_once_with(ORCA)

    def test_priority_selection_unicode_and_queue_flag(self):
        fake = FakePrism([SAPI, NVDA, ORCA], [SAPI, NVDA, ORCA])
        adapter = self.adapter(fake)
        self.assertTrue(adapter.speak("Réussite"))
        fake.prism_backend_speak.assert_called_once_with(NVDA, "Réussite".encode(), False)
        self.assertTrue(adapter.speak("Next", interrupt=True))
        fake.prism_backend_speak.assert_called_with(NVDA, b"Next", True)
        fake.prism_backend_initialize.assert_called_once_with(NVDA)

    def test_failed_reader_initialization_uses_only_next_allowed_reader(self):
        fake = FakePrism([NVDA, SAPI, ORCA], [NVDA, SAPI, ORCA])
        fake.prism_backend_initialize.side_effect = lambda backend: 16 if backend == NVDA else 0
        adapter = self.adapter(fake)
        self.assertTrue(adapter.speak("Diagnostic"))
        self.assertEqual([call.args[1] for call in fake.prism_registry_create.call_args_list],
                         [NVDA, ORCA])
        fake.prism_backend_free.assert_called_once_with(NVDA)

    def test_reader_disappears_without_tts_fallback(self):
        fake = FakePrism([ORCA, SPEECH_DISPATCHER], [ORCA, SPEECH_DISPATCHER])
        adapter = self.adapter(fake)
        self.assertTrue(adapter.speak("One"))
        fake.available.remove(ORCA)
        self.assertFalse(adapter.speak("Two"))
        self.assertEqual(fake.prism_backend_speak.call_count, 1)
        fake.prism_backend_free.assert_called_once_with(ORCA)

    def test_new_higher_priority_reader_is_selected(self):
        fake = FakePrism([NVDA, ORCA], [ORCA])
        adapter = self.adapter(fake)
        self.assertTrue(adapter.speak("One"))
        fake.available.add(NVDA)
        self.assertTrue(adapter.speak("Two"))
        fake.prism_backend_speak.assert_called_with(NVDA, b"Two", False)

    def test_speech_failure_is_not_retried_with_tts(self):
        fake = FakePrism([ORCA, SAPI], [ORCA, SAPI])
        fake.prism_backend_speak.return_value = 6
        adapter = self.adapter(fake)
        self.assertFalse(adapter.speak("Diagnostic"))
        self.assertEqual(adapter.last_result, 6)
        fake.prism_backend_free.assert_called_once_with(ORCA)
        self.assertEqual(fake.prism_registry_create.call_count, 1)

    def test_close_is_idempotent_and_does_not_stop_reader(self):
        fake = FakePrism([ORCA], [ORCA])
        adapter = self.adapter(fake)
        self.assertTrue(adapter.speak("Diagnostic"))
        adapter.close()
        adapter.close()
        self.assertFalse(adapter.speak("After close"))
        fake.prism_backend_free.assert_called_once_with(ORCA)
        fake.prism_shutdown.assert_called_once_with(100)

    def test_missing_library_is_distinct_from_no_reader(self):
        with self.assertRaises(PrismLoadError):
            PrismSpeech(Path(self.directory.name) / "missing.so")

    def test_incompatible_library_fails_before_initialization(self):
        fake = FakePrism([])
        del fake.prism_backend_speak
        with patch("prism_speech.ctypes.CDLL", return_value=fake):
            with self.assertRaises(PrismLoadError):
                PrismSpeech(self.library)
        fake.prism_init.assert_not_called()

    def test_empty_or_nul_text_does_not_reach_native_api(self):
        fake = FakePrism([ORCA], [ORCA])
        adapter = self.adapter(fake)
        self.assertFalse(adapter.speak(""))
        self.assertFalse(adapter.speak("Hidden\0suffix"))
        fake.prism_registry_create.assert_not_called()


if __name__ == "__main__":
    unittest.main()
