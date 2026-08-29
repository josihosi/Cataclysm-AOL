#!/usr/bin/env python3
"""Fail-closed source reports for observation-only production probes.

The probe writes durable observations without granting route, lifecycle, or
certification authority.  The registry may independently ingest a compatible
probe report later; this module never creates that authority.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
from typing import Any, Mapping, Sequence


class ProductionCaptureError(ValueError):
    """The proposed capture cannot establish production origin."""


class RelaunchReceiptError(ProductionCaptureError):
    """A save/relaunch receipt cannot prove one continuous owner transition."""


def _sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree_sha256(path: Path) -> str:
    """Hash one saved world without relying on its absolute source location."""
    if not path.is_dir():
        raise ProductionCaptureError(f"saved world is missing: {path}")
    digest = hashlib.sha256()
    for entry in sorted(path.rglob("*"), key=lambda item: item.relative_to(path).as_posix()):
        relative = entry.relative_to(path).as_posix()
        if entry.is_symlink() or not entry.is_file():
            if entry.is_dir():
                continue
            raise ProductionCaptureError(f"saved world contains unsupported entry: {relative}")
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(entry.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _object(value: Any, field: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ProductionCaptureError(f"{field} is missing or malformed")
    return value


def _sha256(value: Any, field: str) -> str:
    result = str(value or "").strip().lower()
    if len(result) != 64 or any(character not in "0123456789abcdef" for character in result):
        raise ProductionCaptureError(f"{field} is not a SHA-256 digest")
    return result


def _chain(value: Any, field: str) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) and item.strip() for item in value):
        raise ProductionCaptureError(f"{field} is missing or malformed")
    return list(value)


def _observation_probe_sha256(report: Mapping[str, Any]) -> str:
    """Hash the immutable probe payload, excluding the sidecar pointer itself."""
    canonical = dict(report)
    canonical.pop("production_observation", None)
    payload = json.dumps(canonical, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


_RELAUNCH_FORBIDDEN_MARKERS = frozenset({
    "rollback", "resume", "segment_join", "segment_splice", "replacement_save",
    "replacement_world", "replacement_identity", "checkpoint_rollback",
})
def _relaunch_error(message: str) -> RelaunchReceiptError:
    return RelaunchReceiptError("relaunch receipt rejected: " + message)


def _receipt_field(receipt: Mapping[str, Any], identity: Mapping[str, Any], *names: str) -> Any:
    for name in names:
        if name in identity:
            return identity[name]
        if name in receipt:
            return receipt[name]
    return None


def _nonempty_text(value: Any, field: str) -> str:
    result = str(value or "").strip()
    if not result:
        raise _relaunch_error(f"{field} is missing")
    return result


def _positive_int(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise _relaunch_error(f"{field} is invalid")
    return value


def _nonnegative_int(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise _relaunch_error(f"{field} is invalid")
    return value


def _actor_ids(value: Any, field: str) -> list[str]:
    if not isinstance(value, list) or not value or not all(
            isinstance(item, (str, int)) and not isinstance(item, bool) and str(item).strip()
            for item in value):
        raise _relaunch_error(f"{field} is missing or malformed")
    result = [str(item) for item in value]
    if len(result) != len(set(result)):
        raise _relaunch_error(f"{field} contains duplicate identities")
    return result


def _reject_forbidden_route(receipt: Mapping[str, Any], label: str) -> None:
    for key in ("rollback", "resume", "segment_join", "segment_splice", "replacement_save",
                "replacement_world", "replacement_identity", "stale", "duplicate", "partial",
                "malformed"):
        if receipt.get(key) is True:
            raise _relaunch_error(f"{label} carries forbidden {key} marker")
    for key in ("route", "mode", "operation", "transition", "source_kind", "lifecycle_kind"):
        value = str(receipt.get(key, "") or "").strip().lower().replace("-", "_")
        if value in _RELAUNCH_FORBIDDEN_MARKERS:
            raise _relaunch_error(f"{label} uses forbidden {value}")


def _normalize_relaunch_receipt(receipt: Mapping[str, Any], label: str) -> dict[str, Any]:
    if not isinstance(receipt, Mapping):
        raise _relaunch_error(f"{label} is not an object")
    _reject_forbidden_route(receipt, label)
    identity = receipt.get("identity")
    if not isinstance(identity, Mapping):
        identity = receipt
    world_id = _nonempty_text(_receipt_field(receipt, identity, "world_id", "world_identity"), f"{label}.world_id")
    run_id = _nonempty_text(_receipt_field(receipt, identity, "run_id", "lineage_id", "run_lineage"), f"{label}.run_id")
    activity_id = _nonempty_text(_receipt_field(receipt, identity, "activity_id", "operation_id"), f"{label}.activity_id")
    generation = _positive_int(_receipt_field(receipt, identity, "generation"), f"{label}.generation")
    actor_ids = _actor_ids(_receipt_field(receipt, identity, "actor_ids", "member_ids"), f"{label}.actor_ids")
    owner = _nonempty_text(_receipt_field(receipt, identity, "simulation_owner", "authoritative_owner", "owner"), f"{label}.simulation_owner")
    if owner not in {"abstract", "local"}:
        raise _relaunch_error(f"{label}.simulation_owner is not authoritative")
    handoff_epoch = _nonnegative_int(_receipt_field(receipt, identity, "handoff_epoch"), f"{label}.handoff_epoch")
    crossing = receipt.get("crossing_receipt", receipt.get("crossing"))
    if not isinstance(crossing, Mapping):
        raise _relaunch_error(f"{label}.crossing_receipt is missing")
    crossing_values = {
        "actor_ids": _actor_ids(crossing.get("actor_ids"), f"{label}.crossing_receipt.actor_ids"),
        "run_id": _nonempty_text(crossing.get("run_id"), f"{label}.crossing_receipt.run_id"),
        "activity_id": _nonempty_text(crossing.get("activity_id"), f"{label}.crossing_receipt.activity_id"),
        "generation": _positive_int(crossing.get("generation"), f"{label}.crossing_receipt.generation"),
        "prior_owner": _nonempty_text(crossing.get("prior_owner"), f"{label}.crossing_receipt.prior_owner"),
        "next_owner": _nonempty_text(crossing.get("next_owner"), f"{label}.crossing_receipt.next_owner"),
        "handoff_epoch": _nonnegative_int(crossing.get("handoff_epoch"), f"{label}.crossing_receipt.handoff_epoch"),
        "cursor_minutes": _nonnegative_int(crossing.get("cursor_minutes"), f"{label}.crossing_receipt.cursor_minutes"),
        "cursor_waypoint": _nonnegative_int(crossing.get("cursor_waypoint"), f"{label}.crossing_receipt.cursor_waypoint"),
        "outcome": _nonempty_text(crossing.get("outcome"), f"{label}.crossing_receipt.outcome"),
        "persistence_acknowledged": crossing.get("persistence_acknowledged"),
    }
    if crossing_values["prior_owner"] == crossing_values["next_owner"]:
        raise _relaunch_error(f"{label}.crossing_receipt has no owner transition")
    if crossing_values["next_owner"] != owner:
        raise _relaunch_error(f"{label} owner disagrees with crossing receipt")
    if crossing_values["outcome"] != "committed" or crossing_values["persistence_acknowledged"] is not True:
        raise _relaunch_error(f"{label}.crossing_receipt is not one committed persisted transition")
    identity_values = {
        "world_id": world_id, "run_id": run_id, "activity_id": activity_id,
        "generation": generation, "actor_ids": actor_ids,
        "simulation_owner": owner, "handoff_epoch": handoff_epoch,
    }
    if any(crossing_values[key] != identity_values[key] for key in (
            "run_id", "activity_id", "generation", "actor_ids", "handoff_epoch")):
        raise _relaunch_error(f"{label} identity disagrees with crossing receipt")
    return {"identity": identity_values, "crossing_receipt": crossing_values}


def normalize_relaunch_receipt(
    *, before_save: Mapping[str, Any], after_load: Mapping[str, Any],
    transition: Mapping[str, Any] | Sequence[Mapping[str, Any]],
    expected_world_id: str = "", expected_run_id: str = "",
) -> dict[str, Any]:
    """Validate one ordinary save/quit/new-process load against one transition.

    This is an observation-only boundary: it returns a canonical receipt and
    never repairs or promotes a saved world. Any missing, duplicate, stale, or
    contradictory identity is rejected before report ingestion can use it.
    """
    before = _normalize_relaunch_receipt(before_save, "before_save")
    after = _normalize_relaunch_receipt(after_load, "after_load")
    if isinstance(transition, Mapping):
        transition_values = transition
    elif isinstance(transition, Sequence) and not isinstance(transition, (str, bytes)):
        if len(transition) != 1:
            raise _relaunch_error("transition receipt count is not exactly one")
        transition_values = transition[0]
    else:
        raise _relaunch_error("transition receipt is missing or malformed")
    if not isinstance(transition_values, Mapping):
        raise _relaunch_error("transition receipt is not an object")
    transition_receipt = dict(transition_values)
    if transition_receipt.get("outcome") != "committed" or transition_receipt.get("persistence_acknowledged") is not True:
        raise _relaunch_error("transition receipt is not committed and persisted")
    canonical_transition = _normalize_relaunch_receipt(
        {**after_load, "crossing_receipt": transition_receipt}, "transition"
    )["crossing_receipt"]
    if before != after or after["crossing_receipt"] != canonical_transition:
        raise _relaunch_error("post-load normalization disagrees with the single transition")
    identity = after["identity"]
    if expected_world_id and identity["world_id"] != str(expected_world_id):
        raise _relaunch_error("world identity differs from bound save")
    if expected_run_id and identity["run_id"] != str(expected_run_id):
        raise _relaunch_error("run lineage differs from bound run")
    return {
        "schema": "normal-save-relaunch-receipt-v1", "status": "green",
        "world_id": identity["world_id"], "run_id": identity["run_id"],
        "identity": identity, "crossing_receipt": after["crossing_receipt"],
        "normalization": "matched_single_committed_transition",
    }


def audit_sole_owner_receipts(
    receipts: Sequence[Mapping[str, Any]] | Mapping[str, Any], *,
    expected_world_id: str = "", expected_run_id: str = "",
    expected_actor_ids: Sequence[Any] = (),
) -> dict[str, Any]:
    """Audit production owner snapshots independently of route selection.

    Every row is one ownership change and carries complete ``before`` and
    ``after`` receipts.  The audit is deliberately separate from registry
    selection: a duplicate, stale, partial, malformed, ambiguous, or
    replacement-identity claim is red even when a route otherwise matches.
    """
    raw_rows: Any = receipts.get("receipts") if isinstance(receipts, Mapping) else receipts
    if not isinstance(raw_rows, list) or not raw_rows:
        return {"schema": "sole-owner-audit-v1", "status": "red", "errors": ["missing_receipts"]}
    declared = [str(item) for item in expected_actor_ids]
    if isinstance(receipts, Mapping) and receipts.get("actor_ids") is not None:
        declared = [str(item) for item in receipts.get("actor_ids", [])]
    if declared and len(declared) != len(set(declared)):
        return {"schema": "sole-owner-audit-v1", "status": "red", "errors": ["duplicate_declared_identity"]}
    try:
        normalized: list[dict[str, Any]] = []
        actor_set: tuple[str, ...] | None = tuple(declared) if declared else None
        current: dict[str, tuple[str, str, int, int]] = {}
        claimed: set[tuple[str, str, str, int, int]] = set()
        for index, row in enumerate(raw_rows):
            if not isinstance(row, Mapping):
                raise _relaunch_error(f"receipts[{index}] is not an object")
            before = _normalize_relaunch_receipt(row.get("before", {}), f"receipts[{index}].before")
            after = _normalize_relaunch_receipt(row.get("after", {}), f"receipts[{index}].after")
            before_identity = before["identity"]
            after_identity = after["identity"]
            identity_keys = ("world_id", "run_id", "activity_id", "generation", "actor_ids")
            if any(before_identity[key] != after_identity[key] for key in identity_keys):
                raise _relaunch_error(f"receipts[{index}] identity changed across ownership")
            if before_identity["simulation_owner"] == after_identity["simulation_owner"]:
                raise _relaunch_error(f"receipts[{index}] has no owner change")
            if after_identity["handoff_epoch"] != before_identity["handoff_epoch"] + 1:
                raise _relaunch_error(f"receipts[{index}] has stale handoff epoch")
            if expected_world_id and before_identity["world_id"] != str(expected_world_id):
                raise _relaunch_error(f"receipts[{index}] world identity differs from bound world")
            if expected_run_id and before_identity["run_id"] != str(expected_run_id):
                raise _relaunch_error(f"receipts[{index}] run lineage differs from bound run")
            actors = tuple(before_identity["actor_ids"])
            if actor_set is None:
                actor_set = actors
            if actors != actor_set:
                raise _relaunch_error(f"receipts[{index}] replaces or partially names actors")
            before_crossing = before["crossing_receipt"]
            after_crossing = after["crossing_receipt"]
            if (before_crossing["next_owner"] != before_identity["simulation_owner"] or
                    after_crossing["prior_owner"] != before_identity["simulation_owner"] or
                    after_crossing["next_owner"] != after_identity["simulation_owner"]):
                raise _relaunch_error(f"receipts[{index}] crossing owner pair is ambiguous")
            for actor_id in actors:
                before_key = (actor_id, before_identity["world_id"], before_identity["activity_id"],
                              before_identity["generation"], before_identity["handoff_epoch"])
                after_key = (actor_id, after_identity["world_id"], after_identity["activity_id"],
                             after_identity["generation"], after_identity["handoff_epoch"])
                # A contiguous next row legitimately repeats the prior row's
                # after-state as its before-state.  Repeating an after-state
                # (or a before-state with no matching current owner) is a
                # duplicate claim and remains fail-closed.
                if after_key in claimed or (before_key in claimed and actor_id not in current):
                    raise _relaunch_error(f"receipts[{index}] duplicates an owner claim")
                claimed.update((before_key, after_key))
                observed_before = (before_identity["simulation_owner"], before_identity["activity_id"],
                                   before_identity["generation"], before_identity["handoff_epoch"])
                if actor_id in current and current[actor_id] != observed_before:
                    raise _relaunch_error(f"receipts[{index}] has stale or ambiguous owner state")
                current[actor_id] = (after_identity["simulation_owner"], after_identity["activity_id"],
                                     after_identity["generation"], after_identity["handoff_epoch"])
            normalized.append({"before": before, "after": after})
        return {
            "schema": "sole-owner-audit-v1", "status": "green",
            "actor_ids": list(actor_set or ()), "receipt_count": len(normalized),
            "owner_claim_count": len(claimed), "receipts": normalized,
        }
    except RelaunchReceiptError as exc:
        return {"schema": "sole-owner-audit-v1", "status": "red", "errors": [str(exc)]}


OBSERVATION_CREDIT = {
    "lifecycle": False,
    "qualification": False,
    "certification": False,
    "route_authority": False,
}


def _committed_receipts(report: Mapping[str, Any]) -> tuple[str, list[Mapping[str, Any]]]:
    transitions = _object(report.get("structured_transition_events"), "structured transition receipts")
    run_id = str(transitions.get("run_id", "")).strip()
    gates = transitions.get("gates")
    if not run_id or not isinstance(gates, list) or not gates:
        raise ProductionCaptureError("source report has no normal transition receipts")
    if not all(isinstance(gate, Mapping) and gate.get("status") == "green" for gate in gates):
        raise ProductionCaptureError("source report transition receipts are not all committed")
    if not any(
            isinstance(expectation, Mapping)
            and isinstance(expectation.get("predicate"), Mapping)
            and expectation["predicate"].get("outcome") == "committed"
            for gate in gates
            for expectation in gate.get("expectations", []) if isinstance(gate.get("expectations", []), list)
    ):
        raise ProductionCaptureError("source report has no committed normal transition receipt")
    return run_id, gates


def build_observation_source_report(
    *, report_path: Path, before_world: Path, after_world: Path,
) -> dict[str, Any]:
    """Bind a completed ordinary probe without asserting any registry credit."""
    report_path = report_path.resolve()
    try:
        report_bytes = report_path.read_bytes()
        report = json.loads(report_bytes.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductionCaptureError(f"source report is unreadable: {report_path}") from exc
    if not isinstance(report, Mapping):
        raise ProductionCaptureError("source report is not an object")
    if str(report.get("mode", "")).lower() != "probe" or report.get("diagnostic_replay") is True:
        raise ProductionCaptureError("observation source must be one ordinary probe")
    if report.get("wec_authority") or report.get("certification_round"):
        raise ProductionCaptureError("observation source is self-promoted with registry authority")

    scenario = _object(_object(report.get("scenario_manifest"), "scenario_manifest").get("source"), "scenario source")
    startup = _object(report.get("startup"), "startup")
    fixture = _object(startup.get("fixture_install"), "startup.fixture_install")
    profile = _object(startup.get("profile_snapshot"), "startup.profile_snapshot")
    if fixture.get("applied_save_transforms") not in (None, []):
        raise ProductionCaptureError("source report used save transforms")
    fixture_binding = _object(fixture.get("binding"), "fixture source binding")
    profile_binding = _object(profile.get("binding"), "profile source binding")
    screen = _object(startup.get("screen"), "startup.screen")
    runtime = _object(screen.get("runtime_binding_observed"), "runtime binding")
    run_id, gates = _committed_receipts(report)
    proof = _object(report.get("proof_classification"), "proof_classification")

    return {
        "schema": "production-observation-source-v1",
        "status": "green",
        "credit": dict(OBSERVATION_CREDIT),
        "source_probe": {
            "path": str(report_path),
            "sha256": _observation_probe_sha256(report),
            "run_id": run_id,
        },
        "scenario": {
            "path": str(Path(str(scenario.get("path", ""))).resolve()),
            "sha256": _sha256(scenario.get("sha256"), "source report scenario hash"),
        },
        "runtime": {
            "executable_path": str(runtime.get("executable_path", "")),
            "executable_sha256": _sha256(runtime.get("executable_sha256"), "source report executable hash"),
            "runtime_source_sha256": _sha256(runtime.get("runtime_source_sha256"), "source report runtime source hash"),
        },
        "fixture": {"binding": dict(fixture_binding), "source_chain": _chain(fixture.get("source_chain"), "fixture source chain")},
        "profile": {"binding": dict(profile_binding), "source_chain": _chain(profile.get("source_chain"), "profile source chain")},
        "committed_transition_receipts": {
            "run_id": run_id,
            "gate_ids": [str(gate.get("id", "")) for gate in gates],
            "receipt_count": len(gates),
        },
        "saved_world": {
            "before": {"path": str(before_world.resolve()), "tree_sha256": tree_sha256(before_world)},
            "after": {"path": str(after_world.resolve()), "tree_sha256": tree_sha256(after_world)},
        },
        "continuity": {
            "single_observation": True,
            "resumed": False,
            "rolled_back": False,
            "joined_segments": False,
            "replacement_identity": False,
        },
        "proof": dict(proof),
    }


def write_observation_source_report(
    *, report_path: Path, output_path: Path, before_world: Path, after_world: Path,
) -> dict[str, Any]:
    """Write a source report beside a real probe report; never touch the registry."""
    source = build_observation_source_report(
        report_path=report_path, before_world=before_world, after_world=after_world,
    )
    output_path = output_path.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(source, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return source


def write_observation_failure_report(
    *, report_path: Path, output_path: Path, reason: str,
    before_world: Path | None = None, after_world: Path | None = None,
) -> dict[str, Any]:
    """Preserve the first source-side divergence without manufacturing a route."""
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductionCaptureError(f"source report is unreadable: {report_path}") from exc
    if not isinstance(report, Mapping):
        raise ProductionCaptureError("source report is not an object")
    source = {
        "schema": "production-observation-source-v1",
        "status": "red",
        "credit": dict(OBSERVATION_CREDIT),
        "source_probe": {
            "path": str(report_path.resolve()),
            "sha256": _observation_probe_sha256(report),
            "run_id": str(_object(report.get("structured_transition_events"), "structured transition receipts").get("run_id", "")),
        },
        "first_divergence": str(reason),
    }
    scenario_manifest = report.get("scenario_manifest", {})
    startup = report.get("startup", {})
    transitions = report.get("structured_transition_events", {})
    if isinstance(scenario_manifest, Mapping) and isinstance(scenario_manifest.get("source"), Mapping):
        source["scenario"] = dict(scenario_manifest["source"])
    if isinstance(startup, Mapping):
        fixture = startup.get("fixture_install", {})
        profile = startup.get("profile_snapshot", {})
        screen = startup.get("screen", {})
        if isinstance(fixture, Mapping):
            source["fixture"] = {
                "binding": fixture.get("binding", {}),
                "source_chain": fixture.get("source_chain", []),
                "applied_save_transforms": fixture.get("applied_save_transforms", []),
            }
        if isinstance(profile, Mapping):
            source["profile"] = {
                "binding": profile.get("binding", {}),
                "source_chain": profile.get("source_chain", []),
            }
        if isinstance(screen, Mapping) and isinstance(screen.get("runtime_binding_observed"), Mapping):
            source["runtime"] = dict(screen["runtime_binding_observed"])
    if isinstance(transitions, Mapping):
        source["committed_transition_receipts"] = {
            "run_id": str(transitions.get("run_id", "")),
            "gate_count": len(transitions.get("gates", [])) if isinstance(transitions.get("gates"), list) else 0,
            "event_count": int(transitions.get("event_count", 0) or 0),
        }
    worlds: dict[str, Any] = {}
    for label, world in (("before", before_world), ("after", after_world)):
        if world is None:
            continue
        try:
            worlds[label] = {"path": str(world.resolve()), "tree_sha256": tree_sha256(world)}
        except ProductionCaptureError as exc:
            worlds[label] = {"path": str(world.resolve()), "error": str(exc)}
    if worlds:
        source["saved_world"] = worlds
    output_path = output_path.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(source, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return source


def validate_observation_source_report(
    *, source_path: Path, runtime_binding: Mapping[str, Any],
) -> dict[str, Any]:
    """Fail closed before an observation can be offered to canonical ingestion."""
    try:
        source = json.loads(source_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductionCaptureError(f"observation source is unreadable: {source_path}") from exc
    if not isinstance(source, Mapping) or source.get("schema") != "production-observation-source-v1":
        raise ProductionCaptureError("observation source is malformed")
    if source.get("status", "green") != "green":
        raise ProductionCaptureError("observation source is incomplete and cannot be ingested")
    if source.get("credit") != OBSERVATION_CREDIT:
        raise ProductionCaptureError("observation source attempts to grant credit")
    continuity = _object(source.get("continuity"), "observation continuity")
    for key in ("single_observation", "resumed", "rolled_back", "joined_segments", "replacement_identity"):
        expected = key == "single_observation"
        if continuity.get(key) is not expected:
            raise ProductionCaptureError(f"observation source continuity is invalid: {key}")
    probe = _object(source.get("source_probe"), "source probe")
    probe_path = Path(str(probe.get("path", ""))).resolve()
    if not probe_path.is_file():
        raise ProductionCaptureError("observation source probe was replaced or changed")
    try:
        probe_report = json.loads(probe_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductionCaptureError("observation source probe was replaced or changed") from exc
    if not isinstance(probe_report, Mapping) or _observation_probe_sha256(probe_report) != _sha256(probe.get("sha256"), "source probe hash"):
        raise ProductionCaptureError("observation source probe was replaced or changed")
    scenario = _object(source.get("scenario"), "observation scenario")
    scenario_path = Path(str(scenario.get("path", ""))).resolve()
    if not scenario_path.is_file() or _sha256_file(scenario_path) != _sha256(scenario.get("sha256"), "observation scenario hash"):
        raise ProductionCaptureError("observation scenario was replaced or changed")
    runtime = _object(source.get("runtime"), "observation runtime")
    if runtime_binding.get("ok") is not True:
        raise ProductionCaptureError("current runtime binding is unavailable")
    if _sha256(runtime_binding.get("executable_sha256"), "current executable hash") != _sha256(runtime.get("executable_sha256"), "observation executable hash"):
        raise ProductionCaptureError("current executable differs from observation source")
    if _sha256(runtime_binding.get("runtime_source_sha256"), "current runtime source hash") != _sha256(runtime.get("runtime_source_sha256"), "observation runtime source hash"):
        raise ProductionCaptureError("current runtime source differs from observation source")
    for label in ("before", "after"):
        world = _object(_object(source.get("saved_world"), "saved world").get(label), f"saved world {label}")
        world_path = Path(str(world.get("path", ""))).resolve()
        if tree_sha256(world_path) != _sha256(world.get("tree_sha256"), f"saved world {label} hash"):
            raise ProductionCaptureError(f"saved world {label} was replaced or changed")
    # Reconstructing the source from its immutable probe detects synthetic or
    # partial receipts even if the sidecar itself retained plausible hashes.
    reconstructed = build_observation_source_report(
        report_path=probe_path,
        before_world=Path(str(_object(_object(source.get("saved_world"), "saved world").get("before"), "saved world before").get("path", ""))),
        after_world=Path(str(_object(_object(source.get("saved_world"), "saved world").get("after"), "saved world after").get("path", ""))),
    )
    for key in ("scenario", "runtime", "fixture", "profile", "committed_transition_receipts", "saved_world", "continuity", "credit"):
        if source.get(key) != reconstructed.get(key):
            raise ProductionCaptureError(f"observation source {key} does not match its probe")
    return dict(source)


def prepare_production_capture(
    *, report_path: Path, scenario_path: Path, runtime_binding: Mapping[str, Any],
) -> dict[str, Any]:
    """Read the minimum source evidence needed to bind a normal capture.

    A green report is necessary but not enough.  It must contain committed
    normal transition receipts, no installed save transforms, and current
    runtime digests.  It deliberately returns zero credit.
    """
    try:
        report_bytes = report_path.read_bytes()
        report = json.loads(report_bytes.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductionCaptureError(f"source report is unreadable: {report_path}") from exc
    if not isinstance(report, Mapping):
        raise ProductionCaptureError("source report is not an object")

    source = _object(_object(report.get("scenario_manifest"), "scenario_manifest").get("source"), "scenario source")
    scenario_path = scenario_path.resolve()
    if str(Path(str(source.get("path", ""))).resolve()) != str(scenario_path):
        raise ProductionCaptureError("source report scenario path does not match capture scenario")
    scenario_sha256 = _sha256(source.get("sha256"), "source report scenario hash")
    if _sha256_file(scenario_path) != scenario_sha256:
        raise ProductionCaptureError("capture scenario bytes changed after the source report")

    proof = _object(report.get("proof_classification"), "proof_classification")
    if proof.get("status") != "green" or proof.get("feature_proof") is not True:
        raise ProductionCaptureError("source report is not green focused feature evidence")
    if str(proof.get("evidence_class", "")) not in {"feature-path", "focused feature proof"}:
        raise ProductionCaptureError("source report evidence class is not focused feature proof")
    if report.get("diagnostic_replay") is True or str(report.get("mode", "")).lower() in {"diagnostic_replay", "diagnostic-replay"}:
        raise ProductionCaptureError("diagnostic replay cannot produce a normal capture")

    startup = _object(report.get("startup"), "startup")
    fixture_install = _object(startup.get("fixture_install"), "startup.fixture_install")
    if fixture_install.get("applied_save_transforms") not in (None, []):
        raise ProductionCaptureError("source report used save transforms")
    fixture_chain = _chain(fixture_install.get("source_chain"), "fixture source chain")
    profile_snapshot = _object(startup.get("profile_snapshot"), "startup.profile_snapshot")
    profile_chain = _chain(profile_snapshot.get("source_chain"), "profile source chain")

    screen = _object(startup.get("screen"), "startup.screen")
    observed_runtime = _object(screen.get("runtime_binding_observed"), "runtime binding")
    if observed_runtime.get("status") != "matched":
        raise ProductionCaptureError("source report runtime binding is not matched")
    executable_sha256 = _sha256(observed_runtime.get("executable_sha256"), "source report executable hash")
    runtime_source_sha256 = _sha256(observed_runtime.get("runtime_source_sha256"), "source report runtime source hash")
    if runtime_binding.get("ok") is not True:
        raise ProductionCaptureError("current runtime binding is unavailable")
    if _sha256(runtime_binding.get("executable_sha256"), "current executable hash") != executable_sha256:
        raise ProductionCaptureError("current executable differs from the source report")
    if _sha256(runtime_binding.get("runtime_source_sha256"), "current runtime source hash") != runtime_source_sha256:
        raise ProductionCaptureError("current runtime source differs from the source report")

    if report.get("wec_authority") or report.get("certification_round"):
        raise ProductionCaptureError("source report is self-promoted with registry authority")
    run_id, gates = _committed_receipts(report)
    sole_owner_audit = audit_sole_owner_receipts(
        report.get("sole_owner_audit", report.get("ownership_receipts")),
        expected_run_id=run_id,
    )
    if sole_owner_audit.get("status") != "green":
        errors = "; ".join(str(item) for item in sole_owner_audit.get("errors", []))
        raise ProductionCaptureError(
            "source report sole-owner audit is not green" + (f": {errors}" if errors else "")
        )

    return {
        "schema": "production-origin-capture-v2",
        "credit": dict(OBSERVATION_CREDIT),
        "source_report": {"path": str(report_path.resolve()), "sha256": hashlib.sha256(report_bytes).hexdigest()},
        "scenario": {"path": str(scenario_path), "sha256": scenario_sha256},
        "runtime": {
            "executable_path": str(observed_runtime.get("executable_path", "")),
            "executable_sha256": executable_sha256,
            "runtime_source_sha256": runtime_source_sha256,
        },
        "fixture_source_chain": fixture_chain,
        "profile_source_chain": profile_chain,
        "normal_transition_receipts": {
            "run_id": run_id,
            "gate_ids": [str(gate.get("id", "")) for gate in gates],
            "receipt_count": len(gates),
        },
        "sole_owner_audit": sole_owner_audit,
    }


def capture_production_fixture(
    *, source_world: Path, fixture_dir: Path, production_origin: Mapping[str, Any], overwrite: bool,
) -> dict[str, Any]:
    """Copy one already-verified saved world and seal its before/after hashes."""
    source_world = source_world.resolve()
    fixture_dir = fixture_dir.resolve()
    source_sha256 = tree_sha256(source_world)
    if fixture_dir.exists() and not overwrite:
        raise ProductionCaptureError(f"fixture already exists: {fixture_dir}")
    if fixture_dir.exists():
        shutil.rmtree(fixture_dir)
    destination = fixture_dir / "save" / source_world.name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source_world, destination)
    destination_sha256 = tree_sha256(destination)
    if destination_sha256 != source_sha256:
        raise ProductionCaptureError("copied saved world hash does not match source world")
    provenance = dict(production_origin)
    provenance["saved_world"] = {
        "source_path": str(source_world),
        "source_tree_sha256": source_sha256,
        "fixture_tree_sha256": destination_sha256,
    }
    manifest = {
        "name": fixture_dir.name,
        "production_origin": provenance,
        "save_transforms": [],
    }
    (fixture_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest
