"""Exercise cancellation through the live adapter and native request/receipt files."""
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import startup_harness as startup
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge


class CancellationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.session = self.root / "session"
        self.session.mkdir()
        (self.session / "controls").mkdir()
        self.run = self.root / "run"
        self.run.mkdir()
        self.trace = self.run / "native.log"
        self.descriptor = {"event": "surface_descriptor", "schema_version": 1,
            "run_id": "run-a", "frame_id": "f1", "surface_id": "world-1", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [
                {"id": "world.pause", "stable_id": "world.pause", "label": "Pause", "enabled": True}]}
        self.current = dict(self.descriptor)
        self.trace.write_text("openclaw_harness_semantic_step: " + json.dumps(self.current) + "\n")
        self.active("request-1")
        for context in (patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(self.session),
                                "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "bound-a"}),
                        patch.object(startup, "semantic_step_source_trace", return_value=self.trace),
                        patch.object(startup, "current_semantic_step_frame", side_effect=lambda **_: self.current),
                        patch.object(startup, "refresh_semantic_step_trace", return_value=(self.trace, self.trace)),
                        patch.object(startup, "semantic_wake_pipe_contract", return_value={"status": "bound", "path": "test"})):
            context.start()
            self.addCleanup(context.stop)
        self.service = startup.open_cockpit_game_service(profile="test", run_dir=self.run,
            run_id="run-a", trace_start_offset=0, pid=os.getpid(), session_id="test-player", live_session=True, cleanup_on_finish=False,
            transition_timeout_seconds=.1, observe_interval_seconds=.001)
        self.addCleanup(self.service.live_channel.archive.close)
        self.addCleanup(self.service.call, {"action": "run.quit", "stop_reason": "test_cleanup"})
        self.observe()

    def active(self, request_id):
        self.request_id = request_id
        for name, data in (("status.json", {"state": "awaiting_response", "inflight_request_id": request_id}),
                           ("active-request.json", {"request_id": request_id, "run_id": "run-a"})):
            (self.session / name).write_text(json.dumps({**data, "binding_id": "bound-a", "session_generation": 2}))

    def observe(self):
        result = self.service.call({"action": "game.observe"})
        self.assertTrue(result["ok"], result)
        self.frame_id = result["result"]["observation_id"]

    def cancel(self):
        result = Bridge.send_cancel(self.session, request_id=self.request_id, binding_id="bound-a", run_id="run-a")
        self.assertTrue(result["ok"], result)

    def act(self):
        return self.service.call({"action": "game.act", "observation_id": self.frame_id, "action_id": "world.pause"})

    def publish_success(self):
        request = json.loads((self.run / "semantic.requests.jsonl").read_text().splitlines()[-1])
        self.current = {**self.descriptor, "frame_id": "f2"}
        receipt = {"event": "surface_receipt", "run_id": "run-a", "request_id": request["request_id"],
            "requested_run_id": "run-a", "requested_surface_id": "world-1", "requested_frame_id": "f1",
            "consuming_surface_id": "world-1", "consuming_frame_id": "f1", "action_id": "world.pause",
            "accepted": True, "rejection_reason": "", "resulting_frame_id": "f2"}
        with self.trace.open("a") as stream:
            for event in (self.current, receipt):
                stream.write("openclaw_harness_semantic_step: " + json.dumps(event) + "\n")

    def test_cancel_before_dispatch_does_not_emit_input(self):
        self.cancel()
        result = self.act()
        self.assertEqual(result["error"], "player_cancelled")
        self.assertEqual(result["failure"]["detail"]["action_outcome"], "not_dispatched")
        self.assertFalse((self.run / "semantic.requests.jsonl").exists())
        self.assertEqual(self.service.live_channel._state, "active")

    @unittest.skipIf(os.name == "nt", "native Windows does not use the wake pipe")
    def test_cancel_after_write_retains_unknown_input_and_reobserves(self):
        with patch.object(startup, "write_semantic_wake_pipe", side_effect=lambda *_: self.cancel() or 1):
            result = self.act()
        self.assertEqual(result["error"], "player_cancelled", result)
        self.assertEqual(result["failure"]["detail"]["action_outcome"], "unknown")
        self.assertIsNone(result["receipt"]["native_receipt"])
        self.assertEqual(len((self.run / "semantic.requests.jsonl").read_text().splitlines()), 1)
        self.assertFalse(self.act()["ok"])
        self.active("request-2")
        self.observe()
        with patch.object(startup, "write_semantic_wake_pipe", side_effect=lambda *_: self.publish_success() or 1):
            result = self.act()
            self.assertTrue(result["ok"], result)
        self.assertEqual(self.service.live_channel._state, "active")

    @unittest.skipIf(os.name == "nt", "native Windows does not use the wake pipe")
    def test_durable_receipt_wins_cancellation_race(self):
        def wake(*_):
            self.publish_success()
            self.cancel()
            return 1
        with patch.object(startup, "write_semantic_wake_pipe", side_effect=wake):
            result = self.act()
        self.assertTrue(result["ok"], result)
        self.assertTrue(result["receipt"]["native_receipt"]["accepted"])
        self.assertEqual(result["observation"]["observation_id"], "f2")

    def test_old_run_generation_or_request_does_not_cancel_current(self):
        self.cancel()
        path = self.session / "controls" / ("cancel-" + hashlib.sha256(self.request_id.encode()).hexdigest() + ".json")
        original = json.loads(path.read_text())
        for field, value in (("run_id", "old-run"), ("binding_id", "old-binding"), ("session_generation", 1), ("request_id", "old-request")):
            with self.subTest(field=field):
                path.write_text(json.dumps({**original, field: value}))
                self.assertEqual(self.service.live_channel._cancel_request(), {})
        path.write_text(json.dumps(original))
        self.assertTrue(self.service.live_channel._cancel_request()["cancelled"])
        self.active("request-2")
        self.assertEqual(self.service.live_channel._cancel_request(), {})


if __name__ == "__main__":
    unittest.main()
