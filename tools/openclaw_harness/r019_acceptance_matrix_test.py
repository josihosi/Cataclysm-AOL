#!/usr/bin/env python3
"""Legacy R-019 packet validation and current witness-route declaration tests."""

from __future__ import annotations

import unittest
import sys
from pathlib import Path
import json
import sqlite3
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from r019_acceptance_matrix import validate_r019_acceptance_matrix
import scenario_registry_store


STOPS = ("safe_flavour", "safe_prompt", "target_crossed", "meaningful_or_unknown_event",
         "unsafe_condition", "unknown_safety_frame", "stale_observation", "binding_drift",
         "no_progress", "derived_bound_exhausted")
SCENARIO_PATH = Path(__file__).resolve().parent / "scenarios" / "r019.keep_watch_acceptance_mcw.json"


def report(report_id: str, role: str) -> dict[str, object]:
    packet: dict[str, object] = {
        "role": role,
        "clean_start_identity": "fixture:clean-wait:minute-8904",
        "source_identity": "fixture=r013;profile=r009-m095;runtime=abc",
        "native_transitions": ["world.wait", "wait.duration_menu", "wait.1m"],
        "terminal_state": {"game_minutes": 8905, "state": "world"},
        "role_receipt": {"schema": "caol-r019-role-receipt-v1", "role": role,
                         "run_id": "run-" + report_id, "binding_id": "binding-abc"},
        "registry_authority_id": "authority-" + report_id,
        "registry_executable_binding": "executable-binding-abc",
        "round_trip_receipt": {
            "schema": "caol-r019-round-trip-receipt-v1",
            "model": {"count": 1, "measurement": "cockpit transcript decision entries"},
            "tool": {"count": 3, "measurement": "accepted native dispatch receipts"},
        },
    }
    if role.startswith("stop:"):
        packet["stop_receipt"] = {"stop_reason": role.removeprefix("stop:"),
                                  "native_dispatch_after_stop": False}
    if role.startswith("off:"):
        switch = role.removeprefix("off:")
        packet["off_switch_receipt"] = {
            "schema": "caol-r019-off-switch-receipt-v1", "switch": switch,
            "native_dispatch_count": 0, "guarded_recipe_dispatch_count": 0,
            "primitive_native_dispatch_count": 1, "guarded_handling_count": 0,
            "hidden_batching": False, "native_receipt_actions": ["world.wait"],
        }
    return {"report_id": report_id, "evidence_class": "feature-path",
            "wec_authority": {"authority_id": "authority-" + report_id,
                              "binding_id": "executable-binding-abc"},
            "r019_acceptance_matrix": packet}


