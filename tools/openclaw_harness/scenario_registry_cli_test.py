#!/usr/bin/env python3
"""Subprocess contracts for the thin scenario-registry maintenance CLI."""

from __future__ import annotations

import hashlib
import io
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
CLI_PATH = HARNESS_DIR / "scenario_registry_cli.py"
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_cli  # noqa: E402
import startup_harness  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    execute_registry_query,
    ingest_report_reference,
    open_registry,
    parse_registry_query_request,
    rebuild_manifest_projection,
    reconcile_report_bindings,
)
from startup_harness import runtime_source_binding  # noqa: E402


class ScenarioRegistryCliTest(unittest.TestCase):
    def strict_manifest(self) -> dict:
        return {
            "manifest_version": 1,
            "name": "cli.registry.fixture",
            "steps": [
                {"label": "setup", "kind": "wait"},
                {"label": "production", "kind": "press"},
                {"label": "terminal", "kind": "audit_log_contains"},
                {"label": "artifact", "kind": "audit_log_contains"},
                {"label": "shortcut", "kind": "audit_log_not_contains"},
            ],
            "capabilities": {"player.injured": False},
            "runtime_contract": {
                "permitted_input": ["press:f"],
                "forbidden_input": ["debug:spawn"],
                "setup_only_debug": True,
                "disposable_copy": True,
                "helpers": ["Peekaboo"],
                "permissions": ["accessibility"],
                "platform": ["macos"],
                "profile": "dev-harness",
                "fixture": "fixture-a",
                "requirements": {
                    "os": "macos",
                    "source": "worktree",
                    "executable": "cataclysm-tiles",
                    "profile": "dev-harness",
                    "fixture": "fixture-a",
                    "helper": "Peekaboo",
                    "peekaboo": True,
                    "input": ["press:f"],
                    "ocr": True,
                    "cleanup": True,
                },
                "grants_gameplay_proof": False,
            },
            "proof_route": {
                "precondition": ["setup"],
                "production_behavior": ["production"],
                "terminal_persistence": ["terminal"],
                "artifact_verdict": ["artifact"],
                "disallowed_shortcuts": ["shortcut"],
            },
        }

    def write_json(self, path: Path, value: object) -> None:
        path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

    def run_cli(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CLI_PATH), *arguments],
            check=False,
            capture_output=True,
            text=True,
        )

    def report(self, manifest_path: Path, executable_path: Path) -> dict:
        source = runtime_source_binding()
        self.assertTrue(source.get("ok"), source.get("error"))
        return {
            "mode": "probe",
            "scenario": "cli.registry.fixture",
            "contract": {
                "profile": "",
                "config_profile": "",
                "fixture": "",
                "fixture_profile": "",
                "profile_snapshot": "",
                "profile_snapshot_profile": "",
            },
            "scenario_manifest": {
                "source": {
                    "path": str(manifest_path.resolve()),
                    "sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                }
            },
            "startup": {
                "screen": {
                    "runtime_binding_status": "compatible",
                    "runtime_binding_observed": {
                        "executable_path": str(executable_path.resolve()),
                        "executable_sha256": hashlib.sha256(executable_path.read_bytes()).hexdigest(),
                        "runtime_source_sha256": source["sha256"],
                    },
                },
                "fixture_install": {},
                "profile_snapshot": {},
            },
            "proof_classification": {
                "status": "green",
                "verdict": "probe_completed",
                "evidence_class": "feature-path",
                "feature_proof": True,
            },
            "verdict": "green_route_completed",
            "evidence_class": "feature-path",
            "feature_proof": True,
        }

    def issue_selection_token(self, root: Path) -> tuple[Path, Path, str]:
        scenarios = root / "scenarios"
        scenarios.mkdir()
        manifest_path = scenarios / "cli.json"
        self.write_json(manifest_path, self.strict_manifest())
        executable_path = root / "cataclysm-tiles"
        executable_path.write_bytes(b"registry launch runtime fixture")
        report_path = root / "probe.report.json"
        self.write_json(report_path, self.report(manifest_path, executable_path))
        registry_path = root / "registry.sqlite3"
        connection = open_registry(str(registry_path))
        try:
            rebuild_manifest_projection(connection, scenarios)
            adapters = BindingAdapters(
                runtime=lambda _expected: {"status": "compatible", "facts": {}},
                fixture=lambda _expected: {"status": "compatible", "facts": {
                    "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                }},
                profile=lambda _expected: {"status": "compatible", "facts": {
                    "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                }},
            )
            ingested = ingest_report_reference(connection, report_path, adapters=adapters)
            self.assertEqual(ingested["eligibility"], "hard_proven")
            issued = execute_registry_query(
                connection,
                parse_registry_query_request({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertIsNotNone(issued.token_id)
            return registry_path, scenarios, str(issued.token_id)
        finally:
            connection.close()

    def token_events(self, registry_path: Path, token_id: str) -> list[tuple[str, str]]:
        connection = sqlite3.connect(registry_path)
        try:
            return [
                tuple(row)
                for row in connection.execute(
                    "SELECT event_kind, reason FROM token_history WHERE token_id = ? ORDER BY token_event_id",
                    (token_id,),
                ).fetchall()
            ]
        finally:
            connection.close()

    def run_registry_launch(
        self,
        registry_path: Path,
        scenarios: Path,
        token_id: str,
    ) -> tuple[int, mock.Mock]:
        with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                mock.patch.object(startup_harness, "run_probe_mode", return_value=23) as run_probe, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = scenario_registry_cli.main([
                "--registry", str(registry_path), "registry-launch", token_id,
            ])
        return result, run_probe

    def test_registry_launch_adapts_exact_probe_namespace_and_honors_registry_override(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            other_registry = root / "other.sqlite3"
            other_connection = open_registry(str(other_registry))
            try:
                rebuild_manifest_projection(other_connection, scenarios)
            finally:
                other_connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 23)
            run_probe.assert_called_once()
            expected = startup_harness.build_parser().parse_args(["probe", "cli"])
            self.assertEqual(vars(run_probe.call_args.args[0]), vars(expected))
            self.assertEqual(self.token_events(registry_path, token_id), [("issued", "query_selection")])
            other_connection = sqlite3.connect(other_registry)
            try:
                self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM token_history").fetchone()[0], 0)
                self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 0)
            finally:
                other_connection.close()

    def test_registry_launch_rejects_changed_source_without_requery_or_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            manifest_path = scenarios / "cli.json"
            changed = self.strict_manifest()
            changed["description"] = "changed after selection"
            self.write_json(manifest_path, changed)

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_manifest_source_changed"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_registry_launch_rejects_stale_token_without_requery_or_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            connection = open_registry(str(registry_path))
            try:
                stale_adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "stale", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=stale_adapters),
                    {"reconciled": 1, "stale": 1},
                )
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "proof_route_stale"),
                ("invalidated", "registry_launch_token_invalidated"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_registry_launch_rejects_changed_route_binding_without_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            connection = open_registry(str(registry_path))
            try:
                changed_binding_adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"changed fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=changed_binding_adapters),
                    {"reconciled": 1, "stale": 0},
                )
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_route_binding_changed"),
            ])

    def test_registry_launch_records_malformed_token_rejection_without_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            malformed_token = "malformed-selection-token"
            connection = sqlite3.connect(registry_path)
            try:
                issued = connection.execute(
                    "SELECT manifest_id, verification_id, route_key FROM token_history "
                    "WHERE token_id = ? AND event_kind = 'issued'",
                    (token_id,),
                ).fetchone()
                connection.execute(
                    "INSERT INTO token_history( "
                    "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
                    ") VALUES( ?, ?, ?, ?, 'issued', 'test_invalid_receipt', '{}' )",
                    (malformed_token, issued[0], issued[1], issued[2]),
                )
                connection.commit()
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, malformed_token)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, malformed_token), [
                ("issued", "test_invalid_receipt"),
                ("invalidated", "registry_launch_receipt_malformed"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_rebuild_ingest_reconcile_use_the_requested_registry_only(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "cli.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable_path = root / "cataclysm-tiles"
            executable_path.write_bytes(b"cli runtime fixture")
            report_path = root / "probe.report.json"
            self.write_json(report_path, self.report(manifest_path, executable_path))
            registry_path = root / "requested.sqlite3"
            other_registry_path = root / "other.sqlite3"

            rebuilt = self.run_cli(
                "--registry", str(registry_path), "rebuild", "--scenarios-root", str(scenarios)
            )
            self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr)
            self.assertEqual(json.loads(rebuilt.stdout)["result"], {
                "staged": 1, "discovered": 1, "changed": 0, "absent": 0,
            })

            ingested = self.run_cli(
                "--registry", str(registry_path), "ingest-report", "--report", str(report_path)
            )
            self.assertEqual(ingested.returncode, 0, ingested.stderr)
            self.assertEqual(json.loads(ingested.stdout)["result"]["status"], "ingested")
            self.assertEqual(json.loads(ingested.stdout)["result"]["eligibility"], "hard_proven")

            reconciled = self.run_cli("--registry", str(registry_path), "reconcile")
            self.assertEqual(reconciled.returncode, 0, reconciled.stderr)
            self.assertEqual(json.loads(reconciled.stdout)["result"], {"reconciled": 1, "stale": 0})

            selected = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-json", json.dumps({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertEqual(selected.returncode, 0, selected.stderr)
            selected_result = json.loads(selected.stdout)["result"]
            self.assertIsNotNone(selected_result["token_id"])
            self.assertIsNone(selected_result["draft_path"])
            self.assertEqual(selected_result["evaluation"]["evaluation"]["ranked_scenario_ids"], [
                selected_result["evaluation"]["candidates"][0]["scenario_id"],
            ])
            self.assertEqual(
                selected_result["evaluation"]["candidates"][0]["explanation"]["lifecycle"]["state"],
                "active",
            )

            no_match_query = {
                "requirements": [{
                    "key": "player.injured",
                    "op": "eq",
                    "value": True,
                    "minimum_evidence": "declared",
                }],
                "preferences": [],
            }
            no_match_path = root / "no-match.query.json"
            self.write_json(no_match_path, no_match_query)
            no_match = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-file", str(no_match_path)
            )
            self.assertEqual(no_match.returncode, 0, no_match.stderr)
            no_match_result = json.loads(no_match.stdout)["result"]
            self.assertIsNone(no_match_result["token_id"])
            self.assertTrue(Path(no_match_result["draft_path"]).is_file())
            self.assertFalse(json.loads(Path(no_match_result["draft_path"]).read_text(encoding="utf-8"))["executable"])

            connection = sqlite3.connect(registry_path)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM manifest_current").fetchone()[0], 1)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM report_ingestion_history").fetchone()[0], 1)
            self.assertEqual(
                connection.execute(
                    "SELECT resolution_kind FROM verification_resolution_history "
                    "ORDER BY resolution_event_id DESC LIMIT 1"
                ).fetchone()[0],
                "compatible",
            )
            self.assertEqual(
                connection.execute(
                    "SELECT evidence_state FROM capability_evidence_history "
                    "WHERE capability_key = '_registry.proof_route' "
                    "ORDER BY capability_evidence_id DESC LIMIT 1"
                ).fetchone()[0],
                "hard_proven",
            )
            connection.close()

            isolated = self.run_cli(
                "--registry", str(other_registry_path), "rebuild", "--scenarios-root", str(scenarios)
            )
            self.assertEqual(isolated.returncode, 0, isolated.stderr)
            other_connection = sqlite3.connect(other_registry_path)
            self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM manifest_current").fetchone()[0], 1)
            self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM report_ingestion_history").fetchone()[0], 0)
            other_connection.close()

            isolated_query = self.run_cli(
                "--registry", str(other_registry_path), "registry-query", "--query-json", json.dumps({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertEqual(isolated_query.returncode, 0, isolated_query.stderr)
            self.assertIsNone(json.loads(isolated_query.stdout)["result"]["token_id"])

    def test_invalid_invocation_is_machine_readable_and_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            registry_path = Path(temp_dir) / "registry.sqlite3"
            result = self.run_cli("--registry", str(registry_path), "ingest-report")
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, "")
            error = json.loads(result.stderr)
            self.assertFalse(error["ok"])
            self.assertIn("--report", error["error"])
            self.assertFalse(registry_path.exists())

            missing_query = self.run_cli("--registry", str(registry_path), "registry-query")
            self.assertEqual(missing_query.returncode, 2)
            self.assertIn("one of the arguments", json.loads(missing_query.stderr)["error"])

            conflicting_query = self.run_cli(
                "--registry", str(registry_path), "registry-query",
                "--query-json", "{}", "--query-file", str(registry_path),
            )
            self.assertEqual(conflicting_query.returncode, 2)
            self.assertIn("not allowed with argument", json.loads(conflicting_query.stderr)["error"])

            malformed_query = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-json", "{",
            )
            self.assertEqual(malformed_query.returncode, 1)
            self.assertIn("valid JSON", json.loads(malformed_query.stderr)["error"])


if __name__ == "__main__":
    unittest.main()
