"""Fail-closed continuous hostile-ecology certification route.

This module consumes only live structured events and saved receipts already
owned by the harness.  It never manufactures a missing event or identity.
"""

from __future__ import annotations

import hashlib
import json
import os
import tempfile
from pathlib import Path
from typing import Any, Mapping, Sequence

CERTIFICATION_AUTHORITY = "automated-certification"
CERTIFICATION_AUTHORITY_ALIASES = frozenset({CERTIFICATION_AUTHORITY, "certification"})
REQUIRED_LIFECYCLE = (
    "declared_world", "departure", "overmap_advance", "bubble_crossing_out",
    "actor_outcomes", "save", "quit", "relaunch", "bubble_crossing_in",
    "return_report", "camp_decision",
)
FORBIDDEN_ROUTES = frozenset({
    "diagnostic_replay", "checkpoint_rollback", "segment_splice", "replacement_world",
    "replacement_identity", "fixture_mutation", "scenario_mutation",
})


def evaluate_continuous_certification(
    *, round_id: str, binding_id: str, world_id: str, player_id: str,
    actor_ids: Sequence[str], events: Sequence[Mapping[str, Any]],
    authority: str = CERTIFICATION_AUTHORITY,
) -> dict[str, Any]:
    """Evaluate one uninterrupted lifecycle and stop at its first divergence."""
    expected = {
        "round_id": str(round_id), "binding_id": str(binding_id),
        "world_id": str(world_id), "player_id": str(player_id),
        "actor_ids": tuple(sorted(str(item) for item in actor_ids)),
    }
    if authority not in CERTIFICATION_AUTHORITY_ALIASES:
        return {"status": "invalid", "first_divergence": "authority"}
    if not all(expected[key] for key in ("round_id", "binding_id", "world_id", "player_id")) or not expected["actor_ids"]:
        return {"status": "invalid", "first_divergence": "binding"}
    seen: list[str] = []
    first: str = ""
    observations: dict[str, Any] = {}
    for index, event in enumerate(events):
        if not isinstance(event, Mapping):
            first = REQUIRED_LIFECYCLE[len(seen)] if len(seen) < len(REQUIRED_LIFECYCLE) else "event"
            break
        route = str(event.get("route", "")).strip()
        if route in FORBIDDEN_ROUTES:
            first = route
            break
        if any(str(event.get(key, "")) != expected[key] for key in ("round_id", "binding_id", "world_id", "player_id")):
            first = REQUIRED_LIFECYCLE[len(seen)] if len(seen) < len(REQUIRED_LIFECYCLE) else "identity"
            break
        observed_actors = tuple(sorted(str(item) for item in event.get("actor_ids", expected["actor_ids"])))
        if observed_actors != expected["actor_ids"]:
            first = REQUIRED_LIFECYCLE[len(seen)] if len(seen) < len(REQUIRED_LIFECYCLE) else "actors"
            break
        kind = str(event.get("kind", "")).strip()
        if kind != (REQUIRED_LIFECYCLE[len(seen)] if len(seen) < len(REQUIRED_LIFECYCLE) else ""):
            first = REQUIRED_LIFECYCLE[len(seen)] if len(seen) < len(REQUIRED_LIFECYCLE) else "event_order"
            break
        owner = event.get("owner")
        if owner in (None, ""):
            first = kind
            break
        observations[kind] = {"sequence": index, "owner": owner}
        seen.append(kind)
    if not first and len(seen) != len(REQUIRED_LIFECYCLE):
        first = REQUIRED_LIFECYCLE[len(seen)]
    result = {
        "status": "green" if not first else "red",
        "authority": authority,
        "round_id": expected["round_id"], "binding_id": expected["binding_id"],
        "world_id": expected["world_id"], "player_id": expected["player_id"],
        "actor_ids": list(expected["actor_ids"]), "lifecycle": list(seen),
        "required_lifecycle": list(REQUIRED_LIFECYCLE), "observations": observations,
    }
    if first:
        result["first_divergence"] = first
    return result


def write_immutable_certification_report(path: Path, result: Mapping[str, Any]) -> str:
    """Atomically create exactly one report; conflicting rewrites are rejected."""
    payload = json.dumps(dict(result), sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
    digest = hashlib.sha256(payload).hexdigest()
    path = Path(path)
    if path.exists():
        if path.read_bytes() != payload:
            raise ValueError("certification report is immutable")
        return digest
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return digest


def capture_and_finalize_certification(
    *, capture_manifest: Mapping[str, Any], report_path: Path,
    expected_token_id: str, expected_scenario_digest: str, expected_round_id: str,
    expected_binding_id: str, expected_world_id: str, expected_player_id: str,
    expected_actor_ids: Sequence[str],
    write_report: bool = True,
) -> dict[str, Any]:
    """Capture run-owned stream/receipts, evaluate once, and atomically report."""
    if not isinstance(capture_manifest, Mapping) or any(
            key in capture_manifest for key in ("events", "receipts", "lifecycle")):
        raise ValueError("certification finalization rejects inline lifecycle evidence")
    captures = capture_manifest.get("captured_artifacts")
    if not isinstance(captures, Mapping):
        raise ValueError("certification finalization requires captured_artifacts")
    values: dict[str, Any] = {}
    for name in ("lifecycle_stream", "crossing_receipts"):
        path_text = captures.get(name)
        if not isinstance(path_text, str) or not path_text.strip():
            raise ValueError(f"captured artifact path is missing: {name}")
        path = Path(path_text).expanduser().resolve()
        try:
            values[name] = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ValueError(f"captured artifact is unreadable: {name}: {path}") from exc
        if not isinstance(values[name], list):
            raise ValueError(f"captured artifact must be a list: {name}")
    actor_tuple = tuple(sorted(str(item) for item in expected_actor_ids))
    common = {
        "token_id": str(expected_token_id), "scenario_digest": str(expected_scenario_digest),
        "round_id": str(expected_round_id), "binding_id": str(expected_binding_id),
        "world_id": str(expected_world_id), "player_id": str(expected_player_id),
        "actor_ids": actor_tuple,
    }
    events = values["lifecycle_stream"]
    for index, event in enumerate(events, 1):
        if not isinstance(event, Mapping) or int(event.get("sequence", index)) != index:
            raise ValueError("lifecycle sequence is missing, duplicate, or out of order")
        for key, expected in common.items():
            observed = tuple(sorted(str(item) for item in event.get(key, ()))) if key == "actor_ids" else str(event.get(key, ""))
            if observed != expected:
                raise ValueError(f"lifecycle identity mismatch: {key}")
    for receipt in values["crossing_receipts"]:
        if not isinstance(receipt, Mapping):
            raise ValueError("crossing receipt is not an object")
        for key, expected in common.items():
            observed = tuple(sorted(str(item) for item in receipt.get(key, ()))) if key == "actor_ids" else str(receipt.get(key, ""))
            if observed != expected:
                raise ValueError(f"crossing receipt identity mismatch: {key}")
    result = evaluate_continuous_certification(
        round_id=expected_round_id, binding_id=expected_binding_id,
        world_id=expected_world_id, player_id=expected_player_id,
        actor_ids=actor_tuple, events=events,
    )
    if result.get("status") != "green":
        raise ValueError(f"continuous lifecycle failed at {result.get('first_divergence', 'unknown')}")
    result["token_id"] = str(expected_token_id)
    result["scenario_digest"] = str(expected_scenario_digest)
    result["crossing_receipts"] = values["crossing_receipts"]
    if write_report:
        write_immutable_certification_report(Path(report_path), result)
    return result
