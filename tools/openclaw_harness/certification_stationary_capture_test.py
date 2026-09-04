import json
import tempfile
import unittest
from pathlib import Path

import startup_harness as harness
from certification_route import capture_and_finalize_certification


class StationaryCertificationCaptureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.common = {
            "token_id": "token",
            "scenario_digest": "scenario",
            "round_id": "round",
            "binding_id": "binding",
            "world_id": "world",
            "player_id": "player",
            "actor_ids": ["4", "5"],
            "captured_artifacts": {
                "lifecycle_stream": str(self.root / "certification.lifecycle.json"),
                "crossing_receipts": str(self.root / "certification.crossing_receipts.json"),
            },
        }
        self.events = [
            {"sequence": 1, "domain": "bandit_live_world", "transition": "active_sortie_dispatch", "outcome": "committed", "simulation_owner": "abstract", "previous_state": "at_home", "new_state": "outbound", "actor_ids": [4, 5]},
            {"sequence": 2, "domain": "bandit_live_world", "transition": "local_pair_handoff", "outcome": "committed", "simulation_owner": "local", "previous_state": "abstract", "new_state": "local", "actor_ids": [4, 5]},
            {"sequence": 3, "domain": "bandit_live_world", "transition": "structural_member_physical_return", "outcome": "committed", "simulation_owner": "local", "new_state": "at_home", "actor_ids": [4]},
            {"sequence": 4, "domain": "bandit_live_world", "transition": "structural_member_physical_return", "outcome": "committed", "simulation_owner": "abstract", "new_state": "at_home", "actor_ids": [5]},
            {"sequence": 5, "domain": "bandit_live_world", "transition": "camp_decision", "outcome": "committed", "simulation_owner": "abstract", "previous_state": "idle", "new_state": "report_awaiting_assessment"},
            {"sequence": 6, "domain": "certification", "transition": "save_receipt", "outcome": "committed", "certification_save_receipt": {"round_id": "round"}},
        ]
        self.relaunch = {
            "status": "ready", "original_process_exited": True, "initial_pid": 1,
            "pid": 2, "terminal_save_quit_receipt": {"status": "matched"},
        }
        self.steps = [{"label": "audit_normalized_persistence_after_relaunch", "metadata": {"status": "required_state_present"}}]

    def tearDown(self):
        self.temp.cleanup()

    def test_capture_is_derived_from_stationary_live_receipts(self):
        result = harness.write_stationary_certification_capture(
            self.root, capture=self.common, events=self.events,
            relaunch=self.relaunch, steps=self.steps,
        )
        self.assertEqual(result["status"], "green")
        final = capture_and_finalize_certification(
            capture_manifest=self.common, report_path=self.root / "report.json",
            expected_token_id="token", expected_scenario_digest="scenario",
            expected_round_id="round", expected_binding_id="binding",
            expected_world_id="world", expected_player_id="player",
            expected_actor_ids=["4", "5"],
        )
        self.assertEqual(final["status"], "green")
        lifecycle = json.loads((self.root / "certification.lifecycle.json").read_text())
        self.assertEqual(lifecycle[2]["kind"], "shared_route_advance")
        self.assertEqual(lifecycle[-1]["kind"], "normalized_persistence")

    def test_missing_normalized_persistence_does_not_write_capture(self):
        result = harness.write_stationary_certification_capture(
            self.root, capture=self.common, events=self.events,
            relaunch=self.relaunch, steps=[],
        )
        self.assertEqual(result["reason"], "certification_capture_normalized_persistence_missing")
        self.assertFalse((self.root / "certification.lifecycle.json").exists())


if __name__ == "__main__":
    unittest.main()
