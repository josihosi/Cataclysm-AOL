"""The ordinary play response must expose decisions rather than receipts."""
import json
import unittest

from gameplay_display import player_output


class PlayerOutputTest(unittest.TestCase):
    def test_gameplay_and_real_alarms_survive_without_receipts(self):
        full = {"ok": True, "state": "collected", "request_id": "play-1",
                "receipt": {"response_sha256": "a" * 64, "binding_id": "b" * 64},
                "turn_assessment": {"status": "unassessed", "machine": "host",
                                    "alarms": [{"kind": "stalled_progress"}]},
                "response": {"current_input": {"owner": "activity_wait",
                    "actions": [{"id": "activity.pause", "enabled": True}]},
                    "facts_changed": {"messages": ["You finish waiting."]},
                    "authority": {"run_id": "c" * 64},
                    "outcome": {"native_receipt": {"request_id": "internal",
                                "accepted": True}}}}
        shown = player_output(full)
        text = json.dumps(shown)
        self.assertNotIn("sha256", text)
        self.assertNotIn("binding_id", text)
        self.assertNotIn("authority", text)
        self.assertNotIn("native_receipt", text)
        self.assertIn("You finish waiting.", text)
        self.assertEqual(shown["performance"]["alarms"][0]["kind"], "stalled_progress")
        self.assertEqual(shown["current_input"]["actions"][0]["id"], "activity.pause")
        self.assertTrue(shown["outcome"]["accepted"])
        self.assertIn("receipt", full)

    def test_pending_and_failure_keep_next_action(self):
        for status in ({"ok": True, "state": "pending", "next": "collect"},
                       {"ok": False, "error": "stale_observation", "next": "look"}):
            self.assertEqual(player_output({**status, "turn_assessment": {"alarms": []}}), status)

    def test_wait_operation_does_not_reintroduce_nested_receipts(self):
        shown = player_output({"ok": True, "response": {"outcome": {"operation": {
            "kind": "wait", "state": "accepted", "run_id": "a" * 64,
            "binding_id": "b" * 64, "requested_duration_game_minutes": 360,
            "accepted_receipt": {"accepted": True, "requested_run_id": "a" * 64,
                                 "requested_frame_id": "frame:1"}}}}})
        self.assertEqual(shown["outcome"]["operation"], {
            "kind": "wait", "state": "accepted", "requested_duration_game_minutes": 360,
            "accepted": True})


if __name__ == "__main__":
    unittest.main()
