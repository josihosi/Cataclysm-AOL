#!/usr/bin/env python3
"""Focused report-reference ingestion and binding reconciliation contracts."""

from __future__ import annotations

import hashlib
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    RegistryQueryCandidateResult,
    RegistryLaunchToken,
    RegistryQueryCandidateSnapshot,
    RegistryQueryEvaluation,
    RegistryQueryPredicateResult,
    RegistryStoredQueryEvaluation,
    _write_inert_draft,
    build_registry_query_candidate_snapshot,
    claim_repair_token_for_launch,
    append_certification_lifecycle_event,
    certification_round_facts,
    create_certification_round,
    invalidate_certification_round,
    execute_registry_query,
    evaluate_registry_query_from_store,
    ingest_report_reference,
    ingest_repair_compatibility_terminal,
    issue_wec_authority,
    ingest_repair_token_linked_report_reference,
    issue_registry_repair_token,
    _issue_registry_certification_authority,
    register_certification_round,
    final_gate_eligibility,
    open_registry,
    parse_registry_query_request,
    prepare_windows_feel_handoff,
    rebuild_manifest_projection,
    query_diagnostic_capsule_candidates,
    reload_repair_token_for_launch,
    reload_selection_token_for_launch,
    reconcile_report_bindings,
    terminalize_repair_token_cleanup_without_report,
    record_windows_feel_judgment,
    registry_status,
    windows_feel_handoff_status,
)
import scenario_registry_store  # noqa: E402
import startup_harness  # noqa: E402
from startup_harness import seal_wec_authority  # noqa: E402
from identity_binding import canonical_digest, component_identity  # noqa: E402


OPAQUE_PROSE = "opaque report prose must never be copied into sqlite"


