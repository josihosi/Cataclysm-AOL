#!/usr/bin/env python3
"""Thin maintenance CLI for the authoritative SQLite scenario registry."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
from typing import Any, Dict, Mapping, Sequence

from scenario_registry_store import (
    BindingAdapters,
    ScenarioRegistryStoreError,
    ingest_report_reference,
    open_registry,
    rebuild_manifest_projection,
    repository_root,
    resolve_registry_path,
    reconcile_report_bindings,
)
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


def _path_sha256(path: Path) -> str:
    """Hash the real file or directory tree that an adapter resolves."""
    digest = hashlib.sha256()
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise ScenarioRegistryStoreError(f"Could not resolve binding path {path}: {exc}") from exc
    if resolved.is_file():
        value, error = sha256_file(resolved)
        if error:
            raise ScenarioRegistryStoreError(f"Could not hash binding file {resolved}: {error}")
        return value
    if not resolved.is_dir():
        raise ScenarioRegistryStoreError(f"Binding path is neither a file nor directory: {resolved}")
    digest.update(b"caol-scenario-directory-binding-v1\0")
    for entry in sorted(resolved.rglob("*"), key=lambda item: str(item.relative_to(resolved))):
        relative = str(entry.relative_to(resolved)).replace("\\", "/")
        if entry.is_dir():
            digest.update(b"directory\0" + relative.encode("utf-8") + b"\0")
            continue
        if not entry.is_file():
            raise ScenarioRegistryStoreError(f"Binding tree contains unsupported entry: {entry}")
        value, error = sha256_file(entry)
        if error:
            raise ScenarioRegistryStoreError(f"Could not hash binding file {entry}: {error}")
        digest.update(b"file\0" + relative.encode("utf-8") + b"\0" + value.encode("ascii") + b"\0")
    return digest.hexdigest()


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
        source_sha256 = _path_sha256(source_path)
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
        source_sha256 = _path_sha256(source_path)
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
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    registry_path = resolve_registry_path(args.registry)
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
            else:
                raise ScenarioRegistryStoreError(f"Unsupported registry command: {args.command}")
        finally:
            connection.close()
    except (OSError, sqlite3.Error, ScenarioRegistryStoreError, SystemExit, ValueError) as exc:
        _write_result({"ok": False, "command": args.command, "error": str(exc)}, stream=sys.stderr)
        return 1
    _write_result({
        "ok": True,
        "command": args.command,
        "registry": str(registry_path),
        "result": result,
    })
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
