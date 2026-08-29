#!/usr/bin/env python3
"""Ordering controls for R-019's zero-credit startup boundary observation."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DO_TURN = (ROOT / "src" / "do_turn.cpp").read_text(encoding="utf-8")
HANDLE_ACTION = (ROOT / "src" / "handle_action.cpp").read_text(encoding="utf-8")
GAME = (ROOT / "src" / "game.cpp").read_text(encoding="utf-8")


class R019StartupBoundaryObservationTest(unittest.TestCase):
    def test_post_hud_observation_precedes_input_dispatch(self) -> None:
        redraw = DO_TURN.index("ui_manager::redraw();", DO_TURN.index("bool game::do_turn()"))
        boundary = DO_TURN.index("openclaw_harness_trace_post_hud_pre_input_boundary( u,", redraw)
        dispatch = DO_TURN.index("if( handle_action() )", boundary)
        self.assertLess(redraw, boundary)
        self.assertLess(boundary, dispatch)
        self.assertNotIn("openclaw_harness_semantic_initial_world_frame_if_ready(", DO_TURN)

    def test_observation_records_the_required_state_without_dispatch(self) -> None:
        start = DO_TURN.index("static void openclaw_harness_trace_post_hud_pre_input_boundary")
        end = DO_TURN.index("#if defined(__ANDROID__)", start)
        trace = DO_TURN[start:end]
        for field in ("run_id", "avatar_live", "modal_owner", "world_wait_available"):
            self.assertIn(field, trace)
        self.assertNotIn("handle_input", trace)

    def test_native_input_and_descriptor_are_distinct_later_events(self) -> None:
        native_input = HANDLE_ACTION.index("event=native_input_entered")
        descriptor = HANDLE_ACTION.index("event=descriptor_publication", native_input)
        self.assertLess(native_input, descriptor)
        self.assertNotIn("openclaw_harness_semantic_initial_world_frame_if_ready(\n            !u.is_dead_state(), true", HANDLE_ACTION)

    def test_initial_producer_rejects_each_alternate_turn_owner(self) -> None:
        producer = GAME[GAME.index("void game::draw( ui_adaptor &ui )"):]
        for predicate in (
            "!u.activity",
            "!u.has_destination() && !u.has_destination_activity()",
            "uquit == QUIT_WATCH && u.is_dead_state()",
        ):
            self.assertIn(predicate, producer)

        start = HANDLE_ACTION.index("void openclaw_harness_semantic_initial_world_frame_if_ready")
        end = HANDLE_ACTION.index("void openclaw_harness_semantic_wait_activity_complete", start)
        gate = HANDLE_ACTION[start:end]
        for predicate in (
            'active_input_context->get_category() != "DEFAULTMODE"',
            "active_input_context == nullptr",
            "!no_activity_owns_turn",
            "!no_auto_move_owns_turn",
            "!no_dead_watch_owns_turn",
        ):
            self.assertIn(predicate, gate)

    def test_hud_readiness_trace_has_the_same_run_state_facts(self) -> None:
        start = GAME.index('component=gameplay_hud event=rendered')
        end = GAME.index("previous_turn = current_turn", start)
        trace = GAME[start:end]
        for field in ("run_id", "avatar_live", "modal_owner", "world_wait_available"):
            self.assertIn(field, trace)


if __name__ == "__main__":
    unittest.main()
