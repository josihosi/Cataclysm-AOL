"""Bounded run-only state decisions for improved harness routes.

The game owns the transition stream.  This module only consumes that stream
inside the authorized run directory and never uses screenshots or OCR to
decide a verdict.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping, Optional, Sequence


MAX_EVENT_BYTES = 256 * 1024
MAX_EVENTS = 64
SEMANTIC_STEP_PREFIX = "openclaw_harness_semantic_step: "
TRAVEL_PHASES = frozenset({"outbound", "returning_home", "returning_report", "returning_exposed"})
ARRIVAL_PHASES = frozenset({"returning_home", "returning_report", "home", "arrived"})


def _owned_path(path: Path, run_dir: Path) -> bool:
    try:
        Path(path).resolve().relative_to(Path(run_dir).resolve())
        return True
    except ValueError:
        return False


def read_bounded_transition_facts(path: Path, run_dir: Path, run_id: str) -> tuple[list[dict[str, Any]], str]:
    """Read at most the bounded native stream and reject unsafe sources."""
    path = Path(path)
    if not _owned_path(path, Path(run_dir)):
        return [], "escaped_authority"
    try:
        size = path.stat().st_size
        if size > MAX_EVENT_BYTES:
            return [], "unbounded_output"
        body = path.read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        return [], "missing_or_unreadable"
    events: list[dict[str, Any]] = []
    for line in body.splitlines():
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            return [], "malformed_event"
        if not isinstance(value, Mapping):
            return [], "malformed_event"
        if str(value.get("run_id", "")) != str(run_id):
            return [], "contamination"
        events.append(dict(value))
        if len(events) > MAX_EVENTS:
            return [], "unbounded_output"
    return events, "ok"


def read_semantic_step_trace(
    path: Path, run_dir: Path, run_id: str, *, start_offset: int = 0,
) -> tuple[list[dict[str, Any]], str]:
    """Read run-owned semantic frames and native action receipts from a debug trace."""
    path = Path(path)
    if not _owned_path(path, Path(run_dir)):
        return [], "escaped_authority"
    try:
        size = path.stat().st_size
        if start_offset < 0 or start_offset > size:
            return [], "stale_trace_offset"
        if size - start_offset > MAX_EVENT_BYTES:
            return [], "unbounded_output"
        with path.open("rb") as handle:
            handle.seek(start_offset)
            body = handle.read(MAX_EVENT_BYTES).decode("utf-8")
    except (OSError, UnicodeError):
        return [], "missing_or_unreadable"
    events: list[dict[str, Any]] = []
    byte_cursor = 0
    for raw_line in body.splitlines(keepends=True):
        line = raw_line.rstrip("\r\n")
        marker = line.find(SEMANTIC_STEP_PREFIX)
        if marker < 0:
            byte_cursor += len(raw_line.encode("utf-8"))
            continue
        try:
            value = json.loads(line[marker + len(SEMANTIC_STEP_PREFIX):])
        except json.JSONDecodeError:
            return [], "malformed_semantic_step"
        if not isinstance(value, Mapping):
            return [], "malformed_semantic_step"
        if str(value.get("run_id", "")) != str(run_id):
            return [], "contamination"
        event = str(value.get("event", ""))
        frame_id = str(value.get("frame_id", "")).strip()
        if event not in {"frame", "receipt", "travel"}:
            return [], "malformed_semantic_step"
        normalized = dict(value)
        normalized["_event_offset"] = start_offset + byte_cursor + len(
            line[:marker].encode("utf-8")
        )
        if event == "frame":
            if not frame_id:
                return [], "malformed_semantic_step"
            actions = normalized.get("valid_actions")
            bindings = normalized.get("action_inputs")
            if not isinstance(actions, list) or any(not isinstance(item, str) or not item for item in actions):
                return [], "malformed_semantic_step"
            if not isinstance(bindings, Mapping) or set(bindings) != set(actions) or any(
                    not isinstance(item, str) or not item for item in bindings.values()):
                return [], "malformed_semantic_step"
        elif event == "receipt":
            if not frame_id:
                return [], "malformed_semantic_step"
            if not isinstance(normalized.get("accepted"), bool):
                return [], "malformed_semantic_step"
        else:
            destination = normalized.get("destination")
            if str(normalized.get("travel_id", "")).strip() == "" or \
                    str(normalized.get("receipt_id", "")).strip() == "" or \
                    str(normalized.get("state", "")) not in {
                        "active", "progress", "completed_cleared", "blocked", "interrupted",
                    } or not isinstance(destination, list) or len(destination) != 3 or \
                    any(isinstance(value, bool) or not isinstance(value, int) for value in destination) or \
                    not isinstance(normalized.get("destination_present"), bool) or \
                    not isinstance(normalized.get("destination_cleared"), bool):
                return [], "malformed_semantic_travel"
        events.append(normalized)
        if len(events) > MAX_EVENTS:
            return [], "unbounded_output"
        byte_cursor += len(raw_line.encode("utf-8"))
    return events, "ok"


def decide_native_travel_boundary(
    events: Sequence[Mapping[str, Any]], *, run_id: str,
    expected_destination: Sequence[int],
) -> dict[str, Any]:
    """Prove one native travel route completed and cleared its destination."""
    expected = list(expected_destination)
    if len(expected) != 3 or any(isinstance(value, bool) or not isinstance(value, int)
                                 for value in expected):
        return {"status": "blocked", "reason": "invalid_expected_destination"}
    active: Optional[dict[str, Any]] = None
    progress: Optional[dict[str, Any]] = None
    for event in events:
        if str(event.get("event", "")) != "travel":
            continue
        if str(event.get("run_id", "")) != str(run_id):
            return {"status": "blocked", "reason": "wrong_run"}
        if list(event.get("destination", [])) != expected:
            continue
        state = str(event.get("state", ""))
        if state == "active":
            if event.get("destination_present") is not True or event.get("destination_cleared") is not False:
                return {"status": "blocked", "reason": "active_travel_fact_invalid"}
            active = dict(event)
            continue
        if active is None or str(event.get("travel_id", "")) != str(active.get("travel_id", "")):
            continue
        if state == "progress":
            if event.get("destination_present") is not True or event.get("destination_cleared") is not False:
                return {"status": "blocked", "reason": "progress_travel_fact_invalid",
                        "travel_id": active["travel_id"]}
            progress = dict(event)
            continue
        if state in {"blocked", "interrupted"}:
            return {"status": "blocked", "reason": state, "travel_id": active["travel_id"]}
        if state == "completed_cleared":
            if event.get("destination_present") is not False or event.get("destination_cleared") is not True:
                return {"status": "blocked", "reason": "completed_destination_not_cleared"}
            if event.get("receipt_id") == active.get("receipt_id"):
                return {"status": "blocked", "reason": "terminal_receipt_not_distinct"}
            return {
                "status": "green", "reason": "native_travel_completed_and_destination_cleared",
                "travel_id": active["travel_id"],
                "active_receipt_id": active["receipt_id"],
                "completion_receipt_id": event["receipt_id"],
                "destination": expected,
            }
    if active is not None:
        return {
            "status": "blocked",
            "reason": "active_travel_progress_without_terminal" if progress else "active_travel_no_progress",
            "travel_id": active["travel_id"],
            "last_progress_receipt_id": progress.get("receipt_id") if progress else None,
        }
    return {"status": "blocked", "reason": "travel_not_observed"}


def latest_semantic_step_frame(events: Sequence[Mapping[str, Any]]) -> Optional[dict[str, Any]]:
    """Return the latest native frame with its matching receipt, if any."""
    latest: Optional[dict[str, Any]] = None
    receipts: dict[str, dict[str, Any]] = {}
    for event in events:
        if str(event.get("event", "")) == "frame":
            latest = dict(event)
        elif str(event.get("event", "")) == "receipt":
            receipts[str(event.get("frame_id", ""))] = dict(event)
    if latest is not None:
        latest["native_receipt"] = receipts.get(str(latest["frame_id"]))
    return latest


def latest_semantic_game_minutes(events: Sequence[Mapping[str, Any]]) -> Optional[int]:
    """Return the newest native frame's calendar marker without deriving it."""
    for event in reversed(events):
        if str(event.get("event", "")) != "frame":
            continue
        game_minutes = event.get("game_minutes")
        if isinstance(game_minutes, int) and not isinstance(game_minutes, bool) and game_minutes >= 0:
            return game_minutes
    return None


