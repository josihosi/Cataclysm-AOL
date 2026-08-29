#!/usr/bin/env python3
"""R-014 live-prototype bootstrap contracts."""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

import scenario_registry_store as store  # noqa: E402
from startup_harness import load_scenario, scenario_manifest_binding  # noqa: E402


class R014LivePrototypeBootstrapTest( unittest.TestCase ):
    def setUp( self ) -> None:
        self.scenario = load_scenario( "r014.cockpit_live_prototype_bootstrap_mcw" )

    def test_declares_launcher_native_bootstrap_before_cockpit_observation( self ) -> None:
        labels = [str( step["label"] ) for step in self.scenario["steps"]]
        bootstrap = self.scenario["steps"][labels.index( "emit_r014_native_semantic_bootstrap" )]
        self.assertEqual( bootstrap["kind"], "native_semantic_bootstrap" )
        self.assertNotIn( "keys", bootstrap )
        self.assertEqual(
            bootstrap["native_semantic_checkpoint"],
            {
                "required_actions": ["world.wait"],
                "required_state": "world",
            },
        )
        self.assertLess(
            labels.index( "emit_r014_native_semantic_bootstrap" ),
            labels.index( "bind_r014_live_prototype_observation" ),
        )

    def test_valid_setup_receipt_ingests_but_empty_receipts_reject( self ) -> None:
        receipt = {
            "status": "prepared",
            "interventions": [{
                "operation": "fixture_install_and_monster_setup",
                "arguments": {"required_typeid": "mon_zombie_dog", "required_offset_ms": [6, 0, 0]},
                "target": {"typeid": "mon_zombie_dog", "offset_ms": [6, 0, 0]},
                "native_receipt": {"owner": "fixture_save_transform", "accepted": True},
                "before_facts": {},
                "after_facts": {},
                "evidence_effect": "none_for_manufactured_state",
                "gameplay_credit": False,
            }],
            "evidence_effect": "none_for_manufactured_state",
            "gameplay_credit": False,
        }
        report = {
            "scenario_manifest": scenario_manifest_binding( self.scenario ),
            "scenario_setup": receipt,
        }
        facts = store._extract_report_facts( report )
        with tempfile.TemporaryDirectory() as directory:
            root = Path( directory )
            connection = store.open_registry( str( root / "registry.sqlite3" ) )
            try:
                store.rebuild_manifest_projection( connection, HARNESS_DIR / "scenarios" )
                self.assertTrue( store._is_setup_only_report( report, facts ) )
                report_path = root / "valid.probe.report.json"
                report_path.write_text( json.dumps( report ), encoding="utf-8" )
                adapters = store.BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible"},
                    fixture=lambda _expected: {"status": "compatible"},
                    profile=lambda _expected: {"status": "compatible"},
                )
                ingested = store.ingest_report_reference( connection, report_path, adapters=adapters )
                self.assertEqual( ingested["status"], "ingested" )
                self.assertEqual( ingested["classification"], "setup-only" )
                self.assertTrue( ingested["intervention_ids"] )
                empty = copy.deepcopy( report )
                empty["scenario_setup"] = {"interventions": []}
                empty_path = root / "empty.probe.report.json"
                empty_path.write_text( json.dumps( empty ), encoding="utf-8" )
                rejected = store.ingest_report_reference( connection, empty_path, adapters=adapters )
                self.assertEqual( rejected["status"], "invalid_report" )
                self.assertIn( "non-empty list", rejected["error"] )
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main()
