"""Complete native records reach the existing bounded, lossless public channel."""
import io
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

import startup_harness as startup


class DescriptorIntakeTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run = self.root / "run"
        self.run.mkdir()
        self.config = self.root / "config"
        self.config.mkdir()
        self.trace = self.run / "semantic.native.events.jsonl"
        (self.run / "terminal.owner.json").write_text("{}")
        self.descriptor = {
            "event": "surface_descriptor", "schema_version": 1,
            "run_id": "fixture-run", "process_instance": "fixture-process", "sequence": 2,
            "game_turn": 100, "game_minutes": 1, "surface_id": "fixture-surface",
            "frame_id": "fixture-frame", "kind": "inventory", "breadcrumbs": ["Inventory"],
            "payload": {"title": "Inventory"}, "valid_actions": [
                {"id": "inventory.filter", "stable_id": "", "label": "Filter", "enabled": True}],
        }
        for context in (
            patch.object(startup, "config_dir_for_profile", return_value=self.config),
            patch.object(startup, "resolve_profile_name", return_value="fixture"),
            patch.object(startup, "current_owned_process_generation", return_value={
                "pid": os.getpid(), "status": "alive",
                "expected": {"pid": os.getpid(), "birth_identity": "disposable-fixture"},
            }),
            patch.dict(startup.os.environ, {
                "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": "",
                "OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "",
            }),
        ):
            context.start()
            self.addCleanup(context.stop)

    def encode(self, event):
        return (startup.SEMANTIC_STEP_PREFIX + json.dumps(event) + "\n").encode()

    def publish(self, data):
        self.trace.write_bytes(data)
        (self.config / "debug.log").write_bytes(data)

    def service(self):
        return startup.open_cockpit_game_service(
            profile="fixture", run_dir=self.run, run_id=self.descriptor["run_id"], trace_start_offset=0,
            pid=os.getpid(),
            session_id="disposable-fixture", cleanup_on_finish=False,
            transition_timeout_seconds=3, observe_interval_seconds=.001,
        )

    def frame(self):
        return startup.current_semantic_step_frame(
            profile="fixture", run_dir=self.run, run_id=self.descriptor["run_id"], start_offset=0)

    def test_large_action_and_payload_catalogs_are_lossless_not_item_capped(self):
        original = {**self.descriptor, "payload": {"title": "Inventory", "facts": "x" * 600000},
                    "valid_actions": [
                        {"id": "inventory.toggle", "stable_id": "item:" + str(i),
                         "label": "Item " + str(i), "enabled": True} for i in range(50000)]}
        raw = self.encode(original)
        self.assertGreater(len(raw), startup.SEMANTIC_STEP_MAX_BYTES * 16)
        self.publish(raw)
        frame = self.frame()
        self.assertEqual(frame["valid_actions"], original["valid_actions"])
        self.assertEqual(frame["payload"], original["payload"])
        self.assertEqual(frame["_source_path"], str(self.trace.resolve()))
        self.assertEqual(frame["_source_offset"], 0)
        self.assertEqual(frame["_source_end"], len(raw))
        self.assertLessEqual((self.run / "semantic.native.log").stat().st_size,
                             startup.SEMANTIC_STEP_MAX_BYTES)
        self.assertEqual(startup.latest_semantic_source_descriptor(self.trace)["frame_id"],
                         original["frame_id"])
        observed = self.service().call({"action": "game.observe"})
        self.assertTrue(observed["ok"], observed)
        self.assertEqual(observed["result"]["observation_id"], original["frame_id"])
        self.assertFalse((self.run / "semantic.requests.jsonl").exists())

    def test_oversized_first_owner_after_actor_output_is_not_replaced_by_old_mirror(self):
        current = {**self.descriptor, "payload": {"facts": "x" * 600000}}
        self.trace.write_bytes(b"actor output " + b"x" * 5000000 + b"\n" + self.encode(current))
        (self.config / "debug.log").write_bytes(self.encode({
            **self.descriptor, "sequence": 1, "frame_id": "old-mirror",
        }))
        self.assertEqual(startup.semantic_step_source_trace("fixture", self.run), self.trace)
        frame = self.frame()
        self.assertEqual(frame["frame_id"], current["frame_id"])
        self.assertEqual(frame["payload"], current["payload"])
        self.assertEqual(frame["valid_actions"], current["valid_actions"])

    def test_sampled_partial_record_refuses_instead_of_restoring_previous_owner(self):
        previous = self.encode({**self.descriptor, "frame_id": "previous"})
        newer = self.encode({**self.descriptor, "payload": {"facts": "x" * 5000000}})
        self.publish(previous + newer[:-1])
        with self.assertRaisesRegex(ValueError, "incomplete_native_semantic_record"):
            self.frame()
        with self.assertRaisesRegex(ValueError, "incomplete_native_semantic_record"):
            startup.latest_semantic_source_descriptor(self.trace)
        self.assertFalse((self.run / "semantic.requests.jsonl").exists())

    def test_sample_does_not_include_a_record_terminated_only_after_sample(self):
        raw = self.encode(self.descriptor)
        with self.assertRaisesRegex(ValueError, "incomplete_native_semantic_record"):
            list(startup.iter_complete_semantic_trace_records(io.BytesIO(raw), len(raw) - 1))

    def test_malformed_and_unsupported_records_refuse(self):
        for raw, error in (
            (startup.SEMANTIC_STEP_PREFIX.encode() + b'{"event":broken}\n',
             "malformed_native_semantic_record"),
            (self.encode({**self.descriptor, "event": "unsupported"}),
             "malformed_semantic_step"),
        ):
            with self.subTest(error=error):
                self.publish(raw)
                with self.assertRaisesRegex(ValueError, error):
                    self.frame()

    def test_foreign_run_and_duplicate_action_catalog_do_not_authorize(self):
        for descriptor in (
            {**self.descriptor, "run_id": "foreign"},
            {**self.descriptor, "valid_actions": self.descriptor["valid_actions"] * 2},
        ):
            self.publish(self.encode(descriptor))
            with self.assertRaises(ValueError):
                self.frame()
            self.assertFalse((self.run / "semantic.requests.jsonl").exists())

    def test_public_current_choice_dispatches_once_stale_and_disabled_refuse(self):
        self.publish(self.encode(self.descriptor))
        service = self.service()
        current = service.call({"action": "game.observe"})["result"]["observation_id"]
        self.assertFalse(service.call({"action": "game.act", "observation_id": "old",
                                      "action_id": "inventory.filter"})["ok"])
        self.assertFalse((self.run / "semantic.requests.jsonl").exists())

        def native_fixture(*args, **kwargs):
            request = json.loads((self.run / "semantic.requests.jsonl").read_text().splitlines()[-1])
            successor = {**self.descriptor, "surface_id": "next-surface", "frame_id": "next-frame",
                         "sequence": 4, "valid_actions": [
                             {**self.descriptor["valid_actions"][0], "enabled": False}]}
            receipt = {"event": "surface_receipt", "run_id": "fixture-run", "sequence": 3,
                       "request_id": request["request_id"], "action_id": request["action_id"],
                       "requested_run_id": "fixture-run", "requested_surface_id": request["surface_id"],
                       "requested_frame_id": request["frame_id"],
                       "consuming_surface_id": request["surface_id"],
                       "consuming_frame_id": request["frame_id"], "accepted": True,
                       "rejection_reason": "", "resulting_frame_id": "next-frame"}
            successor_bytes = self.encode(successor)
            with self.trace.open("ab") as stream:
                stream.write(self.encode(receipt))
                if getattr(self, "partial_delivery", False):
                    stream.write(successor_bytes[:-1])
                else:
                    stream.write(successor_bytes)
            if getattr(self, "partial_delivery", False):
                def complete_native_record():
                    with self.trace.open("ab") as stream:
                        stream.write(successor_bytes[-1:])
                completion = threading.Timer(.04, complete_native_record)
                completion.start()
                self.addCleanup(completion.join)
            return 1

        with patch.object(startup, "semantic_wake_pipe_contract",
                          return_value={"status": "bound", "path": "fixture",
                                        "contract": {"transport": "disposable-native-fixture"}}), \
                patch.object(startup, "write_semantic_wake_pipe", side_effect=native_fixture):
            acted = service.call({"action": "game.act", "observation_id": current,
                                  "action_id": "inventory.filter", "parameters": {"filter": "M240"}})
        self.assertTrue(acted["ok"], acted)
        self.assertEqual(len((self.run / "semantic.requests.jsonl").read_text().splitlines()), 1)
        self.assertFalse(service.call({"action": "game.act", "observation_id": current,
                                      "action_id": "inventory.filter"})["ok"])
        after = service.call({"action": "game.observe"})["result"]["observation_id"]
        self.assertEqual(after, "next-frame")
        self.assertFalse(service.call({"action": "game.act", "observation_id": after,
                                      "action_id": "inventory.filter"})["ok"])
        self.assertEqual(len((self.run / "semantic.requests.jsonl").read_text().splitlines()), 1)

    def test_completed_receipt_survives_partial_successor_poll_without_replaying(self):
        self.partial_delivery = True
        self.test_public_current_choice_dispatches_once_stale_and_disabled_refuse()

    def test_following_valid_record_after_oversized_nonsemantic_line_is_current(self):
        self.publish(b"unrelated native output " + b"x" * 5000000 + b"\n" + self.encode(self.descriptor))
        self.assertEqual(self.frame()["frame_id"], self.descriptor["frame_id"])
        self.assertEqual(startup.latest_semantic_source_descriptor(self.trace)["frame_id"],
                         self.descriptor["frame_id"])



if __name__ == "__main__":
    unittest.main()
