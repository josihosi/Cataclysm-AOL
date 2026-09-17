#!/usr/bin/env python3
"""Focused startup and immutable replacement handoff checks."""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent
SCENARIO = ROOT / "scenarios/zombie_rider.live_light_memory_continuity_mcw.json"
PREDATOR_SCENARIO = ROOT / "scenarios/horde.predator_lifecycle_abstract_to_physical_mcw.json"

sys.path.insert(0, str(ROOT))
import startup_harness  # noqa: E402
from scenario_registry import validate_manifest  # noqa: E402


class RzlRiderLightRelaunchTest(unittest.TestCase):
    def test_continuity_gate_requires_declared_rider_memory_comparison(self):
        rider = json.loads(SCENARIO.read_text(encoding="utf-8"))
        predator = json.loads(PREDATOR_SCENARIO.read_text(encoding="utf-8"))
        self.assertTrue(startup_harness.scenario_declares_rider_light_memory_relaunch_continuity(
            rider, rider["post_relaunch"]
        ))
        self.assertFalse(startup_harness.scenario_declares_rider_light_memory_relaunch_continuity(
            predator, predator["post_relaunch"]
        ))

    def test_predator_lifecycle_is_a_gameplay_proof_route(self):
        predator = json.loads(PREDATOR_SCENARIO.read_text(encoding="utf-8"))
        self.assertTrue(predator["runtime_contract"]["grants_gameplay_proof"])
        self.assertTrue(predator["semantic_only_startup"])
        self.assertFalse(startup_harness.startup_gui_automation_required(
            "play_now_default", semantic_only=predator["semantic_only_startup"]
        ))
        self.assertEqual(validate_manifest(predator, path=PREDATOR_SCENARIO)["validation"]["status"], "valid")

    def test_post_relaunch_expiry_uses_receipt_backed_semantic_wait(self):
        scenario = json.loads(SCENARIO.read_text(encoding="utf-8"))
        step = next(s for s in scenario["post_relaunch"]["steps"]
                    if s.get("label") == "advance_relaunched_rider_memory_past_expiry")
        self.assertEqual(step["kind"], "adaptive_semantic_window")
        self.assertEqual(step["required_action_chain"],
                         ["world.wait", "wait.duration_menu", "wait.5m"])
        self.assertEqual(step["minimum_elapsed_minutes"], 5)
        self.assertEqual(step["adaptive_interrupt_actions"], ["activity.ignore"])
        self.assertNotIn("count", step)

    def test_isolated_startup_uses_semantic_owner_without_gui_focus(self):
        scenario = json.loads(SCENARIO.read_text(encoding="utf-8"))
        self.assertTrue(scenario["semantic_only_startup"])
        self.assertFalse(startup_harness.startup_gui_automation_required(
            "play_now_default", semantic_only=scenario["semantic_only_startup"]
        ))
        shared = startup_harness.build_game_command(
            Path("/work/cataclysm-tiles"), "dev-harness", "McWilliams"
        )
        isolated = startup_harness.build_game_command(
            Path("/work/cataclysm-tiles"), "rider-light-isolated", "McWilliams"
        )
        self.assertEqual(shared[0], isolated[0])
        self.assertEqual(shared[1::2], isolated[1::2])
        self.assertEqual(isolated[-1], "McWilliams")

    def test_relaunch_binds_exact_saved_snapshot_and_skips_reseed(self):
        with tempfile.TemporaryDirectory(prefix="rzl_relaunch_") as temp:
            root = Path(temp)
            profile_save = root / "profile" / "save" / "McWilliams"
            profile_save.mkdir(parents=True)
            (profile_save / "dimension_data.gsav").write_text("# version\n{}\n", encoding="utf-8")
            artifact = root / "runs" / "run"
            artifact.mkdir(parents=True)
            with mock.patch.object(startup_harness, "save_dir_for_profile", return_value=root / "profile" / "save"), \
                 mock.patch.object(startup_harness, "semantic_step_source_trace", return_value=Path("/tmp/trace")), \
                 mock.patch.object(startup_harness, "native_save_quit_receipt", return_value={"status": "matched"}), \
                 mock.patch.object(startup_harness, "observe_bound_process_exit", return_value={"status": "native_exit", "elapsed_seconds": 0.1}), \
                 mock.patch.object(startup_harness, "run_json_command", return_value=(0, {
                     "ok": True, "pid": 22, "run_dir": str(artifact),
                     "focus": {"ok": True},
                     "proof_classification": {"startup_clean_for_feature_steps": True},
                     "saved_world_snapshot": {"installed_sha256": startup_harness.sha256_tree(profile_save)[0]},
                 }, "", "")), \
                 mock.patch.object(startup_harness, "persisted_run_process_generation", return_value={"birth_identity": "replacement"}):
                result = startup_harness.run_probe_post_relaunch(
                    initial_pid=11,
                    initial_process_command="cataclysm-tiles --userdir .userdata/rider-light-isolated/ --world McWilliams",
                    profile="rider-light-isolated", config_profile="dev-harness", world="McWilliams",
                    scenario_name="zombie_rider.live_light_memory_continuity_mcw",
                    registry_launch_receipt="", terminal_exit_timeout_seconds=2.0,
                    artifact_run_dir=artifact, transition_event_run_id="run-1",
                    transition_event_path=str(artifact / "transition.events.jsonl"),
                )
            command = result["start_command"]
            self.assertIn("--saved-world-snapshot", command)
            self.assertNotIn("--fixture", command)
            self.assertNotIn("--profile-snapshot", command)
            snapshot = json.loads((artifact / "replacement_saved_world.snapshot.json").read_text())
            self.assertEqual(snapshot["sha256"], result["replacement_saved_world_snapshot"]["sha256"])
            self.assertEqual(snapshot["sha256"], result["replacement_saved_world_snapshot"]["installed_sha256"])


if __name__ == "__main__":
    unittest.main()
