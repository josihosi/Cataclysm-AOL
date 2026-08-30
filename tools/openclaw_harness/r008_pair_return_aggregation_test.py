#!/usr/bin/env python3
"""Focused fail-closed controls for R-008's native pair-return boundary."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import classify_structural_member_physical_return_pair  # noqa: E402


class R008PairReturnAggregationTest(unittest.TestCase):
    run_id = "r008-fresh-run"
    binding_id = "r008-bound-stream"
    predicate = {
        "domain": "bandit_live_world",
        "transition": "structural_member_physical_return",
        "outcome": "committed",
        "previous_state": "local_contact",
        "new_state": "at_home",
        "site_id": "overmap_special:bandit_camp@177,13,0",
        "operation_id": "overmap_special:bandit_camp@177,13,0#structural",
        "generation": 1,
        "actor_ids": [4, 5],
        "owner_progression": ["local", "abstract"],
        "handoff_epoch_progression": [1, 2],
    }

    def receipt(self, sequence: int, actor_id: int, *, owner: str, epoch: int, **overrides):
        value = {
            "schema_version": 1,
            "sequence": sequence,
            "run_id": self.run_id,
            "game_minutes": 8711,
            "domain": "bandit_live_world",
            "transition": "structural_member_physical_return",
            "outcome": "committed",
            "site_id": "overmap_special:bandit_camp@177,13,0",
            "operation_id": "overmap_special:bandit_camp@177,13,0#structural",
            "generation": 1,
            "handoff_epoch": epoch,
            "simulation_owner": owner,
            "previous_state": "local_contact",
            "new_state": "at_home",
            "actor_ids": [actor_id],
            "_harness_binding_id": self.binding_id,
        }
        value.update(overrides)
        return value

    def exact_pair(self):
        return [
            self.receipt(3, 4, owner="local", epoch=1),
            self.receipt(4, 5, owner="abstract", epoch=2),
        ]

    def classify(self, events, *, binding_id=None, predicate=None):
        return classify_structural_member_physical_return_pair(
            events,
            run_id=self.run_id,
            binding_id=self.binding_id if binding_id is None else binding_id,
            predicate=self.predicate if predicate is None else predicate,
        )

    def test_accepts_the_exact_two_native_receipts(self):
        result = self.classify(self.exact_pair())

        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["terminal_owner"], "abstract")
        self.assertEqual(result["terminal_handoff_epoch"], 2)
        self.assertEqual([reference["actor_id"] for reference in result["event_references"]], [4, 5])

    def test_rejects_partial_duplicate_stale_and_mixed_identity_receipts(self):
        pair = self.exact_pair()
        controls = {
            "partial": pair[:1],
            "duplicate": [pair[0], self.receipt(4, 4, owner="local", epoch=1), pair[1]],
            "stale_run": [{**pair[0], "run_id": "prior-run"}, pair[1]],
            "mixed_site": [pair[0], {**pair[1], "site_id": "other-site"}],
            "mixed_operation": [pair[0], {**pair[1], "operation_id": "other#structural"}],
            "mixed_generation": [pair[0], {**pair[1], "generation": 2}],
            "mixed_minute": [pair[0], {**pair[1], "game_minutes": 8712}],
        }
        for name, events in controls.items():
            with self.subTest(name=name):
                self.assertEqual(self.classify(events)["status"], "unproved")

    def test_rejects_mixed_binding_receipt_order_epoch_and_owner(self):
        pair = self.exact_pair()
        controls = {
            "mixed_binding": [pair[0], {**pair[1], "_harness_binding_id": "other-binding"}],
            "wrong_stream_binding": pair,
            "reverse_order": [pair[1], pair[0]],
            "wrong_epoch": [pair[0], {**pair[1], "handoff_epoch": 1}],
            "wrong_owner": [pair[0], {**pair[1], "simulation_owner": "local"}],
        }
        for name, events in controls.items():
            with self.subTest(name=name):
                result = self.classify(
                    events,
                    binding_id="other-binding" if name == "wrong_stream_binding" else None,
                )
                self.assertEqual(result["status"], "unproved")


if __name__ == "__main__":
    unittest.main()
