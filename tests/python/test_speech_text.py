"""Boundary handling for explicitly verified controller glyphs."""

import unittest

from speech_text import prepare_text


class SpeechTextTests(unittest.TestCase):
    def test_adjacent_prose(self):
        self.assertEqual(prepare_text("③Random", {"③": "Y"}), "Y Random")
        self.assertEqual(prepare_text("Press①to select", {"①": "A"}),
                         "Press A to select")

    def test_punctuation_is_preserved(self):
        self.assertEqual(prepare_text("①/②: back!", {"①": "A", "②": "B"}),
                         "A / B : back!")

    def test_adjacent_glyphs_remain_separate(self):
        self.assertEqual(prepare_text("①②⑬", {"①": "A", "②": "B",
                                               "⑬": "Right stick"}),
                         "A B Right stick")

    def test_existing_spacing_is_normalized(self):
        self.assertEqual(prepare_text("  ⑬  Rotate\n", {"⑬": "Right stick"}),
                         "Right stick Rotate")

    def test_unmapped_glyph_is_literal(self):
        self.assertEqual(prepare_text("⑤Other", {"①": "A"}), "⑤Other")

    def test_plain_text_keeps_literal_punctuation_and_markup(self):
        self.assertEqual(prepare_text("  [name] <tag> A/B: value!\n"),
                         "[name] <tag> A/B: value!")


if __name__ == "__main__":
    unittest.main()
