#!/usr/bin/env python3
"""Check structured LLM action-status lines in llm_intent.log.

This is a small deterministic checker for the action_status lines emitted by
src/npc.cpp / src/npcmove.cpp / src/llm_intent.cpp.

Typical use:
  python3 tools/llm_runner/action_status_check.py \
    --log-file config/llm_intent.log \
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

FIELD_RE = re.compile(r'(\w+)="([^"]*)"')


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
    "reason_any",
    "terminal_phase",
    "terminal_reason",
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def parse_action_status_line(line: str) -> Optional[ActionStatusEvent]:
    if "action_status" not in line:
        return None
    fields: Dict[str, str] = {key: value for key, value in FIELD_RE.findall(line)}
    if not fields:
        return None
    kind = fields.get("kind", "").strip()
    phase = fields.get("phase", "").strip()
    npc = fields.get("npc", "").strip()
    if not kind or not phase or not npc:
        return None
    return ActionStatusEvent(
        raw_line=line.rstrip("\n"),
        npc=npc,
        kind=kind,
        phase=phase,
        reason=fields.get("reason", "").strip(),
        request=fields.get("request", "").strip(),
        target_hint=fields.get("target_hint", "").strip(),
        target=fields.get("target", "").strip(),
        facts=fields.get("facts", "").strip(),
    )


def load_action_status_events(path: Path) -> List[ActionStatusEvent]:
    events: List[ActionStatusEvent] = []
    for line in read_text(path).splitlines():
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
    if args.reason_any:
        expectations["reason_any"] = list(args.reason_any)
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
    parser = argparse.ArgumentParser(description="Check structured action_status lines in llm_intent.log.")
    parser.add_argument("--log-file", required=True, help="Path to llm_intent.log or another captured status log.")
    parser.add_argument("--expect-file", default="", help="Optional JSON expectation file.")
    parser.add_argument("--npc", default="", help="Exact NPC name to match.")
    parser.add_argument("--kind", default="", help="Exact action kind to match.")
    parser.add_argument("--request", default="", help="Exact request id to match.")
    parser.add_argument("--target", default="", help="Substring that must appear in target.")
    parser.add_argument("--target-hint", default="", help="Substring that must appear in target_hint.")
    parser.add_argument("--min-events", type=int, default=None, help="Require at least this many matched events.")
    parser.add_argument("--phase-any", action="append", default=[], help="Require that at least one matched event has this phase. Repeatable.")
    parser.add_argument("--reason-any", action="append", default=[], help="Require that at least one matched event has this reason. Repeatable.")
    parser.add_argument("--terminal-phase", default="", help="Require this terminal phase on the last matched event.")
    parser.add_argument("--terminal-reason", default="", help="Require this terminal reason on the last matched event.")
    parser.add_argument("--json", action="store_true", help="Print JSON output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    log_path = Path(args.log_file)
    expectations = normalize_expectations(args)
    all_events = load_action_status_events(log_path)
    matched_events = filter_events(all_events, expectations)
    result = {
        "log_file": str(log_path),
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
