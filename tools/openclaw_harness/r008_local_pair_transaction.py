#!/usr/bin/env python3
"""Fail-closed controlled setup adapters for R-008's local-pair evidence.

The retained native stream proves only the dispatch/contact identity tuple.
C++ persistence also needs a complete canonical site/outing serializer payload.
Until a same-run payload is captured and bound to this stream, these adapters
must refuse rather than infer target, route, watch, or physical NPC state.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Iterator, Mapping


class LocalPairTransactionError(RuntimeError):
    """The controlled R-008 setup transaction cannot proceed safely."""


_SITE_ID = "overmap_special:bandit_camp@177,13,0"
_OPERATION_ID = _SITE_ID + "#structural"
_ACTORS = [4, 5]
_DISPATCH_MINUTES = 8520
_CONTACT_MINUTES = 8580


def _object(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise LocalPairTransactionError(f"canonical snapshot {label} is missing or malformed")
    return value


def _require_fields(value: Mapping[str, Any], fields: set[str], label: str) -> None:
    missing = sorted(field for field in fields if field not in value)
    if missing:
        raise LocalPairTransactionError(
            f"canonical snapshot {label} is partial: {', '.join(missing)}"
        )


_SITE_FIELDS = {
    "schema_version", "site_id", "source_kind", "site_kind", "hostile_profile", "source_id",
    "anchor", "living_total", "supply_units", "supply_last_update_minutes",
    "supply_accounted_living_total", "supply_member_minute_remainder", "footprint", "members",
    "spawn_tiles", "next_outing_generation", "applied_return_generation",
    "applied_report_generation", "applied_cargo_generation", "last_cargo_application_key",
    "current_scout_report", "camp_decision", "acted_reports", "returned_cargo_stock",
    "active_outing", "active_hostile_operation", "remembered_target_or_mark",
    "remembered_threat_estimate", "remembered_bounty_estimate", "remembered_retreat_bias",
    "remembered_return_clock", "remembered_pressure", "known_recent_marks", "intelligence_map",
}
_OUTING_FIELDS = {
    "schema_version", "kind", "activity_id", "camp_id", "generation", "member_ids", "leader_id",
    "shared_route", "waypoint_index", "target_id", "target_omt", "job_type", "target_lead_id",
    "target_lead_revision", "phase", "observations", "cargo", "casualty_ids",
    "resolved_member_ids", "started_minutes", "local_contact_minutes", "last_progress_minutes",
    "expected_return_minutes", "missing_deadline_minutes", "simulation_owner", "handoff_epoch",
    "last_advanced_minutes", "local_projection_reconciliation_rejected", "return_application_key",
    "report_application_key", "cargo_application_key", "member_return_receipts",
    "local_return_eligibility", "crossing", "local_handoff", "abstract_encounter",
    "abstract_detour_attempts", "has_withdrawal_detour", "withdrawal_detour_omt",
    "target_footprint", "selected_watch_kind", "selected_watch_omt", "selected_watch_route_cost",
    "alternate_watch_kind", "alternate_watch_omt", "alternate_watch_route_cost",
    "alternate_watch_shared_route", "alternate_watch_attempted",
    "alternate_watch_reposition_pending", "covert_egress_chain_version", "covert_egress_attempts",
    "covert_egress_revision", "failed_covert_egress_omts", "current_covert_egress_route_omts",
    "failed_covert_egress_route_omts", "assessment",
}
_HANDOFF_FIELDS = {
    "schema_version", "activity_id", "activity_generation", "handoff_epoch", "waypoint_index",
    "phase", "route_position", "approach_from", "egress_omt", "cargo", "casualty_ids", "members",
    "cohesion_leader_id", "cohesion_deadline_minutes", "cohesion_reroutes_used",
    "cohesion_assembled", "cohesion_abort_return", "cohesion_best_staging_distances",
    "committed_minutes",
}
_HANDOFF_MEMBER_FIELDS = {
    "npc_id", "prior_position", "entry_position", "staging_position", "exit_position",
    "hp_percent", "dead",
}


def verify_canonical_local_pair_snapshot(
    path: Path, expected_sha256: str, *, source_report: Mapping[str, Any],
    source_binding: Mapping[str, Any],
) -> dict[str, Any]:
    """Validate the native capture without filling any serializer field ourselves."""
    raw = path.read_bytes()
    actual_sha256 = _sha256_bytes(raw)
    if actual_sha256 != expected_sha256:
        raise LocalPairTransactionError("canonical snapshot hash drifted")
    try:
        snapshot = _object(json.loads(raw), "root")
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise LocalPairTransactionError("canonical snapshot is not JSON") from exc
    if snapshot.get("schema_version") != 1:
        raise LocalPairTransactionError("canonical snapshot schema is unsupported")
    transition = _object(snapshot.get("transition"), "transition")
    source = _object(snapshot.get("source"), "source binding")
    if source.get("runtime_source_sha256") != source_binding.get("runtime_source_sha256") or \
            source.get("executable_sha256") != source_binding.get("executable_sha256"):
        raise LocalPairTransactionError("canonical snapshot source/executable binding is stale or mixed")
    expected_transition = source_report["local_pair_handoff"]
    exact_transition_keys = {
        "domain", "transition", "outcome", "site_id", "operation_id", "actor_ids", "generation",
        "simulation_owner", "previous_state", "new_state", "handoff_epoch", "game_minutes",
    }
    if snapshot.get("run_id") != expected_transition.get("run_id") or any(
            transition.get(key) != expected_transition.get(key) for key in exact_transition_keys):
        raise LocalPairTransactionError("canonical snapshot is stale, mixed, or does not match its native handoff")
    if transition.get("owner_transition") != "abstract_to_local" or not transition.get("omt"):
        raise LocalPairTransactionError("canonical snapshot crossing binding is incomplete")
    site = _object(snapshot.get("site_payload"), "site payload")
    _require_fields(site, _SITE_FIELDS, "site payload")
    outing = _object(site.get("active_outing"), "active outing")
    _require_fields(outing, _OUTING_FIELDS, "active outing")
    handoff = _object(outing.get("local_handoff"), "local handoff")
    _require_fields(handoff, _HANDOFF_FIELDS, "local handoff")
    members = handoff.get("members")
    if not isinstance(members, list) or len(members) != 2:
        raise LocalPairTransactionError("canonical snapshot local handoff lacks the exact pair")
    for member in members:
        _require_fields(_object(member, "local handoff member"), _HANDOFF_MEMBER_FIELDS,
                        "local handoff member")
    if site.get("schema_version", 0) < 10 or outing.get("schema_version", 0) < 10 or \
            site.get("site_id") != _SITE_ID or outing.get("activity_id") != _OPERATION_ID or \
            outing.get("generation") != 1 or outing.get("simulation_owner") != "local" or \
            outing.get("handoff_epoch") != 1 or outing.get("member_ids") != _ACTORS or \
            handoff.get("activity_id") != _OPERATION_ID or handoff.get("activity_generation") != 1 or \
            handoff.get("handoff_epoch") != 1 or handoff.get("committed_minutes") != _CONTACT_MINUTES or \
            [member.get("npc_id") for member in members] != _ACTORS:
        raise LocalPairTransactionError("canonical snapshot payload contradicts the required local pair")
    return {
        "path": str(path), "sha256": actual_sha256, "run_id": snapshot["run_id"],
        "transition": dict(transition), "site_payload": dict(site),
        "validation": "schema_v10_v11_required_fields_and_native_tuple_matched",
    }


def _sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _objects(value: Any) -> Iterator[Mapping[str, Any]]:
    if isinstance(value, Mapping):
        yield value
        for child in value.values():
            yield from _objects(child)
    elif isinstance(value, list):
        for child in value:
            yield from _objects(child)


def _exact_event(
    rows: list[Mapping[str, Any]], *, transition: str, minutes: int,
    owner: str, previous: str, next_state: str, epoch: int,
) -> Mapping[str, Any]:
    matches = [
        row for row in rows
        if row.get("domain") == "bandit_live_world"
        and row.get("transition") == transition
        and row.get("game_minutes") == minutes
        and row.get("outcome") == "committed"
        and row.get("site_id") == _SITE_ID
        and row.get("operation_id") == _OPERATION_ID
        and row.get("actor_ids") == _ACTORS
        and row.get("generation") == 1
        and row.get("simulation_owner") == owner
        and row.get("previous_state") == previous
        and row.get("new_state") == next_state
        and row.get("handoff_epoch") == epoch
    ]
    if len(matches) != 1:
        raise LocalPairTransactionError(
            f"source native evidence needs exactly one {transition} at minute {minutes}"
        )
    return matches[0]


def verify_native_source_report(path: Path, expected_sha256: str) -> dict[str, Any]:
    """Validate and bind the one native dispatch/contact evidence pair."""
    raw = path.read_bytes()
    actual_sha256 = _sha256_bytes(raw)
    if actual_sha256 != expected_sha256:
        raise LocalPairTransactionError("source native report hash drifted")
    try:
        report: Any = json.loads(raw)
    except json.JSONDecodeError:
        try:
            report = {"events": [json.loads(line) for line in raw.decode().splitlines() if line]}
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise LocalPairTransactionError("source native report is not JSON or JSONL") from exc
    if not isinstance(report, Mapping):
        raise LocalPairTransactionError("source native report is not an object")
    rows = [row for value in _objects(report) for row in [value]
            if isinstance(row.get("sequence"), int) and isinstance(row.get("game_minutes"), int)]
    return {
        "path": str(path),
        "sha256": actual_sha256,
        "dispatch": dict(_exact_event(
            rows, transition="active_sortie_dispatch", minutes=_DISPATCH_MINUTES,
            owner="abstract", previous="at_home", next_state="outbound", epoch=0,
        )),
        "local_pair_handoff": dict(_exact_event(
            rows, transition="local_pair_handoff", minutes=_CONTACT_MINUTES,
            owner="local", previous="abstract", next_state="local", epoch=1,
        )),
    }


def _verify_authority(source_report: Path, source_report_sha256: str,
                      source_binding: Mapping[str, Any], setup_authority: str) -> dict[str, Any]:
    if not setup_authority:
        raise LocalPairTransactionError("setup authority is required")
    if not source_binding.get("runtime_source_sha256") or not source_binding.get("executable_sha256"):
        raise LocalPairTransactionError("source/executable binding is incomplete")
    return verify_native_source_report(source_report, source_report_sha256)


def _missing_canonical_payload() -> LocalPairTransactionError:
    return LocalPairTransactionError(
        "canonical persisted base/local payload is required; deriving route, target, "
        "watch state, or physical snapshots from the native event tuple is forbidden"
    )


def run_base_fixture_transaction(
    world_dir: Path, *, source_report: Path, source_report_sha256: str,
    source_binding: Mapping[str, Any], setup_authority: str,
    canonical_snapshot: Path | None = None, canonical_snapshot_sha256: str = "",
) -> dict[str, Any]:
    """Install only the captured site payload into an otherwise site-empty world."""
    source = _verify_authority(source_report, source_report_sha256, source_binding, setup_authority)
    if canonical_snapshot is None or not canonical_snapshot_sha256:
        raise _missing_canonical_payload()
    snapshot = verify_canonical_local_pair_snapshot(
        canonical_snapshot, canonical_snapshot_sha256, source_report=source,
        source_binding=source_binding,
    )
    dimension_path = world_dir / "dimension_data.gsav"
    original = dimension_path.read_bytes()
    try:
        version_line, payload_bytes = original.split(b"\n", 1)
        payload = json.loads(payload_bytes)
        live_world = payload["overmapbuffer"]["bandit_live_world"]
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise LocalPairTransactionError("controlled setup world has no readable native ecology owner") from exc
    if not isinstance(live_world, dict) or live_world.get("sites") != []:
        raise LocalPairTransactionError("controlled setup world is not a pristine zero-site target")
    replacement = json.loads(json.dumps(snapshot["site_payload"], sort_keys=True))
    live_world["sites"] = [replacement]
    rendered = version_line + b"\n" + json.dumps(
        payload, ensure_ascii=False, separators=(",", ":"), sort_keys=True,
    ).encode("utf-8")
    temporary = dimension_path.with_name(dimension_path.name + ".r008-local-pair.tmp")
    try:
        temporary.write_bytes(rendered)
        temporary.replace(dimension_path)
    except OSError as exc:
        temporary.unlink(missing_ok=True)
        raise LocalPairTransactionError("controlled setup transaction could not persist its payload") from exc
    return {
        "setup_authority": setup_authority, "gameplay_credit": False,
        "source_native_report_sha256": source["sha256"],
        "canonical_snapshot_sha256": snapshot["sha256"],
        "run_id": snapshot["run_id"], "site_id": _SITE_ID,
        "transaction": "base_fixture_exact_canonical_site_payload",
    }


def run_local_pair_transaction(
    world_dir: Path, *, source_report: Path, source_report_sha256: str,
    source_binding: Mapping[str, Any], setup_authority: str,
    canonical_snapshot: Path | None = None, canonical_snapshot_sha256: str = "",
) -> dict[str, Any]:
    """Revalidate the exact installed payload; this step never manufactures state."""
    source = _verify_authority(source_report, source_report_sha256, source_binding, setup_authority)
    if canonical_snapshot is None or not canonical_snapshot_sha256:
        raise _missing_canonical_payload()
    snapshot = verify_canonical_local_pair_snapshot(
        canonical_snapshot, canonical_snapshot_sha256, source_report=source,
        source_binding=source_binding,
    )
    dimension_path = world_dir / "dimension_data.gsav"
    try:
        _version_line, payload_bytes = dimension_path.read_bytes().split(b"\n", 1)
        installed = json.loads(payload_bytes)["overmapbuffer"]["bandit_live_world"]["sites"]
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise LocalPairTransactionError("controlled setup payload is unavailable after installation") from exc
    if installed != [snapshot["site_payload"]]:
        raise LocalPairTransactionError("controlled setup payload was mutated or mixed after installation")
    return {
        "setup_authority": setup_authority, "gameplay_credit": False,
        "canonical_snapshot_sha256": snapshot["sha256"], "run_id": snapshot["run_id"],
        "site_id": _SITE_ID, "transaction": "local_pair_exact_canonical_payload_verified",
    }
