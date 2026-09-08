"""Focused retained-artifact tests for the compact observation workbench."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from cockpit_evidence import compare_selected, selected_view
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge


class CompactIoObservationTest(unittest.TestCase):
    def test_selected_view_preserves_unavailable_false_and_empty(self):
        value = {"actor": {"active": False, "items": []}}
        view = selected_view(value, ["actor.active", "actor.items", "actor.missing"])
        self.assertTrue(view["selectors"]["actor.active"]["available"])
        self.assertIs(view["selectors"]["actor.active"]["value"], False)
        self.assertEqual(view["selectors"]["actor.items"]["value"], [])
        self.assertFalse(view["selectors"]["actor.missing"]["available"])

    def test_comparison_classifies_changes_and_incompatible_processes(self):
        def observation(process, value, stamp):
            return {"run_id": "run-a", "binding_id": "bind-a", "process_instance": process,
                    "observation_id": "frame-" + process, "game_turn": stamp,
                    "timestamp": stamp, "surface": {"facts": {"same": 1, "value": value}}}

        before = {"observation": observation("process-a", "old", 1)}
        before["observation"]["surface"]["facts"]["removed"] = True
        after = {"observation": observation("process-a", "new", 2)}
        after["observation"]["surface"]["facts"]["added"] = True
        result = compare_selected(before, after, ["observation.surface.facts.same",
                                                   "observation.surface.facts.value",
                                                   "observation.surface.facts.added",
                                                   "observation.surface.facts.removed"])
        self.assertEqual(result["summary"]["unchanged"], ["observation.surface.facts.same"])
        self.assertEqual(result["summary"]["changed"], ["observation.surface.facts.value"])
        self.assertEqual(result["summary"]["added"], ["observation.surface.facts.added"])
        self.assertEqual(result["summary"]["removed"], ["observation.surface.facts.removed"])
        self.assertTrue(result["timestamp_changed"])
        incompatible = compare_selected(before, {"observation": observation("process-b", "new", 2)},
                                         ["observation.surface.facts.value"])
        self.assertEqual(incompatible["status"], "incompatible")
        self.assertEqual(incompatible["fields"]["observation.surface.facts.value"]["status"], "incompatible")

    def test_request_result_and_compare_retrieve_verified_originals(self):
        with tempfile.TemporaryDirectory() as temp:
            session = Path(temp)
            (session / "requests").mkdir()
            (session / "responses").mkdir()
            (session / "status.json").write_text(json.dumps({"binding_id": "bind-a", "state": "ready"}))
            requests = []
            for request_id, value in (("before", "old"), ("after", "new")):
                request = {"action": "game.observe", "observation_id": request_id}
                envelope = {"request_id": request_id, "binding_id": "bind-a", "request": request}
                request_path = session / "requests" / (request_id + ".json")
                request_path.write_text(json.dumps(envelope, sort_keys=True) + "\n")
                raw = json.dumps({"ok": True, "result": {"run_id": "run-a", "binding_id": "bind-a",
                    "process_instance": "proc-a", "observation_id": request_id,
                    "surface": {"facts": {"value": value}}}}).encode()
                response_path = session / "responses" / (request_id + ".json")
                response_path.write_bytes(raw)
                receipt = {"request_id": request_id, "binding_id": "bind-a",
                    "request_sha256": hashlib.sha256((json.dumps(request, separators=(",", ":")) + "\n").encode()).hexdigest(),
                    "response_sha256": hashlib.sha256(raw).hexdigest(),
                    "response_artifact": "responses/" + request_id + ".json"}
                (session / "responses" / (request_id + ".receipt.json")).write_text(json.dumps(receipt))
                requests.append(request_id)
            linked = Bridge.request_result(session, "after")
            self.assertTrue(linked["ok"], linked)
            self.assertEqual(linked["request"]["request"]["observation_id"], "after")
            compared = Bridge.response_compare(session, *requests, ["result.surface.facts.value"])
            self.assertTrue(compared["ok"], compared)
            self.assertEqual(compared["comparison"]["summary"]["changed"], ["result.surface.facts.value"])
            self.assertEqual(compared["comparison"]["requests"]["before"]["request_id"], "before")


if __name__ == "__main__":
    unittest.main()
