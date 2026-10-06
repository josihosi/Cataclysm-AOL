"""Production startup admission controls for recovered native diagnostics."""

import unittest
import json
import os
import sys
import tempfile
import threading
import time
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


class StartupControllerLifetimeTest(unittest.TestCase):
    """Only harmless producers; no game, terminal input or registry launch."""

    def await_frame(self, **extra):
        return harness.await_r014_native_semantic_bootstrap(
            profile="fixture", run_dir=Path("/fixture"), run_id="run-a",
            required_state="world", required_actions=["world.wait"],
            timeout_seconds=0, poll_seconds=0, **extra)

    def readers(self, frames):
        from contextlib import ExitStack
        stack = ExitStack()
        stack.enter_context(patch.object(harness, "semantic_step_source_trace", return_value=Path("/fixture/native")))
        stack.enter_context(patch.object(harness, "semantic_step_effective_source_offset", return_value=0))
        stack.enter_context(patch.object(harness, "current_semantic_step_frame", side_effect=frames))
        stack.enter_context(patch.object(harness, "r014_native_semantic_bootstrap_metadata",
                                        side_effect=lambda **kw: kw["frame"]))
        self.addCleanup(stack.close)

    def test_live_warning_waits_past_timeout_for_actual_frame_without_input(self):
        self.readers([ValueError("warning modal owns startup"),
                      {"status": "required_state_present", "frame_id": "run-a:frame:1"}])
        with patch.object(harness, "execute_semantic_act") as dispatch:
            result = self.await_frame(live_owner_alive=lambda: True)
        self.assertEqual(result["status"], "required_state_present")
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(result["frame_id"], "run-a:frame:1")
        dispatch.assert_not_called()

    def test_nonlive_timeout_is_unchanged_and_does_not_admit_warning(self):
        self.readers([ValueError("warning modal owns startup")])
        result = self.await_frame()
        self.assertEqual(result["reason"], "first_same_run_semantic_frame_timeout")
        self.assertEqual(result["attempts"], 1)
        self.assertNotEqual(result["status"], "required_state_present")

    def test_dead_live_owner_does_not_extend_timeout_or_invent_world(self):
        self.readers([ValueError("no current frame")])
        result = self.await_frame(live_owner_alive=lambda: False)
        self.assertEqual(result["reason"], "first_same_run_semantic_frame_timeout")
        self.assertEqual(result["attempts"], 1)

    def test_existing_frame_is_accepted_without_extending_wait(self):
        self.readers([{"status": "required_state_present", "frame_id": "run-a:frame:1"}])
        alive = unittest.mock.Mock(return_value=True)
        result = self.await_frame(live_owner_alive=alive)
        self.assertEqual(result["attempts"], 1)
        alive.assert_not_called()

    def test_probe_inprocess_start_keeps_live_context_without_second_launch(self):
        def start(args):
            print(json.dumps({"ok": True, "live": args.cockpit_live_session}))
            return 0
        command = [sys.executable, str(Path(harness.__file__)), "start"]
        with patch.object(harness, "run_startup", side_effect=start) as caller:
            live = harness.run_startup_in_process(command, live_session=True)
            ordinary = harness.run_startup_in_process(command)
        self.assertEqual(caller.call_count, 2)
        self.assertTrue(live[1]["live"])
        self.assertFalse(ordinary[1]["live"])

    def test_real_bridge_retains_fixture_controller_then_serves_same_child(self):
        from cockpit_file_bridge import FileBackedCockpitBridge
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            release = base / "release-fixture-frame"
            waited = base / "awaiting-fixture-frame"
            # Only a fixture frame producer is delayed. Creating release is
            # not native input and this child cannot launch a game.
            child = '''import json,os,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import startup_harness as h
release,waited=Path(sys.argv[2]),Path(sys.argv[3])
h.semantic_step_source_trace=lambda *a: Path('/fixture/native')
h.semantic_step_effective_source_offset=lambda *a: 0
def frame(**kw):
    waited.write_text('waiting')
    if not release.exists(): raise ValueError('fixture warning')
    return {'status':'required_state_present','frame_id':'run-a:frame:1'}
h.current_semantic_step_frame=frame
h.r014_native_semantic_bootstrap_metadata=lambda **kw:kw['frame']
r=h.await_r014_native_semantic_bootstrap(profile='fixture',run_dir=Path('/fixture'),run_id='run-a',required_state='world',required_actions=['world.wait'],timeout_seconds=.01,poll_seconds=.01,live_owner_alive=lambda:True)
assert r['status']=='required_state_present'
print(json.dumps({'cockpit_live_session':{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}}),flush=True)
for line in sys.stdin: print(json.dumps({'ok':True,'result':json.loads(line)}),flush=True)
'''
            session = base / "fixture-session"
            bridge = FileBackedCockpitBridge(session,
                [sys.executable, "-u", "-c", child, str(Path(harness.__file__).parent), str(release), str(waited)],
                binding_id="fixture-bound", require_session_ready=True)
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            try:
                def until(predicate):
                    deadline = time.monotonic() + 5
                    while time.monotonic() < deadline:
                        if predicate(): return
                        time.sleep(.01)
                    self.fail("harmless fixture did not reach expected boundary")
                until(waited.exists)
                time.sleep(.08)  # Exceeds the fixture startup timeout.
                self.assertIsNone(bridge._child.poll())
                self.assertEqual(json.loads((session / "status.json").read_text())["state"], "starting")
                original_pid = bridge._child.pid
                release.write_text("fixture producer now publishes a frame")
                until(lambda: json.loads((session / "status.json").read_text())["state"] == "ready")
                self.assertEqual(bridge._child.pid, original_pid)
                self.assertTrue(bridge.send_request(session, request_id="fixture-observe",
                    binding_id="fixture-bound", request={"action": "game.observe"})["ok"])
                until(lambda: (session / "responses/fixture-observe.receipt.json").exists())
                self.assertEqual(bridge.response_slice(session, "fixture-observe", "result.action")["slice"], "game.observe")
            finally:
                bridge.cleanup(session, "fixture-bound")
                thread.join(4)
                self.assertFalse(thread.is_alive())
                self.assertIsNotNone(bridge._child.poll())


if __name__ == "__main__":
    unittest.main()
