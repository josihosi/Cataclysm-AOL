"""Terminal scenario waits must settle native interruptions before applying turns."""
from pathlib import Path
import unittest
from unittest.mock import patch

import startup_harness as harness


class TerminalWaitInterruptionTest(unittest.TestCase):
    def run_wait(self, *, accepted=True, unknown=False, stale=False):
        world = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "wait-test",
            "frame_id": "world-1", "surface_id": "world", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "game_minutes": 100,
            "valid_actions": [{"id": "wait.30m", "stable_id": "", "label": "30 minutes", "enabled": True}],
        }
        prompt = {**world, "frame_id": "prompt-2", "surface_id": "distraction", "kind": "prompt",
                  "breadcrumbs": ["Activity distraction"],
                  "payload": {"title": "OTHER" if unknown else "CANCEL_ACTIVITY_OR_IGNORE_QUERY"},
                  "valid_actions": [{"id": "prompt.choose", "stable_id": "ignore-1", "label": "IGNORE", "enabled": True}]}
        demand = {**world, "frame_id": "demand-3", "surface_id": "demand", "kind": "menu",
                  "valid_actions": [{"id": "shakedown.pay", "stable_id": "", "label": "Pay", "enabled": True}]}
        current = [world]
        calls = []
        def dispatch(**kwargs):
            calls.append(kwargs["action_id"])
            if stale and len(calls) == 2:
                current[0] = {**prompt, "frame_id": "concrete-prompt-3", "surface_id": "concrete"}
                return {"accepted": False, "native_receipt": {
                    "accepted": False, "rejection_reason": "wrong_surface",
                    "run_id": "wait-test", "requested_run_id": "wait-test",
                    "requested_frame_id": prompt["frame_id"],
                    "requested_surface_id": prompt["surface_id"],
                    "action_id": kwargs["action_id"]}}
            if stale and len(calls) == 3:
                self.assertEqual(kwargs["frame_id"], "concrete-prompt-3")
            current[0] = prompt if len(calls) == 1 else demand
            return {"accepted": True if len(calls) == 1 else accepted}
        with patch.object(harness, "current_semantic_step_frame", side_effect=lambda **kw: current[0]), \
                patch.object(harness, "execute_semantic_act", side_effect=dispatch), \
                patch.object(harness, "advance_turns") as applying, \
                patch.object(harness, "adaptive_semantic_session_identity", return_value=("session", "test")), \
                patch.object(harness.time, "monotonic", side_effect=[0, 0, 2] if unknown else [0] * 5), \
                patch.object(harness.time, "sleep"):
            result = harness.execute_semantic_terminal_wait_until(
                step={"required_action_chain": ["wait.30m"], "stop_when_advertised_action": "shakedown.pay",
                      "max_windows": 1, "ordinary_applying_turn_after_duration": True,
                      "observation_timeout_seconds": 1, "observation_interval_seconds": 0.1},
                profile="test", run_dir=Path("/tmp/wait-test"), run_id="wait-test", trace_start_offset=0, pid=1,
            )
            applying.assert_not_called()
        return result, calls

    def test_damage_prompt_is_ignored_and_wait_reaches_demand(self):
        result, calls = self.run_wait()
        self.assertEqual(result["status"], "stop_action_advertised")
        self.assertEqual(calls, ["wait.30m", "prompt.choose"])

    def test_rejected_ignore_is_not_replayed(self):
        result, calls = self.run_wait(accepted=False)
        self.assertEqual(result["status"], "blocked_semantic_duration_receipt_rejected")
        self.assertEqual(calls, ["wait.30m", "prompt.choose"])

    def test_authenticated_stale_ignore_refreshes_concrete_prompt(self):
        result, calls = self.run_wait(stale=True)
        self.assertEqual(result["status"], "stop_action_advertised")
        self.assertEqual(calls, ["wait.30m", "prompt.choose", "prompt.choose"])
        self.assertFalse(result["receipts"][1]["accepted"])

    def test_unknown_prompt_does_not_receive_applying_turn(self):
        result, calls = self.run_wait(unknown=True)
        self.assertEqual(result["status"], "blocked_semantic_duration_incomplete")
        self.assertEqual(calls, ["wait.30m"])


if __name__ == "__main__":
    unittest.main()
