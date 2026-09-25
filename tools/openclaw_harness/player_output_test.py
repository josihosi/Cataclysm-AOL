"""The ordinary play response must expose decisions rather than receipts."""
import json
import unittest

from gameplay_display import player_output, bounded_player_output, plain_player_output, world_look


class PlayerOutputTest(unittest.TestCase):
    def test_enabled_zone_facts_are_not_fabricated_actions(self):
        snapshot = {"owner": "zone_manager", "current": {
            "facts": {"zones": [{"id": "zone-6", "enabled": True,
                                  "name": "Basecamp: Storage"}]},
            "actions": [{"id": "zone.select", "stable_id": "zone-6",
                         "label": "Basecamp: Storage", "enabled": True}]}}
        text = plain_player_output({"state": "collected", "response": {
            "current_input": {"owner": "zone_manager", "view": "full"}}},
            snapshot=snapshot)
        self.assertNotIn("play act zone-6", text)
        self.assertIn("Basecamp: Storage", text)
        self.assertIn("play act zone.<action> --target <target>", text)
        self.assertIn("zone-6", text)
        self.assertIn("select", text)

    def test_empty_inventory_highlight_is_explicit(self):
        text = plain_player_output({"current_input": {"owner": "inventory", "view": "full",
            "facts_changed": {"highlighted_item": {"available": True, "present": False}}}})
        self.assertIn("No highlighted item.", text)

    def test_native_wait_interruption_is_a_decision_not_a_failure(self):
        text = plain_player_output({"ok": False, "error": "native_wait_interrupted"})
        self.assertIn("Wait interrupted. Choose from the current prompt.", text)
        self.assertNotIn("Error:", text)

    def test_wait_prompt_omits_receipt_noise_and_false_rejection(self):
        snapshot = {"owner": "prompt", "current": {
            "facts": {"title": "CANCEL_ACTIVITY_OR_IGNORE_QUERY", "text": "Bandit nearby. Stop waiting?"},
            "actions": [{"id": "prompt.choose", "stable_id": "prompt-option:1", "label": "YES", "enabled": True}]}}
        text = plain_player_output({"state": "rejected", "response": {
            "ok": False, "current_input": {"owner": "prompt", "view": "full"},
            "outcome": {"ok": False, "error": "native_wait_interrupted",
                "next_action": "Choose from terminal_observation actions, or observe again; the macro has stopped.",
                "chain": {"state": "interrupted", "partial_progress": 0,
                          "native_stop_reason": "native_authority_required", "native_frame_id": "internal-hash:frame:25"}}}},
            snapshot=snapshot)
        for expected in ("Bandit nearby. Stop waiting?", "YES", "Waited 0 game minutes."):
            self.assertIn(expected, text)
        for unwanted in ("ok: no", "rejected", "internal-hash", "native_authority_required", "CANCEL_ACTIVITY", "terminal_observation"):
            self.assertNotIn(unwanted, text)

    def test_whole_journal_entry_lists_fields_without_dumping_embedded_observation(self):
        text = plain_player_output({"ok": True,
            "selector": "result.evidence_journal.entries.62",
            "slice": {"citation_id": "J0063", "kind": "macro_interruption",
                      "value": {"kind": "interruption", "result": {
                          "terminal_observation": {"messages": "x" * 100000}}}}},
            inspection_request_id="journal-request")
        self.assertLess(len(text), 1000)
        self.assertIn("J0063: macro interruption", text)
        self.assertIn("INDEX 62; PATH FIELD; FIELD: kind, result", text)
        self.assertIn("entries.INDEX.value.PATH --request-id journal-request", text)

    def test_no_progress_error_directs_observation_without_claiming_a_stall(self):
        text = plain_player_output({"ok": False, "error": "proved_no_progress"})
        self.assertIn("No game progress was recorded", text)
        self.assertIn("Check for an interruption → play look", text)
        self.assertNotIn("proved_no_progress", text)

    def test_collected_activity_directs_fresh_observation_not_receipt_polling(self):
        snapshot = {"owner": "activity_wait", "current": {
            "facts": {"activity_type": "wait"},
            "actions": [{"id": "activity.pause", "enabled": True}]}}
        collected = {"state": "collected", "response": {
            "current_input": {"owner": "activity_wait", "view": "full"}}}
        text = plain_player_output(collected, snapshot=snapshot)
        self.assertIn("Check progress → play look", text)
        self.assertIn("collect repeats this recorded result", text)
        pending = plain_player_output({"state": "pending"}, snapshot=snapshot)
        self.assertIn("Pending → play collect", pending)
        self.assertNotIn("collect repeats", pending)

    def test_journal_scalar_inspection_supplies_exact_typed_check(self):
        for value in ("true", True, "42", 42, None):
            text = plain_player_output({"ok": True,
                "selector": "result.evidence_journal.entries.7.value.value.surface.facts.actual_death",
                "slice": value})
            check = json.loads(text.removeprefix("checks: "))
            self.assertEqual(check, {"value.surface.facts.actual_death": value})
            self.assertIs(type(check["value.surface.facts.actual_death"]), type(value))

    def test_large_journal_string_does_not_bypass_existing_display_bounds(self):
        text = plain_player_output({"ok": True, "selector": "entries.7.value.value.surface.facts.text",
                                    "slice": "x" * 100000})
        self.assertLess(len(text), 8192)
        self.assertFalse(text.startswith("checks:"))
        self.assertIn("98976 characters omitted; total 100000 characters", text)

    def test_short_world_look_preserves_facts_and_controls_remain_retrievable(self):
        snapshot = {"current": {"owner": "world", "facts": {"avatar": {"name": "Ada", "absolute_ms": [1, 2, 0]}},
                               "actions": [{"id": "world.wait", "enabled": True}]}}
        short = world_look(snapshot, show_controls=False)
        self.assertIn("Ada", short)
        self.assertIn("Position: 1, 2, 0", short)
        self.assertIn("Controls unchanged", short)
        self.assertNotIn("ACTIONS", short)
        controls = world_look(snapshot, controls_only=True)
        self.assertIn("play act world.wait", controls)
        self.assertNotIn("Position:", controls)

    def test_ordinary_movement_stop_drops_nested_rejection_bookkeeping(self):
        text = plain_player_output({"ok": False, "state": "rejected", "response": {
            "result": {"ok": False, "error": "raw_move_relative_no_progress",
                       "next_action": "Choose from terminal_observation actions, or observe again; the macro has stopped."}}})
        self.assertIn("Movement stopped:", text)
        self.assertIn("Next: play look", text)
        self.assertNotIn("ok: no", text)
        self.assertNotIn("state: rejected", text)
        self.assertNotIn("terminal_observation", text)

    def test_waiting_alarm_keeps_measurement_limit_and_escalation_concisely(self):
        text = plain_player_output({"turn_assessment": {"alarms": [{"kind": "waiting_slow",
            "mean_seconds": .21997036122, "limit_seconds": .1, "sample_count": 100}]}})
        self.assertIn("220.0 ms/turn", text)
        self.assertIn("100-turn average; limit 100 ms", text)
        self.assertIn("Tell the coordinator", text)
        self.assertNotIn(".21997036122", text)

    def test_text_input_examples_keep_a_sentence_in_one_shell_argument(self):
        import shlex
        from gameplay_display import _plain_controls
        for action in ("prompt.submit", "menu.filter", "inventory.filter"):
            text = _plain_controls([{"id": action}])
            command = text.split(" → ", 1)[1].replace("TEXT", "Ada follow me")
            self.assertEqual(shlex.split(command)[-1], "text=Ada follow me")

    def test_controls_explain_only_available_shortcuts_without_technical_dump(self):
        from cockpit import player_controls
        for enabled in (False, True, None):
            text = plain_player_output({"ok": True, "result": player_controls({
                "game.wait": enabled, "game.move_relative": enabled})})
            self.assertLess(len(text), 1800)
            self.assertEqual("play wait 5m" in text, bool(enabled))
            self.assertEqual("play move --east" in text, bool(enabled))
            if enabled is None:
                self.assertIn("permission unknown", text)
            self.assertNotIn("example_request", text)
            self.assertIn("play --diagnostics controls", text)

    def test_journal_guidance_targets_fields_instead_of_entire_observation(self):
        text = plain_player_output({"ok": True, "selector": "result.evidence_journal.entries",
            "slice": [{"citation_id": "J0001", "kind": "observation",
                       "value": {"game_minutes": 10, "surface": {"kind": "world"}}}]})
        self.assertIn("INDEX 0; PATH FIELD", text)
        self.assertIn("FIELD: game_minutes, surface", text)
        self.assertNotIn("Details →", text)

    def test_nested_journal_observation_points_directly_to_facts(self):
        text = plain_player_output({"ok": True, "selector": "result.evidence_journal.entries",
            "slice": [{"citation_id": "J0002", "kind": "observation", "value": {
                "kind": "observation", "value": {"surface": {
                    "kind": "world", "facts": {"text": "Returned", "game_minutes": 42}}}}}]},
            inspection_request_id="play-journal")
        self.assertIn("INDEX 0; PATH value.surface.facts.FIELD", text)
        self.assertIn("--request-id play-journal", text)
        self.assertIn("FIELD: text, game_minutes", text)
        self.assertNotIn(".value.FIELD", text)

    def test_complete_short_evidence_has_no_hash_or_retrieval_boilerplate(self):
        text = plain_player_output({"matched": 1, "rows": [{"fields": {"event": "pickup", "item": "bandage"}}],
            "snapshot": {"sha256": "a" * 64}})
        self.assertIn("bandage", text)
        self.assertNotIn("sha256", text)
        self.assertNotIn("Full rows", text)

    def test_incomplete_evidence_keeps_request_warning_and_retrieval(self):
        text = plain_player_output({"matched": 1, "rows": [{"event": "request"}],
            "links": [{"outcome": "accepted", "status": "partial"}],
            "unavailable_sources": [{"error": "missing"}], "snapshot": {"sha256": "a" * 64}})
        self.assertIn("1 incomplete or contradictory", text)
        self.assertIn("--selector links", text)
        self.assertIn("--selector unavailable_sources", text)

    def test_large_plain_evidence_uses_existing_snapshot_and_counts_omission(self):
        text = plain_player_output({"ok": True, "status": "matched", "matched": 1,
            "rows": [{"event": "trace", "payload": {"text": "x" * 20000}}],
            "links": [], "snapshot": {"sha256": "a" * 64}})
        self.assertLessEqual(len(text.encode("utf-8")), 8192)
        self.assertRegex(text, r"\d+ rendered characters omitted")
        self.assertIn("--sha256 " + "a" * 64, text)
        self.assertIn("--selector rows", text)

    def test_evidence_budget_includes_footer_and_each_returned_event(self):
        text = plain_player_output({"status": "matched", "matched": 20,
            "rows": [{"event": f"event-{i}", "payload": {"text": "é" * 10000}} for i in range(20)],
            "snapshot": {"sha256": "a" * 64}, "next": {"offset": 20}})
        self.assertLessEqual(len(text.encode("utf-8")), 8192)
        for i in range(20):
            self.assertIn(f"event-{i}\n", text)
        self.assertNotIn("unavailable sources: 0", text)

    def test_completed_wait_is_not_repeated_on_unrelated_action(self):
        text = plain_player_output({"ok": True, "operation": {
            "state": "completed", "completed_progress_game_minutes": 1.0}, "outcome": "moved"})
        self.assertNotIn("Elapsed", text)
        self.assertNotIn("operation.state", text)

    def test_interrupted_wait_keeps_elapsed_time_without_authority_metadata(self):
        text = plain_player_output({"ok": True, "operation": {
            "state": "awaiting_decision", "blocker": "native_authority_required",
            "completed_progress_game_minutes": 10.0}})
        self.assertIn("Elapsed: 10.0 game minutes.", text)
        self.assertNotIn("awaiting_decision", text)
        self.assertNotIn("native_authority_required", text)

    def test_evidence_action_payload_is_not_mistaken_for_game_controls(self):
        text = plain_player_output({"ok": True, "status": "matched", "matched": 1,
            "rows": [{"event": "npc_plan", "payload": {"actions": [{"type": "pickup"}],
                      "owner": {"name": "Mira"}}}], "links": [], "snapshot": {"sha256": "a" * 64}})
        self.assertIn("pickup", text)
        self.assertIn("Mira", text)
        self.assertNotIn("ALLOWED HERE ONLY", text)

    def test_native_stale_rejection_prints_one_recovery_hint(self):
        text = plain_player_output({"ok": False, "response": {
            "accepted": False, "rejection_reason": "stale_frame", "next": "look"}})
        self.assertEqual(text.count("Next: play look"), 1)

    def test_first_world_history_is_not_presented_as_fresh_action_messages(self):
        from gameplay_display import display
        old = {"time": "8:00:00 AM", "text": "Unknown command: }"}
        latest = {"time": "3:59:54 PM", "text": "I'll see you around."}
        response = {"ok": True, "observation": {"run_id": "run", "surface": {
            "kind": "world", "facts": {"messages": [old, latest]}, "actions": []}}}
        shown, snapshot = display(response)
        result = {"ok": True, "state": "collected", "response": shown}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        self.assertNotIn("Unknown command", text)
        self.assertIn("Earlier messages: 1", text)
        self.assertIn("may predate this action", text)
        self.assertIn("I'll see you around", text)
        response = json.loads(json.dumps(response))
        response["observation"]["surface"]["facts"]["messages"].append(
            {"time": "3:59:55 PM", "text": "Zombie hits you for 5 damage."})
        shown, snapshot = display(response, snapshot)
        text = plain_player_output({**result, "response": shown}, snapshot=snapshot, full_look=False)
        self.assertIn("5 damage", text)
        self.assertNotIn("may predate", text)

    def test_last_creature_disappearing_is_reported_in_action_output(self):
        text = plain_player_output({"ok": True, "response": {
            "facts_changed": {"visible_entities": []}}}, full_look=False)
        self.assertIn("No visible creatures.", text)

    def test_unchanged_controls_are_not_repeated_but_look_and_changes_show_them(self):
        snapshot = {"owner": "look_cursor", "current": {"facts": {"text": "floor"},
                    "actions": [{"id": "cursor.east", "label": "east"}]}}
        current = {"owner": "look_cursor", "view": "delta"}
        result = {"ok": True, "state": "collected", "response": {"current_input": current}}
        text = plain_player_output(result, snapshot=snapshot, full_look=False)
        self.assertIn("Allowed actions unchanged.", text)
        self.assertNotIn("play act cursor.east", text)
        self.assertIn("play act cursor.east", plain_player_output(result, snapshot=snapshot))
        for change in ("actions_changed", "actions_removed"):
            snapshot["current"]["actions"] = [{"id": "cursor.west", "label": "west"}]
            current[change] = [{"id": "cursor.east" if change == "actions_removed" else "cursor.west"}]
            updated = plain_player_output(result, snapshot=snapshot, full_look=False)
            self.assertIn("play act cursor.west", updated)
            self.assertNotIn("play act cursor.east", updated)
            self.assertIn("Allowed actions replaced" if change == "actions_removed" else "others unchanged", updated)
            del current[change]

    def test_item_summary_keeps_stats_and_counts_omitted_text_with_retrieval(self):
        prose = "A long historical description.\n"
        snapshot = {"owner": "item_info", "current": {"facts": {
            "item_info_text": "Damage: 21\n" + prose + "* Conducts electricity.\n",
            "item_name": "Bullet"}, "actions": [{"id": "item_info.close", "label": "Close"}]}}
        result = {"ok": True, "state": "collected", "response": {"current_input": {
            "owner": "item_info", "source_selector": "observation"}}}
        text = plain_player_output(result, snapshot=snapshot)
        self.assertIn("Damage: 21", text)
        self.assertIn("* Conducts electricity.", text)
        self.assertNotIn(prose, text)
        self.assertIn(f"{len(prose)} characters omitted", text)
        self.assertIn("play inspect observation.surface.facts.item_info_text", text)
        self.assertIn("play act item_info.close", text)
        retrieved = plain_player_output({"ok": True, "response_sha256": "hidden", "slice": prose})
        self.assertEqual(retrieved, prose)

    def test_npc_summary_keeps_weapon_contents_and_full_details_route(self):
        snapshot = {"owner": "npc_inspection", "current": {"facts": {
            "actor_name": "Ada", "diagnostic_items": {
                "1": {"name": "Pistol", "slot": "wielded", "parent_uid": ""},
                "2": {"name": "Ammo", "slot": "MAGAZINE", "parent_uid": "1", "charges": 6},
                "3": {"name": "Shirt", "slot": "worn", "parent_uid": ""}},
            "diagnostic_rules": {"aim": "Careful"}}, "actions": []}}
        result = {"ok": True, "state": "collected", "response": {"current_input": {
            "owner": "npc_inspection", "source_selector": "result"}}}
        text = plain_player_output(result, snapshot=snapshot)
        self.assertIn("Pistol", text)
        self.assertIn("Ammo", text)
        self.assertIn("Shirt", text)
        self.assertIn("no --contains", text)
        self.assertIn("play inspect result.surface.facts.diagnostic_items", text)
        self.assertIn("play inspect result.surface.facts.diagnostic_rules", text)

    def test_menu_target_prefix_is_shared_without_changing_command_syntax(self):
        from gameplay_display import _plain_controls
        text = _plain_controls([{"id": "menu.choose", "stable_id": "uilist-entry:837", "label": "zombie"}])
        self.assertEqual(text.count("uilist-entry:"), 1)
        self.assertIn("--target uilist-entry:<target>", text)
        self.assertIn("837 — zombie", text)

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
        self.assertIn("ALLOWED HERE ONLY", text)
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
        self.assertIn("Actions for every target below: select", text)
        self.assertIn("option:170 — Choice 170", text)

    def test_inventory_common_verbs_are_shared_even_with_container_extras(self):
        from gameplay_display import _plain_controls
        actions = [{"id": "inventory." + verb, "stable_id": str(i), "label": f"item {i}"}
                   for i in range(3) for verb in ("details", "select")]
        actions.append({"id": "inventory.contents", "stable_id": "2", "label": "item 2"})
        text = _plain_controls(actions)
        self.assertEqual(text.count("details/select"), 1)
        self.assertIn("2 — item 2 — also contents", text)

    def test_explicit_npc_rules_read_retains_every_rule_without_repeated_subject(self):
        text = plain_player_output({"ok": True, "selector": "observation.surface.facts.diagnostic_rules",
            "slice": {"aim": "She will aim carefully.", "engagement": "She will attack nearby enemies.",
                      "rules": [{"label": "She will not use grenades."}, {"label": "She will use guns."}]}})
        for expected in ("aim carefully", "attack nearby enemies", "not use grenades", "use guns"):
            self.assertIn(expected, text)
        self.assertNotIn("She will", text)

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
                         "play act world.wait"):
            self.assertIn(expected, text)
        for unwanted in ("Damp", "<color", "Combat: fire", "play wait 20s", "--bound-maximum 5"):
            self.assertNotIn(unwanted, text)
        with_movement = world_look(self.world_snapshot(), {"game.move_relative": True})
        self.assertIn("--bound-maximum 5", with_movement)

    def test_observation_only_phase_keeps_facts_without_unusable_actions(self):
        snapshot = self.world_snapshot()
        availability = {"game.act": False, "game.wait": False, "game.move_relative": False}
        for show_controls in (True, False):
            text = world_look(snapshot, availability, show_controls=show_controls)
            self.assertIn("six-shooter (0/6)", text)
            self.assertIn("Observation-only phase", text)
            self.assertNotIn("play act", text)
            self.assertNotIn("Controls unchanged", text)
        text = world_look(snapshot, availability, controls_only=True)
        self.assertNotIn("play act", text)
        self.assertIn("play look", text)

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
                         'play act inventory.filter --param "text=TEXT"', "play act inventory.cancel"):
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
                         "World.")

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
