"""Real file-backed client routing; responses are explicit game fixtures."""
import json
import io
import os
import subprocess
import sys
import unittest
from unittest.mock import patch

from plain_waiting import WaitingPlayer, performance_text
from play_cli import PlayerClient
from cockpit_file_bridge import FileBackedCockpitBridge
import play_cli_test as fixtures


class PlainWaitingTest(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.PlayerCliTest()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.client = PlayerClient(self.fixture.session, plain_waiting=True)
        self.player = WaitingPlayer(self.client)

    def reply(self, kind, actions, minute=0, facts=None):
        request_id = self.client.state["pending"]["request_id"]
        self.fixture.reply(request_id, {"ok": True, "observation": {
            "observation_id": request_id, "run_id": "run-a", "game_minutes": minute,
            "surface": {"kind": kind, "actions": actions, "facts": facts or {}}}})
        return self.player.run("look")

    def world(self):
        self.player.run("look")
        return self.reply("world", [{"id": "world.wait", "enabled": True}])

    def test_wait_menu_pending_and_current_choices(self):
        with patch("play_cli.retain", side_effect=AssertionError("Plain play must not archive display snapshots")):
            self.world()
            self.player.run("wait", "5m")
            self.reply("menu", [{"id": "wait.duration_menu", "enabled": True}])
            self.reply("menu", [{"id": "wait.5m", "enabled": True}])
            result = self.reply("activity_wait", [{"id": "activity.pause", "enabled": True}], minute=2)
            self.assertTrue(self.player.render(result).startswith(
                "Waiting: 2 minutes elapsed of 5 minutes.\nStop waiting → play stop"))
            self.player.run("stop")
            result = self.reply("prompt", [
                {"id": "prompt.choose", "stable_id": "yes-target", "label": "YES", "enabled": True},
                {"id": "prompt.choose", "stable_id": "no-target", "label": "NO", "enabled": True}],
                minute=2, facts={"text": "Confirm: Stop waiting? (Case Sensitive)"})
            self.assertTrue(self.player.render(result).startswith("Stop waiting?\nYES → play yes\nNO → play no"))
            self.player.run("no")
            last = self.client.state["pending"]["request"]
            self.assertEqual(last["stable_id"], "no-target")
            self.reply("activity_wait", [{"id": "activity.pause", "enabled": True}], minute=3)
            self.player.run("look")
            result = self.reply("world", [{"id": "world.wait", "enabled": True}], minute=5)
            self.assertTrue(self.player.render(result).startswith("Waited 5 minutes. Ready."))

    def test_not_ready_reply_preserves_session_state(self):
        text = self.player.render({"ok": False, "status": {"state": "starting", "binding_id": "hidden"}})
        self.assertIn("Session is starting", text)
        self.assertIn("play look", text)
        self.assertNotIn("hidden", text)

    def test_menu_does_not_reset_native_message_baseline(self):
        self.world()
        old = {"time": "8:00", "text": "Old saved game message"}
        new = {"time": "8:05", "text": "You finish waiting."}
        self.client.state["plain_current"]["surface"]["facts"] = {"messages": [old]}
        self.player.render({"ok": True})
        self.client.state["plain_current"]["surface"] = {"kind": "menu", "facts": {}}
        self.player.render({"ok": True})
        self.client.state["plain_current"]["surface"] = {"kind": "world", "facts": {"messages": [old, new]}}
        text = self.player.render({"ok": True})
        self.assertIn("You finish waiting.", text)
        self.assertNotIn("Old saved game message", text)

    def test_actionless_native_wait_reports_progress_not_failure(self):
        self.client.state["plain_current"] = {"surface": {"kind": "wait_activity", "facts": {}}}
        text = self.player.render({"ok": True})
        self.assertEqual(text, "Activity in progress.\nCheck progress → play look")

    def test_wait_passes_advertised_alarm_clock_chooser(self):
        self.world()
        self.player.run("wait", "5m")
        self.reply("menu", [
            {"id": "menu.choose", "stable_id": "wait-mode:wait-a-while", "enabled": True},
            {"id": "menu.choose", "stable_id": "wait-mode:set-alarm", "enabled": True}])
        request = self.client.state["pending"]["request"]
        self.assertEqual(request["action_id"], "menu.choose")
        self.assertEqual(request["stable_id"], "wait-mode:wait-a-while")
        self.reply("menu", [{"id": "wait.5m", "enabled": True}])
        self.assertEqual(self.client.state["pending"]["request"]["action_id"], "wait.5m")
        result = self.reply("world", [{"id": "world.wait", "enabled": True}], minute=5)
        self.assertIn("Waited 5 minutes. Ready.", self.player.render(result))

    def test_pending_command_never_resends_and_wrong_prompt_rejects(self):
        self.world()
        with self.assertRaisesRegex(ValueError, "does not offer YES"):
            self.player.run("yes")
        self.player.run("wait", "5m")
        before = self.fixture.requests()
        with self.assertRaisesRegex(ValueError, "still pending"):
            self.player.run("stop")
        self.assertEqual(self.fixture.requests(), before)

    def test_stale_session_cannot_answer(self):
        self.player.run("look")
        self.reply("prompt", [{"id": "prompt.choose", "stable_id": "yes", "label": "YES"}],
                   facts={"text": "Stop waiting?"})
        self.fixture.write("status.json", {"binding_id": "different-session", "state": "ready"})
        before = self.fixture.requests()
        with self.assertRaisesRegex(ValueError, "stale"):
            self.player.run("yes")
        self.assertEqual(self.fixture.requests(), before)

    def test_transcript_deduplicates_only_unchanged_look(self):
        for command in ["look", "look", "yes", "yes"]:
            self.player.record(command, "Ready.")
        text = (self.fixture.session / "playtest.txt").read_text()
        self.assertEqual(text.count("> play look"), 1)
        self.assertEqual(text.count("> play yes"), 2)

    def test_alarm_is_plain_and_tells_worker_to_report(self):
        lines = performance_text({"alarms": [{"kind": "waiting_slow", "mean_seconds": .022,
                                              "sample_count": 100, "limit_seconds": .010}]})
        self.assertIn("Average turn: 22 ms over 100 turns. Limit: 10 ms.", lines)
        self.assertIn("Tell the coordinator: waiting performance needs attention.", lines)

    def test_bridge_keeps_only_current_compact_response_and_recovery_identity(self):
        session = self.fixture.session
        (session / "plain-waiting").touch()
        bridge = FileBackedCockpitBridge(session, [], binding_id="bound-a")
        for index in range(5):
            request_id = f"play-{index}"
            self.fixture.write("requests/" + request_id + ".json", {"request_id": request_id})
            raw = {"ok": True, "receipt": {"irrelevant": "x" * 10000}, "observation": {
                "run_id": "run-a", "observation_id": request_id, "game_minutes": index,
                "surface": {"kind": "prompt", "actions": [
                    {"id": "prompt.choose", "label": "YES", "stable_id": "yes", "enabled": True}],
                    "facts": {"text": "Stop waiting?", "unrelated_world_dump": "x" * 10000}}}}
            receipt = bridge._persist_response(request_id, b'{"action":"game.observe"}', json.dumps(raw).encode())
            retrieved = bridge.response_artifact(session, request_id, receipt["response_sha256"])
            self.assertTrue(retrieved["ok"])
            self.assertEqual(retrieved["response"]["observation"]["observation_id"], request_id)
            self.assertEqual(len(list((session / "responses").glob("*.json"))), 2)
            self.assertEqual(len(list((session / "requests").glob("*.json"))), 1)
            self.assertLess((session / "responses" / (request_id + ".json")).stat().st_size, 500)

    def test_short_command_does_not_cross_session_boundary(self):
        self.world()
        self.fixture.write("status.json", {"state": "ready", "binding_id": "another-run"})
        before = self.fixture.requests()
        with self.assertRaisesRegex(ValueError, "stale"):
            self.player.run("wait", "5m")
        self.assertEqual(before, self.fixture.requests())

    def test_packet_retirement_preserves_newly_submitted_successor(self):
        from waiting_transport import retire_collected_packets
        self.fixture.write("responses/old.receipt.json", {})
        self.fixture.write("responses/old.json", {})
        for name in ("old", "current", "successor"):
            self.fixture.write("requests/" + name + ".json", {})
        retire_collected_packets(self.fixture.session, "current")
        self.assertFalse((self.fixture.session / "requests/old.json").exists())
        self.assertTrue((self.fixture.session / "requests/current.json").exists())
        self.assertTrue((self.fixture.session / "requests/successor.json").exists())

    def test_unavailable_duration_can_be_corrected_in_current_menu(self):
        self.world()
        self.player.run("wait", "7m")
        with self.assertRaisesRegex(ValueError, "play wait 5m"):
            self.reply("menu", [{"id": "wait.5m", "enabled": True}])
        self.player.run("wait", "5m")
        self.assertEqual(self.client.state["pending"]["request"]["action_id"], "wait.5m")

    def test_executable_entrypoint_is_plain_text_and_logs_no_request_ids(self):
        from pathlib import Path
        cli = Path(__file__).with_name("play")
        self.fixture.write("bridge.manifest.json", {"binding_id": "bound-a", "plain_waiting": True})
        result = subprocess.run([sys.executable, str(cli), "look"],
                                env={**os.environ, "CAOL_PLAY_SESSION": str(self.fixture.session)},
                                capture_output=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("Command pending. Check result → play look", result.stdout)
        text = (self.fixture.session / "playtest.txt").read_text(encoding="utf-8")
        self.assertIn("> play look", text)
        self.assertNotIn("request_id", text)
        self.assertNotIn("sha256", text)

    def test_plain_controller_does_not_accumulate_receipt_history(self):
        import cockpit
        import startup_harness
        from cockpit_live_session_test import frame
        channel = cockpit.CockpitRunChannel(lambda: frame(1, 100))
        service = cockpit.CockpitService(run_channel=channel)
        source = io.StringIO('\n'.join(json.dumps({"action": action}) for action in
                                     ("game.observe", "game.observe", "run.quit")) + '\n')
        output = io.StringIO()
        with patch.dict(os.environ, {"CAOL_PLAIN_WAITING": "1"}):
            self.assertEqual(startup_harness.serve_cockpit_live(service, source, output), 0)
        replies = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual(len(replies), 3)
        self.assertTrue(all(reply["ok"] for reply in replies))
        self.assertEqual(channel._transcript, [])
        self.assertLessEqual(len(channel._observations), 1)
        self.assertNotIn("action_observation_sequence", output.getvalue())
        self.assertNotIn("visible_entities", output.getvalue())

    def test_plain_semantic_channel_does_not_write_receipt_log(self):
        from semantic_broker import SemanticStepChannel
        target = self.fixture.session / "semantic.steps.jsonl"
        channel = SemanticStepChannel(run_id="run", session_id="session", receipt_path=target,
                                      read_frame=lambda: {})
        with patch.dict(os.environ, {"CAOL_PLAIN_WAITING": "1"}):
            channel._persist({"accepted": True, "large_receipt": "x" * 10000})
        self.assertFalse(target.exists())

    def test_plain_terminal_report_does_not_export_history(self):
        import startup_harness
        with patch.dict(os.environ, {"CAOL_PLAIN_WAITING": "1"}), patch.object(
                startup_harness, "current_owned_process_generation", return_value={"pid": 0, "status": "exited"}):
            startup_harness.finalize_cockpit_live_session(self.fixture.session, 0, {
                "schema": "caol-cockpit-live-final-v1", "state": "finished",
                "action_observation_sequence": [{"receipt": "x" * 10000}]}, cleanup_process=False)
        text = (self.fixture.session / "cockpit.live.final.json").read_text()
        self.assertNotIn("action_observation_sequence", text)
        self.assertLess(len(text), 1000)

    def test_plain_scenario_finish_keeps_text_and_cleanup_without_certification_archive(self):
        import startup_harness
        from unittest.mock import Mock
        directory = self.fixture.session
        hook = Mock()
        report = {"ok": True, "mode": "probe", "cleanup": {"status": "already_exited"},
                  "steps": [{"receipt": "bureaucracy" * 10000}]}
        with patch.dict(os.environ, {"CAOL_PLAIN_WAITING": "1",
                                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(directory),
                                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding"}), \
                patch("sys.stdout", new_callable=io.StringIO) as output:
            startup_harness.finalize_probe_report(directory, report, post_finalize_hook=hook)
        self.assertEqual(output.getvalue(), "")
        hook.assert_not_called()
        self.assertFalse((directory / "probe.report.json").exists())
        self.assertLess(len((directory / "playtest-summary.txt").read_text()), 150)
        signal = json.loads((directory / "cockpit.bridge.safe_to_cleanup.json").read_text())
        self.assertEqual(signal["state"], "safe_to_cleanup")
        self.assertNotIn("report_sha256", signal)

    def test_plain_scenario_incomplete_cleanup_does_not_release_bridge(self):
        import startup_harness
        directory = self.fixture.session
        with patch.dict(os.environ, {"CAOL_PLAIN_WAITING": "1",
                                    "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(directory),
                                    "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding"}):
            startup_harness.finalize_probe_report(directory, {
                "ok": False, "reason": "process_identity_unavailable",
                "cleanup": {"status": "retained_process_identity_unavailable"}})
        self.assertFalse((directory / "cockpit.bridge.safe_to_cleanup.json").exists())
        self.assertIn("process identity unavailable", (directory / "playtest-summary.txt").read_text())

    def test_compact_failure_preserves_uncertainty_without_frame_dump(self):
        from waiting_transport import compact_response
        result = compact_response({"ok": False, "error": "player_cancelled", "failure": {
            "unused_authority": "revoked", "detail": {"action_outcome": "unknown", "frame": "x" * 10000}}})
        self.assertEqual(result["failure"]["detail"], {"action_outcome": "unknown"})
        self.assertEqual(result["failure"]["unused_authority"], "revoked")
        self.assertLess(len(json.dumps(result)), 300)

    def test_snapshot_alarm_is_once_and_does_not_report_obsolete_recovery(self):
        from pathlib import Path
        from waiting_transport import collect_waiting_snapshot
        directory = self.fixture.session
        trace = directory / "native"
        path = Path(str(trace) + ".performance")
        owner = {"run_id": "run-a"}
        snapshot = {"run_id": "run-a", "sample_count": 100, "last_alarm_mean": .025,
                    "mean_seconds": .025, "alarmed": True, "alarm_count": 2, "recovery_count": 1}
        path.write_text(json.dumps(snapshot))
        first = collect_waiting_snapshot(directory, trace, owner)
        self.assertEqual(len(first["alarms"]), 1)
        self.assertEqual(first["recoveries"], [])
        self.assertIn("Tell the coordinator", "\n".join(performance_text(first)))
        repeated = collect_waiting_snapshot(directory, trace, owner)
        self.assertEqual(repeated["alarms"], [])
        snapshot.update(alarmed=False, recovery_count=2, mean_seconds=.003)
        path.write_text(json.dumps(snapshot))
        recovered = collect_waiting_snapshot(directory, trace, owner)
        self.assertEqual(recovered["recoveries"], [{"kind": "waiting_recovery"}])
        self.assertEqual(collect_waiting_snapshot(directory, trace, owner)["recoveries"], [])

    def test_snapshot_activation_keeps_only_latest_input_state(self):
        from pathlib import Path
        from waiting_transport import activate_native_snapshot, PREFIX
        trace = self.fixture.session / "native"
        obsolete = self.fixture.session / "semantic.native.log"
        obsolete.write_text("obsolete startup projection")
        events = [{"event": "surface_descriptor", "frame_id": 1},
                  {"event": "turn", "seconds": 2},
                  {"event": "frame", "frame_id": 3},
                  {"event": "surface_descriptor", "frame_id": 4}]
        trace.write_text("".join(PREFIX + json.dumps(event) + "\n" for event in events))
        activate_native_snapshot(trace)
        current = [json.loads(line[len(PREFIX):]) for line in trace.read_text().splitlines()]
        self.assertEqual({event["frame_id"] for event in current}, {3, 4})
        self.assertTrue(Path(str(trace) + ".plain").exists())
        self.assertTrue(all(event["_source_offset"] == 0 for event in current))
        self.assertFalse(obsolete.exists())


if __name__ == "__main__":
    unittest.main()
