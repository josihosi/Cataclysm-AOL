#!/usr/bin/env python3
"""Focused typed-query contracts for the scenario registry's pure evaluator."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    ScenarioRegistryQueryError,
    evaluate_registry_query,
    parse_registry_query_request,
)


class ScenarioRegistryQueryTest(unittest.TestCase):
    def fact(
        self,
        value: object = None,
        *,
        present: bool = True,
        state: str = "run-verified",
        proof_depth: str | None = "interaction",
    ) -> dict:
        result = {
            "present": present,
            "evidence_state": state,
            "proof_depth": proof_depth,
        }
        if present:
            result["value"] = value
        return result

    def candidate(self, scenario_id: str, facts: dict, **extra: object) -> dict:
        return {"scenario_id": scenario_id, "facts": facts, **extra}

    def query(self, requirements: list, preferences: list | None = None) -> object:
        return parse_registry_query_request({
            "requirements": requirements,
            "preferences": preferences or [],
        })

    def test_typed_operators_accept_explicit_run_verified_facts(self) -> None:
        request = self.query([
            {"key": "player.thirst", "op": "eq", "value": False},
            {"key": "actors.visible", "op": "contains", "value": "forest observer"},
            {"key": "capabilities.trade", "op": "present"},
            {"key": "runtime.ocr", "op": "absent"},
            {"key": "world.distance", "op": "range", "minimum": 2, "maximum": 5},
        ])
        result = evaluate_registry_query(request, [self.candidate("fully.typed", {
            "player.thirst": self.fact(False),
            "actors.visible": self.fact(["forest observer", "scout"]),
            "capabilities.trade": self.fact(True),
            "runtime.ocr": self.fact(present=False),
            "world.distance": self.fact(4),
        })])

        self.assertEqual(result.ranked_scenario_ids, ("fully.typed",))
        self.assertTrue(all(item.passed for item in result.candidates[0].hard_results))

    def test_malformed_and_injection_shaped_query_values_fail_closed_as_data(self) -> None:
        request = self.query([
            {"key": "world.name", "op": "eq", "value": "x' OR 1=1 --"},
        ])
        result = evaluate_registry_query(request, [self.candidate("ordinary", {
            "world.name": self.fact("forest"),
        })])
        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(result.candidates[0].hard_results[0].reason, "equality_mismatch")

        invalid_cases = [
            {},
            {"requirements": [], "preferences": [], "description": "Camp Fight"},
            {"requirements": [{"key": "description.camp", "op": "eq", "value": True}], "preferences": []},
            {"requirements": [{"key": "player.thirst", "op": "eq"}], "preferences": []},
            {"requirements": [{"key": "player.thirst", "op": "present", "value": True}], "preferences": []},
            {"requirements": [{"key": "world.distance", "op": "range", "minimum": True}], "preferences": []},
            {"requirements": [{"key": "world.distance", "op": "range", "minimum": 4, "maximum": 3}], "preferences": []},
            {"requirements": [{"key": "player.thirst", "op": "like", "value": False}], "preferences": []},
            {"requirements": [{"key": "player.thirst", "op": "eq", "value": False, "minimum_evidence": "hard_proven"}], "preferences": []},
            {"requirements": [{"key": "player.thirst", "op": "eq", "value": False, "minimum_evidence": "stale"}], "preferences": []},
        ]
        for raw in invalid_cases:
            with self.subTest(raw=raw):
                with self.assertRaises(ScenarioRegistryQueryError):
                    parse_registry_query_request(raw)

    def test_all_hard_failures_are_preserved_before_preferences(self) -> None:
        request = self.query([
            {"key": "player.thirst", "op": "eq", "value": False},
            {"key": "actors.visible", "op": "contains", "value": "forest"},
        ], preferences=[
            {"key": "capabilities.trade", "op": "eq", "value": True},
        ])
        result = evaluate_registry_query(request, [self.candidate("thirsty.forest", {
            "player.thirst": self.fact(True),
            "actors.visible": self.fact(["camp"]),
            "capabilities.trade": self.fact(True),
        })])

        candidate = result.candidates[0]
        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertFalse(candidate.hard_valid)
        self.assertEqual(
            [item.reason for item in candidate.hard_results],
            ["equality_mismatch", "containment_mismatch"],
        )
        self.assertEqual(candidate.preference_results, ())

    def test_preference_cannot_rescue_unknown_or_below_floor_hard_fact(self) -> None:
        request = self.query([
            {"key": "player.thirst", "op": "eq", "value": False},
        ], preferences=[
            {"key": "capabilities.trade", "op": "eq", "value": True},
        ])
        result = evaluate_registry_query(request, [
            self.candidate("unknown.hard", {
                "player.thirst": self.fact(False, state="unknown"),
                "capabilities.trade": self.fact(True),
            }),
            self.candidate("declared.hard", {
                "player.thirst": self.fact(False, state="declared"),
                "capabilities.trade": self.fact(True),
            }),
        ])

        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(result.candidates[0].hard_results[0].reason, "unknown")
        self.assertEqual(result.candidates[1].hard_results[0].reason, "below_minimum_evidence")

    def test_public_evidence_floors_distinguish_declared_inspected_and_run_verified(self) -> None:
        request = self.query([
            {"key": "local_place.camp.real", "op": "eq", "value": True, "minimum_evidence": "inspected"},
            {"key": "capabilities.dialogue.choice.fight.visible", "op": "eq", "value": True, "minimum_evidence": "run-verified"},
        ])
        result = evaluate_registry_query(request, [
            self.candidate("declared.only", {
                "local_place.camp.real": self.fact(True, state="declared", proof_depth=None),
                "capabilities.dialogue.choice.fight.visible": self.fact(True, state="declared", proof_depth=None),
            }),
            self.candidate("inspected.only", {
                "local_place.camp.real": self.fact(True, state="inspected", proof_depth="startup"),
                "capabilities.dialogue.choice.fight.visible": self.fact(True, state="inspected", proof_depth="startup"),
            }),
            self.candidate("run.verified", {
                "local_place.camp.real": self.fact(True, state="inspected", proof_depth="startup"),
                "capabilities.dialogue.choice.fight.visible": self.fact(True, state="run-verified", proof_depth="interaction"),
            }),
        ])

        self.assertEqual(result.ranked_scenario_ids, ("run.verified",))
        self.assertEqual(
            [item.reason for item in result.candidates[0].hard_results],
            ["below_minimum_evidence", "below_minimum_evidence"],
        )
        self.assertEqual(
            [item.reason for item in result.candidates[1].hard_results],
            ["matched", "below_minimum_evidence"],
        )

    def test_unknown_stale_and_contradicted_hard_facts_reject(self) -> None:
        request = self.query([
            {"key": "capabilities.dialogue.choice.fight.visible", "op": "eq", "value": True, "minimum_evidence": "run-verified"},
        ])
        result = evaluate_registry_query(request, [
            self.candidate("unknown", {}),
            self.candidate("stale", {
                "capabilities.dialogue.choice.fight.visible": self.fact(True, state="stale"),
            }),
            self.candidate("contradicted", {
                "capabilities.dialogue.choice.fight.visible": self.fact(True, state="contradicted"),
            }),
        ])

        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(
            [candidate.hard_results[0].reason for candidate in result.candidates],
            ["unknown_fact", "stale", "contradicted"],
        )

    def test_malformed_proof_depth_fails_closed_without_changing_authority(self) -> None:
        request = self.query([
            {"key": "capabilities.dialogue.choice.fight.visible", "op": "eq", "value": True, "minimum_evidence": "run-verified"},
        ])
        result = evaluate_registry_query(request, [self.candidate("bad.depth", {
            "capabilities.dialogue.choice.fight.visible": self.fact(
                True,
                state="run-verified",
                proof_depth="feature-path",
            ),
        })])

        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(result.candidates[0].hard_results[0].reason, "malformed_proof_depth")

    def test_malformed_candidate_values_fail_closed(self) -> None:
        request = self.query([
            {"key": "world.distance", "op": "range", "minimum": 1, "maximum": 3},
        ])
        result = evaluate_registry_query(request, [
            self.candidate("boolean.number", {
                "world.distance": self.fact(True),
            }),
            self.candidate("nested.value", {
                "world.distance": self.fact({"nested": {"not": "bounded"}}),
            }),
        ])

        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(result.candidates[0].hard_results[0].reason, "non_numeric_value")
        self.assertEqual(result.candidates[1].hard_results[0].reason, "malformed_value")

    def test_preferences_are_caller_order_lexicographic_with_scenario_id_ties(self) -> None:
        request = self.query([
            {"key": "player.thirst", "op": "eq", "value": False},
        ], preferences=[
            {"key": "actors.visible", "op": "contains", "value": "forest"},
            {"key": "capabilities.trade", "op": "eq", "value": True},
        ])
        result = evaluate_registry_query(request, [
            self.candidate("z.last", {
                "player.thirst": self.fact(False),
                "actors.visible": self.fact(["forest"]),
                "capabilities.trade": self.fact(False),
            }),
            self.candidate("a.first", {
                "player.thirst": self.fact(False),
                "actors.visible": self.fact(["forest"]),
                "capabilities.trade": self.fact(False),
            }),
            self.candidate("preference.two", {
                "player.thirst": self.fact(False),
                "actors.visible": self.fact(["camp"]),
                "capabilities.trade": self.fact(True),
            }),
        ])

        self.assertEqual(result.ranked_scenario_ids, ("a.first", "z.last", "preference.two"))

    def test_prose_fields_are_ignored_when_no_typed_fact_exists(self) -> None:
        request = self.query([
            {"key": "actors.fight", "op": "eq", "value": "available"},
        ])
        result = evaluate_registry_query(request, [self.candidate(
            "prose.only",
            {},
            description="Camp Fight proves a visible observer.",
            world="forest",
        )])

        self.assertEqual(result.ranked_scenario_ids, ())
        self.assertEqual(result.candidates[0].hard_results[0].reason, "unknown_fact")


if __name__ == "__main__":
    unittest.main()
