"""Real registry validators ingest exact archived reports and cited witnesses."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import scenario_registry_store as registry
import scenario_registry_ingestion_test as ingestion_test
from playtest_witness_registry_test import CHARTER
from playtest_witness import build_evidence_journal, validate_witness_statement, compose_evidence_journals
from cockpit_archive import Archive, json_chunks, file_identity
from cockpit_report_reference import load_stored_journal, load_report
from startup_harness import write_json


class RegistryArchiveTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.helper = ingestion_test.ScenarioRegistryIngestionTest()
        self.connection, _, self.manifest, self.path = self.helper.setup_registry(self.root)
        self.addCleanup(self.connection.close)
        self.archive = Archive(self.root / "cockpit-evidence.sqlite", run_id="run-a", binding_id="binding-a")
        self.addCleanup(self.archive.close)
        self.adapters = self.helper.adapters({"runtime": "compatible", "fixture": "compatible", "profile": "compatible"})

    def report(self, count=3):
        report = self.helper.report(self.manifest)
        transcript = self.archive.sequence()
        for index in range(count):
            transcript.append({"kind": "observation", "value": {
                "observation_id": f"run-a:{index}", "run_id": "run-a", "game_minutes": 101,
                "visible_entities": [], "advertised_actions": [],
                "delta": {"game_minutes": {"before": 100, "after": 101}},
                "compact_log": {"receipt_count": 1, "contradictory_evidence": []}}})
        journal = build_evidence_journal(charter=CHARTER,
            identity={"scenario_id": report["scenario"], "source_identity": hashlib.sha256(self.manifest.read_bytes()).hexdigest(),
                      "executable_identity": "runtime-observed-hash", "run_id": "run-a", "binding_id": "binding-a"},
            transcript=transcript, terminal={"stop_reason": "target_reached"}, evidence_ceiling="focused")
        statement = {"verdict": "proved", "smallest_supported_claim": "One native minute advanced.",
            "causal_account": "The bound frame carries a 100 to 101 delta.",
            "citations": [{"citation_id": "J0002", "meaning": "native delta", "checks": {"value.delta.game_minutes.after": 101}}],
            "recommended_disposition": "accept", "evidence_ceiling": "focused"}
        validation = validate_witness_statement(charter=CHARTER, journal=journal, statement=statement)
        report["steps"] = [{"cockpit_live_session": {"final": {"run_id": "run-a", "binding_id": "binding-a",
            "action_observation_sequence": transcript,
            "stop_detail": {"evidence_journal": journal, "witness_validation": validation}}}}]
        return report, journal

    def test_ingestion_runs_real_witness_validators_without_reading_full_report_bytes(self):
        report, journal = self.report()
        write_json(self.path, report)
        expected_sha = file_identity(self.path)["sha256"]
        # Fail if a large-report consumer silently falls back to the old eager reader.
        with patch.object(registry, "_report_path_and_bytes", side_effect=AssertionError("eager report read")):
            result = registry.ingest_report_reference(self.connection, self.path, adapters=self.adapters)
        self.assertEqual(result["status"], "ingested", result)
        stored = self.connection.execute("SELECT report_sha256 FROM report_ingestion_history WHERE report_id=?", (result["report_id"],)).fetchone()
        self.assertEqual(stored[0], expected_sha)
        row = self.connection.execute("SELECT journal_json FROM playtest_witness_history").fetchone()
        self.assertIsNotNone(row)
        reference = json.loads(row[0])
        self.assertEqual(reference["schema"], "caol-playtest-journal-reference-v1")
        restored = load_stored_journal(reference)
        self.assertEqual("".join(json_chunks(restored)), "".join(json_chunks(journal)))
        again = registry.ingest_report_reference(self.connection, self.path, adapters=self.adapters)
        self.assertTrue(again["idempotent"])

    def test_modified_scalar_sidecar_is_rejected_before_any_witness_credit(self):
        report, _ = self.report()
        write_json(self.path, report)
        sidecar = self.path.with_suffix(".ref.json")
        wire = json.loads(sidecar.read_text(encoding="utf-8"))
        wire["feature_proof"] = not wire["feature_proof"]
        sidecar.write_text(json.dumps(wire), encoding="utf-8")
        result = registry.ingest_report_reference(self.connection, self.path, adapters=self.adapters)
        self.assertEqual(result["status"], "invalid_report", result)
        self.assertIn("reconstruction", result["error"])
        self.assertEqual(self.connection.execute("SELECT COUNT(*) FROM playtest_witness_history").fetchone()[0], 0)

    def test_two_readonly_report_archives_compose_persist_and_retrieve_exactly(self):
        journals, reports = [], []
        for number in (1, 2):
            directory = self.root / f"report-{number}"
            directory.mkdir()
            run, binding = f"run-{number}", f"binding-{number}"
            archive = Archive(directory / "cockpit-evidence.sqlite", run_id=run, binding_id=binding)
            self.addCleanup(archive.close)
            transcript = archive.sequence()
            receipts = archive.sequence()
            receipts.append({"native_receipt": {"run_id": run, "binding_id": binding, "accepted": True}})
            transcript.append({"kind": "native_macro", "receipts": receipts})
            report = self.helper.report(self.manifest)
            identity = {"scenario_id": report["scenario"],
                "source_identity": hashlib.sha256(self.manifest.read_bytes()).hexdigest(),
                "executable_identity": "runtime-observed-hash", "run_id": run, "binding_id": binding}
            journal = build_evidence_journal(charter=CHARTER, identity=identity, transcript=transcript,
                terminal={"stop_reason": "settled"}, evidence_ceiling="focused")
            statement = {"verdict": "inconclusive", "smallest_supported_claim": "A bound receipt was retained.",
                "causal_account": "The test does not establish gameplay behavior.",
                "citations": [{"citation_id": "J0001", "meaning": "source run", "checks": {"run_id": run}}],
                "recommended_disposition": "continue", "evidence_ceiling": "focused"}
            validation = validate_witness_statement(charter=CHARTER, journal=journal, statement=statement)
            report["steps"] = [{"cockpit_live_session": {"final": {"run_id": run, "binding_id": binding,
                "action_observation_sequence": transcript,
                "stop_detail": {"evidence_journal": journal, "witness_validation": validation}}}}]
            path = directory / "report.json"
            write_json(path, report)
            result = registry.ingest_report_reference(self.connection, path, adapters=self.adapters)
            self.assertEqual(result["status"], "ingested", result)
            reports.append(result["report_id"])
            _, _, loaded = load_report(path)
            loaded_journal = loaded["steps"][0]["cockpit_live_session"]["final"]["stop_detail"]["evidence_journal"]
            self.assertTrue(loaded_journal["entries"].archive.readonly)
            journals.append(loaded_journal)
        inline = json.loads("".join(json_chunks(journals)))
        expected = compose_evidence_journals(charter=CHARTER, journals=inline)
        combined = compose_evidence_journals(charter=CHARTER, journals=journals)
        self.assertEqual(combined["journal_sha256"], expected["journal_sha256"])
        self.assertEqual(json.loads("".join(json_chunks(combined))), expected)
        statement = {"verdict": "inconclusive", "smallest_supported_claim": "Both independent run identities were retained.",
            "causal_account": "This is a storage comparison, not gameplay proof.",
            "citations": [{"citation_id": f"R{i}:J0001", "meaning": "independent source", "checks": {"binding_id": f"binding-{i}"}} for i in (1, 2)],
            "recommended_disposition": "continue", "evidence_ceiling": "focused"}
        # Also exercise a valid mixed/legacy-first set whose outer entries are
        # inline but whose exact nested receipts still belong to both sources.
        mixed = json.loads(json.dumps(expected))
        for index, source in enumerate(journals):
            mixed["entries"][index * 3 + 1]["value"]["receipts"] = source["entries"][1]["value"]["receipts"]
        self.assertNotEqual(mixed["entries"][1]["value"]["receipts"].archive.path,
                            mixed["entries"][4]["value"]["receipts"].archive.path)
        recorded = registry.record_playtest_witness(self.connection,
            manifest_id=self.connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0],
            report_ids=reports, charter=CHARTER, journal=mixed, statement=statement)
        row = self.connection.execute("SELECT journal_json FROM playtest_witness_history WHERE witness_id=?", (recorded["witness_id"],)).fetchone()
        restored = load_stored_journal(json.loads(row[0]))
        self.assertEqual(restored["journal_sha256"], expected["journal_sha256"])
        self.assertEqual(json.loads("".join(json_chunks(restored))), expected)
        self.assertEqual(restored["entries"][4]["value"]["receipts"][0]["native_receipt"]["binding_id"], "binding-2")


if __name__ == "__main__":
    unittest.main()
