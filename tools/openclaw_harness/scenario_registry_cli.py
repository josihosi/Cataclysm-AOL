#!/usr/bin/env python3
"""Thin maintenance CLI for the authoritative SQLite scenario registry."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
from typing import Any, Dict, Mapping, Sequence

from scenario_registry import ManifestValidationError, validate_manifest
from scenario_registry_store import (
    BindingAdapters,
    RegistryLaunchToken,
    ScenarioRegistryStoreError,
    approve_retirement,
    claim_migration_item_launch,
    execute_registry_query,
    ingest_report_reference,
    ingest_token_linked_report_reference,
    migration_item_current,
    migration_run_snapshot,
    open_registry,
    quarantine_scenario,
    registry_status,
    record_migration_attempt,
    record_migration_terminal,
    retirement_candidates,
    execute_retirement_action,
    rebuild_manifest_projection,
    repository_root,
    resolve_registry_path,
    reconcile_report_bindings,
    record_selection_token_rejection,
    reload_selection_token_for_launch,
    parse_registry_query_request,
    path_sha256,
    record_migration_run_success,
    snapshot_migration_run,
)
import startup_harness
from startup_harness import (
    CLEANUP_ACCEPTED_STATUSES,
    resolve_fixture_payload,
    resolve_profile_snapshot_payload,
    runtime_source_binding,
    sha256_file,
)


class _ArgumentParser(argparse.ArgumentParser):
    """Keep command failures machine-readable as well as nonzero."""

    def error(self, message: str) -> None:
        _write_result({"ok": False, "error": message}, stream=sys.stderr)
        raise SystemExit(2)


def _write_result(value: Mapping[str, Any], *, stream: Any = sys.stdout) -> None:
    print(json.dumps(value, ensure_ascii=False, sort_keys=True), file=stream)


def _identity_sha256(label: str) -> str:
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def _runtime_adapter(expected: Mapping[str, Any]) -> Mapping[str, Any]:
    observed = expected.get("runtime_binding_observed", {})
    if not isinstance(observed, Mapping):
        observed = {}
    executable_text = str(observed.get("executable_path", "")).strip()
    expected_executable_sha256 = str(observed.get("executable_sha256", "")).strip().lower()
    expected_source_sha256 = str(observed.get("runtime_source_sha256", "")).strip().lower()
    current_source = runtime_source_binding()
    source_sha256 = str(current_source.get("sha256", "")).lower()
    facts: Dict[str, Any] = {"source_sha256": source_sha256 or _identity_sha256("runtime source unavailable")}
    if not executable_text:
        facts["reason"] = "runtime executable path is absent"
        return {"status": "stale", "facts": facts}
    executable_path = Path(executable_text)
    executable_sha256, executable_error = sha256_file(executable_path)
    facts.update({
        "executable_path": str(executable_path.resolve()),
        "executable_sha256": executable_sha256,
    })
    expected_status = str(expected.get("runtime_binding_status", "")).strip().lower()
    compatible = (
        expected_status in {"compatible", "matched"}
        and not executable_error
        and bool(expected_executable_sha256)
        and executable_sha256.lower() == expected_executable_sha256
        and bool(expected_source_sha256)
        and bool(current_source.get("ok"))
        and source_sha256 == expected_source_sha256
    )
    if not compatible:
        facts["reason"] = executable_error or str(current_source.get("error", "runtime binding mismatch"))
    return {"status": "compatible" if compatible else "stale", "facts": facts}


def _fixture_adapter(expected: Mapping[str, Any]) -> Mapping[str, Any]:
    fixture_name = str(expected.get("fixture", "")).strip()
    fixture_profile = str(expected.get("fixture_profile", "")).strip()
    if not fixture_name:
        return {"status": "compatible", "facts": {"source_sha256": _identity_sha256("fixture not requested")}}
    installed = expected.get("installed", {})
    if not isinstance(installed, Mapping):
        installed = {}
    try:
        resolved = resolve_fixture_payload(fixture_name, fixture_profile)
        source_path = Path(resolved["fixture_dir"])
        source_sha256 = path_sha256(source_path)
    except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        return {"status": "stale", "facts": {"source_sha256": _identity_sha256(f"fixture error:{exc}"), "reason": str(exc)}}
    expected_sha256 = str(installed.get("source_sha256", "")).strip().lower()
    compatible = (
        bool(expected_sha256)
        and source_sha256 == expected_sha256
        and str(installed.get("resolved_fixture", "")).strip() == str(resolved["fixture"])
        and str(installed.get("resolved_fixture_profile", "")).strip() == str(resolved["fixture_profile"])
    )
    return {
        "status": "compatible" if compatible else "stale",
        "facts": {"source_sha256": source_sha256, "source_path": str(source_path.resolve())},
    }


def _profile_adapter(expected: Mapping[str, Any]) -> Mapping[str, Any]:
    snapshot_name = str(expected.get("profile_snapshot", "")).strip()
    snapshot_profile = str(expected.get("profile_snapshot_profile", "")).strip()
    if not snapshot_name:
        return {"status": "compatible", "facts": {"source_sha256": _identity_sha256("profile snapshot not requested")}}
    installed = expected.get("snapshot_install", {})
    if not isinstance(installed, Mapping):
        installed = {}
    try:
        resolved = resolve_profile_snapshot_payload(snapshot_name, snapshot_profile)
        source_path = Path(resolved["snapshot_dir"])
        source_sha256 = path_sha256(source_path)
    except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        return {"status": "stale", "facts": {"source_sha256": _identity_sha256(f"profile error:{exc}"), "reason": str(exc)}}
    expected_sha256 = str(installed.get("source_sha256", "")).strip().lower()
    compatible = (
        bool(expected_sha256)
        and source_sha256 == expected_sha256
        and str(installed.get("source_path", "")).strip() == str(source_path.resolve())
        and str(installed.get("resolved_snapshot", "")).strip() == str(resolved["snapshot"])
        and str(installed.get("resolved_snapshot_profile", "")).strip() == str(resolved["snapshot_profile"])
    )
    return {
        "status": "compatible" if compatible else "stale",
        "facts": {"source_sha256": source_sha256, "source_path": str(source_path.resolve())},
    }


def production_binding_adapters() -> BindingAdapters:
    """Recompute report bindings from actual runtime, fixture, and profile owners."""
    return BindingAdapters(
        runtime=_runtime_adapter,
        fixture=_fixture_adapter,
        profile=_profile_adapter,
    )


def _default_scenarios_root() -> Path:
    return repository_root() / "tools" / "openclaw_harness" / "scenarios"


def _migration_profile(migration_run_id: str, attempt_identity: str) -> str:
    """Return the deterministic disposable profile for exactly one item identity."""
    return f"registry-migration-{migration_run_id}-{attempt_identity}"


def _migration_receipt(
    *,
    registry_path: Path,
    migration_run_id: str,
    attempt_identity: str,
    source_path: str,
    source_sha256: str,
    profile: str,
) -> Dict[str, str]:
    return {
        "schema": "migration-receipt-v1",
        "registry_path": str(registry_path.resolve()),
        "migration_run_id": migration_run_id,
        "attempt_identity": attempt_identity,
        "source_path": source_path,
        "source_sha256": source_sha256,
        "profile": profile,
    }


def _quarantine_migration_terminal(
    connection: sqlite3.Connection,
    *,
    migration_run_id: str,
    source_path: Path | str,
    source_sha256: str,
    disposition: str,
    reason: str,
    details: Mapping[str, Any] | None = None,
) -> Any:
    """Terminalize one migration outcome and make its current source nonselectable."""
    terminal_details = dict(details or {})
    current = record_migration_terminal(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
        disposition=disposition,
        reason=reason,
        details=terminal_details,
    )
    quarantine_reason = {
        "invalid": "invalid",
        "blocked": "blocked",
        "contradicted": "contradicted",
        "failed": "broken",
    }.get(disposition)
    if quarantine_reason is not None:
        quarantine_scenario(
            connection,
            reason=quarantine_reason,
            source_path=source_path,
            source_sha256=source_sha256,
            details={
                "migration_run_id": migration_run_id,
                "migration_disposition": disposition,
                "migration_reason": reason,
                "migration_details": terminal_details,
            },
        )
    return current


def _migration_report_terminal(
    receipt: Mapping[str, str],
    report_path: Path,
    report: Mapping[str, Any],
) -> tuple[str, str, Dict[str, Any]] | None:
    """Classify only one durable report bound to one frozen migration receipt."""
    scenario_manifest = report.get("scenario_manifest")
    source = scenario_manifest.get("source") if isinstance(scenario_manifest, Mapping) else None
    contract = report.get("contract")
    startup = report.get("startup")
    cleanup = report.get("cleanup")
    if not isinstance(source, Mapping) or not isinstance(cleanup, Mapping):
        return None
    report_profile = str(contract.get("profile", "")).strip() if isinstance(contract, Mapping) else ""
    if not report_profile and isinstance(startup, Mapping):
        report_profile = str(startup.get("profile", "")).strip()
    if (
        str(Path(str(source.get("path", ""))).resolve()) != receipt["source_path"]
        or str(source.get("sha256", "")).strip().lower() != receipt["source_sha256"]
        or report_profile != receipt["profile"]
        or str(cleanup.get("status", "")) not in CLEANUP_ACCEPTED_STATUSES
    ):
        return None
    proof = report.get("proof_classification")
    if not isinstance(proof, Mapping):
        proof = report.get("startup", {}).get("proof_classification", {}) if isinstance(report.get("startup"), Mapping) else {}
    status = str(proof.get("status", "")).strip().lower() if isinstance(proof, Mapping) else ""
    evidence_class = str(proof.get("evidence_class", "")).strip() if isinstance(proof, Mapping) else ""
    feature_proof = bool(proof.get("feature_proof", False)) if isinstance(proof, Mapping) else False
    if status == "green" and feature_proof and evidence_class == "feature-path":
        disposition, reason = "verified", "durable_feature_path_report"
    elif status == "red":
        disposition, reason = "contradicted", "durable_red_report"
    else:
        disposition, reason = "failed", "durable_non_green_report"
    try:
        report_bytes = report_path.read_bytes()
    except OSError:
        return None
    return disposition, reason, {
        "report_path": str(report_path.resolve()),
        "report_sha256": hashlib.sha256(report_bytes).hexdigest(),
        "proof_status": status,
        "proof_verdict": str(proof.get("verdict", "")) if isinstance(proof, Mapping) else "",
        "cleanup_status": str(cleanup.get("status", "")),
    }


def _reconcile_migration_report(
    connection: sqlite3.Connection,
    receipt: Mapping[str, str],
    report_path: Path,
) -> Dict[str, Any] | None:
    """Terminalize exactly one accepted-cleanup report, without registry report ingestion."""
    try:
        report_value = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return None
    if not isinstance(report_value, Mapping):
        return None
    terminal = _migration_report_terminal(receipt, report_path, report_value)
    if terminal is None:
        return None
    disposition, reason, details = terminal
    current = _quarantine_migration_terminal(
        connection,
        migration_run_id=receipt["migration_run_id"],
        source_path=receipt["source_path"],
        source_sha256=receipt["source_sha256"],
        disposition=disposition,
        reason=reason,
        details=details,
    )
    return {
        "status": current.status,
        "terminal_disposition": current.terminal_disposition,
        "report_path": details["report_path"],
    }


def _migration_post_finalize_reconcile(receipt: Mapping[str, str]) -> Any:
    """Return the narrow finalizer callback for one migration launch claim."""
    def reconcile(report_path: Path, _report: Mapping[str, Any]) -> Dict[str, Any]:
        try:
            connection = open_registry(receipt["registry_path"])
            try:
                return _reconcile_migration_report(connection, receipt, report_path) or {
                    "status": "not_terminalized",
                }
            finally:
                connection.close()
        except (OSError, sqlite3.Error, ScenarioRegistryStoreError, ValueError) as exc:
            return {"status": "reconcile_error", "error": str(exc)}

    return reconcile


def _reconcile_claimed_migration_item(
    connection: sqlite3.Connection,
    receipt: Mapping[str, str],
) -> Dict[str, Any] | None:
    """Resume from exactly one matching durable profile report; never relaunch a claim."""
    reports_root = startup_harness.userdir_for_profile(receipt["profile"]) / "harness_runs"
    if not reports_root.is_dir():
        return None
    matches: list[Path] = []
    for report_path in sorted(reports_root.glob("*/probe.report.json")):
        try:
            report_value = json.loads(report_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            continue
        if isinstance(report_value, Mapping) and _migration_report_terminal(receipt, report_path, report_value) is not None:
            matches.append(report_path)
    if len(matches) != 1:
        return None
    return _reconcile_migration_report(connection, receipt, matches[0])


def _dispatch_migration_item(
    connection: sqlite3.Connection,
    *,
    registry_path: Path,
    migration_run_id: str,
    source_path: Path,
    source_sha256: str,
) -> Dict[str, Any]:
    """Claim, classify, and dispatch one immutable item through canonical probe mode."""
    current = migration_item_current(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
    )
    profile = _migration_profile(migration_run_id, current.attempt_identity)
    receipt = _migration_receipt(
        registry_path=registry_path,
        migration_run_id=migration_run_id,
        attempt_identity=current.attempt_identity,
        source_path=current.source_path,
        source_sha256=current.source_sha256,
        profile=profile,
    )
    if current.terminal_disposition is not None:
        return {"source_path": current.source_path, "status": current.status, "action": "terminal"}
    if current.launch_claimed:
        reconciled = _reconcile_claimed_migration_item(connection, receipt)
        current = migration_item_current(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
        )
        return {
            "source_path": current.source_path,
            "status": current.status,
            "action": "reconciled" if reconciled is not None else "awaiting_terminal_evidence",
        }

    current = record_migration_attempt(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
    )
    try:
        source_bytes = source_path.read_bytes()
    except OSError as exc:
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="failed",
            reason="source_unreadable",
            details={"error": str(exc)},
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "failed"}
    observed_sha256 = hashlib.sha256(source_bytes).hexdigest()
    if observed_sha256 != current.source_sha256:
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="failed",
            reason="source_changed_after_snapshot",
            details={"observed_sha256": observed_sha256},
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "failed"}
    try:
        declaration = json.loads(source_bytes.decode("utf-8"))
        validation = validate_manifest(declaration, path=source_path)
    except (UnicodeError, json.JSONDecodeError, ManifestValidationError) as exc:
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="invalid",
            reason="manifest_parse_or_validation_failed",
            details={"error": str(exc)},
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "invalid"}
    if not isinstance(declaration, dict):
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="invalid",
            reason="manifest_top_level_not_object",
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "invalid"}
    blocker = startup_harness.scenario_blocker_info(declaration)
    if blocker["status"] == "blocked":
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="blocked",
            reason="declared_blocker",
            details=dict(blocker),
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "blocked"}
    validation_details = validation.get("validation", {})
    if isinstance(validation_details, Mapping) and bool(validation_details.get("review_required", False)):
        terminal = record_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="imported",
            reason="review_required_declaration",
            details={"validation": dict(validation_details)},
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "imported"}
    scenario_name = source_path.stem
    if startup_harness.scenario_path(scenario_name).resolve() != source_path.resolve():
        terminal = _quarantine_migration_terminal(
            connection,
            migration_run_id=migration_run_id,
            source_path=source_path,
            source_sha256=source_sha256,
            disposition="failed",
            reason="canonical_probe_source_mismatch",
        )
        return {"source_path": current.source_path, "status": terminal.status, "action": "failed"}
    current = claim_migration_item_launch(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
        launch_identity=f"canonical-probe:{current.attempt_identity}",
    )
    probe_namespace = startup_harness.build_parser().parse_args([
        "probe",
        scenario_name,
        "--profile",
        profile,
    ])
    probe_namespace.registry_migration_receipt = json.dumps(receipt, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    probe_namespace.registry_post_finalize_hook = _migration_post_finalize_reconcile(receipt)
    result = startup_harness.run_probe_mode(probe_namespace)
    current = migration_item_current(
        connection,
        migration_run_id=migration_run_id,
        source_path=source_path,
        source_sha256=source_sha256,
    )
    return {
        "source_path": current.source_path,
        "status": current.status,
        "action": "dispatched",
        "runner_exit_code": result,
    }


def _migration_item_is_executable(source_path: Path, source_sha256: str) -> bool:
    """Classify final-set executability from the same structured owners as dispatch."""
    try:
        source_bytes = source_path.read_bytes()
        if hashlib.sha256(source_bytes).hexdigest() != source_sha256:
            return False
        declaration = json.loads(source_bytes.decode("utf-8"))
        validation = validate_manifest(declaration, path=source_path)
    except (OSError, UnicodeError, json.JSONDecodeError, ManifestValidationError):
        return False
    if not isinstance(declaration, dict):
        return False
    blocker = startup_harness.scenario_blocker_info(declaration)
    validation_details = validation.get("validation", {})
    return (
        blocker["status"] != "blocked"
        and isinstance(validation_details, Mapping)
        and not bool(validation_details.get("review_required", True))
    )


def _terminalize_removed_migration_items(
    connection: sqlite3.Connection,
    migration: Any,
    final_identities: set[tuple[str, str]],
) -> None:
    """Close superseded or removed snapshot identities without launching them."""
    for item in migration.items:
        identity = (item.source_path, item.source_sha256)
        if identity in final_identities:
            continue
        current = migration_item_current(
            connection,
            migration_run_id=migration.migration_run_id,
            source_path=item.source_path,
            source_sha256=item.source_sha256,
        )
        if current.terminal_disposition is None:
            record_migration_terminal(
                connection,
                migration_run_id=migration.migration_run_id,
                source_path=item.source_path,
                source_sha256=item.source_sha256,
                disposition="failed",
                reason="source_removed_during_migration",
                details={"source_path": item.source_path, "source_sha256": item.source_sha256},
            )


def _migration_completion_summary(
    connection: sqlite3.Connection,
    migration: Any,
    final_items: Sequence[Any],
) -> Dict[str, Any]:
    """Derive completion solely from the final filesystem identity set and item history."""
    final_identities = {(item.source_path, item.source_sha256) for item in final_items}
    all_items = migration_run_snapshot(connection, migration.migration_run_id).items
    current_by_identity = {
        (item.source_path, item.source_sha256): migration_item_current(
            connection,
            migration_run_id=migration.migration_run_id,
            source_path=item.source_path,
            source_sha256=item.source_sha256,
        )
        for item in all_items
    }
    terminal_final_identities = {
        identity for identity in final_identities
        if current_by_identity[identity].terminal_disposition is not None
    }
    terminal_rows = sum(
        1
        for current in current_by_identity.values()
        for event in current.history
        if event.event_kind == "terminal"
    )
    disposition_counts: Dict[str, int] = {}
    non_verified = []
    unfinished = []
    for identity, current in sorted(current_by_identity.items()):
        terminal_events = [event for event in current.history if event.event_kind == "terminal"]
        if current.terminal_disposition is None:
            unfinished.append({"source_path": identity[0], "source_sha256": identity[1], "status": current.status})
            continue
        disposition = current.terminal_disposition
        disposition_counts[disposition] = disposition_counts.get(disposition, 0) + 1
        if disposition != "verified":
            terminal_event = terminal_events[-1]
            non_verified.append({
                "source_path": identity[0],
                "source_sha256": identity[1],
                "disposition": disposition,
                "reason": terminal_event.reason,
            })

    executable_identities = {
        identity for identity in final_identities
        if _migration_item_is_executable(Path(identity[0]), identity[1])
    }
    non_executable_identities = final_identities - executable_identities
    launch_counts = {
        identity: sum(
            1 for event in current_by_identity[identity].history
            if event.event_kind == "launch_claimed"
        )
        for identity in final_identities
    }
    executable_once = all(launch_counts[identity] == 1 for identity in executable_identities)
    non_executable_zero = all(launch_counts[identity] == 0 for identity in non_executable_identities)
    final_set_equals_terminal_set = final_identities == terminal_final_identities
    completion_ready = (
        final_set_equals_terminal_set
        and not unfinished
        and executable_once
        and non_executable_zero
    )
    return {
        "filesystem_totals": {
            "identities": len(final_identities),
            "executable": len(executable_identities),
            "non_executable": len(non_executable_identities),
        },
        "terminal_rows": terminal_rows,
        "executable_attempts": sum(launch_counts[identity] for identity in executable_identities),
        "disposition_counts": dict(sorted(disposition_counts.items())),
        "non_verified": non_verified,
        "final_set_equals_terminal_set": final_set_equals_terminal_set,
        "every_executable_once": executable_once,
        "non_executable_zero_launches": non_executable_zero,
        "once_only_launches": executable_once and non_executable_zero,
        "unfinished": unfinished,
        "completion_ready": completion_ready,
    }


def _reconcile_migration_final_set(
    connection: sqlite3.Connection,
    *,
    registry_path: Path,
    migration: Any,
) -> Dict[str, Any]:
    """Re-enumerate, process newly visible identities, then prove or withhold completion."""
    root = Path(migration.scenarios_root)
    first_final = snapshot_migration_run(
        connection,
        root,
        launcher_identity="scenario_registry_cli.registry-migrate-all",
    )
    first_final_identities = {(item.source_path, item.source_sha256) for item in first_final.items}
    _terminalize_removed_migration_items(
        connection,
        migration_run_snapshot(connection, migration.migration_run_id),
        first_final_identities,
    )
    for item in first_final.items:
        _dispatch_migration_item(
            connection,
            registry_path=registry_path,
            migration_run_id=migration.migration_run_id,
            source_path=Path(item.source_path),
            source_sha256=item.source_sha256,
        )

    final_check = snapshot_migration_run(
        connection,
        root,
        launcher_identity="scenario_registry_cli.registry-migrate-all",
    )
    final_identities = {(item.source_path, item.source_sha256) for item in final_check.items}
    _terminalize_removed_migration_items(
        connection,
        migration_run_snapshot(connection, migration.migration_run_id),
        final_identities,
    )
    summary = _migration_completion_summary(connection, migration, final_check.items)
    if summary["completion_ready"]:
        summary["success_event"] = record_migration_run_success(
            connection,
            migration_run_id=migration.migration_run_id,
            summary=summary,
        )
    return summary


def _load_query_request(args: argparse.Namespace) -> Mapping[str, Any]:
    if args.query_json is not None:
        source = args.query_json
    else:
        try:
            source = Path(args.query_file).read_text(encoding="utf-8")
        except OSError as exc:
            raise ScenarioRegistryStoreError(f"Could not read query file {args.query_file}: {exc}") from exc
    try:
        value = json.loads(source)
    except json.JSONDecodeError as exc:
        raise ScenarioRegistryStoreError(f"Registry query must be valid JSON: {exc}") from exc
    if not isinstance(value, Mapping):
        raise ScenarioRegistryStoreError("Registry query top level must be an object")
    return value


def _registry_launch_probe_namespace(selection: RegistryLaunchToken) -> argparse.Namespace:
    """Adapt one validated registry selection into the ordinary probe parser."""
    source_path = Path(selection.source_path).resolve()
    canonical_path = startup_harness.scenario_path(selection.scenario).resolve()
    if canonical_path != source_path:
        raise ScenarioRegistryStoreError(
            "Selected scenario source is not the canonical probe manifest: "
            f"{source_path}"
        )
    return startup_harness.build_parser().parse_args(["probe", selection.scenario])


def _registry_post_finalize_ingest(receipt: str) -> Any:
    """Return the narrow callback that links a durable probe report to its token."""
    def ingest(report_path: Path, _report: Mapping[str, Any]) -> Dict[str, Any]:
        try:
            payload = json.loads(receipt)
            if not isinstance(payload, Mapping):
                raise ScenarioRegistryStoreError("registry launch receipt must be an object")
            registry_path = Path(str(payload["registry_path"])).resolve()
            token_id = str(payload["token_id"]).strip()
            if not token_id:
                raise ScenarioRegistryStoreError("registry launch receipt token is missing")
            connection = open_registry(str(registry_path))
            try:
                return ingest_token_linked_report_reference(
                    connection,
                    token_id,
                    report_path,
                    adapters=production_binding_adapters(),
                )
            finally:
                connection.close()
        except (KeyError, OSError, sqlite3.Error, ScenarioRegistryStoreError, ValueError) as exc:
            return {"status": "ingest_error", "error": str(exc)}

    return ingest


def build_parser() -> argparse.ArgumentParser:
    parser = _ArgumentParser(description=__doc__)
    parser.add_argument("--registry", help="SQLite registry path; defaults to the shared harness registry")
    commands = parser.add_subparsers(dest="command", required=True, parser_class=_ArgumentParser)
    rebuild = commands.add_parser("rebuild", help="project scenario declarations into the registry")
    rebuild.add_argument(
        "--scenarios-root",
        default=str(_default_scenarios_root()),
        help="scenario manifest directory (default: canonical harness scenarios directory)",
    )
    ingest = commands.add_parser("ingest-report", help="ingest one immutable report reference")
    ingest.add_argument("--report", required=True, help="full probe or handoff report JSON path")
    commands.add_parser("reconcile", help="recompute report bindings from their authoritative owners")
    query = commands.add_parser("registry-query", help="evaluate typed requirements without launching the harness")
    query_source = query.add_mutually_exclusive_group(required=True)
    query_source.add_argument("--query-file", help="typed registry-query JSON file")
    query_source.add_argument("--query-json", help="typed registry-query JSON object")
    query.add_argument(
        "--include-state",
        action="append",
        choices=("quarantined", "retired"),
        default=[],
        help="include inspect-only lifecycle state; repeated values are accepted",
    )
    status = commands.add_parser("registry-status", help="inspect registry lifecycle, relation, and retirement history")
    status.add_argument(
        "--include-state",
        action="append",
        choices=("quarantined", "retired"),
        default=[],
        help="include non-active lifecycle rows; repeated values are accepted",
    )
    commands.add_parser("retirement-candidates", help="inspect review-only retirement candidates")
    approve = commands.add_parser("approve-retirement", help="prepare one reviewer-approved SHA-bound retirement")
    approve.add_argument("--manifest-id", required=True)
    approve.add_argument("--successor-manifest-id", required=True)
    approve.add_argument("--source-sha256", required=True)
    approve.add_argument("--reason", required=True)
    approve.add_argument("--reviewer", required=True)
    approve.add_argument("--approval", required=True, help="must be the literal value 'approved'")
    action = commands.add_parser("retirement-action", help="resume one approved retirement source removal")
    action.add_argument("action_id")
    launch = commands.add_parser(
        "registry-launch",
        help="reload one selection token and run its canonical probe route",
    )
    launch.add_argument("selection_token", help="selection token returned by registry-query")
    migrate = commands.add_parser(
        "registry-migrate-all",
        help="claim and classify one immutable scenario inventory snapshot",
    )
    migrate.add_argument(
        "--scenarios-root",
        default=str(_default_scenarios_root()),
        help="scenario manifest directory (default: canonical harness scenarios directory)",
    )
    migrate.add_argument(
        "--resume",
        metavar="MIGRATION_ID",
        help="resume one existing immutable migration run without a second launch claim",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    registry_path = resolve_registry_path(args.registry)
    probe_namespace: argparse.Namespace | None = None
    try:
        connection = open_registry(str(registry_path))
        try:
            if args.command == "rebuild":
                result = rebuild_manifest_projection(connection, Path(args.scenarios_root))
            elif args.command == "ingest-report":
                result = ingest_report_reference(
                    connection,
                    Path(args.report),
                    adapters=production_binding_adapters(),
                )
            elif args.command == "reconcile":
                result = reconcile_report_bindings(connection, adapters=production_binding_adapters())
            elif args.command == "registry-query":
                request = parse_registry_query_request(_load_query_request(args))
                result = asdict(execute_registry_query(
                    connection,
                    request,
                    include_lifecycle_states=tuple(args.include_state),
                    drafts_root=registry_path.parent / "drafts",
                ))
            elif args.command == "registry-status":
                result = {"entries": registry_status(
                    connection,
                    include_lifecycle_states=tuple(args.include_state),
                )}
            elif args.command == "retirement-candidates":
                result = {"candidates": retirement_candidates(connection)}
            elif args.command == "approve-retirement":
                result = approve_retirement(
                    connection,
                    manifest_id=args.manifest_id,
                    successor_manifest_id=args.successor_manifest_id,
                    source_sha256=args.source_sha256,
                    reason=args.reason,
                    reviewer_identity=args.reviewer,
                    approval=args.approval,
                )
            elif args.command == "retirement-action":
                result = execute_retirement_action(connection, args.action_id)
            elif args.command == "registry-launch":
                selection = reload_selection_token_for_launch(connection, args.selection_token)
                if not selection.accepted:
                    result = asdict(selection)
                else:
                    try:
                        probe_namespace = _registry_launch_probe_namespace(selection)
                    except ScenarioRegistryStoreError as exc:
                        record_selection_token_rejection(
                            connection,
                            selection.token_id,
                            reason="canonical_probe_source_mismatch",
                            details={"error": str(exc)},
                        )
                        result = asdict(RegistryLaunchToken(
                            token_id=selection.token_id,
                            accepted=False,
                            reason="canonical_probe_source_mismatch",
                        ))
                    else:
                        runtime_binding = startup_harness.build_runtime_binding(
                            startup_harness.detect_executable()
                        )
                        if not runtime_binding.get("ok"):
                            record_selection_token_rejection(
                                connection,
                                selection.token_id,
                                reason="runtime_binding_unavailable",
                                details={"error": str(runtime_binding.get("error", "unknown error"))},
                            )
                            result = asdict(RegistryLaunchToken(
                                token_id=selection.token_id,
                                accepted=False,
                                reason="runtime_binding_unavailable",
                            ))
                            probe_namespace = None
                        else:
                            probe_namespace.registry_launch_receipt = json.dumps({
                                "schema": 1,
                                "registry_path": str(registry_path),
                                "token_id": selection.token_id,
                                "source_path": selection.source_path,
                                "runtime_binding": runtime_binding,
                            }, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
                            probe_namespace.registry_post_finalize_hook = _registry_post_finalize_ingest(
                                probe_namespace.registry_launch_receipt
                            )
                            result = asdict(selection)
            elif args.command == "registry-migrate-all":
                if args.resume:
                    migration = migration_run_snapshot(connection, str(args.resume))
                else:
                    migration = snapshot_migration_run(
                        connection,
                        Path(args.scenarios_root),
                        launcher_identity="scenario_registry_cli.registry-migrate-all",
                    )
                item_results = []
                for item in migration.items:
                    item_results.append(_dispatch_migration_item(
                        connection,
                        registry_path=registry_path,
                        migration_run_id=migration.migration_run_id,
                        source_path=Path(item.source_path),
                        source_sha256=item.source_sha256,
                    ))
                summary = _reconcile_migration_final_set(
                    connection,
                    registry_path=registry_path,
                    migration=migration,
                )
                result = {
                    "migration_run_id": migration.migration_run_id,
                    "run_identity": migration.run_identity,
                    "scenarios_root": migration.scenarios_root,
                    "resumed": bool(args.resume),
                    "items": item_results,
                    "summary": summary,
                }
            else:
                raise ScenarioRegistryStoreError(f"Unsupported registry command: {args.command}")
        finally:
            connection.close()
    except (OSError, sqlite3.Error, ScenarioRegistryStoreError, SystemExit, ValueError) as exc:
        _write_result({"ok": False, "command": args.command, "error": str(exc)}, stream=sys.stderr)
        return 1
    if args.command == "registry-launch":
        if probe_namespace is None:
            _write_result({
                "ok": False,
                "command": args.command,
                "registry": str(registry_path),
                "result": result,
            }, stream=sys.stderr)
            return 1
        return startup_harness.run_probe_mode(probe_namespace)
    _write_result({
        "ok": True,
        "command": args.command,
        "registry": str(registry_path),
        "result": result,
    })
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