def decide_run_state(
    *,
    transition_path: Path,
    run_dir: Path,
    run_id: str,
    expected_destination: Optional[str] = None,
    activity_id: Optional[str] = None,
    previous_progress: Optional[int] = None,
) -> dict[str, Any]:
    """Return a machine decision from native facts only.

    OCR and visual fields are deliberately ignored, even when they contradict
    the structured facts.
    """
    events, read_status = read_bounded_transition_facts(transition_path, run_dir, run_id)
    if read_status != "ok":
        return {"status": "blocked", "reason": read_status, "provenance": "native_transition_stream"}
    return decide_event_facts(events, run_id=run_id, expected_destination=expected_destination,
                             activity_id=activity_id, previous_progress=previous_progress)


def decide_event_facts(
    events: Sequence[Mapping[str, Any]], *, run_id: str,
    expected_destination: Optional[str] = None,
    activity_id: Optional[str] = None,
    previous_progress: Optional[int] = None,
) -> dict[str, Any]:
    """Evaluate already bounded events at a scenario checkpoint."""
    if not isinstance(events, Sequence) or any(not isinstance(event, Mapping) for event in events):
        return {"status": "blocked", "reason": "malformed_event", "provenance": "native_transition_stream"}
    if any(str(event.get("run_id", "")) != str(run_id) for event in events):
        return {"status": "blocked", "reason": "contamination", "provenance": "native_transition_stream"}
    progress = len(events)
    if previous_progress is not None and progress <= previous_progress:
        return {"status": "blocked", "reason": "progress_free_recovery", "provenance": "native_transition_stream"}
    relevant = [event for event in events if not activity_id or str(event.get("operation_id", "")) == activity_id]
    if activity_id and not relevant:
        return {"status": "blocked", "reason": "activity_not_observed", "provenance": "native_transition_stream"}
    destinations = {str(event.get("site_id", "")) for event in relevant if event.get("site_id")}
    if expected_destination and destinations and expected_destination not in destinations:
        return {"status": "blocked", "reason": "wrong_destination", "provenance": "native_transition_stream"}
    phases = [str(event.get("new_phase", "")) for event in relevant]
    travelling = any(phase in TRAVEL_PHASES for phase in phases)
    arrived = any(phase in ARRIVAL_PHASES for phase in phases)
    interrupted = any(str(event.get("outcome", "")) in {"blocked", "interrupted", "rejected"} for event in relevant)
    return {
        "status": "clear" if relevant else "blocked",
        "reason": "structured_facts_observed" if relevant else "no_structured_facts",
        "provenance": "native_transition_stream",
        "progress": progress,
        "traveling": travelling,
        "arrived": arrived,
        "activity": bool(activity_id and relevant),
        "interrupted": interrupted,
        "destination": expected_destination or (next(iter(destinations)) if destinations else None),
    }


__all__ = [
    "MAX_EVENT_BYTES", "MAX_EVENTS", "SEMANTIC_STEP_PREFIX", "decide_event_facts",
    "decide_native_travel_boundary",
    "decide_run_state", "latest_semantic_step_frame", "read_bounded_transition_facts",
    "latest_semantic_game_minutes", "read_semantic_step_trace",
]
