"""Production startup admission controls for recovered native diagnostics."""

import unittest
from pathlib import Path
from unittest.mock import patch

import startup_harness as harness


class StartupRecoveryTest(unittest.TestCase):
    run_id = "run-recovery"
    pid = 4242

    def world(self):
        return {
            "status": "required_state_present",
            "run_id": self.run_id,
            "frame_run_id": self.run_id,
            "frame_id": f"{self.run_id}:frame:1",
            "frame_event": "surface_descriptor",
            "frame_state": "world",
            "advertised_actions": ["world.wait", "world.look"],
        }

    def summary(self, lines=()):
        return {
            "capture_status": "not_requested_native_semantic",
            "native_semantic_startup": self.world(),
            "startup_screen_probe": {
                "classification": "not_requested_native_semantic",
                "visible_error_popup": False,
                "startup_error_logged": bool(lines),
                "debug_error_lines": list(lines),
                "debug_delta_artifact": "/run/debug.final.log",
            },
        }

    def classify(self, summary=None, **kwargs):
        return harness.startup_proof_classification(
            ok=True,
            screen_summary=self.summary() if summary is None else summary,
            native_semantic_startup_ready=True,
            expected_run_id=self.run_id,
            expected_pid=self.pid,
            **kwargs,
        )

    def test_mixed_and_unfamiliar_messages_preserve_every_line_and_allow_world(self):
        lines = [
            "10:24 ERROR : (error message will follow backtrace)",
            "(continued from above) ERROR : item_location owner with character_id 2 missing",
            "10:25 ERROR : item_location lost target",
            "10:26 ERROR : unfamiliar subsystem warns about an unrelated cache",
        ]
        proof = self.classify(self.summary(lines), debug_errors_recorded=1,
                              debug_popups_recorded=2)
        self.assertEqual(proof["status"], "yellow")
        self.assertTrue(proof["startup_ready_for_actions"])
        self.assertFalse(proof["startup_clean_for_feature_steps"])
        self.assertFalse(proof["feature_proof"])
        self.assertEqual(proof["diagnostic_evidence"]["lines"], lines)
        self.assertEqual(proof["diagnostic_evidence"]["artifact_path"], "/run/debug.final.log")
        self.assertEqual(harness.startup_result_status(
            base_ok=True, proof_classification=proof), (True, ""))

    def test_clean_world_retains_clean_load_result(self):
        proof = self.classify()
        self.assertEqual(proof["status"], "green")
        self.assertTrue(proof["startup_ready_for_actions"])
        self.assertTrue(proof["startup_clean_for_feature_steps"])
        self.assertEqual(harness.startup_result_status(
            base_ok=True, proof_classification=proof), (True, ""))

    def test_pid_bound_visible_hud_can_recover_a_logged_diagnostic(self):
        summary = {
            "capture_success": True, "capture_process_pid": self.pid,
            "version_matches_runtime_paths": True,
            "startup_screen_probe": {
                "gameplay_hud_present": True, "visible_error_popup": False,
                "startup_error_logged": True,
                "debug_error_lines": ["ERROR : recoverable diagnostic"],
            },
        }
        proof = harness.startup_proof_classification(
            ok=True, screen_summary=summary, focus_result={"ok": True},
            expected_pid=self.pid,
        )
        self.assertTrue(proof["startup_ready_for_actions"])
        self.assertFalse(proof["startup_clean_for_feature_steps"])

    def test_missing_diagnostic_lines_never_earn_clean_credit(self):
        proof = self.classify(debug_errors_recorded=1)
        self.assertTrue(proof["startup_ready_for_actions"])
        self.assertEqual(proof["diagnostic_evidence"]["status"], "count_without_lines")
        self.assertFalse(proof["startup_clean_for_feature_steps"])

    def test_uncleared_or_recurring_modal_blocks_admission(self):
        summary = self.summary(["ERROR : modal remains"])
        summary["startup_screen_probe"]["visible_error_popup"] = True
        proof = self.classify(summary)
        self.assertEqual(proof["feature_gate"], "visible_error_popup")
        self.assertFalse(proof["startup_ready_for_actions"])
        self.assertFalse(harness.startup_result_status(
            base_ok=True, proof_classification=proof)[0])

    def test_wrong_run_or_process_identity_blocks_admission(self):
        wrong_run = self.summary(["ERROR : recoverable"])
        wrong_run["native_semantic_startup"]["frame_run_id"] = "prior-run"
        proof = self.classify(wrong_run)
        self.assertEqual(proof["feature_gate"], "native_run_identity_mismatch")
        self.assertFalse(proof["startup_ready_for_actions"])

        proof = self.classify(
            self.summary(["ERROR : recoverable"]),
            focus_result={"transport": "terminal_native", "ok": True, "pid": self.pid + 1},
        )
        self.assertEqual(proof["feature_gate"], "native_process_identity_mismatch")
        self.assertFalse(proof["startup_ready_for_actions"])

        gui = {
            "capture_success": True, "capture_process_pid": self.pid + 1,
            "version_matches_runtime_paths": True,
            "startup_screen_probe": {
                "gameplay_hud_present": True, "startup_error_logged": True,
            },
        }
        proof = harness.startup_proof_classification(
            ok=True, screen_summary=gui, focus_result={"ok": True},
            expected_pid=self.pid,
        )
        self.assertEqual(proof["feature_gate"], "captured_process_identity_mismatch")
        self.assertFalse(proof["startup_ready_for_actions"])

    def test_native_prompt_cannot_stand_in_for_recovered_world(self):
        prompt = self.summary(["ERROR : recoverable"])
        prompt["native_semantic_startup"].update({
            "frame_state": "prompt", "advertised_actions": ["prompt.choose"],
        })
        proof = self.classify(prompt)
        self.assertEqual(proof["feature_gate"], "native_world_after_diagnostics_unproven")
        self.assertFalse(proof["startup_ready_for_actions"])

    def test_stale_world_and_process_exit_still_fail(self):
        frame = {
            "event": "surface_descriptor", "run_id": self.run_id,
            "frame_id": f"{self.run_id}:frame:1", "kind": "world",
            "valid_actions": [{"id": "world.wait", "enabled": True}],
            "_event_offset": 10,
        }
        metadata = harness.r014_native_semantic_bootstrap_metadata(
            profile="unused", run_dir=Path("/tmp"), run_id=self.run_id,
            start_offset=0, press_trace_offset=11, required_state="world",
            required_actions=["world.wait"], frame=frame,
        )
        self.assertEqual(metadata["reason"], "stale_frame")
        summary = self.summary(["ERROR : recoverable"])
        summary["native_semantic_startup"] = metadata
        proof = harness.startup_proof_classification(
            ok=True, screen_summary=summary,
            native_semantic_startup_ready=metadata["status"] == "required_state_present",
            expected_run_id=self.run_id, expected_pid=self.pid,
        )
        self.assertFalse(proof["startup_ready_for_actions"])
        exited = harness.startup_proof_classification(
            ok=False, failure_reason="process_exited_after_readiness",
            screen_summary=self.summary(["ERROR : recoverable"]),
            native_semantic_startup_ready=True,
        )
        self.assertEqual(exited["status"], "red")
        self.assertFalse(exited["startup_ready_for_actions"])

    def test_recovered_start_runs_unrelated_steps_but_failed_action_and_integrity_gate_fail(self):
        proof = self.classify(self.summary(["ERROR : recovered item reference"]))
        common = dict(
            verdict="artifacts_matched", startup_classification=proof,
            step_reports=[{"label": "native_action"}],
            artifact_patterns=["observed outcome"],
            matches_by_pattern=[{"pattern": "observed outcome", "lines": ["observed outcome"]}],
        )
        accepted = harness.probe_proof_classification(
            **common, step_ledger_summary={"status": "green_step_local_proof"},
        )
        self.assertTrue(accepted["feature_proof"])
        self.assertFalse(accepted["startup_clean_for_feature_steps"])
        failed_action = harness.probe_proof_classification(
            **common, step_ledger_summary={"status": "red_step_local_proof_failed"},
        )
        self.assertFalse(failed_action["feature_proof"])
        self.assertEqual(failed_action["status"], "red")
        integrity_gate = harness.probe_proof_classification(
            **common, step_ledger_summary={"status": "green_step_local_proof"},
            structured_gate_evidence={"status": "red", "reason": "claim_integrity_unresolved"},
        )
        self.assertFalse(integrity_gate["feature_proof"])
        self.assertEqual(integrity_gate["status"], "red")

        unready = dict(proof, startup_ready_for_actions=False, status="yellow")
        structured_only = harness.probe_proof_classification(
            **{**common, "verdict": "structured_gates_matched",
               "startup_classification": unready, "matches_by_pattern": []},
            step_ledger_summary={"status": "green_step_local_proof"},
            structured_gate_evidence={"status": "green"},
        )
        self.assertFalse(structured_only["feature_proof"])
        self.assertEqual(structured_only["verdict"],
                         "startup_not_ready_feature_proof_inconclusive")

    @patch("startup_harness.run_json_command")
    @patch("startup_harness.wait_for_pid_exit", return_value=True)
    def test_recovered_relaunch_is_usable_without_clean_reload_credit(self, _exit, command):
        proof = self.classify(self.summary(["ERROR : recovered diagnostic"]))
        command.return_value = (0, {
            "ok": True, "pid": 18, "proof_classification": proof,
            "focus": {"ok": True},
        }, "", "")
        result = harness.run_probe_post_relaunch(
            initial_pid=17, initial_process_command="cataclysm-tiles --world TestSetup00",
            profile="test-profile", config_profile="test-profile",
            world="TestSetup00", scenario_name="test-scenario",
            registry_launch_receipt="", terminal_exit_timeout_seconds=1.0,
        )
        self.assertEqual(result["status"], "ready")
        self.assertFalse(result["clean_load_proof"])


if __name__ == "__main__":
    unittest.main()
