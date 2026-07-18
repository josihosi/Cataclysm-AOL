#!/usr/bin/env python3
"""Deterministic named-NPC smoke harness for AOL's LLM runner.

What this does:
- mirrors summary selection precedence from src/llm_intent.cpp
- builds a small but game-shaped snapshot for a named NPC
- renders the normal npc_action_prompt template
- sends one request through tools/llm_runner/runner.py over stdin/stdout JSON
- validates the returned pipe-separated action line with the same broad rules as the game

It is intentionally lightweight: useful for repeatable smoke tests without booting the game.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

PROMPT_DIRNAME = "llm_prompts"
NPC_ACTION_PROMPT_FILENAME = "npc_action_prompt.txt"
DEFAULT_TIMEOUT_SECONDS = 60.0
DEFAULT_MAX_TOKENS = 256
DEFAULT_MAX_PROMPT_LEN = 4096
ALLOWED_ACTIONS = [
    "wait_here",
    "hold_position",
    "follow_close",
    "follow_far",
    "equip_gun",
    "equip_melee",
    "equip_bow",
    "panic_on",
    "panic_off",
    "look_around",
    "look_inventory",
    "idle",
]
VALID_MOVE_TERMINAL_STATES = {"wait_here", "hold_position"}
SNAPSHOT_MOVE_RADIUS = 20


@dataclass
class SummaryEntry:
    selector: str
    background: str
    expression: str
    source_tag: str
    file_path: str


@dataclass
class ResolvedSummary:
    selected_selector: Optional[str]
    entry: Optional[SummaryEntry]
    attempted_selectors: List[str]
    resolution_kind: str = "none"
    matched_trait: Optional[str] = None
    matched_topic: Optional[str] = None


DEFAULT_SCENARIO = {
    "player_name": "Test Survivor",
    "player_utterance": "Rubik, what do you have for sale?",
    "npc_name": "Rubik",
    "chatbin": {
        "talk_friend": "",
        "talk_friend_guard": "",
        "first_topic": "TALK_EXODII_MERCHANT",
        "talk_leader": "",
        "talk_stranger_friendly": "",
        "talk_stranger_neutral": "",
        "talk_stranger_wary": "",
        "talk_stranger_scared": ""
    },
    "state": {
        "follow_mode": "follow-close",
        "morale": 6,
        "hunger": 0,
        "thirst": 0,
        "pain": 0,
        "stamina": 9,
        "sleepiness": 0,
        "hp_percent": 100,
        "danger_assessment": 1,
        "panic": 0,
        "confidence": 7,
        "aggression": 1,
        "bravery": 6,
        "collector": 8,
        "altruism": 2,
        "trust": 5,
        "intimidation": 1,
        "respect": 7,
        "anger": 0
    },
    "world": {
        "threats": "(none)",
        "friendlies": "player",
        "inventory": "wielded=\"integrated toolset\"",
        "weapons": "[none listed]",
        "bandage_possible": "false",
        "map": "-----\n--a--\n--|--\n-----\n-----"
    },
    "expectations": {
        "source_tag_contains": "special_npc:Rubik",
        "selector_equals": "name:Rubik"
    }
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def bundled_prompt_dir() -> Path:
    return repo_root() / "data" / PROMPT_DIRNAME


def prompt_override_dir() -> Path:
    return repo_root() / "config" / PROMPT_DIRNAME


def normalize_line(text: str) -> str:
    return " ".join(str(text or "").replace("\r", "").replace("\n", " ").split())


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def has_required_tokens(template: str, required_tokens: Iterable[str]) -> bool:
    return bool(template.strip()) and all(token in template for token in required_tokens)


def seed_prompt_override_file(filename: str) -> None:
    prompt_override_dir().mkdir(parents=True, exist_ok=True)
    source = bundled_prompt_dir() / filename
    dest = prompt_override_dir() / filename
    if source.exists() and not dest.exists():
        dest.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")


def load_prompt_template(filename: str, fallback_template: str, required_tokens: Iterable[str]) -> str:
    try:
        seed_prompt_override_file(filename)
    except Exception:
        pass
    for path in (prompt_override_dir() / filename, bundled_prompt_dir() / filename):
        if not path.exists():
            continue
        try:
            template = read_text(path)
        except Exception:
            continue
        if has_required_tokens(template, required_tokens):
            return template
    return fallback_template


def default_prompt_template() -> str:
    return (
        "Situation:\n{{snapshot}}\n"
        "<System>You are controlling a human survivor NPC in a cataclysmic world."
        "Return a single line only, with correct syntax, to be parsed by the game."
        "Use actions from {{action_list_with_target}}.</System>"
    )


def render_prompt(template: str, snapshot: str) -> str:
    action_list = ", ".join(ALLOWED_ACTIONS) + ", attack=<target>, move=<dx>,<dy> <state>"
    return template.replace("{{snapshot}}", snapshot).replace("{{action_list_with_target}}", action_list)


def render_map_with_axes(raw_map: str) -> str:
    rows = raw_map.splitlines()
    if not rows or not rows[0]:
        raise ValueError("scenario map must contain at least one row")
    width = len(rows[0])
    if any(len(row) != width for row in rows):
        raise ValueError("scenario map rows must have equal width")
    origins = [
        (row_index, column_index)
        for row_index, row in enumerate(rows)
        for column_index, glyph in enumerate(row)
        if glyph == "|"
    ]
    if len(origins) != 1:
        raise ValueError("scenario map must contain exactly one '|' NPC origin")
    origin_row, origin_column = origins[0]

    labels = [" "] * width
    markers = ["."] * width
    for column in range(width):
        dx = column - origin_column
        if dx % 10 != 0:
            continue
        markers[column] = "|"
        label = "0" if dx == 0 else f"{dx:+d}"
        centered_start = column - len(label) // 2
        clamped_start = max(0, min(centered_start, max(width - len(label), 0)))
        labels[clamped_start:clamped_start + len(label)] = label

    rendered = ["        " + "".join(labels), "        " + "".join(markers)]
    for row_index, row in enumerate(rows):
        dy = origin_row - row_index
        rendered.append(f"dy={dy:+03d} {row}")
    return "\n".join(rendered)


def parse_summary_text_file(path: Path) -> Dict[str, SummaryEntry]:
    out: Dict[str, SummaryEntry] = {}
    for raw_line in read_text(path).splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split("|")]
        if len(parts) < 3:
            continue
        selector = normalize_line(parts[0])
        if not selector:
            continue
        out[selector] = SummaryEntry(
            selector=selector,
            background=normalize_line(parts[1]),
            expression=normalize_line(parts[2]),
            source_tag=normalize_line(parts[3]) if len(parts) > 3 else "",
            file_path=str(path),
        )
    return out


def summary_ids_from_json_record(record: Dict[str, object]) -> List[str]:
    ids: List[str] = []
    for key in ("selector", "topic", "id"):
        value = record.get(key)
        if isinstance(value, str):
            normalized = normalize_line(value)
            if normalized and normalized not in ids:
                ids.append(normalized)
    for key in ("selectors", "topics"):
        value = record.get(key)
        if not isinstance(value, list):
            continue
        for item in value:
            if not isinstance(item, str):
                continue
            normalized = normalize_line(item)
            if normalized and normalized not in ids:
                ids.append(normalized)
    return ids


def parse_summary_json_file(path: Path) -> Dict[str, SummaryEntry]:
    out: Dict[str, SummaryEntry] = {}
    try:
        data = json.loads(read_text(path))
    except Exception:
        return out
    if isinstance(data, dict) and isinstance(data.get("entries"), list):
        records = data.get("entries", [])
    elif isinstance(data, list):
        records = data
    elif isinstance(data, dict):
        records = [data]
    else:
        return out
    for record in records:
        if not isinstance(record, dict):
            continue
        background = normalize_line(str(record.get("your_background", record.get("background", ""))))
        expression = normalize_line(str(record.get("your_expression", record.get("expression", ""))))
        source_tag = normalize_line(str(record.get("source_tag", "")))
        if not background or not expression:
            continue
        for selector in summary_ids_from_json_record(record):
            out[selector] = SummaryEntry(
                selector=selector,
                background=background,
                expression=expression,
                source_tag=source_tag,
                file_path=str(path),
            )
    return out


def summary_file_priority(path: Path) -> Tuple[int, str, int, str]:
    generation_priority = 0 if path.name.startswith("generated_") else 1
    format_priority = 0 if path.suffix == ".txt" else 1 if path.suffix == ".json" else 2
    return (generation_priority, path.stem, format_priority, path.name)


def load_summary_dir(summary_dir: Path) -> Dict[str, SummaryEntry]:
    resolved: Dict[str, SummaryEntry] = {}
    if not summary_dir.exists():
        return resolved
    files = sorted(
        [path for path in summary_dir.iterdir() if path.is_file() and path.suffix in {".txt", ".json"}],
        key=summary_file_priority,
    )
    for path in files:
        if path.suffix == ".json":
            resolved.update(parse_summary_json_file(path))
        else:
            resolved.update(parse_summary_text_file(path))
    return resolved


def default_summary_roots() -> List[Path]:
    return [repo_root() / "data" / "json"]


def gather_traits_from_condition(condition: object, out: List[str]) -> None:
    if not isinstance(condition, dict):
        return
    trait = condition.get("npc_has_trait")
    if isinstance(trait, str) and trait.strip():
        out.append(trait.strip())
    for key in ("and", "or"):
        value = condition.get(key)
        if isinstance(value, list):
            for entry in value:
                gather_traits_from_condition(entry, out)


def load_trait_to_topic(summary_roots: Iterable[Path]) -> Dict[str, str]:
    mapping: Dict[str, str] = {}
    for root in summary_roots:
        toc_path = root / "npcs" / "Backgrounds" / "backgrounds_table_of_contents.json"
        if not toc_path.exists():
            continue
        try:
            data = json.loads(read_text(toc_path))
        except Exception:
            continue
        if not isinstance(data, list):
            continue
        for entry in data:
            if not isinstance(entry, dict) or entry.get("type") != "talk_topic":
                continue
            responses = entry.get("responses")
            if not isinstance(responses, list):
                continue
            for response in responses:
                if not isinstance(response, dict):
                    continue
                topic = response.get("topic")
                condition = response.get("condition")
                if not isinstance(topic, str) or not isinstance(condition, dict):
                    continue
                traits: List[str] = []
                gather_traits_from_condition(condition, traits)
                for trait in traits:
                    mapping[trait] = topic
    return mapping


def load_all_summaries(summary_roots: Iterable[Path]) -> Tuple[Dict[str, SummaryEntry], Dict[str, SummaryEntry], Dict[str, str]]:
    by_topic: Dict[str, SummaryEntry] = {}
    by_selector: Dict[str, SummaryEntry] = {}
    for root in summary_roots:
        by_topic.update(load_summary_dir(root / "npcs" / "Backgrounds" / "Summaries_short"))
        by_selector.update(load_summary_dir(root / "npcs" / "Backgrounds" / "Summaries_extra"))
    trait_to_topic = load_trait_to_topic(summary_roots)
    return by_topic, by_selector, trait_to_topic


def scenario_selectors(scenario: Dict[str, object]) -> List[str]:
    npc_name = str(scenario.get("npc_name", "")).strip()
    chatbin = scenario.get("chatbin", {}) or {}
    if not isinstance(chatbin, dict):
        chatbin = {}
    selectors: List[str] = []

    def add(selector: str) -> None:
        normalized = normalize_line(selector)
        if normalized and normalized not in selectors:
            selectors.append(normalized)

    if npc_name:
        add(f"name:{npc_name}")
    for key in (
        "talk_friend",
        "talk_friend_guard",
        "first_topic",
        "talk_leader",
        "talk_stranger_friendly",
        "talk_stranger_neutral",
        "talk_stranger_wary",
        "talk_stranger_scared",
        "talk_stranger_aggressive",
    ):
        topic = str(chatbin.get(key, "")).strip()
        if topic:
            add(f"topic:{topic}")
    for extra in scenario.get("extra_selectors", []) or []:
        add(str(extra))
    return selectors


def resolve_summary(scenario: Dict[str, object], summary_roots: Iterable[Path]) -> ResolvedSummary:
    by_topic, by_selector, trait_to_topic = load_all_summaries(summary_roots)
    selectors = scenario_selectors(scenario)
    for selector in selectors:
        if selector in by_selector:
            return ResolvedSummary(selector, by_selector[selector], selectors, resolution_kind="selector")
    for selector in selectors:
        topic = selector.removeprefix("topic:")
        if topic in by_topic:
            return ResolvedSummary(selector, by_topic[topic], selectors, resolution_kind="topic")
    for trait in scenario.get("mutations", []) or []:
        trait_name = str(trait).strip()
        if not trait_name:
            continue
        topic = trait_to_topic.get(trait_name)
        if not topic:
            continue
        entry = by_topic.get(topic)
        if not entry:
            continue
        return ResolvedSummary(f"trait:{trait_name}", entry, selectors, resolution_kind="trait", matched_trait=trait_name, matched_topic=topic)
    return ResolvedSummary(None, None, selectors)


def build_snapshot(scenario: Dict[str, object], resolved: ResolvedSummary, request_id: str) -> str:
    state = scenario.get("state", {}) or {}
    world = scenario.get("world", {}) or {}
    npc_name = str(scenario.get("npc_name", "Unknown NPC"))
    player_name = str(scenario.get("player_name", "Test Survivor"))
    player_utterance = str(scenario.get("player_utterance", "Hello."))
    profession = str(scenario.get("profession", "no_past")).strip() or "no_past"
    mutations = [str(item).strip() for item in (scenario.get("mutations", []) or []) if str(item).strip()]
    background = resolved.entry.background if resolved.entry else ""
    expression = resolved.entry.expression if resolved.entry else ""
    lines = [
        "SITUATION",
        f"id: {request_id}",
        f"player_name: {player_name}",
        f"player_utterance: {player_utterance}",
        f"player utterance present: {'true' if player_utterance.strip() else 'false'}",
        "",
        f"your_name: {npc_name}",
        f"your_profession: {profession}",
    ]
    if background:
        lines.append(f"your_tone: {background}")
    if expression:
        lines.append(f"your_example_expression: {expression}")
    if mutations:
        lines.append(f"your_background_traits: [{', '.join(mutations)}]")
    inventory_value = world.get("inventory", 'wielded="none"')
    weapons_value = world.get("weapons", "[none]")
    bandage_value = world.get("bandage_possible", "false")
    lines.extend([
        f"your_follow_mode: {state.get('follow_mode', 'follow-close')}",
        "your_recent_memories: (none)",
        (
            "your_state[0-10]: morale={morale} hunger={hunger} thirst={thirst} pain={pain} "
            "stamina={stamina} sleepiness={sleepiness} hp_percent={hp_percent} effects=[]"
        ).format(
            morale=state.get("morale", 5),
            hunger=state.get("hunger", 0),
            thirst=state.get("thirst", 0),
            pain=state.get("pain", 0),
            stamina=state.get("stamina", 10),
            sleepiness=state.get("sleepiness", 0),
            hp_percent=state.get("hp_percent", 100),
        ),
        (
            "your_emotions[0-10]: danger_assessment={danger_assessment} panic={panic} confidence={confidence}"
        ).format(
            danger_assessment=state.get("danger_assessment", 0),
            panic=state.get("panic", 0),
            confidence=state.get("confidence", 5),
        ),
        (
            "your_personality[0-10]: aggression={aggression} bravery={bravery} collector={collector} altruism={altruism}"
        ).format(
            aggression=state.get("aggression", 3),
            bravery=state.get("bravery", 5),
            collector=state.get("collector", 5),
            altruism=state.get("altruism", 5),
        ),
        (
            "your_opinion_of_player[0-10]: trust={trust} intimidation={intimidation} respect={respect} anger={anger}"
        ).format(
            trust=state.get("trust", 5),
            intimidation=state.get("intimidation", 0),
            respect=state.get("respect", 5),
            anger=state.get("anger", 0),
        ),
        "",
        f"threats: {world.get('threats', '(none)')}",
        f"friendlies: {world.get('friendlies', 'player')}",
        "",
        f"inventory: {inventory_value}",
        f"weapons: {weapons_value}",
        f"bandage_possible: {bandage_value}",
        "",
        "legend:",
        "- ... open area",
        "0 ... obstructive area (movement speed > 100)",
        "6 ... obstructed area",
        "[a - z] ... creature",
        "[A - Z] ... obstructed creature",
        "? ... unlettered creature (not a target handle)",
        "| ... You (NPC)",
        "map_legend:",
        "a ... player",
        "map axes: +x east/right, -x west/left, +y north/up, -y south/down",
        "map:",
        render_map_with_axes(str(world.get("map", "-----\n--a--\n--|--\n-----\n-----"))),
    ])
    return "\n".join(lines) + "\n"


def parse_move_field(field: str) -> Tuple[Optional[Tuple[int, int]], Optional[str], Optional[str]]:
    raw = field.strip()
    lowered = raw.lower()
    if not lowered.startswith("move="):
        return None, None, "Move field must use move=<dx>,<dy> <state>."
    lowered = lowered[5:].strip()
    parts = [part for part in lowered.split() if part]
    if len(parts) != 2:
        return None, None, "Move field must include one delta and terminal state."
    terminal = parts[-1]
    if terminal not in VALID_MOVE_TERMINAL_STATES:
        return None, None, "Move field terminal state is invalid."
    delta = parts[0]
    if delta.count(",") != 1:
        return None, None, "Move field delta is invalid."
    dx_token, dy_token = delta.split(",", 1)
    def is_ascii_signed_integer(token: str) -> bool:
        digits = token[1:] if token.startswith(("+", "-")) else token
        return bool(digits) and all("0" <= char <= "9" for char in digits)
    if not is_ascii_signed_integer(dx_token) or not is_ascii_signed_integer(dy_token):
        return None, None, "Move field delta is invalid."
    try:
        dx = int(dx_token, 10)
        dy = int(dy_token, 10)
    except ValueError:
        return None, None, "Move field delta is invalid."
    if not (-SNAPSHOT_MOVE_RADIUS <= dx <= SNAPSHOT_MOVE_RADIUS and
            -SNAPSHOT_MOVE_RADIUS <= dy <= SNAPSHOT_MOVE_RADIUS):
        return None, None, "Move field delta must stay within the snapshot map (-20..20)."
    return (dx, dy), terminal, None


def validate_csv_payload(payload: str) -> Tuple[bool, str, List[str]]:
    fields = [field.strip() for field in payload.split("|")]
    if len(fields) > 1 and not fields[0]:
        fields.pop(0)
    if len(fields) < 2:
        return False, "CSV must include at least one action field separated by '|'.", []
    if len(fields) > 4:
        return False, "CSV has too many action fields.", []
    speech = fields[0].strip()
    if not speech:
        return False, "CSV speech field missing.", []
    parsed_actions: List[str] = []
    attack_seen = False
    attack_action = ""
    move_seen = False
    for field in fields[1:]:
        if not field:
            return False, "CSV action token is invalid.", []
        lowered_field = field.lower().strip()
        if lowered_field.startswith(("move=", "move:", "move ")):
            if move_seen:
                return False, "CSV move field repeated.", []
            delta, terminal, error = parse_move_field(lowered_field)
            if error:
                return False, error, []
            assert delta is not None and terminal is not None
            move_seen = True
            parsed_actions.append(lowered_field)
            continue
        for token in lowered_field.split():
            if token.startswith("attack="):
                target_raw = token[7:]
                target_length = 0
                while target_length < len(target_raw):
                    char = target_raw[target_length]
                    if not ("a" <= char <= "z" or "0" <= char <= "9" or char == "_"):
                        break
                    target_length += 1
                if target_length == 0:
                    return False, "CSV attack target is invalid.", []
                if attack_seen:
                    return False, "CSV attack target repeated.", []
                attack_seen = True
                attack_action = "attack=" + target_raw[:target_length]
                continue
            if token not in ALLOWED_ACTIONS:
                return False, "CSV action token is invalid.", []
            parsed_actions.append(token)
    if not parsed_actions and attack_seen:
        parsed_actions.append("idle")
    if parsed_actions == ["wait_here", "hold_position"]:
        parsed_actions.pop()
    if len(parsed_actions) == 0:
        return False, "CSV must include at least one action field.", []
    if len(parsed_actions) > 3:
        return False, "CSV has too many action tokens.", []
    if attack_action:
        parsed_actions.append(attack_action)
    return True, "", parsed_actions


def normalize_csv_separators(payload: str) -> str:
    if "|" in payload:
        return payload
    output: List[str] = []
    last_separator = False
    for index, char in enumerate(payload):
        if char == "+":
            signed_number = (
                index + 1 < len(payload)
                and "0" <= payload[index + 1] <= "9"
                and (index == 0 or payload[index - 1] in ",=" or payload[index - 1].isspace())
            )
            if signed_number:
                output.append(char)
                last_separator = False
                continue
            if not last_separator:
                output.append("|")
                last_separator = True
            continue
        last_separator = False
        output.append(char)
    return "".join(output)


def extract_attack_target_hint(payload: str) -> str:
    lowered = payload.lower()
    start = lowered.find("attack=")
    if start == -1:
        return ""
    start += len("attack=")
    end = start
    while end < len(lowered):
        char = lowered[end]
        if not ("a" <= char <= "z" or "0" <= char <= "9" or char == "_"):
            break
        end += 1
    return lowered[start:end]


def extract_lenient_csv(payload: str) -> Tuple[bool, str, List[str], str]:
    speech = ""
    actions: List[str] = []
    if "|" in payload:
        speech = payload.split("|", 1)[0].strip()
    else:
        first_quote = payload.find('"')
        if first_quote == -1:
            return False, "", [], ""
        pos = first_quote + 1
        collected: List[str] = []
        while pos < len(payload):
            char = payload[pos]
            if char == '"':
                if pos + 1 < len(payload) and payload[pos + 1] == '"':
                    collected.append('"')
                    pos += 2
                    continue
                pos += 1
                break
            collected.append(char)
            pos += 1
        speech = "".join(collected).strip()
    if not speech:
        return False, "", [], ""
    lowered = payload.lower()
    for action in ALLOWED_ACTIONS:
        start = lowered.find(action)
        while start != -1:
            left_ok = start == 0 or (not lowered[start - 1].isalnum() and lowered[start - 1] != '_')
            end = start + len(action)
            right_ok = end >= len(lowered) or (not lowered[end].isalnum() and lowered[end] != '_')
            if left_ok and right_ok:
                actions.append(action)
                return True, speech, actions, "Used lenient CSV parsing."
            start = lowered.find(action, end)
    actions.append("idle")
    return True, speech, actions, "Used lenient CSV parsing."


def strip_speaker_prefix(text: str) -> str:
    trimmed = text.strip()
    colon = trimmed.find(":")
    if 0 <= colon < 40:
        return trimmed[colon + 1:].strip()
    return trimmed


def validate_response_like_game(payload: str) -> Dict[str, object]:
    ok, error, parsed_actions = validate_csv_payload(payload)
    parsed_payload = payload
    if not ok:
        normalized = normalize_csv_separators(payload)
        if normalized != payload:
            ok, error, parsed_actions = validate_csv_payload(normalized)
            if ok:
                parsed_payload = normalized
    if ok:
        fields = [field.strip() for field in parsed_payload.split("|")]
        if len(fields) > 1 and not fields[0]:
            fields.pop(0)
        return {
            "ok": True,
            "mode": "strict",
            "error": "",
            "parsed_speech": strip_speaker_prefix(fields[0]),
            "parsed_actions": parsed_actions,
        }
    lenient_ok, speech, lenient_actions, lenient_error = extract_lenient_csv(payload)
    if lenient_ok:
        attack_target = extract_attack_target_hint(payload)
        if attack_target:
            lenient_actions.append("attack=" + attack_target)
        return {
            "ok": True,
            "mode": "lenient",
            "error": lenient_error,
            "parsed_speech": strip_speaker_prefix(speech),
            "parsed_actions": lenient_actions,
        }
    return {
        "ok": False,
        "mode": "strict",
        "error": error,
        "parsed_speech": "",
        "parsed_actions": [],
    }


def run_self_test() -> int:
    failures: List[str] = []

    def check(condition: bool, label: str) -> None:
        if not condition:
            failures.append(label)

    rendered = render_prompt("{{action_list_with_target}}\n{{snapshot}}", "snapshot")
    check("move=<dx>,<dy> <state>" in rendered, "rendered prompt uses delta move contract")
    check("move: <coordinate>" not in rendered, "rendered prompt omits legacy move contract")

    rendered_map = render_map_with_axes("-----\n--a--\n--|--\n-----\n-----")
    check("        ..|.." in rendered_map, "map renders dx marker line")
    check("dy=+01 --a--" in rendered_map, "map renders positive dy row label")
    check("dy=+00 --|--" in rendered_map, "map renders NPC origin row")

    valid_cases = [
        ("move=4,-2 hold_position", (4, -2), "hold_position"),
        ("MOVE= -20,+20 WAIT_HERE", (-20, 20), "wait_here"),
        ("move=0,0 wait_here", (0, 0), "wait_here"),
    ]
    for field, expected_delta, expected_terminal in valid_cases:
        delta, terminal, error = parse_move_field(field)
        check(error is None and delta == expected_delta and terminal == expected_terminal,
              f"valid move parses: {field}")

    invalid_cases = [
        "move: E E hold_position",
        "move E E hold_position",
        "move=4,-2",
        "move=4,-2 later",
        "move=4, -2 hold_position",
        "move=4,-2 hold_position extra",
        "move=4,-2.5 hold_position",
        "move=4,-2,0 hold_position",
        "move=1_0,0 wait_here",
        "move=21,0 hold_position",
        "move=-21,0 wait_here",
        "move=2147483647,0 wait_here",
    ]
    for field in invalid_cases:
        delta, terminal, error = parse_move_field(field)
        check(delta is None and terminal is None and bool(error), f"invalid move rejects: {field}")

    ok, error, actions = validate_csv_payload(
        "On it|move=4,-2 wait_here|equip_gun"
    )
    check(ok and not error and actions == ["move=4,-2 wait_here", "equip_gun"],
          "strict CSV accepts delta move with another action")

    ok, error, actions = validate_csv_payload(
        " | Inspecting inventory... I'm carrying a sharpened rebar. | equip_melee"
    )
    check(ok and not error and actions == ["equip_melee"],
          "strict CSV tolerates one echoed leading separator")
    leading_separator = validate_response_like_game(
        " | Inspecting inventory... I'm carrying a sharpened rebar. | equip_melee"
    )
    check(
        leading_separator.get("parsed_speech") ==
        "Inspecting inventory... I'm carrying a sharpened rebar.",
        "game-like validation preserves speech after one echoed leading separator",
    )
    strict_prefix = validate_response_like_game(
        "Listener NPC: Holding here. | hold_position"
    )
    check(
        strict_prefix.get("parsed_speech") == "Holding here.",
        "game-like strict validation strips a speaker prefix from visible speech",
    )
    lenient_prefix = validate_response_like_game(
        "Listener NPC: Switching weapons. | equip_melee extra_prose"
    )
    check(
        lenient_prefix.get("mode") == "lenient" and
        lenient_prefix.get("parsed_speech") == "Switching weapons.",
        "game-like lenient validation strips a speaker prefix from visible speech",
    )

    ok, error, actions = validate_csv_payload("||Speech|equip_melee")
    check(not ok and error == "CSV speech field missing." and not actions,
          "strict CSV rejects two leading empty fields")

    ok, error, actions = validate_csv_payload(
        "|Speech|equip_melee|follow_close|panic_off|wait_here"
    )
    check(not ok and error == "CSV has too many action fields." and not actions,
          "strict CSV keeps the four-field limit after a leading separator")

    ok, error, actions = validate_csv_payload("|Speech|equip_melee|")
    check(not ok and error == "CSV action token is invalid." and not actions,
          "strict CSV rejects a trailing empty action after a leading separator")

    ok, _error, _actions = validate_csv_payload(
        "On it|move=1,2 wait_here|move=2,1 hold_position"
    )
    check(not ok, "strict CSV rejects duplicate move fields")

    ok, error, actions = validate_csv_payload(
        "Engaging|attack=a|equip_gun|follow_close panic_off"
    )
    check(ok and not error and actions == ["equip_gun", "follow_close", "panic_off", "attack=a"],
          "attack target does not consume the three normal action slots")

    ok, error, actions = validate_csv_payload("Engaging|attack=a")
    check(ok and not error and actions == ["idle", "attack=a"],
          "attack-only CSV mirrors the game's idle action plus target")

    ok, error, actions = validate_csv_payload("Engaging|attack=a,")
    check(ok and not error and actions == ["idle", "attack=a"],
          "attack target accepts the same ASCII prefix as the game")

    ok, error, actions = validate_csv_payload("Staying|wait_here hold_position")
    check(ok and not error and actions == ["wait_here"],
          "wait-here plus hold-position normalizes like the game")

    normalized = normalize_csv_separators("Moving+move=-20,+20 wait_here")
    check(normalized == "Moving|move=-20,+20 wait_here",
          "plus separator normalization preserves signed coordinates")

    mixed = validate_response_like_game("Speech|bogus follow_close")
    check(mixed.get("mode") == "lenient" and mixed.get("parsed_actions") == ["follow_close"],
          "mixed invalid action falls back like the game")

    mixed_attack = validate_response_like_game("Speech|attack=A bogus follow_close")
    check(mixed_attack.get("mode") == "lenient" and
          mixed_attack.get("parsed_actions") == ["follow_close", "attack=a"],
          "lenient recovery retains the canonical attack handle")

    legacy = validate_response_like_game("On it|move: E E wait_here")
    check(legacy.get("ok") is True and legacy.get("mode") == "lenient" and
          legacy.get("parsed_actions") == ["wait_here"],
          "legacy move is not misreported as movement during lenient recovery")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("npc_harness self-test passed")
    return 0


def build_runner_command(args: argparse.Namespace) -> List[str]:
    cmd = [args.python_path or sys.executable, args.runner_path]
    cmd.extend(["--backend", args.backend])
    cmd.extend(["--max-tokens", str(args.max_tokens)])
    cmd.extend(["--max-prompt-len", str(args.max_prompt_len)])
    if args.log_file:
        cmd.extend(["--log-file", args.log_file])
    if args.backend == "openvino":
        if not args.model_dir:
            raise SystemExit("--model-dir is required for --backend openvino")
        cmd.extend(["--model-dir", args.model_dir, "--device", args.device])
    elif args.backend == "ollama":
        if not args.ollama_model:
            raise SystemExit("--ollama-model is required for --backend ollama")
        cmd.extend(["--ollama-url", args.ollama_url, "--ollama-model", args.ollama_model])
    else:
        if not args.api_provider or not args.api_model:
            raise SystemExit("--api-provider and --api-model are required for --backend api")
        cmd.extend(["--api-provider", args.api_provider, "--api-model", args.api_model])
        if args.api_key_env:
            cmd.extend(["--api-key-env", args.api_key_env])
    return cmd


def run_runner_request(args: argparse.Namespace, prompt: str, snapshot: str, request_id: str) -> Dict[str, object]:
    cmd = build_runner_command(args)
    payload = {
        "request_id": request_id,
        "prompt": prompt,
        "snapshot": snapshot,
        "max_tokens": args.max_tokens,
        "temperature": args.temperature,
        "top_p": args.top_p,
        "repetition_penalty": args.repetition_penalty,
    }
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        cwd=str(repo_root()),
    )
    assert proc.stdin is not None and proc.stdout is not None and proc.stderr is not None
    try:
        proc.stdin.write(json.dumps(payload, ensure_ascii=True) + "\n")
        proc.stdin.flush()
        line = proc.stdout.readline().strip()
        if not line:
            stderr = proc.stderr.read()
            raise RuntimeError(f"runner produced no response. stderr={stderr.strip()}")
        response = json.loads(line)
        proc.stdin.write(json.dumps({"command": "shutdown", "request_id": "shutdown"}) + "\n")
        proc.stdin.flush()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        return response
    finally:
        if proc.poll() is None:
            proc.kill()


def load_scenario(path: Optional[str]) -> Dict[str, object]:
    if not path:
        return json.loads(json.dumps(DEFAULT_SCENARIO))
    scenario_path = Path(path)
    data = json.loads(read_text(scenario_path))
    if not isinstance(data, dict):
        raise SystemExit(f"Scenario must be a JSON object: {path}")
    return data


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Named-NPC smoke harness for AOL's LLM runner.")
    parser.add_argument("--scenario", default=str(root / "tools" / "llm_runner" / "scenarios" / "rubik_trade.json"), help="Scenario JSON file.")
    parser.add_argument("--summary-root", action="append", default=[], help="Additional summary root(s) matching data/json style layout.")
    parser.add_argument("--python-path", default=sys.executable, help="Python executable for runner.py.")
    parser.add_argument("--runner-path", default=str(root / "tools" / "llm_runner" / "runner.py"), help="Path to runner.py.")
    parser.add_argument("--backend", choices=["openvino", "ollama", "api"], default="ollama")
    parser.add_argument("--model-dir", default="")
    parser.add_argument("--device", default="AUTO")
    parser.add_argument("--ollama-url", default="http://127.0.0.1:11434")
    parser.add_argument("--ollama-model", default="")
    parser.add_argument("--api-provider", default="")
    parser.add_argument("--api-model", default="")
    parser.add_argument("--api-key-env", default="")
    parser.add_argument("--max-tokens", type=int, default=DEFAULT_MAX_TOKENS)
    parser.add_argument("--max-prompt-len", type=int, default=DEFAULT_MAX_PROMPT_LEN)
    parser.add_argument("--temperature", type=float, default=0.6)
    parser.add_argument("--top-p", type=float, default=0.9)
    parser.add_argument("--repetition-penalty", type=float, default=1.0)
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--log-file", default="")
    parser.add_argument("--resolve-only", action="store_true", help="Only resolve and validate summary selection; do not invoke runner.py.")
    parser.add_argument("--dump-prompt", action="store_true", help="Print the rendered prompt and exit.")
    parser.add_argument("--self-test", action="store_true", help="Run parser/prompt parity checks without invoking a model backend.")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON result.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        return run_self_test()
    scenario = load_scenario(args.scenario)
    summary_roots = default_summary_roots() + [Path(path).resolve() for path in args.summary_root]
    resolved = resolve_summary(scenario, summary_roots)

    result: Dict[str, object] = {
        "scenario": args.scenario,
        "npc_name": scenario.get("npc_name", ""),
        "profession": scenario.get("profession", ""),
        "mutations": scenario.get("mutations", []),
        "attempted_selectors": resolved.attempted_selectors,
        "selected_selector": resolved.selected_selector,
        "resolution_kind": resolved.resolution_kind,
        "matched_trait": resolved.matched_trait,
        "matched_topic": resolved.matched_topic,
        "summary": None,
        "snapshot_fields": None,
        "expectations_ok": True,
        "response": None,
        "action_line_validation": None,
    }

    if resolved.entry:
        result["summary"] = {
            "background": resolved.entry.background,
            "expression": resolved.entry.expression,
            "source_tag": resolved.entry.source_tag,
            "file_path": resolved.entry.file_path,
        }

    expectations = scenario.get("expectations", {}) or {}
    if isinstance(expectations, dict):
        selector_equals = str(expectations.get("selector_equals", "")).strip()
        if selector_equals and resolved.selected_selector != selector_equals:
            result["expectations_ok"] = False
            result["expectation_error"] = f"selected selector mismatch: expected {selector_equals}, got {resolved.selected_selector}"
        source_contains = str(expectations.get("source_tag_contains", "")).strip()
        if source_contains:
            actual_tag = resolved.entry.source_tag if resolved.entry else ""
            if source_contains not in actual_tag:
                result["expectations_ok"] = False
                result["expectation_error"] = f"source tag mismatch: expected substring {source_contains}, got {actual_tag}"

    if not resolved.entry:
        result["expectations_ok"] = False
        result["expectation_error"] = "no matching summary entry found"
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print("No matching summary entry found.")
            print("Attempted selectors:")
            for selector in resolved.attempted_selectors:
                print(f"- {selector}")
        return 1

    request_id = f"harness_{int(time.time())}"
    snapshot = build_snapshot(scenario, resolved, request_id)
    result["snapshot_fields"] = {
        "your_profession": str(scenario.get("profession", "no_past")).strip() or "no_past",
        "your_tone": resolved.entry.background if resolved.entry else "",
        "your_example_expression": resolved.entry.expression if resolved.entry else "",
        "your_background_traits": [str(item).strip() for item in (scenario.get("mutations", []) or []) if str(item).strip()],
    }
    prompt_template = load_prompt_template(
        NPC_ACTION_PROMPT_FILENAME,
        default_prompt_template(),
        ["{{snapshot}}", "{{action_list_with_target}}"],
    )
    prompt = render_prompt(prompt_template, snapshot)

    if args.dump_prompt:
        print(prompt)
        return 0

    if args.resolve_only:
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"Resolved selector: {resolved.selected_selector}")
            print(f"Source tag: {resolved.entry.source_tag}")
            print(f"Background: {resolved.entry.background}")
            print(f"Expression: {resolved.entry.expression}")
            print(f"File: {resolved.entry.file_path}")
        return 0 if result.get("expectations_ok") else 1

    response = run_runner_request(args, prompt, snapshot, request_id)
    result["response"] = response
    csv_text = str(response.get("text", "")) if isinstance(response, dict) else ""
    validation = validate_response_like_game(csv_text)
    result["action_line_validation"] = validation
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(f"Resolved selector: {resolved.selected_selector}")
        print(f"Source tag: {resolved.entry.source_tag}")
        print(f"Runner ok: {response.get('ok', False)}")
        print(f"Runner text: {csv_text}")
        print(f"Action line valid: {validation.get('ok', False)} ({validation.get('mode', 'strict')})")
        parsed_actions = validation.get("parsed_actions", []) or []
        if parsed_actions:
            print(f"Parsed actions: {', '.join(parsed_actions)}")
        if validation.get("error"):
            print(f"Action line note: {validation.get('error')}")
    return 0 if result.get("expectations_ok") and bool(validation.get("ok", False)) and bool(response.get("ok", False)) else 1


if __name__ == "__main__":
    raise SystemExit(main())
