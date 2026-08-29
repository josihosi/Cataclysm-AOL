#!/usr/bin/env python3
"""Fail-closed controls for the stale R-019 current-source repair successor."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_store as store


class R019CurrentSourceRepairSuccessorTest(unittest.TestCase):
    def establish(self, root: Path, *, claim_token: bool = True, issue_token: bool = True):
        scenarios = root / "scenarios"
        scenarios.mkdir()
        source = HARNESS_DIR / "scenarios" / "r019.keep_watch_acceptance_mcw.json"
        manifest_path = scenarios / source.name
        manifest_path.write_bytes(source.read_bytes())
        connection = store.open_registry(str(root / "registry.sqlite3"))
        store.rebuild_manifest_projection(connection, scenarios)
        manifest_id = connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0]
        route_key = "r019_keep_watch_acceptance_bound"
        red_id = "historical-stale-red"
        connection.execute(
            "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, report_kind, ingestion_status ) "
            "VALUES( 'historical-report', ?, 'historical-report.json', 'old', 'probe', 'ingested' )",
            (manifest_id,),
        )
        connection.execute(
            "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
            "VALUES( ?, ?, 'historical-report', ?, 'stale-binding', 'red', 'red', ? )",
            (red_id, manifest_id, route_key, json.dumps({"proof": {"status": "red"}})),
        )
        connection.execute(
            "INSERT INTO verification_resolution_history( verification_id, manifest_id, route_key, resolution_kind, binding_fingerprint, details_json ) "
            "VALUES( ?, ?, ?, 'compatible', 'stale-binding', '{}' )", (red_id, manifest_id, route_key),
        )
        store._resolve_route_evidence(connection, manifest_id=manifest_id, route_key=route_key)
        executable = root / "cataclysm-tiles"
        executable.write_bytes(b"current runtime")
        binding = {
            "runtime": {"schema": 1, "executable_path": str(executable.resolve()),
                        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                        "runtime_source_sha256": hashlib.sha256(b"current source").hexdigest()},
            "fixture": {"status": "compatible", "name": "r013_clean_wait_duration_v1", "profile": "live-debug",
                        "source_path": str(root.resolve()), "source_sha256": hashlib.sha256(b"fixture").hexdigest()},
            "profile": {"status": "compatible", "name": "", "profile": "",
                        "source_path": str(root.resolve()), "source_sha256": hashlib.sha256(b"profile").hexdigest()},
        }
        request = store.parse_registry_query_request({"requirements": [{
            "key": "capabilities.r019.keep_watch_acceptance", "op": "eq",
            "value": "source_bound_guarded_keep_watch_and_primitive_wait_comparison",
            "minimum_evidence": "declared"}], "preferences": []})
        if not issue_token:
            return connection, None, {}, red_id
        token = store.issue_registry_repair_token(connection, request, manifest_id=manifest_id,
                                                  route_key=route_key, red_verification_id=red_id, binding=binding)
        self.assertTrue(token.accepted, token.reason)
        if claim_token:
            claimed = store.claim_repair_token_for_launch(connection, token.token_id, binding=binding)
            self.assertTrue(claimed.accepted, claimed.reason)
        manifest_sha = hashlib.sha256(manifest_path.read_bytes()).hexdigest()
        successor = {
            "schema": "caol-r019-current-source-repair-successor-v1", "repair_token": token.token_id,
            "manifest": {"source_path": str(manifest_path.resolve()), "source_sha256": manifest_sha},
            "runtime_binding": binding["runtime"], "gameplay_credit": False, "matrix_credit": False,
            "terminal_result": "current_source_r019_hud_boundary_repaired",
            "hud_boundary": {"status": "terminal", "step_label": "post_load_r019_keep_watch_acceptance_hud",
                             "expected_visible_fact": "the ordinary gameplay HUD visibly presents the declared Move: and Wield: controls before the Keep watch session opens",
                             "observed_screen_text": ["Move:", "Wield:"]},
        }
        return connection, token, successor, red_id

    def test_successor_preserves_stale_history_and_restores_non_matrix_authority(self):
        with tempfile.TemporaryDirectory() as temp:
            connection, token, successor, red_id = self.establish(Path(temp))
            path = Path(temp) / "successor.json"
            path.write_text(json.dumps(successor), encoding="utf-8")
            result = store.ingest_r019_current_source_repair_successor(connection, token.token_id, path)
            self.assertEqual(result["status"], "ingested_zero_credit")
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM verification_history WHERE verification_id = ?", (red_id,)).fetchone()[0], 1)
            detail = json.loads(connection.execute("SELECT details_json FROM verification_history WHERE verification_id = ?", (result["verification_id"],)).fetchone()[0])
            self.assertFalse(detail["proof"]["feature_proof"])
            self.assertFalse(detail["repair_bootstrap"]["matrix_credit"])
            manifest_id = connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0]
            route = store._current_route_evidence(connection, manifest_id)[0]
            successor_binding = next(
                item for item in route["bindings"] if item["verification_id"] == result["verification_id"]
            )
            self.assertEqual(successor_binding["resolution"], "compatible")

    def test_stale_red_row_issues_only_current_source_successor_authority(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            connection, _token, _successor, red_id = self.establish(
                root, claim_token=False, issue_token=False,
            )
            manifest_id = connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0]
            route_key = "r019_keep_watch_acceptance_bound"
            connection.execute(
                "INSERT INTO verification_resolution_history( verification_id, manifest_id, route_key, "
                "resolution_kind, binding_fingerprint, details_json ) VALUES( ?, ?, ?, 'stale', 'current-source', '{}' )",
                (red_id, manifest_id, route_key),
            )
            store._resolve_route_evidence(connection, manifest_id=manifest_id, route_key=route_key)
            executable = root / "cataclysm-tiles"
            binding = {
                "runtime": {"schema": 1, "executable_path": str(executable.resolve()),
                            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                            "runtime_source_sha256": hashlib.sha256(b"current source").hexdigest()},
                "fixture": {"status": "compatible", "name": "r013_clean_wait_duration_v1", "profile": "live-debug",
                            "source_path": str(root.resolve()), "source_sha256": hashlib.sha256(b"fixture").hexdigest()},
                "profile": {"status": "compatible", "name": "", "profile": "",
                            "source_path": str(root.resolve()), "source_sha256": hashlib.sha256(b"profile").hexdigest()},
            }
            request = store.parse_registry_query_request({"requirements": [{
                "key": "capabilities.r019.keep_watch_acceptance", "op": "eq",
                "value": "source_bound_guarded_keep_watch_and_primitive_wait_comparison",
                "minimum_evidence": "declared"}], "preferences": []})
            token = store.issue_registry_repair_token(connection, request, manifest_id=manifest_id,
                                                      route_key=route_key, red_verification_id=red_id, binding=binding)
            self.assertTrue(token.accepted, token.reason)
            details = json.loads(connection.execute(
                "SELECT details_json FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued'",
                (token.token_id,)).fetchone()[0])
            self.assertEqual(details["authority_kind"], "registry_repair_r019_current_source_successor")
            claimed = store.claim_repair_token_for_launch(connection, token.token_id, binding=binding)
            self.assertTrue(claimed.accepted, claimed.reason)
            terminal_path = root / "unused.json"
            terminal_path.write_text("{}", encoding="utf-8")
            rejected = store.ingest_repair_compatibility_terminal(connection, token.token_id, terminal_path)
            self.assertEqual(rejected["reason"], "r019_successor_required")

    def test_wrong_source_manifest_visible_fact_nonterminal_and_credit_fail_closed(self):
        for label, mutate, reason in (
                ("wrong_source", lambda item: item["runtime_binding"].__setitem__("runtime_source_sha256", "wrong"), "successor_runtime_binding_mismatch"),
                ("wrong_manifest", lambda item: item["manifest"].__setitem__("source_sha256", "wrong"), "successor_manifest_mismatch"),
                ("missing_visible_fact", lambda item: item["hud_boundary"].__setitem__("observed_screen_text", ["Move:"]), "successor_visible_fact_missing"),
                ("nonterminal", lambda item: item["hud_boundary"].__setitem__("status", "running"), "successor_nonterminal"),
                ("credit", lambda item: item.__setitem__("matrix_credit", True), "successor_attempted_credit")):
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temp:
                connection, token, successor, _ = self.establish(Path(temp))
                mutate(successor)
                path = Path(temp) / "bad.json"
                path.write_text(json.dumps(successor), encoding="utf-8")
                result = store.ingest_r019_current_source_repair_successor(connection, token.token_id, path)
                self.assertEqual(result["reason"], reason)


if __name__ == "__main__":
    unittest.main()
