#!/usr/bin/env python3
"""Focused report-reference ingestion and binding reconciliation contracts."""

from __future__ import annotations

import hashlib
import json
import sqlite3
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    build_registry_query_candidate_snapshot,
    execute_registry_query,
    evaluate_registry_query_from_store,
    ingest_report_reference,
    open_registry,
    parse_registry_query_request,
    rebuild_manifest_projection,
    reconcile_report_bindings,
)
import startup_harness  # noqa: E402


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

    def report_with_proof(
        self,
        manifest_path: Path,
        *,
        status: str,
        feature_proof: bool,
        supersedes_verification_id: str = "",
    ) -> dict:
        report = self.report(manifest_path)
        report["proof_classification"].update({
            "status": status,
            "evidence_class": "feature-path" if feature_proof else "startup/load-or-inconclusive",
            "feature_proof": feature_proof,
        })
        report["verdict"] = f"{status}_route_result"
        report["evidence_class"] = report["proof_classification"]["evidence_class"]
        report["feature_proof"] = feature_proof
        if supersedes_verification_id:
            report["supersedes_verification_id"] = supersedes_verification_id
        return report

    def ingest_named_report(
        self,
        connection: object,
        root: Path,
        name: str,
        report: dict,
        state: dict,
    ) -> dict:
        path = root / name
        self.write_json(path, report)
        return ingest_report_reference(connection, path, adapters=self.adapters(state))

    def test_declarations_and_unknown_reports_never_hard_prove(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, _report_path = self.setup_registry(root)
            declared = connection.execute(
                "SELECT evidence_state, value_json FROM capability_evidence_history "
                "WHERE capability_key = 'player.injured' AND evidence_kind = 'declaration'"
            ).fetchone()
            self.assertEqual((declared["evidence_state"], declared["value_json"]), ("declared", "false"))
            declared_count = connection.execute(
                "SELECT COUNT(*) FROM capability_evidence_history WHERE evidence_kind = 'declaration'"
            ).fetchone()[0]
            rebuild_manifest_projection(connection, _scenarios)
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM capability_evidence_history WHERE evidence_kind = 'declaration'"
                ).fetchone()[0],
                declared_count,
            )

            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            result = self.ingest_named_report(
                connection,
                root,
                "unknown.probe.report.json",
                self.report_with_proof(manifest_path, status="yellow", feature_proof=False),
                state,
            )
            self.assertEqual(result["eligibility"], "unknown")
            evidence = connection.execute(
                "SELECT evidence_state, details_json FROM capability_evidence_history "
                "WHERE capability_key = '_registry.proof_route' AND evidence_kind = 'route_resolution' "
                "ORDER BY capability_evidence_id DESC LIMIT 1"
            ).fetchone()
            self.assertEqual(evidence["evidence_state"], "unknown")
            self.assertEqual(json.loads(evidence["details_json"])["hard_proven_verification_ids"], [])
            connection.close()

    def test_compatible_contradiction_requires_explicit_same_route_supersession(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, _report_path = self.setup_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            contradiction = self.ingest_named_report(
                connection,
                root,
                "contradiction.probe.report.json",
                self.report_with_proof(manifest_path, status="red", feature_proof=False),
                state,
            )
            unsuperseded_success = self.ingest_named_report(
                connection,
                root,
                "later-success.probe.report.json",
                self.report_with_proof(manifest_path, status="green", feature_proof=True),
                state,
            )
            self.assertEqual(contradiction["eligibility"], "contradicted")
            self.assertEqual(unsuperseded_success["eligibility"], "contradicted")

            superseding_success = self.ingest_named_report(
                connection,
                root,
                "superseding-success.probe.report.json",
                self.report_with_proof(
                    manifest_path,
                    status="green",
                    feature_proof=True,
                    supersedes_verification_id=contradiction["verification_id"],
                ),
                state,
            )
            self.assertEqual(superseding_success["eligibility"], "hard_proven")
            verification = connection.execute(
                "SELECT route_key, supersedes_verification_id FROM verification_history WHERE verification_id = ?",
                (superseding_success["verification_id"],),
            ).fetchone()
            self.assertEqual(verification["supersedes_verification_id"], contradiction["verification_id"])
            evidence = connection.execute(
                "SELECT evidence_state, details_json FROM capability_evidence_history "
                "WHERE capability_key = '_registry.proof_route' AND evidence_kind = 'route_resolution' "
                "ORDER BY capability_evidence_id DESC LIMIT 1"
            ).fetchone()
            self.assertEqual(evidence["evidence_state"], "hard_proven")
            details = json.loads(evidence["details_json"])
            self.assertEqual(details["unresolved_contradiction_ids"], [])
            self.assertEqual(details["superseded_contradiction_ids"], [contradiction["verification_id"]])
            connection.close()

    def test_stale_evidence_quarantines_and_invalidates_tokens_without_erasing_history(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            ingested = ingest_report_reference(connection, report_path, adapters=self.adapters(state))
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
                drafts_root=root / "drafts",
            )
            self.assertIsNotNone(issued.token_id)
            verification = connection.execute(
                "SELECT manifest_id, route_key FROM verification_history WHERE verification_id = ?",
                (ingested["verification_id"],),
            ).fetchone()
            stale_report = self.report(manifest_path)
            stale_report["opaque_report_payload"]["prose"] = "changed report bytes"
            self.write_json(report_path, stale_report)

            self.assertEqual(reconcile_report_bindings(connection, adapters=self.adapters(state)), {"reconciled": 1, "stale": 1})
            self.assertEqual(
                connection.execute(
                    "SELECT evidence_state FROM capability_evidence_history "
                    "WHERE capability_key = '_registry.proof_route' AND evidence_kind = 'route_resolution' "
                    "ORDER BY capability_evidence_id DESC LIMIT 1"
                ).fetchone()[0],
                "stale",
            )
            self.assertEqual(
                connection.execute(
                    "SELECT quarantine_kind FROM quarantine_history ORDER BY quarantine_event_id DESC LIMIT 1"
                ).fetchone()[0],
                "quarantined_no_compatible_verification",
            )
            self.assertEqual(
                connection.execute(
                    "SELECT event_kind FROM lifecycle_history WHERE manifest_id = ? "
                    "ORDER BY lifecycle_event_id DESC LIMIT 1",
                    (verification["manifest_id"],),
                ).fetchone()[0],
                "proof_route_stale",
            )
            self.assertEqual(
                [
                    tuple(row)
                    for row in connection.execute(
                        "SELECT event_kind, reason FROM token_history WHERE token_id = ? "
                        "ORDER BY token_event_id",
                        (issued.token_id,),
                    ).fetchall()
                ],
                [("issued", "query_selection"), ("invalidated", "proof_route_stale")],
            )
            self.assertGreaterEqual(
                connection.execute("SELECT COUNT(*) FROM binding_history WHERE binding_kind = 'report'").fetchone()[0],
                2,
            )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "UPDATE capability_evidence_history SET evidence_state = 'rewritten' "
                    "WHERE capability_key = '_registry.proof_route'"
                )
            counts = {
                table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("capability_evidence_history", "lifecycle_history", "quarantine_history", "token_history")
            }
            reconcile_report_bindings(connection, adapters=self.adapters(state))
            self.assertEqual(
                {
                    table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                    for table in counts
                },
                counts,
            )
            connection.close()

    def test_query_audit_token_manifest_invalidation_and_inert_draft_are_isolated(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, scenarios, manifest_path, report_path = self.setup_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            ingest_report_reference(connection, report_path, adapters=self.adapters(state))
            valid = parse_registry_query_request({
                "requirements": [{
                    "key": "player.injured",
                    "op": "eq",
                    "value": False,
                    "minimum_evidence": "declared",
                }],
                "preferences": [],
            })
            forbidden = ["run_startup", "run_probe_mode", "launch_game", "peekaboo_command"]
            with mock.patch.object(startup_harness, forbidden[0], side_effect=AssertionError), \
                    mock.patch.object(startup_harness, forbidden[1], side_effect=AssertionError), \
                    mock.patch.object(startup_harness, forbidden[2], side_effect=AssertionError), \
                    mock.patch.object(startup_harness, forbidden[3], side_effect=AssertionError):
                selected = execute_registry_query(connection, valid, drafts_root=root / "drafts")

            self.assertIsNotNone(selected.token_id)
            self.assertIsNone(selected.draft_path)
            token = connection.execute(
                "SELECT manifest_id, verification_id, route_key, details_json FROM token_history "
                "WHERE token_id = ? AND event_kind = 'issued'",
                (selected.token_id,),
            ).fetchone()
            self.assertIsNotNone(token)
            details = json.loads(token["details_json"])
            self.assertEqual(details["query_sha256"], selected.query_sha256)
            self.assertEqual(details["manifest_sha256"], hashlib.sha256(manifest_path.read_bytes()).hexdigest())
            self.assertEqual(details["selected_values"]["player.injured"]["value"], False)
            self.assertTrue(details["route_evidence"]["bindings"])

            changed = self.strict_manifest()
            changed["description"] = "query token invalidation control"
            self.write_json(manifest_path, changed)
            rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(
                connection.execute(
                    "SELECT reason FROM token_history WHERE token_id = ? AND event_kind = 'invalidated'",
                    (selected.token_id,),
                ).fetchone()[0],
                "manifest_changed",
            )
            self.assertEqual(build_registry_query_candidate_snapshot(connection), ())

            no_match = parse_registry_query_request({
                "requirements": [{
                    "key": "player.injured",
                    "op": "eq",
                    "value": True,
                    "minimum_evidence": "declared",
                }],
                "preferences": [],
            })
            draft = execute_registry_query(
                connection,
                no_match,
                include_lifecycle_states=("quarantined",),
                drafts_root=root / "drafts",
            )
            self.assertIsNone(draft.token_id)
            self.assertEqual(draft.draft_path, str(root / "drafts" / f"{draft.query_sha256}.json"))
            artifact = json.loads(Path(draft.draft_path).read_text(encoding="utf-8"))
            draft_bytes = Path(draft.draft_path).read_bytes()
            self.assertEqual(artifact["review_status"], "pending")
            self.assertFalse(artifact["executable"])
            self.assertEqual(artifact["query"]["requirements"][0]["value"], True)
            self.assertEqual(artifact["unmet_capabilities"][0]["unmet"][0]["reason"], "stale")
            self.assertEqual(artifact["candidate_manifest"]["fixture"], "fixture-a")
            repeated = execute_registry_query(
                connection,
                no_match,
                include_lifecycle_states=("quarantined",),
                drafts_root=root / "drafts",
            )
            self.assertIsNone(repeated.token_id)
            self.assertEqual(repeated.draft_path, draft.draft_path)
            self.assertEqual(Path(repeated.draft_path).read_bytes(), draft_bytes)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 3)
            connection.close()

    def test_compatible_surviving_verification_prevents_stale_quarantine_and_token_invalidation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, first_report_path = self.setup_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            first = ingest_report_reference(connection, first_report_path, adapters=self.adapters(state))
            second_path = root / "second.probe.report.json"
            self.write_json(second_path, self.report(manifest_path))
            second = ingest_report_reference(connection, second_path, adapters=self.adapters(state))
            verification = connection.execute(
                "SELECT manifest_id, route_key FROM verification_history WHERE verification_id = ?",
                (first["verification_id"],),
            ).fetchone()
            self.assertEqual(
                verification["route_key"],
                connection.execute(
                    "SELECT route_key FROM verification_history WHERE verification_id = ?",
                    (second["verification_id"],),
                ).fetchone()[0],
            )
            connection.execute(
                "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                "VALUES( 'token-survives', ?, ?, ?, 'issued', 'selection', '{}' )",
                (verification["manifest_id"], first["verification_id"], verification["route_key"]),
            )
            changed = self.report(manifest_path)
            changed["opaque_report_payload"]["prose"] = "changed second report bytes"
            self.write_json(second_path, changed)

            self.assertEqual(reconcile_report_bindings(connection, adapters=self.adapters(state)), {"reconciled": 2, "stale": 1})
            self.assertIsNone(connection.execute("SELECT quarantine_event_id FROM quarantine_history").fetchone())
            self.assertEqual(
                [
                    tuple(row)
                    for row in connection.execute(
                        "SELECT event_kind FROM token_history WHERE token_id = 'token-survives'"
                    ).fetchall()
                ],
                [("issued",)],
            )
            self.assertEqual(
                connection.execute(
                    "SELECT evidence_state FROM capability_evidence_history "
                    "WHERE capability_key = '_registry.proof_route' AND evidence_kind = 'route_resolution' "
                    "ORDER BY capability_evidence_id DESC LIMIT 1"
                ).fetchone()[0],
                "hard_proven",
            )
            connection.close()

    def test_snapshot_derives_lifecycle_evidence_freshness_and_fixed_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            request = parse_registry_query_request({
                "requirements": [{
                    "key": "player.injured",
                    "op": "eq",
                    "value": False,
                    "minimum_evidence": "run-verified",
                }],
                "preferences": [],
            })

            before = evaluate_registry_query_from_store(connection, request)
            self.assertEqual(len(before.candidates), 1)
            self.assertEqual(before.candidates[0].lifecycle_state, "active")
            self.assertTrue(before.candidates[0].token_eligible)
            self.assertEqual(before.evaluation.ranked_scenario_ids, ())
            self.assertEqual(before.evaluation.candidates[0].hard_results[0].reason, "below_minimum_evidence")

            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            ingested = ingest_report_reference(connection, report_path, adapters=self.adapters(state))
            current = build_registry_query_candidate_snapshot(connection)[0]
            self.assertEqual(current.lifecycle_state, "active")
            self.assertEqual(current.facts["player.injured"]["evidence_state"], "declared")
            self.assertEqual(current.explanation["route_evidence"][0]["evidence_state"], "run-verified")
            self.assertEqual(current.explanation["route_evidence"][0]["bindings"][0]["resolution"], "compatible")

            contradiction = self.ingest_named_report(
                connection,
                root,
                "contradiction.probe.report.json",
                self.report_with_proof(manifest_path, status="red", feature_proof=False),
                state,
            )
            self.assertEqual(contradiction["eligibility"], "contradicted")
            self.assertEqual(build_registry_query_candidate_snapshot(connection), ())
            quarantined = evaluate_registry_query_from_store(
                connection,
                request,
                include_lifecycle_states=("quarantined",),
            )
            self.assertEqual(quarantined.candidates[0].lifecycle_state, "quarantined")
            self.assertFalse(quarantined.candidates[0].token_eligible)
            self.assertEqual(quarantined.candidates[0].facts["player.injured"]["evidence_state"], "contradicted")
            self.assertEqual(quarantined.evaluation.ranked_scenario_ids, ())

            state["runtime"] = "stale"
            self.assertEqual(reconcile_report_bindings(connection, adapters=self.adapters(state)), {"reconciled": 2, "stale": 2})
            stale = build_registry_query_candidate_snapshot(
                connection,
                include_lifecycle_states=("quarantined",),
            )[0]
            self.assertEqual(stale.facts["player.injured"]["evidence_state"], "stale")
            self.assertEqual(stale.explanation["route_evidence"][0]["evidence_state"], "stale")
            self.assertTrue(all(
                binding["resolution"] == "stale"
                for binding in stale.explanation["route_evidence"][0]["bindings"]
            ))
            self.assertGreater(
                connection.execute("SELECT COUNT(*) FROM verification_resolution_history").fetchone()[0],
                2,
            )
            self.assertGreater(
                connection.execute(
                    "SELECT COUNT(*) FROM capability_evidence_history WHERE evidence_state = 'contradicted'"
                ).fetchone()[0],
                0,
            )
            manifest_id = stale.explanation["manifest"]["manifest_id"]
            connection.execute(
                "INSERT INTO retirement_history( manifest_id, retirement_kind, authority, reason, details_json ) "
                "VALUES( ?, 'approved', 'reviewer', 'replacement', '{}' )",
                (manifest_id,),
            )
            self.assertEqual(build_registry_query_candidate_snapshot(connection), ())
            retired = build_registry_query_candidate_snapshot(
                connection,
                include_lifecycle_states=("retired",),
            )[0]
            self.assertEqual(retired.lifecycle_state, "retired")
            self.assertFalse(retired.token_eligible)
            connection.close()


if __name__ == "__main__":
    unittest.main()
