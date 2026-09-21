#!/usr/bin/env python3
"""Contract tests for the R-033 changed-premise sound age/reentry route."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
SCENARIO = HARNESS_DIR / "scenarios" / "r033.phase4_semantic_autonomous_sound_return_drive500_age_reentry_rev1_mcw.json"
CHARTER = HARNESS_DIR / "charters" / "r033-drive500-sound-return-age-reentry-rev1.json"


class R033Drive500AgeReentryScenarioTest(unittest.TestCase):
    def setUp(self) -> None:
        self.scenario = json.loads(SCENARIO.read_text(encoding="utf-8"))
        self.charter = json.loads(CHARTER.read_text(encoding="utf-8"))

    def test_uses_only_the_existing_zero_credit_drive500_fixture(self) -> None:
        self.assertEqual(self.scenario["fixture"], "r033_phase4_autonomous_sound_return_drive500_v1")
        self.assertEqual(self.scenario["capabilities"]["capabilities.r033.production_dispatch_premise"],
                         "supply_empty_drive_ge_500")
        contract = self.scenario["runtime_contract"]
        self.assertFalse(contract["grants_gameplay_proof"])
        self.assertIn("direct-state-mutation", contract["forbidden_input"])

    def test_requires_a_real_initial_production_falsification(self) -> None:
        self.assertEqual(
            self.scenario["capabilities"]["capabilities.r033.production_initial_state_falsification"],
            "native_scheduler_drive_ge_500_distinguishes_retained_drive358_control",
        )
        operation = next(step for step in self.scenario["steps"]
                         if step["label"] == "operate_r033_drive500_sound_return_age_reentry")
        self.assertIn("drive is at least 500", operation["objective"])
        self.assertIn("do not alter the 500 threshold", operation["invariants"])
        self.assertIn("pre-input production scheduler drive below 500", self.charter["material_contradiction"][0])

    def test_declares_native_save_reload_and_reentry_provenance_audit(self) -> None:
        labels = [step["label"] for step in self.scenario["steps"]]
        self.assertIn("save_returned_sound_age_boundary", labels)
        self.assertIn("confirm_returned_sound_age_process_exit", labels)
        self.assertEqual(self.scenario["post_relaunch"]["terminal_save_step_label"],
                         "confirm_returned_sound_age_process_exit")
        post_steps = self.scenario["post_relaunch"]["steps"]
        self.assertEqual(post_steps[0]["kind"], "cockpit_live_session")
        audit = next(step for step in post_steps if step["label"] == "audit_returned_sound_report_after_reentry")
        self.assertEqual(audit["required_lead_origin"], "returned_report")
        self.assertEqual(audit["required_lead_last_outcome"], "returned_structural_sound_report")
        self.assertIn("original observation minute", self.charter["material_proof"])


if __name__ == "__main__":
    unittest.main()
