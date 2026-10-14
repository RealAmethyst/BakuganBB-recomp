"""Synthetic screen lifetimes and queue ordering, without a screen reader."""

import json
import unittest

from screen_speech import ScreenSpeech, parse_event


def event(kind, **changes):
    value = dict(source="menu", kind=kind, id=1, title="", body="", section="",
                 label="", description="", hints=[], position=0, total=0,
                 title_key="", label_key="", locale=0)
    value.update(changes)
    return value


class ScreenTests(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.reader = ScreenSpeech({"screen_position": "{position} of {total}"}, self)

    def speak(self, text, *, interrupt):
        self.calls.append((text, interrupt))

    def send(self, kind, **changes):
        self.reader.handle(event(kind, **changes))

    def test_delayed_item_and_hints_preserve_opening_queue(self):
        self.send("opened", title="Synthetic title", hints=["Synthetic hint"])
        self.send("focused")
        self.send("focused", label="Choice", description="Description", position=1, total=5)
        self.send("focused", label="Other", position=2, total=5)
        self.assertEqual(self.calls, [("Synthetic title", True),
                                    ("Choice, Description, 1 of 5", False),
                                    ("Synthetic hint", False), ("Other, 2 of 5", True)])

    def test_no_title_first_item_interrupts_and_one_menu_row_has_index(self):
        self.send("opened")
        self.send("focused", label="Only row", position=1, total=1)
        self.assertEqual(self.calls, [("Only row, 1 of 1", True)])

    def test_popup_body_then_single_option_without_index(self):
        self.send("opened", source="message", body="Synthetic question?")
        self.send("focused", source="message", label="Continue", position=1, total=1)
        self.assertEqual(self.calls, [("Synthetic question?", True), ("Continue", False)])

    def test_multiple_options_include_index(self):
        self.send("opened", source="message", title="Question", body="Body")
        self.send("focused", source="message", label="Later", position=2, total=2)
        self.assertEqual(self.calls, [("Question", True), ("Body", False),
                                     ("Later, 2 of 2", False)])

    def test_native_close_required_to_repeat_title(self):
        self.send("opened", title="Title")
        self.send("focused", label="Choice", position=1, total=2)
        self.send("opened", title="Title")
        self.send("focused", label="Choice", position=1, total=2)
        self.send("closed")
        self.send("focused", label="Ignored", position=1, total=2)
        self.send("opened", title="Title")
        self.assertEqual(self.calls, [("Title", True), ("Choice, 1 of 2", False),
                                     ("Title", True)])

    def test_child_return_repeats_item_without_parent_title_or_hints(self):
        self.send("opened", title="Parent", label="Choice", position=1, total=2,
                  hints=["Hint"])
        self.send("opened", source="message", body="Child question")
        self.send("closed", source="message")
        self.send("resumed", label="Choice", position=1, total=2)
        self.assertEqual(self.calls[-2:], [("Child question", True), ("Choice, 1 of 2", True)])
        self.assertEqual(sum(text == "Parent" for text, _ in self.calls), 1)
        self.assertEqual(sum(text == "Hint" for text, _ in self.calls), 1)

    def test_section_precedes_initial_item_and_changes_without_title(self):
        self.send("opened", title="Title", section="First")
        self.send("focused", section="First", label="Row", position=1, total=2)
        self.send("focused", section="Second", label="Row", position=1, total=2)
        self.assertEqual(self.calls, [("Title", True), ("First", False),
                                     ("Row, 1 of 2", False), ("Second", True),
                                     ("Row, 1 of 2", False)])

    def test_parser_rejects_incomplete_or_misleading_position(self):
        valid = event("focused", label="Choice", position=1, total=2)
        self.assertEqual(parse_event(json.dumps(valid)), valid)
        for change in ({"position": True}, {"id": 0}, {"position": 3},
                       {"position": 0}, {"label": ""}, {"hints": "Hint"},
                       {"title": "\ud800"}, {"source": "unknown"}):
            with self.subTest(change=change):
                self.assertIsNone(parse_event(json.dumps(valid | change)))
        self.assertIsNone(parse_event("[]"))
        self.assertIsNone(parse_event("{"))

    def test_no_reader_is_silent(self):
        reader = ScreenSpeech({"screen_position": "{position} of {total}"}, None)
        reader.handle(event("opened", title="Title", label="Row", position=1, total=1))

    def test_late_hints_queue_once_after_delayed_item(self):
        self.send("opened", title="Title")
        self.send("focused", label="Choice", position=1, total=2)
        self.send("focused", label="Choice", position=1, total=2, hints=["Late hint"])
        self.send("focused", label="Next", position=2, total=2, hints=["Late hint"])
        self.assertEqual(self.calls, [("Title", True), ("Choice, 1 of 2", False),
                                     ("Late hint", False), ("Next, 2 of 2", True)])

    def test_verified_artwork_has_no_locale_fallback(self):
        self.reader.messages["artwork"] = {"1": {"synthetic": "Synthetic artwork"}}
        self.send("opened", locale=2)
        self.send("focused", locale=2, label_key="synthetic", position=1, total=2)
        self.assertEqual(self.calls, [])
        self.send("focused", locale=1, label_key="synthetic", position=1, total=2)
        self.assertEqual(self.calls, [("Synthetic artwork, 1 of 2", True)])

    def test_game_glyphs_are_not_applied_to_literal_host_messages(self):
        self.reader.messages["game_glyphs"] = {"①": "A", "②": "B"}
        self.send("opened", source="title", label="Synthetic ① prompt")
        self.send("opened", source="message", body="Literal ① character")
        self.assertEqual(self.calls, [("Synthetic A prompt", True), ("Literal ① character", True)])

    def test_save_row_and_native_popup_have_independent_lifetimes(self):
        self.reader.messages["game_glyphs"] = {"①": "A", "④": "X", "⑥": "Start"}
        save = dict(source="save", label="Synthetic date", description="Synthetic duration",
                    position=1, total=1, hints=["⑥ Device ① Accept ④ Remove"])
        self.assertIsNotNone(parse_event(json.dumps(event("opened", **save))))
        self.send("opened", **save)
        self.send("opened", source="popup", body="Synthetic removal question")
        self.send("focused", source="popup", label="Keep", position=2, total=2,
                  hints=["① Accept"])
        self.send("focused", source="popup", label="Remove", position=1, total=2)
        self.send("closed", source="popup")
        self.send("resumed", **save)
        self.assertEqual(self.calls, [
            ("Synthetic date, Synthetic duration, 1 of 1", True),
            ("Start Device A Accept X Remove", False),
            ("Synthetic removal question", True),
            ("Keep, 2 of 2", False), ("A Accept", False),
            ("Remove, 1 of 2", True),
            ("Synthetic date, Synthetic duration, 1 of 1", True)])


if __name__ == "__main__":
    unittest.main()
