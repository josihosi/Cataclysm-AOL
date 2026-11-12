"""Exercise the public player CLI against retained bridge traffic, without a game."""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_archive import Archive, ArchiveSequence
from play_cli import PlayerClient, session_lock

CLI = Path(__file__).with_name("play_cli.py")


class PlayerCliTest(unittest.TestCase):
    def test_finish_rejects_journal_label_before_submission_and_accepts_observation_id(self):
        client = PlayerClient(self.session)
        client.state["terminal_observation_id"] = "run-a:frame:terminal"
        with patch.object(client, "_reporting_exit_refusal", return_value=None), \
                patch.object(client, "submit", return_value={"ok": True}) as submit:
            result = client.finish(None, 0, outcome={"citations": [{"observation_id": "J0004"}]})
            self.assertEqual(result["error"], "outcome_citation_requires_observation_id")
            self.assertIn("witness journal", result["next"])
            submit.assert_not_called()
            outcome = {"citations": [{"observation_id": "run-a:frame:7"}]}
            self.assertTrue(client.finish(None, 0, outcome=outcome)["ok"])
            self.assertEqual(submit.call_args.args[0]["outcome"], outcome)

    def test_typed_omt_preview_and_move_require_preview_identity(self):
        self.observe()
        self.cli("act", "world.debug_omt_teleport", "--omt", "-4", "7", "-1", ok=False)
        self.assertFalse(any(x.get("request", {}).get("action_id") == "world.debug_omt_teleport" for x in self.requests()))
        self.cli("act", "world.debug_omt_preview", "--omt", "-4", "7", "-1")
        sent = next(x["request"] for x in self.requests() if x.get("request", {}).get("action_id") == "world.debug_omt_preview")
        self.assertEqual(sent["parameters"], {"x_omt": "-4", "y_omt": "7", "z": "-1"})

    def test_typed_omt_move_preserves_exact_preview_and_negative_coordinates(self):
        self.observe()
        self.cli("act", "world.debug_omt_teleport", "--omt", "-181", "3", "2",
                 "--preview-request-id", "native-preview-42")
        sent = next(x["request"] for x in self.requests() if x.get("request", {}).get("action_id") == "world.debug_omt_teleport")
        self.assertEqual(sent["parameters"], {"x_omt": "-181", "y_omt": "3", "z": "2",
                                                "preview_request_id": "native-preview-42"})

    def test_omt_preview_is_visible_in_ordinary_cli_output(self):
        self.observe()
        pending = self.cli("act", "world.debug_omt_preview", "--omt", "145", "149", "1")
        preview = json.dumps({"preview_request_id": "native-preview", "terrain": "t_floor",
                              "floor": "floor_present", "support_and_hazards": "unknown"})
        self.reply(pending["request_id"], {"ok": True, "receipt": {"native_receipt": {
            "action_id": "world.debug_omt_preview", "accepted": True, "outcome": preview}},
            "observation": {"observation_id": "frame-2", "run_id": "run-a", "surface": {
                "kind": "world", "facts": {}, "actions": [{"id": "world.wait", "enabled": True}]}}})
        result = subprocess.run([sys.executable, str(CLI), "--session", str(self.session), "collect"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("t_floor", result.stdout)
        self.assertIn("native-preview", result.stdout)
        self.assertIn("unknown", result.stdout)

    def test_omt_options_cannot_be_repurposed_or_duplicate_parameters(self):
        self.observe()
        for args in (("world.wait", "--omt", "1", "2", "0"),
                     ("world.debug_omt_preview", "--omt", "1", "2", "0", "--param", "x_omt=8"),
                     ("world.debug_omt_preview", "--omt", "1", "2", "0", "--preview-request-id", "old")):
            self.cli("act", *args, ok=False)
        self.assertFalse(any(x.get("request", {}).get("action") == "game.act" for x in self.requests()))

    def test_ordinary_stopped_wait_reports_gameplay_without_receipt_dump(self):
        operation = {"schema":"caol-native-operation-v1", "kind":"wait", "state":"cancelled",
            "run_id":"run-a", "binding_id":"bound-a", "completed_progress_game_minutes":0,
            "requested_duration_game_minutes":1, "start_game_minutes":7920,
            "request_identity":{"request_id":"opaque-original-request","observation_id":"native-menu"},
            "accepted_owner":{"process_instance":"opaque-process", "sequence":21,
                              "frame_id":"native-menu"},
            "accepted_receipt":{"action_id":"wait.1m", "accepted":True,
                "provenance":"native_wait_duration_legacy_receipt", "legacy_native_frame_id":"opaque-legacy-frame"},
            "termination":{"reason":"native_activity_stopped", "duration_fulfilled":False,
                "receipt":{"process_instance":"opaque-process", "sequence":28, "accepted":True}}}
        pending=self.cli("look")
        self.reply(pending["request_id"], {"ok":True, "operation":operation,
            "observation":{"run_id":"run-a", "observation_id":"current-world",
                "surface":{"kind":"world", "facts":{
                    "avatar":json.dumps({"name":"Survivor","absolute_ms":[100,100,0]}),
                    "visible_local":json.dumps([{"dx":1,"dy":0,"visibility":"clear",
                        "terrain":"floor","passable":True,"fields":["fd_fire"]}]),
                    "visible_entities":json.dumps([{"name":"zombie","attitude":"hostile","dx":-1,"dy":0}])},
                    "actions":[{"id":"world.move.east","stable_id":"","enabled":True}]}}})
        output=subprocess.run([sys.executable,str(CLI),"--session",str(self.session),"collect"],
                              capture_output=True,text=True)
        self.assertEqual(output.returncode,0,output.stderr)
        print("R067_NORMAL_WAIT_OUTPUT bytes="+str(len(output.stdout.encode())))
        self.assertIn("stopped",output.stdout.lower())
        self.assertIn("0 game minutes",output.stdout)
        self.assertIn("zombie",output.stdout)
        self.assertIn("fd_fire",output.stdout)
        self.assertIn("world.move.<direction>",output.stdout)
        self.assertIn("east",output.stdout)
        for secretless_internal in ("opaque-legacy-frame","opaque-process","opaque-original-request",
                                    "Sequence:","Accepted receipt:","Accepted owner:"):
            self.assertNotIn(secretless_internal,output.stdout)
        inspected=self.cli("inspect","operation.accepted_receipt")
        self.assertEqual(inspected["slice"],operation["accepted_receipt"])

    def test_optional_look_map_survives_pending_collect_without_extra_native_request(self):
        pending = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                  "--wait-seconds", "0", "look", "--map"],
                                 capture_output=True, text=True)
        self.assertEqual(pending.returncode, 0, pending.stderr + pending.stdout)
        self.assertIn("play collect --map", pending.stdout)
        requests = self.requests()
        self.assertEqual(len(requests), 1)
        self.assertEqual(requests[0]["request"], {"action": "game.observe"})
        request_id = requests[0]["request_id"]
        facts = {"avatar": json.dumps({"name": "Test00", "absolute_ms": [3158, 3449, 0]}),
                 "minimap": json.dumps({"schema": "caol-native-minimap-v1", "radius": 12,
                                        "cells": [{"dx": -3, "dy": -9, "visibility": "clear",
                                                   "terrain": "closed wood door", "passable": False}]}),
                 "visible_local": json.dumps([{"dx": 1, "dy": 0, "visibility": "clear", "terrain": "floor",
                                                "furniture": "f_brazier", "fields": ["fd_smoke"]}]),
                 "visible_entities": "[]"}
        self.reply(request_id, {"ok": True, "result": {"observation_id": "run-a:frame:1",
            "run_id": "run-a", "surface": {"kind": "world", "facts": facts, "actions": []}}})
        collected = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                    "collect", "--map"], capture_output=True, text=True)
        self.assertEqual(collected.returncode, 0, collected.stderr + collected.stdout)
        self.assertIn("LOCAL MAP z=0", collected.stdout)
        self.assertIn("D closed door", collected.stdout)
        self.assertIn("S brazier with smoke", collected.stdout)
        ordinary = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                   "collect"], capture_output=True, text=True)
        self.assertEqual(ordinary.returncode, 0, ordinary.stderr + ordinary.stdout)
        self.assertNotIn("LOCAL MAP", ordinary.stdout)
        self.assertEqual(len(self.requests()), 1)

    def test_repeated_world_controls_refresh_on_changed_actions_and_generation(self):
        for generation, action, unchanged in ((0, "world.wait", False), (0, "world.wait", True),
                                              (0, "world.fire", False), (1, "world.fire", False)):
            self.write("status.json", {"binding_id": "bound-a", "state": "ready", "session_generation": generation})
            pending = self.cli("look")
            self.reply(pending["request_id"], {"ok": True, "result": {
                "observation_id": "frame-" + pending["request_id"], "surface": {"kind": "world", "facts": {},
                    "actions": [{"id": action, "stable_id": "", "label": action, "enabled": True}]}}})
            result = self.cli("collect")
            self.assertTrue(result["world_observation"])
            self.assertEqual(result["world_controls_unchanged"], unchanged)

    def test_journal_summary_keeps_finish_fields_and_stable_retrieval(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "request_id": "journal-request", "result": {
            "evidence_journal": {"entries": [{"value": "history" * 10000}]}}})
        for required in ("1 entries", "--request-id journal-request", "--outcome TEXT", "--verdict", "--witness FILE"):
            self.assertIn(required, text)
        self.assertNotIn("Copy exact values", text)
        self.assertNotIn("history", text)

    def test_filtered_journal_commands_keep_source_index_and_filter(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "selector": "result.evidence_journal.entries",
            "slice": [{"citation_id": "J0008", "kind": "observation", "value": {"game_minutes": 10}}],
            "source_indices": [7], "filter": {"contains": "world"},
            "page": {"offset": 0, "next_offset": 1}}, inspection_request_id="old-request")
        self.assertIn("INDEX 7; SELECTOR result.evidence_journal.entries.INDEX.value.FIELD", text)
        self.assertIn("--request-id old-request", text)
        self.assertIn("--offset 1 --limit 1 --contains world --request-id old-request", text)

    def test_journal_listing_does_not_expand_nested_game_history(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "selector": "result.evidence_journal.entries",
            "slice": [{"citation_id": "J0001", "kind": "observation", "value": {
                "surface": {"kind": "world", "facts": {"irrelevant_history": "x" * 100000}}}}],
            "page": {"offset": 0, "total": 1, "next_offset": None}})
        self.assertIn("J0001: observation — world", text)
        self.assertIn("SELECTOR result.evidence_journal.entries.INDEX.value.surface.facts.FIELD", text)
        self.assertNotIn("x" * 1000, text)
        self.assertLess(len(text), 500)

    def test_plain_messages_group_comfort_but_keep_damage_and_alarms(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "slice": [
            {"text": "You feel your left leg getting warm.", "time": "12:00"},
            {"text": "You feel your right leg getting warm.", "time": "12:00"},
            {"text": "The zombie hits your left leg for 5 damage.", "time": "12:01"}],
            "turn_assessment": {"alarms": [{"message": "Waiting is too slow. Tell the coordinator."}]}})
        self.assertIn("Temperature comfort messages: 2 grouped", text)
        self.assertIn("5 damage", text)
        self.assertIn("Tell the coordinator", text)

    def test_display_cache_replaces_previous_snapshot_in_session(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "world-frame", "surface": {"kind": "world", "facts": {}, "actions": []}}})
        self.cli("collect")
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertIn("display", state)
        self.assertNotIn("display_sha256", state)

    def test_world_cache_keeps_gameplay_once_and_diagnostics_in_exact_response(self):
        facts = {
            "avatar": {"name": "Survivor", "absolute_ms": [100, 100, 0]},
            "visible_local": [{"dx": 1, "dy": 0, "visibility": "clear",
                               "terrain": "floor", "passable": True, "fields": ["fd_acid"]}],
            "visible_entities": [{"id": 7, "name": "zombie", "attitude": "hostile",
                                  "dx": -1, "dy": 0, "absolute_ms": [99, 100, 0]}],
            "visible_zones": [{"id": "fuel", "name": "Firewood"}],
            "messages": [{"time": "12:00", "text": "The zombie hits you for 5 damage."}],
            "minimap": {"radius": 1, "cells": [{"dx": 1, "dy": 0, "visibility": "clear",
                                                 "terrain": "floor", "fields": ["fd_acid"]}]},
        }
        diagnostics = {key: {"activity_id": "internal-operation", "source_sha256": "a" * 64,
                             "history": "internal state " * 60} for key in (
            "structural_outing_owner", "site_outing_owners", "structural_signal_dispatch",
            "staffed_camp_signal_leads")}
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "run_id": "run-a", "observation_id": "world-1", "surface": {
                "kind": "world", "facts": {k: json.dumps(v) for k, v in (facts | diagnostics).items()},
                "actions": [{"id": "world.pickup", "enabled": True}]}}})
        collected = self.cli("collect")
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("world", state["display"])
        self.assertEqual(state["display"]["current"]["facts"], facts)
        self.assertTrue(diagnostics.keys().isdisjoint(collected["response"]["facts_changed"]))
        self.assertNotIn("internal state", (self.session / "play-client.json").read_text())
        self.assertEqual(self.cli("inspect", "surface.facts.structural_outing_owner")["slice"],
                         diagnostics["structural_outing_owner"])
        plain = subprocess.run([sys.executable, str(CLI), "--session", str(self.session), "collect"],
                               capture_output=True, text=True)
        self.assertEqual(plain.returncode, 0, plain.stderr)
        for text in ("fd_acid", "zombie", "hostile"):
            self.assertIn(text, plain.stdout)
        from gameplay_display import plain_player_output
        self.assertIn("5 damage", plain_player_output(
            collected, snapshot=PlayerClient(self.session).display_snapshot(), full_look=False))
        self.assertNotIn("internal-operation", plain.stdout)
        self.assertNotIn("a" * 64, plain.stdout)

        # A native child owner retains the preceding World only while needed.
        action = self.cli("act", "world.pickup")
        self.reply(action["request_id"], {"ok": True, "observation": {
            "run_id": "run-a", "observation_id": "pickup-2", "surface": {
                "kind": "pickup", "facts": {"title": "Pickup"},
                "actions": [{"id": "inventory.cancel", "enabled": True}]}}})
        self.cli("collect")
        modal = json.loads((self.session / "play-client.json").read_text())["display"]
        self.assertEqual(modal["world"]["facts"], facts)
        self.assertEqual(modal["current"]["observation_id"], "pickup-2")
        # A read-only refresh inside the modal must not erase same-run World
        # history or make old damage messages appear newly observed on return.
        looked = self.cli("look")
        self.reply(looked["request_id"], {"ok": True, "result": {
            "run_id": "run-a", "observation_id": "pickup-look-2", "surface": {
                "kind": "pickup", "facts": {"title": "Pickup"},
                "actions": [{"id": "inventory.cancel", "enabled": True}]}}})
        self.cli("collect")
        modal = json.loads((self.session / "play-client.json").read_text())["display"]
        self.assertEqual(modal.get("world", {}).get("facts"), facts)
        cancelled = self.cli("act", "inventory.cancel")
        request = next(r for r in self.requests() if r["request_id"] == cancelled["request_id"])
        self.assertEqual(request["request"]["observation_id"], "pickup-look-2")
        self.reply(cancelled["request_id"], {"ok": True, "observation": {
            "run_id": "run-a", "observation_id": "world-3", "surface": {
                "kind": "world", "facts": {k: json.dumps(v) for k, v in (facts | diagnostics).items()},
                "actions": [{"id": "world.pickup", "enabled": True}]}}})
        returned = self.cli("collect")
        self.assertEqual(returned["response"]["facts_changed"], {})
        final = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("world", final["display"])
        self.assertEqual(final["observation_id"], "world-3")
        self.assertEqual(len(self.requests()), 4)

    def test_modal_refresh_discards_world_history_on_changed_run(self):
        from gameplay_display import display
        def response(run, kind, facts):
            return {"ok": True, "result": {"run_id": run, "observation_id": run + kind,
                    "surface": {"kind": kind, "facts": facts, "actions": []}}}
        _, world = display(response("a", "world", {"terrain": "floor"}))
        _, modal = display(response("a", "pickup", {"title": "Pickup"}), world)
        _, refreshed = display(response("a", "pickup", {"title": "Pickup"}), modal, refresh=True)
        self.assertEqual(refreshed["world"]["facts"], {"terrain": "floor"})
        _, different = display(response("b", "pickup", {"title": "Pickup"}), refreshed, refresh=True)
        self.assertNotIn("world", different)
        returned, _ = display(response("b", "world", {"terrain": "floor"}), different)
        self.assertEqual(returned["facts_changed"], {"terrain": "floor"})

    def test_world_menu_only_explains_macros_allowed_in_this_session(self):
        from gameplay_display import world_look
        snapshot = {"current": {"facts": {}, "actions": [
            {"id": "world.move.east", "enabled": True},
            {"id": "world.wait", "enabled": True}]}}
        restricted = world_look(snapshot, {"game.wait": False, "game.move_relative": False})
        self.assertIn("play act world.move.<direction>", restricted)
        self.assertIn("play act world.wait", restricted)
        self.assertNotIn("play move --east", restricted)
        self.assertNotIn("play wait <duration>", restricted)
        allowed = world_look(snapshot, {"game.wait": True, "game.move_relative": True})
        self.assertIn("play move --east", allowed)
        self.assertIn("play wait <duration>", allowed)

    def test_inventory_commit_does_not_invite_a_target_argument(self):
        from gameplay_display import _plain_controls
        text = _plain_controls([
            {"id": "inventory.commit", "label": "Select", "enabled": True},
            {"id": "inventory.select", "label": "lighter", "enabled": True, "stable_id": "12097"},
        ])
        self.assertIn("Confirm highlighted item: Select → play act inventory.commit (no --target)", text)
        self.assertIn("select: confirm the supplied target", text)
        self.assertIn("12097 — lighter", text)

    def test_plain_startup_pending_is_not_an_action_failure(self):
        from gameplay_display import plain_player_output
        self.assertEqual(plain_player_output({"ok": False, "status": {
            "state": "starting", "bridge_pid": 123, "session_generation": 0}}),
            "Loading → play look")

    def test_plain_startup_dialog_shows_exact_message_and_decision_boundary(self):
        from gameplay_display import plain_player_output
        dialog = {"state": "confirmed_debug_dialog", "pid": 61861,
                  "birth_identity": "posix-lstart:Sat Sep 26 20:33:17 2026",
                  "message": "parent location doesn't exist. Item_location has lost its target over a save/load cycle.",
                  "source_file": "src/item_location.cpp", "source_line": 1012,
                  "log_path": "/profile/config/debug.log", "log_byte_offset": 1234,
                  "image_path": "/session/startup-dialog-captures/abc.png",
                  "image_sha256": "a" * 64, "session": "/session", "run_id": "run-r014"}
        text = plain_player_output({"ok": False, "status": {
            "state": "process_dead", "reason": "startup_proof_red"},
            "startup_dialog": dialog})
        self.assertIn("parent location doesn't exist", text)
        self.assertIn("src/item_location.cpp:1012", text)
        self.assertIn("PID 61861", text)
        self.assertIn("20:33:17", text)
        self.assertIn("notify the coordinator asynchronously", text)
        self.assertIn("debug-ignore --capture " + "a" * 64, text)
        self.assertIn(".agents/skills/caol-harness/references/debug-errors.md", text)
        self.assertNotIn("owner approval", text)
        self.assertIn("No input sent", text)

    def test_plain_loading_without_confirmed_dialog_does_not_offer_recovery_input(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": False, "status": {"state": "starting"},
            "startup_dialog": {"state": "loading_unknown", "pid": 42,
                               "reason": "no_debug_dialog_confirmed"}})
        self.assertIn("Loading", text)
        self.assertIn("PID 42", text)
        self.assertNotIn("one I/i", text)
        preparing = plain_player_output({"ok": False, "status": {"state": "preparing"},
            "startup_dialog": {"state": "loading_unknown", "pid": 43}})
        self.assertIn("PID 43", preparing)

    def test_target_menu_prints_exact_action_and_target_together(self):
        from gameplay_display import _plain_controls
        text = _plain_controls([
            {"id": "target.move_cursor", "stable_id": "north", "label": "Move cursor north"},
            {"id": "target.choose", "stable_id": "coordinate:destination", "label": "Choose destination"},
        ])
        self.assertIn("play act target.move_cursor --target north", text)
        self.assertIn("play act target.choose --target coordinate:destination", text)
        self.assertNotIn("target.<action>", text)

    def test_plain_startup_failure_omits_transport_metadata(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": False, "status": {
            "state": "process_dead", "binding_id": "opaque", "bridge_pid": 123,
            "reason": "pre_descriptor_no_progress", "cleanup": {"status": "accepted"}}},
            startup_error="NPC API credential unavailable")
        self.assertEqual(text, "Playtest stopped: NPC API credential unavailable")

    def test_plain_lifecycle_collection_keeps_outcome_without_process_receipts(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "state": "finished",
            "bridge_state": "safe_to_cleanup", "cleanup": {"status": "already_exited",
                "process_generation": {"expected": {"command": "opaque-process-command"}}},
            "terminalization": {"cleanup": {"status": "already_exited"}},
            "next": "inspect retained evidence"})
        self.assertEqual(text, "Playtest ended. Cleanup complete.")
        self.assertEqual(plain_player_output({"ok": True, "state": "reentered",
            "session_generation": 1, "next": "look"}),
            "Saved game reloaded. Observe → play look")

    def test_plain_lifecycle_collection_keeps_failure_and_performance_alarm(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": False, "state": "cleanup_failed",
            "reason": "replacement could not load", "bridge_state": "reentry_failed",
            "turn_assessment": {"alarms": [{"message": "Tell the coordinator: slow turns."}]}})
        self.assertIn("replacement could not load", text)
        self.assertIn("Tell the coordinator: slow turns.", text)
        self.assertEqual(plain_player_output({"state": "finished", "cleanup": {"status": "unknown"}}),
                         "Playtest ended. Cleanup: unknown")

    def test_short_wait_preserves_existing_request(self):
        pending = self.cli("wait", "5m")
        sent = next(row for row in self.requests() if row["request_id"] == pending["request_id"])
        self.assertEqual(sent["request"], {"action": "game.wait", "wait": {
            "enabled": True, "recipe": ["world.wait", "wait.5m"],
            "target_delta_game_minutes": 5.0, "danger_handling": "ignore_danger_and_interruptions",
            "bound": {"basis": "game_mechanic", "source": "Player requested 5m",
                      "unit": "game_minutes", "maximum": 5.0, "progress_required": True}}})

    def test_wait_guidance_names_each_observation_policy(self):
        help_result = subprocess.run(
            [sys.executable, str(CLI), "--session", str(self.session), "wait", "--help"],
            capture_output=True, text=True, check=True,
        ).stdout
        guidance = " ".join(help_result.split())
        self.assertIn("safe returns to inspect danger/damage", guidance)
        self.assertIn("ignore continues through combat", guidance)
        self.assertIn("stop: deliberately stops at each interruption", guidance)
        self.assertIn("automatically ignores recognized typed activity distractions", guidance)
        self.assertIn("thirst and weather changes", guidance)
        self.assertIn("untyped or unknown distractions", guidance)
        self.assertIn("independently known to be harmless", guidance)
        self.assertIn("short command: ignore", help_result)
        for spelling, expected in (
            ("safe", "handle_classified_non_dangerous"),
            ("ignore", "ignore_danger_and_interruptions"),
            ("stop", "stop_on_interruption"),
        ):
            with self.subTest(spelling=spelling):
                pending = self.cli("wait", "5m", spelling)
                sent = next(row for row in self.requests() if row["request_id"] == pending["request_id"])
                self.assertEqual(sent["request"]["wait"]["danger_handling"], expected)
                self.reply(pending["request_id"], {"ok": True, "result": {"stop_reason": "target_reached"}})
                self.cli("collect")

    def test_prompt_ignore_uses_current_advertised_choice(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "chatter-frame", "run_id": "run-a", "surface": {
                "kind": "prompt", "facts": {"text": "In the way here mate! Stop waiting?"},
                "actions": [{"id": "prompt.choose", "stable_id": "prompt-option:6",
                             "label": "IGNORE", "enabled": True}]}}})
        self.cli("collect")
        chosen = self.cli("ignore")
        sent = next(row for row in self.requests() if row["request_id"] == chosen["request_id"])
        self.assertEqual(sent["request"]["action_id"], "prompt.choose")
        self.assertEqual(sent["request"]["stable_id"], "prompt-option:6")

    def test_retained_interrupt_prompt_cli_explains_and_uses_current_ignore_target(self):
        fixture = json.loads((CLI.with_name("fixtures") / "r014_cli_native_responses.json")
                             .read_text(encoding="utf-8"))["cases"]["interruption"]["response"]
        current = fixture["current_input"]
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "retained-interruption", "run_id": "run-a", "surface": {
                "kind": "prompt", "facts": current["facts_changed"], "actions": current["actions"]}}})
        shown = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                "--wait-seconds", "0", "collect"], capture_output=True, text=True)
        self.assertEqual(shown.returncode, 0, shown.stderr)
        self.assertIn("NO → play no — continue this time", shown.stdout)
        self.assertIn("IGNORE → play ignore — continue; ignore this distraction type", shown.stdout)
        manager_target = next(action["stable_id"] for action in current["actions"]
                              if action["label"] == "MANAGER")
        self.assertIn("MANAGER → play act prompt.choose --target " + manager_target, shown.stdout)
        chosen = self.cli("ignore")
        sent = next(row for row in self.requests() if row["request_id"] == chosen["request_id"])
        native_ignore = next(action["stable_id"] for action in current["actions"]
                             if action["label"] == "IGNORE" and action["enabled"])
        self.assertEqual(sent["request"]["stable_id"], native_ignore)

    def test_diagnostics_before_or_after_subcommand_keeps_the_same_read_only_result(self):
        base = [sys.executable, str(CLI), "--session", str(self.session), "--wait-seconds", "0"]
        before = subprocess.run([*base, "--diagnostics", "controls"], capture_output=True, text=True)
        after = subprocess.run([*base, "controls", "--diagnostics"], capture_output=True, text=True)
        self.assertEqual(before.returncode, 0, before.stderr)
        self.assertEqual(after.returncode, 0, after.stderr)
        self.assertEqual(json.loads(before.stdout), json.loads(after.stdout))
        observed_placement = subprocess.run([*base, "look", "--diagnostics"],
                                            capture_output=True, text=True)
        self.assertEqual(observed_placement.returncode, 0, observed_placement.stderr)
        self.assertIn("state", json.loads(observed_placement.stdout))

    def test_advertised_examine_opens_existing_native_menu_with_current_targets(self):
        fixture = json.loads((CLI.with_name("fixtures") / "r014_cli_native_responses.json")
                             .read_text(encoding="utf-8"))["cases"]["examine_menu"]["response"]
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "examine-world", "run_id": "run-a", "surface": {
                "kind": "world", "facts": {}, "actions": [
                    {"id": "world.look", "enabled": True},
                    {"id": "world.examine", "enabled": True}]}}})
        self.cli("collect")
        opened = self.cli("act", "world.examine")
        sent = next(row for row in self.requests() if row["request_id"] == opened["request_id"])
        self.assertEqual(sent["request"]["action_id"], "world.examine")
        menu = fixture["current_input"]
        self.reply(opened["request_id"], {"ok": True, "result": {
            "observation_id": "examine-menu", "run_id": "run-a", "surface": {
                "kind": "menu", "facts": menu["facts_changed"], "actions": menu["actions"]}}})
        shown = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                "--wait-seconds", "0", "collect"], capture_output=True, text=True)
        self.assertEqual(shown.returncode, 0, shown.stderr)
        current_target = next(action["stable_id"] for action in menu["actions"]
                              if action["id"] == "menu.choose" and action["label"] == "Extinguish fire")
        self.assertIn("Extinguish fire", shown.stdout)
        self.assertIn(current_target.removeprefix("uilist-entry:"), shown.stdout)
        chosen = self.cli("act", "menu.choose", "--target", current_target)
        choice = next(row for row in self.requests() if row["request_id"] == chosen["request_id"])
        self.assertEqual(choice["request"]["stable_id"], current_target)

    def test_plain_observation_preserves_facts_and_action(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "response": {"current_input": {
            "owner": "world", "actions": [{"id": "world.wait", "enabled": True}],
            "facts_changed": {"terrain": "forest", "weather": "rain"}}}})
        self.assertIn("forest", text)
        self.assertIn("rain", text)
        self.assertIn("play act world.wait", text)
        self.assertNotIn("Start waiting", text)
        self.assertNotIn("{", text)

    def test_short_answers_before_observation_request_look_without_sending_input(self):
        for answer in ("yes", "no", "ignore"):
            with self.subTest(answer=answer):
                self.assertIn("look_required", self.cli(answer, ok=False)["error"])
        self.assertEqual(self.requests(), [])

    def test_short_yes_uses_existing_advertised_target(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "prompt-frame", "run_id": "run-a", "surface": {
                "kind": "prompt", "facts": {"text": "Stop waiting?"},
                "actions": [{"id": "prompt.choose", "stable_id": "prompt-option:1",
                             "label": "YES", "enabled": True}]}}})
        self.cli("collect")
        chosen = self.cli("yes")
        sent = next(row for row in self.requests() if row["request_id"] == chosen["request_id"])
        self.assertEqual(sent["request"]["action"], "game.act")
        self.assertEqual(sent["request"]["action_id"], "prompt.choose")
        self.assertEqual(sent["request"]["stable_id"], "prompt-option:1")

    def test_plain_wait_keeps_progress_messages_and_alarm(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "response": {"outcome": {"chain": {
            "start_game_minutes": 10, "terminal_game_minutes": 15, "stop_reason": "target_reached",
            "model_round_trips": 1}}, "facts_changed": {"messages": {
                "new_events": [{"value": {"time": "12:05", "text": "You finish waiting."}}]}}},
            "turn_assessment": {"alarms": [{"kind": "waiting_slow", "mean_seconds": .02,
                "message": "Tell the coordinator: waiting performance needs attention."}]}})
        self.assertIn("Waited 5 game minutes. target reached.", text)
        self.assertIn("12:05: You finish waiting.", text)
        self.assertIn("Tell the coordinator", text)
        self.assertNotIn("round trips", text)

    def test_plain_quit_keeps_cleanup_failure_visible(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": True, "result": {"state": "finished",
            "cleanup": {"status": "failed", "pid": 1234}}})
        self.assertIn("Cleanup: failed", text)
        self.assertNotIn("1234", text)

    def test_interrupted_menu_requires_refresh_before_advertising_input(self):
        from gameplay_display import plain_player_output
        text = plain_player_output({"ok": False, "state": "rejected", "next": "look",
            "response": {"current_input": {"owner": "shakedown", "view": "full"},
                         "outcome": {"error": "keep_watch_recipe_action_not_advertised"}}},
            snapshot={"owner": "shakedown", "current": {
                "facts": {"title": "SHAKEDOWN DEMAND"},
                "actions": [{"id": "shakedown.fight", "label": "Fight"}]}})
        self.assertIn("SHAKEDOWN DEMAND", text)
        self.assertIn("Next: play look", text)
        self.assertIn("Refresh before choosing an action.", text)
        self.assertNotIn("play act shakedown.fight", text)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.session = Path(self.temp.name)
        (self.session / "requests").mkdir()
        (self.session / "responses").mkdir()
        self.write("bridge.manifest.json", {"binding_id": "bound-a"})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready"})

    def write(self, name, value):
        (self.session / name).write_text(json.dumps(value))

    def cli(self, *arguments, ok=True, wait_seconds=0):
        process = subprocess.run([sys.executable, str(CLI), "--diagnostics", "--session", str(self.session),
                                  "--wait-seconds", str(wait_seconds), *arguments], capture_output=True, text=True)
        self.assertEqual(process.returncode, 0 if ok else 1, process.stderr + process.stdout)
        from evidence_display import DEFAULT_BYTES, recover
        self.assertLessEqual(len(process.stdout.encode()), DEFAULT_BYTES)
        shown = json.loads(process.stdout)
        # The supported CLI now presents handles; verify the complete retained
        # result as well as the serialized boundary instead of demanding a dump.
        return recover(shown["presentation"]["full_evidence"]["sha256"]) if "presentation" in shown else shown

    def cli_async(self, *arguments, wait_seconds):
        return subprocess.Popen([sys.executable, str(CLI), "--diagnostics", "--session", str(self.session),
                                 "--wait-seconds", str(wait_seconds), *arguments],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def requests(self):
        return [json.loads(p.read_text()) for p in (self.session / "requests").glob("*.json")]

    def reply(self, request_id, value, binding="bound-a"):
        raw = json.dumps(value).encode()
        self.reply_raw(request_id, raw, binding=binding)

    def reply_raw(self, request_id, raw, binding="bound-a"):
        path = "responses/" + request_id + ".json"
        (self.session / path).write_bytes(bytes(raw))
        session_generation = json.loads((self.session / "status.json").read_text()).get("session_generation", 0)
        self.write("responses/" + request_id + ".receipt.json", {
            "request_id": request_id, "binding_id": binding,
            "session_generation": session_generation,
            "response_sha256": hashlib.sha256(raw).hexdigest(), "response_artifact": path,
        })

    def observe(self, frame="frame-1", exited=False):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": frame, "run_id": "run-a",
            "surface": {"kind": "process_exited" if exited else "world", "facts": {"last_save_result": "unattempted"},
                        "actions": [{"id": "world.wait", "enabled": True}]},
        }})
        self.cli("collect")
        return pending["request_id"]

    def test_cancel_is_out_of_band_and_preserves_pending_identity(self):
        self.write("play-client.json", {"binding_id": "bound-a", "pending": {
            "request_id": "pending-1", "request": {"action": "game.wait"}}})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "inflight_request_id": "pending-1"})
        self.write("active-request.json", {"request_id": "pending-1", "binding_id": "bound-a",
                                            "run_id": "run-a"})
        (self.session / "controls").mkdir()
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("cancel", "--reason", "user stopped waiting")
        self.assertTrue(result["ok"])
        self.assertEqual(result["request_id"], "pending-1")
        self.assertEqual(json.loads((self.session / "play-client.json").read_text())["pending"]["request_id"], "pending-1")
        markers = list((self.session / "controls").glob("cancel-*.json"))
        self.assertEqual(len(markers), 1)
        marker = json.loads(markers[0].read_text())
        self.assertEqual(marker["request_id"], "pending-1")
        self.assertEqual(marker["binding_id"], "bound-a")
        self.assertEqual(marker["run_id"], "run-a")

    def test_cancel_collection_revokes_old_frame_and_preserves_unknown_outcome(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "player_cancelled",
            "failure": {"unused_authority": "revoked", "detail": {"action_outcome": "unknown"}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "look")
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("pending", state)
        self.assertNotIn("observation_id", state)
        self.assertFalse(state.get("finished", False))
        self.assertEqual(state["cancellation"]["action_outcome"], "unknown")
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_performance_is_readable_under_pending_request_lock(self):
        self.cli("look")
        before = (self.session / "play-client.json").read_bytes()
        self.write("performance.latest.json", {"record_id": "p1", "owner": {"binding_id": "bound-a"}})
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("performance")
        self.assertEqual(result["latest"]["record_id"], "p1")
        self.assertEqual(result["comparison"]["status"], "unavailable")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), 1)

    def test_performance_configure_is_explicit_bound_and_consumed(self):
        import process_performance as perf
        from r009_technical_witness import sample_child_resources
        owner = {"pid": os.getpid(), "run_id": "run-a", "binding_id": "bound-a",
                 "process_identity": sample_child_resources(os.getpid())["process_identity"],
                 "source_binding": {"head": "source-a"}}
        self.write("performance-owner.json", owner)
        config = {"schema": "caol-performance-turn-config-v1", "provenance": "camp comparison",
                  "window_turns": 4, "tail_percentile": 0.95, "spike_seconds": 0.1,
                  "slow_turn_seconds": 0.1, "sustained_turns": 2,
                  "incomplete_turn_seconds": 5.0, "stalled_observation_seconds": 5.0,
                  "expected_progress": True}
        source = self.session / "explicit-config.json"
        source.write_text(json.dumps(config))
        result = self.cli("performance-configure", "--config", str(source))
        self.assertEqual(result["run_id"], "run-a")
        self.assertEqual(perf._turn_config(self.session), (config, None))
        trace = self.session / "semantic.native.events.jsonl"
        events = [{"event": "turn", "stage": stage, "run_id": "run-a",
                   "process_instance": "native-a", "turn_id": "1", "sequence": sequence,
                   "game_turn": 100, "game_minutes": 10, "wall_time_seconds": 10 + sequence,
                   "simulation_seconds": seconds, "phase": "simulation", "owner": "game::do_turn"}
                  for stage, sequence, seconds in (("start", 1, 0), ("end", 2, 0.2))]
        trace.write_text("".join(perf.TURN_TRACE_PREFIX + json.dumps(event) + "\n" for event in events))
        with patch("evidence_display.STORE", self.session / "evidence"):
            assessment = perf.collect_turn_assessment(self.session, "bound-a")
        self.assertIn("spike", {alarm["kind"] for alarm in assessment["alarms"]})
        self.cli("performance-configure", "--config", str(source), ok=False)
        self.assertEqual(perf._turn_config(self.session), (config, None))
        (self.session / perf.TURN_CONFIG_NAME).unlink()
        plain = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                "performance-configure", "--config", str(source)],
                               capture_output=True, text=True)
        self.assertEqual(plain.returncode, 0, plain.stderr)
        self.assertIn("Turn assessment configured for run run-a", plain.stdout)

    def test_performance_configure_rejects_bad_config_and_wrong_owner(self):
        import process_performance as perf
        from r009_technical_witness import sample_child_resources
        owner = {"pid": os.getpid(), "run_id": "run-a", "binding_id": "other",
                 "process_identity": sample_child_resources(os.getpid())["process_identity"],
                 "source_binding": {"head": "source-a"}}
        self.write("performance-owner.json", owner)
        config = {"schema": "caol-performance-turn-config-v1", "provenance": "camp comparison",
                  "window_turns": 4, "tail_percentile": 0.95, "spike_seconds": 0.1,
                  "slow_turn_seconds": 0.1, "sustained_turns": 2,
                  "incomplete_turn_seconds": 5.0, "stalled_observation_seconds": 5.0,
                  "expected_progress": True}
        source = self.session / "explicit-config.json"
        source.write_text(json.dumps(config))
        self.cli("performance-configure", "--config", str(source), ok=False)
        owner["binding_id"] = "bound-a"
        self.write("performance-owner.json", owner)
        owner["process_identity"] = "not-the-current-process"
        self.write("performance-owner.json", owner)
        self.cli("performance-configure", "--config", str(source), ok=False)
        owner["process_identity"] = sample_child_resources(os.getpid())["process_identity"]
        self.write("performance-owner.json", owner)
        self.write("status.json", {"binding_id": "bound-a", "state": "safe_to_cleanup"})
        self.cli("performance-configure", "--config", str(source), ok=False)
        self.write("status.json", {"binding_id": "bound-a", "state": "ready"})
        config["spike_seconds"] = 0
        source.write_text(json.dumps(config))
        self.cli("performance-configure", "--config", str(source), ok=False)
        self.assertFalse((self.session / perf.TURN_CONFIG_NAME).exists())
        config["spike_seconds"] = 0.1
        config["extra"] = float("nan")
        source.write_text(json.dumps(config))
        self.cli("performance-configure", "--config", str(source), ok=False)
        self.assertFalse((self.session / perf.TURN_CONFIG_NAME).exists())

    def test_performance_consumer_changes_runtime_source_binding(self):
        import startup_harness
        root = self.session / "repo"
        source = root / "tools/openclaw_harness/process_performance.py"
        source.parent.mkdir(parents=True)
        source.write_text("value = 1\n")
        subprocess.run(["git", "-C", str(root), "init", "-q"], check=True)
        subprocess.run(["git", "-C", str(root), "add", str(source.relative_to(root))], check=True)
        subprocess.run(["git", "-C", str(root), "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                        "commit", "-qm", "baseline"], check=True)
        with patch("startup_harness.repo_root", return_value=root):
            before = startup_harness.runtime_source_binding()
            source.write_text("value = 2\n")
            after = startup_harness.runtime_source_binding()
        self.assertTrue(before["ok"] and after["ok"])
        self.assertNotEqual(before["sha256"], after["sha256"])

    def test_declared_saved_checkpoint_preserves_explicit_native_option(self):
        import startup_harness as harness
        scenario = {"saved_world_snapshot": "/task/owned/checkpoint",
                    "profile_option_overrides": {"RAID_PERFORMANCE_DIAGNOSTIC": "normal"}}
        declared = harness.resolve_scenario_profile_option_overrides(scenario)
        selected = harness.continuation_profile_option_overrides(scenario, declared)
        options = self.session / "options.json"
        options.write_text("[]")
        self.assertEqual(harness.apply_option_overrides_to_file(options, selected)["applied"], declared)
        self.assertEqual(json.loads(options.read_text())[0]["value"], "normal")
        self.assertEqual(harness.continuation_profile_option_overrides(
            {**scenario, "post_relaunch": {"steps": ["save"]}}, declared), {})
        self.assertEqual(harness.continuation_profile_option_overrides({}, declared), {})

    def test_performance_samples_actual_owned_process_under_request_lock(self):
        import os
        from r009_technical_witness import sample_child_resources
        initial = sample_child_resources(os.getpid())
        owner = {"pid": os.getpid(), "run_id": "test-process", "binding_id": "bound-a",
                 "process_identity": initial["process_identity"], "platform": initial["platform"],
                 "source_binding": {"runtime_binding_sha256": "test-source"}}
        self.write("performance-owner.json", owner)
        self.write("performance-context.json", {"owner": owner, "context_id": "c1",
                   "context": {"phase": "unknown", "source": "test_process_fixture"}})
        self.cli("look")
        before = (self.session / "play-client.json").read_bytes()
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("performance", "--sample-seconds", "0.05")
        resources = result["latest"]["resources"]
        self.assertEqual(resources["cpu_percent"]["status"], "available")
        self.assertGreater(resources["interval_wall_seconds"], 0)
        self.assertGreater(resources["resident_memory"]["value"], 0)
        self.assertEqual(result["latest"]["owner"]["pid"], os.getpid())
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), 1)

    def test_performance_baseline_is_explicit_and_does_not_overwrite(self):
        self.write("performance.latest.json", {"record_id": "p1", "owner": {"binding_id": "bound-a"}})
        baseline = self.session / "baseline.json"
        self.cli("performance", "--save-baseline", str(baseline), ok=False)
        self.cli("performance", "--tag", "camp idle", "--save-baseline", str(baseline))
        self.assertEqual(json.loads(baseline.read_text())["comparison_tag"], "camp idle")
        self.cli("performance", "--tag", "camp idle", "--save-baseline", str(baseline), ok=False)

    def test_controls_preserves_pending_and_observation_authority(self):
        self.observe()
        self.write("play-client.json", {**json.loads((self.session / "play-client.json").read_text()),
                   "operation_availability": {"game.wait": True, "game.move_relative": False}})
        self.cli("act", "world.wait")
        before = (self.session / "play-client.json").read_bytes()
        requests = self.requests()
        result = self.cli("controls")["result"]
        self.assertEqual(result["availability"], {"game.wait": True, "game.move_relative": False})
        self.assertEqual(result["setup"]["debug_life_support"]["mutation_id"], "DEBUG_LS")
        self.assertIn("Only if missing", result["setup"]["debug_life_support"]["sequence"])
        self.assertEqual(result["wait"]["manual_start_request"]["action"], "game.act")
        self.assertEqual(result["wait"]["manual_start_request"]["action_id"], "world.wait")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(self.requests(), requests)

    def test_controls_discovers_bound_and_shared_logs_without_touching_pending_state(self):
        run_dir = self.session.parent / "actual-run"
        profile = self.session.parent / "profile"
        config = profile / "config"
        config.mkdir(parents=True, exist_ok=True)
        logs = {
            "native_semantic_events": run_dir / "semantic.native.events.jsonl",
            "native_semantic_snapshot": run_dir / "semantic.native.log",
            "transition_events": run_dir / "transition.events.jsonl",
            "profile_diagnostic_debug": config / "debug.log",
        }
        run_dir.mkdir(exist_ok=True)
        record = {"event": "surface_descriptor", "run_id": "run-a", "frame_id": "frame-a"}
        for name, path in logs.items():
            path.write_text(json.dumps(record) + "\n")
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a",
                                         "command": "cataclysm-tiles --userdir " + str(profile),
                                         "log_paths": {name: {"path": str(path),
                                                              "scope": "run_bound" if name != "profile_diagnostic_debug" else "profile_shared"}
                                                       for name, path in logs.items()}})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "session_descriptor": {"run_id": "run-a"}})
        before = (self.session / "status.json").read_bytes()
        result = self.cli("controls")
        logs_result = result["evidence_logs"]
        self.assertEqual(logs_result["run_id"], "run-a")
        by_name = {entry["name"]: entry for entry in logs_result["entries"]}
        self.assertEqual(by_name["native_semantic_events"]["scope"], "run_bound")
        self.assertEqual(by_name["native_semantic_events"]["status"], "available")
        self.assertEqual(by_name["transition_events"]["status"], "available")
        self.assertEqual(by_name["profile_diagnostic_debug"]["scope"], "profile_shared")
        self.assertEqual(by_name["profile_diagnostic_debug"]["status"], "available")
        self.assertNotIn("--run-id", by_name["profile_diagnostic_debug"]["query"])
        queried = subprocess.run(by_name["native_semantic_events"]["query"], capture_output=True, text=True, check=True)
        self.assertEqual(json.loads(queried.stdout)["matched"], 1)
        self.assertEqual((self.session / "status.json").read_bytes(), before)
        self.assertFalse(self.requests())

    def test_controls_reports_descriptor_owner_run_mismatch_without_querying(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "owner-run",
                                         "log_paths": {"native_semantic_events": {
                                             "path": str(self.session / "events.jsonl"), "scope": "run_bound"}}})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                    "session_descriptor": {"run_id": "descriptor-run"}})
        result = self.cli("controls")
        logs = result["evidence_logs"]
        self.assertEqual(logs["identity_status"], "run_mismatch")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_requires_current_bridge_identity_before_querying_logs(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a"})
        for status in ({}, {"state": "ready"}, {"binding_id": "bound-a", "state": "ready"}):
            with self.subTest(status=status):
                self.write("status.json", status)
                logs = self.cli("controls")["evidence_logs"]
                self.assertEqual(logs["identity_status"], "unavailable")
                self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))
        (self.session / "status.json").unlink()
        logs = self.cli("controls")["evidence_logs"]
        self.assertEqual(logs["identity_status"], "unavailable")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_rejects_status_binding_mismatch(self):
        self.write("game-process.json", {"binding_id": "bound-a", "run_id": "run-a"})
        self.write("status.json", {"binding_id": "stale-binding", "state": "ready",
                                    "session_descriptor": {"run_id": "run-a"}})
        logs = self.cli("controls")["evidence_logs"]
        self.assertEqual(logs["identity_status"], "binding_mismatch")
        self.assertTrue(all(entry["query"] is None for entry in logs["entries"]))

    def test_controls_reports_unavailable_and_mismatched_log_identity(self):
        result = self.cli("controls")
        self.assertEqual(result["evidence_logs"]["identity_status"], "unavailable")
        self.assertTrue(all(entry["status"] == "unavailable" for entry in result["evidence_logs"]["entries"]))
        self.write("game-process.json", {"binding_id": "other-binding", "run_id": "run-a",
                                         "command": ["cataclysm-tiles", "--userdir", ".userdata/profile/"]})
        result = self.cli("controls")
        self.assertEqual(result["evidence_logs"]["identity_status"], "binding_mismatch")
        by_name = {entry["name"]: entry for entry in result["evidence_logs"]["entries"]}
        self.assertEqual(by_name["native_semantic_events"]["status"], "binding_mismatch")
        self.assertEqual(by_name["transition_events"]["status"], "binding_mismatch")
        self.assertEqual(by_name["profile_diagnostic_debug"]["status"], "binding_mismatch")

    def test_controls_before_observation_reports_unknown_permission(self):
        result = self.cli("controls")["result"]
        self.assertIsNone(result["availability"]["game.wait"])
        self.assertFalse((self.session / "play-client.json").exists())
        self.assertFalse(self.requests())

    def test_collect_remembers_service_macro_permission_metadata(self):
        pending = self.cli("look")
        availability = {"game.wait": False, "game.move_relative": True}
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "f1"},
                                         "operation_availability": availability})
        self.cli("collect")
        self.assertEqual(self.cli("controls")["result"]["availability"], availability)

    def test_persistent_pending_never_replays_and_action_uses_fresh_successor(self):
        pending = self.cli("look")
        self.assertEqual(pending["state"], "pending")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        self.cli("collect")
        self.assertEqual(len(self.requests()), 1)
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "frame-1",
            "surface": {"kind": "world", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        action = self.cli("act", "menu.choose", "--target", "choice-a", "--param", "amount=2")
        request = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(request["binding_id"], "bound-a")
        self.assertEqual(request["request"], {"action": "game.act", "action_id": "menu.choose",
                         "observation_id": "frame-1", "stable_id": "choice-a", "parameters": {"amount": "2"}})
        self.cli("act", "menu.choose", ok=False)
        self.assertEqual(len(self.requests()), 2)
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "frame-2",
            "surface": {"kind": "world", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        final = self.cli("act", "world.wait")
        request = next(r for r in self.requests() if r["request_id"] == final["request_id"])
        self.assertEqual(request["request"]["observation_id"], "frame-2")

    def test_structured_call_preserves_recipe_and_adopts_terminal_observation(self):
        self.observe()
        request = {"action": "game.wait", "wait": {
            "enabled": True, "target_game_minutes": 101,
            "recipe": ["world.wait"], "danger_handling": "stop_on_interruption",
            "bound": {"run_id": "run-a", "actor_id": "player-a"},
        }}
        path = self.session / "macro.json"
        path.write_text(json.dumps(request))
        pending = self.cli("call", "--request", str(path))
        sent = next(r for r in self.requests() if r["request_id"] == pending["request_id"])
        self.assertEqual(sent["request"], request)
        self.assertEqual(sent["binding_id"], "bound-a")
        self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"], "request_in_flight")
        self.cli("collect")
        self.assertEqual(len(self.requests()), 2)
        self.reply(pending["request_id"], {"ok": True, "result": {
            "terminal_observation": {"observation_id": "frame-after-wait", "game_minutes": 101,
                "surface": {"kind": "world", "actions": [{"id": "world.inventory"}]}}}})
        result = self.cli("collect")
        self.assertIn("look", result["next"])
        action = self.cli("act", "world.inventory")
        sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "frame-after-wait")
        self.reply(action["request_id"], {"ok": True, "observation": {"observation_id": "inventory-1",
            "surface": {"kind": "inventory", "actions": [{"id": "world.wait"}]}}})
        self.cli("collect")
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_structured_call_keeps_service_authorization_and_lifecycle_guards(self):
        from cockpit import CockpitService
        path = self.session / "macro.json"
        request = {"action": "game.wait", "wait": {"enabled": True}}
        path.write_text(json.dumps(request))
        pending = self.cli("call", "--request", str(path))
        service = CockpitService(allowed_live_operations=set())
        self.reply(pending["request_id"], service.call(request))
        self.assertEqual(self.cli("collect", ok=False)["state"], "rejected")
        for value in ([], {"action": "run.finish"}, {"action": 3}):
            path.write_text(json.dumps(value))
            self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"],
                             "call_requires_a_structured_game_request")
        path.write_text(json.dumps(request))
        self.write("play-client.json", {"binding_id": "bound-a", "sealed_terminal": {"observation_id": "f"}})
        self.assertIn("journal_is_sealed", self.cli("call", "--request", str(path), ok=False)["error"])
        self.assertEqual(len(self.requests()), 1)

    def test_structured_call_terminal_exit_offers_journal(self):
        path = self.session / "macro.json"
        path.write_text(json.dumps({"action": "game.wait", "wait": {"enabled": True}}))
        pending = self.cli("call", "--request", str(path))
        self.reply(pending["request_id"], {"ok": False, "result": {"terminal_observation": {
            "observation_id": "run-a:process-exit:123", "surface": {
                "kind": "process_exited", "facts": {"exit_code": 1}, "actions": []}}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "finish --outcome TEXT --verdict positive|negative|inconclusive")
        self.assertEqual(self.cli("call", "--request", str(path), ok=False)["error"], "game_process_exited")
        self.cli("journal", "--reason", "process exited during wait")
        self.assertEqual(len(self.requests()), 2)

    def test_inspect_is_exact_and_does_not_send_actions(self):
        request_id = self.observe()
        result = self.cli("inspect", "result.surface.facts.last_save_result")
        self.assertEqual(result["slice"], "unattempted")
        self.assertEqual(result["request_id"], request_id)
        self.assertEqual(len(self.requests()), 1)
        result = self.cli("inspect", "result.surface.actions", "--limit", "1")
        self.assertEqual(result["slice"][0]["id"], "world.wait")

    def test_inspect_default_output_is_plain_text_and_logged_without_new_input(self):
        self.observe()
        process = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                  "inspect", "result.surface.facts.last_save_result"],
                                 capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout.strip(),
                         "resolved selector: result.surface.facts.last_save_result\nunattempted")
        self.assertEqual(len(self.requests()), 1)
        self.assertIn("unattempted", (self.session / "playtest.txt").read_text())

    def test_plain_transcript_preserves_shell_argument_boundaries(self):
        import shlex
        reason = "finished O'Brien's inventory check"
        process = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                  "--wait-seconds", "0", "quit", "--reason", reason],
                                 capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        command = (self.session / "playtest.txt").read_text().splitlines()[0]
        self.assertEqual(shlex.split(command.removeprefix("> play ")),
                         ["quit", "--reason", reason])

    def test_macro_stop_reuses_only_explicitly_released_native_decision(self):
        for released in (False, True):
            with self.subTest(released=released):
                self.observe()
                path = self.session / "interrupted-wait.json"
                path.write_text(json.dumps({"action": "game.wait", "wait": {"enabled": True}}))
                pending = self.cli("call", "--request", str(path))
                self.reply(pending["request_id"], {"ok": False,
                    "error": "keep_watch_recipe_action_not_advertised", "result": {
                        "observation_id": "demand", "state": "interrupted",
                        "unused_authority": "released" if released else "revoked",
                        "session_state": "active", "terminal_observation": {
                            "observation_id": "demand", "continuation": {"state": "released"},
                            "surface": {"kind": "shakedown", "actions": [
                                {"id": "shakedown.fight", "enabled": True}]}}}})
                result = self.cli("collect", ok=False)
                if released:
                    self.assertNotEqual(result["next"], "look")
                    action = self.cli("act", "shakedown.fight")
                    sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
                    self.assertEqual(sent["request"]["observation_id"], "demand")
                else:
                    self.assertEqual(result["next"], "look")
                    self.assertIn("look_required", self.cli("act", "shakedown.fight", ok=False)["error"])

    def test_live_reporting_refusal_writes_neither_queue_nor_client_state(self):
        self.observe()
        before = (self.session / "play-client.json").read_bytes()
        requests = self.requests()
        witness = self.session / "witness.json"
        witness.write_text('{"verdict":"inconclusive"}')
        for args in (("journal", "--reason", "evidence"), ("finish", "--witness", str(witness))):
            result = self.cli(*args, ok=False)
            self.assertEqual(result["error"], "native_exit_required_before_reporting")
            self.assertEqual((self.session / "play-client.json").read_bytes(), before)
            self.assertEqual(self.requests(), requests)
        self.cli("act", "world.wait")
        self.assertEqual(len(self.requests()), len(requests) + 1)

    def test_optional_journal_preserves_sealed_terminal_for_detailed_review(self):
        self.observe(exited=True)
        witness_path = self.session / "witness.json"
        witness_path.write_text(json.dumps({"verdict": "inconclusive"}))
        pending = self.cli("journal", "--reason", "missing gameplay outcome")
        self.reply(pending["request_id"], {"ok": True, "result": {"evidence_journal": {"entries": []}}})
        self.cli("collect")
        self.cli("look", ok=False)
        self.cli("act", "world.wait", ok=False)
        finish = self.cli("finish", "--witness", str(witness_path))
        request = next(r["request"] for r in self.requests() if r["request_id"] == finish["request_id"])
        self.assertEqual(request, {"action": "run.finish", "observation_id": "frame-1",
                         "stop_reason": "missing gameplay outcome", "unused_authority": "released",
                         "witness": {"verdict": "inconclusive"}})
        self.reply(finish["request_id"], {"ok": True, "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished"}})
        self.cli("collect")
        self.cli("finish", "--witness", str(witness_path), ok=False)
        self.assertEqual(len(self.requests()), 3)

    def test_public_journal_and_finish_serialize_archived_replies(self):
        """The real CLI must cache lazy terminal evidence as references."""
        self.observe(exited=True)
        archive = Archive(self.session / "cockpit-evidence.sqlite", run_id="run-a", binding_id="bound-a")
        self.addCleanup(archive.close)

        journal_entries = archive.sequence()
        journal_entries.append({"citation_id": "J0001", "kind": "observation",
                                "value": {"game_minutes": 100}})
        journal = self.cli("journal", "--reason", "archived terminal evidence")
        self.reply_raw(journal["request_id"], json.dumps(archive.wire({
            "ok": True,
            "result": {"evidence_journal": {"entries": journal_entries}},
        })).encode())
        journal_result = self.cli("collect")
        self.assertTrue(journal_result["ok"])
        self.assertEqual(journal_result["response"]["result"]["evidence_journal"]["entries"][0]["citation_id"], "J0001")

        witness = self.session / "witness.json"
        witness.write_text(json.dumps({"verdict": "inconclusive"}))
        transcript = archive.sequence()
        transcript.append({"kind": "terminal", "observation_id": "frame-1"})
        finish = self.cli("finish", "--witness", str(witness))
        self.reply_raw(finish["request_id"], json.dumps(archive.wire({
            "ok": True,
            "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished",
                       "action_observation_sequence": transcript},
        })).encode())
        finish_result = self.cli("collect")
        self.assertTrue(finish_result["ok"])
        self.assertEqual(finish_result["state"], "finishing")
        self.assertTrue(finish_result["cleanup_pending"])
        self.assertEqual(finish_result["response"]["result"]["action_observation_sequence"][0]["kind"], "terminal")

        cached = json.loads((self.session / "play-client.json").read_text())["last_collected_result"]
        reference = cached["response"]["result"]["action_observation_sequence"]
        self.assertEqual(reference["schema"], "caol-archive-sequence-ref-v1")
        self.assertEqual(reference["count"], 1)

    def test_public_one_step_outcome_uses_actual_service_and_separates_cleanup(self):
        from cockpit import CockpitRunChannel, CockpitService
        from cockpit_witness_test import frame, CHARTER
        alive = True
        channel = CockpitRunChannel(frame, binding_id="bound-a", witness_charter=CHARTER,
            witness_identity={"scenario_id": "scenario-a", "source_identity": "source-a", "executable_identity": "exe-a"},
            read_process_state=lambda: {"run_id": "run-a", "pid": 123, "alive": alive})
        service = CockpitService(run_channel=channel)
        prior = service.call({"action": "game.observe"})["result"]
        alive = False
        pending = self.cli("look")
        self.reply(pending["request_id"], service.call({"action": "game.observe"}))
        self.cli("collect")
        invalid = self.cli("finish", "--outcome", "Observed outcome", "--cite", "J0004", ok=False)
        self.assertEqual(invalid["error"], "outcome_citation_requires_observation_id")
        self.assertIn("witness journal", invalid["next"])
        plain = subprocess.run([sys.executable, str(CLI), "--session", str(self.session),
                                "finish", "--outcome", "Observed outcome", "--cite", "J0004"],
                               capture_output=True, text=True)
        self.assertEqual(plain.returncode, 1, plain.stderr + plain.stdout)
        self.assertIn("witness journal", plain.stdout)
        self.assertEqual(len(self.requests()), 1)
        finish = self.cli("finish", "--outcome", "Fight achieved the selected gameplay result",
                          "--verdict", "positive", "--unknown", "Per-bandit participation unobserved",
                          "--cite", prior["observation_id"], "--select", "game_minutes")
        request = next(record["request"] for record in self.requests()
                       if record["request_id"] == finish["request_id"])
        self.assertNotIn("witness", request)
        self.reply(finish["request_id"], service.call(request))
        collected = self.cli("collect")
        self.assertEqual(collected["state"], "finishing")
        self.assertEqual(collected["gameplay_outcome"]["verdict"], "positive")
        self.assertEqual(collected["gameplay_outcome"]["citations"][0]["values"], {"game_minutes": 100})
        self.assertEqual(len(self.requests()), 2)  # No seal, reconstruct, replay or native action.
        for state, ok in (("terminalization_failed", False), ("safe_to_cleanup", True)):
            self.write("status.json", {"binding_id": "bound-a", "state": state,
                                       "cleanup": {"status": "accepted" if ok else "failed"}})
            result = self.cli("collect", ok=ok)
            self.assertEqual(result["gameplay_outcome"]["verdict"], "positive")
            self.assertEqual(result["gameplay_outcome"]["unknowns"], ["Per-bandit participation unobserved"])
            process = subprocess.run([sys.executable, str(CLI), "--session", str(self.session), "collect"],
                                     capture_output=True, text=True)
            self.assertIn("Gameplay positive: Fight achieved", process.stdout)
            self.assertIn("Unknown: Per-bandit", process.stdout)
        self.assertEqual(len(self.requests()), 2)

    def test_corrupt_or_wrong_binding_response_remains_pending(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "wrong"}}, "other")
        result = self.cli("collect", ok=False)
        self.assertEqual(result["error"], "response_binding_mismatch")
        self.cli("act", "world.wait", ok=False)
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "right"}})
        (self.session / "responses" / (pending["request_id"] + ".json")).write_text("{}")
        self.assertEqual(self.cli("collect", ok=False)["error"], "response_artifact_hash_mismatch")
        self.assertEqual(len(self.requests()), 1)

    def test_collect_recovers_and_verifies_archive_once_without_display_rescan(self):
        from play_cli import Bridge
        pending = self.cli("look")
        archive = Archive(self.session / "cockpit-evidence.sqlite", run_id="run-a", binding_id="bound-a")
        self.addCleanup(archive.close)
        entries = archive.sequence()
        for index in range(40):
            entries.append({"index": index, "detail": "exact retained evidence" * 500})
        wire = archive.wire({"ok": True, "result": {"action_observation_sequence": entries}})
        self.reply_raw(pending["request_id"], json.dumps(wire).encode())
        references = []
        original_reference = ArchiveSequence.reference
        def reference(sequence):
            references.append(sequence)
            return original_reference(sequence)
        original_recover = Bridge.response_artifact
        def recover(*args, **kwargs):
            recovered = original_recover(*args, **kwargs)
            if recovered.get("ok"):
                self.addCleanup(recovered["response"]["result"]["action_observation_sequence"].archive.close)
            return recovered
        with patch.object(Bridge, "response_artifact", side_effect=recover) as recovery, \
                patch.object(ArchiveSequence, "reference", reference):
            result = PlayerClient(self.session).collect()
            from cockpit_evidence import describe
            metadata = describe(result["response"]["result"]["action_observation_sequence"],
                                "result.action_observation_sequence")
        self.assertTrue(result["ok"])
        self.assertEqual(recovery.call_count, 1)
        self.assertEqual(len(references), 1)
        self.assertEqual(metadata["count"], 40)
        self.assertGreater(metadata["json_bytes"], 0)
        self.assertEqual(len(self.requests()), 1)
        # A later explicit recovery validates again; metadata is not a global
        # cache that can hide mutation of the retained source.
        with archive.connection:
            archive.connection.execute("UPDATE records SET value=? WHERE stream=? AND seq=0",
                                       (archive.encode({"changed": True}), entries.stream))
        rejected = Bridge.response_artifact(self.session, pending["request_id"],
                                            hashlib.sha256(json.dumps(wire).encode()).hexdigest())
        self.assertFalse(rejected["ok"])

    def test_diagnostics_failure_after_collect_keeps_committed_result_without_replay(self):
        from play_cli import main
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "frame-after", "surface": {"kind": "world", "facts": {}, "actions": []}}})
        with patch("play_cli.emit", side_effect=OSError("output unavailable")), \
                patch("sys.stderr", new_callable=io.StringIO) as errors:
            code = main(["--session", str(self.session), "--diagnostics", "collect"])
        self.assertEqual(code, 1)
        failure = json.loads(errors.getvalue())
        self.assertEqual(failure["error"], "presentation_failed")
        self.assertTrue(failure["collection_committed"])
        self.assertEqual(failure["request_id"], pending["request_id"])
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("pending", state)
        self.assertEqual(self.cli("collect")["request_id"], pending["request_id"])
        self.assertEqual(len(self.requests()), 1)

    def test_public_large_nested_diagnostics_inspect_preserves_exact_retrieval(self):
        pending = self.cli("look")
        detail = {str(index): {"facts": {"history": "雪\\\"" * 16000}, "accepted": True}
                  for index in range(40)}
        self.reply(pending["request_id"], {"ok": True, "result": {"diagnostics": detail}})
        self.cli("collect")
        inspected = self.cli("inspect", "result.diagnostics")
        self.assertEqual(inspected["slice"], detail)
        self.assertEqual(len(self.requests()), 1)

    def test_lock_prevents_concurrent_client_submission(self):
        with session_lock(self.session / "play-client.lock"):
            self.assertEqual(self.cli("look", ok=False)["error"], "another_play_client_is_active")
        self.assertEqual(self.requests(), [])
        self.cli("look")
        self.assertEqual(len(self.requests()), 1)

    def test_dead_bridge_returns_failure_without_resubmitting(self):
        pending = self.cli("look")
        for state in ("process_dead", "bridge_failed", "terminalization_failed"):
            self.write("status.json", {"binding_id": "bound-a", "state": state,
                                       "reason": "native owner stopped"})
            started = time.monotonic()
            result = self.cli("collect", ok=False, wait_seconds=10)
            self.assertLess(time.monotonic() - started, 2)
            self.assertEqual(result["error"], "bridge_ended_before_response")
            self.assertEqual(result["reason"], "native owner stopped")
            self.assertEqual(result["request_id"], pending["request_id"])
            self.assertIn("child.stderr.log", result["log_path"])
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "child_exit_code": 0})
        self.assertEqual(self.cli("collect", ok=False)["state"], "session_ended_without_response")
        self.cli("look", ok=False)
        self.assertEqual(len(self.requests()), 1)

    def test_single_execution_wait_collects_delayed_response_and_retains_receipt(self):
        process = self.cli_async("look", wait_seconds=2)
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline and not self.requests():
            time.sleep(0.01)
        requests = self.requests()
        self.assertEqual(len(requests), 1)
        request_id = requests[0]["request_id"]
        self.reply(request_id, {"ok": True, "result": {"observation_id": "delayed-frame"}})
        stdout, stderr = process.communicate(timeout=2)
        self.assertEqual(process.returncode, 0, stderr)
        from evidence_display import recover
        shown = json.loads(stdout)
        result = recover(shown["presentation"]["full_evidence"]["sha256"]) if "presentation" in shown else shown
        self.assertEqual(result["request_id"], request_id)
        self.assertEqual(result["state"], "collected")
        self.assertEqual(len(self.requests()), 1)
        receipt = json.loads((self.session / "responses" / (request_id + ".receipt.json")).read_text())
        self.assertEqual(receipt["request_id"], request_id)

    def test_one_collect_returns_alarm_observed_on_an_earlier_internal_poll(self):
        self.write("play-client.json", {"binding_id": "bound-a", "pending": {
            "request_id": "pending-1", "request": {"action": "game.observe"},
            "submitted_unix_seconds": 10.0,
        }})
        client = PlayerClient(self.session)
        missing = {"ok": False, "error": "response_not_available_or_stale"}
        receipt = {"ok": True, "receipt": {"binding_id": "bound-a", "session_generation": 0,
                                                "response_sha256": "ignored"}}
        response = {"ok": True, "response": {"ok": True, "result": {
            "observation_id": "frame-1", "surface": {"kind": "world", "actions": []},
        }}}
        first_assessment = {"status": "measured", "alarms": [{"kind": "spike", "key": "spike:1"}],
                            "recoveries": []}
        final_assessment = {"status": "measured", "alarms": [],
                            "recoveries": [{"kind": "recovery", "recovered_alarm": "spike:1"}]}
        with patch("play_cli.Bridge.response_status", side_effect=[missing, receipt]), \
                patch("play_cli.Bridge.response_artifact", return_value=response), \
                patch("process_performance.collect_turn_assessment",
                      side_effect=[first_assessment, final_assessment]), \
                patch("play_cli.time.sleep"):
            before = json.loads((self.session / "play-client.json").read_text())
            with patch("play_cli.time.sleep") as sleep:
                result = client.collect(wait_seconds=60)
                sleep.assert_not_called()
            self.assertEqual(result["state"], "pending")
            self.assertTrue(result["performance_alarm"])
            self.assertEqual(result["request_id"], "pending-1")
            self.assertEqual(client.state["pending"]["request_id"], "pending-1")
            self.assertEqual(json.loads((self.session / "play-client.json").read_text()), before)
            self.assertEqual(result["turn_assessment"]["alarms"], first_assessment["alarms"])
            self.assertEqual(len(self.requests()), 0)
            completed = client.collect(wait_seconds=60)
        self.assertEqual(completed["state"], "collected")
        self.assertIsNone(client.state.get("pending"))
        self.assertEqual(completed["turn_assessment"]["recoveries"], final_assessment["recoveries"])
        self.assertEqual(len(self.requests()), 0)

    def test_collect_wakes_on_cancellation_without_resubmitting(self):
        pending = self.cli("look")
        (self.session / "controls").mkdir()
        self.write("active-request.json", {"request_id": pending["request_id"],
                                            "binding_id": "bound-a", "run_id": "run-a"})
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                    "inflight_request_id": pending["request_id"],
                                    "session_descriptor": {"run_id": "run-a"}})
        cancelled = self.cli("cancel", "--reason", "stop delayed request")
        self.assertEqual(cancelled["request_id"], pending["request_id"])
        markers = list((self.session / "controls").glob("cancel-*.json"))
        self.assertEqual(len(markers), 1)
        marker = json.loads(markers[0].read_text())
        self.assertEqual(marker["request_id"], pending["request_id"])
        self.assertEqual(marker["binding_id"], "bound-a")
        self.assertEqual(marker["schema"], "caol-cockpit-cancel-v1")
        result = self.cli("collect", wait_seconds=10)
        self.assertEqual(result["state"], "cancellation_requested")
        self.assertEqual(result["request_id"], pending["request_id"])
        self.assertEqual(len(self.requests()), 1)

    def test_collect_deadline_preserves_single_pending_request(self):
        pending = self.cli("look", wait_seconds=0.01)
        self.assertEqual(pending["state"], "pending")
        self.assertEqual(pending["request_id"], self.cli("collect")["request_id"])
        self.assertEqual(len(self.requests()), 1)

    def test_duplicate_collect_returns_the_same_result_without_another_submission(self):
        request_id = self.observe()
        first = self.cli("collect")
        repeated = self.cli("collect")
        explicit = self.cli("collect", "--request-id", request_id)
        self.assertEqual(repeated, first)
        self.assertEqual(explicit, first)
        self.assertEqual(len(self.requests()), 1)

    def test_resume_rebuilds_lost_client_state_only_while_the_same_request_is_outstanding(self):
        pending = self.cli("look")
        (self.session / "play-client.json").unlink()
        resumed = self.cli("resume", "--request-id", pending["request_id"])
        self.assertEqual(resumed["state"], "pending")
        self.assertEqual(resumed["recovered_request"], pending["request_id"])
        self.assertEqual(len(self.requests()), 1)
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "recovered-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        action = self.cli("act", "world.pause")
        sent = next(row for row in self.requests() if row["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "recovered-frame")
        self.assertEqual(len(self.requests()), 2)

    def test_older_collected_request_is_read_only_after_newer_collection(self):
        request_a = self.observe("frame-a")
        request_b = self.cli("act", "world.wait")
        self.reply(request_b["request_id"], {"ok": True, "result": {
            "observation_id": "frame-b", "surface": {
                "kind": "world", "facts": {"turn": 2},
                "actions": [{"id": "world.pause", "enabled": True}]}}})
        self.cli("collect")
        before = (self.session / "play-client.json").read_bytes()
        count = len(self.requests())
        collected = self.cli("collect", "--request-id", request_a, ok=False)
        resumed = self.cli("resume", "--request-id", request_a, ok=False)
        self.assertEqual(collected["error"], "request_recovery_superseded")
        self.assertEqual(resumed["error"], "request_recovery_superseded")
        self.assertEqual(collected["retrieval"]["request_result"],
                         f"request-result --request-id {request_a}")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(len(self.requests()), count)
        readonly = self.cli("request-result", "--request-id", request_a)
        self.assertTrue(readonly["ok"])
        action = self.cli("act", "world.pause")
        sent = next(row for row in self.requests() if row["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "frame-b")

    def test_lost_state_does_not_reclassify_an_already_recorded_response_as_pending(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "recorded-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        (self.session / "play-client.json").unlink()
        result = self.cli("resume", "--request-id", pending["request_id"], ok=False)
        self.assertEqual(result["error"], "request_already_recorded_read_only")
        self.assertFalse((self.session / "play-client.json").exists())
        self.assertEqual(len(self.requests()), 1)

    def test_safe_read_only_retrieval_does_not_wait_for_the_collect_lock(self):
        request_id = self.observe()
        self.write("play-client.json", {**json.loads((self.session / "play-client.json").read_text()),
                   "pending": {"request_id": "still-pending", "request": {"action": "game.wait"}}})
        with session_lock(self.session / "play-client.lock"):
            result = self.cli("inspect", "result.surface.actions", "--request-id", request_id,
                              "--limit", "1")
        self.assertEqual(result["slice"], [{"id": "world.wait", "enabled": True}])
        self.assertEqual(len(self.requests()), 1)

    def test_messages_remain_readable_from_the_last_displayed_frame_while_action_collects(self):
        looked = self.cli("look")
        self.reply(looked["request_id"], {"ok": True, "result": {
            "observation_id": "message-frame", "surface": {"kind": "world", "facts": {
                "messages": json.dumps([{"text": "current retained message", "time": 1}])},
                "actions": [{"id": "world.wait", "enabled": True}]}}})
        self.cli("collect")
        pending = self.cli("act", "world.wait")
        self.assertEqual(pending["state"], "pending")
        with session_lock(self.session / "play-client.lock"):
            messages = self.cli("messages", "--contains", "retained", "--limit", "1")
        self.assertEqual(messages["slice"], [{"text": "current retained message", "time": 1}])
        self.assertEqual(len(self.requests()), 2)

    def test_direct_wait_and_move_commands_preserve_player_chosen_bounds_without_request_files(self):
        wait = self.cli("wait", "--target-delta-game-minutes", "5", "--duration-action", "wait.5m",
                        "--bound-maximum", "5", "--bound-basis", "scheduler_boundary",
                        "--bound-source", "chosen observation window")
        sent_wait = next(row for row in self.requests() if row["request_id"] == wait["request_id"])
        self.assertEqual(sent_wait["request"], {"action": "game.wait", "wait": {
            "enabled": True, "recipe": ["world.wait", "wait.5m"],
            "target_delta_game_minutes": 5.0, "danger_handling": "stop_on_interruption",
            "bound": {"basis": "scheduler_boundary", "source": "chosen observation window",
                      "unit": "game_minutes", "maximum": 5.0, "progress_required": True},
        }})
        self.reply(wait["request_id"], {"ok": False, "error": "interrupted"})
        rejected = self.cli("collect", ok=False)
        self.assertEqual(self.cli("collect", "--request-id", wait["request_id"], ok=False), rejected)
        self.assertEqual(len(self.requests()), 1)
        move = self.cli("move", "--east", "1", "--south", "-1", "--bound-maximum", "2",
                        "--bound-basis", "path_progress", "--bound-source", "chosen diagonal")
        sent_move = next(row for row in self.requests() if row["request_id"] == move["request_id"])
        self.assertEqual(sent_move["request"], {"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, -1], "danger_handling": "stop_on_interruption",
            "bound": {"basis": "path_progress", "source": "chosen diagonal",
                      "unit": "steps", "maximum": 2},
        }})

    def test_result_frame_reuse_sends_next_action_without_redundant_look(self):
        observe_request_id = self.observe()
        first = self.cli("act", "world.wait")
        self.reply(first["request_id"], {"ok": True, "result": {
            "observation_id": "successor-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        second = self.cli("act", "world.pause")
        sent = next(item for item in self.requests() if item["request_id"] == second["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "successor-frame")
        # Request filenames contain random UUIDs, so glob order is not the
        # submission order. Use the authoritative IDs returned by each
        # submission to keep this assertion deterministic.
        request_by_id = {item["request_id"]: item for item in self.requests()}
        self.assertEqual([request_by_id[request_id]["request"]["action"]
                          for request_id in (observe_request_id, first["request_id"], second["request_id"])],
                         ["game.observe", "game.act", "game.act"])

    def test_reuse_matches_redundant_look_outcome_with_one_fewer_observe(self):
        def reset_session():
            for directory in (self.session / "requests", self.session / "responses"):
                for path in directory.glob("*"):
                    path.unlink()
            state = self.session / "play-client.json"
            if state.exists():
                state.unlink()
            self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                        "session_generation": 0})

        def run(redundant_look):
            self.observe()
            first = self.cli("act", "world.wait")
            successor = {"observation_id": "same-successor", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}
            self.reply(first["request_id"], {"ok": True, "result": successor})
            self.cli("collect")
            if redundant_look:
                refresh = self.cli("look")
                self.reply(refresh["request_id"], {"ok": True, "result": successor})
                self.cli("collect")
            second = self.cli("act", "world.pause")
            self.reply(second["request_id"], {"ok": True, "result": {
                "observation_id": "final-frame", "surface": {
                    "kind": "world", "actions": []}}})
            final = self.cli("collect")
            return final, len(self.requests())

        reused, reused_requests = run(False)
        reset_session()
        redundant, redundant_requests = run(True)
        self.assertEqual(reused["state"], redundant["state"])
        self.assertEqual(reused["response"]["current_input"], redundant["response"]["current_input"])
        self.assertEqual(reused_requests, 3)
        self.assertEqual(redundant_requests, 4)

    def test_factless_action_result_requires_refresh_before_reuse(self):
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {"observation_id": "factless"}})
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")
        self.assertEqual(len(self.requests()), 2)

    def test_stale_generation_and_binding_invalidate_result_frame(self):
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {
            "observation_id": "old-generation", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        receipt_path = self.session / "responses" / (action["request_id"] + ".receipt.json")
        receipt = json.loads(receipt_path.read_text())
        receipt["session_generation"] = 99
        receipt_path.write_text(json.dumps(receipt))
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")
        self.assertEqual(len(self.requests()), 2)

        self.write("play-client.json", {"binding_id": "bound-a", "observation_id": "bound-frame",
                                         "observation_generation": 0, "observation_binding_id": "other",
                                         "observation_owner": "world"})
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: current observation authority is stale")

        self.write("status.json", {"binding_id": "other-binding", "state": "ready",
                                    "session_generation": 0})
        self.write("play-client.json", {"binding_id": "bound-a", "observation_id": "bound-frame",
                                         "observation_generation": 0, "observation_binding_id": "bound-a",
                                         "observation_owner": "world"})
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: current observation authority is stale")

        self.write("status.json", {"binding_id": "bound-a", "state": "ready",
                                    "session_generation": 0})
        self.write("play-client.json", {"binding_id": "bound-a"})
        self.observe()
        action = self.cli("act", "world.wait")
        self.reply(action["request_id"], {"ok": True, "result": {
            "observation_id": "binding-drift-frame", "surface": {
                "kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.write("status.json", {"binding_id": "other-binding", "state": "ready",
                                    "session_generation": 0})
        self.cli("collect")
        self.assertEqual(self.cli("act", "world.pause", ok=False)["error"],
                         "look_required: no current unconsumed observation")

    def test_error_final_never_turns_an_action_failure_into_client_quit(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "owner_lost",
                    "final": {"schema": "caol-cockpit-live-final-v1", "state": "finished"}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["next"], "look")
        self.assertFalse(json.loads((self.session / "play-client.json").read_text()).get("finished"))
        self.assertEqual(self.cli("look")["state"], "pending")
        self.assertEqual(len(self.requests()), 3)

    def test_quit_requires_explicit_abort_flag_in_public_request(self):
        default = self.cli("quit", "--reason", "graceful close")
        request = self.requests()[-1]["request"]
        self.assertFalse(request["abort"])
        self.reply(default["request_id"], {"ok": False, "error": "native_graceful_close_required"})
        self.cli("collect", ok=False)
        explicit = self.cli("quit", "--abort", "--reason", "owner authorized abort")
        self.assertTrue(next(record for record in self.requests()
                             if record["request_id"] == explicit["request_id"])["request"]["abort"])
        self.reply(explicit["request_id"], {"ok": True, "result": {"schema": "caol-cockpit-live-final-v1", "state": "finished", "closure_kind": "explicit_abort", "native_exit_credit": False}})
        self.cli("collect")

    def test_explicit_quit_works_without_a_frame_and_after_journal_sealing(self):
        for structured in (False, True):
            with self.subTest(structured=structured):
                self.write("play-client.json", {"binding_id": "bound-a", "sealed_terminal": {"observation_id": "old"}})
                if structured:
                    path = self.session / "quit.json"
                    path.write_text(json.dumps({"action": "run.quit", "stop_reason": "player chooses to stop"}))
                    pending = self.cli("call", "--request", str(path))
                else:
                    pending = self.cli("quit", "--reason", "player chooses to stop")
                self.reply(pending["request_id"], {"ok": True, "result": {
                    "schema": "caol-cockpit-live-final-v1", "state": "finished"}})
                self.assertEqual(self.cli("collect")["next"], "collect")
                self.assertTrue(json.loads((self.session / "play-client.json").read_text())["finished"])

    def test_quit_ack_without_terminal_receipt_keeps_client_recoverable(self):
        self.observe()
        pending = self.cli("quit", "--reason", "stop")
        self.reply(pending["request_id"], {"ok": True})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "unconfirmed_terminal_response")
        self.assertFalse(json.loads((self.session / "play-client.json").read_text()).get("finished"))
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_abort_pending_keeps_client_live_and_requires_fresh_observation(self):
        self.observe()
        pending = self.cli("quit", "--abort", "--reason", "authorized abort")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "schema": "caol-cockpit-abort-pending-v1", "state": "exit_observation_pending",
            "cleanup": {"status": "graceful_quit_unconfirmed_process_retained"}}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "abort_exit_observation_pending")
        self.assertEqual(result["next"], "look")
        state = json.loads((self.session / "play-client.json").read_text())
        self.assertFalse(state.get("finished"))
        self.assertNotIn("observation_id", state)
        self.assertEqual(self.cli("look")["state"], "pending")

    def test_refused_abort_offers_explicit_retry_without_reissuing_it(self):
        self.observe()
        pending = self.cli("quit", "--abort", "--reason", "first attempt")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "schema": "caol-cockpit-abort-pending-v1", "state": "exit_observation_pending",
            "cleanup": {"status": "retained_explicit_abort_refused"}, "retry_allowed": True}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "abort_exit_observation_pending")
        self.assertIn("explicit quit --abort retry", result["next"])
        self.assertEqual(len(self.requests()), 2)
        retry = self.cli("quit", "--abort", "--reason", "explicit retry")
        self.assertEqual(retry["state"], "pending")
        self.assertEqual(len(self.requests()), 3)

    def test_declared_reentry_releases_old_grants_before_new_observation(self):
        self.write("play-client.json", {"binding_id": "bound-a",
                   "session_generation": 0, "process_exited": True,
                   "sealed_terminal": {"observation_id": "old"}, "observation_id": "old",
                   "last_request_id": "old-request", "last_collected_result": {"request_id": "old-request"},
                   "display_sha256": "0" * 64, "display_generation": 0})
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "session_generation": 0})
        self.cli("look", ok=False)
        self.write("status.json", {"binding_id": "bound-a", "state": "ready", "session_generation": 1})
        self.assertEqual(self.cli("collect")["state"], "reentered")
        reentered = json.loads((self.session / "play-client.json").read_text())
        self.assertNotIn("last_request_id", reentered)
        self.assertNotIn("display_sha256", reentered)
        self.assertEqual(self.cli("collect", "--request-id", "old-request", ok=False)["error"],
                         "request_recovery_superseded_by_reentry")
        self.assertIn("look_required", self.cli("act", "world.pause", ok=False)["error"])
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "new-process:frame:1",
            "surface": {"kind": "world", "actions": [{"id": "world.pause"}]}}})
        self.cli("collect")
        action = self.cli("act", "world.pause")
        sent = next(r for r in self.requests() if r["request_id"] == action["request_id"])
        self.assertEqual(sent["request"]["observation_id"], "new-process:frame:1")

    def test_messages_reads_typed_native_text_from_the_displayed_frame_without_input(self):
        for direct in (False, True):
            with self.subTest(direct=direct):
                self.write("play-client.json", {"binding_id": "bound-a"})
                pending = self.cli("look")
                observation = {"observation_id": "reply-frame", "surface": {"kind": "world", "actions": [], "facts": {
                    "messages": json.dumps([{"text": "old fixture"}, {"text": "Earlier rejection"},
                                            {"text": 'Katharina says: "Done."', "time": "16:00"}])}}}
                self.reply(pending["request_id"], {"ok": True, "observation" if direct else "result": observation})
                self.cli("collect")
                before = (self.session / "play-client.json").read_bytes()
                count = len(self.requests())
                result = self.cli("messages", "--contains", "Katharina", "--limit", "1")
                self.assertEqual(result["slice"], [{"text": 'Katharina says: "Done."', "time": "16:00"}])
                self.assertEqual(result["observation_id"], "reply-frame")
                self.assertEqual(result["source_indices"], [2])
                self.assertEqual((self.session / "play-client.json").read_bytes(), before)
                self.assertEqual(len(self.requests()), count)

    def test_terminal_process_view_offers_journal_without_another_native_action(self):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {"observation_id": "run:process-exit:123",
                   "surface": {"kind": "process_exited", "facts": {"exit_code": 0}, "actions": []}}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "process_exited")
        self.assertEqual(result["next"], "finish --outcome TEXT --verdict positive|negative|inconclusive")
        self.cli("act", "world.move.north", ok=False)
        self.assertEqual(len(self.requests()), 1)
        self.cli("journal", "--reason", "process exited")
        self.assertEqual(len(self.requests()), 2)

    def test_finished_collect_reports_actual_cleanup_instead_of_inventing_termination(self):
        self.write("play-client.json", {"binding_id": "bound-a", "finished": True})
        self.write("status.json", {"binding_id": "bound-a", "state": "terminalizing"})
        before = (self.session / "play-client.json").read_bytes()
        self.assertEqual(self.cli("collect", "--request-id", "old-request", ok=False)["error"],
                         "request_recovery_session_finished")
        self.assertEqual((self.session / "play-client.json").read_bytes(), before)
        self.assertEqual(self.cli("collect")["state"], "finishing")
        self.write("status.json", {"binding_id": "bound-a", "state": "safe_to_cleanup",
                   "cleanup": {"status": "accepted"}, "terminalization": {
                       "cleanup": {"status": "already_exited", "native_exit_credit": True}}})
        result = self.cli("collect")
        self.assertEqual(result["state"], "finished")
        self.assertEqual(result["cleanup"]["status"], "already_exited")
        self.write("status.json", {"binding_id": "bound-a", "state": "terminalization_failed",
                                   "reason": "owned cleanup failed"})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "cleanup_failed")
        self.assertEqual(result["reason"], "owned cleanup failed")
        self.assertEqual(self.requests(), [])
        self.write("status.json", {"binding_id": "bound-a", "state": "reentry_failed",
                   "reason": "missing_cockpit_session_descriptor",
                   "admission": "closed_until_explicit_cleanup",
                   "reentry_failure": {"replacement": {"status": "retained", "game_process": {"status": "alive", "pid": 96450}}}})
        result = self.cli("collect", ok=False)
        self.assertEqual(result["state"], "cleanup_failed")
        self.assertEqual(result["bridge_state"], "reentry_failed")
        self.assertEqual(result["next"], "inspect retained evidence")
        self.assertEqual(result["reentry_failure"]["replacement"]["status"], "retained")

    def test_rejected_action_without_successor_requires_new_look(self):
        self.observe()
        pending = self.cli("act", "world.wait")
        self.reply(pending["request_id"], {"ok": False, "error": "stale_frame"})
        result = self.cli("collect", ok=False)
        self.assertFalse(result["response"]["ok"])
        self.assertEqual(result["next"], "look")
        self.cli("act", "world.wait", ok=False)
        self.cli("look")
        self.assertEqual(len(self.requests()), 3)

    @staticmethod
    def repeat_surface(action, *, owner="world", moves=1000, hp=100):
        return {"kind": owner, "facts": {
            "avatar_moves": moves,
            "avatar_status": {"health": {"body_parts": {"torso": {"current": hp}}}},
        }, "actions": [{"id": action, "stable_id": "", "enabled": True}] if owner == "world" else []}

    def repeat_look(self, action="world.autoattack"):
        pending = self.cli("look")
        self.reply(pending["request_id"], {"ok": True, "result": {
            "observation_id": "repeat-frame-0", "run_id": "repeat-run", "game_turn": 100,
            "game_minutes": 10, "avatar_moves": 1000,
            "surface": self.repeat_surface(action)}})
        self.cli("collect")

    def repeat_reply(self, request_id, action="world.autoattack", *, step=1,
                     owner="world", turn=None, moves=None, hp=100, accepted=True):
        self.reply(request_id, {"ok": accepted,
            "receipt": {"native_receipt": {"action_id": action, "accepted": accepted}},
            "observation": {"observation_id": f"repeat-frame-{step}",
                "run_id": "repeat-run", "game_turn": 100 + step if turn is None else turn,
                "game_minutes": 10, "avatar_moves": 1000 if moves is None else moves,
                "surface": self.repeat_surface(action, owner=owner,
                                               moves=1000 if moves is None else moves, hp=hp)}})

    def test_repeat_validates_action_and_count_before_any_request(self):
        for args in (("world.wait", "--count", "10"), ("world.pause", "--count", "0"),
                     ("world.autoattack", "--count", "-1"), ("world.pause",)):
            self.assertEqual(self.cli("repeat", *args, ok=False)["error"],
                             "repeat_requires_pause_or_autoattack_and_positive_count")
        self.assertEqual(self.requests(), [])

    def test_repeat_ten_ordered_autoattacks_collects_each_exact_receipt(self):
        self.repeat_look()
        result = self.cli("repeat", "world.autoattack", "--count", "10")
        ids = []
        for step in range(1, 11):
            self.assertEqual(result["state"], "pending")
            request_id = result["outstanding_request_id"]
            ids.append(request_id)
            requests = [item for item in self.requests() if item["request"].get("action") == "game.act"]
            self.assertEqual(len(requests), step)
            sent = next(item for item in requests if item["request_id"] == request_id)
            self.assertEqual(sent["request"]["action_id"], "world.autoattack")
            self.assertEqual(sent["request"]["observation_id"], f"repeat-frame-{step-1}")
            self.repeat_reply(request_id, step=step)
            result = self.cli("repeat", "--resume")
        self.assertEqual(result["state"], "completed")
        self.assertEqual(result["completed_count"], 10)
        self.assertEqual(result["unused_count"], 0)
        self.assertEqual(result["first_native_turn"], 100)
        self.assertEqual(result["last_native_turn"], 110)
        self.assertEqual([item["request_id"] for item in result["receipt_handles"]], ids)
        self.assertEqual(len(set(ids)), 10)

    def test_repeat_short_action_progresses_on_moves_without_a_new_minute_or_turn(self):
        self.repeat_look("world.pause")
        pending = self.cli("repeat", "world.pause", "--count", "2")
        self.repeat_reply(pending["outstanding_request_id"], "world.pause", step=1,
                          turn=100, moves=900)
        next_step = self.cli("repeat", "--resume")
        self.assertEqual(next_step["completed_count"], 1)
        self.assertEqual(next_step["state"], "pending")
        self.repeat_reply(next_step["outstanding_request_id"], "world.pause", step=2,
                          turn=100, moves=800)
        complete = self.cli("repeat", "--resume")
        self.assertEqual(complete["completed_count"], 2)
        self.assertEqual(complete["state"], "completed")

    def test_repeat_stops_on_menu_change_harm_and_no_effect(self):
        for owner, hp, turn, moves, state, reason in (
                ("menu", 100, 101, 1000, "interrupted", "owner_changed"),
                ("world", 90, 101, 1000, "interrupted", "avatar_harm"),
                ("world", 100, 100, 1000, "failed", "native_action_progress_unproved")):
            with self.subTest(reason=reason):
                self.repeat_look()
                pending = self.cli("repeat", "world.autoattack", "--count", "10")
                before_count = len(self.requests())
                self.repeat_reply(pending["outstanding_request_id"], owner=owner, hp=hp,
                                  turn=turn, moves=moves)
                stopped = self.cli("repeat", "--resume", ok=state != "failed")
                self.assertEqual(stopped["state"], state)
                self.assertEqual(stopped["stop_reason"], reason)
                self.assertEqual(len(self.requests()), before_count)
                # Clear this test's retained operation before the next subcase.
                self.state_path = self.session / "play-client.json"
                self.state_path.unlink()

    def test_repeat_pending_collect_and_process_restart_do_not_duplicate_input(self):
        self.repeat_look()
        pending = self.cli("repeat", "world.autoattack", "--count", "2")
        request_id = pending["outstanding_request_id"]
        durable = json.loads((self.session / "play-client.json").read_text())
        self.assertEqual(durable["repeat"]["requested_count"], 2)
        self.assertEqual(durable["repeat"]["completed_count"], 0)
        self.assertEqual(durable["repeat"]["outstanding"]["request_id"], request_id)
        self.assertEqual(self.cli("repeat", "world.autoattack", "--count", "2", ok=False)["error"],
                         "repeat_in_progress_use_repeat_resume_or_abort")
        still_pending = self.cli("repeat", "--resume")
        self.assertEqual(still_pending["outstanding_request_id"], request_id)
        self.assertEqual(len([item for item in self.requests() if item["request"].get("action") == "game.act"]), 1)
        self.repeat_reply(request_id, step=1)
        self.cli("collect", "--request-id", request_id)
        restarted = self.cli("repeat", "--resume")
        self.assertEqual(restarted["completed_count"], 1)
        self.assertEqual(len([item for item in self.requests() if item["request"].get("action") == "game.act"]), 2)
        self.repeat_reply(restarted["outstanding_request_id"], step=2)
        complete = self.cli("repeat", "--resume")
        self.assertEqual(complete["state"], "completed")
        self.assertEqual(complete["completed_count"], 2)

    def test_collected_repeat_then_abort_retires_without_more_native_input(self):
        for progressed in (True, False):
            with self.subTest(progressed=progressed):
                self.repeat_look()
                pending = self.cli("repeat", "world.autoattack", "--count", "3")
                action_id = pending["outstanding_request_id"]
                self.assertEqual(self.cli("quit", "--abort", ok=False)["error"], "request_in_flight")
                self.repeat_reply(action_id, step=1, turn=101 if progressed else 100)
                collected = self.cli("collect", "--request-id", action_id)
                response_hash = collected["receipt"]["response_sha256"]
                quit_request = self.cli("quit", "--abort", "--reason", "authorized abort")
                self.reply(quit_request["request_id"], {"ok": True, "result": {
                    "schema": "caol-cockpit-abort-pending-v1", "state": "exit_observation_pending",
                    "cleanup": {"status": "explicit_abort_exit_unobserved_process_retained"},
                    "native_exit_credit": False, "native_save_credit": False}})
                self.assertEqual(self.cli("collect")["state"], "abort_exit_observation_pending")
                repeat = json.loads((self.session / "play-client.json").read_text())["repeat"]
                self.assertEqual(repeat["status"], "interrupted" if progressed else "failed")
                self.assertEqual(repeat["completed_count"], int(progressed))
                self.assertEqual(repeat["requested_count"] - repeat["completed_count"], 3 - int(progressed))
                self.assertIsNone(repeat["outstanding"])
                self.assertEqual(repeat["receipt_handles"], [{"request_id": action_id,
                    "response_sha256": response_hash, "native_action_id": "world.autoattack",
                    "native_accepted": True}])
                resumed = self.cli("repeat", "--resume", ok=progressed)
                self.assertEqual(resumed["completed_count"], int(progressed))
                self.assertEqual(len([r for r in self.requests() if r["request"].get("action") == "game.act"]), 1)
                look = self.cli("look")
                self.reply(look["request_id"], {"ok": True, "result": {"observation_id": "process-exit",
                    "run_id": "repeat-run", "surface": {"kind": "process_exited", "facts": {}, "actions": []}}})
                self.assertEqual(self.cli("collect")["state"], "process_exited")
                report = self.cli("journal", "--reason", "observed exit")
                self.assertEqual(report["state"], "pending")
                self.assertEqual(next(r for r in self.requests() if r["request_id"] == report["request_id"])
                                 ["request"]["action"], "run.witness")
                # Fresh independent fixture state for the next subcase.
                for path in self.session.glob("requests/*.json"):
                    path.unlink()
                (self.session / "play-client.json").unlink()

    def test_rejected_abort_keeps_settled_repeat_resumable(self):
        self.repeat_look()
        pending = self.cli("repeat", "world.autoattack", "--count", "2")
        action_id = pending["outstanding_request_id"]
        self.repeat_reply(action_id, step=1)
        self.cli("collect", "--request-id", action_id)
        quit_request = self.cli("quit", "--abort")
        self.reply(quit_request["request_id"], {"ok": False, "error": "binding_drift"})
        self.cli("collect", ok=False)
        resumed = self.cli("repeat", "--resume")
        self.assertEqual(resumed["completed_count"], 1)
        self.assertEqual(resumed["state"], "pending")
        self.assertNotEqual(resumed["outstanding_request_id"], action_id)
        self.assertEqual(len([r for r in self.requests() if r["request"].get("action") == "game.act"]), 1)
        observation_id = json.loads((self.session / "play-client.json").read_text())["pending"]["request_id"]
        self.reply(observation_id, {"ok": True, "result": {
            "observation_id": "repeat-frame-1-fresh", "run_id": "repeat-run", "game_turn": 101,
            "game_minutes": 10, "avatar_moves": 1000, "surface": self.repeat_surface("world.autoattack")}})
        next_step = self.cli("repeat", "--resume")
        self.assertEqual(next_step["completed_count"], 1)
        self.assertEqual(len([r for r in self.requests() if r["request"].get("action") == "game.act"]), 2)

    def test_repeat_cancelled_request_stops_without_another_press(self):
        self.repeat_look()
        pending = self.cli("repeat", "world.autoattack", "--count", "10")
        request_id = pending["outstanding_request_id"]
        (self.session / "controls").mkdir()
        self.write("status.json", {"binding_id": "bound-a", "state": "awaiting_response",
                                   "inflight_request_id": request_id, "session_generation": 0,
                                   "session_descriptor": {"run_id": "repeat-run"}})
        self.write("active-request.json", {"request_id": request_id, "binding_id": "bound-a",
                                            "run_id": "repeat-run", "session_generation": 0})
        self.cli("cancel")
        self.reply(request_id, {"ok": False, "error": "player_cancelled",
            "failure": {"unused_authority": "revoked"},
            "receipt": {"native_receipt": {"action_id": "world.autoattack", "accepted": False}}})
        stopped = self.cli("repeat", "--resume")
        self.assertEqual(stopped["state"], "interrupted")
        self.assertEqual(stopped["completed_count"], 0)
        self.assertEqual(len([item for item in self.requests() if item["request"].get("action") == "game.act"]), 1)

    def test_repeat_rejection_and_stale_binding_cannot_issue_successor(self):
        self.repeat_look()
        pending = self.cli("repeat", "world.autoattack", "--count", "3")
        request_id = pending["outstanding_request_id"]
        self.repeat_reply(request_id, accepted=False)
        stopped = self.cli("repeat", "--resume")
        self.assertEqual(stopped["state"], "interrupted")
        self.assertEqual(stopped["completed_count"], 0)
        self.assertEqual(len([item for item in self.requests() if item["request"].get("action") == "game.act"]), 1)
        (self.session / "play-client.json").unlink()
        self.repeat_look()
        self.write("status.json", {"binding_id": "other", "state": "ready"})
        stale = self.cli("repeat", "world.autoattack", "--count", "3", ok=False)
        self.assertEqual(stale["stop_reason"], "stale_world_authority")
        self.assertEqual(len([item for item in self.requests() if item["request"].get("action") == "game.act"]), 1)


if __name__ == "__main__":
    unittest.main()
