"""Speech for verified native keyboard-dialog events; no standalone synthesis."""

import json
import re

from speech_text import prepare_text


PREFIX = "BRAWLERS_UI:"
KINDS = frozenset({"opened", "focused", "edited", "closed"})


def parse_event(payload):
    """Reject incomplete events and byte offsets that split a Unicode character."""
    if len(payload) > 65536:
        return None
    try:
        event = json.loads(payload)
        if not isinstance(event, dict) or event.get("kind") not in KINDS:
            return None
        for key in ("id", "capacity", "position", "total"):
            if type(event.get(key)) is not int or event[key] < 0:
                return None
        if event["id"] == 0 or event["capacity"] == 0:
            return None
        if type(event.get("cancelled")) is not bool:
            return None
        for key in ("title", "description", "text", "label"):
            if not isinstance(event.get(key), str) or "\0" in event[key]:
                return None
            event[key].encode("utf-8", errors="strict")
        encoded = event["text"].encode("utf-8")
        unknown_cursor = event["kind"] in {"opened", "focused", "closed"} and all(
            type(event.get(key)) is int and event[key] == -1
            for key in ("cursor", "selection_start", "selection_end"))
        for key in ("cursor", "selection_start", "selection_end"):
            if unknown_cursor:
                continue
            if type(event.get(key)) is not int or not 0 <= event[key] <= len(encoded):
                return None
            # Convert native UTF-8 byte offsets to Python character positions.
            event[key] = len(encoded[:event[key]].decode("utf-8", errors="strict"))
        if event["label"]:
            if event["position"] not in (1, 2) or event["total"] != 2:
                return None
        elif event["position"] != 0 or event["total"] != 0:
            return None
        return event
    except (ValueError, TypeError, UnicodeError, RecursionError):
        return None


class KeyboardSpeech:
    def __init__(self, messages, speech):
        self.messages = messages
        self.speech = speech
        self.states = {}

    def say(self, text, *, interrupt):
        text = prepare_text(text)
        if text and self.speech is not None:
            self.speech.speak(text, interrupt=interrupt)

    def character_text(self, text):
        parts = []
        for part in re.split("( +)", text):
            if not part:
                continue
            if part == " ":
                parts.append(self.messages["keyboard_space"])
            elif part.isspace():
                parts.append(self.messages["keyboard_spaces"].format(count=len(part)))
            else:
                parts.append(part)
        return ", ".join(parts)

    def field_text(self, text):
        return self.messages["keyboard_field"].format(
            text=(text if text.strip() else self.character_text(text))
            if text else self.messages["keyboard_blank"])

    def handle(self, event):
        identity = event["id"]
        kind = event["kind"]
        if kind == "opened":
            if identity in self.states:
                return
            self.states[identity] = {"event": event, "focus": ("", 0), "initial_focus": True}
            opening = [event["title"], event["description"], self.field_text(event["text"]),
                       self.messages["keyboard_hints"]]
            interrupt = True
            for text in opening:
                if prepare_text(text):
                    self.say(text, interrupt=interrupt)
                    interrupt = False
            return
        state = self.states.get(identity)
        if state is None:
            return
        if kind == "closed":
            del self.states[identity]
            return
        previous = state["event"]
        if event["cursor"] < 0 and event["text"] == previous["text"]:
            event = dict(event)
            for key in ("cursor", "selection_start", "selection_end"):
                event[key] = previous[key]
        state["event"] = event
        if kind == "focused":
            label = event["label"]
            initial = state["initial_focus"]
            state["initial_focus"] = False
            focus = (label, event["position"])
            changed = focus != state["focus"]
            state["focus"] = focus
            if initial and not label:
                return
            if not changed:
                return
            if label:
                text = label
                if event["total"] > 1:
                    text += ", " + self.messages["keyboard_position"].format(
                        position=event["position"], total=event["total"])
                self.say(text, interrupt=True)
            else:
                self.say(self.field_text(event["text"]), interrupt=True)
            return
        state["initial_focus"] = False
        before, after = previous["text"], event["text"]
        if before != after:
            first = 0
            while first < min(len(before), len(after)) and before[first] == after[first]:
                first += 1
            old_end, new_end = len(before), len(after)
            while old_end > first and new_end > first and before[old_end - 1] == after[new_end - 1]:
                old_end -= 1
                new_end -= 1
            removed, inserted = before[first:old_end], after[first:new_end]
            if removed:
                self.say(self.messages["keyboard_deleted"].format(
                    text=self.character_text(removed)), interrupt=True)
            if inserted:
                self.say(self.character_text(inserted), interrupt=not bool(removed))
        elif (event["selection_start"], event["selection_end"]) != (
                previous["selection_start"], previous["selection_end"]) and (
                event["selection_start"] != event["selection_end"]):
            start, end = sorted((event["selection_start"], event["selection_end"]))
            self.say(self.messages["keyboard_selected"].format(
                text=self.character_text(after[start:end])), interrupt=True)
        elif previous["cursor"] >= 0 and event["cursor"] != previous["cursor"]:
            start, end = sorted((event["cursor"], previous["cursor"]))
            self.say(self.character_text(after[start:end]), interrupt=True)
