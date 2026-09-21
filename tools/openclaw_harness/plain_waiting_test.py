"""Real file-backed client routing; responses are explicit game fixtures."""
import json
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

    def test_executable_entrypoint_is_plain_text_and_logs_no_request_ids(self):
        from pathlib import Path
        cli = Path(__file__).with_name("play")
        result = subprocess.run([sys.executable, str(cli), "look"],
                                env={**os.environ, "CAOL_PLAY_SESSION": str(self.fixture.session)},
                                capture_output=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("Command pending. Check result → play look", result.stdout)
        text = (self.fixture.session / "playtest.txt").read_text(encoding="utf-8")
        self.assertIn("> play look", text)
        self.assertNotIn("request_id", text)
        self.assertNotIn("sha256", text)


if __name__ == "__main__":
    unittest.main()
