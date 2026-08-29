#!/usr/bin/env python3
"""Focused R-014 proof for durable catalog, run receipts, and reusable gaps."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
from scenario_registry_store import open_registry, record_capability_contract_revision  # noqa: E402


class CockpitDurableCatalogTest(unittest.TestCase):
    def test_catalog_gap_reuse_and_synthetic_finish_are_compact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            database = str(Path(directory) / "registry.sqlite3")
            with open_registry(database) as connection:
                record_capability_contract_revision(connection, capability_id="game.observe", contract={
                    "summary": "Read the avatar's visible surroundings.", "inputs": {}, "results": {},
                    "preconditions": [], "postconditions": [], "recovery": [], "examples": [],
                    "proof_effects": ["focused"], "supported_scenarios": [], "platforms": [],
                    "validation_evidence": "unavailable",
                })
            service = cockpit.CockpitService(database)
            searched = service.call({"action": "capability.search", "requirements": "observe"})
            described = service.call({"action": "capability.describe", "id": "game.observe"})
            first_status = service.call({"action": "run.status", "run_id": "synthetic-r014", "scenario_id": "one"})
            second_status = service.call({"action": "run.status", "run_id": "synthetic-r014"})
            first_gap = service.call({
                "action": "gap.report", "run_id": "synthetic-r014", "scenario_id": "one",
                "blocked_intent": "game.trade", "missing_kind": "action",
                "evidence": {"observed": "action is not advertised"},
                "reusable_outcome": "Expose a structured trade action.", "affected_scenarios": ["two"],
            })
            second_gap = service.call({
                "action": "gap.report", "run_id": "synthetic-r014-b", "scenario_id": "two",
                "blocked_intent": "game.trade", "missing_kind": "action",
                "evidence": {"observed": "action is not advertised"},
                "reusable_outcome": "Expose a structured trade action.", "affected_scenarios": ["one"],
            })
            retrieved = service.call({"action": "gap.search", "scenario_id": "two"})
            finished = service.call({"action": "run.finish", "run_id": "synthetic-r014", "scenario_id": "one"})

        self.assertTrue(searched["ok"])
        self.assertEqual(searched["result"]["matches"][0]["id"], "game.observe")
        self.assertEqual(described["result"]["contract"]["summary"], "Read the avatar's visible surroundings.")
        self.assertEqual(first_status["result"]["evidence_effect"], "none")
        self.assertEqual(first_status["result"]["observed_cost"], {"state": "unavailable"})
        self.assertEqual(second_status["result"]["delta"], "unchanged")
        self.assertEqual(first_gap["result"]["gap_id"], second_gap["result"]["gap_id"])
        self.assertTrue(second_gap["result"]["linked"])
        self.assertEqual(len(retrieved["result"]["matches"]), 1)
        self.assertEqual(finished["result"]["state"], "finished")
        encoded = json.dumps([searched, described, first_status, second_status, first_gap, retrieved, finished]).lower()
        for forbidden in ("token", "pid", "offset", "ocr", "logs", "path", "subprocess"):
            self.assertNotIn(forbidden, encoded)


if __name__ == "__main__":
    unittest.main()
