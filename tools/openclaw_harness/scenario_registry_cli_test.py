#!/usr/bin/env python3
"""Subprocess contracts for the thin scenario-registry maintenance CLI."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest


HARNESS_DIR = Path(__file__).resolve().parent
CLI_PATH = HARNESS_DIR / "scenario_registry_cli.py"
sys.path.insert(0, str(HARNESS_DIR))

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


if __name__ == "__main__":
    unittest.main()
