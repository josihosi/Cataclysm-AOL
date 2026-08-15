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

from scenario_registry_store import (
    BindingAdapters,
    RegistryLaunchToken,
    ScenarioRegistryStoreError,
    execute_registry_query,
    ingest_report_reference,
    ingest_token_linked_report_reference,
    open_registry,
    rebuild_manifest_projection,
    repository_root,
    resolve_registry_path,
    reconcile_report_bindings,
    record_selection_token_rejection,
    reload_selection_token_for_launch,
    parse_registry_query_request,
    path_sha256,
)
import startup_harness
from startup_harness import (
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
    launch = commands.add_parser(
        "registry-launch",
        help="reload one selection token and run its canonical probe route",
    )
    launch.add_argument("selection_token", help="selection token returned by registry-query")
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
