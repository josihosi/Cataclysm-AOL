"""Quicksave checkpoints must describe the confirmed disk turn, not just UI success."""

import json
import unittest

from gameplay_display import display, plain_player_output


def save_response(status, saved_turn, current_turn, *, accepted=True, state="collected", previous=None):
    observation = {
        "run_id": "save-test-run",
        "observation_id": f"save-test-run:frame:{current_turn}:{status}",
        "game_turn": current_turn,
        "game_minutes": current_turn // 60,
        "surface": {"kind": "world", "facts": {
            "last_save_result": status,
            "last_save_checkpoint": json.dumps({"confirmed_turn": saved_turn}),
        }, "actions": []},
    }
    response = {"ok": accepted,
                "observation": observation,
                "receipt": {"native_receipt": {
                    "action_id": "world.quicksave", "accepted": accepted,
                    "rejection_reason": "stale_frame" if not accepted else ""}}}
    view, snapshot = display(response, previous)
    text = plain_player_output({"state": state, "response": view},
                               snapshot=snapshot, full_look=False)
    return text, snapshot


class SaveResultDisplayTest(unittest.TestCase):
    def test_repeated_save_reports_no_write_at_the_same_turn(self):
        first, previous = save_response("saved", 100, 100)
        second, _ = save_response("not_needed", 100, 100, previous=previous)
        self.assertIn("Quicksave: saved turn 100.", first)
        self.assertIn("Quicksave: skipped", second)
        self.assertIn("Confirmed saved turn 100; current turn 100.", second)
        self.assertIn("Current state not confirmed on disk.", second)

    def test_elapsed_wait_noop_does_not_claim_current_turn_saved(self):
        _, previous = save_response("saved", 100, 100)
        text, _ = save_response("not_needed", 100, 460, previous=previous)
        self.assertIn("Confirmed saved turn 100; current turn 460.", text)
        self.assertNotIn("saved turn 460", text)

    def test_completed_write_advances_confirmed_turn(self):
        text, _ = save_response("saved", 461, 461)
        self.assertIn("Quicksave: saved turn 461.", text)

    def test_stale_snapshot_cannot_claim_a_write(self):
        text, _ = save_response("saved", 100, 100, accepted=False, state="rejected")
        self.assertNotIn("Quicksave:", text)
        self.assertNotIn("saved turn 100", text)

    def test_failed_write_reports_uncertain_disk_state(self):
        text, _ = save_response("failed_maps", None, 460)
        self.assertIn("Quicksave: failed (maps).", text)
        self.assertIn("Disk state and saved turn uncertain.", text)
        self.assertNotIn("saved turn 460", text)

    def test_old_native_result_does_not_infer_turn_from_observation(self):
        text, _ = save_response("saved", None, 460)
        self.assertIn("write complete; saved turn unknown", text)
        self.assertNotIn("saved turn 460", text)


if __name__ == "__main__":
    unittest.main()
