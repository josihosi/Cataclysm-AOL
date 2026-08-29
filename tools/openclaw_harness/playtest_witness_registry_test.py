#!/usr/bin/env python3
"""Registry persistence tests for generic playtest witness history."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from playtest_witness import build_evidence_journal, compose_evidence_journals  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    _playtest_witness_hard_proven_candidates,
    ScenarioRegistryStoreError,
    open_registry,
    record_playtest_witness,
    review_playtest_witness,
)


CHARTER = {
    "claim": "A native wait advanced one minute.",
    "material_proof": "Accepted wait receipt and native time delta.",
    "material_contradiction": ["rejected receipt"],
    "already_accepted_evidence": [],
    "current_uncertainty": "whether time advanced",
    "forbidden_shortcuts": ["setup promotion"],
    "honest_stop_conditions": ["proof", "contradiction"],
    "requested_evidence_ceiling": "focused",
}


class PlaytestWitnessRegistryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.connection = open_registry(str(Path(self.temp.name) / "registry.sqlite3"))
        declaration = {
            "name": "r018.raw_wait_acceptance_mcw",
            "runtime_contract": {"permitted_input": ["cockpit:run.witness"]},
        }
        self.connection.execute(
            "INSERT INTO manifest_current( manifest_id, source_path, present, revision, current_sha256, "
            "last_content_sha256, declaration_json, normalized_json, validation_json ) "
            "VALUES( 'manifest-a', '/scenario.json', 1, 1, 'source-a', 'source-a', ?, ?, ? )",
            (json.dumps(declaration), json.dumps(declaration), json.dumps({"status": "valid"})),
        )
        report_runs = {
            "raw-report": "raw-run", "primitive-report": "primitive-run",
            "master-off-report": "master-off-run", "gadget-off-report": "gadget-off-run",
        }
        for report_id, run_id in report_runs.items():
            report_path = Path(self.temp.name) / (report_id + ".json")
            report_bytes = json.dumps({
                "scenario": "r018.raw_wait_acceptance_mcw",
                "steps": [{"cockpit_live_session": {"final": {
                    "run_id": run_id, "binding_id": "binding-a",
                }}}],
            }, sort_keys=True).encode("utf-8")
            report_path.write_bytes(report_bytes)
            import hashlib
            report_sha = hashlib.sha256(report_bytes).hexdigest()
            self.connection.execute(
                "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, "
                "report_kind, ingestion_status ) VALUES( ?, 'manifest-a', ?, ?, 'probe', 'ingested' )",
                (report_id, str(report_path), report_sha),
            )
            self.connection.execute(
                "INSERT INTO verification_history( verification_id, manifest_id, report_id, "
                "route_key, binding_fingerprint, outcome_kind, proof_status, details_json ) "
                "VALUES( ?, 'manifest-a', ?, 'r018-route', 'binding-a', 'focused', 'yellow', ? )",
                ("verification-" + report_id, report_id, json.dumps({
                    "manifest": {"source_sha256": "source-a"},
                    "runtime": {"runtime_binding_observed": {"executable_sha256": "exe-a"}},
                    "proof": {"status": "yellow"},
                })),
            )
        self.connection.commit()

    def tearDown(self) -> None:
        self.connection.close()
        self.temp.cleanup()

    def witness_inputs(
        self, *, run_id: str = "raw-run", scenario_id: str = "r018.raw_wait_acceptance_mcw",
        source_identity: str = "source-a", executable_identity: str = "exe-a",
    ) -> tuple[dict[str, object], dict[str, object]]:
        journal = build_evidence_journal(
            charter=CHARTER,
            identity={
                "scenario_id": scenario_id, "source_identity": source_identity,
                "executable_identity": executable_identity, "run_id": run_id,
                "binding_id": "binding-a",
            },
            transcript=[{"kind": "observation", "value": {
                "observation_id": run_id + ":1", "run_id": run_id, "game_minutes": 101,
                "visible_entities": [], "advertised_actions": [],
                "delta": {"game_minutes": {"before": 100, "after": 101}},
                "compact_log": {"receipt_count": 1, "contradictory_evidence": []},
            }}],
            terminal={"stop_reason": "target_reached"},
            evidence_ceiling="focused",
        )
        statement = {
            "verdict": "proved",
            "smallest_supported_claim": "The cited run advanced one minute.",
            "causal_account": "The bound native delta is 100 to 101.",
            "citations": [{
                "citation_id": "J0002", "meaning": "native minute delta",
                "checks": {"value.delta.game_minutes.before": 100,
                           "value.delta.game_minutes.after": 101},
            }],
            "recommended_disposition": "accept",
            "evidence_ceiling": "focused",
        }
        return journal, statement

    def test_witness_and_coordinator_review_are_separate_append_only_facts(self) -> None:
        report_ids = [
            "raw-report", "primitive-report", "master-off-report", "gadget-off-report",
        ]
        candidates = {"verification-" + report_id: {} for report_id in report_ids}
        self.assertEqual(_playtest_witness_hard_proven_candidates(
            self.connection, manifest_id="manifest-a", route_key="r018-route",
            candidate_rows=candidates, report_local_hard_proven=set(),
        ), set())
        journals = [
            self.witness_inputs(run_id=run_id)[0]
            for run_id in ("raw-run", "primitive-run", "master-off-run", "gadget-off-run")
        ]
        journal = compose_evidence_journals(charter=CHARTER, journals=journals)
        statement = {
            "verdict": "proved",
            "smallest_supported_claim": "The four bound R-018 roles contain the cited native delta.",
            "causal_account": "Each independently identified role cites its terminal minute delta.",
            "citations": [
                {"citation_id": f"R{index}:J0002", "meaning": "bound native minute delta",
                 "checks": {"value.delta.game_minutes.after": 101}}
                for index in range(1, 5)
            ],
            "recommended_disposition": "accept", "evidence_ceiling": "focused",
        }
        witness = record_playtest_witness(
            self.connection, manifest_id="manifest-a",
            report_ids=report_ids,
            charter=CHARTER, journal=journal, statement=statement,
        )
        self.assertEqual(witness["status"], "mechanically_valid")
        self.assertEqual(self.connection.execute(
            "SELECT COUNT(*) FROM playtest_witness_review_history"
        ).fetchone()[0], 0)
        reviewed = review_playtest_witness(
            self.connection, witness_id=witness["witness_id"], decision="accept",
            rationale="The cited native delta settles the bounded claim.",
        )
        self.assertEqual(reviewed["decision"], "accept")
        self.assertEqual(_playtest_witness_hard_proven_candidates(
            self.connection, manifest_id="manifest-a", route_key="r018-route",
            candidate_rows=candidates, report_local_hard_proven=set(),
        ), set(candidates))
        self.assertEqual(self.connection.execute(
            "SELECT COUNT(*) FROM playtest_witness_report_history"
        ).fetchone()[0], 4)

    def test_wrong_report_owner_and_unexplained_rejection_fail(self) -> None:
        journal, statement = self.witness_inputs()
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "binding"):
            record_playtest_witness(
                self.connection, manifest_id="manifest-a", report_ids=["missing"],
                charter=CHARTER, journal=journal, statement=statement,
            )
        wrong_identity, wrong_statement = self.witness_inputs(executable_identity="wrong-executable")
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "source/executable/run/ownership"):
            record_playtest_witness(
                self.connection, manifest_id="manifest-a", report_ids=["raw-report"],
                charter=CHARTER, journal=wrong_identity, statement=wrong_statement,
            )
        witness = record_playtest_witness(
            self.connection, manifest_id="manifest-a", report_ids=["raw-report"],
            charter=CHARTER, journal=journal, statement=statement,
        )
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "concrete_causal_risk"):
            review_playtest_witness(
                self.connection, witness_id=witness["witness_id"], decision="repair",
                rationale="The format is unfamiliar.",
            )

    def test_witness_persistence_is_not_scenario_specific(self) -> None:
        declaration = {"name": "generic.live_playtest"}
        self.connection.execute(
            "INSERT INTO manifest_current( manifest_id, source_path, present, revision, current_sha256, "
            "last_content_sha256, declaration_json, normalized_json, validation_json ) "
            "VALUES( 'manifest-b', '/generic.json', 1, 1, 'source-b', 'source-b', ?, ?, ? )",
            (json.dumps(declaration), json.dumps(declaration), json.dumps({"status": "valid"})),
        )
        report_path = Path(self.temp.name) / "generic-report.json"
        report_bytes = json.dumps({
            "scenario": "generic.live_playtest",
            "steps": [{"cockpit_live_session": {"final": {
                "run_id": "generic-run", "binding_id": "binding-a",
            }}}],
        }, sort_keys=True).encode("utf-8")
        report_path.write_bytes(report_bytes)
        import hashlib
        report_sha = hashlib.sha256(report_bytes).hexdigest()
        self.connection.execute(
            "INSERT INTO report_ingestion_history( report_id, manifest_id, report_path, report_sha256, "
            "report_kind, ingestion_status ) VALUES( 'generic-report', 'manifest-b', ?, ?, "
            "'probe', 'ingested' )", (str(report_path), report_sha),
        )
        self.connection.execute(
            "INSERT INTO verification_history( verification_id, manifest_id, report_id, route_key, "
            "binding_fingerprint, outcome_kind, proof_status, details_json ) VALUES( "
            "'generic-verification', 'manifest-b', 'generic-report', 'generic-route', 'binding-a', "
            "'focused', 'yellow', ? )",
            (json.dumps({
                "manifest": {"source_sha256": "source-b"},
                "runtime": {"runtime_binding_observed": {"executable_sha256": "exe-b"}},
            }),),
        )
        self.connection.commit()
        journal, statement = self.witness_inputs(
            run_id="generic-run", scenario_id="generic.live_playtest",
            source_identity="source-b", executable_identity="exe-b",
        )
        witness = record_playtest_witness(
            self.connection, manifest_id="manifest-b", report_ids=["generic-report"],
            charter=CHARTER, journal=journal, statement=statement,
        )
        reviewed = review_playtest_witness(
            self.connection, witness_id=witness["witness_id"], decision="accept",
            rationale="The cited bound delta settles this generic live claim.",
        )
        self.assertEqual(reviewed["decision"], "accept")


if __name__ == "__main__":
    unittest.main()
