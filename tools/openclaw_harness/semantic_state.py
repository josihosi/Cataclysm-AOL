"""Bounded run-only state decisions for improved harness routes.

The game owns the transition stream.  This module only consumes that stream
inside the authorized run directory and never uses screenshots or OCR to
decide a verdict.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping, Optional, Sequence, Set


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
    event_filter: Optional[Set[str]] = None,
) -> tuple[list[dict[str, Any]], str]:
    """Read run-owned semantic frames, surfaces, and receipts from a debug trace."""
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
        if event not in {"frame", "receipt", "surface_descriptor", "surface_receipt", "travel"}:
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
            # Production native descriptors are the sole semantic action
            # authority.  A companion observational frame intentionally has
            # no physical-input bindings; retain it only as observation.
            if bindings is not None and (not isinstance(bindings, Mapping) or
                                         set(bindings) != set(actions) or any(
                                             not isinstance(item, str) or not item
                                             for item in bindings.values())):
                return [], "malformed_semantic_step"
        elif event == "receipt":
            if not frame_id:
                return [], "malformed_semantic_step"
            if not isinstance(normalized.get("accepted"), bool):
                return [], "malformed_semantic_step"
        elif event == "surface_descriptor":
            schema_version = normalized.get("schema_version")
            surface_id = normalized.get("surface_id")
            kind = normalized.get("kind")
            breadcrumbs = normalized.get("breadcrumbs")
            payload = normalized.get("payload")
            actions = normalized.get("valid_actions")
            if schema_version != 1 or not isinstance(surface_id, str) or not surface_id or \
                    not frame_id or not isinstance(kind, str) or not kind or \
                    not isinstance(breadcrumbs, list) or not breadcrumbs or \
                    any(not isinstance(value, str) or not value for value in breadcrumbs) or \
                    not isinstance(payload, Mapping) or any(
                        not isinstance(key, str) or not isinstance(value, str)
                        for key, value in payload.items()
                    ) or not isinstance(actions, list):
                return [], "malformed_semantic_surface_descriptor"
            action_keys = set()
            repeated_action_ids = set()
            action_ids = set()
            for action in actions:
                if not isinstance(action, Mapping) or not isinstance(action.get("id"), str) or \
                        not action["id"] or not isinstance(action.get("stable_id"), str) or \
                        not isinstance(action.get("label"), str) or not isinstance(
                            action.get("enabled"), bool):
                    return [], "malformed_semantic_surface_descriptor"
                action_id = action["id"]
                stable_id = action["stable_id"]
                action_key = (action_id, stable_id)
                if action_key in action_keys:
                    return [], "malformed_semantic_surface_descriptor"
                if action_id in action_ids:
                    repeated_action_ids.add(action_id)
                action_keys.add(action_key)
                action_ids.add(action_id)
            if any(not action["stable_id"] and action["id"] in repeated_action_ids
                   for action in actions):
                return [], "malformed_semantic_surface_descriptor"
            if kind == "unsupported" and actions:
                return [], "malformed_semantic_surface_descriptor"
        elif event == "surface_receipt":
            required = (
                "request_id", "requested_run_id", "requested_surface_id", "requested_frame_id",
                "consuming_surface_id", "consuming_frame_id", "action_id", "rejection_reason",
                "resulting_frame_id",
            )
            if any(not isinstance(normalized.get(key), str) for key in required) or \
                    not isinstance(normalized.get("accepted"), bool) or \
                    normalized.get("requested_run_id") != normalized.get("run_id"):
                return [], "malformed_semantic_surface_receipt"
        else:
            destination = normalized.get("destination")
            if str(normalized.get("travel_id", "")).strip() == "" or \
                    str(normalized.get("receipt_id", "")).strip() == "" or \
                    str(normalized.get("state", "")) not in {
                        "active", "progress", "completed_cleared", "blocked", "interrupted",
                        "hostile_boundary",
                    } or not isinstance(destination, list) or len(destination) != 3 or \
                    any(isinstance(value, bool) or not isinstance(value, int) for value in destination) or \
                    not isinstance(normalized.get("destination_present"), bool) or \
                    not isinstance(normalized.get("destination_cleared"), bool):
                return [], "malformed_semantic_travel"
            if normalized["state"] == "hostile_boundary":
                avatar_omt = normalized.get("avatar_omt")
                if not isinstance(avatar_omt, list) or len(avatar_omt) != 3 or any(
                        isinstance(value, bool) or not isinstance(value, int)
                        for value in avatar_omt):
                    return [], "malformed_semantic_travel"
        if event_filter is not None and event not in event_filter:
            byte_cursor += len(raw_line.encode("utf-8"))
            continue
        events.append(normalized)
        if len(events) > MAX_EVENTS:
            return [], "unbounded_output"
        byte_cursor += len(raw_line.encode("utf-8"))
    return events, "ok"


def decide_native_travel_boundary(
    events: Sequence[Mapping[str, Any]], *, run_id: str,
    expected_destination: Sequence[int], allow_handled_hostile_boundaries: bool = False,
) -> dict[str, Any]:
    """Prove one native travel route completed and cleared its destination."""
    expected = list(expected_destination)
    if len(expected) != 3 or any(isinstance(value, bool) or not isinstance(value, int)
                                 for value in expected):
        return {"status": "blocked", "reason": "invalid_expected_destination"}
    active: Optional[dict[str, Any]] = None
    progress: Optional[dict[str, Any]] = None
    handled_hostile_boundaries: list[dict[str, Any]] = []
    first_handled_hostile_boundary: Optional[dict[str, Any]] = None
    for event_index, event in enumerate(events):
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
            # Native travel publishes the zero-remaining-path progress fact before
            # its distinct completed_cleared receipt.  It establishes no terminal
            # proof by itself, but it is a valid transition that must remain visible
            # so the following completion receipt can settle the boundary.
            if (event.get("destination_present") is not True or event.get("destination_cleared") is not False) and \
                    (event.get("destination_present") is not False or event.get("destination_cleared") is not True):
                return {"status": "blocked", "reason": "progress_travel_fact_invalid",
                        "travel_id": active["travel_id"]}
            progress = dict(event)
            continue
        if state == "hostile_boundary":
            if allow_handled_hostile_boundaries:
                handled_boundary = {
                    "receipt_id": event.get("receipt_id"),
                    "avatar_omt": event.get("avatar_omt"),
                }
                handled_hostile_boundaries.append(handled_boundary)
                if first_handled_hostile_boundary is None:
                    first_handled_hostile_boundary = handled_boundary
                continue
            for later_event in events[event_index + 1:]:
                if str( later_event.get( "event", "" ) ) != "travel" or \
                        str( later_event.get( "run_id", "" ) ) != str( run_id ) or \
                        str( later_event.get( "travel_id", "" ) ) != str( active["travel_id"] ):
                    continue
                if str( later_event.get( "state", "" ) ) in {
                    "progress", "completed_cleared", "blocked", "interrupted",
                }:
                    return {
                        "status": "blocked",
                        "reason": "post_hostile_boundary_travel",
                        "travel_id": active["travel_id"],
                        "hostile_boundary_omt": event.get( "avatar_omt" ),
                        "post_boundary_receipt_id": later_event.get( "receipt_id" ),
                    }
            return {
                "status": "blocked",
                "reason": "hostile_boundary",
                "travel_id": active["travel_id"],
                "hostile_boundary_omt": event.get("avatar_omt"),
            }
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
                "handled_hostile_boundaries": handled_hostile_boundaries,
            }
    if active is not None:
        if first_handled_hostile_boundary is not None:
            # A permissive choice permits observing a hostile boundary, never
            # promotes an incomplete route to success.  Keep the native fact
            # available for a durable adverse-terminal receipt.
            return {
                "status": "blocked",
                "reason": "hostile_boundary_without_terminal",
                "travel_id": active["travel_id"],
                "hostile_boundary_omt": first_handled_hostile_boundary.get("avatar_omt"),
                "hostile_boundary_receipt_id": first_handled_hostile_boundary.get("receipt_id"),
                "handled_hostile_boundaries": handled_hostile_boundaries,
                "last_progress_receipt_id": progress.get("receipt_id") if progress else None,
            }
        return {
            "status": "blocked",
            "reason": "active_travel_progress_without_terminal" if progress else "active_travel_no_progress",
            "travel_id": active["travel_id"],
            "last_progress_receipt_id": progress.get("receipt_id") if progress else None,
        }
    return {"status": "blocked", "reason": "travel_not_observed"}


def latest_semantic_step_frame(events: Sequence[Mapping[str, Any]]) -> Optional[dict[str, Any]]:
    """Return the latest production observation with its matching receipt.

    A production ``surface_descriptor`` remains authoritative while its owner
    is active.  Legacy frames remain a compatibility fallback only for routes
    that have not published a production descriptor.
    """
    latest: Optional[dict[str, Any]] = None
    latest_native_observation: Optional[dict[str, Any]] = None
    legacy_receipts: dict[str, dict[str, Any]] = {}
    surface_receipts: dict[tuple[str, str], dict[str, Any]] = {}
    for event in events:
        event_kind = str(event.get("event", ""))
        if event_kind == "surface_descriptor":
            latest = dict(event)
        elif event_kind == "frame":
            latest_native_observation = dict(event)
            if latest is None:
                latest = dict(event)
        elif event_kind == "receipt":
            legacy_receipts[str(event.get("frame_id", ""))] = dict(event)
        elif event_kind == "surface_receipt":
            surface_receipts[(
                str(event.get("requested_surface_id", "")),
                str(event.get("requested_frame_id", "")),
            )] = dict(event)
    if latest is not None:
        if latest.get("event") == "surface_descriptor":
            latest["native_receipt"] = surface_receipts.get((
                str(latest.get("surface_id", "")), str(latest["frame_id"]),
            ))
            # A World descriptor owns the public action grant, but deliberately
            # omits the private map projection.  Keep its newest same-run World
            # frame as private issuing evidence so guarded movement can verify
            # its actual position and next tile without substituting legacy
            # actions or exposing that projection on the public surface.
            if latest.get("kind") == "world" and isinstance(latest_native_observation, Mapping) and \
                    latest_native_observation.get("run_id") == latest.get("run_id") and \
                    latest_native_observation.get("state") == "world":
                latest["observation"] = dict(latest_native_observation.get("observation", {}))
                latest["keep_watch_safety"] = dict(
                    latest_native_observation.get("keep_watch_safety", {})
                )
                latest["state"] = "world"
                latest["provenance"] = "native_semantic_step_trace"
        else:
            latest["native_receipt"] = legacy_receipts.get(str(latest["frame_id"]))
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
