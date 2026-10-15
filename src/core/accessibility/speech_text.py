"""Central preparation for plain captions and verified game text."""


def prepare_text(text, glyphs=None):
    # Preserve literal punctuation and markup until its renderer contract is verified.
    if glyphs:
        for glyph, label in glyphs.items():
            text = text.replace(glyph, f" {label} ")
    return " ".join(text.split())
