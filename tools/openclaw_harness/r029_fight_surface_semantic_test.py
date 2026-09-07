#!/usr/bin/env python3
"""Regression guard for the native R-029 Pay/Fight owner handoff."""

from pathlib import Path
import unittest


SOURCE = ( Path( __file__ ).resolve().parents[2] / "src" / "do_turn.cpp" )


class R029FightSurfaceSemanticTest( unittest.TestCase ):
    def test_activity_opened_demand_installs_a_bound_semantic_owner( self ) -> None:
        source = SOURCE.read_text( encoding="utf-8" )
        start = source.index( "live_bandit_shakedown_response query_live_bandit_shakedown_dialogue" )
        end = source.index( "bool open_live_bandit_shakedown_surface", start )
        dialogue = source[start:end]
        self.assertIn( "active_semantic_surface_manager() == nullptr", dialogue )
        self.assertIn( '"shakedown_demand"', dialogue )
        self.assertIn( '"shakedown.pay"', dialogue )
        self.assertIn( '"shakedown.fight"', dialogue )
        self.assertIn( "if( semantic_response )", dialogue )


if __name__ == "__main__":
    unittest.main()
