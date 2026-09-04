import json
import tempfile
import unittest
from pathlib import Path

from certification_route import (
    capture_and_finalize_certification,
    continuous_capture_proof_classification,
)


class CertificationFinalizationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        kinds = ("declared_world", "departure", "shared_route_advance", "bubble_crossing_out", "actor_outcomes", "bubble_crossing_in", "return_report", "camp_decision", "save", "quit", "relaunch", "normalized_persistence")
        common = {"token_id": "tok", "scenario_digest": "scenario", "round_id": "round", "binding_id": "bind", "world_id": "world", "player_id": "player", "actor_ids": ["a", "b"]}
        self.events = [dict(common, sequence=i, kind=kind, owner="abstract") for i, kind in enumerate(kinds, 1)]
        self.receipts = [dict(common, sequence=1, owner="abstract", next_owner="local")]
        (self.root / "events.json").write_text(json.dumps(self.events), encoding="utf-8")
        (self.root / "receipts.json").write_text(json.dumps(self.receipts), encoding="utf-8")

    def tearDown(self): self.temp.cleanup()

    def manifest(self):
        return {"captured_artifacts": {"lifecycle_stream": str(self.root / "events.json"), "crossing_receipts": str(self.root / "receipts.json")}}

    def kwargs(self):
        return dict(capture_manifest=self.manifest(), report_path=self.root / "report.json", expected_token_id="tok", expected_scenario_digest="scenario", expected_round_id="round", expected_binding_id="bind", expected_world_id="world", expected_player_id="player", expected_actor_ids=["a", "b"])

    def test_complete_capture_finalizes_once(self):
        result = capture_and_finalize_certification(**self.kwargs())
        self.assertEqual(result["status"], "green")
        self.assertEqual(result["events"], self.events)
        self.assertEqual(capture_and_finalize_certification(**self.kwargs())["status"], "green")

    def test_only_green_structured_and_capture_receipts_promote_classification(self):
        finalization = capture_and_finalize_certification(**self.kwargs())
        promoted = continuous_capture_proof_classification(
            base_classification={"status": "yellow", "feature_proof": False},
            structured_gate_evidence={"status": "green"},
            finalization=finalization,
        )
        self.assertEqual(
            (promoted["status"], promoted["evidence_class"], promoted["feature_proof"]),
            ("green", "automated continuous-round certification", True),
        )
        preserved = continuous_capture_proof_classification(
            base_classification={"status": "yellow", "feature_proof": False},
            structured_gate_evidence={"status": "yellow"},
            finalization=finalization,
        )
        self.assertEqual(preserved, {"status": "yellow", "feature_proof": False})

    def test_incomplete_or_mutated_capture_rejects(self):
        events = list(self.events); events[7] = dict(events[7], sequence=9)
        (self.root / "events.json").write_text(json.dumps(events), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "sequence"):
            capture_and_finalize_certification(**self.kwargs())
        inline = self.manifest(); inline["events"] = self.events
        with self.assertRaisesRegex(ValueError, "inline"):
            capture_and_finalize_certification(**dict(self.kwargs(), capture_manifest=inline))

    def test_identity_mismatch_rejects(self):
        bad = list(self.receipts); bad[0] = dict(bad[0], actor_ids=["replacement"])
        (self.root / "receipts.json").write_text(json.dumps(bad), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "receipt identity"):
            capture_and_finalize_certification(**self.kwargs())

    def test_scheduler_diagnostic_cannot_satisfy_any_certification_gate(self):
        diagnostic = {
            "kind": "bandit_live_world diagnostic: scheduler_prefilter",
            "reason": "active_outside_pressure",
            "actor_ids": ["a", "b"],
            "round_id": "round",
            "binding_id": "bind",
            "world_id": "world",
            "player_id": "player",
        }
        events = [diagnostic]
        (self.root / "events.json").write_text(json.dumps(events), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "lifecycle identity mismatch"):
            capture_and_finalize_certification(**self.kwargs())

    def test_scheduler_exit_inventory_has_one_diagnostic_reason_per_exit(self):
        source = (Path(__file__).parents[2] / "src" / "bandit_live_world.cpp").read_text(
            encoding="utf-8"
        )
        reasons = (
            "dispatch_cooldown",
            "dispatch_cap_zero",
            "active_outside_pressure",
            "camp_decision_denied",
            "routine_policy_ineligible",
            "scout_pair_unavailable",
            "no_structural_candidate_source",
            "drive_below_threshold_without_plan",
            "drive_below_threshold",
            "invalid_time",
            "scheduler_replay_suppressed",
            "no_eligible_routine_camps",
            "full_route_solve_cap_reached",
            "planned_lead_missing",
            "full_route_candidate_ineligible",
            "no_score_eligible_full_route",
        )
        for reason in reasons:
            self.assertIn(f'"{reason}"', source)
        self.assertGreaterEqual(source.count("scheduler_exit_diagnostic("), len(reasons))
        self.assertIn("proof_eligible=no", source)


if __name__ == "__main__": unittest.main()
