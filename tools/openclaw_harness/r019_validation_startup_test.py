#!/usr/bin/env python3
"""Focused R-019 validation-startup ownership controls."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness  # noqa: E402
from startup_harness import (  # noqa: E402
    declared_startup_overlay_state_is_qualified,
    first_initial_hud_world_frame_after_boundary,
    initial_hud_world_semantic_frame_is_qualified,
    load_scenario,
    recover_declared_startup_action_menu_overlay,
    resolve_fixture_payload,
    startup_profile_continue_keys,
)


class R019ValidationStartupTest(unittest.TestCase):
    def test_native_hud_gate_uses_post_redraw_boundary_not_input_stack_cardinality(self) -> None:
        game_source = (HARNESS_DIR.parent.parent / "src" / "game.cpp").read_text(
            encoding="utf-8"
        )
        do_turn_source = (HARNESS_DIR.parent.parent / "src" / "do_turn.cpp").read_text(
            encoding="utf-8"
        )
        render = game_source.index("void game::draw( ui_adaptor &ui )")
        panels = game_source.index("draw_panels( true );", render)
        gate_start = game_source.index("openclaw_harness_semantic_initial_world_frame_if_ready(", panels)
        gate_end = game_source.index(";", gate_start)
        gate = game_source[gate_start:gate_end]
        self.assertIn("input_context::get_active_context()", gate)
        self.assertIn("!u.activity", gate)
        self.assertIn("!u.has_destination()", gate)
        self.assertIn("uquit == QUIT_WATCH && u.is_dead_state()", gate)
        self.assertNotIn("ui_stack_size", gate)
        self.assertNotIn("openclaw_harness_semantic_initial_world_frame_if_ready(", do_turn_source)

    def test_render_gate_is_fail_closed_for_every_non_world_owner(self) -> None:
        source = (HARNESS_DIR.parent.parent / "src" / "handle_action.cpp").read_text(
            encoding="utf-8"
        )
        start = source.index("void openclaw_harness_semantic_initial_world_frame_if_ready")
        end = source.index("void openclaw_harness_semantic_wait_activity_complete", start)
        gate = source[start:end]

        # Main-world, modal-context, and missing-context controls all flow
        # through the active native input-context category, never UI topology.
        self.assertIn("active_input_context == nullptr", gate)
        self.assertIn('active_input_context->get_category() != "DEFAULTMODE"', gate)
        self.assertIn("active_input_context->first_keyboard_character_for_action( \"wait\" )", gate)
        self.assertIn("!no_activity_owns_turn", gate)
        self.assertIn("!no_auto_move_owns_turn", gate)
        self.assertIn("!no_dead_watch_owns_turn", gate)
        self.assertNotIn("is_on_top", gate)
        self.assertNotIn("ui_stack_size", gate)

    def test_native_owner_accessor_reads_the_context_stack(self) -> None:
        source = (HARNESS_DIR.parent.parent / "src" / "input_context.cpp").read_text(
            encoding="utf-8"
        )
        start = source.index("input_context *input_context::get_active_context()")
        end = source.index("\n}\n\n#if defined(__ANDROID__)", start)
        accessor = source[start:end]
        self.assertIn("return input_context_stack.back();", accessor)

    def test_declared_validation_routes_profile_escape_to_its_native_owner(self) -> None:
        self.assertEqual(
            startup_profile_continue_keys(
                {"post_lastworld_continue_keys": ["escape"]},
                declared_overlay_dismissal=True,
            ),
            ["escape"],
        )

    def test_initial_hud_producer_accepts_only_one_fresh_current_run_world_frame(self) -> None:
        metadata = {
            "frame_run_id": "run-1", "frame_state": "world",
            "advertised_actions": ["world.wait"], "frame_trace_offset": 101,
            "frame_producer": "hud_world_ready", "initial_world_ready": True,
            "matching_initial_hud_world_frame_count": 1,
            "foreign_frame_count_after_dismissal": 0,
        }
        self.assertTrue(initial_hud_world_semantic_frame_is_qualified(metadata, run_id="run-1"))
        for invalid in (
            {**metadata, "matching_initial_hud_world_frame_count": 2},
            {**metadata, "foreign_frame_count_after_dismissal": 1},
            {**metadata, "frame_trace_offset": -1},
            {**metadata, "frame_run_id": "stale-run"},
            {**metadata, "frame_producer": "player_input"},
            {**metadata, "initial_world_ready": False},
        ):
            self.assertFalse(initial_hud_world_semantic_frame_is_qualified(invalid, run_id="run-1"))
        self.assertEqual(
            startup_profile_continue_keys(
                {"post_lastworld_continue_keys": ["escape"]},
                declared_overlay_dismissal=False,
            ),
            ["escape"],
        )

    def test_validation_accepts_only_its_native_overlay_result(self) -> None:
        accepted = {
            "requested": True,
            "status": "dismissed_declared_blocking_overlay",
            "native_main_menu_active_before": True,
            "native_main_menu_active_after": False,
            "transport_receipt": {"ok": True},
            "profile_owned_input": ["escape"],
            "input_dispatch_count": 1,
        }
        self.assertTrue(declared_startup_overlay_state_is_qualified(accepted))
        self.assertFalse(declared_startup_overlay_state_is_qualified({
            **accepted, "status": "declared_overlay_not_present",
        }))
        self.assertFalse(declared_startup_overlay_state_is_qualified({
            **accepted, "input_dispatch_count": 2,
        }))
        self.assertTrue(declared_startup_overlay_state_is_qualified({
            "requested": True,
            "status": "declared_overlay_not_present",
            "native_action_menu_active_before": False,
            "native_main_menu_active_before": False,
        }))

    def test_declared_overlay_recovery_does_not_open_a_menu_at_a_clean_hud(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            receipt = Path(directory) / "overlay-recovery.log"
            trace.write_text("current-run world HUD\n", encoding="utf-8")
            with mock.patch("startup_harness.peekaboo_focus_pid") as focus, \
                    mock.patch("startup_harness.peekaboo_press_sequence") as press:
                recovery = recover_declared_startup_action_menu_overlay(
                    42, trace, receipt, profile_owned_input=["escape"],
                )

        self.assertEqual(recovery["status"], "declared_overlay_not_present")
        self.assertEqual(recovery["input_dispatch_count"], 0)
        self.assertFalse(recovery["native_main_menu_active_after"])
        self.assertTrue(declared_startup_overlay_state_is_qualified({
            "requested": True, **recovery,
        }))
        focus.assert_not_called()
        press.assert_not_called()

    def test_closure_validation_requires_the_initial_hud_world_frame(self) -> None:
        scenario = load_scenario("r019.keep_watch_off_interruption_closure059_validation_mcw")
        self.assertTrue(scenario["suppress_profile_startup_input"])
        self.assertNotEqual(scenario["steps"][0]["kind"], "press")
        checkpoint = scenario["steps"][1]["native_semantic_checkpoint"]
        self.assertTrue(checkpoint["require_initial_hud_world_ready_frame"])
        self.assertEqual(checkpoint["required_state"], "world")
        self.assertEqual(checkpoint["required_actions"], ["world.wait"])

    def test_live_validation_fixture_clears_inherited_auto_move_before_hostile_setup(self) -> None:
        resolved = resolve_fixture_payload("r019_keep_watch_off_positive_progress_v1", "live-debug")
        transforms = resolved["save_transforms"]
        clear_index = next(
            index for index, transform in enumerate(transforms)
            if transform["kind"] == "clear_avatar_auto_move"
        )
        hostile_index = next(
            index for index, transform in enumerate(transforms)
            if transform["kind"] == "active_monsters_near_player"
            and transform.get("monsters")
        )
        self.assertLess(clear_index, hostile_index)
        self.assertEqual(transforms[clear_index]["player_save"], "#Wm9yYWlkYSBWaWNr.sav.zzip")

    def test_initial_frame_count_uses_this_launch_trace_boundary(self) -> None:
        source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        call_start = source.index("matching, foreign = initial_hud_world_frame_counts(")
        call_end = source.index(")\n                report[\"metadata\"].update", call_start)
        call = source[call_start:call_end]
        self.assertIn("trace_offset=semantic_trace_start", call)
        self.assertNotIn("trace_offset=0", call)

    def test_initial_frame_selector_retains_first_eligible_frame_not_later_ordinary_frame(self) -> None:
        old = {
            "event": "frame", "run_id": "old-run", "frame_id": "old:1", "state": "world",
            "valid_actions": ["world.wait"], "producer": "hud_world_ready",
            "initial_world_ready": True,
        }
        eligible = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:initial", "state": "world",
            "valid_actions": ["world.wait"], "producer": "hud_world_ready",
            "initial_world_ready": True,
        }
        later = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:ordinary", "state": "world",
            "valid_actions": ["world.wait"], "producer": "", "initial_world_ready": False,
        }
        with self.subTest("pre-boundary frame is excluded"), mock.patch(
                "startup_harness.semantic_step_source_trace") as source_trace:
            with tempfile.TemporaryDirectory() as directory:
                trace = Path(directory) / "semantic.log"
                prefix = b"before\n" + b"openclaw_harness_semantic_step: " + json.dumps(old).encode() + b"\n"
                trace.write_bytes(prefix + b"openclaw_harness_semantic_step: " + json.dumps(eligible).encode() + b"\n" +
                                  b"openclaw_harness_semantic_step: " + json.dumps(later).encode() + b"\n")
                source_trace.return_value = trace
                selected = first_initial_hud_world_frame_after_boundary(
                    profile="test", trace_offset=len(prefix), run_id="run-1", required_state="world",
                    required_actions=["world.wait"],
                )
        self.assertEqual(selected["frame_id"], "run-1:initial")
        self.assertEqual(selected["_event_offset"], len(prefix))

    def test_initial_frame_selector_rebases_after_debug_log_truncation(self) -> None:
        eligible = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:initial", "state": "world",
            "valid_actions": ["world.wait"], "producer": "hud_world_ready",
            "initial_world_ready": True,
        }
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "semantic.log"
            trace.write_bytes(b"openclaw_harness_semantic_step: " + json.dumps(eligible).encode() + b"\n")
            with mock.patch("startup_harness.semantic_step_source_trace", return_value=trace):
                selected = first_initial_hud_world_frame_after_boundary(
                    profile="test", trace_offset=10_000, run_id="run-1", required_state="world",
                    required_actions=["world.wait"],
                )
        self.assertEqual(selected["frame_id"], "run-1:initial")
        self.assertLess(selected["_event_offset"], 10_000)

    def test_native_bootstrap_rebases_to_the_run_owned_trace_at_byte_zero(self) -> None:
        eligible = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:initial", "state": "world",
            "valid_actions": ["world.wait"], "producer": "hud_world_ready",
            "initial_world_ready": True,
        }
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            (run_dir / "semantic.native.events.jsonl").write_bytes(
                b"openclaw_harness_semantic_step: " + json.dumps(eligible).encode() + b"\n"
            )
            metadata = startup_harness.await_r014_native_semantic_bootstrap(
                profile="test", run_dir=run_dir, run_id="run-1", required_state="world",
                required_actions=["world.wait"], timeout_seconds=0.0, poll_seconds=0.0,
                trace_start_offset=10_000, require_initial_hud_world_ready_frame=True,
            )
        self.assertEqual(metadata["status"], "required_state_present")
        self.assertEqual(metadata["frame_trace_offset"], 0)

    def test_live_reader_and_dispatch_keep_their_cursor_in_the_run_owned_trace(self) -> None:
        source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        service_start = source.index("def open_cockpit_game_service(")
        service_end = source.index("    player_fire_setup = None", service_start)
        service = source[service_start:service_end]
        self.assertEqual(service.count("semantic_step_source_trace(profile, run_dir)"), 2)

    def test_world_successor_requires_its_immediately_preceding_same_run_descriptor(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "run-1",
            "surface_id": "run-1:surface:2", "frame_id": "run-1:frame:2", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        raw = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:turn:2",
            "state": "world", "game_minutes": 8222, "observed_turn": 10,
            "observation": {"visible_local": []},
            "keep_watch_safety": {"classification": "clear"},
            "valid_actions": ["world.wait"],
        }
        prefix = b"openclaw_harness_semantic_step: "
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            trace = run_dir / "semantic.native.events.jsonl"
            first = prefix + json.dumps(descriptor).encode() + b"\n"
            trace.write_bytes(first + prefix + json.dumps(raw).encode() + b"\n")
            paired = startup_harness.current_semantic_step_frame(
                profile="test", run_dir=run_dir, run_id="run-1", start_offset=len(first),
            )
        self.assertEqual(paired["event"], "surface_descriptor")
        self.assertEqual(paired["frame_id"], "run-1:frame:2")
        self.assertEqual(paired["paired_raw_frame_id"], "run-1:turn:2")
        self.assertEqual(paired["game_minutes"], 8222)

    def test_world_descriptor_retains_only_its_adjacent_wait_activity_state(self) -> None:
        activity = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:activity:1",
            "state": "wait_activity", "valid_actions": [],
        }
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "run-1",
            "surface_id": "run-1:surface:2", "frame_id": "run-1:frame:2", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        prefix = b"openclaw_harness_semantic_step: "
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            trace = run_dir / "semantic.native.events.jsonl"
            trace.write_bytes(prefix + json.dumps(activity).encode() + b"\n" +
                              prefix + json.dumps(descriptor).encode() + b"\n")
            paired = startup_harness.current_semantic_step_frame(
                profile="test", run_dir=run_dir, run_id="run-1", start_offset=0,
            )
        self.assertEqual(paired["frame_id"], "run-1:frame:2")
        self.assertEqual(paired["paired_raw_frame_id"], "run-1:activity:1")
        self.assertEqual(paired["paired_raw_state"], "wait_activity")

    def test_world_descriptor_prefers_its_immediate_world_successor_over_prior_activity(self) -> None:
        activity = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:activity:1",
            "state": "wait_activity_complete", "valid_actions": [],
        }
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "run-1",
            "surface_id": "run-1:surface:2", "frame_id": "run-1:frame:2", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        world = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:turn:2",
            "state": "world", "game_minutes": 8222, "valid_actions": ["world.wait"],
            "observation": {"visible_local": []}, "keep_watch_safety": {"classification": "clear"},
        }
        prefix = b"openclaw_harness_semantic_step: "
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            trace = run_dir / "semantic.native.events.jsonl"
            trace.write_bytes(b"".join(prefix + json.dumps(event).encode() + b"\n"
                                      for event in (activity, descriptor, world)))
            paired = startup_harness.current_semantic_step_frame(
                profile="test", run_dir=run_dir, run_id="run-1", start_offset=0,
            )
        self.assertEqual(paired["frame_id"], "run-1:frame:2")
        self.assertEqual(paired["paired_raw_frame_id"], "run-1:turn:2")
        self.assertEqual(paired["paired_raw_state"], "world")
        self.assertEqual(paired["game_minutes"], 8222)
        self.assertIsInstance(paired["dispatch_descriptor_event_offset"], int)

    def test_initial_frame_selector_rejects_counterexamples(self) -> None:
        base = {
            "event": "frame", "run_id": "run-1", "frame_id": "run-1:initial", "state": "world",
            "valid_actions": ["world.wait"], "producer": "hud_world_ready",
            "initial_world_ready": True,
        }
        cases = {
            "absent": [],
            "duplicate": [base, {**base, "frame_id": "run-1:duplicate"}],
            "wrong-run": [{**base, "run_id": "run-2", "frame_id": "run-2:initial"}],
            "mismatched-producer": [{**base, "producer": "player_input"}],
        }
        for label, events in cases.items():
            with self.subTest(label), tempfile.TemporaryDirectory() as directory:
                trace = Path(directory) / "semantic.log"
                trace.write_bytes(b"".join(
                    b"openclaw_harness_semantic_step: " + json.dumps(event).encode() + b"\n" for event in events
                ))
                with mock.patch("startup_harness.semantic_step_source_trace", return_value=trace):
                    with self.assertRaises(ValueError):
                        first_initial_hud_world_frame_after_boundary(
                            profile="test", trace_offset=0, run_id="run-1", required_state="world",
                            required_actions=["world.wait"],
                        )


if __name__ == "__main__":
    unittest.main()
