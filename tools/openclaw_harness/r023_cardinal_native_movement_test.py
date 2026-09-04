#!/usr/bin/env python3
"""Focused contract checks for the cardinal native semantic movement slice."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from semantic_state import read_semantic_step_trace


class CardinalNativeMovementTest(unittest.TestCase):
    def test_trace_keeps_each_bound_cardinal_action_and_post_step_frame(self) -> None:
        actions = {
            "world.move.north": "k",
            "world.move.south": "j",
            "world.move.west": "h",
            "world.move.east": "l",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            trace = root / "debug.log"
            before = {
                "event": "frame", "run_id": "r023", "frame_id": "r023:before",
                "state": "world", "valid_actions": list(actions), "action_inputs": actions,
                "observation": {"avatar": {"absolute_ms": [10, 20, 0]}},
            }
            receipt = {
                "event": "receipt", "run_id": "r023", "frame_id": "r023:before",
                "action_id": "world.move.east", "accepted": True, "outcome": "moved",
                "before_absolute_ms": [10, 20, 0], "expected_absolute_ms": [11, 20, 0],
                "after_absolute_ms": [11, 20, 0], "after_terrain": "t_floor",
            }
            after = {
                "event": "frame", "run_id": "r023", "frame_id": "r023:after",
                "state": "world", "valid_actions": list(actions), "action_inputs": actions,
                "observation": {"avatar": {"absolute_ms": [11, 20, 0]}},
            }
            trace.write_text("\n".join(
                "openclaw_harness_semantic_step: " + json.dumps(event)
                for event in (before, receipt, after)
            ) + "\n", encoding="utf-8")

            events, status = read_semantic_step_trace(trace, root, "r023")

        self.assertEqual(status, "ok")
        self.assertEqual(events[0]["action_inputs"], actions)
        self.assertEqual(events[1]["outcome"], "moved")
        self.assertEqual(events[1]["expected_absolute_ms"], events[1]["after_absolute_ms"])
        self.assertEqual(events[2]["observation"]["avatar"]["absolute_ms"], [11, 20, 0])

    def test_source_keeps_native_bindings_and_exact_failure_outcomes(self) -> None:
        source = (Path(__file__).resolve().parents[2] / "src" / "handle_action.cpp").read_text(
            encoding="utf-8"
        )
        for action in (
            "world.move.north", "world.move.south", "world.move.west", "world.move.east",
        ):
            self.assertIn(action, source)
        for action_id in ("UP", "DOWN", "LEFT", "RIGHT"):
            self.assertIn(action_id, source)
        for outcome in ("blocked", "no_progress", "unexpected_displacement"):
            self.assertIn(f'"{outcome}"', source)
        self.assertIn('"post_step"', source)
        self.assertIn('\\"after_terrain\\"', source)
        self.assertIn('const tripoint_abs_ms pos_before_abs = player_character.pos_abs();', source)
        self.assertIn('const tripoint_abs_ms expected_abs = pos_before_abs +', source)
        self.assertIn('const tripoint_abs_ms pos_after_abs = player_character.pos_abs();', source)
        self.assertIn('const tripoint_bub_ms &after_bub', source)
        self.assertIn('\\"coordinate_space\\":\\"absolute_ms\\"', source)
        movement_receipt = source[source.index('static void openclaw_harness_semantic_movement_receipt'):source.index('static std::string openclaw_harness_semantic_movement_action_id')]
        self.assertIn('openclaw_harness_write_semantic_step_event( event.str() )', movement_receipt)


if __name__ == "__main__":
    unittest.main()
