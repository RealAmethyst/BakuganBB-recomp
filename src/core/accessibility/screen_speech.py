"""Speech ordering for verified native screen and option-dialog events."""

import json

from speech_text import prepare_text


PREFIX = "BRAWLERS_SCREEN:"


def parse_event(payload):
    if len(payload) > 65536:
        return None
    try:
        event = json.loads(payload)
        if not isinstance(event, dict) or event.get("kind") not in {
                "opened", "focused", "closed", "resumed"}:
            return None
        if event.get("source") not in {
                "message", "menu", "title", "logo", "popup", "save", "creator", "status",
                "battle_selection", "battle_versus", "settings", "settings_confirmation"}:
            return None
        for key in ("id", "position", "total"):
            if type(event.get(key)) is not int or event[key] < 0:
                return None
        if not event["id"] or event["position"] > event["total"]:
            return None
        if bool(event["position"]) != bool(event["total"]):
            return None
        if event["source"] in {"status", "battle_versus"} and (event["position"] or event["total"]):
            return None
        event.setdefault("value", "")
        event.setdefault("selected", False)
        if type(event["selected"]) is not bool:
            return None
        for key in ("title", "body", "section", "label", "value", "description"):
            if not isinstance(event.get(key), str) or "\0" in event[key]:
                return None
            event[key].encode("utf-8", errors="strict")
        for key in ("title_key", "label_key", "description_key"):
            event.setdefault(key, "")
            if not isinstance(event[key], str):
                return None
        event.setdefault("locale", 0)
        if type(event["locale"]) is not int or not 0 <= event["locale"] <= 7:
            return None
        hints = event.get("hints")
        if not isinstance(hints, list) or any(not isinstance(hint, str) for hint in hints):
            return None
        for hint in hints:
            if "\0" in hint:
                return None
            hint.encode("utf-8", errors="strict")
        if event["position"] and not (prepare_text(event["label"]) or event["label_key"]):
            return None
        return event
    except (ValueError, TypeError, UnicodeError, RecursionError):
        return None


class ScreenSpeech:
    def __init__(self, messages, speech):
        self.messages = messages
        self.speech = speech
        self.states = {}

    def say(self, text, *, interrupt):
        text = prepare_text(text)
        if text and self.speech is not None:
            self.speech.speak(text, interrupt=interrupt)
        return bool(text)

    def handle(self, event):
        event = dict(event)
        event.setdefault("value", "")
        if event["source"] == "creator" and event.get("selected", False):
            event["description"] = ", ".join(
                part for part in (self.messages["screen_selected"], event["description"]) if part)
        if event["source"] == "creator" and event.get("description_key"):
            detail = self.messages.get("creator_descriptions", {}).get(event["description_key"], "")
            event["description"] = ", ".join(
                part for part in (event["description"], detail) if part)
        artwork = self.messages.get("artwork", {}).get(str(event.get("locale", 0)), {})
        for field in ("title", "label"):
            key = event.get(field + "_key", "")
            if key:
                # No fallback to another locale or a guessed caption.
                event[field] = artwork.get(key, "")
        if event["source"] in {
                "menu", "title", "popup", "save", "creator", "status", "battle_selection",
                "battle_versus", "settings", "settings_confirmation"}:
            glyphs = self.messages.get("game_glyphs", {})
            for field in ("title", "body", "section", "label", "value", "description"):
                event[field] = prepare_text(event[field], glyphs)
            event["hints"] = [prepare_text(hint, glyphs) for hint in event["hints"]]
        identity = (event["source"], event["id"])
        kind = event["kind"]
        if kind == "closed":
            self.states.pop(identity, None)
            return
        if event["source"] == "status":
            if kind == "opened":
                if identity in self.states:
                    return
                self.states[identity] = {"body": None}
            state = self.states.get(identity)
            if state is None:
                return
            body = prepare_text(event["body"])
            if body and body != state["body"]:
                self.say(body, interrupt=True)
                state["body"] = body
            return
        if kind == "opened":
            if identity in self.states:
                return
            state = {"introduced": False, "first_item": True, "section": "",
                     "item": None, "hints": event["hints"], "hints_spoken": False,
                     "queue_item": False, "repeat_item": False}
            self.states[identity] = state
            for text in (event["title"], event["body"]):
                if self.say(text, interrupt=not state["introduced"]):
                    state["introduced"] = True
                    state["queue_item"] = True
        else:
            state = self.states.get(identity)
            if state is None:
                return

        section = prepare_text(event["section"])
        section_changed = section != state["section"]
        if section_changed or kind == "resumed":
            state["section"] = section
            state["repeat_item"] = True
            initial_section = kind != "resumed" and state["first_item"] and state["introduced"]
            state["queue_item"] = initial_section
            if section:
                self.say(section, interrupt=not initial_section)
                state["introduced"] = True
                state["queue_item"] = True
        if event["hints"]:
            state["hints"] = event["hints"]
        label = prepare_text(event["label"])
        if not label:
            # The pending first item keeps its queue intent across native events.
            return
        description = prepare_text(event["description"])
        value = prepare_text(event["value"])
        item = (label, value, description, event["position"], event["total"])
        if item == state["item"] and not state["repeat_item"]:
            self.hints(state)
            return
        parts = [label, value, description]
        popup = event["source"] in {"message", "popup", "settings_confirmation"}
        if event["total"] and not (popup and event["total"] == 1):
            parts.append(self.messages["screen_position"].format(
                position=event["position"], total=event["total"]))
        self.say(", ".join(part for part in parts if part), interrupt=not state["queue_item"])
        state["item"] = item
        state["queue_item"] = False
        state["repeat_item"] = False
        if state["first_item"]:
            state["first_item"] = False
        self.hints(state)

    def hints(self, state):
        if not state["first_item"] and state["hints"] and not state["hints_spoken"]:
            for hint in state["hints"]:
                self.say(hint, interrupt=False)
            state["hints_spoken"] = True
