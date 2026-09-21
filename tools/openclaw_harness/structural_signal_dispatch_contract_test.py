#!/usr/bin/env python3
"""Guard the read-only premise surface against parallel scheduler logic."""

from pathlib import Path
import unittest


SOURCE = Path(__file__).resolve().parents[2] / "src" / "handle_action.cpp"


class StructuralSignalDispatchContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        start = cls.source.index(
            "static std::string openclaw_harness_structural_signal_dispatch_snapshot()"
        )
        end = cls.source.index(
            "// This is a read-only view of the storage destination", start
        )
        cls.function = cls.source[start:end]

    def test_surface_uses_production_evaluator_and_policy_owners(self):
        for symbol in (
            "routine_scout_policy",
            "select_routine_scout_pair",
            "plan_structural_bounty_outing_candidates",
            "evaluate_hostile_camp_routine_dispatch",
        ):
            self.assertIn(symbol, self.function)

    def test_surface_keeps_threshold_and_read_only_boundary_explicit(self):
        self.assertIn("dispatch_threshold", self.function)
        self.assertIn("threshold", self.function)
        self.assertIn("diagnostic_read_only_production_scheduler_evaluator_no_materialization_or_dispatch", self.function)
        self.assertNotIn("apply_structural_bounty_outing_plan", self.function)

    def test_surface_exposes_source_countdown_and_time_dependent_gate(self):
        for field in (
            "loaded_source",
            "countdown_turn",
            "countdown_minutes",
            "observer",
            "scheduler_inputs",
            "time_dependent",
            "reason",
        ):
            self.assertIn(field, self.function)

    def test_loaded_source_scan_covers_map_squares_beyond_first_submap(self):
        """Regression guard for sources outside the map's first submap."""
        self.assertIn(
            "const int loaded_width = SEEX * here.getmapsize();", self.function
        )
        self.assertIn(
            "const int loaded_height = SEEY * here.getmapsize();", self.function
        )
        self.assertIn(
            "for( int y = 0; y < loaded_height; ++y )", self.function
        )
        self.assertIn(
            "for( int x = 0; x < loaded_width; ++x )", self.function
        )

        # The previous loops treated submap counts as map-square extents and
        # therefore missed this active source in the second loaded submap.
        seex, seey, map_size = 12, 12, 2
        source_outside_first_submap = (seex + 1, seey + 1)
        loaded_positions = {
            (x, y)
            for y in range(seey * map_size)
            for x in range(seex * map_size)
        }
        self.assertIn(source_outside_first_submap, loaded_positions)
        active_source = {
            "active": True,
            "map_square": source_outside_first_submap,
            "countdown_minutes": 17,
        }
        discovered = [
            item for item in (active_source,)
            if item["active"] and item["map_square"] in loaded_positions
        ]
        self.assertEqual(discovered, [active_source])
        self.assertNotIn(
            source_outside_first_submap,
            {(x, y) for y in range(map_size) for x in range(map_size)},
        )

    def test_time_dependent_gate_uses_production_threshold_owner(self):
        self.assertIn(
            "evaluation.drive <\n                    bandit_live_world::hostile_camp_routine_dispatch_threshold()",
            self.function,
        )
        self.assertNotIn("evaluation.drive < 500", self.function)


if __name__ == "__main__":
    unittest.main()
