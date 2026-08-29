#!/usr/bin/env python3
"""Focused contract tests for the adaptive semantic playtest channel."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))

from semantic_broker import SemanticStepChannel, SemanticStepFrame
from semantic_state import latest_semantic_step_frame, read_semantic_step_trace
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
                if Path(path).resolve() == source.resolve():
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
            sent: list[str] = []

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
                send_input=sent.append, await_transition=transition,
            )
            duration = channel.act(
                frame_id="frame-2", action_id="wait.6h",
                send_input=sent.append, await_transition=transition,
            )
            self.assertTrue(parent["accepted"])
            self.assertTrue(duration["accepted"])
            self.assertEqual(duration["next_frame"]["state"], "wait_activity")
            self.assertEqual(sent, ["w", "8"])
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
            sent: list[str] = []
            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-atomic",
                receipt_path=root / "semantic.steps.jsonl", read_frame=lambda: world,
            )

            receipt = channel.act_observed(
                observed_frame=interruption,
                frame_id="activity-1",
                action_id="activity.ignore",
                send_input=sent.append,
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
            self.assertEqual(receipt["_next_frame"]["action_inputs"], {"world.wait": "|"})
            self.assertEqual(receipt["semantic_response"]["resolved_action"], "IGNORE")
            self.assertEqual(sent, ["I"])
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
                send_input=lambda _: None,
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
                    patch.object(startup_harness, "dispatch_semantic_input",
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
            self.assertTrue(receipt["accepted"])
            self.assertEqual(sent, [((17, "I"), {"delay_ms": 200, "focus_once": True})])
            self.assertEqual(read_receipt.call_args.args[1], 7)
            self.assertEqual(read_receipt.call_args.kwargs["issuing_open_offset"], 7)

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
                    patch.object(startup_harness, "dispatch_semantic_input"), \
                    patch.object(startup_harness, "read_latest_activity_query_trace", return_value=None):
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker-atomic",
                    frame_id="activity-1", action_id="activity.ignore",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=interruption,
                )
            self.assertFalse(receipt["accepted"])
            self.assertEqual(receipt["reason"], "native_receipt_missing")

    def test_live_wait_dispatch_retains_bound_diagnostic_records(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            initialize_wait_diagnostic_ledger(
                root, enabled=True, run_id=self.run_id, executable=Path(sys.executable),
            )
            wait = self.frame("wait-1", "wait_duration_choice", {"wait.1h": "h"}, 101)
            with patch.object(SemanticStepChannel, "act_observed", return_value={"accepted": True}), \
                    patch.object(startup_harness, "classify_wait_input_trace", return_value={
                        "status": "wait_dispatched",
                    }):
                receipt = execute_semantic_act(
                    run_dir=root, profile="ignored", run_id=self.run_id,
                    trace_start_offset=0, pid=17, session_id="worker-atomic",
                    frame_id="wait-1", action_id="wait.1h",
                    transition_timeout_seconds=0.01, observe_interval_seconds=0,
                    observed_frame=wait,
                )
            self.assertTrue(receipt["accepted"])
            self.assertEqual(receipt["wait_diagnostic_before_input"]["status"], "retained")
            self.assertEqual(receipt["wait_diagnostic_result"]["status"], "retained")
            records = [json.loads(line) for line in
                       (root / "wait-diagnostic.records.jsonl").read_text(encoding="utf-8").splitlines()]
            self.assertEqual([record["phase"] for record in records[1:]], ["before_input", "result"])

    def test_original_wrong_menu_choice_and_stale_frame_are_rejected_without_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            current = self.frame("frame-parent", "wait_mode_choice", {
                "wait.duration_menu": "w", "alarm.duration_menu": "a",
            }, 100)
            sent: list[str] = []
            channel = SemanticStepChannel(
                run_id=self.run_id, session_id="worker-1",
                receipt_path=root / "semantic.steps.jsonl", read_frame=lambda: current,
            )
            wrong_menu = channel.act(
                frame_id="frame-parent", action_id="wait.6h",
                send_input=sent.append, await_transition=lambda *_: {},
            )
            stale = channel.act(
                frame_id="frame-old", action_id="wait.duration_menu",
                send_input=sent.append, await_transition=lambda *_: {},
            )
            self.assertEqual(wrong_menu["reason"], "action_not_advertised")
            self.assertEqual(stale["reason"], "stale_frame")
            self.assertEqual(sent, [])

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

    def test_refresh_rejects_unbounded_current_run_events(self) -> None:
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
                with self.assertRaisesRegex(ValueError, "bounded run channel"):
                    refresh_semantic_step_trace(
                        profile="ignored", run_dir=run_dir, run_id=self.run_id, start_offset=0,
                    )

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
