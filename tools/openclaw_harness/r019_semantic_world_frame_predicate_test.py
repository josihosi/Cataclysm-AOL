#!/usr/bin/env python3
"""Keep R-019's initial semantic world frame available to revived fixtures."""

from __future__ import annotations

import unittest
from pathlib import Path


SOURCE = Path( __file__ ).resolve().parents[2] / "src" / "handle_action.cpp"


class R019SemanticWorldFramePredicateTest( unittest.TestCase ):
    def setUp( self ) -> None:
        self.source = SOURCE.read_text( encoding="utf-8" )

    def test_alive_avatar_with_stale_watch_status_uses_default_input_context( self ) -> None:
        self.assertIn(
            "const bool watching_dead_avatar = uquit == QUIT_WATCH && u.is_dead_state();",
            self.source,
        )
        self.assertIn( "if( watching_dead_avatar ) {", self.source )

    def test_semantic_world_frame_is_absent_only_for_an_actual_deathcam( self ) -> None:
        self.assertIn( "if( !watching_dead_avatar ) {", self.source )
        self.assertIn(
            "openclaw_harness_semantic_step_frame(\n                    \"world\", semantic_actions );",
            self.source,
        )


if __name__ == "__main__":
    unittest.main()