class R019AcceptanceMatrixTest(unittest.TestCase):
    def test_acceptance_hud_declares_the_visible_fact_bound_by_its_text_guard(self) -> None:
        scenario = json.loads(SCENARIO_PATH.read_text(encoding="utf-8"))
        hud_step = next(step for step in scenario["steps"] if step["label"] ==
                        "post_load_r019_keep_watch_acceptance_hud")

        self.assertIn("Move:", hud_step["expected_visible_fact"])
        self.assertIn("Wield:", hud_step["expected_visible_fact"])
        self.assertEqual(hud_step["expected_screen_text_after_contains"], ["Move:", "Wield:"])
        self.assertTrue(scenario["suppress_profile_startup_input"])
        self.assertTrue(scenario["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(scenario["fixture"], "r019_keep_watch_safe_popup_v1")
        self.assertIn("cockpit:run.witness", scenario["runtime_contract"]["permitted_input"])
        live = next(step for step in scenario["steps"] if step["kind"] == "cockpit_live_session")
        self.assertNotIn("r019_timed_entry", live)

    def complete_reports(self) -> list[dict[str, object]]:
        return [report("guarded-report", "guarded"), report("primitive-report", "primitive"),
                *(report("stop-" + stop, "stop:" + stop) for stop in STOPS),
                report("off-master", "off:master_enabled"), report("off-recipe", "off:enabled")]

    def test_distinct_identical_clean_start_reports_prove_the_relation(self) -> None:
        result = validate_r019_acceptance_matrix(self.complete_reports())
        self.assertEqual(result["status"], "green")
        self.assertTrue(all(value == "feature-path" for value in
                            result["preserved_evidence_classes"].values()))

    def test_terminal_mismatch_is_red_even_when_all_other_inputs_are_present(self) -> None:
        reports = self.complete_reports()
        primitive = reports[1]
        primitive["r019_acceptance_matrix"]["terminal_state"] = {"game_minutes": 8906, "state": "world"}  # type: ignore[index]
        result = validate_r019_acceptance_matrix(reports)
        self.assertEqual(result["status"], "red")
        self.assertIn("mismatched_terminal_state", result["errors"])

    def test_missing_or_invalid_role_and_cost_receipts_are_red(self) -> None:
        reports = self.complete_reports()
        guarded = reports[0]["r019_acceptance_matrix"]  # type: ignore[index]
        guarded.pop("role_receipt")
        primitive = reports[1]["r019_acceptance_matrix"]  # type: ignore[index]
        primitive["round_trip_receipt"]["tool"]["count"] = -1  # type: ignore[index]

        result = validate_r019_acceptance_matrix(reports)

        self.assertEqual(result["status"], "red")
        self.assertIn("invalid_role_receipt:guarded", result["errors"])
        self.assertIn("missing_measured_round_trips:primitive", result["errors"])

    def test_duplicate_or_contradictory_role_receipts_are_red(self) -> None:
        reports = self.complete_reports()
        duplicate = report("second-guarded", "guarded")
        reports.append(duplicate)
        reports[1]["r019_acceptance_matrix"]["role_receipt"]["role"] = "guarded"  # type: ignore[index]

        result = validate_r019_acceptance_matrix(reports)

        self.assertEqual(result["status"], "red")
        self.assertIn("duplicate_role:guarded", result["errors"])
        self.assertIn("invalid_role_receipt:primitive", result["errors"])

    def test_paired_aggregation_is_green_before_future_controls_are_authorized(self) -> None:
        result = validate_r019_acceptance_matrix(self.complete_reports()[:2])
        self.assertEqual(result["status"], "green")

    def test_shared_authority_or_executable_mismatch_is_red(self) -> None:
        reports = self.complete_reports()[:2]
        primitive = reports[1]["r019_acceptance_matrix"]  # type: ignore[index]
        primitive["registry_authority_id"] = "authority-guarded-report"
        primitive["registry_executable_binding"] = "other-executable"

        result = validate_r019_acceptance_matrix(reports)

        self.assertEqual(result["status"], "red")
        self.assertIn("comparison_requires_distinct_authority_ids", result["errors"])
        self.assertIn("mismatched_registry_executable_binding", result["errors"])

    def test_registry_persists_only_the_distinct_report_relation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            complete = self.complete_reports()
            report_paths = []
            for item in complete:
                path = root / (str(item["report_id"]) + ".json")
                path.write_text(json.dumps(item), encoding="utf-8")
                report_paths.append((str(item["report_id"]), path))
            connection = sqlite3.connect(":memory:")
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA foreign_keys = ON")
            scenario_registry_store.apply_migrations(connection)
            connection.execute(
                "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
                "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
                (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
            )
            for report_id, path in report_paths:
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, 'r019', ?, 'sha', 'probe', 'ingested' )", (report_id, str(path)),
                )
            result = scenario_registry_store._record_r019_acceptance_matrix(connection, manifest_id="r019")
            self.assertEqual(result["status"], "green")
            self.assertEqual(connection.execute(
                "SELECT status FROM r019_acceptance_matrix_history").fetchone()["status"], "green")
            self.assertEqual(connection.execute(
                "SELECT status FROM r019_acceptance_matrix_evaluation_history").fetchone()["status"], "green")

    def test_hard_proof_requires_the_current_green_matrix_input_identities(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            reports = self.complete_reports()
            relation = validate_r019_acceptance_matrix(reports)
            connection = sqlite3.connect(":memory:")
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA foreign_keys = ON")
            scenario_registry_store.apply_migrations(connection)
            connection.execute(
                "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
                "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
                (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
            )
            route_key = "r019-route"
            candidates = {}
            for item in reports:
                report_id = str(item["report_id"])
                report_path = root / (report_id + ".json")
                report_path.write_text(json.dumps(item), encoding="utf-8")
                verification_id = "verification-" + report_id
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, 'r019', ?, 'sha', 'probe', 'ingested' )", (report_id, str(report_path)),
                )
                details = json.dumps({"proof": {"status": "green", "feature_proof": True,
                                                "evidence_class": "feature-path"}})
                connection.execute(
                    "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
                    "VALUES( ?, 'r019', ?, ?, 'binding', 'green', 'green', ? )",
                    (verification_id, report_id, route_key, details),
                )
                candidates[verification_id] = connection.execute(
                    "SELECT * FROM verification_history WHERE verification_id = ?", (verification_id,)
                ).fetchone()
            details_json = json.dumps(relation)
            connection.execute(
                "INSERT INTO r019_acceptance_matrix_evaluation_history( manifest_id, guarded_report_id, primitive_report_id, status, details_json, details_sha256 ) "
                "VALUES( 'r019', 'guarded-report', 'primitive-report', 'green', ?, 'green-matrix' )",
                (details_json,),
            )
            promoted = scenario_registry_store._r019_matrix_hard_proven_candidates(
                connection, manifest_id="r019", route_key=route_key, candidate_rows=candidates,
                report_local_hard_proven=set(candidates),
            )
            self.assertEqual(promoted, set(candidates))
            connection.execute(
                "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, supersedes_verification_id, details_json ) "
                "VALUES( 'replacement', 'r019', 'guarded-report', ?, 'replacement', 'green', 'green', 'verification-guarded-report', ? )",
                (route_key, json.dumps({"proof": {"status": "green", "feature_proof": True,
                                                    "evidence_class": "feature-path"}})),
            )
            self.assertEqual(scenario_registry_store._r019_matrix_hard_proven_candidates(
                connection, manifest_id="r019", route_key=route_key, candidate_rows=candidates,
                report_local_hard_proven=set(candidates),
            ), set())

    def test_missing_or_red_matrix_never_promotes_report_local_feature_proof(self) -> None:
        connection = sqlite3.connect(":memory:")
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA foreign_keys = ON")
        scenario_registry_store.apply_migrations(connection)
        connection.execute(
            "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
            "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
            (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
        )
        self.assertEqual(scenario_registry_store._r019_matrix_hard_proven_candidates(
            connection, manifest_id="r019", route_key="r019-route", candidate_rows={},
            report_local_hard_proven={"report-local-proof"},
        ), set())

    def test_tokenized_zero_credit_aggregation_binds_only_the_exact_fresh_pair(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection = sqlite3.connect(":memory:")
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA foreign_keys = ON")
            scenario_registry_store.apply_migrations(connection)
            connection.execute(
                "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
                "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
                (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
            )
            reports = self.complete_reports()[:2]
            for item in reports:
                report_id = str(item["report_id"])
                path = root / (report_id + ".json")
                payload = json.dumps(item)
                path.write_text(payload, encoding="utf-8")
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, 'r019', ?, ?, 'probe', 'ingested' )",
                    (report_id, str(path), __import__("hashlib").sha256(payload.encode()).hexdigest()),
                )
                connection.execute(
                    "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
                    "VALUES( ?, 'r019', ?, 'r019-route', 'binding', 'green', 'green', '{}' )",
                    ("verification-" + report_id, report_id),
                )
            connection.commit()

            rejected_missing = scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="missing", primitive_report_id="primitive-report",
            )
            self.assertFalse(rejected_missing.accepted)
            self.assertEqual(rejected_missing.reason, "r019_aggregation_report_absent")
            rejected_duplicate = scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="guarded-report", primitive_report_id="guarded-report",
            )
            self.assertFalse(rejected_duplicate.accepted)
            self.assertEqual(rejected_duplicate.reason, "r019_aggregation_duplicate_report_id")

            token = scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="guarded-report", primitive_report_id="primitive-report",
            )
            self.assertTrue(token.accepted)
            terminal = scenario_registry_store.finalize_r019_aggregation_token(connection, token.token_id)
            self.assertEqual(terminal["status"], "terminalized")
            packet = json.loads(connection.execute(
                "SELECT packet_json FROM r019_aggregation_terminal_history"
            ).fetchone()["packet_json"])
            self.assertEqual(packet["credit"], "zero")
            self.assertEqual(packet["guarded_report_id"], "guarded-report")
            self.assertEqual(packet["primitive_report_id"], "primitive-report")
            self.assertEqual(connection.execute(
                "SELECT COUNT(*) FROM r019_aggregation_terminal_history"
            ).fetchone()[0], 1)
            self.assertFalse(scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="guarded-report", primitive_report_id="primitive-report",
            ).accepted)

    def test_tokenized_aggregation_rejects_contradictory_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection = sqlite3.connect(":memory:")
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA foreign_keys = ON")
            scenario_registry_store.apply_migrations(connection)
            connection.execute(
                "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
                "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
                (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
            )
            reports = self.complete_reports()[:2]
            reports[1]["r019_acceptance_matrix"]["terminal_state"] = {"game_minutes": 999, "state": "world"}  # type: ignore[index]
            for item in reports:
                report_id = str(item["report_id"])
                path = root / (report_id + ".json")
                payload = json.dumps(item)
                path.write_text(payload, encoding="utf-8")
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, 'r019', ?, ?, 'probe', 'ingested' )",
                    (report_id, str(path), __import__("hashlib").sha256(payload.encode()).hexdigest()),
                )
                connection.execute(
                    "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
                    "VALUES( ?, 'r019', ?, 'r019-route', 'binding', 'green', 'green', '{}' )",
                    ("verification-" + report_id, report_id),
                )
            connection.commit()
            contradictory = scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="guarded-report", primitive_report_id="primitive-report",
            )
            self.assertFalse(contradictory.accepted)
            self.assertIn("r019_aggregation_relation_red", contradictory.reason)

    def test_tokenized_aggregation_rejects_superseded_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection = sqlite3.connect(":memory:")
            connection.row_factory = sqlite3.Row
            connection.execute("PRAGMA foreign_keys = ON")
            scenario_registry_store.apply_migrations(connection)
            connection.execute(
                "INSERT INTO manifest_current( manifest_id, source_path, present, revision, declaration_json, normalized_json, validation_json ) "
                "VALUES( 'r019', 'scenario.json', 1, 1, ?, '{}', '{}' )",
                (json.dumps({"name": "r019.keep_watch_acceptance_mcw"}),),
            )
            for item in self.complete_reports()[:2]:
                report_id = str(item["report_id"])
                path = root / (report_id + ".json")
                payload = json.dumps(item)
                path.write_text(payload, encoding="utf-8")
                connection.execute(
                    "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
                    "VALUES( ?, 'r019', ?, ?, 'probe', 'ingested' )",
                    (report_id, str(path), __import__("hashlib").sha256(payload.encode()).hexdigest()),
                )
                connection.execute(
                    "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
                    "VALUES( ?, 'r019', ?, 'r019-route', 'binding', 'green', 'green', '{}' )",
                    ("verification-" + report_id, report_id),
                )
            connection.execute(
                "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, supersedes_verification_id, details_json ) "
                "VALUES( 'replacement', 'r019', 'primitive-report', 'r019-route', 'new', 'green', 'green', 'verification-primitive-report', '{}' )"
            )
            connection.commit()
            superseded = scenario_registry_store.issue_r019_aggregation_token(
                connection, guarded_report_id="guarded-report", primitive_report_id="primitive-report",
            )
            self.assertFalse(superseded.accepted)
            self.assertEqual(superseded.reason, "r019_aggregation_report_superseded")


if __name__ == "__main__":
    unittest.main()
