"""Speech ordering for verified native screen and option-dialog events."""

import json

from speech_text import prepare_text


PREFIX = "BRAWLERS_SCREEN:"
NO_CONTROLLER = 0xFFFFFFFF
VS_SOURCES = {"vs_mode", "vs_rules", "vs_characters", "vs_arena", "vs_decks", "vs_viewer",
              "vs_editor", "vs_editor_picker", "vs_editor_confirmation", "vs_editor_status",
              "vs_status"}
STATUS_SOURCES = {"status", "vs_status", "vs_editor_status"}


def parse_event(payload):
    if len(payload) > 65536:
        return None
    try:
        event = json.loads(payload)
        if not isinstance(event, dict) or event.get("kind") not in {
                "opened", "focused", "closed", "resumed"}:
            return None
        if event.get("source") not in VS_SOURCES | {
                "message", "menu", "title", "logo", "popup", "save", "creator", "status",
                "battle_selection", "battle_versus", "settings", "settings_confirmation",
                "vs_mode", "vs_rules", "vs_characters", "vs_arena", "vs_decks", "vs_viewer"}:
            return None
        for key in ("id", "position", "total"):
            if type(event.get(key)) is not int or event[key] < 0:
                return None
        if not event["id"] or event["position"] > event["total"]:
            return None
        if bool(event["position"]) != bool(event["total"]):
            return None
        if event["source"] in STATUS_SOURCES | {"battle_versus", "vs_viewer"} and (event["position"] or event["total"]):
            return None
        event.setdefault("value", "")
        event.setdefault("selected", False)
        if type(event["selected"]) is not bool:
            return None
        for key in ("title", "body", "section", "label", "value", "description"):
            if not isinstance(event.get(key), str) or "\0" in event[key]:
                return None
            event[key].encode("utf-8", errors="strict")
        for key in ("title_key", "section_key", "label_key", "value_key", "description_key"):
            event.setdefault(key, "")
            if not isinstance(event[key], str) or "\0" in event[key]:
                return None
            event[key].encode("utf-8", errors="strict")
        event.setdefault("controller", NO_CONTROLLER)
        if type(event["controller"]) is not int or event["controller"] not in {0, 1, 2, 3, NO_CONTROLLER}:
            return None
        event.setdefault("continuation", False)
        if type(event["continuation"]) is not bool or (event["continuation"] and (
                event["source"] not in VS_SOURCES or event["kind"] != "resumed")):
            return None
        event.setdefault("label_template", "")
        event.setdefault("label_arguments", [])
        if not isinstance(event["label_template"], str) or "\0" in event["label_template"]:
            return None
        event["label_template"].encode("utf-8", errors="strict")
        arguments = event["label_arguments"]
        if not isinstance(arguments, list) or len(arguments) > 8:
            return None
        if bool(arguments) != bool(event["label_template"]):
            return None
        for argument in arguments:
            if not isinstance(argument, dict) or set(argument) != {"text", "key"}:
                return None
            for value in argument.values():
                if not isinstance(value, str) or "\0" in value:
                    return None
                value.encode("utf-8", errors="strict")
            if bool(argument["text"]) == bool(argument["key"]):
                return None
        if event["source"] not in VS_SOURCES and (
                event["controller"] != NO_CONTROLLER or event["label_template"]):
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
        if event["position"] and not (prepare_text(event["label"]) or event["label_key"] or event["label_template"]):
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
        if event["source"] in {"creator", "vs_mode", "vs_characters"} and event.get("selected", False):
            event["description"] = ", ".join(
                part for part in (self.messages["screen_selected"], event["description"]) if part)
        if event["source"] == "creator" and event.get("description_key"):
            detail = self.messages.get("creator_descriptions", {}).get(event["description_key"], "")
            event["description"] = ", ".join(
                part for part in (event["description"], detail) if part)
        artwork = self.messages.get("artwork", {}).get(str(event.get("locale", 0)), {})
        descriptors = self.messages.get("vs_descriptions", {}) if event["source"] in VS_SOURCES else {}

        def resolve(key):
            return descriptors.get(key, artwork.get(key, ""))

        for field in ("title", "section", "label", "value"):
            key = event.get(field + "_key", "")
            if key:
                # No fallback to another locale or a guessed caption.
                event[field] = resolve(key)
        if event["source"] in VS_SOURCES and event.get("description_key"):
            detail = resolve(event["description_key"])
            event["description"] = ", ".join(part for part in (event["description"], detail) if part)
        if event.get("label_template"):
            template = self.messages.get("vs_templates", {}).get(event["label_template"], "")
            arguments = [resolve(arg["key"]) if arg["key"] else arg["text"]
                         for arg in event.get("label_arguments", [])]
            if not template or not arguments or not all(arguments):
                return
            try:
                event["label"] = template.format(*arguments)
            except (IndexError, KeyError, ValueError):
                return
        if event["source"] in VS_SOURCES | {
                "menu", "title", "popup", "save", "creator", "status", "battle_selection",
                "battle_versus", "settings", "settings_confirmation",
                "vs_mode", "vs_rules", "vs_characters", "vs_arena", "vs_decks", "vs_viewer"}:
            glyphs = self.messages.get("game_glyphs", {})
            for field in ("title", "body", "section", "label", "value", "description"):
                event[field] = prepare_text(event[field], glyphs)
            event["hints"] = [prepare_text(hint, glyphs) for hint in event["hints"]]
        identity = (event["source"], event["id"])
        kind = event["kind"]
        controller = event.get("controller", NO_CONTROLLER)

        def identified(text):
            if not text or controller == NO_CONTROLLER:
                return text
            prefix = self.messages.get("vs_controller", "").format(number=controller + 1)
            return ", ".join(part for part in (prefix, text) if part)

        if kind == "closed":
            self.states.pop(identity, None)
            return
        if event["source"] in STATUS_SOURCES:
            if kind == "opened":
                if identity in self.states:
                    return
                self.states[identity] = {"bodies": {}, "first_item": True,
                                         "hints": event["hints"], "hints_spoken": False,
                                         "section": "", "queue_body": False}
            state = self.states.get(identity)
            if state is None:
                return
            body = prepare_text(event["body"] or (event["label"] if event["source"] in VS_SOURCES else ""))
            if event["source"] == "vs_editor_status" and event["section"] and event["section"] != state["section"]:
                state["queue_body"] = self.say(event["section"], interrupt=True)
                state["section"] = event["section"]
            if body and body != state["bodies"].get(controller):
                self.say(identified(body), interrupt=not state["queue_body"])
                state["bodies"][controller] = body
                state["first_item"] = False
                state["queue_body"] = False
            if event["source"] == "vs_editor_status":
                if event["hints"]:
                    state["hints"] = event["hints"]
                self.hints(state)
            return
        if kind == "opened":
            if identity in self.states:
                return
            state = {"introduced": False, "first_item": True, "contexts": {},
                     "hints": event["hints"], "hints_spoken": False}
            self.states[identity] = state
            for text in (event["title"], event["body"]):
                if self.say(identified(text), interrupt=not state["introduced"]):
                    state["introduced"] = True
            if event["source"] == "vs_viewer":
                # This native detail viewer has content and a Back hint, but no
                # selectable rows. Its capture waits for the complete content.
                state["first_item"] = False
                self.hints(state)
                return
        else:
            state = self.states.get(identity)
            if state is None:
                return

        context = state["contexts"].setdefault(controller, {
            "first_item": True, "section": "", "item": None, "repeat_item": False,
            "queue_item": state["introduced"] or not state["first_item"]})
        section = prepare_text(event["section"])
        section_changed = section != context["section"]
        if section_changed or kind == "resumed":
            context["section"] = section
            context["repeat_item"] = True
            initial_section = event.get("continuation", False) or (
                kind != "resumed" and context["first_item"] and context["queue_item"])
            context["queue_item"] = initial_section
            if section:
                self.say(section, interrupt=not initial_section)
                state["introduced"] = True
                context["queue_item"] = True
        if event["hints"]:
            state["hints"] = event["hints"]
        label = prepare_text(event["label"])
        if not label:
            # The pending first item keeps its queue intent across native events.
            self.hints(state)
            return
        description = prepare_text(event["description"])
        value = prepare_text(event["value"])
        item = (label, value, description, event["position"], event["total"])
        if item == context["item"] and not context["repeat_item"]:
            self.hints(state)
            return
        parts = [label, value, description]
        popup = event["source"] in {"message", "popup", "settings_confirmation", "vs_editor_confirmation"}
        if event["total"] and not (popup and event["total"] == 1):
            parts.append(self.messages["screen_position"].format(
                position=event["position"], total=event["total"]))
        self.say(identified(", ".join(part for part in parts if part)), interrupt=not context["queue_item"])
        context["item"] = item
        context["queue_item"] = False
        context["repeat_item"] = False
        context["first_item"] = False
        if state["first_item"]:
            state["first_item"] = False
        self.hints(state)

    def hints(self, state):
        if not state["first_item"] and state["hints"] and not state["hints_spoken"]:
            for hint in state["hints"]:
                self.say(hint, interrupt=False)
            state["hints_spoken"] = True
