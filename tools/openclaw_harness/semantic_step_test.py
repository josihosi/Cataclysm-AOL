#!/usr/bin/env python3
"""Focused contract tests for the adaptive semantic playtest channel."""

from __future__ import annotations

import inspect
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from semantic_broker import SemanticStepChannel, SemanticStepFrame
from semantic_state import MAX_EVENTS as SEMANTIC_STEP_MAX_EVENTS, latest_semantic_step_frame, read_semantic_step_trace
import startup_harness
from startup_harness import (
    adaptive_semantic_baseline_minutes,
    adaptive_semantic_session_identity,
    current_semantic_step_frame,
    execute_semantic_act,
    initialize_wait_diagnostic_ledger,
    refresh_semantic_step_trace,
    read_latest_activity_query_trace,
    semantic_step_frame_source_offset,
    semantic_step_trace_start_after_launch,
)


class SemanticStepChannelTest(unittest.TestCase):
    run_id = "run-natural-1"

    def frame(self, frame_id: str, state: str, actions: dict[str, str], turn: int) -> dict:
        return {
            "event": "frame",
            "run_id": self.run_id,
            "frame_id": frame_id,
            "state": state,
            "observed_turn": turn,
            "valid_actions": list(actions),
            "action_inputs": actions,
        }

    def test_named_wake_pipe_contract_and_writer_are_run_bound(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            read_fd, write_fd, environment = startup_harness.open_semantic_wake_pipe(root, self.run_id)
            try:
                self.assertTrue(environment["OPENCLAW_HARNESS_SEMANTIC_WAKE_READ_FD"].isdigit())
                self.assertFalse(os.get_blocking(read_fd))
                self.assertEqual(startup_harness.semantic_wake_pipe_contract(root, self.run_id)["status"], "bound")
                self.assertEqual(write_fd, -1)  # Named FIFO writers are opened per submission.
                self.assertEqual(startup_harness.write_semantic_wake_pipe(root, self.run_id), 1)
                self.assertEqual(os.read(read_fd, 1), b"w")
            finally:
                os.close(read_fd)

    def test_adaptive_session_identity_generates_when_no_supervisor_value_exists(self) -> None:
        with patch.dict("os.environ", {"OPENCLAW_ADAPTIVE_SESSION_ID": ""}):
            session_id, source = adaptive_semantic_session_identity()
        self.assertTrue(session_id.startswith("harness-"))
        self.assertEqual(source, "harness_generated")

    def test_adaptive_session_identity_preserves_supervisor_value(self) -> None:
        with patch.dict("os.environ", {"OPENCLAW_ADAPTIVE_SESSION_ID": "worker-060"}):
            session_id, source = adaptive_semantic_session_identity()
        self.assertEqual(session_id, "worker-060")
        self.assertEqual(source, "supervisor_supplied")

    def test_native_activity_frame_keeps_its_current_avatar_observation(self) -> None:
        frame = self.frame("activity-1", "activity_distraction", {
            "activity.ignore": "I",
        }, 101) | {
            "producer": "activity_distraction_query",
            "game_minutes": 8901,
            "observation": {
                "schema": "caol-avatar-visible-v1",
                "avatar": {"absolute_ms": [3844, 957, 0]},
                "visible_local": [],
                "visible_entities": [{
                    "fixture_actor_id": "r019-zombie-dog-positive-progress-v1",
                    "absolute_ms": [3845, 960, 0],
                }],
            },
        }

        public = SemanticStepFrame.from_mapping(frame).public()

        self.assertEqual(public["producer"], "activity_distraction_query")
        self.assertEqual(public["observed_turn"], 101)
        self.assertEqual(public["game_minutes"], 8901)
        self.assertEqual(
            public["observation"]["visible_entities"][0]["fixture_actor_id"],
            "r019-zombie-dog-positive-progress-v1",
        )

    def test_native_activity_frame_is_dispatched_as_an_activity_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            source.write_text("", encoding="utf-8")
            native = self.frame("activity-1", "activity_distraction", {
                "activity.continue": "N",
            }, 101) | {"producer": "activity_distraction_query"}
            active = {"event_offset": 7, "type": "eoc"}
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(source, source)), \
                    patch.object(startup_harness, "read_semantic_step_trace", return_value=([native], "ok")), \
                    patch.object(startup_harness, "semantic_step_frame_source_offset", return_value=10), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value=None), \
                    patch.object(startup_harness, "read_active_activity_query_trace", return_value=active):
                frame = current_semantic_step_frame(
                    profile="ignored", run_dir=root, run_id=self.run_id, start_offset=0,
                )
        self.assertEqual(frame["_kind"], "activity")
        self.assertEqual(frame["activity_query_offset"], 7)

    def test_adaptive_baseline_prefers_current_native_frame_over_prior_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            artifact_log = root / "debug.log"
            artifact_log.write_text("previous now marker\n", encoding="utf-8")
            with patch("startup_harness.latest_semantic_game_minutes_marker", return_value=8904), \
                    patch("startup_harness.latest_now_minutes_marker", return_value=8905):
                minutes, provenance = adaptive_semantic_baseline_minutes(
                    profile="r009-m095",
                    run_dir=root,
                    run_id=self.run_id,
                    start_offset=17,
                    artifact_log=artifact_log,
                )
        self.assertEqual(minutes, 8904)
        self.assertEqual(provenance, "native_semantic_frame.game_minutes")

    def test_truncated_startup_log_resets_the_semantic_trace_cursor(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text("previous run\n", encoding="utf-8")
            previous_size = trace.stat().st_size
            previous_identity = {
                "device": trace.stat().st_dev,
                "inode": trace.stat().st_ino,
            }
            trace.write_text("new\n", encoding="utf-8")

            self.assertEqual(
                semantic_step_trace_start_after_launch(
                    trace,
                    prelaunch_size=previous_size,
                    prelaunch_identity=previous_identity,
                ),
                0,
            )

    def test_append_only_startup_log_keeps_the_prior_run_cursor(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text("previous run\n", encoding="utf-8")
            previous_size = trace.stat().st_size
            previous_identity = {
                "device": trace.stat().st_dev,
                "inode": trace.stat().st_ino,
            }
            with trace.open("a", encoding="utf-8") as handle:
                handle.write("new run\n")

            self.assertEqual(
                semantic_step_trace_start_after_launch(
                    trace,
                    prelaunch_size=previous_size,
                    prelaunch_identity=previous_identity,
                ),
                previous_size,
            )

    def test_late_truncation_recovers_a_current_semantic_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            source.write_text("prior startup output\n" * 20, encoding="utf-8")
            stale_offset = source.stat().st_size
            event = self.frame("frame-current", "world", {"world.wait": "|"}, 100)
            source.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(event) + "\n",
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=self.run_id,
                    start_offset=stale_offset,
                )
            events, status = read_semantic_step_trace(owned, run_dir, self.run_id)
            self.assertEqual(status, "ok")
            self.assertEqual(events[0]["frame_id"], "frame-current")

    def test_frame_source_offset_uses_the_unfiltered_log_position(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "debug.log"
            event = self.frame("frame-current", "world", {"world.wait": "|"}, 100)
            prefix = "ordinary native output\n"
            source.write_text(
                prefix + "openclaw_harness_semantic_step: " + json.dumps(event) + "\n",
                encoding="utf-8",
            )
            self.assertEqual(
                semantic_step_frame_source_offset(
                    source, start_offset=0, frame_id="frame-current",
                ),
                len(prefix.encode("utf-8")),
            )

    def test_current_frame_keeps_the_original_log_offset(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            event = self.frame("frame-current", "world", {"world.wait": "|"}, 100)
            prefix = "activity query output omitted from semantic copy\n"
            source.write_text(
                prefix + "openclaw_harness_semantic_step: " + json.dumps(event) + "\n",
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                frame = current_semantic_step_frame(
                    profile="r009-m095", run_dir=run_dir, run_id=self.run_id,
                    start_offset=0,
                )

            self.assertEqual(frame["_event_offset"], len(prefix.encode("utf-8")))

    def test_current_frame_exposes_only_the_current_run_eoc_popup(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            event = self.frame("frame-wait", "wait_activity", {}, 100)
            source.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(event) + "\n" +
                "openclaw_harness_ui_trace: component=semantic_ui event=open "
                "instance_id=\"eoc-1\" run_id=\"other-run\" intent=\"eoc_popup\" "
                "valid_actions=[\"space\"] postcondition=\"eoc_popup_returned\"\n" +
                "openclaw_harness_ui_trace: component=semantic_ui event=open "
                f"instance_id=\"eoc-2\" run_id=\"{self.run_id}\" intent=\"eoc_popup\" "
                "valid_actions=[\"space\"] postcondition=\"eoc_popup_returned\"\n",
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                frame = current_semantic_step_frame(
                    profile="r009-m095", run_dir=run_dir, run_id=self.run_id,
                    start_offset=0,
                )

            self.assertEqual(frame["state"], "semantic_ui")
            self.assertEqual(frame["valid_actions"], ["modal.acknowledge"])
            self.assertEqual(frame["semantic_ui"]["instance_id"], "eoc-2")
            self.assertEqual(frame["keep_watch_safety"], {
                "classification": "safe_prompt",
                "monster": False,
                "danger": False,
                "damage": False,
                "action_id": "modal.acknowledge",
                "recovery": {"modal_id": "eoc-2"},
            })

    def test_current_frame_resets_to_the_rotated_log_coordinate(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            stale_offset = 100000
            event = self.frame("frame-current", "world", {"world.wait": "|"}, 100)
            prefix = "new log generation output\n"
            source.write_text(
                prefix + "openclaw_harness_semantic_step: " + json.dumps(event) + "\n",
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                frame = current_semantic_step_frame(
                    profile="r009-m095", run_dir=run_dir, run_id=self.run_id,
                    start_offset=stale_offset,
                )

            self.assertEqual(frame["_event_offset"], len(prefix.encode("utf-8")))

    def test_refresh_reads_only_the_sampled_live_log_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            event = self.frame("frame-live", "world", {"world.wait": "|"}, 100)
            source.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(event) + "\n",
                encoding="utf-8",
            )
            sampled_size = source.stat().st_size
            with source.open("a", encoding="utf-8") as handle:
                handle.write("openclaw_harness_semantic_step: not-json\n")

            original_stat = Path.stat

            def sampled_stat(path: Path):
                if Path(path).absolute() == source.absolute():
                    return type("Stat", (), {"st_size": sampled_size})()
                return original_stat(path)

            # The snapshot length is the production reader's boundary: a
            # later partial writer must not contaminate the current frame.
            with patch("startup_harness.Path.stat", new=sampled_stat), \
                    patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="r009-m095", run_dir=run_dir, run_id=self.run_id,
                    start_offset=0,
                )
            events, status = read_semantic_step_trace(owned, run_dir, self.run_id)
            self.assertEqual(status, "ok")
            self.assertEqual(events[0]["frame_id"], "frame-live")

    def test_same_session_observes_parent_then_duration_and_receives_next_state(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            frames = [
                self.frame("frame-1", "wait_mode_choice", {
                    "wait.duration_menu": "w", "alarm.duration_menu": "a",
                }, 100),
                self.frame("frame-2", "wait_duration_choice", {
                    "wait.30m": "4", "wait.6h": "8",
                }, 100),
                self.frame("frame-3", "wait_activity", {}, 100),
            ]
            current = 0

            def read_frame() -> dict:
                return frames[current]

            def transition(frame_id: str, action_id: str) -> dict:
                nonlocal current
                current += 1
                return {
                    "native_receipt": {
                        "frame_id": frame_id, "action_id": action_id, "accepted": True,
                    },
                    "semantic_response": {
                        "frame_id": frame_id, "action_id": action_id, "accepted": True,
                    },
                    "next_frame": frames[current],
                }

            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-1",
                receipt_path=root / "semantic.steps.jsonl", read_frame=read_frame,
            )
            self.assertEqual(channel.observe()["valid_actions"], [
                "wait.duration_menu", "alarm.duration_menu",
            ])
            parent = channel.act(
                frame_id="frame-1", action_id="wait.duration_menu",
                submit_request=lambda *_: {"accepted": True}, await_transition=transition,
            )
            duration = channel.act(
                frame_id="frame-2", action_id="wait.6h",
                submit_request=lambda *_: {"accepted": True}, await_transition=transition,
            )
            self.assertTrue(parent["accepted"])
            self.assertTrue(duration["accepted"])
            self.assertEqual(duration["next_frame"]["state"], "wait_activity")
            receipts = [
                json.loads(line)
                for line in (root / "semantic.steps.jsonl").read_text(encoding="utf-8").splitlines()
            ]
            self.assertEqual({item["session_id"] for item in receipts}, {"worker-1"})

    def test_atomic_interruption_uses_issuing_frame_after_live_source_reaches_world(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            interruption = self.frame("activity-1", "activity_distraction", {
                "activity.ignore": "I",
            }, 101)
            world = self.frame("world-2", "world", {"world.wait": "|"}, 101)
            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-atomic",
                receipt_path=root / "semantic.steps.jsonl", read_frame=lambda: world,
            )

            receipt = channel.act_observed(
                observed_frame=interruption,
                frame_id="activity-1",
                action_id="activity.ignore",
                submit_request=lambda *_: {"accepted": True},
                await_transition=lambda frame_id, action_id: {
                    "native_receipt": {
                        "frame_id": frame_id, "action_id": action_id, "accepted": True,
                    },
                    "semantic_response": {
                        "frame_id": frame_id, "action_id": action_id, "accepted": True,
                        "kind": "activity_distraction_return", "resolved_action": "IGNORE",
                    },
                    "next_frame": world,
                },
            )

            self.assertTrue(receipt["accepted"])
            self.assertEqual(receipt["current_frame"]["frame_id"], "activity-1")
            self.assertEqual(receipt["next_frame"]["frame_id"], "world-2")
            self.assertEqual(receipt["semantic_response"]["resolved_action"], "IGNORE")
            durable = json.loads((root / "semantic.steps.jsonl").read_text(encoding="utf-8"))
            self.assertNotIn("_next_frame", durable)
            self.assertNotIn("action_inputs", durable["next_frame"])

    def test_atomic_interruption_rejects_world_without_matching_semantic_response(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            interruption = self.frame("activity-1", "activity_distraction", {
                "activity.ignore": "I",
            }, 101)
            world = self.frame("world-2", "world", {"world.wait": "|"}, 101)
            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-atomic",
                receipt_path=root / "semantic.steps.jsonl", read_frame=lambda: world,
            )

            receipt = channel.act_observed(
                observed_frame=interruption,
                frame_id="activity-1",
                action_id="activity.ignore",
                submit_request=lambda *_: {"accepted": True},
                await_transition=lambda frame_id, action_id: {
                    "native_receipt": {
                        "frame_id": frame_id, "action_id": action_id, "accepted": True,
                    },
                    "semantic_response": {
                        "frame_id": frame_id, "action_id": "activity.continue", "accepted": True,
                    },
                    "next_frame": world,
                },
            )

            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "semantic_response_mismatch")
            self.assertIsNone(receipt["next_frame"])

    def test_activity_return_keeps_the_issuing_opening_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="hostile" truncated=no action=none\n'
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=return type=hostile_spotted_far text="hostile" truncated=no action=IGNORE\n',
                encoding="utf-8",
            )
            returned = read_latest_activity_query_trace(trace, 0)
            self.assertEqual(returned["event"], "return")
            self.assertEqual(returned["issuing_open_offset"], 0)

    def test_activity_return_is_a_fresh_frame_without_a_later_semantic_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "debug.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="hostile" truncated=no action=none\n'
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=return type=hostile_spotted_far text="hostile" truncated=no action=IGNORE\n',
                encoding="utf-8",
            )
            with patch.object(startup_harness, "semantic_step_source_trace", return_value=trace):
                frame = startup_harness.current_semantic_step_frame(
                    profile="ignored", run_dir=root, run_id=self.run_id, start_offset=0,
                )
            self.assertEqual(frame["state"], "activity_resumed")
            self.assertEqual(frame["provenance"], "native_activity_distraction_return")
            self.assertEqual(frame["resolved_action"], "IGNORE")

    def test_activity_return_yields_a_later_native_world_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            source.write_text("native trace", encoding="utf-8")
            descriptor = {
                "event": "surface_descriptor",
                "run_id": self.run_id,
                "surface_id": "activity-surface",
                "frame_id": "activity-frame",
                "kind": "activity_distraction",
                "valid_actions": [],
            }
            resumed_world = self.frame("world-after-ignore", "world", {
                "world.wait": "|",
            }, 102)
            returned = {"event": "return", "event_offset": 20, "action": "IGNORE"}
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(source, source)), \
                    patch.object(startup_harness, "read_semantic_step_trace",
                                 return_value=([descriptor, resumed_world], "ok")), \
                    patch.object(startup_harness, "semantic_step_frame_source_offset",
                                 side_effect=[10, 30]), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value=returned), \
                    patch.object(startup_harness, "read_active_activity_query_trace", return_value=None):
                frame = current_semantic_step_frame(
                    profile="ignored", run_dir=root, run_id=self.run_id, start_offset=0,
                )
            self.assertEqual(frame["frame_id"], "world-after-ignore")
            self.assertEqual(frame["state"], "world")
            self.assertEqual(frame["_event_offset"], 30)

    def test_world_descriptor_projects_only_its_bound_native_clock(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            source.write_text("native trace", encoding="utf-8")
            descriptor = {
                "event": "surface_descriptor", "run_id": self.run_id,
                "surface_id": "world-surface", "frame_id": "world-frame",
                "kind": "world", "valid_actions": [],
            }
            native_frame = {
                "event": "frame", "run_id": self.run_id,
                "frame_id": "native-world-frame", "state": "world",
                "game_minutes": 9241,
            }
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(source, source)), \
                    patch.object(startup_harness, "read_semantic_step_trace",
                                 side_effect=[([descriptor], "ok"),
                                              ([descriptor, native_frame], "ok")]), \
                    patch.object(startup_harness, "semantic_step_frame_source_offset", return_value=10), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value=None), \
                    patch.object(startup_harness, "read_active_activity_query_trace", return_value=None):
                frame = current_semantic_step_frame(
                    profile="ignored", run_dir=root, run_id=self.run_id, start_offset=0,
                )
            self.assertEqual(frame["game_minutes"], 9241)

    def test_activity_return_uses_the_bound_open_outside_the_moving_tail(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            opening = (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="hostile" truncated=no action=none\n'
            )
            prefix = "x" * startup_harness.EOC_POPUP_TRACE_READ_CAP_BYTES
            trace.write_text(
                prefix + opening +
                ( "x" * ( startup_harness.EOC_POPUP_TRACE_READ_CAP_BYTES + 1024 ) ) +
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=return type=hostile_spotted_far text="hostile" truncated=no action=IGNORE\n',
                encoding="utf-8",
            )
            returned = read_latest_activity_query_trace(trace, 0, len(prefix))
            self.assertEqual(returned["event"], "return")
            self.assertEqual(returned["issuing_open_offset"], len(prefix))

    def test_activity_return_rejects_a_mismatched_issuing_open(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="hostile" truncated=no action=none\n'
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=return type=hostile_spotted_far text="hostile" truncated=no action=IGNORE\n',
                encoding="utf-8",
            )
            self.assertIsNone(read_latest_activity_query_trace(trace, 0, 1))

    def test_activity_return_rejects_a_stale_pair_before_the_issuing_open(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            stale_pair = (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="stale" truncated=no action=none\n'
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=return type=hostile_spotted_far text="stale" truncated=no action=IGNORE\n'
            )
            current_open = (
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=hostile_spotted_far text="current" truncated=no action=none\n'
            )
            trace.write_text(stale_pair + current_open, encoding="utf-8")
            self.assertIsNone(
                read_latest_activity_query_trace(trace, 0, len(stale_pair))
            )

    def test_activity_recovery_dispatches_from_issuing_frame_and_requires_return(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            interruption = self.frame("activity-1", "activity_distraction", {
                "activity.ignore": "I",
            }, 101) | {
                "_kind": "activity", "_event_offset": 10,
                "activity_query_offset": 7,
            }
            world = self.frame("world-2", "world", {"world.wait": "|"}, 101) | {
                "_event_offset": 20,
            }
            sent = []
            with patch.object(startup_harness, "current_semantic_step_frame", return_value=world), \
                    patch.object(startup_harness, "dispatch_semantic_input", create=True,
                                 side_effect=lambda *args, **kwargs: sent.append((args, kwargs))), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value={
                        "event": "return", "action": "IGNORE", "issuing_open_offset": 7,
                        "event_offset": 21,
                    }) as read_receipt:
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=8, pid=17, session_id="worker-atomic",
                    frame_id="activity-1", action_id="activity.ignore",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=interruption,
                )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_surface_descriptor_required")
            self.assertEqual(sent, [])

    def test_activity_recovery_recovers_missing_query_metadata_from_matching_native_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            interruption = self.frame("activity-1", "activity_distraction", {
                "activity.ignore": "I",
            }, 101) | {
                "_kind": "semantic_step", "_event_offset": 10,
                "producer": "activity_distraction_query",
                "observation": {
                    "schema": "caol-avatar-visible-v1",
                    "avatar": {"name": "tester"},
                    "visible_local": [],
                },
            }
            world = self.frame("world-2", "world", {"world.wait": "|"}, 101) | {
                "_event_offset": 20,
            }
            with patch.object(startup_harness, "current_semantic_step_frame", return_value=world), \
                    patch.object(startup_harness, "dispatch_semantic_input", create=True), \
                    patch.object(startup_harness, "semantic_step_source_trace", return_value=root / "debug.log"), \
                    patch.object(startup_harness, "read_active_activity_query_trace", return_value={
                        "event": "open", "type": "noise", "game_minutes": 101,
                        "event_offset": 7,
                    }), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value={
                        "event": "return", "action": "IGNORE", "issuing_open_offset": 7,
                        "event_offset": 21,
                    }) as read_receipt:
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=8, pid=17, session_id="worker-atomic",
                    frame_id="activity-1", action_id="activity.ignore",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=interruption,
                )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_surface_descriptor_required")

    def test_native_action_retries_a_transient_missing_semantic_frame(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            issuing = self.frame("wait-1", "world", {"world.wait": "|"}, 101) | {
                "_event_offset": 10,
            }
            successor = self.frame("wait-2", "wait_mode_choice", {
                "wait.duration_menu": "w",
            }, 101) | {"_event_offset": 20}
            source = root / "debug.log"
            source.write_text("semantic trace", encoding="utf-8")
            receipt_event = {
                "event": "receipt", "frame_id": "wait-1", "action_id": "world.wait",
                "accepted": True, "_event_offset": 15,
            }
            with patch.object(
                startup_harness, "current_semantic_step_frame",
                side_effect=[ValueError("the current run has not emitted a semantic frame"), successor],
            ), patch.object(startup_harness, "dispatch_semantic_input", create=True), \
                    patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(source, source)), \
                    patch.object(startup_harness, "semantic_step_effective_source_offset", return_value=0), \
                    patch.object(startup_harness, "read_semantic_step_trace",
                                 return_value=([receipt_event], "ok")):
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker-atomic",
                    frame_id="wait-1", action_id="world.wait",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=issuing,
                )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_surface_descriptor_required")

    def test_single_focus_activity_chord_uses_hotkey_without_refocus(self) -> None:
        with patch.object(startup_harness, "peekaboo_focus_pid", return_value={"ok": True}), \
                patch.object(startup_harness, "peekaboo_hotkey") as hotkey, \
                patch.object(startup_harness, "run_peekaboo_interaction") as press:
            startup_harness.peekaboo_press_sequence(17, ["I"], focus_once=True)
        hotkey.assert_called_once_with(17, "shift,i", hold_ms=200, focus_pid=False)
        press.assert_not_called()

    def test_later_world_frame_without_activity_return_stays_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            interruption = self.frame("activity-1", "activity_distraction", {
                "activity.ignore": "I",
            }, 101) | {"_kind": "activity", "_event_offset": 10}
            world = self.frame("world-2", "world", {"world.wait": "|"}, 101) | {
                "_event_offset": 20,
            }
            with patch.object(startup_harness, "current_semantic_step_frame", return_value=world), \
                    patch.object(startup_harness, "dispatch_semantic_input", create=True), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value=None):
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker-atomic",
                    frame_id="activity-1", action_id="activity.ignore",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=interruption,
                )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_surface_descriptor_required")

    def test_live_wait_dispatch_retains_bound_diagnostic_records(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            initialize_wait_diagnostic_ledger(
                root, enabled=True, run_id=self.run_id, executable=Path(sys.executable),
            )
            wait = self.frame("wait-1", "wait_duration_choice", {"wait.1h": "h"}, 101)
            with patch.object(startup_harness, "classify_wait_input_trace", return_value={
                "status": "wait_dispatched",
            }):
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker-atomic",
                    frame_id="wait-1", action_id="wait.1h",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=wait,
            )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_surface_descriptor_required")

    def test_original_wrong_menu_choice_and_stale_frame_are_rejected_without_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            current = self.frame("frame-parent", "wait_mode_choice", {
                "wait.duration_menu": "w", "alarm.duration_menu": "a",
            }, 100)
            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-1",
                receipt_path=root / "semantic.steps.jsonl", read_frame=lambda: current,
            )
            wrong_menu = channel.act(
                frame_id="frame-parent", action_id="wait.6h",
                submit_request=lambda *_: {"accepted": True}, await_transition=lambda *_: {},
            )
            stale = channel.act(
                frame_id="frame-old", action_id="wait.duration_menu",
                submit_request=lambda *_: {"accepted": True}, await_transition=lambda *_: {},
            )
            self.assertEqual(wrong_menu["reason"], "action_not_advertised")
            self.assertEqual(stale["reason"], "stale_frame")

    def test_native_trace_parser_rejects_contamination_and_preserves_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "debug.log"
            events = [
                self.frame("frame-1", "wait_mode_choice", {"wait.duration_menu": "w"}, 100),
                {
                    "event": "receipt", "run_id": self.run_id, "frame_id": "frame-1",
                    "action_id": "wait.duration_menu", "accepted": True, "observed_turn": 100,
                },
            ]
            trace.write_text("".join(
                "openclaw_harness_semantic_step: " + json.dumps(event) + "\n"
                for event in events
            ), encoding="utf-8")
            parsed, status = read_semantic_step_trace(trace, root, self.run_id)
            self.assertEqual(status, "ok")
            self.assertTrue(latest_semantic_step_frame(parsed)["native_receipt"]["accepted"])
            trace.write_text(
                trace.read_text(encoding="utf-8")
                + "openclaw_harness_semantic_step: "
                + json.dumps(self.frame("other", "wait_activity", {}, 100) | {"run_id": "other"})
                + "\n",
                encoding="utf-8",
            )
            self.assertEqual(read_semantic_step_trace(trace, root, self.run_id)[1], "contamination")

    def test_surface_descriptor_is_the_current_observation_and_binds_its_receipt(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {},
            "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": "request-1",
            "requested_run_id": self.run_id, "requested_surface_id": "surface-world",
            "requested_frame_id": "surface-frame", "consuming_surface_id": "surface-world",
            "consuming_frame_id": "surface-frame", "action_id": "world.wait", "accepted": False,
            "rejection_reason": "no_native_binding", "resulting_frame_id": "",
        }
        later_legacy = self.frame("legacy-after", "activity_distraction", {
            "activity.ignore": "I",
        }, 101)
        latest = latest_semantic_step_frame([
            self.frame("legacy-before", "world", {}, 100), descriptor, receipt, later_legacy,
        ])
        self.assertEqual(latest["event"], "surface_descriptor")
        self.assertEqual(latest["surface_id"], "surface-world")
        self.assertEqual(latest["native_receipt"]["request_id"], "request-1")

    def test_world_descriptor_keeps_private_same_run_map_projection_for_guarded_movement(self) -> None:
        native_world = self.frame("legacy-world", "world", {}, 100) | {
            "observation": {
                "avatar": {"absolute_ms": [3372, 996, 1]},
                "visible_local": [], "visible_entities": [],
            },
            "keep_watch_safety": {
                "classification": "clear", "monster": False, "danger": False, "damage": False,
            },
        }
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        latest = latest_semantic_step_frame([native_world, descriptor])
        self.assertEqual(latest["event"], "surface_descriptor")
        self.assertEqual(latest["observation"]["avatar"]["absolute_ms"], [3372, 996, 1])
        self.assertEqual(latest["keep_watch_safety"]["classification"], "clear")
        self.assertEqual(latest["state"], "world")
        self.assertEqual(latest["provenance"], "native_semantic_step_trace")

    def test_current_semantic_observation_returns_the_production_descriptor(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(descriptor) + "\n",
                encoding="utf-8",
            )
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                current = current_semantic_step_frame(
                    profile="ignored", run_dir=root, run_id=self.run_id, start_offset=0,
                )
        self.assertEqual(current["event"], "surface_descriptor")
        self.assertEqual(current["frame_id"], "surface-frame")

    def test_surface_descriptor_action_uses_native_request_transport_only(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1) as wake:
            receipt = execute_semantic_act(
                run_dir=Path(temp), profile="ignored", run_id=self.run_id,
                trace_start_offset=0, pid=17, session_id="worker", frame_id="surface-frame",
                action_id="world.wait", transition_timeout_seconds=0.1,
                observe_interval_seconds=0.01, observed_frame=descriptor,
            )
        self.assertFalse(receipt["accepted"])
        self.assertEqual(receipt["reason"], "native_surface_receipt_timeout")
        self.assertEqual(receipt["surface_request"]["action_id"], "world.wait")
        wake.assert_called_once_with(Path(temp).resolve(), self.run_id)
        self.assertNotIn("dispatch_semantic_input", inspect.getsource(startup_harness.execute_semantic_act))

    def test_surface_descriptor_preserves_accepted_receipt_when_successor_is_missing(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
            root = Path(temp)
            trace = root / "semantic.native.log"
            request_id = (
                "cockpit:worker:surface-frame:world.wait:" +
                hashlib.sha256(json.dumps({"stable_id": None, "parameters": {}},
                                           sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()[:16]
            )
            receipt = {
                "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
                "requested_run_id": self.run_id, "requested_surface_id": "surface-world",
                "requested_frame_id": "surface-frame", "consuming_surface_id": "surface-world",
                "consuming_frame_id": "surface-frame", "action_id": "world.wait", "accepted": True,
                "rejection_reason": "", "resulting_frame_id": "",
            }
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (descriptor, receipt)
            ) + "\n", encoding="utf-8")
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="surface-frame",
                    action_id="world.wait", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.01, observed_frame=descriptor,
                )
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "native_surface_successor_timeout")
        self.assertEqual(result["native_receipt"]["request_id"], request_id)
        self.assertTrue(result["native_receipt"]["accepted"])
        self.assertIsNone(result["next_frame"])

    def test_surface_descriptor_request_preserves_native_receipt_identity(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        request_id = (
            "cockpit:worker:surface-frame:world.wait:" +
            hashlib.sha256( json.dumps( {
                "stable_id": None, "parameters": {},
            }, sort_keys=True, separators=( ",", ":" ) ).encode( "utf-8" ) ).hexdigest()[:16]
        )
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
            "requested_run_id": self.run_id, "requested_surface_id": "surface-world",
            "requested_frame_id": "surface-frame", "consuming_surface_id": "surface-world",
            "consuming_frame_id": "surface-frame", "action_id": "world.wait", "accepted": True,
            "rejection_reason": "", "resulting_frame_id": "surface-frame-next",
        }
        successor = {**descriptor, "frame_id": "surface-frame-next"}
        later_descriptor = {**descriptor, "frame_id": "surface-frame-later"}
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1) as wake:
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (descriptor, successor, receipt, later_descriptor)
            ) + "\n", encoding="utf-8")
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="surface-frame",
                    action_id="world.wait", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.01, observed_frame=descriptor,
                    proof_step_label="witness", proof_step_index=7,
                )
            request = json.loads((root / "semantic.requests.jsonl").read_text(encoding="utf-8"))
            transition = json.loads((root / "transition.events.jsonl").read_text(encoding="utf-8"))
        self.assertTrue(result["accepted"])
        self.assertEqual(result["native_receipt"]["request_id"], request_id)
        self.assertEqual(result["native_receipt"]["requested_frame_id"], request["frame_id"])
        self.assertEqual(result["next_frame"]["frame_id"], "surface-frame-next")
        self.assertEqual(transition["run_id"], self.run_id)
        self.assertEqual(transition["request_id"], request_id)
        self.assertEqual(transition["proof_step_label"], "witness")
        self.assertEqual(transition["proof_step_index"], 7)
        self.assertEqual(transition["native_receipt"]["resulting_frame_id"], "surface-frame-next")
        wake.assert_called_once_with(root.resolve(), self.run_id)

    def test_surface_descriptor_retains_receipt_published_during_final_poll_sleep(self) -> None:
        """A bound native receipt must win over a just-expired poll deadline."""
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        successor = {**descriptor, "frame_id": "surface-frame-next"}
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1), \
                patch.object(startup_harness, "refresh_semantic_step_trace",
                             return_value=(Path(temp) / "trace", Path(temp) / "trace")), \
                patch.object(startup_harness.time, "monotonic", side_effect=[0.0, 0.05]), \
                patch.object(startup_harness.time, "sleep") as sleep:
            request_path = Path(temp) / "semantic.requests.jsonl"
            calls = 0

            def read_trace(*_args: object) -> tuple[list[dict], str]:
                nonlocal calls
                calls += 1
                if calls == 1:
                    return [descriptor], "ok"
                request = json.loads(request_path.read_text(encoding="utf-8"))
                receipt = {
                    "event": "surface_receipt", "run_id": self.run_id,
                    "request_id": request["request_id"], "requested_run_id": self.run_id,
                    "requested_surface_id": "surface-world", "requested_frame_id": "surface-frame",
                    "consuming_surface_id": "surface-world", "consuming_frame_id": "surface-frame",
                    "action_id": "world.wait", "accepted": True, "rejection_reason": "",
                    "resulting_frame_id": "surface-frame-next",
                }
                return [descriptor, receipt, successor], "ok"

            with patch.object(startup_harness, "read_semantic_step_trace", side_effect=read_trace):
                result = execute_semantic_act(
                    run_dir=Path(temp), profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="surface-frame",
                    action_id="world.wait", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.1, observed_frame=descriptor,
                )
        self.assertTrue(result["accepted"], f"trace polls={calls}; result={result}")
        self.assertEqual(result["native_receipt"]["request_id"], "cockpit:worker:surface-frame:world.wait:" +
                         hashlib.sha256(json.dumps({"stable_id": None, "parameters": {}},
                                                   sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()[:16])
        sleep.assert_called_once_with(0.05)

    def test_surface_descriptor_rejects_receipt_with_mismatched_owner_binding(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "surface-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        request_id = (
            "cockpit:worker:surface-frame:world.wait:" +
            hashlib.sha256( json.dumps( {
                "stable_id": None, "parameters": {},
            }, sort_keys=True, separators=( ",", ":" ) ).encode( "utf-8" ) ).hexdigest()[:16]
        )
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
            "requested_run_id": self.run_id, "requested_surface_id": "surface-other",
            "requested_frame_id": "surface-frame", "consuming_surface_id": "surface-world",
            "consuming_frame_id": "surface-frame", "action_id": "world.wait", "accepted": True,
            "rejection_reason": "", "resulting_frame_id": "surface-frame-next",
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(descriptor) + "\n" +
                "openclaw_harness_semantic_step: " + json.dumps(receipt) + "\n",
                encoding="utf-8",
            )
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="surface-frame",
                    action_id="world.wait", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.01, observed_frame=descriptor,
                )
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "native_surface_receipt_correlation_mismatch")
        self.assertEqual(result["mismatched_fields"]["requested_surface_id"], {
            "expected": "surface-world", "actual": "surface-other",
        })

    def test_wait_parent_resume_yields_to_the_fresh_duration_owner(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-world", "frame_id": "world-frame", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "world.wait", "stable_id": "world.wait", "label": "Wait", "enabled": True,
            }],
        }
        request_id = (
            "cockpit:worker:world-frame:world.wait:" +
            hashlib.sha256( json.dumps( {
                "stable_id": None, "parameters": {},
            }, sort_keys=True, separators=( ",", ":" ) ).encode( "utf-8" ) ).hexdigest()[:16]
        )
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
            "requested_run_id": self.run_id, "requested_surface_id": "surface-world",
            "requested_frame_id": "world-frame", "consuming_surface_id": "surface-world",
            "consuming_frame_id": "world-frame", "action_id": "world.wait", "accepted": True,
            "rejection_reason": "", "resulting_frame_id": "resumed-world-frame",
        }
        resumed_parent = {**descriptor, "frame_id": "resumed-world-frame"}
        duration_owner = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-wait-duration", "frame_id": "duration-frame", "kind": "menu",
            "breadcrumbs": ["World", "Wait duration"], "payload": {}, "valid_actions": [{
                "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
            }],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (descriptor, resumed_parent, receipt, duration_owner)
            ) + "\n", encoding="utf-8")
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="world-frame",
                    action_id="world.wait", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.01, observed_frame=descriptor,
                )
        self.assertTrue(result["accepted"])
        self.assertEqual(result["next_frame"]["frame_id"], "duration-frame")

    def test_immediate_dialogue_receipt_binds_its_fresh_child_descriptor(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-dialogue", "frame_id": "dialogue-frame", "kind": "dialogue",
            "breadcrumbs": ["World", "NPC"], "payload": {}, "valid_actions": [{
                "id": "dialogue.choose", "stable_id": "response-1", "label": "Rules", "enabled": True,
            }],
        }
        request_id = (
            "cockpit:worker:dialogue-frame:dialogue.choose:" +
            hashlib.sha256( json.dumps( {
                "stable_id": "response-1", "parameters": {},
            }, sort_keys=True, separators=( ",", ":" ) ).encode( "utf-8" ) ).hexdigest()[:16]
        )
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
            "requested_run_id": self.run_id, "requested_surface_id": "surface-dialogue",
            "requested_frame_id": "dialogue-frame", "consuming_surface_id": "surface-dialogue",
            "consuming_frame_id": "dialogue-frame", "action_id": "dialogue.choose", "accepted": True,
            "rejection_reason": "", "resulting_frame_id": "",
        }
        child = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-rules", "frame_id": "rules-frame", "kind": "npc_rules_menu",
            "breadcrumbs": ["World", "NPC", "Rules"], "payload": {}, "valid_actions": [],
        }
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (descriptor, receipt, child)
            ) + "\n", encoding="utf-8")
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="dialogue-frame",
                    action_id="dialogue.choose", stable_id="response-1",
                    transition_timeout_seconds=0.1, observe_interval_seconds=0.01,
                    observed_frame=descriptor,
                )
        self.assertTrue(result["accepted"])
        self.assertEqual(result["native_receipt"]["resulting_frame_id"], "")
        self.assertEqual(result["next_frame"]["frame_id"], "rules-frame")

    def test_bound_process_exit_retains_only_matching_actual_terminal_receipt(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "quit-prompt", "frame_id": "quit-frame", "kind": "prompt",
            "breadcrumbs": ["Main menu", "Quit?"], "payload": {}, "valid_actions": [{
                "id": "prompt.choose", "stable_id": "prompt-option:3", "label": "YES", "enabled": True,
            }],
        }
        request_id = "cockpit:worker:quit-frame:prompt.choose:" + hashlib.sha256(json.dumps({
            "stable_id": "prompt-option:3", "parameters": {},
        }, sort_keys=True, separators=(",", ":")).encode()).hexdigest()[:16]
        native = {"event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
                  "requested_run_id": self.run_id, "requested_surface_id": "quit-prompt",
                  "requested_frame_id": "quit-frame", "consuming_surface_id": "quit-prompt",
                  "consuming_frame_id": "quit-frame", "action_id": "prompt.choose", "accepted": True,
                  "rejection_reason": "", "resulting_frame_id": ""}
        for present, alive, wrong_run, wrong_receipt in ((True, False, False, False),
                (False, False, False, False), (True, True, False, False),
                (True, False, True, False), (True, False, False, True)):
            with self.subTest(present=present, alive=alive, wrong_run=wrong_run, wrong_receipt=wrong_receipt), \
                    tempfile.TemporaryDirectory() as temp, \
                    patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={"status": "bound", "path": "pipe-contract"}), \
                    patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
                root = Path(temp)
                trace = root / "semantic.native.log"
                receipt = {**native, **({"consuming_frame_id": "other-frame"} if wrong_receipt else {})}
                trace.write_text("\n".join("openclaw_harness_semantic_step: " + json.dumps(event)
                                           for event in ([descriptor, receipt] if present else [descriptor])) + "\n")
                with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                    result = execute_semantic_act(
                        run_dir=root, profile="ignored", run_id=self.run_id, trace_start_offset=0,
                        pid=17, session_id="worker", frame_id="quit-frame", action_id="prompt.choose",
                        stable_id="prompt-option:3", observed_frame=descriptor,
                        transition_timeout_seconds=0.02, observe_interval_seconds=0.001,
                        read_process_state=lambda: {"run_id": "wrong-run" if wrong_run else self.run_id,
                                                    "pid": 17, "alive": alive, "exit_code": 0},
                    )
                expected = present and not alive and not wrong_run and not wrong_receipt
                self.assertEqual(result["accepted"], expected)
                if expected:
                    self.assertEqual(result["native_receipt"]["request_id"], request_id)
                    self.assertIsNone(result["next_frame"])
                    durable = json.loads((root / "semantic.steps.jsonl").read_text())
                    self.assertTrue(durable["native_receipt"]["accepted"])
                elif not present:
                    self.assertIsNone(result["native_receipt"])
                elif wrong_receipt:
                    self.assertEqual(result["reason"], "native_surface_receipt_correlation_mismatch")

    def test_surface_descriptor_serializes_the_advertised_stable_id(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": self.run_id,
            "surface_id": "surface-inventory", "frame_id": "inventory-frame", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {}, "valid_actions": [{
                "id": "inventory.select", "stable_id": "item-uid-42", "label": "flashlight",
                "enabled": True,
            }],
        }
        request_id = (
            "cockpit:worker:inventory-frame:inventory.select:" +
            hashlib.sha256( json.dumps( {
                "stable_id": "item-uid-42", "parameters": {},
            }, sort_keys=True, separators=( ",", ":" ) ).encode( "utf-8" ) ).hexdigest()[:16]
        )
        receipt = {
            "event": "surface_receipt", "run_id": self.run_id, "request_id": request_id,
            "requested_run_id": self.run_id, "requested_surface_id": "surface-inventory",
            "requested_frame_id": "inventory-frame", "consuming_surface_id": "surface-inventory",
            "consuming_frame_id": "inventory-frame", "action_id": "inventory.select", "accepted": True,
            "rejection_reason": "", "resulting_frame_id": "inventory-next",
        }
        successor = {**descriptor, "frame_id": "inventory-next"}
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={
                    "status": "bound", "path": "pipe-contract",
                }), \
                patch.object(startup_harness, "write_semantic_wake_pipe", return_value=1):
            root = Path(temp)
            trace = root / "semantic.native.log"
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (descriptor, receipt, successor)
            ) + "\n", encoding="utf-8")
            with patch.object(startup_harness, "refresh_semantic_step_trace", return_value=(trace, trace)):
                missing = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="inventory-frame",
                    action_id="inventory.select", transition_timeout_seconds=0.1,
                    observe_interval_seconds=0.01, observed_frame=descriptor,
                )
                result = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker", frame_id="inventory-frame",
                    action_id="inventory.select", stable_id="item-uid-42",
                    transition_timeout_seconds=0.1, observe_interval_seconds=0.01,
                    observed_frame=descriptor,
                )
            request = json.loads((root / "semantic.requests.jsonl").read_text(encoding="utf-8"))
        self.assertEqual(missing["reason"], "missing_stable_id")
        self.assertTrue(result["accepted"])
        self.assertEqual(request["stable_id"], "item-uid-42")

    def test_unrelated_debug_volume_cannot_overrun_semantic_channel(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            source.write_text(
                ("unrelated debug noise\n" * 20000)
                + "openclaw_harness_semantic_step: "
                + json.dumps(self.frame("frame-live", "wait_activity", {}, 100))
                + "\n",
                encoding="utf-8",
            )
            run_dir = root / "run"
            run_dir.mkdir()
            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="ignored", run_dir=run_dir, run_id=self.run_id,
                    start_offset=0,
                )
            parsed, status = read_semantic_step_trace(owned, run_dir, self.run_id)
            self.assertEqual(status, "ok")
            self.assertEqual(latest_semantic_step_frame(parsed)["frame_id"], "frame-live")
            self.assertNotIn("unrelated debug noise", owned.read_text(encoding="utf-8"))

    def test_refresh_ignores_unbounded_prior_run_events_before_current_run_cap(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            prior = self.frame("frame-prior", "wait_activity", {}, 100) | {"run_id": "prior-run"}
            current = self.frame("frame-current", "wait_activity", {}, 100)
            source.write_text(
                "".join(
                    "openclaw_harness_semantic_step: " + json.dumps(prior) + "\n"
                    for _ in range(2000)
                ) + "openclaw_harness_semantic_step: " + json.dumps(current) + "\n",
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="ignored", run_dir=run_dir, run_id=self.run_id, start_offset=0,
                )

            events, status = read_semantic_step_trace(owned, run_dir, self.run_id)
            self.assertEqual(status, "ok")
            self.assertEqual([event["frame_id"] for event in events], ["frame-current"])

    def test_refresh_bounds_unbounded_current_run_events(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            current = self.frame("frame-current", "wait_activity", {}, 100)
            source.write_text(
                "".join(
                    "openclaw_harness_semantic_step: " + json.dumps(current) + "\n"
                    for _ in range(2000)
                ),
                encoding="utf-8",
            )

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="ignored", run_dir=run_dir, run_id=self.run_id, start_offset=0,
                )

            events, status = read_semantic_step_trace( owned, run_dir, self.run_id )
            self.assertEqual( status, "ok" )
            self.assertLessEqual( len( events ), SEMANTIC_STEP_MAX_EVENTS )
            self.assertEqual( events[-1]["frame_id"], "frame-current" )

    def test_cursor_keeps_only_the_unconsumed_current_run_window(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "debug.log"
            run_dir = root / "run"
            run_dir.mkdir()
            history = self.frame("history", "wait_activity", {}, 100)
            fresh = self.frame("fresh", "world", {"world.wait": "."}, 101)
            source.write_text(
                "".join(
                    "openclaw_harness_semantic_step: " + json.dumps(history | {
                        "frame_id": f"history-{index}",
                    }) + "\n"
                    for index in range(2000)
                ), encoding="utf-8",
            )
            fresh_offset = source.stat().st_size
            with source.open("a", encoding="utf-8") as handle:
                handle.write("openclaw_harness_semantic_step: " + json.dumps(fresh) + "\n")

            with patch("startup_harness.semantic_step_source_trace", return_value=source):
                _, owned = refresh_semantic_step_trace(
                    profile="ignored", run_dir=run_dir, run_id=self.run_id,
                    start_offset=fresh_offset,
                )

            events, status = read_semantic_step_trace(owned, run_dir, self.run_id)
            self.assertEqual(status, "ok")
            self.assertEqual([event["frame_id"] for event in events], ["fresh"])

    def test_live_client_advances_its_semantic_cursor_after_each_observation(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            frames = [
                self.frame("frame-first", "world", {"world.wait": "."}, 100),
                self.frame("frame-second", "world", {"world.wait": "."}, 101),
            ]
            for index, frame in enumerate(frames):
                frame["game_minutes"] = 100 + index
                frame["provenance"] = "native_semantic_step_trace"
                frame["observation"] = {
                    "schema": "caol-avatar-visible-v1",
                    "avatar": {"name": "Ada"},
                    "visible_local": [],
                }
            frames[0]["_event_offset"] = 101
            frames[1]["_event_offset"] = 202
            read_offsets: list[int] = []

            def current(**kwargs):
                read_offsets.append(kwargs["start_offset"])
                return frames[len(read_offsets) - 1]

            source = run_dir / "debug.log"
            source.write_text("x" * 300, encoding="utf-8")
            with patch("startup_harness.semantic_step_source_trace", return_value=source), \
                    patch("startup_harness.current_semantic_step_frame", side_effect=current):
                service = startup_harness.open_cockpit_game_service(
                    profile="ignored", run_dir=run_dir, run_id=self.run_id,
                    trace_start_offset=0,
                )
                self.assertTrue(service.call({"action": "game.observe"})["ok"])
                self.assertTrue(service.call({"action": "game.observe"})["ok"])

            self.assertEqual(read_offsets, [0, 300])


if __name__ == "__main__":
    unittest.main()
