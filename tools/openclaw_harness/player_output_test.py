"""The ordinary play response must expose decisions rather than receipts."""
import json
import unittest

from gameplay_display import player_output, bounded_player_output, plain_player_output, world_look


class PlayerOutputTest(unittest.TestCase):
    def test_successful_native_receipt_with_empty_reason_is_not_a_rejection(self):
        text = plain_player_output({"ok": True, "response": {"outcome": {
            "native_receipt": {"accepted": True, "rejection_reason": ""}}}})
        self.assertNotIn("Rejected", text)

    def test_native_rejection_cause_survives_outer_and_nested_receipts(self):
        for reason in ("stale_frame", "target_missing", "unknown_native_reason"):
            for nested in (False, True):
                receipt = {"accepted": False, "rejection_reason": reason, "frame_id": "secret"}
                result = {"ok": False, "error": "native_action_rejected",
                          "receipt": {"native_receipt": receipt} if nested else receipt}
                text = plain_player_output(result)
                self.assertIn("Rejected: " + reason.replace("_", " "), text)
                self.assertNotIn("secret", text)
                self.assertNotIn("native_action_rejected", text)
                self.assertEqual("Next: play look" in text, reason == "stale_frame")

    def test_inventory_controls_explain_marking_without_inventing_targets(self):
        snapshot = {"owner": "inventory", "current": {"facts": {
            "title": "Multidrop", "selected_items": '{"42":{"count":2,"unit":"items"}}'},
            "actions": [{"id": "inventory.toggle", "stable_id": "42", "label": "Arrow"},
                        {"id": "inventory.select", "stable_id": "42", "label": "Arrow"},
                        {"id": "inventory.commit", "label": "Drop selected items"},
                        {"id": "inventory.cancel", "label": "Cancel"}]}}
        text = plain_player_output({"state": "collected", "response": {
            "current_input": {"owner": "inventory"}}}, snapshot=snapshot)
        self.assertIn("ALLOWED ACTIONS IN THIS VIEW", text)
        self.assertIn("select confirms; it does not mark", text)
        self.assertIn("Marked: 42 ×2", text)
        self.assertIn("Drop selected items → play act inventory.commit", text)
        self.assertNotIn("item:42", text)

    def test_shared_menu_actions_keep_every_choice_and_disabled_reason(self):
        from gameplay_display import _plain_controls
        actions = [{"id": "menu." + verb, "stable_id": f"option:{i}", "label": f"Choice {i}"}
                   for i in range(171) for verb in ("select", "choose")]
        text = _plain_controls(actions)
        self.assertEqual(text.count("select/choose"), 1)
        for i in range(171):
            self.assertIn(f"option:{i} — Choice {i}", text)
        actions[-1].update(enabled=False, label="Choice 170 — unavailable in this mode")
        text = _plain_controls(actions)
        self.assertIn("Unavailable: choose: Choice 170 — unavailable in this mode", text)
        self.assertIn("option:170 — Choice 170 — select", text)

    def test_comparison_does_not_advertise_missing_commit(self):
        from gameplay_display import _plain_controls
        text = _plain_controls([{"id": "inventory.toggle", "stable_id": "42",
                                 "label": "Toggle comparison"}])
        self.assertIn("Toggle comparison", text)
        self.assertNotIn("commit", text)

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

    def test_menu_reply_exposes_current_choices_without_an_extra_look(self):
        snapshot = {"owner": "inventory", "current": {"facts": {"title": "Pickup"}, "actions": [
            {"id": "inventory.select", "stable_id": "42", "label": "six-shooter — No room", "enabled": False},
            {"id": "inventory.wield", "stable_id": "42", "label": "Wield", "enabled": True},
            {"id": "inventory.filter", "label": "Filter", "enabled": True},
            {"id": "inventory.cancel", "label": "Cancel", "enabled": True}]}}
        result = {"ok": True, "state": "collected", "response": {"current_input": {
            "owner": "inventory", "actions": {"omitted": True, "evidence": {"sha256": "a" * 64}},
            "facts_removed": ["avatar", "minimap"]}}}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        for expected in ("Pickup", "42", "six-shooter", "wield", "No room",
                         "play act inventory.<action> --target <target>",
                         "play act inventory.filter --param text=TEXT", "play act inventory.cancel"):
            self.assertIn(expected, text)
        for unwanted in ("sha256", "omitted", "facts removed", "minimap"):
            self.assertNotIn(unwanted, text)

    def test_world_action_keeps_changed_state_messages_and_failure_without_maps(self):
        snapshot = self.world_snapshot()
        result = {"ok": False, "state": "rejected", "response": {
            "current_input": {"owner": "world"}, "outcome": {"error": "blocked_by_door"},
            "facts_changed": {"avatar": {"absolute_ms": {"omitted": True}},
                              "avatar_status": {"observed_turn": 12},
                              "messages": [{"time": "12:00", "text": "You open the door."}],
                              "minimap": {"cells": "unused"}, "structural_signal_dispatch": "unused"}}}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        self.assertIn("You open the door.", text)
        self.assertIn("blocked_by_door", text)
        self.assertIn("avatar.absolute ms", text)
        for unwanted in ("minimap", "structural", "omitted", "HP", "observed turn", "sha256"):
            self.assertNotIn(unwanted, text)

    def test_menu_close_does_not_repeat_world_catalog(self):
        result = {"ok": True, "state": "collected", "response": {"current_input": {"owner": "world"}}}
        self.assertEqual(plain_player_output(result, snapshot=self.world_snapshot(), full_look=False),
                         "World. Controls: play look")

    def test_changed_ammo_keeps_weapon_name_and_injury_keeps_maximum(self):
        snapshot = self.world_snapshot()
        result = {"ok": True, "state": "collected", "response": {
            "current_input": {"owner": "world"}, "facts_changed": {"avatar_status": {
                "weapon": {"ammo": "(0/6)"}, "health": {"body_parts": {"arm_l": {"current": 50}}}}}}}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        self.assertIn("six-shooter (0/6)", text)
        self.assertIn("left arm 50/80", text)
        self.assertNotIn("None", text)

    def test_nested_json_rules_are_text_and_unknown_menu_actions_survive(self):
        snapshot = {"owner": "npc_inspection", "current": {"facts": {
            "actor_name": "Alex", "diagnostic_rules": {"rules": '[{"label":"Will use guns","enabled":true}]'}},
            "actions": [{"id": "npc_inspection.future_action", "label": "New choice", "enabled": True}]}}
        result = {"ok": True, "state": "collected", "response": {"current_input": {"owner": "npc_inspection"}}}
        text = plain_player_output(result, snapshot=snapshot)
        self.assertIn("Rules: Will use guns", text)
        self.assertIn("play act npc_inspection.future_action", text)
        self.assertNotIn('{"', text)

    def test_partial_movement_and_wait_keep_their_units(self):
        movement = plain_player_output({"ok": False, "error": "blocked", "response": {"outcome": {
            "chain": {"offset_ms": [3, -2], "partial_progress": 2, "planned_steps": 5,
                      "terminal_absolute_ms": [10, 20, 0]}}}})
        self.assertIn("Moved 2/5 steps.", movement)
        self.assertIn("Position: 10, 20, 0", movement)
        self.assertIn("blocked", movement)
        self.assertNotIn("Waited", movement)
        waiting = plain_player_output({"ok": True, "response": {"outcome": {
            "chain": {"partial_progress": 1.0, "stop_reason": "target_reached"}}}})
        self.assertIn("Waited 1 game minutes.", waiting)

    def test_snapshot_debug_facts_do_not_restore_receipt_ids(self):
        snapshot = self.world_snapshot()
        snapshot["current"]["facts"]["last_debug_intervention"] = {
            "run_id": "a" * 64, "operation": "native_debug_kill", "name": "zombie",
            "before": {"hp": 80}, "after": {"hp": 0, "dead": True}}
        result = {"ok": True, "state": "collected", "response": {
            "current_input": {"owner": "world"}, "facts_changed": {
                "last_debug_intervention": {"omitted": True}}}}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        self.assertIn("zombie", text)
        self.assertIn("after.dead: yes", text)
        self.assertNotIn("a" * 64, text)
        self.assertNotIn("run id", text)

    def test_rejected_input_explains_recovery_without_replaying_stale_controls(self):
        for error, explanation in (
                ("action_not_advertised", "current menu"),
                ("stable_id_not_advertised", "this exact action"),
                ("stale_observation", "moved on"),
                ("look_required: no current unconsumed observation", "fresh observation")):
            with self.subTest(error=error):
                result = {"ok": False, "state": "rejected", "error": error}
                text = plain_player_output(result, snapshot=self.world_snapshot())
                self.assertIn(explanation, text)
                self.assertEqual(text.count("Next: play look"), 1)
                self.assertNotIn("YOU", text)
                self.assertNotIn("play act", text)
                result["next"] = "look"
                self.assertEqual(plain_player_output(result).count("Next: play look"), 1)

    def test_process_exit_has_one_status_no_unknown_clock_and_retains_failure(self):
        result = {"ok": True, "state": "process_exited", "response": {
            "current_input": {"owner": "process_exited", "facts_changed": {
                "state": "process_exited", "exit_code": 1,
                "save_outcome": "not_established_by_process_exit"}},
            "state": "process_exited", "game_minutes": {"before": 10, "after": None}}}
        text = plain_player_output(result)
        self.assertEqual(text.count("Game exited."), 1)
        self.assertNotIn("Game time", text)
        self.assertIn("exit code: 1", text)
        self.assertIn("not_established_by_process_exit", text)

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
