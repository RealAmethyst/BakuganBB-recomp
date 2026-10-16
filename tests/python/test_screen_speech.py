"""Synthetic screen lifetimes and queue ordering, without a screen reader."""

import json
import unittest

from screen_speech import ScreenSpeech, parse_event


def event(kind, **changes):
    value = dict(source="menu", kind=kind, id=1, title="", body="", section="",
                 label="", value="", description="", hints=[], position=0, total=0,
                 title_key="", section_key="", label_key="", value_key="",
                 description_key="", locale=0, selected=False, controller=0xFFFFFFFF,
                 label_template="", label_arguments=[], continuation=False)
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

    def test_battle_selection_uses_game_text_and_native_lifetime(self):
        self.reader.messages["game_glyphs"] = {"\ue000": "Confirm"}
        opening = event("opened", source="battle_selection")
        self.assertEqual(parse_event(json.dumps(opening)), opening)
        self.reader.handle(opening)
        self.send("focused", source="battle_selection", label="Synthetic card",
                  description="Synthetic effect", position=1, total=3, hints=["\ue000"])
        self.send("focused", source="battle_selection", label="Synthetic Bakugan",
                  position=2, total=3)
        self.send("closed", source="battle_selection")
        self.send("focused", source="battle_selection", label="Stale selection",
                  position=1, total=3)
        self.assertEqual(self.calls, [("Synthetic card, Synthetic effect, 1 of 3", True),
                                     ("Confirm", False), ("Synthetic Bakugan, 2 of 3", True)])

    def test_versus_names_speak_once_per_native_lifetime_without_a_menu_index(self):
        opening = event("opened", source="battle_versus", body="Synthetic opponent A, Synthetic opponent B")
        self.assertEqual(parse_event(json.dumps(opening)), opening)
        self.assertIsNone(parse_event(json.dumps(opening | {"label": "Opponent", "position": 1, "total": 2})))
        self.reader.handle(opening)
        self.reader.handle(opening)
        self.send("closed", source="battle_versus")
        self.reader.handle(opening | {"id": 2})
        self.assertEqual(self.calls, [(opening["body"], True), (opening["body"], True)])

    def test_vs_artwork_values_and_player_sections_preserve_speech_order(self):
        self.reader.messages["artwork"] = {"1": {
            "synthetic_player": "Player panel", "synthetic_slot": "Slot",
            "synthetic_value": "Occupied", "synthetic_title": "Setup",
        }}
        self.reader.messages["screen_selected"] = "Selected"
        opening = event("opened", source="vs_mode", locale=1,
                        title_key="synthetic_title", section_key="synthetic_player",
                        label_key="synthetic_slot", value_key="synthetic_value",
                        position=1, total=3, hints=["Hint"])
        self.assertEqual(parse_event(json.dumps(opening)), opening)
        self.reader.handle(opening)
        self.reader.handle(opening | {"kind": "focused", "selected": True})
        self.reader.handle(opening | {"kind": "resumed", "selected": True})
        self.assertEqual(self.calls, [
            ("Setup", True), ("Player panel", False),
            ("Slot, Occupied, 1 of 3", False), ("Hint", False),
            ("Slot, Occupied, Selected, 1 of 3", True),
            ("Player panel", True), ("Slot, Occupied, Selected, 1 of 3", False)])

    def test_vs_viewer_reads_content_and_hint_without_inventing_a_row(self):
        self.reader.messages["game_glyphs"] = {"\ue000": "Back"}
        opening = event("opened", source="vs_viewer", body="Synthetic name. Synthetic biography.",
                        hints=["\ue000"])
        self.assertEqual(parse_event(json.dumps(opening)), opening)
        self.reader.handle(opening)
        self.reader.handle(opening)
        self.assertIsNone(parse_event(json.dumps(opening | {"label": "Name", "position": 1, "total": 1})))
        self.send("closed", source="vs_viewer")
        self.reader.handle(opening | {"id": 2})
        self.assertEqual(self.calls, [(opening["body"], True), ("Back", False),
                                     (opening["body"], True), ("Back", False)])

    def test_all_controllers_keep_separate_focus_and_queue_initial_items(self):
        self.reader.messages["vs_controller"] = "Controller {number}"
        self.send("opened", source="vs_characters", title="Characters", hints=["Hint"])
        self.send("focused", source="vs_characters", controller=0, label="Same", position=1, total=3)
        self.send("focused", source="vs_characters", controller=1, label="Same", position=1, total=3)
        self.send("focused", source="vs_characters", controller=0, label="Same", position=1, total=3)
        self.send("focused", source="vs_characters", controller=1, label="Next", position=2, total=3)
        self.send("resumed", source="vs_characters", controller=0, label="Same", position=1, total=3)
        self.send("resumed", source="vs_characters", controller=1, label="Next", position=2, total=3,
                  continuation=True)
        self.assertEqual(self.calls, [("Characters", True),
            ("Controller 1, Same, 1 of 3", False), ("Hint", False),
            ("Controller 2, Same, 1 of 3", False), ("Controller 2, Next, 2 of 3", True),
            ("Controller 1, Same, 1 of 3", True), ("Controller 2, Next, 2 of 3", False)])

    def test_vs_description_and_composition_use_central_resources(self):
        self.reader.messages["vs_controller"] = "Controller {number}"
        self.reader.messages["vs_descriptions"] = {"vs_empty": "Empty slot"}
        self.reader.messages["vs_templates"] = {"vs_team": "{0} and {1} versus {2} and {3}"}
        self.reader.messages["artwork"] = {"1": {"a": "One", "b": "Two", "c": "Three", "d": "Four"}}
        self.send("opened", source="vs_mode", label_key="vs_empty", position=1, total=4)
        team = event("opened", source="vs_characters", locale=1, controller=0, section="Teams",
                     label_template="vs_team", label_arguments=[{"text": "", "key": key} for key in "abcd"],
                     position=1, total=3)
        self.assertEqual(parse_event(json.dumps(team)), team)
        self.reader.handle(team)
        self.assertEqual(self.calls, [("Empty slot, 1 of 4", True), ("Teams", True),
            ("Controller 1, One and Two versus Three and Four, 1 of 3", False)])
        self.reader.handle(team | {"kind": "focused", "locale": 2})
        self.assertEqual(len(self.calls), 3)  # Missing localized argument fails closed.

    def test_vs_status_does_not_replace_parent_focus(self):
        self.reader.messages["vs_controller"] = "Controller {number}"
        self.send("opened", source="vs_characters", controller=0, label="Portrait", position=1, total=2)
        self.send("opened", source="vs_status", controller=0, body="Committed character, Hard")
        self.send("focused", source="vs_characters", controller=0, label="Portrait", position=1, total=2)
        self.assertEqual(self.calls, [("Controller 1, Portrait, 1 of 2", True),
                                     ("Controller 1, Committed character, Hard", True)])

    def test_vs_late_hint_arrives_after_first_item_without_repeating_it(self):
        self.send("opened", source="vs_editor", controller=0)
        self.send("focused", source="vs_editor", controller=0, label="Item", position=1, total=2)
        self.send("focused", source="vs_editor", controller=0, hints=["Late hint"])
        self.send("focused", source="vs_editor", controller=0, hints=["Late hint"])
        self.assertEqual(self.calls, [("Item, 1 of 2", True), ("Late hint", False)])

    def test_vs_empty_inventory_status_queues_late_back_hint_without_an_index(self):
        self.send("opened", source="vs_editor_status", section="Synthetic deck", body="No matching items")
        self.send("focused", source="vs_editor_status", body="No matching items", hints=["Back"])
        self.send("focused", source="vs_editor_status", body="No matching items", hints=["Back"])
        self.assertEqual(self.calls, [("Synthetic deck", True), ("No matching items", False), ("Back", False)])

    def test_vs_presentation_parser_rejects_bad_controller_and_arguments(self):
        item = event("focused", source="vs_characters", label_template="vs_team",
                     label_arguments=[{"text": "", "key": "identity"}], position=1, total=3)
        for change in ({"controller": True}, {"controller": 4}, {"controller": -1},
                       {"label_arguments": [{"text": "x", "key": "y"}]},
                       {"label_arguments": [{"text": "", "key": ""}]},
                       {"label_arguments": []}, {"label_template": ""},
                       {"label_arguments": [{"text": "\ud800", "key": ""}]},
                       {"source": "creator"}):
            with self.subTest(change=change):
                self.assertIsNone(parse_event(json.dumps(item | change)))

    def test_vs_missing_artwork_value_has_no_guessed_or_other_locale_fallback(self):
        self.reader.messages["artwork"] = {"1": {"value": "Other language"}}
        self.send("opened", source="vs_mode", locale=2, label="Slot",
                  value="Unverified fallback", value_key="value", position=1, total=2)
        self.assertEqual(self.calls, [("Slot, 1 of 2", True)])
        invalid = event("opened", source="vs_mode", value_key=7)
        self.assertIsNone(parse_event(json.dumps(invalid)))

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

    def test_settings_child_return_and_reset_preserve_parent_lifetime(self):
        opening = event("opened", source="settings", title="Synthetic options",
                        label="Synthetic category", position=1, total=5, hints=["Navigate"])
        self.assertEqual(parse_event(json.dumps(opening)), opening)
        self.reader.handle(opening)
        self.send("resumed", source="settings", section="Synthetic category",
                  label="Synthetic volume", value="20%", description="Synthetic detail",
                  position=1, total=2)
        question = event("opened", source="settings_confirmation", body="Reset category?",
                         label="Cancel", position=2, total=2)
        self.assertEqual(parse_event(json.dumps(question)), question)
        self.reader.handle(question)
        self.send("closed", source="settings_confirmation")
        self.send("resumed", source="settings", section="Synthetic category",
                  label="Reset", position=2, total=2)
        self.assertEqual(self.calls, [
            ("Synthetic options", True), ("Synthetic category, 1 of 5", False),
            ("Navigate", False), ("Synthetic category", True),
            ("Synthetic volume, 20%, Synthetic detail, 1 of 2", False),
            ("Reset category?", True), ("Cancel, 2 of 2", False),
            ("Synthetic category", True), ("Reset, 2 of 2", False)])
        self.send("closed", source="settings")
        self.send("focused", source="settings", label="Stale", position=1, total=1)
        self.assertNotIn(("Stale, 1 of 1", True), self.calls)

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

    def test_creator_value_changes_include_value_description_and_index(self):
        row = dict(source="creator", label="Synthetic setting", value="First value",
                   description="Synthetic description", position=1, total=1)
        self.send("opened", **row)
        self.send("focused", **row)
        row["value"] = "Second value"
        self.send("focused", **row)
        self.send("focused", **row)
        self.assertEqual(self.calls, [
            ("Synthetic setting, First value, Synthetic description, 1 of 1", True),
            ("Synthetic setting, Second value, Synthetic description, 1 of 1", True)])

    def test_creator_visual_description_supplements_native_caption(self):
        self.reader.messages["creator_descriptions"] = {"synthetic_shape": "Synthetic visual detail"}
        row = dict(source="creator", label="Native caption", description="Native detail",
                   position=1, total=2, description_key="synthetic_shape")
        self.send("opened", **row)
        self.send("focused", **row)
        self.send("focused", **(row | {"description_key": "unknown_shape"}))
        self.assertEqual(self.calls, [
            ("Native caption, Native detail, Synthetic visual detail, 1 of 2", True),
            ("Native caption, Native detail, 1 of 2", True)])

    def test_new_sources_and_optional_value_are_validated(self):
        legacy = event("opened")
        del legacy["value"]
        self.assertEqual(parse_event(json.dumps(legacy))["value"], "")
        creator = event("focused", source="creator", label="Setting", value="Value",
                        position=1, total=2)
        self.assertEqual(parse_event(json.dumps(creator)), creator)
        for value in (None, 1, "\0", "\ud800"):
            with self.subTest(value=value):
                self.assertIsNone(parse_event(json.dumps(creator | {"value": value})))
        status = event("opened", source="status", body="Synthetic status")
        self.assertEqual(parse_event(json.dumps(status)), status)
        self.assertIsNone(parse_event(json.dumps(status | {
            "label": "Not a choice", "position": 1, "total": 1})))

    def test_creator_selection_uses_short_localized_state(self):
        self.reader.messages["screen_selected"] = "Selected"
        self.reader.messages["creator_descriptions"] = {"skin": "Light peach skin."}
        row = dict(source="creator", label="Synthetic tone", selected=True,
                   description_key="skin", position=1, total=5)
        self.send("opened", **row)
        self.send("focused", **row)
        self.send("focused", **(row | {"selected": False}))
        self.assertEqual(self.calls, [
            ("Synthetic tone, Selected, Light peach skin., 1 of 5", True),
            ("Synthetic tone, Light peach skin., 1 of 5", True)])
        for invalid in (None, 0, 1, "yes"):
            self.assertIsNone(parse_event(json.dumps(event("focused", **(row | {
                "selected": invalid})))))
        legacy = event("opened")
        del legacy["selected"]
        self.assertFalse(parse_event(json.dumps(legacy))["selected"])

    def test_delayed_item_after_section_change_keeps_queue_and_repeat_intent(self):
        row = dict(source="creator", label="Same row", position=1, total=2)
        self.send("opened", title="Creator title", section="First section", **row)
        self.send("focused", source="creator", section="Second section")
        self.send("focused", source="creator", section="Second section")
        self.send("focused", section="Second section", **row)
        self.send("focused", section="Second section", **row)
        self.send("focused", source="creator", section="Second section",
                  label="Next row", position=2, total=2)
        self.assertEqual(self.calls, [
            ("Creator title", True), ("First section", False), ("Same row, 1 of 2", False),
            ("Second section", True), ("Same row, 1 of 2", False), ("Next row, 2 of 2", True)])

    def test_delayed_parent_return_repeats_same_item_once_with_or_without_section(self):
        for section in ("", "Synthetic section"):
            with self.subTest(section=section):
                self.setUp()
                row = dict(source="creator", section=section, label="Same row", value="Same value",
                           position=1, total=1)
                self.send("opened", title="Parent title", hints=["Parent hint"], **row)
                self.send("opened", source="message", body="Child body")
                self.send("closed", source="message")
                self.calls.clear()
                self.send("resumed", source="creator", section=section)
                self.send("focused", source="creator", section=section)
                self.send("focused", **row)
                self.send("focused", **row)
                expected = [(section, True)] if section else []
                expected.append(("Same row, Same value, 1 of 1", not bool(section)))
                self.assertEqual(self.calls, expected)

    def test_return_before_first_item_interrupts_section_then_queues_item_and_hints(self):
        self.send("opened", source="creator", title="Parent title", section="Section",
                  hints=["Parent hint"])
        self.send("opened", source="message", body="Child body")
        self.send("closed", source="message")
        self.send("resumed", source="creator", section="Section")
        self.send("focused", source="creator", section="Section", label="Initial row",
                  position=1, total=1)
        self.assertEqual(self.calls, [
            ("Parent title", True), ("Section", False), ("Child body", True),
            ("Section", True), ("Initial row, 1 of 1", False), ("Parent hint", False)])

    def test_status_announces_body_changes_without_items_or_frame_repeats(self):
        status = dict(source="status", body="Synthetic status", title="Unused title",
                      label="Unused label", hints=["Unused hint"])
        self.send("focused", **status)
        self.send("opened", **status)
        self.send("opened", **status)
        self.send("focused", **status)
        self.send("focused", source="status")
        self.send("focused", **status)
        self.send("focused", source="status", body="Changed status")
        self.send("focused", source="status", body="Changed status")
        self.send("closed", source="status")
        self.send("focused", **status)
        self.send("opened", **status)
        self.assertEqual(self.calls, [("Synthetic status", True), ("Changed status", True),
                                     ("Synthetic status", True)])

    def test_creator_status_lifetimes_and_game_glyphs_are_independent(self):
        self.reader.messages["game_glyphs"] = {"①": "A"}
        row = dict(source="creator", label="Synthetic row", value="①", position=1, total=1)
        self.send("opened", title="Creator title", **row)
        self.send("opened", source="status", body="Synthetic status ①")
        self.send("closed", source="status")
        self.send("resumed", **row)
        self.assertEqual(self.calls, [("Creator title", True), ("Synthetic row, A, 1 of 1", False),
                                     ("Synthetic status A", True), ("Synthetic row, A, 1 of 1", True)])


if __name__ == "__main__":
    unittest.main()
