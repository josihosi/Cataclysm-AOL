"""SQLite proof for deterministic diagnostic capsule selection."""

import json
import sys
import tempfile
import unittest

from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from scenario_registry_store import open_registry, select_diagnostic_capsule_candidate


class DiagnosticCapsuleSelectorTest(unittest.TestCase):
    def setUp(self):
        self.registry_file = tempfile.NamedTemporaryFile(suffix=".sqlite3", delete=False)
        self.registry_file.close()
        self.db = open_registry(self.registry_file.name)
        self.db.execute("PRAGMA foreign_keys = OFF")
        self.db.executemany(
            "INSERT INTO report_ingestion_history(report_id, report_path, report_sha256, report_kind, ingestion_status) VALUES(?,?,?,?,?)",
            [(f"r-{i}", f"/tmp/r-{i}", "a" * 64, "probe", "ingested") for i in range(8)],
        )
        self.db.executemany(
            "INSERT INTO verification_history(verification_id, manifest_id, report_id, route_key, binding_fingerprint, outcome_kind, proof_status, details_json) VALUES(?,?,?,?,?,?,?,?)",
            [(f"v-{i}", "m", f"r-{i}", "route", "b", "completed", "green", "{}") for i in range(8)],
        )
        self.binding = {"state": {"save": "s1"}, "player": {"id": "p1"}, "actors": [{"id": "a1"}], "owner": "world"}
        self.expected = {"binding_id": "bind-1", "binding": self.binding, "site_id": "camp", "operation": "scout", "generation": "7", "actor_ids": ["a1"], "owner": "world", "run_id": "failed"}

    def add(self, candidate_id, *, run_id="history", binding=None, site="camp", operation="scout", generation="7", actors=("a1",), owner="world", gate=2, timestamp="2026-08-22T10:00:00Z"):
        index = int(candidate_id[-1], 36) % 8
        self.db.execute(
            "INSERT INTO diagnostic_capsule_candidate(candidate_id, report_id, verification_id, run_id, report_path, report_sha256, artifact_path, artifact_sha256, binding_id, binding_json, site_id, operation, generation, actor_ids_json, owner, gate_id, gate_index, durable_timestamp, source_kind, details_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (candidate_id, f"r-{index}", f"v-{index}", run_id, "/tmp/report", "b" * 64, "/tmp/save", "c" * 64, "bind-1", json.dumps(binding or self.binding), site, operation, generation, json.dumps(list(actors)), owner, "gate", gate, timestamp, "focused", "{}"),
        )

    def test_ranks_and_explains_all_compatibility_outcomes(self):
        self.add("z-gate", gate=4)
        self.add("a-time", gate=4, timestamp="2026-08-22T11:00:00Z")
        self.add("b-id", gate=4, timestamp="2026-08-22T11:00:00Z")
        self.add("newer-incompatible", site="other", timestamp="2026-08-22T99:00:00Z")
        self.add("missing-dimension", binding={"state": self.binding["state"], "player": self.binding["player"], "actors": self.binding["actors"]})
        self.add("same-run", run_id="failed", gate=99)
        first = select_diagnostic_capsule_candidate(self.db, failed_run=self.expected)
        second = select_diagnostic_capsule_candidate(self.db, failed_run=self.expected)
        self.assertEqual(first, second)
        self.assertEqual(first["selected_candidate"]["candidate_id"], "a-time")
        self.assertEqual([row["candidate_id"] for row in first["compatible_candidates"]], ["a-time", "b-id", "z-gate"])
        rejected = {row["candidate"]["candidate_id"]: row["reasons"] for row in first["rejected_candidates"]}
        self.assertIn({"dimension": "site_id", "reason": "unequal", "observed": "other"}, rejected["newer-incompatible"])
        self.assertTrue(any(reason["dimension"] == "binding.owner" and reason["reason"] == "missing" for reason in rejected["missing-dimension"]))
        self.assertNotIn("same-run", str(first))

    def test_probe_route_attaches_first_divergence_without_mutating_finalized_report(self):
        self.add("history")
        from startup_harness import attach_first_divergence_diagnostic, finalize_probe_report

        report = {"mode": "probe", "run_id": "failed", "binding_id": "bind-1"}
        gates = [{"id": "setup", "label": "setup", "predecessors": [], "expectations": [
            {"kind": "identity", "predicate": {"site_id": "camp", "operation_id": "scout", "generation": "7", "actor_ids": ["a1"], "owner": "world"}}
        ]}]
        evidence = {"status": "red", "gates": [{"id": "setup", "status": "red", "event_range": {}}]}
        attach_first_divergence_diagnostic(
            report, proof_gates=gates, gate_evidence=evidence, registry_path=self.registry_file.name,
            capsule_binding=self.binding,
        )
        self.assertEqual(report["first_divergence_diagnostic"]["status"], "red")
        self.assertEqual(report["first_divergence_diagnostic"]["capsule_recommendation"]["capsule"]["candidate_id"], "history")
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)
            finalize_probe_report(path, report)
            before = (path / "probe.report.json").read_bytes()
            select_diagnostic_capsule_candidate(self.db, failed_run=self.expected)
            self.assertEqual((path / "probe.report.json").read_bytes(), before)

    def test_production_probe_seam_forwards_all_selection_inputs(self):
        source = Path(__file__).with_name("startup_harness.py").read_text(encoding="utf-8")
        call = source[source.index("attach_first_divergence_diagnostic(", source.index('if isinstance(scenario.get("proof_gates")')):]
        call = call[:call.index("        )")]
        for required in (
            "gate_evidence=structured_gate_evidence", "events=structured_events",
            "saved_artifacts=saved_artifacts_for_gates",
            "repeated_noncommitted_summary=summarize_noncommitted_transition_events(structured_events)",
            "registry_path=diagnostic_registry_path(registry_launch_receipt, certification_registry)", "capsule_binding=",
        ):
            self.assertIn(required, call)

    def test_non_cert_registry_launch_receipt_supplies_registry_owner(self):
        from startup_harness import diagnostic_registry_path

        receipt = json.dumps({"registry_path": self.registry_file.name, "token_id": "launch-token"})
        self.assertEqual(diagnostic_registry_path(receipt, ""), self.registry_file.name)
        self.assertEqual(diagnostic_registry_path(receipt, "/certification.sqlite3"), "/certification.sqlite3")
        self.assertEqual(diagnostic_registry_path(json.dumps({"registry_path": "/report-supplied.sqlite3"}), ""), "/report-supplied.sqlite3")
        self.assertEqual(diagnostic_registry_path("not-json", ""), "")


if __name__ == "__main__":
    unittest.main()