class ScenarioRegistryIngestionTest(unittest.TestCase):
    def test_empty_scenario_setup_is_not_a_manufactured_state_claim(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            report = self.report(manifest_path)
            report["scenario_setup"] = {}
            self.write_json(report_path, report)
            result = ingest_report_reference(
                connection, report_path,
                adapters=self.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}),
            )
            self.assertEqual(result["status"], "ingested")
            self.assertNotEqual(
                connection.execute(
                    "SELECT report_kind FROM report_ingestion_history WHERE report_id = ?",
                    (result["report_id"],),
                ).fetchone()[0],
                "setup-only",
            )
            connection.close()

    def test_diagnostic_replay_ingestion_is_non_authoritative_even_when_spoofed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            report = self.report(manifest_path)
            report.update({
                "mode": "diagnostic_replay",
                "run_id": "replay-run",
                "diagnostic_replay": True,
                "evidence_class": "automated continuous-round certification",
                "wec_authority": {
                    "authority_id": "caller-forged", "evidence_class": "automated continuous-round certification",
                    "authority": "certification", "run_id": "replay-run", "binding_id": "forged",
                },
                "diagnostic_capsule_candidate": {"source_kind": "diagnostic_replay"},
            })
            self.write_json(report_path, report)
            result = ingest_report_reference(
                connection, report_path,
                adapters=self.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}),
            )
            self.assertEqual(result["status"], "ingested")
            self.assertTrue(result["non_authoritative"])
            self.assertIsNone(result["verification_id"])
            self.assertFalse(result["final_gates"]["automated_certification"])
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM verification_history").fetchone()[0], 0)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM verification_resolution_history").fetchone()[0], 0)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM diagnostic_capsule_candidate").fetchone()[0], 0)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM certification_round").fetchone()[0], 0)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM wec_authority_history").fetchone()[0], 0)
            self.assertFalse(final_gate_eligibility(connection)["automated_certification"])
            duplicate = ingest_report_reference(
                connection, report_path,
                adapters=self.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}),
            )
            self.assertTrue(duplicate["idempotent"])
            with self.assertRaisesRegex(Exception, "diagnostic replay"):
                issue_wec_authority(
                    connection, evidence_class="diagnostic replay", authority="replay",
                    run_id="replay-run-2", binding_id="binding", source_sha256="a" * 64,
                )
            connection.close()

    def test_ingestion_appends_bound_capsule_once_and_excludes_own_run(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            artifact_path = root / "saved.receipt.json"
            artifact_path.write_text('{"site":"camp","player":"p1"}\n', encoding="utf-8")
            artifact_sha = hashlib.sha256(artifact_path.read_bytes()).hexdigest()
            report = self.report(manifest_path)
            report.update({"run_id": "capsule-run", "cleanup": {"accepted": True}})
            report["step_ledger"] = [{"primitive_step": "gate.scout", "verdict": "green_scout"}]
            report["capsule_binding"] = {
                "state": {"identity": "save-1"}, "player": {"identity": "p1"},
                "actors": [{"identity": "a1"}], "owner": "overmap",
            }
            report["diagnostic_capsule_candidate"] = {
                "saved_artifact": {"path": str(artifact_path), "sha256": artifact_sha},
                "site_id": "camp", "operation": "scout", "generation": "7",
                "actor_ids": ["a1"], "owner": "overmap", "gate_id": "gate.scout",
                "gate_index": 2, "gate_verdict": "green", "durable_timestamp": "2026-08-22T10:00:00Z",
                "cleanup": {"accepted": True},
            }
            self.write_json(report_path, report)
            adapters = self.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"})
            first = ingest_report_reference(connection, report_path, adapters=adapters)
            self.assertEqual(first["status"], "ingested")
            self.assertFalse(first["diagnostic_capsule"]["idempotent"])
            binding_id = connection.execute("SELECT binding_id FROM diagnostic_capsule_candidate").fetchone()[0]
            self.assertEqual(len(query_diagnostic_capsule_candidates(connection, binding_id=binding_id, run_id="other")), 1)
            self.assertEqual(query_diagnostic_capsule_candidates(connection, binding_id=binding_id, run_id="capsule-run"), ())
            duplicate = ingest_report_reference(connection, report_path, adapters=adapters)
            self.assertTrue(duplicate["idempotent"])
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM diagnostic_capsule_candidate").fetchone()[0], 1)
            connection.close()

    def test_explicit_relaunch_receipts_are_normalized_before_ingestion(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            receipt = {
                "world_id": "world-1", "run_id": "lineage-1",
                "identity": {
                    "world_id": "world-1", "run_id": "lineage-1", "activity_id": "activity-7",
                    "generation": 7, "actor_ids": ["actor-a", "actor-b"],
                    "simulation_owner": "local", "handoff_epoch": 3,
                },
                "crossing_receipt": {
                    "actor_ids": ["actor-a", "actor-b"], "run_id": "lineage-1",
                    "activity_id": "activity-7", "generation": 7,
                    "prior_owner": "abstract", "next_owner": "local", "handoff_epoch": 3,
                    "cursor_minutes": 120, "cursor_waypoint": 2, "outcome": "committed",
                    "persistence_acknowledged": True,
                },
            }
            report = self.report(manifest_path)
            report["relaunch_receipts"] = {
                "before_save": receipt, "after_load": json.loads(json.dumps(receipt)),
                "transition": receipt["crossing_receipt"], "expected_world_id": "world-1",
                "expected_run_id": "lineage-1",
            }
            self.write_json(report_path, report)
            result = ingest_report_reference(connection, report_path, adapters=self.adapters({
                "runtime": "compatible", "fixture": "compatible", "profile": "compatible",
            }))
            self.assertEqual(result["status"], "ingested")
            malformed = json.loads(json.dumps(report))
            malformed["relaunch_receipts"]["after_load"]["identity"]["actor_ids"] = ["actor-a", "actor-a"]
            bad_path = root / "malformed.probe.report.json"
            self.write_json(bad_path, malformed)
            rejected = ingest_report_reference(connection, bad_path, adapters=self.adapters({
                "runtime": "compatible", "fixture": "compatible", "profile": "compatible",
            }))
            self.assertEqual(rejected["status"], "invalid_report")
            self.assertIn("duplicate identities", rejected["error"])
            connection.close()

    def test_inert_draft_uses_one_closest_candidate_and_reports_scenario_work(self) -> None:
        matched = RegistryQueryPredicateResult(
            "fixture.kind", "eq", "camp", "camp", "declared", True, "matched",
        )
        missing = RegistryQueryPredicateResult(
            "route.observer", "eq", "natural", None, "unknown", False, "unknown_fact",
        )
        unknown_fixture = RegistryQueryPredicateResult(
            "fixture.kind", "eq", "camp", None, "unknown", False, "unknown_fact",
        )
        useful_snapshot = RegistryQueryCandidateSnapshot(
            "a-useful", {}, "quarantined", False,
            {"manifest": {"known_footing": {"fixture": "camp-a"}}},
        )
        unrelated_snapshot = RegistryQueryCandidateSnapshot(
            "z-unrelated", {}, "quarantined", False,
            {"manifest": {"known_footing": {"fixture": "unrelated-z"}}},
        )
        stored = RegistryStoredQueryEvaluation(
            (useful_snapshot, unrelated_snapshot),
            RegistryQueryEvaluation((
                RegistryQueryCandidateResult("a-useful", (matched, missing), ()),
                RegistryQueryCandidateResult("z-unrelated", (unknown_fixture, missing), ()),
            ), ()),
        )
        request_json = json.dumps({
            "requirements": [
                {"key": "fixture.kind", "op": "eq", "value": "camp", "minimum_evidence": "declared"},
                {"key": "route.observer", "op": "eq", "value": "natural", "minimum_evidence": "run-verified"},
            ],
            "preferences": [],
        })
        with tempfile.TemporaryDirectory() as temp_dir:
            path = _write_inert_draft(
                request_json=request_json,
                query_sha256="closest",
                evaluation=stored,
                drafts_root=Path(temp_dir),
            )
            artifact = json.loads(Path(path).read_text(encoding="utf-8"))

        self.assertEqual(artifact["build_action"], "repair_closest_scenario")
        self.assertEqual(artifact["closest_candidate"]["scenario_id"], "a-useful")
        self.assertEqual(artifact["candidate_manifest"], {"fixture": "camp-a"})
        self.assertEqual([item["key"] for item in artifact["satisfied_requirements"]], ["fixture.kind"])
        self.assertEqual([item["key"] for item in artifact["missing_requirements"]], ["route.observer"])
        self.assertEqual(len(artifact["unmet_capabilities"]), 1)

    def test_inert_draft_requests_a_new_scenario_when_every_fact_is_unknown(self) -> None:
        unknown = RegistryQueryPredicateResult(
            "route.observer", "eq", "natural", None, "unknown", False, "unknown_fact",
        )
        snapshot = RegistryQueryCandidateSnapshot(
            "unrelated", {}, "quarantined", False,
            {"manifest": {"known_footing": {"fixture": "must-not-leak"}}},
        )
        stored = RegistryStoredQueryEvaluation(
            (snapshot,),
            RegistryQueryEvaluation((RegistryQueryCandidateResult("unrelated", (unknown,), ()),), ()),
        )
        request_json = json.dumps({
            "requirements": [{
                "key": "route.observer", "op": "eq", "value": "natural",
                "minimum_evidence": "run-verified",
            }],
            "preferences": [],
        })
        with tempfile.TemporaryDirectory() as temp_dir:
            path = _write_inert_draft(
                request_json=request_json,
                query_sha256="new",
                evaluation=stored,
                drafts_root=Path(temp_dir),
            )
            artifact = json.loads(Path(path).read_text(encoding="utf-8"))

        self.assertEqual(artifact["build_action"], "create_scenario")
        self.assertIsNone(artifact["closest_candidate"])
        self.assertEqual(artifact["candidate_manifest"], {})
        self.assertEqual(artifact["unmet_capabilities"], [])
        self.assertEqual(artifact["missing_requirements"][0]["reason"], "unknown_fact")

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
            # Certification-success fixtures must carry the complete natural
            # lifecycle; ordinary probe assertions remain lower-classed.
            "certification_lifecycle": {
                "world_id": "world-1", "player_id": "player-1", "actor_ids": ["actor-1"],
                "events": [
                    {"kind": kind, "round_id": "round-1", "binding_id": "binding-1",
                     "world_id": "world-1", "player_id": "player-1", "actor_ids": ["actor-1"],
                     "owner": "abstract"}
                    for kind in ("declared_world", "departure", "overmap_advance", "bubble_crossing_out",
                                 "actor_outcomes", "save", "quit", "relaunch", "bubble_crossing_in",
                                 "return_report", "camp_decision")
                ],
            },
            "opaque_report_payload": {"prose": OPAQUE_PROSE, "nested": [OPAQUE_PROSE]},
        }

    def gated_manifest(self) -> dict:
        manifest = self.strict_manifest()
        manifest["capabilities"] = {
            "player.startup_gate": True,
            "player.interaction_gate": True,
            "player.terminal_gate": True,
            "player.persistence_gate": True,
            "player.replay_gate": True,
        }
        manifest["proof_route"]["capability_gates"] = {
            "player.startup_gate": {"startup": ["setup"]},
            "player.interaction_gate": {"interaction": ["production"]},
            "player.terminal_gate": {"terminal": ["terminal"]},
            "player.persistence_gate": {"persistence": ["artifact"]},
            "player.replay_gate": {"replay": ["artifact"]},
        }
        return manifest

    def report_with_named_gates(
        self,
        manifest_path: Path,
        *,
        setup: str = "green_setup",
        production: str = "green_production",
        terminal: str = "green_terminal",
        artifact: str = "green_artifact",
        status: str = "green",
        feature_proof: bool = True,
        supersedes_verification_id: str = "",
    ) -> dict:
        report = self.report_with_proof(
            manifest_path,
            status=status,
            feature_proof=feature_proof,
            supersedes_verification_id=supersedes_verification_id,
        )
        report["startup"]["startup_step_ledger"] = [
            {"step": "setup", "verdict": setup},
        ]
        report["step_ledger"] = [
            {"primitive_step": "production", "verdict": production},
            {"primitive_step": "terminal", "verdict": terminal},
            {"primitive_step": "artifact", "verdict": artifact},
            {"primitive_step": "shortcut", "verdict": "green_debug_setup"},
        ]
        return report

    def setup_gated_registry(self, root: Path) -> tuple:
        scenarios = root / "scenarios"
        scenarios.mkdir()
        manifest_path = scenarios / "gated.json"
        self.write_json(manifest_path, self.gated_manifest())
        connection = open_registry(str(root / "registry.sqlite3"))
        rebuild_manifest_projection(connection, scenarios)
        return connection, scenarios, manifest_path

    def phase4_terminal_gate_manifest(self) -> dict:
        manifest = self.strict_manifest()
        manifest["capabilities"] = {
            "capabilities.phase4.scenario": "structural_signal_matrix",
            "capabilities.phase4.control": "real_signal_returns_approximate_lead_without_player_truth",
            "capabilities.phase4.unmapped": "must_remain_declared",
        }
        manifest["proof_route"]["capability_gates"] = {
            "capabilities.phase4.scenario": {"terminal": ["terminal"]},
            "capabilities.phase4.control": {"terminal": ["terminal"]},
        }
        return manifest

    def setup_phase4_terminal_gate_registry(self, root: Path) -> tuple:
        scenarios = root / "scenarios"
        scenarios.mkdir()
        manifest_path = scenarios / "phase4.json"
        self.write_json(manifest_path, self.phase4_terminal_gate_manifest())
        connection = open_registry(str(root / "registry.sqlite3"))
        rebuild_manifest_projection(connection, scenarios)
        return connection, scenarios, manifest_path

    def phase4_terminal_gate_request(self):
        return parse_registry_query_request({
            "requirements": [
                {
                    "key": "capabilities.phase4.scenario",
                    "op": "eq",
                    "value": "structural_signal_matrix",
                    "minimum_evidence": "run-verified",
                },
                {
                    "key": "capabilities.phase4.control",
                    "op": "eq",
                    "value": "real_signal_returns_approximate_lead_without_player_truth",
                    "minimum_evidence": "run-verified",
                },
            ],
            "preferences": [],
        })

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

    def register_valid_round(
        self, connection: sqlite3.Connection, *, source_sha256: str, round_id: str = "registry-owned",
    ) -> tuple:
        names = ("worktree", "executable", "data_config", "harness", "scenario", "fixture", "profile", "world_save", "player", "actors")
        authoritative = {name: {"identity": name} for name in names}
        authoritative["scenario"] = {"identity": "scenario", "content_sha256": source_sha256}
        authoritative["executable"] = {"identity": "executable", "content_sha256": "b" * 64}
        components = {name: component_identity(name, authoritative[name]) for name in names}
        binding = {"schema": 1, "components": components, "authoritative_components": authoritative}
        binding["sha256"] = canonical_digest(
            {key: value["sha256"] for key, value in components.items()},
            domain="caol-complete-binding:v1",
        )
        authority = _issue_registry_certification_authority(
            connection, round_id=round_id, binding_id=binding["sha256"],
            source_sha256=source_sha256, launch_token="ingestion-test-token-" + round_id,
        )
        manifest = {
            "schema": 1, "version": 1, "round_id": authority["run_id"],
            "scenario_lineage_id": "lineage", "authority_id": authority["authority_id"],
            "authority_kind": "automated-certification", "event_stream_id": "stream",
            "event_stream_schema": 1, "binding_id": binding["sha256"], "binding": binding,
        }
        manifest["manifest_sha256"] = canonical_digest(manifest, domain="caol-round-manifest:v1")
        register_certification_round(connection, manifest)
        append_certification_lifecycle_event(connection, round_id=manifest["round_id"], event_sequence=1, event_kind="complete")
        return manifest, authority

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

            status = registry_status(connection)
            self.assertEqual(len(status), 1)
            handoff = status[0]["history"]["report_ingestions"]
            self.assertEqual(handoff, ({
                "report_id": first["report_id"],
                "report_path": str(report_path.resolve()),
                "report_sha256": expected_sha256,
                "report_kind": "probe",
                "ingestion_status": "ingested",
                "error": "",
                "recorded_at": handoff[0]["recorded_at"],
            },))
            self.assertEqual(
                status[0]["history"]["verifications"][0]["report_id"],
                first["report_id"],
            )

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

    def test_reconciliation_reuses_ingested_bindings_without_decoding_opaque_report_body(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
            report = self.report(manifest_path)
            opaque_payload = "x" * (2 * 1024 * 1024)
            report["opaque_report_payload"]["large_unbound_payload"] = opaque_payload
            self.write_json(report_path, report)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            self.assertEqual(ingest_report_reference(connection, report_path, adapters=self.adapters(state))["status"], "ingested")

            original_loads = json.loads
            with mock.patch.object(scenario_registry_store.json, "loads", wraps=original_loads) as loads:
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=self.adapters(state)),
                    {"reconciled": 1, "stale": 0},
                )

            self.assertTrue(loads.call_args_list)
            self.assertTrue(all(
                not isinstance(call.args[0], str) or len(call.args[0]) < len(opaque_payload)
                for call in loads.call_args_list
            ))
            connection.close()

    def test_reconciliation_observes_shared_binding_once_for_many_reports(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            duplicate_path = root / "duplicate.probe.report.json"
            duplicate_path.write_bytes(report_path.read_bytes())
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            adapters = self.adapters(state)
            self.assertEqual(ingest_report_reference(connection, report_path, adapters=adapters)["status"], "ingested")
            self.assertEqual(ingest_report_reference(connection, duplicate_path, adapters=adapters)["status"], "ingested")
            calls = {"runtime": 0, "fixture": 0, "profile": 0}

            def counted(kind: str):
                def observe(expected):
                    calls[kind] += 1
                    return getattr(adapters, kind)(expected)
                return observe

            self.assertEqual(
                reconcile_report_bindings(connection, adapters=BindingAdapters(
                    runtime=counted("runtime"), fixture=counted("fixture"), profile=counted("profile"),
                )),
                {"reconciled": 2, "stale": 0},
            )
            self.assertEqual(calls, {"runtime": 1, "fixture": 1, "profile": 1})
            connection.close()

    def test_reconciliation_ignores_rejected_legacy_binding_ownership(self) -> None:
        """A v11-rejected legacy row cannot re-enter route reduction by decoding."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            self.assertEqual(
                ingest_report_reference(connection, report_path, adapters=self.adapters(state))["status"],
                "ingested",
            )
            manifest_id = connection.execute(
                "SELECT manifest_id FROM report_ingestion_history WHERE report_path = ?",
                (str(report_path.resolve()),),
            ).fetchone()[0]
            connection.execute(
                "INSERT INTO binding_history( manifest_id, report_id, binding_kind, binding_fingerprint, "
                "binding_status, payload_json ) VALUES( ?, NULL, 'manifest', 'rejected-legacy', 'compatible', "
                "'not-json' )",
                (manifest_id,),
            )
            rejected_event_id = connection.execute("SELECT last_insert_rowid()").fetchone()[0]
            connection.execute(
                "INSERT INTO binding_identity_migration( binding_event_id, migration_status, reason ) "
                "VALUES( ?, 'rejected', 'ambiguous duplicate ownership' )",
                (rejected_event_id,),
            )

            self.assertEqual(
                reconcile_report_bindings(connection, adapters=self.adapters(state)),
                {"reconciled": 1, "stale": 0},
            )
            migration = connection.execute(
                "SELECT migration_status, report_id FROM binding_identity_migration "
                "WHERE binding_event_id = ?", (rejected_event_id,),
            ).fetchone()
            self.assertEqual((migration["migration_status"], migration["report_id"]), ("rejected", ""))
            self.assertIsNone(connection.execute(
                "SELECT report_id FROM binding_history WHERE binding_event_id = ?", (rejected_event_id,),
            ).fetchone()[0])
            connection.close()

    def test_canonical_ingestion_preserves_commitment_only_proof_without_final_credit(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
            report = self.report(manifest_path)
            report.update({"run_id": "commitment-only", "binding_id": "runtime-observed-hash"})
            report["wec_authority"] = seal_wec_authority(
                evidence_class="automated continuous-round certification",
                authority="certification",
                run_id=report["run_id"],
                binding_id=report["binding_id"],
                source_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            )
            self.write_json(report_path, report)

            result = ingest_report_reference(
                connection, report_path,
                adapters=self.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}),
            )

            self.assertEqual(result["status"], "ingested")
            self.assertEqual(result["resolution"], "compatible")
            self.assertEqual(result["eligibility"], "hard_proven")
            self.assertFalse(result["final_gates"]["automated_certification"])
            self.assertFalse(final_gate_eligibility(connection)["automated_certification"])
            connection.close()

    def test_canonical_ingestion_grants_final_credit_to_matching_registry_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
            source_sha256 = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
            report = self.report(manifest_path)
            round_manifest, authority = self.register_valid_round(
                connection, source_sha256=source_sha256,
            )
            report["startup"]["screen"]["runtime_binding_observed"]["executable_sha256"] = "b" * 64
            report.update({
                "run_id": authority["run_id"],
                "binding_id": authority["binding_id"],
                "wec_authority": authority,
                "event_stream_id": round_manifest["event_stream_id"],
                "certification_round": {
                    "round_id": round_manifest["round_id"],
                    "authority_id": round_manifest["authority_id"],
                    "binding_id": round_manifest["binding_id"],
                    "event_stream_id": round_manifest["event_stream_id"],
                    "manifest_sha256": round_manifest["manifest_sha256"],
                    "lifecycle_state": "complete",
                    "registry_derived": "true",
                },
            })
            lifecycle = report["certification_lifecycle"]
            lifecycle["events"] = [dict(event, round_id=round_manifest["round_id"], binding_id=round_manifest["binding_id"])
                                    for event in lifecycle["events"]]
            self.write_json(report_path, report)
            adapters = BindingAdapters(
                runtime=lambda _expected: {
                    "status": "compatible",
                    "facts": {
                        "executable_sha256": "b" * 64,
                        "source_sha256": "1" * 64,
                    },
                },
                fixture=lambda _expected: {
                    "status": "compatible", "facts": {"source_sha256": "2" * 64},
                },
                profile=lambda _expected: {
                    "status": "compatible", "facts": {"source_sha256": "3" * 64},
                },
            )

            result = ingest_report_reference(connection, report_path, adapters=adapters)

            self.assertEqual(result["status"], "ingested")
            self.assertTrue(result["final_gates"]["automated_certification"])
            self.assertFalse(result["final_gates"]["windows_feel"])
            self.assertTrue(final_gate_eligibility(connection)["automated_certification"])
            verification_id = result["verification_id"]
            history_before = connection.execute(
                "SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (verification_id,)
            ).fetchone()[0]
            binding_before = connection.execute(
                "SELECT COUNT(*) FROM binding_history WHERE manifest_id = (SELECT manifest_id FROM verification_history WHERE verification_id = ?)", (verification_id,)
            ).fetchone()[0]
            invalidate_certification_round(
                connection, round_id=round_manifest["round_id"], reason="component_drift", component_name="world_save",
            )
            self.assertFalse(final_gate_eligibility(connection)["automated_certification"])
            self.assertEqual(connection.execute(
                "SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (verification_id,)
            ).fetchone()[0], history_before)
            self.assertEqual(connection.execute(
                "SELECT COUNT(*) FROM binding_history WHERE manifest_id = (SELECT manifest_id FROM verification_history WHERE verification_id = ?)", (verification_id,)
            ).fetchone()[0], binding_before)
            self.assertEqual(connection.execute(
                "SELECT resolution_kind FROM verification_resolution_history WHERE verification_id = ? ORDER BY resolution_event_id DESC LIMIT 1",
                (verification_id,),
            ).fetchone()[0], "compatible")
            connection.close()

    def test_registry_owner_created_round_keeps_history_when_only_final_credit_is_invalidated(self) -> None:
        """Final credit uses the owner-issued round, never caller-assigned digests."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path, report_path = self.setup_registry(root)
            repo = root / "repo"
            repo.mkdir()
            (repo / "tracked").write_bytes(b"tracked")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "tracked"], check=True)
            subprocess.run([
                "git", "-C", str(repo), "-c", "user.name=test",
                "-c", "user.email=test@example.invalid", "commit", "-qm", "base",
            ], check=True)
            executable = root / "game"
            executable.write_bytes(b"owner-created-executable")
            data = root / "data"
            data.mkdir()
            (data / "config").write_bytes(b"config")
            harness_root = root / "harness"
            harness_root.mkdir()
            (harness_root / "runner").write_bytes(b"runner")
            fixture = root / "fixture"
            fixture.write_bytes(b"fixture")
            profile = root / "profile"
            profile.write_bytes(b"profile")
            world = root / "world"
            world.mkdir()
            (world / "player.sav").write_bytes(b"save")
            connection.execute(
                "INSERT INTO token_history(token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json) "
                "VALUES(?,?,?,?,?,?,?)",
                (
                    "canonical-token",
                    connection.execute("SELECT manifest_id FROM manifest_current LIMIT 1").fetchone()[0],
                    None, "canonical-route", "issued", "test", "{}",
                ),
            )
            with mock.patch(
                    "scenario_registry_store.reload_selection_token_for_launch",
                    return_value=RegistryLaunchToken(
                        "canonical-token", True, "current", "strict", str(manifest_path),
                    ),
            ):
                created = create_certification_round(
                    connection,
                    scenario_lineage_id="lineage-owner",
                    event_stream_id="owner-event-stream",
                    launch_token="canonical-token",
                    launch_source_path=manifest_path,
                    launch_route_key="canonical-route",
                    current_executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    producer_inputs={
                        "repo_root": repo, "executable": executable, "runtime_paths": ["tracked"],
                        "data_config_roots": [data], "harness_roots": [harness_root],
                        "scenario_path": manifest_path, "fixture_path": fixture, "profile_path": profile,
                        "world_dir": world, "player_save": "player.sav",
                        "saved_player_payload": {"player": {"id": "player-1"}},
                        "ecology_audit": {"actors": [{"actor_id": "actor-1"}]},
                    },
                )
            manifest = created["manifest"]
            append_certification_lifecycle_event(
                connection, round_id=manifest["round_id"], event_sequence=2, event_kind="complete",
            )
            report = self.report(manifest_path)
            executable_sha256 = hashlib.sha256(executable.read_bytes()).hexdigest()
            report["startup"]["screen"]["runtime_binding_observed"]["executable_sha256"] = executable_sha256
            report.update({
                "run_id": manifest["round_id"],
                "binding_id": manifest["binding_id"],
                "wec_authority": created["authority"],
                "certification_round": certification_round_facts(connection, manifest["round_id"]),
            })
            lifecycle = report["certification_lifecycle"]
            lifecycle["events"] = [dict(event, round_id=manifest["round_id"], binding_id=manifest["binding_id"])
                                    for event in lifecycle["events"]]
            self.write_json(report_path, report)
            adapters = BindingAdapters(
                runtime=lambda _expected: {"status": "compatible", "facts": {
                    "executable_sha256": executable_sha256, "source_sha256": "1" * 64,
                }},
                fixture=lambda _expected: {"status": "compatible", "facts": {"source_sha256": "2" * 64}},
                profile=lambda _expected: {"status": "compatible", "facts": {"source_sha256": "3" * 64}},
            )
            result = ingest_report_reference(connection, report_path, adapters=adapters)
            self.assertTrue(result["final_gates"]["automated_certification"])
            verification_id = result["verification_id"]
            history_before = connection.execute(
                "SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (verification_id,)
            ).fetchone()[0]
            invalidate_certification_round(
                connection, round_id=manifest["round_id"], reason="binding_drift", component_name="world_save",
            )
            self.assertFalse(final_gate_eligibility(connection)["automated_certification"])
            self.assertEqual(connection.execute(
                "SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (verification_id,)
            ).fetchone()[0], history_before)
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

    def test_named_capability_gates_are_exact_depths_and_hud_debug_fields_grant_nothing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path = self.setup_gated_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            hud_only = self.report_with_proof(manifest_path, status="green", feature_proof=True)
            hud_only["startup"]["screen"]["gameplay_hud_present"] = True
            hud_only["feature_debug_guard"] = {"status": "green", "verdict": "debug_setup_only"}
            self.ingest_named_report(connection, root, "hud-only.probe.report.json", hud_only, state)
            facts = build_registry_query_candidate_snapshot(connection)[0].facts
            self.assertEqual(
                {key: (fact["evidence_state"], fact["proof_depth"]) for key, fact in facts.items()},
                {
                    "player.interaction_gate": ("declared", None),
                    "player.persistence_gate": ("declared", None),
                    "player.replay_gate": ("declared", None),
                    "player.startup_gate": ("declared", None),
                    "player.terminal_gate": ("declared", None),
                },
            )

            startup_only = self.report_with_proof(manifest_path, status="green", feature_proof=True)
            startup_only["startup"]["startup_step_ledger"] = [
                {"step": "setup", "verdict": "green_startup"},
                {"step": "production", "verdict": "green_spoofed_startup"},
            ]
            self.ingest_named_report(connection, root, "startup-only.probe.report.json", startup_only, state)
            facts = build_registry_query_candidate_snapshot(connection)[0].facts
            self.assertEqual(facts["player.startup_gate"]["proof_depth"], "startup")
            self.assertEqual(facts["player.interaction_gate"], {
                "present": True, "value": True, "evidence_state": "declared", "proof_depth": None,
            })

            green = self.ingest_named_report(
                connection,
                root,
                "named-gates.probe.report.json",
                self.report_with_named_gates(manifest_path),
                state,
            )
            self.assertEqual(green["eligibility"], "hard_proven")
            facts = build_registry_query_candidate_snapshot(connection)[0].facts
            self.assertEqual(facts["player.startup_gate"], {
                "present": True, "value": True, "evidence_state": "inspected", "proof_depth": "startup",
            })
            for key, depth in (
                ("player.interaction_gate", "interaction"),
                ("player.terminal_gate", "terminal"),
                ("player.persistence_gate", "persistence"),
                ("player.replay_gate", "replay"),
            ):
                self.assertEqual(facts[key]["evidence_state"], "run-verified")
                self.assertEqual(facts[key]["proof_depth"], depth)
            query = evaluate_registry_query_from_store(
                connection,
                parse_registry_query_request({
                    "requirements": [{
                        "key": "player.startup_gate", "op": "eq", "value": True,
                        "minimum_evidence": "run-verified",
                    }],
                    "preferences": [],
                }),
            )
            self.assertEqual(query.evaluation.candidates[0].hard_results[0].reason, "below_minimum_evidence")
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM capability_evidence_history "
                    "WHERE capability_key = 'player.startup_gate' AND evidence_kind = 'named_proof_gate'"
                ).fetchone()[0],
                2,
            )
            connection.close()

    def test_phase4_terminal_capability_gates_require_a_current_green_terminal_audit(self) -> None:
        def terminal_report(manifest_path: Path, verdict: str | None, *, status: str = "green") -> dict:
            report = self.report_with_proof(
                manifest_path,
                status=status,
                feature_proof=status == "green",
            )
            if verdict is not None:
                report["step_ledger"] = [{"primitive_step": "terminal", "verdict": verdict}]
            return report

        for case in ("missing", "green", "red", "incompatible", "stale"):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                connection, scenarios, manifest_path = self.setup_phase4_terminal_gate_registry(root)
                state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
                try:
                    if case == "missing":
                        report = terminal_report(manifest_path, None)
                    elif case == "red":
                        report = terminal_report(manifest_path, "red_terminal_audit_failed", status="red")
                    else:
                        report = terminal_report(manifest_path, "green_terminal_audit_present")
                    if case == "incompatible":
                        state["runtime"] = "stale"

                    ingested = self.ingest_named_report(connection, root, f"{case}.probe.report.json", report, state)
                    if case == "stale":
                        changed = self.phase4_terminal_gate_manifest()
                        changed["description"] = "current source changed after terminal audit"
                        self.write_json(manifest_path, changed)
                        rebuild_manifest_projection(connection, scenarios)
                        self.assertEqual(
                            reconcile_report_bindings(connection, adapters=self.adapters(state)),
                            {"reconciled": 1, "stale": 1},
                        )

                    if case == "green":
                        self.assertEqual(ingested["eligibility"], "hard_proven")
                        facts = build_registry_query_candidate_snapshot(connection)[0].facts
                        for key in ("capabilities.phase4.scenario", "capabilities.phase4.control"):
                            self.assertEqual(facts[key]["evidence_state"], "run-verified")
                            self.assertEqual(facts[key]["proof_depth"], "terminal")
                        self.assertEqual(facts["capabilities.phase4.unmapped"], {
                            "present": True,
                            "value": "must_remain_declared",
                            "evidence_state": "declared",
                            "proof_depth": None,
                        })
                    elif case == "missing":
                        facts = build_registry_query_candidate_snapshot(connection)[0].facts
                        self.assertTrue(all(
                            facts[key]["evidence_state"] == "declared"
                            for key in ("capabilities.phase4.scenario", "capabilities.phase4.control")
                        ))
                    else:
                        snapshots = build_registry_query_candidate_snapshot(
                            connection,
                            include_lifecycle_states=("quarantined",),
                        )
                        self.assertEqual(len(snapshots), 1)
                        expected_state = "contradicted" if case == "red" else "stale"
                        self.assertTrue(all(
                            snapshots[0].facts[key]["evidence_state"] == expected_state
                            for key in ("capabilities.phase4.scenario", "capabilities.phase4.control")
                        ))

                    selected = execute_registry_query(
                        connection,
                        self.phase4_terminal_gate_request(),
                        drafts_root=root / "drafts",
                    )
                    if case == "green":
                        self.assertIsNotNone(selected.token_id)
                        self.assertIsNone(selected.draft_path)
                    else:
                        self.assertIsNone(selected.token_id)
                        self.assertIsNotNone(selected.draft_path)
                finally:
                    connection.close()

    def test_current_hard_proven_route_selects_without_erasing_stale_history(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, scenarios, manifest_path = self.setup_phase4_terminal_gate_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            try:
                def report() -> dict:
                    result = self.report_with_proof(manifest_path, status="green", feature_proof=True)
                    result["step_ledger"] = [{
                        "primitive_step": "terminal",
                        "verdict": "green_terminal_audit_present",
                    }]
                    return result

                first = self.ingest_named_report(connection, root, "old.probe.report.json", report(), state)
                changed = self.phase4_terminal_gate_manifest()
                changed["description"] = "current source after stale report"
                self.write_json(manifest_path, changed)
                rebuild_manifest_projection(connection, scenarios)
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=self.adapters(state)),
                    {"reconciled": 1, "stale": 1},
                )
                current = self.ingest_named_report(connection, root, "current.probe.report.json", report(), state)
                self.assertEqual(current["eligibility"], "hard_proven")

                selected = execute_registry_query(
                    connection,
                    self.phase4_terminal_gate_request(),
                    drafts_root=root / "drafts",
                )
                self.assertIsNotNone(selected.token_id)
                issued = connection.execute(
                    "SELECT verification_id, details_json FROM token_history WHERE token_id = ? AND event_kind = 'issued'",
                    (selected.token_id,),
                ).fetchone()
                self.assertEqual(issued["verification_id"], current["verification_id"])
                receipt = json.loads(issued["details_json"])
                self.assertEqual(
                    [binding["verification_id"] for binding in receipt["route_evidence"]["bindings"]],
                    [current["verification_id"]],
                )
                self.assertTrue(reload_selection_token_for_launch(connection, str(selected.token_id)).accepted)

                state["runtime"] = "stale"
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=self.adapters(state)),
                    {"reconciled": 2, "stale": 2},
                )
                rejected = execute_registry_query(
                    connection,
                    self.phase4_terminal_gate_request(),
                    drafts_root=root / "drafts",
                )
                self.assertIsNone(rejected.token_id)
                self.assertFalse(reload_selection_token_for_launch(connection, str(selected.token_id)).accepted)
                self.assertEqual(first["eligibility"], "hard_proven")
            finally:
                connection.close()

    def test_repair_token_supersedes_only_its_bound_current_red_verification(self) -> None:
        def repair_binding(root: Path) -> dict:
            executable = root / "repair-runtime"
            executable.write_bytes(b"repair runtime")
            return {
                "runtime": {
                    "schema": 1,
                    "executable_path": str(executable.resolve()),
                    "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                    "runtime_source_sha256": hashlib.sha256(b"repair source").hexdigest(),
                },
                "fixture": {
                    "status": "compatible", "name": "", "profile": "",
                    "source_path": str(root.resolve()),
                    "source_sha256": hashlib.sha256(b"repair fixture").hexdigest(),
                },
                "profile": {
                    "status": "compatible", "name": "", "profile": "",
                    "source_path": str(root.resolve()),
                    "source_sha256": hashlib.sha256(b"repair profile").hexdigest(),
                },
            }

        def establish(root: Path, *, claim: bool = True) -> tuple:
            connection, _scenarios, manifest_path = self.setup_phase4_terminal_gate_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            red_report = self.report_with_proof(manifest_path, status="red", feature_proof=False)
            red_report["step_ledger"] = [{"primitive_step": "terminal", "verdict": "red_terminal"}]
            red = self.ingest_named_report(connection, root, "red.probe.report.json", red_report, state)
            route_key = connection.execute(
                "SELECT route_key FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
            ).fetchone()[0]
            token = issue_registry_repair_token(
                connection,
                self.phase4_terminal_gate_request(),
                manifest_id=connection.execute(
                    "SELECT manifest_id FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0],
                route_key=route_key,
                red_verification_id=red["verification_id"],
                binding=repair_binding(root),
            )
            self.assertTrue(token.accepted)
            if claim:
                claimed = claim_repair_token_for_launch(
                    connection, token.token_id, binding=repair_binding(root),
                )
                self.assertTrue(claimed.accepted, claimed.reason)
            return connection, manifest_path, state, red, token, repair_binding(root)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _manifest_path, _state, red, token, binding = establish(root, claim=False)
            try:
                manifest_id = connection.execute(
                    "SELECT manifest_id FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0]
                route_key = connection.execute(
                    "SELECT route_key FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0]
                repeated = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id,
                    route_key=route_key, red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertTrue(repeated.accepted, repeated.reason)
                self.assertEqual(repeated.token_id, token.token_id)
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM token_history WHERE event_kind = 'repair_issued'"
                ).fetchone()[0], 1)
                claimed = claim_repair_token_for_launch(connection, token.token_id, binding=binding)
                self.assertTrue(claimed.accepted, claimed.reason)
                blocked = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id,
                    route_key=route_key, red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertFalse(blocked.accepted)
                self.assertEqual(blocked.reason, "token_already_claimed")
                replayed = claim_repair_token_for_launch(connection, token.token_id, binding=binding)
                self.assertFalse(replayed.accepted)
                self.assertEqual(replayed.reason, "token_already_claimed")
                claimed_invalidation = json.loads(connection.execute(
                    "SELECT details_json FROM token_history WHERE token_id = ? "
                    "AND event_kind = 'repair_invalidated' ORDER BY token_event_id DESC LIMIT 1",
                    (token.token_id,),
                ).fetchone()[0])
                self.assertEqual(claimed_invalidation, {
                    "observed_claimed": True,
                    "required_claimed": False,
                })
                successor = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id,
                    route_key=route_key, red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertTrue(successor.accepted, successor.reason)
                self.assertNotEqual(successor.token_id, token.token_id)
                successor_details = json.loads(connection.execute(
                    "SELECT details_json FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued'",
                    (successor.token_id,),
                ).fetchone()[0])
                self.assertEqual(successor_details["attempt_sequence"], 2)
                self.assertEqual(successor_details["predecessor"]["token_id"], token.token_id)
                self.assertEqual(
                    successor_details["predecessor"]["terminal_event"]["reason"], "token_already_claimed",
                )
                self.assertEqual(issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id,
                    route_key=route_key, red_verification_id=red["verification_id"], binding=binding,
                ).token_id, successor.token_id)
            finally:
                connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _manifest_path, _state, red, token, binding = establish(root, claim=False)
            try:
                claimed = claim_repair_token_for_launch(connection, token.token_id, binding=binding)
                self.assertTrue(claimed.accepted, claimed.reason)
                run_dir = root / "claimed-no-report-run"
                run_dir.mkdir()
                cleanup = {"status": "terminated", "pid": 42}
                terminal = terminalize_repair_token_cleanup_without_report(
                    connection, token.token_id, run_dir=run_dir, cleanup=cleanup,
                )
                self.assertTrue(terminal.accepted, terminal.reason)
                self.assertEqual(terminal.reason, "cleanup_no_report_terminal")
                replayed = terminalize_repair_token_cleanup_without_report(
                    connection, token.token_id, run_dir=run_dir, cleanup=cleanup,
                )
                self.assertTrue(replayed.accepted, replayed.reason)
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM token_history WHERE token_id = ? AND event_kind = 'repair_no_report_terminal'",
                    (token.token_id,),
                ).fetchone()[0], 1)
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run'",
                    (token.token_id,),
                ).fetchone()[0], 0)
                rejected = reload_repair_token_for_launch(
                    connection, token.token_id, require_claimed=True, binding=binding,
                )
                self.assertFalse(rejected.accepted)
                self.assertEqual(rejected.reason, "token_invalidated")
                invalidation = json.loads(connection.execute(
                    "SELECT details_json FROM token_history WHERE token_id = ? "
                    "AND event_kind = 'repair_invalidated' ORDER BY token_event_id DESC LIMIT 1",
                    (token.token_id,),
                ).fetchone()[0])
                self.assertEqual(invalidation["prior_terminal_event"]["reason"], "cleanup_no_report_terminal")
                self.assertFalse(invalidation["prior_terminal_event"]["details"]["report_present"])
                manifest_id = connection.execute(
                    "SELECT manifest_id FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0]
                route_key = connection.execute(
                    "SELECT route_key FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0]
                successor = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id,
                    route_key=route_key, red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertTrue(successor.accepted, successor.reason)
                self.assertNotEqual(successor.token_id, token.token_id)
            finally:
                connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, manifest_path, state, red, token, binding = establish(root)
            try:
                green_report = self.report_with_proof(
                    manifest_path, status="green", feature_proof=True,
                    supersedes_verification_id=red["verification_id"],
                )
                green_report["step_ledger"] = [{"primitive_step": "terminal", "verdict": "green_terminal"}]
                path = root / "green.probe.report.json"
                self.write_json(path, green_report)
                accepted = ingest_repair_token_linked_report_reference(
                    connection, token.token_id, path, adapters=self.adapters(state),
                )
                self.assertEqual(accepted["status"], "ingested")
                route_history = connection.execute(
                    "SELECT evidence_state FROM capability_evidence_history WHERE capability_key = '_registry.proof_route' "
                    "ORDER BY capability_evidence_id"
                ).fetchall()
                self.assertEqual([row[0] for row in route_history], ["contradicted", "hard_proven"])
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0], 1)
                self.assertEqual(connection.execute(
                    "SELECT reason FROM token_history WHERE token_id = ? AND event_kind = 'repair_verification_run'",
                    (token.token_id,),
                ).fetchone()[0], "report_ingested_authoritative")
            finally:
                connection.close()

        for label, mutate, expected_reason in (
                ("missing_client", lambda terminal: terminal.pop("authoritative_terminal"),
                 "terminal_client_or_authority_missing"),
                ("wrong_binding", lambda terminal: terminal.__setitem__("runtime_binding", {}),
                 "terminal_runtime_binding_mismatch"),
                ("nonterminal", lambda terminal: terminal["authoritative_terminal"].__setitem__("status", "running"),
                 "terminal_client_or_authority_missing"),
                ("gameplay_promotion", lambda terminal: terminal.__setitem__("gameplay_credit", True),
                 "terminal_attempted_gameplay_promotion")):
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                connection, _manifest_path, _state, _red, token, binding = establish(root)
                try:
                    terminal = {
                        "schema": "caol-repair-compatibility-terminal-v1", "repair_token": token.token_id,
                        "runtime_binding": binding["runtime"], "gameplay_credit": False, "matrix_credit": False,
                        "terminal_result": "current_source_runtime_compatible",
                        "authoritative_terminal": {
                            "kind": "registry_current_source_runtime_compatibility", "status": "terminal",
                            "runtime_binding": binding["runtime"],
                        },
                    }
                    mutate(terminal)
                    path = root / f"{label}.repair-terminal.json"
                    self.write_json(path, terminal)
                    result = ingest_repair_compatibility_terminal(connection, token.token_id, path)
                    self.assertEqual(result["status"], "rejected_terminal")
                    self.assertEqual(result["reason"], expected_reason)
                finally:
                    connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _manifest_path, _state, red, token, binding = establish(root)
            try:
                terminal = {
                    "schema": "caol-repair-compatibility-terminal-v1", "repair_token": token.token_id,
                    "runtime_binding": binding["runtime"], "gameplay_credit": False, "matrix_credit": False,
                    "terminal_result": "current_source_runtime_compatible",
                    "authoritative_terminal": {
                        "kind": "registry_current_source_runtime_compatibility", "status": "terminal",
                        "runtime_binding": binding["runtime"],
                    },
                }
                path = root / "green.repair-terminal.json"
                self.write_json(path, terminal)
                result = ingest_repair_compatibility_terminal(connection, token.token_id, path)
                self.assertEqual(result["status"], "ingested_zero_credit")
                route = build_registry_query_candidate_snapshot(
                    connection, include_lifecycle_states=("quarantined",),
                )[0].explanation["route_evidence"][0]
                self.assertEqual(route["evidence_state"], "run-verified")
                self.assertIn(red["verification_id"], route["details"]["superseded_contradiction_ids"])
                self.assertEqual(route["details"]["hard_proven_verification_ids"], [])
                self.assertEqual(route["details"]["repair_bootstrap_verification_ids"], [result["verification_id"]])
            finally:
                connection.close()

        for label, status, feature_proof, supersession in (
                ("missing", "green", True, ""),
                ("wrong", "green", True, "not-the-bound-red-verification"),
                ("red", "red", False, "")):
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temp_dir:
                root = Path(temp_dir)
                connection, manifest_path, state, red, token, _binding = establish(root)
                try:
                    report = self.report_with_proof(
                        manifest_path, status=status, feature_proof=feature_proof,
                        supersedes_verification_id=supersession,
                    )
                    report["step_ledger"] = [{"primitive_step": "terminal", "verdict": label}]
                    path = root / f"{label}.probe.report.json"
                    self.write_json(path, report)
                    rejected = ingest_repair_token_linked_report_reference(
                        connection, token.token_id, path, adapters=self.adapters(state),
                    )
                    self.assertEqual(rejected["status"], "rejected_report")
                    route = build_registry_query_candidate_snapshot(
                        connection, include_lifecycle_states=("quarantined",),
                    )[0].explanation["route_evidence"][0]
                    self.assertEqual(route["evidence_state"], "contradicted")
                    self.assertIn(red["verification_id"], route["details"]["unresolved_contradiction_ids"])
                    self.assertFalse(reload_repair_token_for_launch(
                        connection, token.token_id, require_claimed=True,
                    ).accepted)
                    replayed = ingest_repair_token_linked_report_reference(
                        connection, token.token_id, path, adapters=self.adapters(state),
                    )
                    self.assertEqual(replayed["status"], "rejected_token")
                    self.assertGreaterEqual(connection.execute(
                        "SELECT COUNT(*) FROM token_history WHERE token_id = ? AND event_kind = 'repair_invalidated'",
                        (token.token_id,),
                    ).fetchone()[0], 1)
                finally:
                    connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _manifest_path, _state, red, token, binding = establish(root, claim=False)
            try:
                changed_binding = dict(binding)
                changed_binding["fixture"] = {**binding["fixture"], "source_sha256": hashlib.sha256(
                    b"changed fixture"
                ).hexdigest()}
                rejected = reload_repair_token_for_launch(
                    connection, token.token_id, binding=changed_binding,
                )
                self.assertEqual(rejected.reason, "binding_changed")
                route = build_registry_query_candidate_snapshot(
                    connection, include_lifecycle_states=("quarantined",),
                )[0].explanation["route_evidence"][0]
                self.assertEqual(route["evidence_state"], "contradicted")
                self.assertIn(red["verification_id"], route["details"]["unresolved_contradiction_ids"])
            finally:
                connection.close()

    def test_yellow_report_with_red_route_verdict_is_current_repair_contradiction(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, scenarios, manifest_path = self.setup_phase4_terminal_gate_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            try:
                report = self.report_with_proof(manifest_path, status="yellow", feature_proof=False)
                report["verdict"] = "red_wait_modal_receipt_missing"
                report["step_ledger"] = [{"primitive_step": "terminal", "verdict": report["verdict"]}]
                red = self.ingest_named_report(connection, root, "yellow-red-route.probe.report.json", report, state)
                self.assertEqual(red["eligibility"], "contradicted")
                manifest_id, route_key = connection.execute(
                    "SELECT manifest_id, route_key FROM verification_history WHERE verification_id = ?",
                    (red["verification_id"],),
                ).fetchone()
                route = build_registry_query_candidate_snapshot(
                    connection, include_lifecycle_states=("quarantined",),
                )[0].explanation["route_evidence"][0]
                self.assertEqual(route["evidence_state"], "contradicted")
                self.assertEqual(route["details"]["unresolved_contradiction_ids"], [red["verification_id"]])

                runtime = root / "runtime"
                runtime.write_bytes(b"runtime")
                binding = {
                    "runtime": {"schema": 1, "executable_path": str(runtime.resolve()),
                                "executable_sha256": hashlib.sha256(runtime.read_bytes()).hexdigest(),
                                "runtime_source_sha256": hashlib.sha256(b"source").hexdigest()},
                    "fixture": {"status": "compatible", "name": "", "profile": "", "source_path": str(root),
                                "source_sha256": hashlib.sha256(b"fixture").hexdigest()},
                    "profile": {"status": "compatible", "name": "", "profile": "", "source_path": str(root),
                                "source_sha256": hashlib.sha256(b"profile").hexdigest()},
                }
                issued = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id, route_key=route_key,
                    red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertTrue(issued.accepted, issued.reason)

                changed = self.phase4_terminal_gate_manifest()
                changed["description"] = "stale current route must reject repair authority"
                self.write_json(manifest_path, changed)
                rebuild_manifest_projection(connection, scenarios)
                self.assertEqual(reconcile_report_bindings(connection, adapters=self.adapters(state)), {"reconciled": 1, "stale": 1})
                current = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id, route_key=route_key,
                    red_verification_id=red["verification_id"], binding=binding,
                )
                self.assertTrue(current.accepted, current.reason)
                rejected = issue_registry_repair_token(
                    connection, self.phase4_terminal_gate_request(), manifest_id=manifest_id, route_key=route_key,
                    red_verification_id="stale-or-unbound-red-verification", binding=binding,
                )
                self.assertFalse(rejected.accepted)
                self.assertEqual(rejected.reason, "route_not_current_contradiction")
            finally:
                connection.close()

    def test_yellow_successor_cannot_hide_current_focused_red_repair_route(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path = self.setup_phase4_terminal_gate_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            try:
                red_report = self.report_with_proof(manifest_path, status="yellow", feature_proof=False)
                red_report["verdict"] = "red_wait_modal_receipt_missing"
                red_report["step_ledger"] = [{"primitive_step": "terminal", "verdict": red_report["verdict"]}]
                red = self.ingest_named_report(connection, root, "focused-red.probe.report.json", red_report, state)
                successor = self.ingest_named_report(
                    connection,
                    root,
                    "yellow-successor.probe.report.json",
                    self.report_with_proof(
                        manifest_path,
                        status="yellow",
                        feature_proof=False,
                        supersedes_verification_id=red["verification_id"],
                    ),
                    state,
                )
                self.assertEqual(successor["eligibility"], "contradicted")
                route = build_registry_query_candidate_snapshot(
                    connection, include_lifecycle_states=("quarantined",),
                )[0].explanation["route_evidence"][0]
                self.assertEqual(route["evidence_state"], "contradicted")
                self.assertEqual(route["details"]["unresolved_contradiction_ids"], [red["verification_id"]])
            finally:
                connection.close()

    def test_named_gate_contradiction_outranks_success_until_same_route_supersession(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection, _scenarios, manifest_path = self.setup_gated_registry(root)
            state = {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}
            current_manifest = connection.execute(
                "SELECT revision, current_sha256, declaration_json FROM manifest_current"
            ).fetchone()
            success = self.ingest_named_report(
                connection, root, "success.probe.report.json", self.report_with_named_gates(manifest_path), state,
            )
            contradiction = self.ingest_named_report(
                connection,
                root,
                "contradiction.probe.report.json",
                self.report_with_named_gates(
                    manifest_path,
                    production="red_production_gate_failed",
                    status="red",
                    feature_proof=False,
                ),
                state,
            )
            self.assertEqual(success["eligibility"], "hard_proven")
            self.assertEqual(contradiction["eligibility"], "contradicted")
            self.assertEqual(
                build_registry_query_candidate_snapshot(connection, include_lifecycle_states=("quarantined",))[0]
                .facts["player.interaction_gate"]["evidence_state"],
                "contradicted",
            )
            later_success = self.ingest_named_report(
                connection, root, "later-success.probe.report.json", self.report_with_named_gates(manifest_path), state,
            )
            self.assertEqual(later_success["eligibility"], "contradicted")
            superseding_success = self.ingest_named_report(
                connection,
                root,
                "superseding-success.probe.report.json",
                self.report_with_named_gates(
                    manifest_path,
                    supersedes_verification_id=contradiction["verification_id"],
                ),
                state,
            )
            self.assertEqual(superseding_success["eligibility"], "hard_proven")
            facts = build_registry_query_candidate_snapshot(connection)[0].facts
            self.assertEqual(facts["player.interaction_gate"]["evidence_state"], "run-verified")
            self.assertEqual(facts["player.interaction_gate"]["proof_depth"], "interaction")
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM capability_evidence_history "
                    "WHERE capability_key = 'player.interaction_gate' AND evidence_state = 'contradicted'"
                ).fetchone()[0],
                1,
            )
            self.assertEqual(
                tuple(connection.execute("SELECT revision, current_sha256, declaration_json FROM manifest_current").fetchone()),
                tuple(current_manifest),
            )
            repeated = ingest_report_reference(
                connection,
                root / "superseding-success.probe.report.json",
                adapters=self.adapters(state),
            )
            self.assertTrue(repeated["idempotent"])
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM capability_evidence_history "
                    "WHERE capability_key = 'player.interaction_gate' AND evidence_kind = 'named_proof_gate'"
                ).fetchone()[0],
                4,
            )
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

    def test_windows_feel_handoff_preserves_external_result_without_forging_a_machine_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection, _scenarios, manifest_path, report_path = self.setup_registry(Path(temp_dir))
            source_sha256 = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
            report = self.report(manifest_path)
            round_manifest, authority = self.register_valid_round(connection, source_sha256=source_sha256)
            report["startup"]["screen"]["runtime_binding_observed"]["executable_sha256"] = "b" * 64
            report.update({
                "run_id": authority["run_id"], "binding_id": authority["binding_id"],
                "wec_authority": authority, "event_stream_id": round_manifest["event_stream_id"],
                "certification_round": {
                    "round_id": round_manifest["round_id"], "authority_id": round_manifest["authority_id"],
                    "binding_id": round_manifest["binding_id"], "event_stream_id": round_manifest["event_stream_id"],
                    "manifest_sha256": round_manifest["manifest_sha256"], "lifecycle_state": "complete",
                    "registry_derived": "true",
                },
            })
            report["certification_lifecycle"]["events"] = [
                dict(event, round_id=round_manifest["round_id"], binding_id=round_manifest["binding_id"])
                for event in report["certification_lifecycle"]["events"]
            ]
            self.write_json(report_path, report)
            adapters = BindingAdapters(
                runtime=lambda _expected: {"status": "compatible", "facts": {
                    "executable_sha256": "b" * 64, "source_sha256": "1" * 64}},
                fixture=lambda _expected: {"status": "compatible", "facts": {"source_sha256": "2" * 64}},
                profile=lambda _expected: {"status": "compatible", "facts": {"source_sha256": "3" * 64}},
            )
            certified = ingest_report_reference(connection, report_path, adapters=adapters)
            with self.assertRaisesRegex(Exception, "does not match the certified binding"):
                prepare_windows_feel_handoff(
                    connection, certification_verification_id=certified["verification_id"], windows_build={
                        "platform": "windows", "executable_path": "C:/AOL/cataclysm-tiles.exe",
                        "executable_sha256": "c" * 64, "world": "ordinary-play-world",
                    },
                )
            handoff = prepare_windows_feel_handoff(
                connection, certification_verification_id=certified["verification_id"], windows_build={
                    "platform": "windows", "executable_path": "C:/AOL/cataclysm-tiles.exe",
                    "executable_sha256": "b" * 64, "world": "ordinary-play-world",
                },
            )["handoffs"][0]
            self.assertEqual(handoff["state"], "pending")
            self.assertNotIn("debug", json.dumps(handoff["ordinary_play"]).lower())
            self.assertFalse(final_gate_eligibility(connection)["overall_acceptance"])
            with self.assertRaisesRegex(Exception, "does not authenticate callers"):
                record_windows_feel_judgment(
                    connection, handoff_id=handoff["handoff_id"], outcome="pass", author="automation",
                )
            recorded = record_windows_feel_judgment(
                connection, handoff_id=handoff["handoff_id"], outcome="fail", author="Josef", notes="not coherent",
            )["handoffs"][0]
            self.assertEqual(recorded["state"], "fail")
            self.assertFalse(recorded["judgment"]["machine_verified"])
            self.assertFalse(final_gate_eligibility(connection)["windows_feel"])
            with self.assertRaisesRegex(Exception, "immutable"):
                record_windows_feel_judgment(
                    connection, handoff_id=handoff["handoff_id"], outcome="pass", author="Josef",
                )
            second_round, second_authority = self.register_valid_round(
                connection, source_sha256=source_sha256, round_id="registry-owned-second",
            )
            second_report = json.loads(json.dumps(report))
            second_report.update({
                "run_id": second_authority["run_id"], "binding_id": second_authority["binding_id"],
                "wec_authority": second_authority, "event_stream_id": second_round["event_stream_id"],
                "certification_round": {
                    "round_id": second_round["round_id"], "authority_id": second_round["authority_id"],
                    "binding_id": second_round["binding_id"], "event_stream_id": second_round["event_stream_id"],
                    "manifest_sha256": second_round["manifest_sha256"], "lifecycle_state": "complete",
                    "registry_derived": "true",
                },
            })
            second_report["certification_lifecycle"]["events"] = [
                dict(event, round_id=second_round["round_id"], binding_id=second_round["binding_id"])
                for event in second_report["certification_lifecycle"]["events"]
            ]
            second_path = report_path.with_name("second.certification.report.json")
            self.write_json(second_path, second_report)
            second = ingest_report_reference(connection, second_path, adapters=adapters)
            passed_handoff = prepare_windows_feel_handoff(
                connection, certification_verification_id=second["verification_id"], windows_build={
                    "platform": "windows", "executable_path": "C:/AOL/cataclysm-tiles.exe",
                    "executable_sha256": "b" * 64, "world": "ordinary-play-world",
                },
            )["handoffs"][0]
            record_windows_feel_judgment(
                connection, handoff_id=passed_handoff["handoff_id"], outcome="pass", author="Josef",
            )
            final_gates = final_gate_eligibility(connection)
            self.assertFalse(final_gates["windows_feel"])
            self.assertFalse(final_gates["overall_acceptance"])
            self.assertEqual(final_gates["windows_feel_authority"], "external-non-machine-verifiable")
            self.assertEqual(final_gates["overall_acceptance_state"], "external-owner-judgment-required")
            self.assertIn(
                "pass", {item["outcome"] for item in final_gates["external_windows_feel_attestations"]},
            )
            self.assertEqual(windows_feel_handoff_status(connection)["handoffs"][0]["judgment"]["author"], "Josef")
            connection.close()


if __name__ == "__main__":
    unittest.main()
