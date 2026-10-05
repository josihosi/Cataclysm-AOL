"""Original Talk selection receipt plus stale/choose controls; no game input."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import startup_harness

FIXTURE = Path(__file__).parent / "fixtures/controls/r067_already_selected_talk.json"


class AlreadySelectedMenuTest(unittest.TestCase):
    def dispatch(self, action="menu.select", receipt_change=None, frame_change=None):
        original = json.loads(FIXTURE.read_text())
        frame = copy.deepcopy(original["724"]["record"])
        receipt = copy.deepcopy(original["730"]["record"])
        if frame_change:
            frame_change(frame)
        stable_id = "uilist-entry:49"
        request_id = ("cockpit:worker:" + frame["frame_id"] + ":" + action + ":" +
                      hashlib.sha256(json.dumps({"stable_id": stable_id, "parameters": {}},
                                                sort_keys=True, separators=(",", ":")).encode()).hexdigest()[:16])
        receipt.update(request_id=request_id, action_id=action)
        if receipt_change:
            receipt_change(receipt)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("openclaw_harness_semantic_step: " + json.dumps(receipt) + "\n")
            with patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={"status": "bound", "path": "pipe"}), \
                    patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1) as wake, \
                    patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = startup_harness.execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=frame["run_id"], trace_start_offset=0,
                    pid=17, session_id="worker", frame_id=frame["frame_id"], action_id=action,
                    stable_id=stable_id, observed_frame=frame, transition_timeout_seconds=0.01,
                    observe_interval_seconds=0.001)
            return result, frame, wake.call_count

    def test_original_native_same_frame_select_completes_without_activation(self):
        result, frame, writes = self.dispatch()
        self.assertTrue(result["accepted"], result)
        self.assertEqual(result["next_frame"], frame)
        self.assertTrue(result["selection_only"])
        self.assertIn("menu.choose", result["selection_note"])
        self.assertEqual(result["native_receipt"]["action_id"], "menu.select")
        self.assertEqual(writes, 1)  # exactly one request, no choose/retry

    def test_choose_still_requires_real_successor(self):
        result, _, _ = self.dispatch(action="menu.choose")
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "native_surface_successor_timeout")

    def test_different_selection_does_not_reuse_prior_descriptor(self):
        result, _, _ = self.dispatch(frame_change=lambda frame: frame["payload"].update(selected_stable_id="uilist-entry:48"))
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "native_surface_successor_timeout")

    def test_mismatched_receipt_is_not_same_owner_authority(self):
        for field in ("requested_surface_id", "consuming_surface_id", "requested_frame_id", "consuming_frame_id"):
            with self.subTest(field=field):
                result, _, _ = self.dispatch(receipt_change=lambda receipt: receipt.update({field: "stale"}))
                self.assertFalse(result["accepted"])

    def test_different_resulting_frame_is_not_an_unchanged_selection(self):
        result, _, _ = self.dispatch(receipt_change=lambda receipt: receipt.update(resulting_frame_id="new-unpublished"))
        self.assertFalse(result["accepted"])


if __name__ == "__main__":
    unittest.main()
