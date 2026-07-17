#!/usr/bin/env python3
"""Check same-run structured LLM action-status events.

This is a small deterministic checker for the action_status lines emitted by
src/npc.cpp / src/npcmove.cpp / src/llm_intent.cpp.

Typical use:
  python3 tools/llm_runner/action_status_check.py \
    --log-file config/llm_intent_events.log \
    --after-byte-offset 1234 \
    --npc "Rubik" \
    --kind look_around_pickup \
    --terminal-phase blocked \
    --terminal-reason pickup.no_path
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional

EVENT_LOG_NAME = "llm_intent_events.log"
JSON_STRING_PATTERN = r'"(?:\\.|[^"\\])*"'
ACTION_STATUS_RE = re.compile(
    rf'^\[CAOL_EVENT\] action_status '
    rf'npc=(?P<npc>{JSON_STRING_PATTERN}) '
    rf'kind=(?P<kind>{JSON_STRING_PATTERN}) '
    rf'phase=(?P<phase>{JSON_STRING_PATTERN}) '
    rf'reason=(?P<reason>{JSON_STRING_PATTERN}) '
    rf'request=(?P<request>{JSON_STRING_PATTERN}) '
    rf'target_hint=(?P<target_hint>{JSON_STRING_PATTERN}) '
    rf'target=(?P<target>{JSON_STRING_PATTERN}) '
    rf'facts=(?P<facts>{JSON_STRING_PATTERN})$'
)


@dataclass
class ActionStatusEvent:
    raw_line: str
    npc: str
    kind: str
    phase: str
    reason: str
    request: str
    target_hint: str
    target: str
    facts: str


EXPECTED_KEYS = {
    "npc",
    "kind",
    "request",
    "target",
    "target_hint",
    "min_events",
    "phase_any",
    "phase_sequence",
    "reason_any",
    "terminal_phase",
    "terminal_reason",
}


def read_text(path: Path, after_byte_offset: int = 0) -> str:
    size = path.stat().st_size
    if after_byte_offset < 0 or after_byte_offset > size:
        raise ValueError(
            f"byte offset {after_byte_offset} is outside current log size {size}; "
            "the event log may have rotated or been truncated"
        )
    with path.open("rb") as stream:
        stream.seek(after_byte_offset)
        return stream.read().decode("utf-8", errors="replace")


def parse_action_status_line(line: str) -> Optional[ActionStatusEvent]:
    physical_line = line.rstrip("\r\n")
    match = ACTION_STATUS_RE.fullmatch(physical_line)
    if match is None:
        return None
    try:
        fields: Dict[str, str] = {
            key: json.loads(value)
            for key, value in match.groupdict().items()
        }
    except json.JSONDecodeError:
        return None
    if not fields:
        return None
    kind = fields.get("kind", "").strip()
    phase = fields.get("phase", "").strip()
    npc = fields.get("npc", "").strip()
    if not kind or not phase or not npc:
        return None
    return ActionStatusEvent(
        raw_line=physical_line,
        npc=npc,
        kind=kind,
        phase=phase,
        reason=fields.get("reason", "").strip(),
        request=fields.get("request", "").strip(),
        target_hint=fields.get("target_hint", "").strip(),
        target=fields.get("target", "").strip(),
        facts=fields.get("facts", "").strip(),
    )


def load_action_status_events(path: Path, after_byte_offset: int = 0) -> List[ActionStatusEvent]:
    events: List[ActionStatusEvent] = []
    for line in read_text(path, after_byte_offset).splitlines():
        event = parse_action_status_line(line)
        if event is not None:
            events.append(event)
    return events


def filter_events(events: List[ActionStatusEvent], expectations: Dict[str, object]) -> List[ActionStatusEvent]:
    filtered = list(events)
    npc = str(expectations.get("npc", "")).strip()
    kind = str(expectations.get("kind", "")).strip()
    request = str(expectations.get("request", "")).strip()
    target = str(expectations.get("target", "")).strip()
    target_hint = str(expectations.get("target_hint", "")).strip()
    if npc:
        filtered = [event for event in filtered if event.npc == npc]
    if kind:
        filtered = [event for event in filtered if event.kind == kind]
    if request:
        filtered = [event for event in filtered if event.request == request]
    if target:
        filtered = [event for event in filtered if target in event.target]
    if target_hint:
        filtered = [event for event in filtered if target_hint in event.target_hint]
    return filtered


def normalize_expectations(args: argparse.Namespace) -> Dict[str, object]:
    expectations: Dict[str, object] = {}
    if args.expect_file:
        loaded = json.loads(read_text(Path(args.expect_file)))
        if not isinstance(loaded, dict):
            raise SystemExit("Expectation file must contain a JSON object.")
        unknown = sorted(set(loaded.keys()) - EXPECTED_KEYS)
        if unknown:
            raise SystemExit(f"Unknown expectation key(s): {', '.join(unknown)}")
        expectations.update(loaded)
    for key in ("npc", "kind", "request", "target", "target_hint", "terminal_phase", "terminal_reason"):
        value = getattr(args, key)
        if value:
            expectations[key] = value
    if args.min_events is not None:
        expectations["min_events"] = args.min_events
    if args.phase_any:
        expectations["phase_any"] = list(args.phase_any)
    if args.phase_sequence:
        expectations["phase_sequence"] = list(args.phase_sequence)
    if args.reason_any:
        expectations["reason_any"] = list(args.reason_any)
    for key in ("npc", "kind", "request", "target", "target_hint", "terminal_phase", "terminal_reason"):
        if key not in expectations:
            continue
        value = expectations[key]
        if not isinstance(value, str) or not value.strip():
            raise SystemExit(f"{key} must be a non-empty string.")
    for key in ("phase_any", "phase_sequence", "reason_any"):
        if key not in expectations:
            continue
        value = expectations[key]
        if not isinstance(value, list) or any(
            not isinstance(entry, str) or not entry.strip() for entry in value
        ):
            raise SystemExit(f"{key} must be a list of non-empty strings.")
    expectations.setdefault("min_events", 1)
    min_events = expectations["min_events"]
    if isinstance(min_events, bool) or not isinstance(min_events, int) or min_events < 1:
        raise SystemExit("min_events must be an integer greater than or equal to 1.")
    return expectations


def evaluate_events(events: List[ActionStatusEvent], expectations: Dict[str, object]) -> Dict[str, object]:
    result: Dict[str, object] = {
        "ok": True,
        "errors": [],
        "matched_events": [asdict(event) for event in events],
        "matched_count": len(events),
        "terminal_event": asdict(events[-1]) if events else None,
    }

    min_events = expectations.get("min_events")
    if isinstance(min_events, int) and len(events) < min_events:
        result["ok"] = False
        result["errors"].append(f"expected at least {min_events} matched events, got {len(events)}")

    phase_any = expectations.get("phase_any")
    if isinstance(phase_any, list):
        phases = {event.phase for event in events}
        for wanted in phase_any:
            if str(wanted) not in phases:
                result["ok"] = False
                result["errors"].append(f"missing expected phase: {wanted}")

    phase_sequence = expectations.get("phase_sequence")
    if isinstance(phase_sequence, list) and phase_sequence:
        actual_phases = [event.phase for event in events]
        cursor = 0
        for wanted in phase_sequence:
            wanted_str = str(wanted)
            while cursor < len(actual_phases) and actual_phases[cursor] != wanted_str:
                cursor += 1
            if cursor >= len(actual_phases):
                result["ok"] = False
                result["errors"].append(
                    f"missing expected phase sequence element: {wanted_str} in order {phase_sequence}"
                )
                break
            cursor += 1

    reason_any = expectations.get("reason_any")
    if isinstance(reason_any, list):
        reasons = {event.reason for event in events if event.reason}
        for wanted in reason_any:
            if str(wanted) not in reasons:
                result["ok"] = False
                result["errors"].append(f"missing expected reason: {wanted}")

    terminal_phase = str(expectations.get("terminal_phase", "")).strip()
    if terminal_phase:
        actual = events[-1].phase if events else ""
        if actual != terminal_phase:
            result["ok"] = False
            result["errors"].append(f"terminal phase mismatch: expected {terminal_phase}, got {actual or '(none)'}")

    terminal_reason = str(expectations.get("terminal_reason", "")).strip()
    if terminal_reason:
        actual = events[-1].reason if events else ""
        if actual != terminal_reason:
            result["ok"] = False
            result["errors"].append(f"terminal reason mismatch: expected {terminal_reason}, got {actual or '(none)'}")

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check framed action_status lines in llm_intent_events.log.")
    parser.add_argument("--log-file", required=True, help="Path to the dedicated llm_intent_events.log.")
    parser.add_argument(
        "--after-byte-offset",
        type=int,
        default=None,
        help="Read only bytes appended after this pre-run event-log size.",
    )
    parser.add_argument(
        "--print-byte-offset",
        action="store_true",
        help="Print the current event-log size for a later same-run check and exit.",
    )
    parser.add_argument(
        "--fixture-mode",
        action="store_true",
        help="Allow a non-event-log fixture path; deterministic tests only.",
    )
    parser.add_argument("--expect-file", default="", help="Optional JSON expectation file.")
    parser.add_argument("--npc", default="", help="Exact NPC name to match.")
    parser.add_argument("--kind", default="", help="Exact action kind to match.")
    parser.add_argument("--request", default="", help="Exact request id to match.")
    parser.add_argument("--target", default="", help="Substring that must appear in target.")
    parser.add_argument("--target-hint", default="", help="Substring that must appear in target_hint.")
    parser.add_argument("--min-events", type=int, default=None, help="Require at least this many matched events.")
    parser.add_argument("--phase-any", action="append", default=[], help="Require that at least one matched event has this phase. Repeatable.")
    parser.add_argument("--phase-sequence", action="append", default=[], help="Require these phases to appear in order across matched events. Repeatable.")
    parser.add_argument("--reason-any", action="append", default=[], help="Require that at least one matched event has this reason. Repeatable.")
    parser.add_argument("--terminal-phase", default="", help="Require this terminal phase on the last matched event.")
    parser.add_argument("--terminal-reason", default="", help="Require this terminal reason on the last matched event.")
    parser.add_argument("--json", action="store_true", help="Print JSON output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    log_path = Path(args.log_file)
    if args.print_byte_offset:
        print(log_path.stat().st_size if log_path.exists() else 0)
        return 0
    if not args.fixture_mode and log_path.name != EVENT_LOG_NAME:
        print(f"Refusing untrusted log source; expected {EVENT_LOG_NAME}.", file=sys.stderr)
        return 2
    if args.after_byte_offset is None:
        print("--after-byte-offset is required for same-run evidence.", file=sys.stderr)
        return 2
    expectations = normalize_expectations(args)
    try:
        all_events = load_action_status_events(log_path, args.after_byte_offset)
    except (OSError, ValueError) as exc:
        print(f"Unable to read event log: {exc}", file=sys.stderr)
        return 2
    matched_events = filter_events(all_events, expectations)
    result = {
        "log_file": str(log_path),
        "after_byte_offset": args.after_byte_offset,
        "expectations": expectations,
        "total_action_status_events": len(all_events),
    }
    result.update(evaluate_events(matched_events, expectations))

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(f"Log file: {log_path}")
        print(f"Total action_status events: {len(all_events)}")
        print(f"Matched events: {result['matched_count']}")
        terminal = result.get("terminal_event")
        if terminal:
            print(
                "Terminal event: "
                f"kind={terminal['kind']} phase={terminal['phase']} reason={terminal['reason'] or '(none)'} "
                f"target={terminal['target'] or terminal['target_hint'] or '(none)'}"
            )
        if result["errors"]:
            print("Errors:")
            for error in result["errors"]:
                print(f"- {error}")
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
