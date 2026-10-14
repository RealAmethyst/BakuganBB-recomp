"""Original dialog events; never connect to a screen reader or launch a game."""

import json
from pathlib import Path
import unittest

from keyboard_speech import KeyboardSpeech, parse_event


class Speech:
    def __init__(self):
        self.calls = []

    def speak(self, text, interrupt=False):
        self.calls.append((text, interrupt))


def event(kind="opened", text="", **values):
    value = dict(kind=kind, id=1, title="Synthetic prompt", description="Synthetic description",
                 text=text, label="", cursor=len(text.encode()), selection_start=len(text.encode()),
                 selection_end=len(text.encode()), capacity=16, position=0, total=0, cancelled=False)
    value.update(values)
    return value


class KeyboardTests(unittest.TestCase):
    def setUp(self):
        self.speech = Speech()
        messages = json.loads(Path(__file__).with_name("messages.en.json").read_text())
        self.reader = KeyboardSpeech(messages, self.speech)

    def feed(self, value):
        parsed = parse_event(json.dumps(value))
        self.assertIsNotNone(parsed)
        self.reader.handle(parsed)

    def open(self, text=""):
        self.feed(event(text=text))
        self.feed(event("focused", text))
        self.speech.calls.clear()

    def test_open_queues_field_and_hints_once(self):
        self.feed(event(text="Ab"))
        self.feed(event("focused", "Ab"))
        self.feed(event(text="Ab"))
        self.assertEqual(self.speech.calls[:3], [
            ("Synthetic prompt", True), ("Synthetic description", False),
            ("Ab, text field", False)])
        self.assertEqual(len(self.speech.calls), 4)
        self.assertFalse(self.speech.calls[3][1])

    def test_no_invented_title(self):
        self.feed(event(title="", description=""))
        self.assertEqual(self.speech.calls[0], ("Blank, text field", True))
        self.assertEqual(len(self.speech.calls), 2)

    def test_typed_deleted_and_replaced_characters(self):
        self.open("Ab")
        self.feed(event("edited", "Abé"))
        self.feed(event("edited", "Ab"))
        self.feed(event("edited", "Ac"))
        self.assertEqual(self.speech.calls, [
            ("é", True), ("Deleted é", True), ("Deleted b", True), ("c", False)])

    def test_space_and_non_bmp_character(self):
        self.open()
        self.feed(event("edited", " "))
        self.feed(event("edited", " 🦊"))
        self.feed(event("edited", " "))
        self.assertEqual(self.speech.calls, [("Space", True), ("🦊", True), ("Deleted 🦊", True)])

    def test_multiple_spaces_are_not_lost(self):
        self.open("a  b")
        self.feed(event("edited", "ab"))
        self.feed(event("edited", "a  b"))
        self.feed(event("edited", "a  b", cursor=1, selection_start=1, selection_end=3))
        self.assertEqual(self.speech.calls, [
            ("Deleted 2 spaces", True), ("2 spaces", True), ("2 spaces, selected", True)])

    def test_selection_and_cursor_use_unicode_boundaries(self):
        self.open("Aé🦊")
        self.feed(event("edited", "Aé🦊", cursor=3, selection_start=3, selection_end=3))
        self.feed(event("edited", "Aé🦊", cursor=0, selection_start=0, selection_end=3))
        self.assertEqual(self.speech.calls, [("🦊", True), ("Aé, selected", True)])

    def test_buttons_and_field_return_do_not_repeat_title(self):
        self.open("Ab")
        self.feed(event("focused", "Ab", label="OK", position=1, total=2))
        self.feed(event("focused", "Ab", label="Cancel", position=2, total=2))
        self.feed(event("focused", "Ab"))
        self.assertEqual(self.speech.calls, [
            ("OK, 1 of 2", True), ("Cancel, 2 of 2", True), ("Ab, text field", True)])

    def test_only_native_close_permits_reopening(self):
        self.open()
        self.feed(event("closed", cancelled=True))
        self.feed(event("edited", "ignored"))
        self.assertEqual(self.speech.calls, [])
        self.feed(event())
        self.assertEqual(self.speech.calls[0], ("Synthetic prompt", True))

    def test_bad_events_fail_closed(self):
        bad = ["null", "[]", "{", "{" * 70000, json.dumps(event(cursor=True)),
               json.dumps(event(text="é", cursor=1)), json.dumps(event(text="a\0b")),
               json.dumps(event(kind=[])), json.dumps(event(kind="invented")),
               json.dumps(event(label="OK", position=0, total=2)),
               json.dumps(event(label="OK", position=3, total=3)),
               json.dumps(event(label="", position=2, total=2)),
               json.dumps(event(label="OK", position=1, total=1)),
               json.dumps(dict(event(), text="\ud800")), json.dumps(event(id=0))]
        for payload in bad:
            with self.subTest(payload=payload[:80]):
                self.assertIsNone(parse_event(payload))

    def test_missing_screen_reader_is_silent(self):
        self.reader.speech = None
        self.feed(event())
        self.feed(event("edited", "a"))
        self.assertEqual(self.speech.calls, [])

    def test_initial_focus_supplies_previously_unknown_cursor(self):
        self.feed(event(text="Ab", cursor=-1, selection_start=-1, selection_end=-1))
        self.feed(event("focused", "Ab"))
        self.assertEqual(len(self.speech.calls), 4)
        self.speech.calls.clear()
        self.feed(event("edited", "Abc"))
        self.assertEqual(self.speech.calls, [("c", True)])

    def test_selection_is_read_even_without_previous_cursor(self):
        self.feed(event(text="Ab", cursor=-1, selection_start=-1, selection_end=-1))
        self.feed(event("focused", "Ab", cursor=-1, selection_start=-1, selection_end=-1))
        self.speech.calls.clear()
        self.feed(event("edited", "Ab", cursor=0, selection_start=0, selection_end=2))
        self.assertEqual(self.speech.calls, [("Ab, selected", True)])


if __name__ == "__main__":
    unittest.main()
