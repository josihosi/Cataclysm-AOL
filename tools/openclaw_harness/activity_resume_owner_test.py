#!/usr/bin/env python3
"""Contract for the native owner following an ignored activity interruption."""

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
DO_TURN = ( ROOT / "src" / "do_turn.cpp" ).read_text( encoding="utf-8" )


class ActivityResumeOwnerTest( unittest.TestCase ):
    def test_ignored_activity_handoff_exposes_only_native_pause(self ) -> None:
        """An actionless resume must not strand the DEFAULTMODE activity owner."""
        start = DO_TURN.index( "void handle_key_blocking_activity()" )
        end = DO_TURN.index( "namespace\n{\nstruct live_bandit_pair_boundary_step", start )
        body = DO_TURN[start:end]

        self.assertIn( "openclaw_harness_semantic_session_active()", body )
        self.assertIn( '"activity_wait", "Activity in progress"', body )
        self.assertIn( '{ "activity.pause", "", _( "Pause activity" ), true }', body )
        self.assertIn( 'request.action_id != "activity.pause"', body )
        self.assertIn( 'semantic_action = "pause";', body )
        self.assertIn(
            "semantic_manager->withhold_parent_authority_until_recreated( request.surface_id );",
            body,
        )
        self.assertIn( "semantic_scope->consume_request();", body )

        # The action remains native: the input loop receives its canonical
        # DEFAULTMODE action, rather than a harness-only resume/no-op key.
        self.assertIn( "if( action == \"pause\" )", body )
        self.assertNotIn( 'semantic_action = "resume"', body )


if __name__ == "__main__":
    unittest.main()
