#!/usr/bin/env python3
"""Checks the isolated representative human journey footing."""
import json
import sys
import unittest
from pathlib import Path

HARNESS = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS))
import startup_harness
from scenario_registry import validate_manifest


class HumanJourneyFixtureTest(unittest.TestCase):
    def test_isolated_setup_retains_geometry_and_only_adds_brazier(self):
        old = startup_harness.resolve_fixture_payload('r050_rural_bandit_remote_reach_v1', 'live-debug')
        new = startup_harness.resolve_fixture_payload('r050_human_journey_field_v1', 'live-debug')
        transforms = new['save_transforms']
        startup_harness.validate_fixture_save_transform_chain(transforms)
        self.assertEqual(transforms[0], old['save_transforms'][0])
        self.assertEqual(transforms[-1], old['save_transforms'][-1])
        self.assertEqual(transforms[1], {'kind': 'player_items', 'player_save': '#VGVzdDAw.sav.zzip',
                         'storage': 'live_accessible_wielded', 'replace_existing_weapon': False,
                         'items': [{'typeid': 'brazier', 'count': 1}]})
        self.assertEqual(transforms[-1]['minimum_distance_omt'], 10)

    def test_scenario_uses_its_own_profile_and_the_qualified_fixture(self):
        path = HARNESS / 'scenarios/bandit.r050_human_journey_field_isolated_mcw.json'
        manifest = json.loads(path.read_text())
        self.assertEqual(validate_manifest(manifest, path=path)['validation']['status'], 'valid')
        self.assertEqual(manifest['fixture'], 'r050_human_journey_field_v1')
        self.assertEqual(manifest['profile'], 'r050-human-journey-isolated')
        self.assertEqual(manifest['runtime_contract']['profile'], manifest['profile'])
        self.assertEqual(manifest['runtime_contract']['requirements']['profile'], manifest['profile'])
        self.assertFalse(manifest['runtime_contract']['grants_gameplay_proof'])

    def test_deploy_comparison_uses_same_footing_and_targeted_diagnostics(self):
        path = HARNESS / 'scenarios/bandit.r050_brazier_deploy_diagnostic_mcw.json'
        manifest = json.loads(path.read_text())
        self.assertEqual(validate_manifest(manifest, path=path)['validation']['status'], 'valid')
        self.assertEqual(manifest['fixture'], 'r050_human_journey_field_v1')
        self.assertEqual(manifest['profile_option_overrides'], {'RAID_PERFORMANCE_DIAGNOSTIC': 'normal'})
        self.assertEqual(manifest['runtime_contract']['profile'], manifest['profile'])
        self.assertNotIn('post_relaunch', manifest)
        self.assertFalse(manifest['runtime_contract']['grants_gameplay_proof'])


if __name__ == '__main__':
    unittest.main()
