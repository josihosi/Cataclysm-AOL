#!/usr/bin/env python3
"""Focused and inverse tests for run-only travel state decisions."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from semantic_state import MAX_EVENT_BYTES, decide_native_travel_boundary, decide_run_state
import startup_harness


class SemanticStateTest(unittest.TestCase):
    def write_events(self, directory: Path, events: list[dict]) -> Path:
        path = directory / "transition.events.jsonl"
        path.write_text("".join(json.dumps(event) + "\n" for event in events), encoding="utf-8")
        return path

    def event(self, **extra) -> dict:
        value = {"run_id": "run-1", "site_id": "camp-3", "operation_id": "outing-1",
                 "new_phase": "outbound", "outcome": "applied"}
        value.update(extra)
        return value

    def test_travel_arrival_and_activity_use_structured_facts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            path = self.write_events(root, [self.event(), self.event(new_phase="arrived")])
            result = decide_run_state(transition_path=path, run_dir=root, run_id="run-1",
                                      expected_destination="camp-3", activity_id="outing-1")
            self.assertEqual(result["status"], "clear")
            self.assertTrue(result["traveling"])
            self.assertTrue(result["arrived"])
            self.assertTrue(result["activity"])

    def test_missing_escape_contamination_and_wrong_destination_fail_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            missing = decide_run_state(transition_path=root / "missing", run_dir=root, run_id="run-1")
            self.assertEqual(missing["reason"], "missing_or_unreadable")
            outside = Path(temp).parent / "escaped-events.jsonl"
            outside.write_text(json.dumps(self.event()) + "\n", encoding="utf-8")
            self.assertEqual(decide_run_state(transition_path=outside, run_dir=root, run_id="run-1")["reason"], "escaped_authority")
            path = self.write_events(root, [self.event(run_id="other")])
            self.assertEqual(decide_run_state(transition_path=path, run_dir=root, run_id="run-1")["reason"], "contamination")
            path = self.write_events(root, [self.event(site_id="wrong")])
            self.assertEqual(decide_run_state(transition_path=path, run_dir=root, run_id="run-1",
                                              expected_destination="camp-3")["reason"], "wrong_destination")

    def test_output_and_progress_bounds_and_visuals_do_not_decide(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            path = root / "transition.events.jsonl"
            path.write_bytes(b"x" * (MAX_EVENT_BYTES + 1))
            self.assertEqual(decide_run_state(transition_path=path, run_dir=root, run_id="run-1")["reason"], "unbounded_output")
            path = self.write_events(root, [self.event(screen_text="contradictory OCR", weather="moon")])
            result = decide_run_state(transition_path=path, run_dir=root, run_id="run-1", previous_progress=1)
            self.assertEqual(result["reason"], "progress_free_recovery")

    def test_structured_gate_consumes_semantic_state_without_visuals(self):
        events = [self.event(sequence=1, screen_text="wrong OCR"), self.event(sequence=2, new_phase="arrived", moon="contradiction")]
        result = startup_harness.evaluate_structured_proof_gates(
            [{"id": "arrival", "boundary_step": "travel", "expectations": [{
                "kind": "semantic_state", "predicate": {
                    "status": "clear", "expected_destination": "camp-3",
                    "activity_id": "outing-1", "arrived": True, "activity": True,
                },
            }]}],
            events=events,
            watermarks={"travel": {"last_sequence": 2, "byte_offset": 100}},
            run_id="run-1",
        )
        self.assertEqual(result["status"], "green")

    def test_native_travel_boundary_requires_completed_and_cleared_receipt(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "destination_present": True, "destination_cleared": False,
        }
        completed = {
            **active, "receipt_id": "travel-1:completed_cleared", "state": "completed_cleared",
            "destination_present": False, "destination_cleared": True,
        }
        result = decide_native_travel_boundary(
            [active, completed], run_id="run-1", expected_destination=[140, 31, 0],
        )
        self.assertEqual(result["status"], "green")
        self.assertNotEqual(result["active_receipt_id"], result["completion_receipt_id"])

    def test_native_travel_boundary_fails_active_stale_wrong_run_blocked_and_interrupted(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "destination_present": True, "destination_cleared": False,
        }
        completed = {
            **active, "receipt_id": "travel-1:completed_cleared", "state": "completed_cleared",
            "destination_present": False, "destination_cleared": True,
        }
        self.assertEqual(
            decide_native_travel_boundary([active], run_id="run-1", expected_destination=[140, 31, 0])["reason"],
            "active_travel_no_progress",
        )
        self.assertEqual(
            decide_native_travel_boundary([completed], run_id="run-1", expected_destination=[140, 31, 0])["reason"],
            "travel_not_observed",
        )
        self.assertEqual(
            decide_native_travel_boundary([{**active, "run_id": "other"}], run_id="run-1",
                                          expected_destination=[140, 31, 0])["reason"],
            "wrong_run",
        )
        for terminal in ("blocked", "interrupted"):
            with self.subTest(terminal=terminal):
                self.assertEqual(
                    decide_native_travel_boundary(
                        [active, {**active, "state": terminal, "receipt_id": f"travel-1:{terminal}"}],
                        run_id="run-1", expected_destination=[140, 31, 0],
                )["reason"], terminal,
                )

    def test_native_travel_boundary_distinguishes_progress_without_terminal(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "destination_present": True, "destination_cleared": False,
        }
        progress = {
            **active, "receipt_id": "travel-1:progress", "state": "progress",
        }
        result = decide_native_travel_boundary(
            [active, progress], run_id="run-1", expected_destination=[140, 31, 0],
        )
        self.assertEqual(result["reason"], "active_travel_progress_without_terminal")
        self.assertEqual(result["last_progress_receipt_id"], "travel-1:progress")


if __name__ == "__main__":
    unittest.main()
