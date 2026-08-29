#!/usr/bin/env python3
"""Focused contract test for the native semantic calendar marker."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from semantic_state import latest_semantic_game_minutes, read_semantic_step_trace


class SemanticTimeMarkerTest(unittest.TestCase):
    def test_native_frame_calendar_marker_is_preserved_without_derivation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            trace = root / "debug.final.log"
            frame = {
                "event": "frame", "run_id": "run-062", "frame_id": "run-062:10:1",
                "state": "world", "observed_turn": 10, "game_minutes": 8940,
                "valid_actions": ["world.wait"], "action_inputs": {"world.wait": "|"},
            }
            trace.write_text(
                "openclaw_harness_semantic_step: " + json.dumps(frame) + "\n",
                encoding="utf-8",
            )
            events, status = read_semantic_step_trace(trace, root, "run-062")
            self.assertEqual(status, "ok")
            self.assertEqual(latest_semantic_game_minutes(events), 8940)


if __name__ == "__main__":
    unittest.main()
