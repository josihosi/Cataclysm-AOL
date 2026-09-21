"""The ordinary play response must expose decisions rather than receipts."""
import json
import unittest

from gameplay_display import player_output, bounded_player_output, plain_player_output, world_look


class PlayerOutputTest(unittest.TestCase):
    def world_snapshot(self):
        return {"owner": "world", "current": {"facts": {
            "avatar": {"name": "Test survivor", "absolute_ms": [10, 20, 0]},
            "avatar_status": {"weapon": {"name": "six-shooter", "ammo": "(<color_red>0/6</color>)"},
                "health": {"body_parts": {"arm_l": {"name": "left arm", "current": 50, "maximum": 80},
                                          "arm_r": {"name": "right arm", "current": 80, "maximum": 80}}}},
            "avatar_effects": {"entries": {"wet": {"arm_l": {"name": "Damp left arm"}},
                                           "bleed": {"arm_l": {"name": "Bleeding left arm"}}}},
            "visible_local": [{"dx": 0, "dy": 0, "terrain": "floor", "fields": ["fd_fire"]}],
            "visible_entities": []}, "actions": [
                {"id": "world.move.north", "enabled": True},
                {"id": "world.wait", "enabled": True},
                {"id": "world.reload", "enabled": True},
                {"id": "world.inspect_npc", "stable_id": "character:2", "label": "Inspect Ada", "enabled": True},
                {"id": "world.unfamiliar_action", "label": "Unfamiliar", "enabled": True},
                {"id": "world.fire", "enabled": False}]}}

    def test_sectioned_look_keeps_game_state_and_all_enabled_controls(self):
        text = world_look(self.world_snapshot())
        for expected in ("YOU", "SURROUNDINGS", "MOVE", "ACTIONS", "PEOPLE", "WAIT", "SESSION",
                         "left arm 50/80", "right arm 80/80", "six-shooter (0/6)", "fd_fire", "Bleeding left arm",
                         "north", "Combat: reload", "character:2", "play act world.unfamiliar_action",
                         "play act world.wait", "--bound-maximum 5"):
            self.assertIn(expected, text)
        for unwanted in ("Damp", "<color", "Combat: fire", "play wait 20s"):
            self.assertNotIn(unwanted, text)

    def test_sectioned_look_refreshes_weapon_creatures_and_position(self):
        snapshot = self.world_snapshot()
        before = world_look(snapshot)
        facts = snapshot["current"]["facts"]
        facts["avatar_status"]["weapon"] = {"name": "fists"}
        facts["avatar"]["absolute_ms"] = [11, 20, 0]
        facts["visible_entities"] = [{"name": "zombie", "attitude": "hostile", "dx": 2, "dy": -1,
                                     "identity": {"id": "monster:1"}}]
        after = world_look(snapshot)
        self.assertIn("six-shooter", before)
        self.assertNotIn("six-shooter", after)
        self.assertIn("Weapon: fists", after)
        self.assertIn("zombie · hostile · 2 east, 1 north", after)
        self.assertIn("Position: 11, 20, 0", after)
        self.assertNotIn("No creatures", after)

    def test_sectioned_look_does_not_replace_pending_errors_or_prompts(self):
        snapshot = self.world_snapshot()
        cases = [{"ok": True, "state": "pending", "next": "collect"},
                 {"ok": False, "error": "stale_observation"},
                 {"ok": True, "state": "collected", "response": {"current_input": {
                     "owner": "prompt", "facts_changed": {"text": "Stop waiting?"}}}}]
        for result in cases:
            self.assertEqual(plain_player_output(result, snapshot=snapshot), plain_player_output(result))

    def test_sectioned_look_preserves_alarm(self):
        result = {"ok": True, "state": "collected", "response": {"current_input": {"owner": "world"}},
                  "turn_assessment": {"alarms": [{"kind": "waiting_slow",
                      "message": "Tell the coordinator: waiting performance needs attention."}]}}
        text = plain_player_output(result, snapshot=self.world_snapshot())
        self.assertIn("YOU", text)
        self.assertIn("Tell the coordinator", text)

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

    def test_large_game_output_keeps_existing_budget_and_retrieval_identity(self):
        from evidence_display import DEFAULT_BYTES
        full = {"ok": True, "request_id": "play-large", "next": "look",
                "response": {"current_input": {"owner": "world"},
                             "facts_changed": {"large": "x" * 100000}}}
        shown = bounded_player_output(full)
        text = json.dumps(shown, separators=(",", ":"))
        self.assertLessEqual(len(text.encode()), DEFAULT_BYTES)
        self.assertEqual(shown["request_id"], "play-large")
        self.assertEqual(shown["current_input"]["owner"], "world")
        self.assertNotIn("sha256", text)
        self.assertTrue(shown["facts_changed"]["large"]["omitted"])
        self.assertGreater(shown["facts_changed"]["large"]["evidence"]["json_bytes"], 100000)


if __name__ == "__main__":
    unittest.main()
