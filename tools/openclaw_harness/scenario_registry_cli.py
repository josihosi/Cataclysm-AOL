#!/usr/bin/env python3
"""Thin maintenance CLI for the authoritative SQLite scenario registry."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import traceback
import uuid
from typing import Any, Dict, Mapping, Optional, Sequence

from scenario_registry import ManifestValidationError, validate_manifest
from scenario_registry_store import (
    BindingAdapters,
    RegistryBootstrapToken,
    RegistryLaunchToken,
    RegistryRepairToken,
    ScenarioRegistryStoreError,
    approve_retirement,
    claim_migration_item_launch,
    create_certification_round,
    execute_registry_query,
    final_gate_eligibility,
    issue_wec_authority,
    issue_registry_bootstrap_token,
    issue_registry_repair_token,
    ingest_report_reference,
    ingest_bootstrap_token_linked_report_reference,
    ingest_repair_compatibility_terminal,
    ingest_r019_current_source_repair_successor,
    issue_r019_aggregation_token,
    finalize_r019_aggregation_token,
    ingest_repair_token_linked_report_reference,
    ingest_token_linked_report_reference,
    migration_item_current,
    migration_run_snapshot,
    open_registry,
    prepare_windows_feel_handoff,
    quarantine_scenario,
    registry_query_repair_action,
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
    record_bootstrap_token_rejection,
    record_repair_token_rejection,
    terminalize_repair_token_cleanup_without_report,
    claim_bootstrap_token_for_launch,
    claim_repair_token_for_launch,
    reload_bootstrap_token_for_launch,
    reload_repair_token_for_launch,
    reload_selection_token_for_launch,
    parse_registry_query_request,
    path_sha256,
    record_migration_run_success,
    record_playtest_witness,
    record_windows_feel_judgment,
    review_playtest_witness,
    revalidate_current_bootstrap_authority,
    snapshot_migration_run,
    windows_feel_handoff_status,
)
import startup_harness
import production_capture
from command_receipts import read_command_artifact, write_command_artifact
from registry_query_output import query_page
from startup_harness import (
    CLEANUP_ACCEPTED_STATUSES,
    fixture_source_binding,
    profile_snapshot_source_binding,
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


def _write_result(value: Mapping[str, Any], *, stream: Optional[Any] = None) -> None:
    """Write JSON to the caller's current stdout unless an explicit stream is supplied."""
    print(json.dumps(value, ensure_ascii=False, sort_keys=True), file=sys.stdout if stream is None else stream)


def _positive_page_size(value: str) -> int:
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("page size must be positive")
    return number


def _query_launch_action(result: Mapping[str, Any], args: argparse.Namespace,
                         registry_path: Path) -> Mapping[str, Any] | None:
    if result.get("next_action") is not None:
        return result["next_action"]
    ranked = result["evaluation"]["evaluation"]["ranked_scenario_ids"]
    if not result.get("token_id"):
        return {"kind": "inspect_query_fit", "draft_path": result.get("draft_path"),
                "action": "Inspect candidate fit and missing evidence; refine the query or repair the route."}
    readiness = result.get("source_executable_readiness", {})
    if readiness.get("status") != "ready":
        return {"kind": "resolve_runtime_readiness", "reason": readiness.get("reason"),
                "action": readiness.get("next_action"),
                "evidence_ceiling": readiness.get("evidence_ceiling", "none")}
    selected = next(item for item in result["evaluation"]["candidates"]
                    if item["scenario_id"] == ranked[0])
    manifest = selected["explanation"]["manifest"]
    source = Path(manifest["source_path"])
    try:
        declaration = json.loads(source.read_text(encoding="utf-8"))
        detached = any(isinstance(step, Mapping) and step.get("kind") == "cockpit_live_session"
                       for step in declaration.get("steps", []))
    except (OSError, ValueError, AttributeError) as error:
        return {"kind": "inspect_selected_declaration", "source_path": str(source),
                "reason": str(error)}
    charter = str(args.witness_charter or "").strip()
    if detached and not charter:
        return {"kind": "provide_witness_charter", "scenario_id": ranked[0],
                "action": "Supply the matching witness charter for the selected live-cockpit launch."}
    command = [sys.executable, str(Path(__file__).resolve()), "--registry", str(registry_path),
               "registry-detached-launch" if detached else "registry-launch", result["token_id"]]
    if charter:
        command.extend(["--witness-charter", str(Path(charter).resolve())])
    if detached:
        session = registry_path.parent / "bridge-sessions" / ("selected-" + uuid.uuid4().hex)
        command.extend(["--session-dir", str(session)])
    return {"kind": "launch_selected_scenario", "command": {"argv": command},
            "precondition": "Launch revalidates the selected token and bindings; a detached session "
                            "directory must not exist before launch."}


def _identity_sha256(label: str) -> str:
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def _runtime_adapter(
    expected: Mapping[str, Any], *, observation_cache: Optional[Dict[str, Any]] = None,
) -> Mapping[str, Any]:
    observed = expected.get("runtime_binding_observed", {})
    if not isinstance(observed, Mapping):
        observed = {}
    executable_text = str(observed.get("executable_path", "")).strip()
    expected_executable_sha256 = str(observed.get("executable_sha256", "")).strip().lower()
    expected_source_sha256 = str(observed.get("runtime_source_sha256", "")).strip().lower()
    cache = observation_cache if observation_cache is not None else {}
    current_source = cache.get("current_source")
    if not isinstance(current_source, Mapping):
        current_source = runtime_source_binding()
        cache["current_source"] = current_source
    source_sha256 = str(current_source.get("sha256", "")).lower()
    facts: Dict[str, Any] = {"source_sha256": source_sha256 or _identity_sha256("runtime source unavailable")}
    if not executable_text:
        facts["reason"] = "runtime executable path is absent"
        return {"status": "stale", "facts": facts}
    executable_path = Path(executable_text)
    executable_observations = cache.setdefault("executables", {})
    cached_executable = executable_observations.get(str(executable_path.resolve()))
    if cached_executable is None:
        cached_executable = sha256_file(executable_path)
        executable_observations[str(executable_path.resolve())] = cached_executable
    executable_sha256, executable_error = cached_executable
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


def _fixture_current_observation(
    fixture_name: str, fixture_profile: str, *, observation_cache: Dict[str, Any],
) -> Mapping[str, Any]:
    """Observe one fixture owner once; expected bindings are compared by each caller."""
    observations = observation_cache.setdefault("fixtures", {})
    request_identity = (fixture_name, fixture_profile)
    aliases = observation_cache.setdefault("fixture_aliases", {})
    if request_identity in aliases:
        return observations[aliases[request_identity]]
    try:
        resolved = resolve_fixture_payload(fixture_name, fixture_profile)
        identity = (
            str(resolved["fixture"]), str(resolved["fixture_profile"]),
        )
        if identity in observations:
            aliases[request_identity] = identity
            return observations[identity]
        source_path = Path(resolved["fixture_dir"])
        current_binding = fixture_source_binding(
            fixture_name,
            fixture_profile,
            content_identity_cache=observation_cache.setdefault("directory_content_identities", {}),
        )
        observation = {
            "status": "observed",
            "source_sha256": current_binding["sha256"],
            "source_path": str(source_path.resolve()),
            "binding": current_binding,
            "resolved": resolved,
        }
    except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        identity = (fixture_name, fixture_profile)
        observation = {
            "status": "stale",
            "source_sha256": _identity_sha256(f"fixture error:{exc}"),
            "reason": str(exc),
        }
    observations[identity] = observation
    aliases[request_identity] = identity
    return observation


def _fixture_adapter(
    expected: Mapping[str, Any], *, observation_cache: Optional[Dict[str, Any]] = None,
) -> Mapping[str, Any]:
    fixture_name = str(expected.get("fixture", "")).strip()
    fixture_profile = str(expected.get("fixture_profile", "")).strip()
    if not fixture_name:
        return {"status": "compatible", "facts": {"source_sha256": _identity_sha256("fixture not requested")}}
    cache = observation_cache if observation_cache is not None else {}
    installed = expected.get("installed", {})
    if not isinstance(installed, Mapping):
        installed = {}
    binding = installed.get("binding")
    observation = _fixture_current_observation(
        fixture_name, fixture_profile, observation_cache=cache,
    )
    if observation.get("status") != "observed":
        return {"status": "stale", "facts": dict(observation)}
    resolved = observation["resolved"]
    source_path = Path(str(observation["source_path"]))
    source_sha256 = str(observation["source_sha256"])
    current_binding = observation["binding"]
    if isinstance(binding, Mapping):
        expected_binding_sha256 = str(binding.get("sha256", "")).strip().lower()
        compatible = bool(expected_binding_sha256) and current_binding["sha256"] == expected_binding_sha256
        return {
            "status": "compatible" if compatible else "stale",
            "facts": {
                "source_sha256": current_binding["sha256"],
                "source_path": str(source_path.resolve()),
                "binding": current_binding,
                **({} if compatible else {"reason": "fixture source-chain binding mismatch"}),
            },
        }
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


def _profile_current_observation(
    snapshot_name: str, snapshot_profile: str, *, observation_cache: Dict[str, Any],
) -> Mapping[str, Any]:
    """Observe one profile snapshot owner once; compare expected bindings separately."""
    observations = observation_cache.setdefault("profiles", {})
    request_identity = (snapshot_name, snapshot_profile)
    aliases = observation_cache.setdefault("profile_aliases", {})
    if request_identity in aliases:
        return observations[aliases[request_identity]]
    try:
        resolved = resolve_profile_snapshot_payload(snapshot_name, snapshot_profile)
        identity = (
            str(resolved["snapshot"]), str(resolved["snapshot_profile"]),
        )
        if identity in observations:
            aliases[request_identity] = identity
            return observations[identity]
        source_path = Path(resolved["snapshot_dir"])
        current_binding = profile_snapshot_source_binding(
            snapshot_name,
            snapshot_profile,
            content_identity_cache=observation_cache.setdefault("directory_content_identities", {}),
        )
        observation = {
            "status": "observed",
            "source_sha256": current_binding["sha256"],
            "source_path": str(source_path.resolve()),
            "binding": current_binding,
            "resolved": resolved,
        }
    except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        identity = (snapshot_name, snapshot_profile)
        observation = {
            "status": "stale",
            "source_sha256": _identity_sha256(f"profile error:{exc}"),
            "reason": str(exc),
        }
    observations[identity] = observation
    aliases[request_identity] = identity
    return observation


