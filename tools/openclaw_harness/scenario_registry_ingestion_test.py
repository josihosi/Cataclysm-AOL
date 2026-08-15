#!/usr/bin/env python3
"""Focused report-reference ingestion and binding reconciliation contracts."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    ingest_report_reference,
    open_registry,
    rebuild_manifest_projection,
    reconcile_report_bindings,
)


OPAQUE_PROSE = "opaque report prose must never be copied into sqlite"


class ScenarioRegistryIngestionTest(unittest.TestCase):
    def strict_manifest(self) -> dict:
        return {
            "manifest_version": 1,
            "name": "strict.ingestion.fixture",
            "description": "declaration text is not report evidence",
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

    def report(self, manifest_path: Path) -> dict:
        source_bytes = manifest_path.read_bytes()
        return {
            "mode": "probe",
            "scenario": "strict.ingestion.fixture",
            "contract": {
                "profile": "dev-harness",
                "config_profile": "dev-harness",
                "fixture": "fixture-a",
                "fixture_profile": "dev-harness",
                "profile_snapshot": "baseline",
                "profile_snapshot_profile": "dev-harness",
            },
            "scenario_manifest": {
                "source": {
                    "path": str(manifest_path.resolve()),
                    "sha256": hashlib.sha256(source_bytes).hexdigest(),
                }
            },
            "startup": {
                "screen": {
                    "runtime_binding_status": "compatible",
                    "runtime_binding_observed": {
                        "status": "compatible",
                        "executable_path": "/tmp/cataclysm-tiles",
                        "executable_sha256": "runtime-observed-hash",
                    },
                },
                "fixture_install": {
                    "fixture": "fixture-a",
                    "fixture_profile": "dev-harness",
                    "destination": "/tmp/profile/save",
                },
                "profile_snapshot": {
                    "profile": "dev-harness",
                    "snapshot": "baseline",
                    "source_path": "/tmp/baseline",
                },
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
            "opaque_report_payload": {"prose": OPAQUE_PROSE, "nested": [OPAQUE_PROSE]},
        }

    def adapters(self, state: dict) -> BindingAdapters:
        def result(kind: str, expected: object) -> dict:
            self.assertIsInstance(expected, dict)
            return {
                "status": state[kind],
                # Fixture/profile source hashes come from these explicit current-owner adapters,
                # never from report fixture/profile display names.
                "facts": {
                    "source_sha256": state.get(
                        f"{kind}_sha",
                        hashlib.sha256(f"current-{kind}".encode("utf-8")).hexdigest(),
                    )
                },
            }

        return BindingAdapters(
            runtime=lambda expected: result("runtime", expected),
            fixture=lambda expected: result("fixture", expected),
            profile=lambda expected: result("profile", expected),
        )

    def setup_registry(self, root: Path) -> tuple:
        scenarios = root / "scenarios"
        scenarios.mkdir()
        manifest_path = scenarios / "strict.json"
        self.write_json(manifest_path, self.strict_manifest())
        report_path = root / "full.probe.report.json"
        self.write_json(report_path, self.report(manifest_path))
        connection = open_registry(str(root / "registry.sqlite3"))
        rebuild_manifest_projection(connection, scenarios)
        return connection, scenarios, manifest_path, report_path

    def test_ingestion_is_exact_reference_idempotent_and_never_copies_prose(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}

            first = ingest_report_reference(connection, report_path, adapters=self.adapters(state))
            self.assertEqual(first["status"], "ingested")
            self.assertEqual(first["resolution"], "compatible")
            expected_sha256 = hashlib.sha256(report_path.read_bytes()).hexdigest()
            reference = connection.execute(
                "SELECT report_path, report_sha256, ingestion_status, error_text, manifest_id "
                "FROM report_ingestion_history"
            ).fetchone()
            self.assertEqual(
                tuple(reference)[:4],
                (str(report_path.resolve()), expected_sha256, "ingested", ""),
            )
            self.assertEqual(
                reference["manifest_id"],
                connection.execute(
                    "SELECT manifest_id FROM manifest_current WHERE source_path = ?",
                    (str(manifest_path.resolve()),),
                ).fetchone()[0],
            )
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM verification_history").fetchone()[0], 1)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM binding_history").fetchone()[0], 5)
            self.assertEqual(
                connection.execute("SELECT resolution_kind FROM verification_resolution_history").fetchone()[0],
                "compatible",
            )
            verification_details = json.loads(
                connection.execute("SELECT details_json FROM verification_history").fetchone()[0]
            )
            self.assertEqual(verification_details["proof"]["route_verdict"], "green_route_completed")
            self.assertNotIn(OPAQUE_PROSE, "\n".join(connection.iterdump()))

            counts = {
                table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("report_ingestion_history", "verification_history", "binding_history", "verification_resolution_history")
            }
            duplicate = ingest_report_reference(connection, report_path, adapters=self.adapters(state))
            self.assertTrue(duplicate["idempotent"])
            self.assertEqual(
                {
                    table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                    for table in counts
                },
                counts,
            )
            self.assertEqual(
                reconcile_report_bindings(connection, adapters=self.adapters(state)),
                {"reconciled": 1, "stale": 0},
            )
            self.assertEqual(
                {
                    table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                    for table in counts
                },
                counts,
            )
            connection.close()

    def test_reconciliation_records_stale_events_for_every_bound_component(self) -> None:
        for component in ("report", "manifest", "runtime", "fixture", "profile"):
            with self.subTest(component=component), tempfile.TemporaryDirectory() as temp_dir:
                connection, scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
                state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
                ingest_report_reference(connection, report_path, adapters=self.adapters(state))

                if component == "report":
                    changed = self.report(manifest_path)
                    changed["opaque_report_payload"]["prose"] = "changed report bytes"
                    self.write_json(report_path, changed)
                elif component == "manifest":
                    changed = self.strict_manifest()
                    changed["description"] = "changed declaration bytes"
                    self.write_json(manifest_path, changed)
                    rebuild_manifest_projection(connection, scenarios)
                else:
                    state[component] = "stale"
                    state[f"{component}_sha"] = hashlib.sha256(
                        f"changed-{component}".encode("utf-8")
                    ).hexdigest()

                result = reconcile_report_bindings(connection, adapters=self.adapters(state))
                self.assertEqual(result, {"reconciled": 1, "stale": 1})
                self.assertEqual(
                    connection.execute(
                        "SELECT resolution_kind FROM verification_resolution_history "
                        "ORDER BY resolution_event_id DESC LIMIT 1"
                    ).fetchone()[0],
                    "stale",
                )
                self.assertIsNotNone(
                    connection.execute(
                        "SELECT binding_event_id FROM binding_history "
                        "WHERE binding_kind = ? AND binding_status = 'stale'",
                        (component,),
                    ).fetchone()
                )
                self.assertGreaterEqual(
                    connection.execute(
                        "SELECT COUNT(*) FROM binding_history WHERE binding_kind = ?",
                        (component,),
                    ).fetchone()[0],
                    2,
                )
                connection.close()

    def test_fixture_and_profile_display_names_cannot_substitute_for_source_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, _manifest_path, report_path = self.setup_registry(Path(temp_dir))
            valid_hash = hashlib.sha256(b"runtime-current").hexdigest()
            invalid = BindingAdapters(
                runtime=lambda _expected: {"status": "compatible", "facts": {"source_sha256": valid_hash}},
                fixture=lambda _expected: {"status": "compatible", "facts": {"source_sha256": "fixture-a"}},
                profile=lambda _expected: {"status": "compatible", "facts": {"source_sha256": valid_hash}},
            )
            result = ingest_report_reference(connection, report_path, adapters=invalid)
            self.assertEqual(result["status"], "rejected_manifest")
            self.assertIn("SHA-256", result["error"])
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM verification_history").fetchone()[0], 0)
            connection.close()


if __name__ == "__main__":
    unittest.main()
