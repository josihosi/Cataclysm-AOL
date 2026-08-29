#!/usr/bin/env python3
"""Contract controls for the isolated R-019 lifecycle diagnostic route."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry import validate_manifest  # noqa: E402


class R019TaggedLifecycleDiagnosticTest(unittest.TestCase):
    def setUp(self) -> None:
        self.path = HARNESS_DIR / "scenarios" / "r019.keep_watch_tagged_lifecycle_diagnostic_mcw.json"
        self.manifest = json.loads(self.path.read_text(encoding="utf-8"))

    def test_diagnostic_route_is_current_zero_credit_and_primitive_only(self) -> None:
        validated = validate_manifest(self.manifest, path=self.path)
        self.assertEqual(validated["validation"]["status"], "valid")
        self.assertEqual(
            self.manifest["r019_lifecycle_actor_id"],
            "r019-zombie-dog-positive-progress-v1",
        )
        self.assertFalse(self.manifest["runtime_contract"]["grants_gameplay_proof"])
        self.assertEqual(
            self.manifest["steps"][-1]["action_chain"],
            ["world.wait", "wait.duration_menu", "wait.1m"],
        )
        forbidden = self.manifest["runtime_contract"]["forbidden_input"]
        self.assertIn("cockpit:game.keep_watch", forbidden)
        self.assertIn("semantic:activity.ignore", forbidden)

    def test_launcher_receives_tag_only_from_the_selected_scenario(self) -> None:
        source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        self.assertIn('child_environment.pop("OPENCLAW_HARNESS_R019_LIFECYCLE_ACTOR_ID", None)', source)
        self.assertIn('"--r019-lifecycle-actor-id", lifecycle_actor_id', source)
        self.assertIn('"OPENCLAW_HARNESS_R019_LIFECYCLE_ACTOR_ID"] = lifecycle_actor_id', source)

    def test_registry_allows_only_the_exact_diagnostic_first_run_route(self) -> None:
        source = (HARNESS_DIR / "scenario_registry_store.py").read_text(encoding="utf-8")
        self.assertIn('elif name == "r019.keep_watch_tagged_lifecycle_diagnostic_mcw":', source)
        self.assertIn('facts.get("capabilities.r019.tagged_lifecycle_diagnostic", {})', source)

    def test_activity_interruption_publishes_a_same_turn_native_observation(self) -> None:
        game_source = (HARNESS_DIR.parent.parent / "src" / "game.cpp").read_text(encoding="utf-8")
        action_source = (HARNESS_DIR.parent.parent / "src" / "handle_action.cpp").read_text(
            encoding="utf-8")
        cockpit_source = (HARNESS_DIR / "cockpit.py").read_text(encoding="utf-8")
        harness_source = (HARNESS_DIR / "startup_harness.py").read_text(encoding="utf-8")
        trace = game_source.index('openclaw_harness_trace_activity_query( "open", type, text );')
        observation = game_source.index("openclaw_harness_semantic_activity_distraction();", trace)
        popup = game_source.index("const std::string &action = query_popup()", observation)
        self.assertLess(trace, observation)
        self.assertLess(observation, popup)
        self.assertIn('openclaw_harness_semantic_step_frame( "activity_distraction"', action_source)
        self.assertIn('"activity.ignore", "I"', action_source)
        self.assertIn('"observation_provenance"] = "native_semantic_step_trace"', harness_source)
        self.assertIn("is_observed_activity_interruption", cockpit_source)


if __name__ == "__main__":
    unittest.main()
