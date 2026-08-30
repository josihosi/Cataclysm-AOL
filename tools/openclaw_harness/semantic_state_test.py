#!/usr/bin/env python3
"""Focused and inverse tests for run-only travel state decisions."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
import sys
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from semantic_state import (
    MAX_EVENT_BYTES,
    SEMANTIC_STEP_PREFIX,
    decide_native_travel_boundary,
    decide_run_state,
    read_semantic_step_trace,
)
from startup_harness import (
    persist_native_travel_hostile_boundary_receipt,
    persist_native_travel_terminal_receipt,
)
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

    def test_native_travel_boundary_allows_cleared_progress_before_terminal_receipt(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "destination_present": True, "destination_cleared": False,
        }
        cleared_progress = {
            **active, "receipt_id": "travel-1:progress", "state": "progress",
            "destination_present": False, "destination_cleared": True,
        }
        completed = {
            **active, "receipt_id": "travel-1:completed_cleared", "state": "completed_cleared",
            "destination_present": False, "destination_cleared": True,
        }
        result = decide_native_travel_boundary(
            [active, cleared_progress, completed], run_id="run-1", expected_destination=[140, 31, 0],
        )
        self.assertEqual(result["status"], "green")

    def test_native_travel_filter_keeps_long_native_travel_within_its_own_bound(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            frame = {
                "event": "frame", "run_id": "run-1", "frame_id": "frame-1",
                "valid_actions": [], "action_inputs": {},
            }
            travel = {
                "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
                "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
                "destination_present": True, "destination_cleared": False,
            }
            trace.write_text(
                "".join(
                    SEMANTIC_STEP_PREFIX + json.dumps(frame) + "\n"
                    for _ in range(65)
                ) + SEMANTIC_STEP_PREFIX + json.dumps(travel) + "\n",
                encoding="utf-8",
            )
            events, status = read_semantic_step_trace(
                trace, root, "run-1", event_filter={"travel"},
            )
            self.assertEqual(status, "ok")
            self.assertEqual(len(events), 1)
            self.assertEqual(events[0]["event"], "travel")

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

    def test_native_travel_boundary_rejects_any_post_boundary_travel(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "avatar_omt": [140, 32, 0], "destination_present": True,
            "destination_cleared": False,
        }
        boundary = {
            **active, "receipt_id": "travel-1:hostile_boundary", "state": "hostile_boundary",
            "avatar_omt": [140, 31, 0],
        }
        post_boundary = {
            **active, "receipt_id": "travel-1:progress", "state": "progress",
        }
        result = decide_native_travel_boundary(
            [active, boundary, post_boundary], run_id="run-1", expected_destination=[140, 31, 0],
        )
        self.assertEqual(result["reason"], "post_hostile_boundary_travel")
        self.assertEqual(result["hostile_boundary_omt"], [140, 31, 0])

        completed = {
            **active, "receipt_id": "travel-1:completed", "state": "completed_cleared",
            "destination_present": False, "destination_cleared": True,
        }
        permissive = decide_native_travel_boundary(
            [active, boundary, post_boundary, completed], run_id="run-1",
            expected_destination=[140, 31, 0], allow_handled_hostile_boundaries=True,
        )
        self.assertEqual(permissive["status"], "green")
        self.assertEqual(permissive["handled_hostile_boundaries"], [{
            "receipt_id": "travel-1:hostile_boundary", "avatar_omt": [140, 31, 0],
        }])

    def test_native_travel_terminal_receipt_is_durable_and_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("native terminal facts\n", encoding="utf-8")
            green = persist_native_travel_terminal_receipt({
                "danger_handling": "ignore_danger_and_interruptions",
                "handled_interruptions": [{
                    "classification": "hostile_auto_move_cancel",
                    "response": "N",
                }],
                "native_travel_boundary": {
                    "status": "green",
                    "travel_id": "travel-1",
                    "active_receipt_id": "travel-1:active",
                    "completion_receipt_id": "travel-1:completed_cleared",
                    "destination": [140, 31, 0],
                    "source": str(trace),
                    "event_count": 2,
                    "handled_hostile_boundaries": [{
                        "receipt_id": "travel-1:hostile_boundary",
                        "avatar_omt": [140, 31, 0],
                    }],
                },
            }, run_dir=root, label="arrival", run_id="run-1")
            red = persist_native_travel_terminal_receipt({
                "native_travel_boundary": {"status": "blocked"},
            }, run_dir=root, label="missing", run_id="run-1")

            self.assertEqual(green["status"], "required_state_present")
            self.assertTrue(Path(green["artifact_path"]).is_file())
            self.assertTrue(green["artifact_sha256"])
            self.assertEqual(green["danger_handling"], "ignore_danger_and_interruptions")
            self.assertEqual(green["handled_interruptions"][0]["response"], "N")
            self.assertEqual(
                green["handled_hostile_boundaries"][0]["receipt_id"],
                "travel-1:hostile_boundary",
            )
            self.assertEqual(red["status"], "required_state_missing")
            self.assertEqual(red["reason"], "native_travel_completion_boundary_not_green")

    def test_hostile_boundary_receipt_requires_no_response_and_owned_trace(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("native hostile boundary facts\n", encoding="utf-8")
            green = persist_native_travel_hostile_boundary_receipt({
                "response_keys": [],
                "native_travel_boundary": {
                    "reason": "hostile_boundary", "travel_id": "travel-1",
                    "hostile_boundary_omt": [142, 31, 0], "source": str(trace),
                    "event_count": 2,
                },
            }, run_dir=root, label="boundary", run_id="run-1")
            answered = persist_native_travel_hostile_boundary_receipt({
                "response_keys": ["N"],
                "native_travel_boundary": {
                    "reason": "hostile_boundary", "travel_id": "travel-1",
                    "hostile_boundary_omt": [142, 31, 0], "source": str(trace),
                },
            }, run_dir=root, label="answered", run_id="run-1")
            self.assertEqual(green["status"], "required_state_present")
            self.assertEqual(green["prompt_responses"], [])
            self.assertTrue(Path(green["artifact_path"]).is_file())
            self.assertEqual(answered["reason"], "hostile_boundary_prompt_was_answered")

    def test_permissive_hostile_boundary_without_terminal_stays_red_and_is_receipted(self):
        active = {
            "event": "travel", "run_id": "run-1", "travel_id": "travel-1",
            "receipt_id": "travel-1:active", "state": "active", "destination": [140, 31, 0],
            "destination_present": True, "destination_cleared": False,
        }
        hostile = {
            **active, "receipt_id": "travel-1:hostile", "state": "hostile_boundary",
            "avatar_omt": [142, 31, 0],
        }
        result = decide_native_travel_boundary(
            [active, hostile], run_id="run-1", expected_destination=[140, 31, 0],
            allow_handled_hostile_boundaries=True,
        )
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["reason"], "hostile_boundary_without_terminal")
        self.assertEqual(result["hostile_boundary_omt"], [142, 31, 0])

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("native hostile boundary facts\n", encoding="utf-8")
            receipt = persist_native_travel_hostile_boundary_receipt({
                "danger_handling": "ignore_danger_and_interruptions",
                "response_keys": ["N"],
                "native_travel_boundary": {**result, "source": str(trace), "event_count": 2},
            }, run_dir=root, label="permissive", run_id="run-1")
            self.assertEqual(receipt["status"], "required_state_present")
            self.assertFalse(receipt["completed_destination_cleared"])
            self.assertEqual(receipt["prompt_responses"], ["N"])
            self.assertEqual(receipt["danger_handling"], "ignore_danger_and_interruptions")

    def test_permissive_hostile_boundary_stops_stabilization_without_polling_past_it(self):
        boundary = {
            "status": "blocked",
            "reason": "hostile_boundary_without_terminal",
        }
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.object(startup_harness, "pid_is_alive", return_value=True), \
                    mock.patch.object(startup_harness, "capture_screenshot", return_value={}), \
                    mock.patch.object(startup_harness, "capture_screen_text_artifact",
                                      return_value={"text": ""}):
                result = startup_harness.stabilize_native_travel_after_route_confirmation(
                    0, Path(temp), "permissive_boundary", delay_ms=0,
                    timeout_seconds=0, danger_handling="ignore_danger_and_interruptions",
                    native_travel_boundary_reader=lambda: boundary,
                )
            self.assertEqual(result["verdict"], "blocked_native_travel_hostile_boundary")

    def test_permissive_hostile_modal_is_answered_before_boundary_is_terminal(self):
        prompt = {"ok": True, "text":
                  "crawling zombie spotted! Cancel auto move? (Case Sensitive)"}
        hud = {"ok": True, "text": "Move: 100\nWield: empty\nActivity: None"}
        boundaries = iter([
            {"status": "blocked", "reason": "hostile_boundary_without_terminal"},
            {"status": "green", "reason": "native_travel_completed_and_destination_cleared"},
        ])
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.object(startup_harness, "pid_is_alive", return_value=True), \
                    mock.patch.object(startup_harness, "capture_screenshot", return_value={}), \
                    mock.patch.object(
                        startup_harness, "capture_screen_text_artifact",
                        side_effect=[prompt, hud],
                    ), \
                    mock.patch.object(startup_harness, "peekaboo_type_text") as type_text:
                result = startup_harness.stabilize_native_travel_after_route_confirmation(
                    0, Path(temp), "permissive_modal", delay_ms=0,
                    timeout_seconds=1, danger_handling="ignore_danger_and_interruptions",
                    native_travel_boundary_reader=lambda: next(boundaries),
                )
        self.assertEqual(result["verdict"], "green_native_travel_stabilized")
        self.assertEqual(result["response_keys"], ["N"])
        type_text.assert_called_once_with(0, "N", delay_ms=0)

    def test_native_travel_stabilization_rejects_unbounded_without_native_boundary(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaisesRegex(ValueError, "requires a native completion boundary"):
                startup_harness.stabilize_native_travel_after_route_confirmation(
                    0, Path(temp), "arrival", delay_ms=1, timeout_seconds=0,
                )

    def test_undeclared_hostile_auto_move_prompt_fails_closed(self):
        prompt = {
            "ok": True,
            "text": "fat zombie spotted! Cancel auto move? (Case Sensitive)",
        }
        declared = startup_harness.classify_blocking_interruption(
            prompt, allow_hostile_auto_move_cancel=True,
        )
        undeclared = startup_harness.classify_blocking_interruption(
            prompt, allow_hostile_auto_move_cancel=False,
        )

        self.assertEqual(declared["classification"], "authorized_hostile_auto_move_cancel")
        self.assertEqual(undeclared["status"], "unknown_prompt")
        self.assertEqual(undeclared["classification"], "partial_hostile_auto_move_cancel_prompt")


if __name__ == "__main__":
    unittest.main()
