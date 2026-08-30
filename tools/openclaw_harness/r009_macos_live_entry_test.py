#!/usr/bin/env python3
"""Focused declaration guard for the R-009 macOS cockpit witness."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


SCENARIO_PATH = Path( __file__ ).resolve().parent / "scenarios" / (
    "bandit.r009_m095_current_route_safe_watch_mcw.json"
)


class R009MacosLiveEntryTest( unittest.TestCase ):
    def test_current_route_declares_the_chartered_live_boundary( self ) -> None:
        scenario = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )

        self.assertEqual(
            scenario["capabilities"]["runtime.entry_mode"],
            "cockpit_live_session",
        )
        post_relaunch = scenario["post_relaunch"]["steps"]
        live_step = next(
            step for step in post_relaunch
            if step.get( "label" ) == "r009_macos_semantic_wait_witness"
        )
        self.assertEqual( live_step["kind"], "cockpit_live_session" )
        self.assertIs( live_step["cleanup_on_finish"], True )
        self.assertIs( live_step["stop_after_live_session"], True )
        self.assertIn( "run.witness", live_step["authority"] )
        self.assertIn( "run.finish", live_step["authority"] )


if __name__ == "__main__":
    unittest.main()