def _profile_adapter(
    expected: Mapping[str, Any], *, observation_cache: Optional[Dict[str, Any]] = None,
) -> Mapping[str, Any]:
    snapshot_name = str(expected.get("profile_snapshot", "")).strip()
    snapshot_profile = str(expected.get("profile_snapshot_profile", "")).strip()
    if not snapshot_name:
        return {"status": "compatible", "facts": {"source_sha256": _identity_sha256("profile snapshot not requested")}}
    cache = observation_cache if observation_cache is not None else {}
    installed = expected.get("snapshot_install", {})
    if not isinstance(installed, Mapping):
        installed = {}
    binding = installed.get("binding")
    observation = _profile_current_observation(
        snapshot_name, snapshot_profile, observation_cache=cache,
    )
    if observation.get("status") != "observed":
        return {"status": "stale", "facts": dict(observation)}
    resolved = observation["resolved"]
    source_path = Path(str(observation["source_path"]))
    source_sha256 = str(observation["source_sha256"])
    current_binding = observation["binding"]
    if isinstance(binding, Mapping):
        expected_binding_sha256 = str(binding.get("sha256", "")).strip().lower()
        compatible = bool(expected_binding_sha256) and current_binding["sha256"] == expected_binding_sha256
        return {
            "status": "compatible" if compatible else "stale",
            "facts": {
                "source_sha256": current_binding["sha256"],
                "source_path": str(source_path.resolve()),
                "binding": current_binding,
                **({} if compatible else {"reason": "profile source-chain binding mismatch"}),
            },
        }
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
    runtime_observation_cache: Dict[str, Any] = {}
    # This map belongs to exactly one reconciliation invocation.  Never retain it
    # beyond these adapters: a later observation must notice source changes.
    directory_content_identities: Dict[str, str] = {}
    fixture_observation_cache: Dict[str, Any] = {
        "directory_content_identities": directory_content_identities,
    }
    profile_observation_cache: Dict[str, Any] = {
        "directory_content_identities": directory_content_identities,
    }
    return BindingAdapters(
        runtime=lambda expected: _runtime_adapter(expected, observation_cache=runtime_observation_cache),
        fixture=lambda expected: _fixture_adapter(expected, observation_cache=fixture_observation_cache),
        profile=lambda expected: _profile_adapter(expected, observation_cache=profile_observation_cache),
    )


def _current_bootstrap_revalidation_facts(declaration: Mapping[str, Any]) -> Mapping[str, Any]:
    """Observe the live owners used by a stale-route first-run release."""
    runtime = startup_harness.build_runtime_binding(startup_harness.detect_executable())

    def current_payload(kind: str, name_key: str, profile_key: str, resolver: Any) -> Mapping[str, Any]:
        name = str(declaration.get(name_key, "")).strip()
        profile = str(declaration.get(profile_key, "")).strip()
        if not name:
            return {
                "status": "compatible",
                "name": "",
                "profile": profile,
                "source_path": "",
                "source_sha256": _identity_sha256(
                    "fixture not requested" if kind == "fixture" else "profile snapshot not requested"
                ),
            }
        try:
            resolved = resolver(name, profile)
            source_path = Path(
                resolved["fixture_dir"] if kind == "fixture" else resolved["snapshot_dir"]
            )
            binding = (
                fixture_source_binding(name, profile)
                if kind == "fixture"
                else profile_snapshot_source_binding(name, profile)
            )
            return {
                "status": "compatible",
                "name": name,
                "profile": profile,
                "source_path": str(source_path.resolve()),
                "source_sha256": binding["sha256"],
                "binding": binding,
            }
        except (KeyError, OSError, SystemExit, ScenarioRegistryStoreError) as exc:
            return {"status": "stale", "reason": str(exc)}

    return {
        "runtime": runtime,
        "fixture": current_payload(
            "fixture", "fixture", "fixture_profile", resolve_fixture_payload,
        ),
        "profile": current_payload(
            "profile", "profile_snapshot", "profile_snapshot_profile", resolve_profile_snapshot_payload,
        ),
    }


def _current_repair_binding(declaration: Mapping[str, Any]) -> Mapping[str, Any]:
    """Observe the exact runtime, fixture, and profile footing for one repair token."""
    return _current_bootstrap_revalidation_facts(declaration)


def _current_source_executable_readiness(
    *,
    isolated_harness_diagnosis: bool = False,
    executable: str = "",
) -> Mapping[str, Any]:
    """Observe one actionable source/executable status without launching gameplay."""
    executable_text = str(executable).strip()
    if executable_text:
        candidate = Path(executable_text).expanduser()
    else:
        try:
            candidate = startup_harness.detect_executable()
        except SystemExit as exc:
            return {
                "status": "build_required",
                "reason": "runnable_executable_absent",
                "next_action": (
                    "build or select a source-matching executable, then repeat the same registry query"
                ),
                "evidence_ceiling": "none until source-matching executable revalidation",
                "comparison_error": str(exc),
            }
    return startup_harness.executable_source_readiness(
        candidate,
        isolated_harness_diagnosis=isolated_harness_diagnosis,
    )


def _runtime_status(*, executable: str, isolated_harness_diagnosis: bool) -> Mapping[str, Any]:
    """Return one exact build/runtime binding observation without launching gameplay."""
    readiness = dict(_current_source_executable_readiness(
        executable=executable,
        isolated_harness_diagnosis=isolated_harness_diagnosis,
    ))
    executable_path = str(readiness.get("executable_path", "")).strip()
    executable_sha256, executable_error = (
        sha256_file(Path(executable_path)) if executable_path else ("", "executable path unavailable")
    )
    return {
        "build_runtime_status": readiness,
        "runtime_binding": {
            "runtime_source": runtime_source_binding(),
            "executable_path": executable_path,
            "executable_sha256": executable_sha256,
            "executable_error": executable_error,
        },
    }


def _apply_source_readiness_to_query(
    result: Mapping[str, Any],
    readiness: Mapping[str, Any],
) -> Dict[str, Any]:
    """Put the cheap prerequisite ahead of a query-bound repair authority."""
    routed = dict(result)
    routed["source_executable_readiness"] = dict(readiness)
    action = routed.get("next_action")
    if not isinstance(action, Mapping) or action.get("kind") != "repair_current_contradiction":
        return routed
    routed["source_executable_readiness"] = dict(readiness)
    status = str(readiness.get("status", ""))
    if status == "ready":
        return routed
    routed["next_action"] = {
        "kind": (
            "isolated_harness_diagnosis"
            if status == "provisional_diagnosis_allowed"
            else "build_or_select_source_matching_executable"
        ),
        "reason": str(readiness.get("reason", "source_executable_readiness_unproved")),
        "action": str(readiness.get("next_action", "")),
        "evidence_ceiling": str(readiness.get("evidence_ceiling", "none")),
        "after_readiness": dict(action),
    }
    return routed


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


def _registry_launch_probe_namespace(selection: RegistryLaunchToken,
        *, post_relaunch_continuation: bool = False) -> argparse.Namespace:
    """Adapt one validated registry selection into the ordinary probe parser."""
    source_path = Path(selection.source_path).resolve()
    canonical_path = startup_harness.scenario_path(selection.scenario).resolve()
    if canonical_path != source_path:
        raise ScenarioRegistryStoreError(
            "Selected scenario source is not the canonical probe manifest: "
            f"{source_path}"
        )
    command = ["probe", selection.scenario]
    if post_relaunch_continuation:
        command.extend(["--fixture", "", "--post-relaunch-continuation"])
    scenario = startup_harness.load_scenario(selection.scenario)
    if bool(scenario.get("replace_existing_worlds", False)):
        command.append("--replace-existing-worlds")
    return startup_harness.build_parser().parse_args(command)


def _registry_bootstrap_probe_namespace(
    selection: RegistryBootstrapToken, *, cockpit_live_session: bool = False,
) -> argparse.Namespace:
    """Adapt a separately authorized bootstrap run into the same canonical probe route."""
    source_path = Path(selection.source_path).resolve()
    canonical_path = startup_harness.scenario_path(selection.scenario).resolve()
    if canonical_path != source_path:
        raise ScenarioRegistryStoreError(
            "Bootstrap scenario source is not the canonical probe manifest: "
            f"{source_path}"
        )
    # A detached bridge owns the child's stdout.  The terminal probe report is
    # already immutable on disk and ingested by the post-finalize hook; emit
    # only its bounded index card so a completed child cannot block on an
    # unread stdout pipe after its correlated run.finish reply was persisted.
    command = ["probe", selection.scenario, "--compact-stdout"]
    if cockpit_live_session:
        command.append("--cockpit-live-session")
    return startup_harness.build_parser().parse_args(command)


