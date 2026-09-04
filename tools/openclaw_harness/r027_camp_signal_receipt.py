#!/usr/bin/env python3
"""Fail-closed normalization for one staffed-camp signal observation receipt.

The game stream owns the observation facts.  This small harness boundary only
adds the already-sealed run/executable/lease identity and refuses any receipt
that cannot be tied back to that exact live process.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Mapping


SCHEMA = "caol-r027-staffed-camp-signal-read-v1"
AGING_SCHEMA = "caol-r027-staffed-camp-signal-aging-v1"

_REQUIRED = (
    "identity", "observer", "camp", "channel", "source", "range", "line_of_sight",
    "elevation", "weather", "visibility", "observation_clocks", "lead",
    "outcome", "work", "persistence",
)

_AGING_REQUIRED = ("identity", "camp", "lead", "previous", "result", "persistence",
                   "drive_response")


def normalize_receipt(event: Mapping[str, Any], *, run_id: str, executable_sha256: str,
                      pid: int, profile: str, world: str,
                      lease: Mapping[str, Any], cleanup: Mapping[str, Any]) -> dict[str, Any]:
    """Return a complete run-bound receipt or a concrete rejection reason."""
    if not run_id or event.get("run_id") != run_id:
        return {"accepted": False, "reason": "wrong_or_missing_run"}
    if event.get("transition") != "staffed_camp_signal_read" or not isinstance(
            event.get("staffed_camp_signal_read"), Mapping):
        return {"accepted": False, "reason": "aggregate_or_wrong_transition"}
    actor_ids = event.get("actor_ids")
    observer = event["staffed_camp_signal_read"].get("observer")
    if not isinstance(actor_ids, list) or len(actor_ids) != 1 or not isinstance(observer, Mapping) or \
            str(actor_ids[0]) != str(observer.get("id", "")):
        return {"accepted": False, "reason": "missing_or_mismatched_actor_binding"}
    receipt = dict(event["staffed_camp_signal_read"])
    missing = [key for key in _REQUIRED if not isinstance(receipt.get(key), Mapping)]
    if missing:
        return {"accepted": False, "reason": "missing_required_fields", "missing": missing}
    identity = receipt["identity"]
    if any( not str( identity.get( key, "" )).strip() for key in
            ( "run_id", "scenario", "source_sha256", "executable_sha256", "binding_id" ) ):
        return {"accepted": False, "reason": "incomplete_production_identity"}
    if str( identity["run_id"] ) != run_id or str( identity["executable_sha256"] ) != executable_sha256:
        return {"accepted": False, "reason": "receipt_runtime_identity_mismatch"}
    if not executable_sha256 or pid <= 0 or not profile or not world:
        return {"accepted": False, "reason": "incomplete_runtime_binding"}
    if lease.get("run_id") != run_id or lease.get("executable_sha256") != executable_sha256 or \
            lease.get("pid") != pid or lease.get("profile") != profile or lease.get("world") != world:
        return {"accepted": False, "reason": "stale_or_wrong_lease"}
    if cleanup.get("pid") != pid or cleanup.get("lease_id") != lease.get("lease_id") or \
            not lease.get("cleanup_token") or cleanup.get("cleanup_token") != lease.get("cleanup_token"):
        return {"accepted": False, "reason": "incomplete_cleanup_binding"}
    return {
        "accepted": True,
        "schema": SCHEMA,
        "run_id": run_id,
        "executable_sha256": executable_sha256,
        "pid": pid,
        "profile": profile,
        "world": world,
        "lease_id": lease["lease_id"],
        "cleanup": dict(cleanup),
        "event_sequence": event.get("sequence"),
        "read": receipt,
    }


def normalize_aging_receipt(event: Mapping[str, Any], *, run_id: str, executable_sha256: str,
                            pid: int, profile: str, world: str,
                            lease: Mapping[str, Any], cleanup: Mapping[str, Any]) -> dict[str, Any]:
    """Bind one production aging receipt without treating aggregate counts as proof."""
    if not run_id or event.get("run_id") != run_id:
        return {"accepted": False, "reason": "wrong_or_missing_run"}
    if event.get("transition") != "staffed_camp_signal_aging" or not isinstance(
            event.get("staffed_camp_signal_aging"), Mapping):
        return {"accepted": False, "reason": "aggregate_or_wrong_transition"}
    receipt = dict(event["staffed_camp_signal_aging"])
    missing = [key for key in _AGING_REQUIRED if not isinstance(receipt.get(key), Mapping)]
    if missing:
        return {"accepted": False, "reason": "missing_required_fields", "missing": missing}
    identity = receipt["identity"]
    if any(not str(identity.get(key, "")).strip() for key in
           ("run_id", "scenario", "source_sha256", "executable_sha256", "binding_id")):
        return {"accepted": False, "reason": "incomplete_production_identity"}
    if str(identity["run_id"]) != run_id or str(identity["executable_sha256"]) != executable_sha256:
        return {"accepted": False, "reason": "receipt_runtime_identity_mismatch"}
    if not executable_sha256 or pid <= 0 or not profile or not world:
        return {"accepted": False, "reason": "incomplete_runtime_binding"}
    if lease.get("run_id") != run_id or lease.get("executable_sha256") != executable_sha256 or \
            lease.get("pid") != pid or lease.get("profile") != profile or lease.get("world") != world:
        return {"accepted": False, "reason": "stale_or_wrong_lease"}
    if cleanup.get("pid") != pid or cleanup.get("lease_id") != lease.get("lease_id") or \
            not lease.get("cleanup_token") or cleanup.get("cleanup_token") != lease.get("cleanup_token"):
        return {"accepted": False, "reason": "incomplete_cleanup_binding"}
    lead = receipt["lead"]
    previous = receipt["previous"]
    result = receipt["result"]
    persistence = receipt["persistence"]
    if not all(str(lead.get(key, "")).strip() for key in ("id", "channel", "source_omt", "source_key")) or \
            not all(key in previous for key in ("last_seen_minutes", "age_minutes", "status")) or \
            not all(key in result for key in ("last_seen_minutes", "age_minutes", "status", "expired_removed")) or \
            not all(str(persistence.get(key, "")).strip() for key in
                    ("lead_set_hash_before", "lead_set_hash_after")):
        return {"accepted": False, "reason": "incomplete_aging_transition"}
    return {
        "accepted": True,
        "schema": AGING_SCHEMA,
        "run_id": run_id,
        "executable_sha256": executable_sha256,
        "pid": pid,
        "profile": profile,
        "world": world,
        "lease_id": lease["lease_id"],
        "cleanup": dict(cleanup),
        "event_sequence": event.get("sequence"),
        "aging": receipt,
    }


def normalize_run_bound_receipts(run_dir: Path) -> list[dict[str, Any]]:
    """Bind production reads only through the run's existing sidecars.

    This deliberately has no aggregate fallback and does not accept caller
    supplied identity.  A missing process, lease, cleanup, profile, world, or
    runtime digest therefore stays a rejection for every event in the stream.
    """
    root = Path(run_dir)

    def load(name: str) -> Mapping[str, Any]:
        try:
            value = json.loads((root / name).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}
        return value if isinstance(value, Mapping) else {}

    transition = load("transition.events.binding.json")
    runtime = load("runtime.binding.json")
    process = load("process.json")
    plan = load("plan.json")
    run_id = str(transition.get("run_id", ""))
    executable_sha256 = str(runtime.get("executable_sha256", ""))
    pid = process.get("pid", 0)
    if isinstance(pid, bool) or not isinstance(pid, int):
        pid = 0
    lease = process.get("certification_lease")
    cleanup = process.get("cleanup")
    lease = lease if isinstance(lease, Mapping) else {}
    cleanup = cleanup if isinstance(cleanup, Mapping) else {}
    try:
        lines = (root / str(transition.get("event_path", ""))).read_text(encoding="utf-8").splitlines()
    except OSError:
        return []
    results: list[dict[str, Any]] = []
    for line in lines:
        try:
            event = json.loads(line)
        except ValueError:
            continue
        if not isinstance(event, Mapping):
            continue
        results.append(normalize_receipt(
            event, run_id=run_id, executable_sha256=executable_sha256, pid=pid,
            profile=str(plan.get("profile", "")), world=str(plan.get("target_world", "")),
            lease=lease, cleanup=cleanup,
        ))
    return results


def validate_unchanged_deduplication(receipts: list[Mapping[str, Any]]) -> dict[str, Any]:
    """Prove one in-range lead remains one lead across a second native read.

    The caller supplies only normalized, run-bound receipts.  The first valid
    creation and its later unchanged counterpart must agree on every causal
    identity, while the second receipt must preserve the first receipt's
    persisted lead count and digest exactly.
    """
    accepted = [receipt for receipt in receipts if receipt.get("accepted") is True]
    created = next((receipt for receipt in accepted
                    if _read_outcome(receipt) == "created"), None)
    if created is None:
        return {"accepted": False, "reason": "missing_created_read"}
    unchanged = next((receipt for receipt in accepted
                      if _read_outcome(receipt) == "unchanged" and
                      _same_signal_identity(created, receipt)), None)
    if unchanged is None:
        return {"accepted": False, "reason": "missing_matching_unchanged_read"}
    first = created["read"]
    second = unchanged["read"]
    first_persistence = first["persistence"]
    second_persistence = second["persistence"]
    required = ("lead_count_after", "lead_hash_after")
    if any(key not in first_persistence or key not in second_persistence for key in required) or \
            "lead_count_before" not in second_persistence or "lead_hash_before" not in second_persistence:
        return {"accepted": False, "reason": "missing_persistence_dedup_fields"}
    if first_persistence["lead_count_after"] != second_persistence["lead_count_before"] or \
            first_persistence["lead_count_after"] != second_persistence["lead_count_after"]:
        return {"accepted": False, "reason": "lead_count_changed_or_duplicated"}
    if first_persistence["lead_hash_after"] != second_persistence["lead_hash_before"] or \
            first_persistence["lead_hash_after"] != second_persistence["lead_hash_after"]:
        return {"accepted": False, "reason": "lead_payload_changed"}
    signal_range = second["range"]
    if not second["line_of_sight"].get("result") or not isinstance(signal_range.get("actual"), int) or \
            not isinstance(signal_range.get("cap"), int) or signal_range["actual"] > signal_range["cap"]:
        return {"accepted": False, "reason": "unchanged_read_not_in_range_visible"}
    return {
        "accepted": True,
        "schema": "caol-r027-unchanged-dedup-v1",
        "created_event_sequence": created.get("event_sequence"),
        "unchanged_event_sequence": unchanged.get("event_sequence"),
        "observer_id": second["observer"].get("id"),
        "camp_id": second["camp"].get("id"),
        "channel": second["channel"].get("kind"),
        "source_omt": second["source"].get("omt"),
        "lead_id": second["lead"].get("id"),
        "lead_count": second_persistence["lead_count_after"],
        "lead_hash": second_persistence["lead_hash_after"],
    }


def validate_changed_observation_capture(
    records: list[Mapping[str, Any]], *, persisted: Mapping[str, Any], run_id: str,
    source_sha256: str, executable_sha256: str, binding_id: str,
    source_omt: tuple[int, int, int], lead_id: str, source_key: str,
) -> dict[str, Any]:
    """Fail closed over R-027's native channel scan and saved post-state."""
    expected_omt = "(" + ",".join( str(value) for value in source_omt ) + ")"
    smoke = []
    for record in records:
        binding = record.get("binding") if isinstance(record.get("binding"), Mapping) else {}
        if record.get("run_id") != run_id or binding.get("runtime_source_sha256") != source_sha256 or \
                binding.get("executable_sha256") != executable_sha256 or \
                binding.get("binding_id") != binding_id:
            return {"accepted": False, "reason": "sidecar_identity_mismatch"}
        if record.get("channel") == "smoke" and record.get("observed") is True:
            smoke.append(record)
    if len(smoke) != 1 or smoke[0].get("source_omt") != expected_omt or \
            smoke[0].get("signal_origin") != "local_field":
        return {"accepted": False, "reason": "missing_exact_native_smoke_channel"}
    tile = persisted.get("tile") if isinstance(persisted.get("tile"), Mapping) else {}
    fields = tile.get("fields") if isinstance(tile.get("fields"), list) else []
    if not any(field.get("field_id") == "fd_fire" and field.get("intensity") == 2
               for field in fields if isinstance(field, Mapping)):
        return {"accepted": False, "reason": "persisted_intensity_two_source_missing"}
    leads = persisted.get("leads") if isinstance(persisted.get("leads"), list) else []
    target = next((lead for lead in leads if isinstance(lead, Mapping) and
                   lead.get("lead_id") == lead_id and lead.get("source_key") == source_key), None)
    if persisted.get("lead_count") != 3 or target is None:
        return {"accepted": False, "reason": "same_lead_or_count_not_persisted"}
    if target.get("first_seen_minutes") != 9245 or target.get("last_seen_minutes") != 9250 or \
            target.get("last_checked_minutes") != 9250:
        return {"accepted": False, "reason": "lead_refresh_clock_not_persisted"}
    return {"accepted": True, "schema": "caol-r027-changed-observation-capture-v1",
            "channel": "smoke", "source_omt": list(source_omt), "lead_id": lead_id,
            "source_key": source_key, "lead_count": 3, "refreshed_minutes": 9250}


def _read_outcome(receipt: Mapping[str, Any]) -> str:
    read = receipt.get("read")
    outcome = read.get("outcome") if isinstance(read, Mapping) else None
    return str(outcome.get("kind", "")) if isinstance(outcome, Mapping) else ""


def _same_signal_identity(first: Mapping[str, Any], second: Mapping[str, Any]) -> bool:
    left = first.get("read")
    right = second.get("read")
    if not isinstance(left, Mapping) or not isinstance(right, Mapping):
        return False
    for key in ("observer", "camp", "channel", "source", "lead"):
        if left.get(key) != right.get(key):
            return False
    return True
