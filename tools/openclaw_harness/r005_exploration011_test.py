#!/usr/bin/env python3
"""Contract checks for the R-005 x144 route observation."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


SCENARIO_PATH = Path( __file__ ).resolve().parent / "scenarios" / \
                "bandit.r005_natural_route_observation_x144.json"


class R005Exploration011Test( unittest.TestCase ):
    def test_x144_candidate_is_preview_only_and_excludes_known_boundaries( self ) -> None:
        observation = json.loads( SCENARIO_PATH.read_text( encoding="utf-8" ) )
        candidate = observation["route_candidates"][0]
        planned_omts = candidate["planned_omts"]

        self.assertEqual( candidate["id"], "x144_northern_return" )
        self.assertEqual(
            planned_omts,
            [[140, 41, 0], [144, 42, 0], [144, 29, 0], [140, 29, 0], [140, 31, 0]],
        )
        self.assertEqual( planned_omts[1][0], 144 )
        self.assertEqual( planned_omts[2][0], 144 )
        self.assertNotIn( [141, 32, 0], planned_omts )
        self.assertFalse( observation["runtime_contract"]["grants_gameplay_proof"] )
        self.assertEqual(
            observation["runtime_contract"]["permitted_input"],
            ["ordinary-overmap-route-preview"],
        )
        self.assertIn(
            "ordinary-overmap-route-confirm",
            observation["runtime_contract"]["forbidden_input"],
        )
        route_steps = [
            step for step in observation["steps"]
            if step["kind"] == "ordinary_overmap_route_constructor"
        ]
        self.assertTrue( route_steps )
        self.assertTrue( all( step["require_native_corridor"] for step in route_steps ) )


if __name__ == "__main__":
    unittest.main()
