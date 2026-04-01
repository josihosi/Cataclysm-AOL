#!/usr/bin/env python3
"""Deterministic named-NPC smoke harness for AOL's LLM runner.

What this does:
- mirrors summary selection precedence from src/llm_intent.cpp
- builds a small but game-shaped snapshot for a named NPC
- renders the normal npc_action_prompt template
- sends one request through tools/llm_runner/runner.py over stdin/stdout JSON
- validates the returned CSV-like action payload with the same broad rules as the game

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
VALID_MOVE_COORDS = {"n", "s", "e", "w", "ne", "nw", "se", "sw"}
VALID_MOVE_TERMINAL_STATES = {"wait_here", "hold_position"}


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
    action_list = ", ".join(ALLOWED_ACTIONS) + ", attack=<target>, move: <coordinate> <coordinate> ... <state>"
    return template.replace("{{snapshot}}", snapshot).replace("{{action_list_with_target}}", action_list)


def parse_summary_file(path: Path) -> Dict[str, SummaryEntry]:
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


def summary_file_priority(path: Path) -> Tuple[int, str]:
    name = path.name
    return (0 if name.startswith("generated_") else 1, name)


def load_summary_dir(summary_dir: Path) -> Dict[str, SummaryEntry]:
    resolved: Dict[str, SummaryEntry] = {}
    if not summary_dir.exists():
        return resolved
    files = sorted([path for path in summary_dir.iterdir() if path.is_file() and path.suffix == ".txt"], key=summary_file_priority)
    for path in files:
        resolved.update(parse_summary_file(path))
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
        "| ... You (NPC)",
        "map_legend:",
        "a ... player",
        "map:",
        str(world.get("map", "-----\n--a--\n--|--\n-----\n-----")),
    ])
    return "\n".join(lines) + "\n"


def parse_move_field(field: str) -> Tuple[Optional[List[str]], Optional[str], Optional[str]]:
    raw = field.strip()
    lowered = raw.lower()
    if lowered.startswith("move:"):
        lowered = lowered[5:].strip()
    elif lowered.startswith("move "):
        lowered = lowered[5:].strip()
    else:
        return None, None, "move token missing prefix"
    parts = [part for part in lowered.split() if part]
    if len(parts) < 2:
        return None, None, "move field must include coordinates and terminal state"
    terminal = parts[-1]
    if terminal not in VALID_MOVE_TERMINAL_STATES:
        return None, None, "move field terminal state is invalid"
    coords = parts[:-1]
    if not coords or len(coords) > 15:
        return None, None, "move field must have 1-15 coordinates"
    if any(coord not in VALID_MOVE_COORDS for coord in coords):
        return None, None, "move coordinate is invalid"
    return coords, terminal, None


def validate_csv_payload(payload: str) -> Tuple[bool, str, List[str]]:
    fields = [field.strip() for field in payload.split("|")]
    if len(fields) < 2:
        return False, "CSV must include at least one action field separated by '|'.", []
    if len(fields) > 4:
        return False, "CSV has too many action fields.", []
    speech = fields[0].strip()
    if not speech:
        return False, "CSV speech field missing.", []
    parsed_actions: List[str] = []
    attack_seen = False
    for field in fields[1:]:
        if not field:
            return False, "CSV action token is invalid.", []
        lowered_field = field.lower().strip()
        if lowered_field.startswith("move:") or lowered_field.startswith("move "):
            coords, terminal, error = parse_move_field(lowered_field)
            if error:
                return False, error, []
            parsed_actions.append(f"move: {' '.join(coords)} {terminal}")
            continue
        for token in lowered_field.split():
            if token.startswith("attack="):
                target = token[7:]
                if not target or not all(ch.isalnum() or ch == '_' for ch in target):
                    return False, "CSV attack target is invalid.", []
                if attack_seen:
                    return False, "CSV attack target repeated.", []
                attack_seen = True
                parsed_actions.append(token)
                continue
            if token not in ALLOWED_ACTIONS:
                return False, "CSV action token is invalid.", []
            parsed_actions.append(token)
    if len(parsed_actions) == 0:
        return False, "CSV must include at least one action field.", []
    if len(parsed_actions) > 3:
        return False, "CSV has too many action tokens.", []
    return True, "", parsed_actions


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


def validate_response_like_game(payload: str) -> Dict[str, object]:
    ok, error, parsed_actions = validate_csv_payload(payload)
    if ok:
        return {
            "ok": True,
            "mode": "strict",
            "error": "",
            "parsed_actions": parsed_actions,
        }
    lenient_ok, _speech, lenient_actions, lenient_error = extract_lenient_csv(payload)
    if lenient_ok:
        return {
            "ok": True,
            "mode": "lenient",
            "error": lenient_error,
            "parsed_actions": lenient_actions,
        }
    return {
        "ok": False,
        "mode": "strict",
        "error": error,
        "parsed_actions": [],
    }


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
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON result.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
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
        "csv_validation": None,
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
    result["csv_validation"] = validation
    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(f"Resolved selector: {resolved.selected_selector}")
        print(f"Source tag: {resolved.entry.source_tag}")
        print(f"Runner ok: {response.get('ok', False)}")
        print(f"Runner text: {csv_text}")
        print(f"CSV valid: {validation.get('ok', False)} ({validation.get('mode', 'strict')})")
        parsed_actions = validation.get("parsed_actions", []) or []
        if parsed_actions:
            print(f"Parsed actions: {', '.join(parsed_actions)}")
        if validation.get("error"):
            print(f"CSV note: {validation.get('error')}")
    return 0 if result.get("expectations_ok") and bool(validation.get("ok", False)) and bool(response.get("ok", False)) else 1


if __name__ == "__main__":
    raise SystemExit(main())