def _registry_repair_probe_namespace(
    selection: RegistryRepairToken, *, cockpit_live_session: bool = False,
) -> argparse.Namespace:
    """Adapt a separately authorized repair run into the same canonical probe route."""
    source_path = Path(selection.source_path).resolve()
    canonical_path = startup_harness.scenario_path(selection.scenario).resolve()
    if canonical_path != source_path:
        raise ScenarioRegistryStoreError(
            "Repair scenario source is not the canonical probe manifest: "
            f"{source_path}"
        )
    # See the bootstrap counterpart: the file bridge persists correlated
    # request replies, while the post-session report must stay bounded.
    command = ["probe", selection.scenario, "--compact-stdout"]
    if cockpit_live_session:
        command.append("--cockpit-live-session")
    return startup_harness.build_parser().parse_args(command)


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
                if payload.get("authority_kind") == "registry_bootstrap_first_compatible_run":
                    return ingest_bootstrap_token_linked_report_reference(
                        connection,
                        token_id,
                        report_path,
                        adapters=production_binding_adapters(),
                    )
                if payload.get("authority_kind") == "registry_repair_exact_contradiction":
                    return ingest_repair_token_linked_report_reference(
                        connection,
                        token_id,
                        report_path,
                        adapters=production_binding_adapters(),
                    )
                return ingest_token_linked_report_reference(
                    connection,
                    token_id,
                    report_path,
                    adapters=production_binding_adapters(),
                    witness_charter=(
                        payload.get("witness_charter")
                        if isinstance(payload.get("witness_charter"), Mapping) else None
                    ),
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
    validate_observation = commands.add_parser(
        "validate-production-observation",
        help="revalidate one uncredited production-observation source report",
    )
    validate_observation.add_argument("--source-report", required=True)
    commands.add_parser("reconcile", help="recompute report bindings from their authoritative owners")
    commands.add_parser("final-gates", help="derive automated-certification and Windows-feel eligibility")
    windows_handoff = commands.add_parser(
        "prepare-windows-feel-handoff",
        help="prepare ordinary Windows play from one current certification pass",
    )
    windows_handoff.add_argument("certification_verification_id")
    windows_handoff.add_argument(
        "--windows-build", required=True,
        help="JSON file with the Windows executable and ordinary world reference",
    )
    windows_status = commands.add_parser(
        "windows-feel-status",
        help="display pending or externally attested Windows feel results",
    )
    windows_status.add_argument("handoff_id", nargs="?")
    windows_judgment = commands.add_parser(
        "record-windows-feel",
        help="store an external Josef-labelled result; local caller identity is not authenticated",
    )
    windows_judgment.add_argument("handoff_id")
    windows_judgment.add_argument("--outcome", required=True, choices=("pass", "fail"))
    windows_judgment.add_argument("--author", required=True, choices=("Josef",))
    windows_judgment.add_argument("--notes", default="")
    bootstrap = commands.add_parser(
        "registry-bootstrap",
        help="issue one manifest-SHA/query/runtime-bound first-evidence authority",
    )
    bootstrap_source = bootstrap.add_mutually_exclusive_group(required=True)
    bootstrap_source.add_argument("--query-file", help="typed bootstrap query JSON file")
    bootstrap_source.add_argument("--query-json", help="typed bootstrap query JSON object")
    revalidate_bootstrap = commands.add_parser(
        "registry-revalidate-bootstrap",
        help="append one current-facts release for a valid stale bootstrap manifest",
    )
    revalidate_source = revalidate_bootstrap.add_mutually_exclusive_group(required=True)
    revalidate_source.add_argument("--query-file", help="typed bootstrap query JSON file")
    revalidate_source.add_argument("--query-json", help="typed bootstrap query JSON object")
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
    query.add_argument(
        "--isolated-harness-diagnosis",
        action="store_true",
        help=(
            "route a stale current contradiction only to a provisional harness-only "
            "diagnosis; no playtest outcome authority is issued"
        ),
    )
    query.add_argument("--coordinator-brief", help="coordinator brief JSON for explicit playtest authority")
    query.add_argument("--witness-charter", help="validated witness charter JSON for explicit playtest authority")
    query.add_argument("--page-size", type=_positive_page_size, default=5,
                       help="matches per page (default: 5; owner-selected presentation preference)")
    query.add_argument("--full", action="store_true", help="print the complete query evaluation")
    query_page_parser = commands.add_parser("registry-query-page", help="read another page of one saved query")
    query_page_parser.add_argument("--sha256", required=True)
    query_page_parser.add_argument("--offset", type=int, default=0)
    query_page_parser.add_argument("--page-size", type=_positive_page_size, default=5)
    query_artifact = commands.add_parser("registry-query-artifact", help="recover one complete query result")
    query_artifact.add_argument("--sha256", required=True)
    status = commands.add_parser("registry-status", help="inspect registry lifecycle, relation, and retirement history")
    status.add_argument("--manifest-id", action="append", default=[],
                        help="exact manifest identity; repeat to retrieve only named current entries")
    status.add_argument("--full", action="store_true",
                        help="explicitly print the complete status payload instead of its artifact receipt")
    status.add_argument(
        "--include-state",
        action="append",
        choices=("quarantined", "retired"),
        default=[],
        help="include non-active lifecycle rows; repeated values are accepted",
    )
    runtime_status = commands.add_parser(
        "runtime-status",
        help="inspect one build/runtime binding without launching gameplay",
    )
    runtime_status.add_argument(
        "--executable", default="",
        help="exact executable binding to inspect; defaults to the detected executable",
    )
    runtime_status.add_argument(
        "--isolated-harness-diagnosis", action="store_true",
        help="allow only the explicitly provisional isolated-harness readiness route",
    )
    runtime_status.add_argument(
        "--full", action="store_true",
        help="explicitly print the complete runtime-status payload instead of its artifact receipt",
    )
    runtime_artifact = commands.add_parser(
        "runtime-status-artifact",
        help="retrieve one digest-bound runtime-status payload",
    )
    runtime_artifact.add_argument(
        "--sha256", required=True, help="exact SHA-256 from a runtime-status receipt",
    )
    commands.add_parser("retirement-candidates", help="inspect review-only retirement candidates")
    artifact = commands.add_parser("registry-artifact", help="retrieve one digest-bound registry-status payload")
    artifact.add_argument("--sha256", required=True, help="exact SHA-256 from a registry-status receipt")
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
    launch.add_argument(
        "--witness-charter",
        help="coordinator-authored compact playtest witness charter JSON",
    )
    launch.add_argument(
        "--cockpit-bridge-binding-id",
        help="file-bridge identity forwarded to the canonical selected launch",
    )
    launch.add_argument("--post-relaunch-continuation", action="store_true", help=argparse.SUPPRESS)
    launch.add_argument(
        "--certification-inputs",
        help="authoritative installed-input JSON for a registry-owned certification launch",
    )
    detached_launch = commands.add_parser(
        "registry-detached-launch",
        help="start one selected live-cockpit scenario on its bound file bridge",
    )
    detached_launch.add_argument("selection_token", help="selection token returned by registry-query")
    detached_launch.add_argument(
        "--session-dir", required=True,
        help="new private file-backed bridge session directory",
    )
    detached_launch.add_argument(
        "--witness-charter", required=True,
        help="coordinator-authored compact playtest witness charter JSON",
    )
    detached_launch.add_argument("--post-relaunch-continuation", action="store_true",
                                 help="continue only declared post-relaunch steps from the saved world")
    witness = commands.add_parser(
        "registry-record-witness",
        help="validate and append one cited witness over immutable reports",
    )
    witness.add_argument("--manifest-id", required=True)
    witness.add_argument("--report-id", action="append", required=True)
    witness.add_argument("--charter", required=True)
    witness.add_argument("--journal", required=True)
    witness.add_argument("--statement", required=True)
    witness_review = commands.add_parser(
        "registry-review-witness",
        help="record the coordinator's causal judgment of a valid witness",
    )
    witness_review.add_argument("witness_id")
    witness_review.add_argument("--decision", required=True,
                                choices=("accept", "continue", "repair", "change-strategy"))
    witness_review.add_argument("--rationale", required=True)
    witness_review.add_argument("--concrete-risk", default="")
    witness_review.add_argument("--reviewer-role", default="coordinator",
                                choices=("coordinator", "mutation-reviewer"))
    certification_launch = commands.add_parser(
        "certification-launch",
        help="launch one selected scenario under the registry-owned continuous-certification authority",
    )
    certification_launch.add_argument("selection_token", help="selection token returned by registry-query")
    certification_launch.add_argument(
        "--certification-inputs",
        help="authoritative installed-input JSON for this certification launch",
    )
    bootstrap_launch = commands.add_parser(
        "registry-bootstrap-launch",
        help="claim one bootstrap authority and run its canonical probe route",
    )
    bootstrap_launch.add_argument("bootstrap_token", help="token returned by registry-bootstrap")
    bootstrap_launch.add_argument(
        "--adaptive-semantic-autodrive",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    bootstrap_launch.add_argument(
        "--cockpit-live-session",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    detached_bootstrap_launch = commands.add_parser(
        "registry-bootstrap-detached-launch",
        help="start one bootstrap-owned cockpit on the file-backed JSONL bridge",
    )
    detached_bootstrap_launch.add_argument(
        "bootstrap_token", help="token returned by registry-bootstrap",
    )
    detached_bootstrap_launch.add_argument(
        "--session-dir", required=True,
        help="new private file-backed bridge session directory",
    )
    detached_bootstrap_launch.add_argument("--witness-charter")
    production_capture_command = commands.add_parser(
        "capture-production-fixture",
        help="capture one normal production world with registry-owned focused provenance",
    )
    production_capture_command.add_argument("fixture", help="fixture name to create or replace")
    production_capture_command.add_argument("--report", required=True, help="green source report from the normal production run")
    production_capture_command.add_argument("--scenario", required=True, help="exact scenario source named by the source report")
    production_capture_command.add_argument("--profile", default="", help="profile containing the source world")
    production_capture_command.add_argument("--world", default="", help="explicit source world name")
    production_capture_command.add_argument("--overwrite", action="store_true", help="replace an existing fixture")
    observation = commands.add_parser(
        "production-observe",
        help="run one transform-free ordinary probe and write uncredited source evidence",
    )
    observation.add_argument("scenario", help="packaged scenario to execute without registry selection")
    observation.add_argument("--fixture", required=True, help="transform-free fixture to install")
    observation.add_argument("--fixture-profile", default="", help="fixture source profile")
    observation.add_argument("--profile", default="", help="target harness profile")
    observation.add_argument("--world", default="", help="explicit fixture world")
    observation.add_argument("--source-report", default="production.source.json", help="relative output within the run directory, or an absolute path")
    observation.add_argument("--replace-existing-worlds", action="store_true")
    observation.add_argument("--compact-stdout", action="store_true")
    observation.add_argument("--dry-run", action="store_true")
    repair = commands.add_parser(
        "registry-repair-bootstrap",
        help="issue one exact manifest/route/red-verification repair authority",
    )
    repair_source = repair.add_mutually_exclusive_group(required=True)
    repair_source.add_argument("--query-file", help="typed repair query JSON file")
    repair_source.add_argument("--query-json", help="typed repair query JSON object")
    repair_source.add_argument(
        "--query-id",
        help="derive the current manifest, route, red verification, and typed request from registry-query",
    )
    repair.add_argument("--manifest-id")
    repair.add_argument("--route-key")
    repair.add_argument("--red-verification-id")
    repair_launch = commands.add_parser(
        "registry-repair-launch",
        help="claim one repair authority and run its canonical probe route",
    )
    repair_launch.add_argument("repair_token", help="token returned by registry-repair-bootstrap")
    repair_launch.add_argument(
        "--adaptive-semantic-autodrive",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    repair_launch.add_argument(
        "--cockpit-live-session",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    detached_repair_launch = commands.add_parser(
        "registry-repair-detached-launch",
        help="start one repair-owned cockpit on the file-backed JSONL bridge",
    )
    detached_repair_launch.add_argument("repair_token", help="token returned by registry-repair-bootstrap")
    detached_repair_launch.add_argument(
        "--session-dir", required=True,
        help="new private file-backed bridge session directory",
    )
    detached_repair_launch.add_argument("--witness-charter")
    repair_terminal = commands.add_parser(
        "registry-repair-finalize-no-report",
        help="append accepted cleanup for one claimed repair that produced no probe report",
    )
    repair_terminal.add_argument("repair_token")
    repair_terminal.add_argument("--run-dir", required=True)
    repair_terminal.add_argument("--cleanup-json", required=True)
    repair_compatibility_terminal = commands.add_parser(
        "registry-repair-compatibility-terminal",
        help="record one zero-credit current-source/runtime repair terminal",
    )
    repair_compatibility_terminal.add_argument("repair_token")
    repair_compatibility_terminal.add_argument("--terminal-json", required=True)
    repair_successor = commands.add_parser(
        "registry-repair-r019-current-source-successor",
        help="record the zero-credit current-source R-019 HUD repair successor",
    )
    repair_successor.add_argument("repair_token")
    repair_successor.add_argument("--successor-json", required=True)
    r019_aggregation_authorize = commands.add_parser(
        "registry-r019-aggregation-authorize",
        help="issue one separate authority for an exact zero-credit R-019 report pair",
    )
    r019_aggregation_authorize.add_argument("--guarded-report-id", required=True)
    r019_aggregation_authorize.add_argument("--primitive-report-id", required=True)
    r019_aggregation_finalize = commands.add_parser(
        "registry-r019-aggregation-finalize",
        help="consume one R-019 aggregation authority and append its immutable zero-credit packet",
    )
    r019_aggregation_finalize.add_argument("aggregation_token")
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


def _bootstrap_bridge_binding_id(selection: RegistryBootstrapToken) -> str:
    """Bind a private bridge session to the exact bootstrap launch inputs."""
    binding = selection.runtime_binding
    payload = {
        "bootstrap_token": selection.token_id,
        "executable_sha256": str(binding.get("executable_sha256", "")),
        "runtime_source_sha256": str(binding.get("runtime_source_sha256", "")),
        "source_path": selection.source_path,
    }
    return hashlib.sha256(
        json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _repair_bridge_binding_id(selection: RegistryRepairToken, repair_details: Mapping[str, Any]) -> str:
    """Bind a private bridge session to one exact repair authority and footing."""
    payload = {
        "repair_token": selection.token_id,
        "manifest_id": repair_details.get("manifest_id"),
        "route_key": repair_details.get("route_key"),
        "red_verification_id": repair_details.get("red_verification_id"),
        "binding": repair_details.get("binding"),
        "executable_sha256": str(selection.runtime_binding.get("executable_sha256", "")),
        "runtime_source_sha256": str(selection.runtime_binding.get("runtime_source_sha256", "")),
        "source_path": selection.source_path,
    }
    return hashlib.sha256(
        json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _declared_pre_descriptor_prefix(selection: Any) -> list[dict[str, Any]]:
    """Return only the scenario-owned adaptive stages allowed before live entry."""
    try:
        scenario = json.loads(Path(selection.source_path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ScenarioRegistryStoreError("bootstrap scenario declaration is unavailable") from exc
    if not isinstance(scenario, Mapping):
        raise ScenarioRegistryStoreError("bootstrap scenario declaration is malformed")
    declared = scenario.get("cockpit_pre_descriptor_bootstrap")
    if declared is None:
        return []
    if not isinstance(declared, Mapping) or declared.get("schema") != \
            "caol-cockpit-pre-descriptor-bootstrap-v1" or declared.get("gameplay_credit") is not False:
        raise ScenarioRegistryStoreError("scenario has no zero-credit pre-descriptor bootstrap contract")
    prefix = declared.get("adaptive_prefix")
    if not isinstance(prefix, list) or not prefix:
        raise ScenarioRegistryStoreError("pre-descriptor bootstrap prefix is missing")
    declared_steps = scenario.get("steps")
    if not isinstance(declared_steps, list):
        raise ScenarioRegistryStoreError("scenario steps are unavailable")
    result: list[dict[str, Any]] = []
    first_label = str(prefix[0].get("label", "")) if isinstance(prefix[0], Mapping) else ""
    start_index = next((index for index, step in enumerate(declared_steps)
                        if isinstance(step, Mapping) and step.get("label") == first_label), -1)
    for index, expected in enumerate(prefix):
        step_index = start_index + index
        if not isinstance(expected, Mapping) or start_index < 0 or step_index >= len(declared_steps):
            raise ScenarioRegistryStoreError("pre-descriptor bootstrap prefix is malformed")
        step = declared_steps[step_index]
        if not isinstance(step, Mapping) or step.get("kind") != "adaptive_semantic_window":
            raise ScenarioRegistryStoreError("pre-descriptor stage is not an adaptive semantic window")
        stage = {
            "label": str(expected.get("label", "")),
            "objective": str(expected.get("objective", "")),
            "required_action_chain": expected.get("required_action_chain"),
            "adaptive_interrupt_actions": expected.get("adaptive_interrupt_actions", []),
        }
        if not stage["objective"] or not isinstance(stage["required_action_chain"], list) or \
                not isinstance(stage["adaptive_interrupt_actions"], list) or \
                not stage["label"] or step.get("label") != stage["label"] or \
                step.get("required_action_chain") != stage["required_action_chain"] or \
                step.get("adaptive_interrupt_actions", []) != stage["adaptive_interrupt_actions"]:
            raise ScenarioRegistryStoreError("pre-descriptor bootstrap stage does not match scenario declaration")
        result.append(stage)
    interstitial = declared.get("interstitial_setup_steps", [])
    if not isinstance(interstitial, list):
        raise ScenarioRegistryStoreError("pre-descriptor interstitial setup is malformed")
    next_step_index = start_index + len(prefix)
    for expected in interstitial:
        if not isinstance(expected, Mapping) or next_step_index >= len(declared_steps):
            raise ScenarioRegistryStoreError("pre-descriptor interstitial setup is malformed")
        step = declared_steps[next_step_index]
        if not isinstance(step, Mapping) or step.get("kind") != expected.get("kind") or \
                step.get("label") != expected.get("label") or step.get("keys") != expected.get("keys"):
            raise ScenarioRegistryStoreError("pre-descriptor interstitial setup does not match scenario declaration")
        next_step_index += 1
    next_step = declared_steps[next_step_index] if next_step_index < len(declared_steps) else None
    if not isinstance(next_step, Mapping) or next_step.get("kind") != "cockpit_live_session":
        raise ScenarioRegistryStoreError("pre-descriptor prefix does not end at a live cockpit entry")
    return result


def _scenario_requires_bound_live_bridge(scenario_name: str) -> bool:
    """Identify a selected scenario whose public owner needs a live stdin channel."""
    scenario = startup_harness.load_scenario(scenario_name)
    return any(
        isinstance(step, Mapping) and step.get("kind") == "cockpit_live_session"
        for step in scenario.get("steps", [])
    )


def _declared_live_session_reentries(
    selection: Any, *, post_relaunch_continuation: bool = False,
) -> int:
    """Count only cockpit sessions that follow the bridge's initial session."""
    try:
        scenario = json.loads(Path(selection.source_path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ScenarioRegistryStoreError("live bridge scenario declaration is unavailable") from exc
    if not isinstance(scenario, Mapping):
        raise ScenarioRegistryStoreError("live bridge scenario declaration is malformed")
    post_relaunch = scenario.get("post_relaunch")
    if post_relaunch is None:
        return 0
    if not isinstance(post_relaunch, Mapping):
        raise ScenarioRegistryStoreError("post_relaunch declaration is malformed")
    steps = post_relaunch.get("steps")
    if not isinstance(steps, list):
        raise ScenarioRegistryStoreError("post_relaunch steps are malformed")
    declared_sessions = sum(
        1 for step in steps
        if isinstance(step, Mapping) and step.get("kind") == "cockpit_live_session"
    )
    if post_relaunch_continuation:
        # The probe rewrites its steps to start directly at the saved-world
        # continuation. Its first cockpit session is already the bridge's
        # initial session; only later declared sessions need reentry slots.
        return max(0, declared_sessions - 1)
    return declared_sessions


def _witness_launch_environment(args: argparse.Namespace) -> Dict[str, str]:
    """Validate the coordinator charter before any launch authority is claimed."""
    environment = dict(os.environ)
    charter_path = str(getattr(args, "witness_charter", "") or "").strip()
    if not charter_path:
        return environment
    try:
        value = json.loads(Path(charter_path).read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ScenarioRegistryStoreError("witness charter is unreadable") from exc
    if not isinstance(value, Mapping):
        raise ScenarioRegistryStoreError("witness charter must be a JSON object")
    from playtest_witness import WitnessError, normalize_witness_charter
    try:
        charter = normalize_witness_charter(value)
    except WitnessError as exc:
        raise ScenarioRegistryStoreError(str(exc)) from exc
    environment["OPENCLAW_PLAYTEST_WITNESS_CHARTER"] = json.dumps(
        charter, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    )
    return environment


def _load_json_object_file(path_value: str, label: str) -> Mapping[str, Any]:
    try:
        value = json.loads(Path(path_value).read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ScenarioRegistryStoreError(label + " is unreadable") from exc
    if not isinstance(value, Mapping):
        raise ScenarioRegistryStoreError(label + " must be a JSON object")
    return value


def _witness_charter_from_environment() -> Mapping[str, Any] | None:
    raw = os.environ.get("OPENCLAW_PLAYTEST_WITNESS_CHARTER", "").strip()
    if not raw:
        return None
    try:
        value = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ScenarioRegistryStoreError("witness charter environment is malformed") from exc
    if not isinstance(value, Mapping):
        raise ScenarioRegistryStoreError("witness charter environment must be a JSON object")
    return value


def _launch_selection_file_bridge(args: argparse.Namespace, registry_path: Path) -> int:
    """Start a brief-requested session while the bridge preserves the technical claim."""
    session_dir = Path(args.session_dir).resolve()
    registry_path = registry_path.resolve()
    witness_charter_path = Path(args.witness_charter).resolve()
    if session_dir.exists():
        _write_result({"ok": False, "command": args.command,
                       "error": "bridge session directory already exists",
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    connection = open_registry(str(registry_path))
    try:
        charter = _load_json_object_file(str(witness_charter_path), "witness charter")
        selection = reload_selection_token_for_launch(
            connection, args.selection_token, witness_charter=charter,
        )
        if not selection.accepted:
            _write_result({"ok": False, "command": args.command, "registry": str(registry_path),
                           "result": asdict(selection)}, stream=sys.stderr)
            return 1
        readiness = _current_source_executable_readiness()
        if readiness.get("status") != "ready":
            # Do this before starting the bridge, so an executable that cannot
            # prove the current product source never strands or consumes a
            # selection token.  The canonical child repeats this check to
            # cover a source change between bridge startup and token claim.
            _write_result({
                "ok": False,
                "command": args.command,
                "registry": str(registry_path),
                "result": asdict(RegistryLaunchToken(
                    selection.token_id, False, "source_matching_executable_required",
                )),
                "source_executable_readiness": dict(readiness),
            }, stream=sys.stderr)
            return 1
        if not _scenario_requires_bound_live_bridge(selection.scenario):
            _write_result({"ok": False, "command": args.command,
                           "error": "selected scenario has no live cockpit session"}, stream=sys.stderr)
            return 1
    finally:
        connection.close()
    try:
        pre_descriptor_prefix = _declared_pre_descriptor_prefix(selection)
        session_reentries = _declared_live_session_reentries(
            selection, post_relaunch_continuation=bool(getattr(args, "post_relaunch_continuation", False))
        )
    except (OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        _write_result({"ok": False, "command": args.command, "error": str(exc),
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    bridge_binding_id = _identity_sha256(
        f"caol-selected-cockpit-bridge-v1:{selection.token_id}:{selection.source_path}"
    )
    bridge_command = [
        sys.executable, str(Path(__file__).with_name("cockpit_file_bridge.py")), "start",
        "--session-dir", str(session_dir), "--binding-id", bridge_binding_id,
        "--require-session-ready",
        "--session-reentries", str(session_reentries),
        "--pre-descriptor-prefix-json", json.dumps(pre_descriptor_prefix, separators=(",", ":")),
        "--", sys.executable, str(Path(__file__).resolve()), "--registry", str(registry_path),
        "registry-launch", selection.token_id, "--witness-charter", str(witness_charter_path),
        "--cockpit-bridge-binding-id", bridge_binding_id,
    ]
    if bool(getattr(args, "post_relaunch_continuation", False)):
        bridge_command.append("--post-relaunch-continuation")
    started = subprocess.run(bridge_command, cwd=str(repository_root()), check=False,
                             env=_witness_launch_environment(args),
                             capture_output=True, text=True)
    if started.returncode != 0:
        _write_result({"ok": False, "command": args.command,
                       "error": "file-backed bridge did not start", "session_dir": str(session_dir)},
                      stream=sys.stderr)
        return 1
    try:
        bridge_receipt = json.loads(started.stdout)
    except json.JSONDecodeError:
        bridge_receipt = {}
    if not isinstance(bridge_receipt, Mapping) or not bridge_receipt.get("ok"):
        _write_result({"ok": False, "command": args.command,
                       "error": "file-backed bridge returned an invalid launch receipt",
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    _write_result({
        "ok": True, "command": args.command, "registry": str(registry_path),
        "selection_token": selection.token_id, "session_dir": str(session_dir),
        "binding_id": bridge_binding_id, "bridge": bridge_receipt,
        "authority": "technical run token remains unclaimed until the canonical child launch",
    })
    return 0


def _launch_bootstrap_file_bridge(args: argparse.Namespace, registry_path: Path) -> int:
    """Start one detached bridge whose child owns the canonical bootstrap claim.

    The wrapper only verifies that the still-unclaimed authority can start.  The
    child invokes ``registry-bootstrap-launch`` unchanged, retaining its
    one-use claim, exact source/runtime revalidation, report ingestion, and
    cleanup behavior.  Claiming in this wrapper would strand a token if bridge
    startup failed.
    """
    session_dir = Path(args.session_dir).resolve()
    if session_dir.exists():
        _write_result({
            "ok": False,
            "command": args.command,
            "error": "bridge session directory already exists",
            "session_dir": str(session_dir),
        }, stream=sys.stderr)
        return 1
    connection = open_registry(str(registry_path))
    try:
        selection = reload_bootstrap_token_for_launch(connection, args.bootstrap_token)
        if not selection.accepted:
            _write_result({
                "ok": False,
                "command": args.command,
                "registry": str(registry_path),
                "result": asdict(selection),
            }, stream=sys.stderr)
            return 1
        comparison = startup_harness.compare_runtime_binding(selection.runtime_binding)
        if comparison.get("status") != "matched":
            record_bootstrap_token_rejection(
                connection,
                selection.token_id,
                reason="runtime_binding_changed",
                details={"comparison": dict(comparison)},
            )
            _write_result({
                "ok": False,
                "command": args.command,
                "registry": str(registry_path),
                "result": asdict(RegistryBootstrapToken(
                    selection.token_id, False, "runtime_binding_changed",
                )),
            }, stream=sys.stderr)
            return 1
    finally:
        connection.close()

    try:
        pre_descriptor_prefix = _declared_pre_descriptor_prefix(selection)
        session_reentries = _declared_live_session_reentries(
            selection, post_relaunch_continuation=bool(getattr(args, "post_relaunch_continuation", False))
        )
    except (OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        _write_result({
            "ok": False,
            "command": args.command,
            "error": str(exc),
            "session_dir": str(session_dir),
        }, stream=sys.stderr)
        return 1
    bridge_binding_id = _bootstrap_bridge_binding_id(selection)
    cockpit_command = [
        sys.executable, str(Path(__file__).resolve()), "--registry", str(registry_path),
        "registry-bootstrap-launch", selection.token_id, "--cockpit-live-session",
        "--adaptive-semantic-autodrive",
    ]
    bridge_command = [
        sys.executable, str(Path(__file__).with_name("cockpit_file_bridge.py")), "start",
        "--session-dir", str(session_dir), "--binding-id", bridge_binding_id,
        "--require-session-ready",
        "--session-reentries", str(session_reentries),
        "--pre-descriptor-prefix-json", json.dumps(pre_descriptor_prefix, separators=(",", ":")),
        "--", *cockpit_command,
    ]
    started = subprocess.run(bridge_command, cwd=str(repository_root()), check=False,
                             env=_witness_launch_environment(args),
                             capture_output=True, text=True)
    if started.returncode != 0:
        _write_result({
            "ok": False,
            "command": args.command,
            "error": "file-backed bridge did not start",
            "session_dir": str(session_dir),
        }, stream=sys.stderr)
        return 1
    try:
        bridge_receipt = json.loads(started.stdout)
    except json.JSONDecodeError:
        bridge_receipt = {}
    if not isinstance(bridge_receipt, Mapping) or not bridge_receipt.get("ok"):
        _write_result({
            "ok": False,
            "command": args.command,
            "error": "file-backed bridge returned an invalid launch receipt",
            "session_dir": str(session_dir),
        }, stream=sys.stderr)
        return 1
    _write_result({
        "ok": True,
        "command": args.command,
        "registry": str(registry_path),
        "bootstrap_token": selection.token_id,
        "session_dir": str(session_dir),
        "binding_id": bridge_binding_id,
        "runtime_binding": selection.runtime_binding,
        "bridge": bridge_receipt,
        "authority": "bootstrap token remains unclaimed until the canonical child launch",
    })
    return 0


def _launch_repair_file_bridge(args: argparse.Namespace, registry_path: Path) -> int:
    """Start an unclaimed repair authority through the canonical file bridge."""
    session_dir = Path(args.session_dir).resolve()
    if session_dir.exists():
        _write_result({"ok": False, "command": args.command,
                       "error": "bridge session directory already exists",
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    connection = open_registry(str(registry_path))
    try:
        issued = connection.execute(
            "SELECT manifest_id, verification_id, route_key, details_json FROM token_history "
            "WHERE token_id = ? AND event_kind = 'repair_issued' ORDER BY token_event_id LIMIT 1",
            (args.repair_token,),
        ).fetchone()
        if issued is None:
            _write_result({"ok": False, "command": args.command,
                           "result": asdict(RegistryRepairToken(args.repair_token, False, "token_unknown"))},
                          stream=sys.stderr)
            return 1
        manifest = connection.execute(
            "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (str(issued["manifest_id"]),),
        ).fetchone()
        if manifest is None:
            _write_result({"ok": False, "command": args.command,
                           "result": asdict(RegistryRepairToken(args.repair_token, False, "manifest_absent"))},
                          stream=sys.stderr)
            return 1
        declaration = json.loads(str(manifest["declaration_json"]))
        if not isinstance(declaration, Mapping):
            raise ScenarioRegistryStoreError("repair manifest declaration must be an object")
        binding = _current_repair_binding(declaration)
        selection = reload_repair_token_for_launch(connection, args.repair_token, binding=binding)
        if not selection.accepted:
            _write_result({"ok": False, "command": args.command, "registry": str(registry_path),
                           "result": asdict(selection)}, stream=sys.stderr)
            return 1
        comparison = startup_harness.compare_runtime_binding(selection.runtime_binding)
        if comparison.get("status") != "matched":
            record_repair_token_rejection(connection, selection.token_id, reason="runtime_binding_changed",
                                          details={"comparison": dict(comparison)})
            _write_result({"ok": False, "command": args.command, "registry": str(registry_path),
                           "result": asdict(RegistryRepairToken(selection.token_id, False,
                                                                  "runtime_binding_changed"))}, stream=sys.stderr)
            return 1
        repair_details = json.loads(str(issued["details_json"]))
        if not isinstance(repair_details, Mapping):
            raise ScenarioRegistryStoreError("repair token details must be an object")
    finally:
        connection.close()
    try:
        pre_descriptor_prefix = _declared_pre_descriptor_prefix(selection)
    except (OSError, SystemExit, ScenarioRegistryStoreError) as exc:
        _write_result({"ok": False, "command": args.command, "error": str(exc),
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    bridge_binding_id = _repair_bridge_binding_id(selection, repair_details)
    cockpit_command = [
        sys.executable, str(Path(__file__).resolve()), "--registry", str(registry_path),
        "registry-repair-launch", selection.token_id, "--cockpit-live-session",
        "--adaptive-semantic-autodrive",
    ]
    bridge_command = [
        sys.executable, str(Path(__file__).with_name("cockpit_file_bridge.py")), "start",
        "--session-dir", str(session_dir), "--binding-id", bridge_binding_id,
        "--require-session-ready",
        "--pre-descriptor-prefix-json", json.dumps(pre_descriptor_prefix, separators=(",", ":")),
        "--", *cockpit_command,
    ]
    started = subprocess.run(bridge_command, cwd=str(repository_root()), check=False,
                             env=_witness_launch_environment(args),
                             capture_output=True, text=True)
    if started.returncode != 0:
        _write_result({"ok": False, "command": args.command,
                       "error": "file-backed bridge did not start", "session_dir": str(session_dir)},
                      stream=sys.stderr)
        return 1
    try:
        bridge_receipt = json.loads(started.stdout)
    except json.JSONDecodeError:
        bridge_receipt = {}
    if not isinstance(bridge_receipt, Mapping) or not bridge_receipt.get("ok"):
        _write_result({"ok": False, "command": args.command,
                       "error": "file-backed bridge returned an invalid launch receipt",
                       "session_dir": str(session_dir)}, stream=sys.stderr)
        return 1
    _write_result({
        "ok": True, "command": args.command, "registry": str(registry_path),
        "repair_token": selection.token_id, "session_dir": str(session_dir),
        "binding_id": bridge_binding_id, "runtime_binding": selection.runtime_binding,
        "repair_provenance": {
            "manifest_id": repair_details["manifest_id"], "route_key": repair_details["route_key"],
            "red_verification_id": repair_details["red_verification_id"],
            "authority_kind": repair_details["authority_kind"],
        },
        "bridge": bridge_receipt,
        "authority": "repair token remains unclaimed until the canonical child launch",
    })
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "production-observe":
        # This deliberately bypasses registry open/query/token paths.  The
        # resulting report carries explicit zero-credit evidence; an operator
        # must choose canonical ingest separately after reviewing it.
        scenario = startup_harness.load_scenario(args.scenario)
        fixture_profile = args.fixture_profile or str(scenario.get("fixture_profile", ""))
        resolved_fixture = startup_harness.resolve_fixture_payload(args.fixture, fixture_profile)
        transforms = resolved_fixture.get("save_transforms", [])
        allowed_transform = [{
            "kind": "player_mutations",
            "player_save": scenario.get("installed_save_player"),
            "mutations": scenario.get("required_stabilizer_traits"),
        }]
        source_binding = resolved_fixture.get("source_binding")
        if transforms != allowed_transform or not isinstance(source_binding, Mapping):
            raise SystemExit("production observation requires a transform-free fixture source chain")
        return startup_harness.run_probe(argparse.Namespace(
            scenario=args.scenario,
            profile=args.profile,
            world=args.world,
            fixture=args.fixture,
            fixture_profile=fixture_profile,
            replace_existing_worlds=args.replace_existing_worlds,
            advance_turns=None,
            settle_seconds=None,
            artifact_pattern="",
            test_command="",
            compact_stdout=args.compact_stdout,
            production_observation_output=args.source_report,
            certification_registry="",
            certification_round_manifest="",
            certification_lease_id="",
            certification_recheck_inputs="",
            dry_run=args.dry_run,
        ))
    registry_path = resolve_registry_path(args.registry)
    if args.command in {"registry-query-page", "registry-query-artifact"}:
        try:
            payload = read_command_artifact(artifact_root=registry_path.parent / "command-artifacts",
                                            command="registry-query", sha256=args.sha256)
            if args.command == "registry-query-artifact":
                _write_result(payload)
            else:
                artifact = registry_path.parent / "command-artifacts" / "registry-query" / (args.sha256.lower() + ".json")
                receipt = {"schema": "caol-command-receipt-v1", "command": "registry-query",
                           "artifact": {"path": str(artifact), "sha256": args.sha256.lower(),
                                        "bytes": artifact.stat().st_size}}
                result = query_page(payload, receipt, offset=args.offset, page_size=args.page_size,
                                    cli=[sys.executable, str(Path(__file__).resolve()), "--registry", str(registry_path)])
                _write_result({"ok": True, "command": args.command, "registry": str(registry_path), "result": result})
            return 0
        except (OSError, ValueError, KeyError) as error:
            _write_result({"ok": False, "command": args.command, "error": str(error)}, stream=sys.stderr)
            return 1
    if args.command == "registry-detached-launch":
        return _launch_selection_file_bridge(args, registry_path)
    if args.command == "registry-repair-detached-launch":
        return _launch_repair_file_bridge(args, registry_path)
    if args.command == "registry-bootstrap-detached-launch":
        return _launch_bootstrap_file_bridge(args, registry_path)
    if args.command in {"registry-bootstrap-launch", "registry-repair-launch"} and \
            bool(args.cockpit_live_session) and not \
            str(os.environ.get("OPENCLAW_COCKPIT_BRIDGE_BINDING_ID", "")).strip():
        _write_result({
            "ok": False,
            "command": args.command,
            "error": "cockpit_live_session_requires_bound_file_bridge",
        }, stream=sys.stderr)
        return 1
    probe_namespace: argparse.Namespace | None = None
    try:
        connection = open_registry(str(registry_path))
        try:
            if args.command == "rebuild":
                result = rebuild_manifest_projection(connection, Path(args.scenarios_root))
            elif args.command == "ingest-report":
                try:
                    report = json.loads(Path(args.report).read_text(encoding="utf-8"))
                except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise ScenarioRegistryStoreError("report is unreadable") from exc
                observation = report.get("production_observation") if isinstance(report, Mapping) else None
                if isinstance(observation, Mapping) and observation.get("status") == "recorded":
                    production_capture.validate_observation_source_report(
                        source_path=Path(str(observation.get("source_report", ""))),
                        runtime_binding=startup_harness.build_runtime_binding(startup_harness.detect_executable()),
                    )
                result = ingest_report_reference(
                    connection,
                    Path(args.report),
                    adapters=production_binding_adapters(),
                )
            elif args.command == "validate-production-observation":
                result = production_capture.validate_observation_source_report(
                    source_path=Path(args.source_report),
                    runtime_binding=startup_harness.build_runtime_binding(startup_harness.detect_executable()),
                )
            elif args.command == "reconcile":
                result = reconcile_report_bindings(connection, adapters=production_binding_adapters())
            elif args.command == "final-gates":
                result = final_gate_eligibility(connection)
            elif args.command == "prepare-windows-feel-handoff":
                try:
                    windows_build = json.loads(Path(args.windows_build).read_text(encoding="utf-8"))
                except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise ScenarioRegistryStoreError("Windows handoff build reference is unreadable") from exc
                if not isinstance(windows_build, Mapping):
                    raise ScenarioRegistryStoreError("Windows handoff build reference must be a JSON object")
                result = prepare_windows_feel_handoff(
                    connection,
                    certification_verification_id=args.certification_verification_id,
                    windows_build=windows_build,
                )
            elif args.command == "windows-feel-status":
                result = windows_feel_handoff_status(connection, args.handoff_id)
            elif args.command == "record-windows-feel":
                result = record_windows_feel_judgment(
                    connection, handoff_id=args.handoff_id, outcome=args.outcome,
                    author=args.author, notes=args.notes,
                )
            elif args.command == "registry-record-witness":
                try:
                    charter = json.loads(Path(args.charter).read_text(encoding="utf-8"))
                    journal = json.loads(Path(args.journal).read_text(encoding="utf-8"))
                    statement = json.loads(Path(args.statement).read_text(encoding="utf-8"))
                except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
                    raise ScenarioRegistryStoreError("witness input is unreadable") from exc
                if not all(isinstance(value, Mapping) for value in (charter, journal, statement)):
                    raise ScenarioRegistryStoreError("witness inputs must be JSON objects")
                result = record_playtest_witness(
                    connection, manifest_id=args.manifest_id, report_ids=args.report_id,
                    charter=charter, journal=journal, statement=statement,
                )
            elif args.command == "registry-review-witness":
                result = review_playtest_witness(
                    connection, witness_id=args.witness_id, decision=args.decision,
                    rationale=args.rationale, concrete_risk=args.concrete_risk,
                    reviewer_role=args.reviewer_role,
                )
            elif args.command == "capture-production-fixture":
                report_path = Path(args.report).resolve()
                scenario_path = Path(args.scenario).resolve()
                runtime_binding = startup_harness.build_runtime_binding(
                    startup_harness.detect_executable()
                )
                provenance = production_capture.prepare_production_capture(
                    report_path=report_path,
                    scenario_path=scenario_path,
                    runtime_binding=runtime_binding,
                )
                profile = startup_harness.resolve_profile_name(args.profile)
                world = startup_harness.choose_world_for_fixture(profile, args.world)
                manifest = production_capture.capture_production_fixture(
                    source_world=world.path,
                    fixture_dir=startup_harness.profile_fixture_root(profile) / args.fixture,
                    production_origin=provenance,
                    overwrite=bool(args.overwrite),
                )
                result = {
                    "status": "captured",
                    "fixture": args.fixture,
                    "profile": profile,
                    "world": world.name,
                    "manifest": manifest,
                    "credit": "none; canonical report ingestion is required before route authority exists",
                }
            elif args.command == "registry-query":
                request = parse_registry_query_request(_load_query_request(args))
                brief = _load_json_object_file(args.coordinator_brief, "coordinator brief") \
                    if str(args.coordinator_brief or "").strip() else None
                charter = _load_json_object_file(args.witness_charter, "witness charter") \
                    if str(args.witness_charter or "").strip() else None
                result = _apply_source_readiness_to_query(asdict(execute_registry_query(
                    connection,
                    request,
                    include_lifecycle_states=tuple(args.include_state),
                    drafts_root=registry_path.parent / "drafts",
                    coordinator_brief=brief,
                    witness_charter=charter,
                )), _current_source_executable_readiness(
                    isolated_harness_diagnosis=bool(args.isolated_harness_diagnosis),
                ))
                result["next_action"] = _query_launch_action(result, args, registry_path)
            elif args.command == "registry-bootstrap":
                runtime_binding = startup_harness.build_runtime_binding(
                    startup_harness.detect_executable()
                )
                if not runtime_binding.get("ok"):
                    raise ScenarioRegistryStoreError(
                        "bootstrap runtime binding unavailable: "
                        + str(runtime_binding.get("error", "unknown error"))
                    )
                result = asdict(issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(_load_query_request(args)),
                    runtime_binding=runtime_binding,
                ))
            elif args.command == "registry-repair-bootstrap":
                readiness = _current_source_executable_readiness()
                if readiness.get("status") != "ready":
                    result = {
                        "accepted": False,
                        "reason": "source_matching_executable_required",
                        "source_executable_readiness": dict(readiness),
                        "next_action": str(readiness.get("next_action", "")),
                    }
                    _write_result({
                        "ok": True,
                        "command": args.command,
                        "registry": str(registry_path),
                        "result": result,
                    })
                    return 0
                if args.query_id:
                    action = registry_query_repair_action(connection, args.query_id)
                    identifiers = action["required_identifiers"]
                    manifest_id = str(identifiers["manifest_id"])
                    route_key = str(identifiers["route_key"])
                    red_verification_id = str(identifiers["red_verification_id"])
                    request = parse_registry_query_request(action["query"])
                else:
                    required = {
                        "manifest-id": args.manifest_id,
                        "route-key": args.route_key,
                        "red-verification-id": args.red_verification_id,
                    }
                    missing = [name for name, value in required.items() if not str(value or "").strip()]
                    if missing:
                        raise ScenarioRegistryStoreError(
                            "repair bootstrap requires " + ", ".join("--" + name for name in missing)
                        )
                    manifest_id = str(args.manifest_id)
                    route_key = str(args.route_key)
                    red_verification_id = str(args.red_verification_id)
                    request = parse_registry_query_request(_load_query_request(args))
                manifest = connection.execute(
                    "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?",
                    (manifest_id,),
                ).fetchone()
                if manifest is None:
                    result = asdict(RegistryRepairToken("", False, "manifest_absent"))
                else:
                    declaration = json.loads(str(manifest["declaration_json"]))
                    if not isinstance(declaration, Mapping):
                        raise ScenarioRegistryStoreError("repair manifest declaration must be an object")
                    result = asdict(issue_registry_repair_token(
                        connection,
                        request,
                        manifest_id=manifest_id,
                        route_key=route_key,
                        red_verification_id=red_verification_id,
                        binding=_current_repair_binding(declaration),
                    ))
            elif args.command == "registry-repair-finalize-no-report":
                cleanup = json.loads(args.cleanup_json)
                if not isinstance(cleanup, Mapping):
                    raise ScenarioRegistryStoreError("repair cleanup must be an object")
                result = asdict(terminalize_repair_token_cleanup_without_report(
                    connection,
                    args.repair_token,
                    run_dir=Path(args.run_dir),
                    cleanup=cleanup,
                ))
            elif args.command == "registry-revalidate-bootstrap":
                result = dict(revalidate_current_bootstrap_authority(
                    connection,
                    parse_registry_query_request(_load_query_request(args)),
                    current_facts=_current_bootstrap_revalidation_facts,
                ))
            elif args.command == "registry-status":
                result = {"entries": registry_status(
                    connection,
                    include_lifecycle_states=tuple(args.include_state),
                    manifest_ids=tuple(args.manifest_id),
                )}
            elif args.command == "registry-artifact":
                result = read_command_artifact(
                    artifact_root=registry_path.parent / "command-artifacts",
                    command="registry-status", sha256=args.sha256,
                )
            elif args.command == "runtime-status":
                result = _runtime_status(
                    executable=args.executable,
                    isolated_harness_diagnosis=bool(args.isolated_harness_diagnosis),
                )
            elif args.command == "runtime-status-artifact":
                result = read_command_artifact(
                    artifact_root=registry_path.parent / "command-artifacts",
                    command="runtime-status", sha256=args.sha256,
                )
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
            elif args.command in {"registry-launch", "certification-launch"}:
                bridge_binding_id = str(
                    getattr(args, "cockpit_bridge_binding_id", "") or ""
                ).strip()
                if bridge_binding_id:
                    os.environ["OPENCLAW_COCKPIT_BRIDGE_BINDING_ID"] = bridge_binding_id
                witness_charter = (
                    _load_json_object_file(args.witness_charter, "witness charter")
                    if str(getattr(args, "witness_charter", "") or "").strip()
                    else _witness_charter_from_environment()
                )
                selection = reload_selection_token_for_launch(
                    connection, args.selection_token,
                    witness_charter=witness_charter,
                )
                if not selection.accepted:
                    result = asdict(selection)
                else:
                    readiness = _current_source_executable_readiness()
                    if readiness.get("status") != "ready":
                        record_selection_token_rejection(
                            connection,
                            selection.token_id,
                            reason="source_matching_executable_required",
                            details={"source_executable_readiness": dict(readiness)},
                        )
                        result = asdict(RegistryLaunchToken(
                            token_id=selection.token_id,
                            accepted=False,
                            reason="source_matching_executable_required",
                        ))
                    elif _scenario_requires_bound_live_bridge(selection.scenario) and not \
                            bridge_binding_id and not str(
                                os.environ.get("OPENCLAW_COCKPIT_BRIDGE_BINDING_ID", "")
                            ).strip():
                        result = asdict(RegistryLaunchToken(
                            token_id=selection.token_id,
                            accepted=False,
                            reason="live_cockpit_requires_registry_detached_launch",
                        ))
                    else:
                        try:
                            probe_namespace = _registry_launch_probe_namespace(
                                selection,
                                post_relaunch_continuation=bool(
                                    getattr(args, "post_relaunch_continuation", False)
                                ),
                            )
                            # A registry-owned launch is the canonical executor for
                            # an adaptive semantic window.  Leaving this disabled
                            # makes the run wait for an external caller after the
                            # issuing observation, which cannot produce a bound
                            # transaction receipt or final report evidence.
                            probe_namespace.adaptive_semantic_autodrive = True
                            if args.command == "certification-launch":
                                # The selected fixture is the sealed setup input;
                                # reinstalling that same fixture is not a world or
                                # identity replacement.  It makes retries after
                                # an interrupted setup deterministic.
                                probe_namespace.replace_existing_worlds = True
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
                                certification_setup_installed = False
                                # Only ordinary focused routes may be marked as a
                                # diagnostic replay.  A certification launch owns
                                # its continuous-round authority below.
                                diagnostic_replay = False
                                if args.certification_inputs:
                                    producer_inputs = startup_harness.capture_certification_inputs(args.certification_inputs)
                                elif args.command == "certification-launch":
                                    # Fixture installation is a registry-owned setup
                                    # mutation.  Perform it before sealing the
                                    # manifest, then prevent the probe from
                                    # reinstalling (and thereby changing) the
                                    # sealed world between preflight and recheck.
                                    launch_scenario = startup_harness.load_scenario(selection.scenario)
                                    launch_profile = startup_harness.resolve_profile_name(
                                        str(launch_scenario.get("profile", ""))
                                    )
                                    launch_fixture = str(launch_scenario.get("fixture", "")).strip()
                                    launch_fixture_profile = str(
                                        launch_scenario.get("fixture_profile", "")
                                    ).strip()
                                    if launch_fixture:
                                        startup_harness.install_fixture(
                                            launch_profile,
                                            launch_fixture,
                                            replace=True,
                                            fixture_profile=launch_fixture_profile,
                                        )
                                        certification_setup_installed = True
                                    producer_inputs = startup_harness.derive_registry_owned_certification_inputs(
                                        selection.scenario
                                    )
                                else:
                                    producer_inputs = None
                                if producer_inputs is not None:
                                    route = connection.execute(
                                        "SELECT route_key FROM token_history WHERE token_id = ? AND event_kind = 'issued' "
                                        "ORDER BY token_event_id LIMIT 1", (selection.token_id,),
                                    ).fetchone()
                                    if route is None:
                                        raise ScenarioRegistryStoreError("selected launch token has no route")
                                    created = create_certification_round(
                                        connection,
                                        scenario_lineage_id=Path(selection.source_path).stem,
                                        producer_inputs=producer_inputs,
                                        launch_token=selection.token_id,
                                        launch_source_path=Path(selection.source_path),
                                        launch_route_key=str(route["route_key"]),
                                        current_executable_sha256=str(runtime_binding["executable_sha256"]),
                                    )
                                    round_dir = registry_path.parent / "certification_rounds" / created["manifest"]["round_id"]
                                    round_dir.mkdir(parents=True, exist_ok=False)
                                    manifest_path = round_dir / "round.manifest.json"
                                    recheck_path = round_dir / "current-inputs.json"
                                    manifest_path.write_text(
                                        json.dumps(created["manifest"], ensure_ascii=False, sort_keys=True),
                                        encoding="utf-8",
                                    )
                                    recheck_path.write_text(
                                        json.dumps(producer_inputs, default=str,
                                                   ensure_ascii=False, sort_keys=True),
                                        encoding="utf-8",
                                    )
                                    probe_namespace.certification_registry = str(registry_path)
                                    probe_namespace.certification_round_manifest = str(manifest_path)
                                    probe_namespace.certification_lease_id = uuid.uuid4().hex
                                    probe_namespace.certification_recheck_inputs = str(recheck_path)
                                    probe_namespace.certification_save_capability = created["save_capability"]
                                    wec_authority = created["authority"]
                                    if certification_setup_installed:
                                        probe_namespace.fixture = ""
                                        probe_namespace.fixture_profile = ""
                                else:
                                    launch_scenario = startup_harness.load_scenario(selection.scenario)
                                    runtime_contract = launch_scenario.get("runtime_contract", {})
                                    setup_support = isinstance(runtime_contract, Mapping) and \
                                        runtime_contract.get("setup_only_debug") is True and \
                                        runtime_contract.get("disposable_copy") is True
                                    diagnostic_replay = isinstance(runtime_contract, Mapping) and \
                                        runtime_contract.get("diagnostic_replay") is True
                                    # A disposable setup transaction still needs registry-owned
                                    # authority, run, and executable binding so its immutable,
                                    # zero-credit receipt can be ingested.  Only an explicit
                                    # diagnostic replay is authority-free.
                                    wec_authority = None if diagnostic_replay else issue_wec_authority(
                                        connection,
                                        evidence_class="setup support" if setup_support else "focused feature proof",
                                        authority="registry",
                                        run_id=selection.token_id, binding_id=str(runtime_binding.get("executable_sha256", "")),
                                        source_sha256=path_sha256(Path(selection.source_path)),
                                    )
                                probe_namespace.registry_launch_receipt = json.dumps({
                                    "schema": 1,
                                    "registry_path": str(registry_path),
                                    "token_id": selection.token_id,
                                    "source_path": selection.source_path,
                                    "runtime_binding": runtime_binding,
                                    "wec_authority": wec_authority,
                                    "diagnostic_replay": diagnostic_replay,
                                    "witness_charter": _witness_charter_from_environment(),
                                }, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
                                probe_namespace.registry_post_finalize_hook = _registry_post_finalize_ingest(
                                    probe_namespace.registry_launch_receipt
                                )
                                result = asdict(selection)
            elif args.command == "registry-bootstrap-launch":
                selection = reload_bootstrap_token_for_launch(connection, args.bootstrap_token)
                if not selection.accepted:
                    result = asdict(selection)
                elif _scenario_requires_bound_live_bridge(selection.scenario) and not \
                        str(os.environ.get("OPENCLAW_COCKPIT_BRIDGE_BINDING_ID", "")).strip():
                    # A live session reads its worker protocol from stdin.  The public
                    # bootstrap command has no worker attached, so claiming here would
                    # consume a one-use token and immediately produce a disconnected
                    # zero-action report.  The detached bridge is the canonical route.
                    result = asdict(RegistryBootstrapToken(
                        selection.token_id, False, "cockpit_live_session_requires_bound_file_bridge",
                    ))
                else:
                    comparison = startup_harness.compare_runtime_binding(selection.runtime_binding)
                    if comparison.get("status") != "matched":
                        record_bootstrap_token_rejection(
                            connection,
                            selection.token_id,
                            reason="runtime_binding_changed",
                            details={"comparison": dict(comparison)},
                        )
                        result = asdict(RegistryBootstrapToken(
                            selection.token_id, False, "runtime_binding_changed",
                        ))
                    else:
                        selection = claim_bootstrap_token_for_launch(connection, selection.token_id)
                        if not selection.accepted:
                            result = asdict(selection)
                        else:
                            try:
                                probe_namespace = _registry_bootstrap_probe_namespace(
                                    selection,
                                    cockpit_live_session=bool(args.cockpit_live_session),
                                )
                            except ScenarioRegistryStoreError as exc:
                                record_bootstrap_token_rejection(
                                    connection,
                                    selection.token_id,
                                    reason="canonical_probe_source_mismatch",
                                    details={"error": str(exc)},
                                )
                                result = asdict(RegistryBootstrapToken(
                                    selection.token_id, False, "canonical_probe_source_mismatch",
                                ))
                            else:
                                probe_namespace.adaptive_semantic_autodrive = bool(
                                    args.adaptive_semantic_autodrive
                                )
                                wec_authority = issue_wec_authority(
                                    connection, evidence_class="focused feature proof", authority="registry",
                                    run_id=selection.token_id, binding_id=str(selection.runtime_binding.get("executable_sha256", "")),
                                    source_sha256=path_sha256(Path(selection.source_path)),
                                )
                                probe_namespace.registry_launch_receipt = json.dumps({
                                    "schema": 1,
                                    "authority_kind": "registry_bootstrap_first_compatible_run",
                                    "registry_path": str(registry_path),
                                    "token_id": selection.token_id,
                                    "source_path": selection.source_path,
                                    "runtime_binding": selection.runtime_binding,
                                    "wec_authority": wec_authority,
                                }, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
                                probe_namespace.registry_post_finalize_hook = _registry_post_finalize_ingest(
                                    probe_namespace.registry_launch_receipt
                                )
                                result = asdict(selection)
            elif args.command == "registry-repair-launch":
                issued = connection.execute(
                    "SELECT manifest_id, verification_id FROM token_history "
                    "WHERE token_id = ? AND event_kind = 'repair_issued' "
                    "ORDER BY token_event_id LIMIT 1",
                    (args.repair_token,),
                ).fetchone()
                if issued is None:
                    result = asdict(RegistryRepairToken(args.repair_token, False, "token_unknown"))
                else:
                    manifest = connection.execute(
                        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?",
                        (str(issued["manifest_id"]),),
                    ).fetchone()
                    if manifest is None:
                        result = asdict(RegistryRepairToken(args.repair_token, False, "manifest_absent"))
                    else:
                        declaration = json.loads(str(manifest["declaration_json"]))
                        if not isinstance(declaration, Mapping):
                            raise ScenarioRegistryStoreError("repair manifest declaration must be an object")
                        binding = _current_repair_binding(declaration)
                        selection = reload_repair_token_for_launch(
                            connection, args.repair_token, binding=binding,
                        )
                        if not selection.accepted:
                            result = asdict(selection)
                        else:
                            comparison = startup_harness.compare_runtime_binding(selection.runtime_binding)
                            if comparison.get("status") != "matched":
                                record_repair_token_rejection(
                                    connection,
                                    selection.token_id,
                                    reason="runtime_binding_changed",
                                    details={"comparison": dict(comparison)},
                                )
                                result = asdict(RegistryRepairToken(
                                    selection.token_id, False, "runtime_binding_changed",
                                ))
                            else:
                                selection = claim_repair_token_for_launch(
                                    connection, selection.token_id, binding=binding,
                                )
                                if not selection.accepted:
                                    result = asdict(selection)
                                else:
                                    try:
                                        probe_namespace = _registry_repair_probe_namespace(
                                            selection,
                                            cockpit_live_session=bool(args.cockpit_live_session),
                                        )
                                    except ScenarioRegistryStoreError as exc:
                                        record_repair_token_rejection(
                                            connection,
                                            selection.token_id,
                                            reason="canonical_probe_source_mismatch",
                                            details={"error": str(exc)},
                                        )
                                        result = asdict(RegistryRepairToken(
                                            selection.token_id, False, "canonical_probe_source_mismatch",
                                        ))
                                    else:
                                        # A repair authority is the canonical executor for its
                                        # current semantic transaction.  Leaving autodrive off
                                        # only observes the frame and cannot produce the
                                        # bound receipts that the repair route requires.
                                        probe_namespace.adaptive_semantic_autodrive = True
                                        wec_authority = issue_wec_authority(
                                            connection, evidence_class="focused feature proof", authority="registry",
                                            run_id=selection.token_id,
                                            binding_id=str(selection.runtime_binding.get("executable_sha256", "")),
                                            source_sha256=path_sha256(Path(selection.source_path)),
                                        )
                                        probe_namespace.registry_launch_receipt = json.dumps({
                                            "schema": 1,
                                            "authority_kind": "registry_repair_exact_contradiction",
                                            "registry_path": str(registry_path),
                                            "token_id": selection.token_id,
                                            "red_verification_id": str(issued["verification_id"]),
                                            "source_path": selection.source_path,
                                            "runtime_binding": selection.runtime_binding,
                                            "wec_authority": wec_authority,
                                        }, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
                                        probe_namespace.registry_post_finalize_hook = _registry_post_finalize_ingest(
                                            probe_namespace.registry_launch_receipt
                                        )
                                        result = asdict(selection)
            elif args.command == "registry-repair-compatibility-terminal":
                issued = connection.execute(
                    "SELECT manifest_id FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued' "
                    "ORDER BY token_event_id LIMIT 1", (args.repair_token,),
                ).fetchone()
                if issued is None:
                    result = asdict(RegistryRepairToken(args.repair_token, False, "token_unknown"))
                else:
                    manifest = connection.execute(
                        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (str(issued["manifest_id"]),),
                    ).fetchone()
                    if manifest is None:
                        result = asdict(RegistryRepairToken(args.repair_token, False, "manifest_absent"))
                    else:
                        binding = _current_repair_binding(json.loads(str(manifest["declaration_json"])))
                        selection = reload_repair_token_for_launch(connection, args.repair_token, binding=binding)
                        comparison = startup_harness.compare_runtime_binding(selection.runtime_binding) if selection.accepted else {}
                        if not selection.accepted or comparison.get("status") != "matched":
                            result = asdict(RegistryRepairToken(
                                args.repair_token, False,
                                selection.reason if not selection.accepted else "runtime_binding_changed",
                            ))
                        else:
                            claimed = claim_repair_token_for_launch(connection, args.repair_token, binding=binding)
                            result = (ingest_repair_compatibility_terminal(
                                connection, claimed.token_id, Path(args.terminal_json),
                            ) if claimed.accepted else asdict(claimed))
            elif args.command == "registry-repair-r019-current-source-successor":
                issued = connection.execute(
                    "SELECT manifest_id FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued' "
                    "ORDER BY token_event_id LIMIT 1", (args.repair_token,),
                ).fetchone()
                if issued is None:
                    result = asdict(RegistryRepairToken(args.repair_token, False, "token_unknown"))
                else:
                    manifest = connection.execute(
                        "SELECT declaration_json FROM manifest_current WHERE manifest_id = ?", (str(issued["manifest_id"]),),
                    ).fetchone()
                    if manifest is None:
                        result = asdict(RegistryRepairToken(args.repair_token, False, "manifest_absent"))
                    else:
                        binding = _current_repair_binding(json.loads(str(manifest["declaration_json"])))
                        selection = reload_repair_token_for_launch(connection, args.repair_token, binding=binding)
                        comparison = startup_harness.compare_runtime_binding(selection.runtime_binding) if selection.accepted else {}
                        if not selection.accepted or comparison.get("status") != "matched":
                            result = asdict(RegistryRepairToken(
                                args.repair_token, False,
                                selection.reason if not selection.accepted else "runtime_binding_changed",
                            ))
                        else:
                            claimed = claim_repair_token_for_launch(connection, args.repair_token, binding=binding)
                            result = (ingest_r019_current_source_repair_successor(
                                connection, claimed.token_id, Path(args.successor_json),
                            ) if claimed.accepted else asdict(claimed))
            elif args.command == "registry-r019-aggregation-authorize":
                result = asdict(issue_r019_aggregation_token(
                    connection,
                    guarded_report_id=args.guarded_report_id,
                    primitive_report_id=args.primitive_report_id,
                ))
            elif args.command == "registry-r019-aggregation-finalize":
                result = finalize_r019_aggregation_token(connection, args.aggregation_token)
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
    if args.command in {"registry-launch", "certification-launch", "registry-bootstrap-launch", "registry-repair-launch"}:
        if probe_namespace is None:
            _write_result({
                "ok": False,
                "command": args.command,
                "registry": str(registry_path),
                "result": result,
            }, stream=sys.stderr)
            return 1
        capability = str(getattr(probe_namespace, "certification_save_capability", "") or "")
        prior_capability = os.environ.get("OPENCLAW_CERTIFICATION_SAVE_CAPABILITY")
        if capability:
            os.environ["OPENCLAW_CERTIFICATION_SAVE_CAPABILITY"] = capability
        try:
            return startup_harness.run_probe_mode(probe_namespace)
        except (Exception, SystemExit) as error:
            # A native launch may already have happened before the report owner
            # exists. Revoke ordinary selection authority instead of leaving the
            # same token silently reusable after an unreported adapter failure.
            failure = {"ok": False, "command": args.command,
                       "error": str(error), "error_type": type(error).__name__,
                       "cleanup": "unconfirmed by this exception boundary; inspect retained process ownership"}
            if args.command in {"registry-launch", "certification-launch"}:
                failure["token_id"] = args.selection_token
                try:
                    with open_registry(str(registry_path), writable=True) as failed_connection:
                        failure["selection_invalidated"] = record_selection_token_rejection(
                            failed_connection, args.selection_token,
                            reason="adapter_failed_before_report",
                            details={"error": str(error), "error_type": type(error).__name__,
                                     "scenario": str(getattr(probe_namespace, "scenario", "")),
                                     "cleanup": "unconfirmed", "gameplay_credit": False},
                        )
                except Exception as invalidation_error:
                    failure["selection_invalidation_error"] = str(invalidation_error)
            # Retain exception chains, including cleanup failures; no successful
            # cleanup or gameplay outcome is inferred from a caught exception.
            traceback.print_exc(file=sys.stderr)
            _write_result(failure, stream=sys.stderr)
            return 1
        finally:
            if capability:
                if prior_capability is None:
                    os.environ.pop("OPENCLAW_CERTIFICATION_SAVE_CAPABILITY", None)
                else:
                    os.environ["OPENCLAW_CERTIFICATION_SAVE_CAPABILITY"] = prior_capability
    response = {
        "ok": True,
        "command": args.command,
        "registry": str(registry_path),
        "result": result,
    }
    if args.command == "registry-query":
        receipt = write_command_artifact(artifact_root=registry_path.parent / "command-artifacts",
                                         command="registry-query", payload=response)
        if not args.full:
            response["result"] = query_page(response, receipt, offset=0, page_size=args.page_size,
                                           cli=[sys.executable, str(Path(__file__).resolve()),
                                                "--registry", str(registry_path)])
        _write_result(response)
        return 0
    if args.command in {"registry-status", "runtime-status"} and not bool(args.full):
        receipt = write_command_artifact(
            artifact_root=registry_path.parent / "command-artifacts",
            command=args.command, payload=response,
        )
        if args.command == "registry-status":
            receipt["selector"] = {
                "manifest_ids": list(args.manifest_id),
                "include_states": list(args.include_state),
            }
            receipt["result_summary"] = {"entry_count": len(result["entries"])}
        else:
            runtime_binding = result["runtime_binding"]
            receipt["selector"] = {
                "executable_path": runtime_binding["executable_path"],
                "executable_sha256": runtime_binding["executable_sha256"],
                "runtime_source_sha256": runtime_binding["runtime_source"].get("sha256", ""),
                "isolated_harness_diagnosis": bool(args.isolated_harness_diagnosis),
            }
            receipt["result_summary"] = {
                "status": result["build_runtime_status"].get("status", ""),
                "evidence_ceiling": result["build_runtime_status"].get("evidence_ceiling", ""),
            }
        _write_result({"ok": True, "command": args.command, "registry": str(registry_path),
                       "result": receipt})
    else:
        _write_result(response)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
