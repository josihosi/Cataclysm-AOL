import copy
import json
import shutil
import tempfile
import unittest
from pathlib import Path

import startup_harness as harness

FIXTURE = Path(__file__).parent / 'fixtures/saves/live-debug/travelling_ordinary_pair_preflight_001/manifest.json'
RAW = Path(__file__).parent / 'fixtures/saves/live-debug/tmp_bandit_live_world_local_contact_raw_2026-04-23/save/McWilliams'
SITE = 'overmap_special:bandit_camp@140,51,0'


def read_site(world):
    payload = json.loads((world / 'dimension_data.gsav').read_text().split('\n', 1)[1])
    return next(s for s in payload['overmapbuffer']['bandit_live_world']['sites'] if s['site_id'] == SITE)


def read_bodies(world):
    bodies = {}
    for path in (world / 'overmaps').glob('o.*.*.zzip'):
        plain, _, payload = harness.extract_overmap_payload(path)
        try:
            for body in payload.get('npcs', []):
                if body.get('id') in (4, 5):
                    bodies[body['id']] = copy.deepcopy(body)
        finally:
            harness.cleanup_extracted_overmap(plain, keep=not bool(payload.get('_created_plain', False)))
    return bodies


class OrdinaryScoutFootingTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.world = Path(self.tmp.name) / 'McWilliams'
        shutil.copytree(RAW, self.world)
        manifest = json.loads(FIXTURE.read_text())
        self.transforms = harness.normalize_fixture_save_transforms(manifest['save_transforms'], manifest_path=FIXTURE)
        harness.apply_fixture_save_transforms(self.world, self.transforms[:-2])
        self.footing = self.transforms[-2]
        self.leases = self.transforms[-1]

    def test_installed_producer_preserves_bodies_and_site_except_declared_fields(self):
        old_site, old_bodies = read_site(self.world), read_bodies(self.world)
        result = harness.apply_fixture_save_transforms(self.world, [self.footing])[0]
        new_site, new_bodies = read_site(self.world), read_bodies(self.world)
        self.assertFalse(result['gameplay_credit'])
        self.assertEqual(new_site.pop('active_target_omt'), [141, 41, 0])
        old_site.pop('active_target_omt', None)
        self.assertEqual(new_site, old_site)
        self.assertEqual(set(new_bodies), {4, 5})
        for member, expected in ((4, [3388, 1005, 0]), (5, [3389, 1005, 0])):
            self.assertEqual(new_bodies[member].pop('location'), expected)
            old_bodies[member].pop('location')
            self.assertEqual(new_bodies[member], old_bodies[member])

    def test_rejects_wrong_roster_without_writes(self):
        before = (self.world / 'dimension_data.gsav').read_bytes()
        old_bodies = read_bodies(self.world)
        invalid = copy.deepcopy(self.footing)
        invalid['member_positions_ms'][99] = invalid['member_positions_ms'].pop(5)
        with self.assertRaises(SystemExit):
            harness.apply_fixture_save_transforms(self.world, [invalid])
        self.assertEqual((self.world / 'dimension_data.gsav').read_bytes(), before)
        self.assertEqual(read_bodies(self.world), old_bodies)

    def test_rejects_cross_overmap_before_any_write(self):
        before = (self.world / 'dimension_data.gsav').read_bytes()
        old_bodies = read_bodies(self.world)
        invalid = copy.deepcopy(self.footing)
        invalid['member_positions_ms'][5] = [9000, 1005, 0]
        with self.assertRaises(SystemExit):
            harness.apply_fixture_save_transforms(self.world, [invalid])
        self.assertEqual((self.world / 'dimension_data.gsav').read_bytes(), before)
        self.assertEqual(read_bodies(self.world), old_bodies)

    def _prepare_unrelated_plain_overmap(self):
        overmaps = self.world / 'overmaps'
        shutil.copyfile(overmaps / 'o.0.0.zzip', overmaps / 'o.1.1.zzip')
        plain = self.world / 'o.1.1'
        plain.write_text('# version 42\n' + json.dumps({'npcs': [], 'sentinel': 'retain exact bytes'}) + '\n')
        return plain

    def test_preserves_unrelated_preexisting_plain_on_success(self):
        unrelated = self._prepare_unrelated_plain_overmap()
        original = unrelated.read_bytes()
        harness.apply_fixture_save_transforms(self.world, [self.footing])
        self.assertEqual(unrelated.read_bytes(), original)
        read_bodies(self.world)
        self.assertEqual(unrelated.read_bytes(), original)

    def test_selected_preexisting_plain_uses_normal_rewrite(self):
        archive = self.world / 'overmaps/o.0.0.zzip'
        plain, _, _ = harness.extract_overmap_payload(archive)
        self.assertTrue(plain.exists())
        harness.apply_fixture_save_transforms(self.world, [self.footing])
        self.assertFalse(plain.exists())
        self.assertEqual(read_bodies(self.world)[4]['location'], [3388, 1005, 0])
        self.assertFalse(plain.exists())

    def test_preserves_preexisting_plain_on_rejected_missing_body(self):
        unrelated = self._prepare_unrelated_plain_overmap()
        archive = self.world / 'overmaps/o.0.0.zzip'
        plain, version, payload = harness.extract_overmap_payload(archive)
        payload.pop('_created_plain', None)
        payload['npcs'] = [body for body in payload.get('npcs', []) if body.get('id') != 5]
        plain.write_text(version + '\n' + json.dumps(payload) + '\n')
        originals = {path: path.read_bytes() for path in (plain, unrelated)}
        with self.assertRaises(SystemExit):
            harness.apply_fixture_save_transforms(self.world, [self.footing])
        read_bodies(self.world)
        for path, contents in originals.items():
            self.assertEqual(path.read_bytes(), contents)

    def test_projection_leases_bind_pair_and_preserve_unrelated_plain(self):
        harness.apply_fixture_save_transforms(self.world, [self.footing])
        before = read_bodies(self.world)
        unrelated = self._prepare_unrelated_plain_overmap()
        original = unrelated.read_bytes()
        result = harness.apply_fixture_save_transforms(self.world, [self.leases])[0]
        after = read_bodies(self.world)
        self.assertEqual(result['npc_ids'], [4, 5])
        self.assertEqual(unrelated.read_bytes(), original)
        for member_id in (4, 5):
            self.assertEqual(after[member_id].pop('bandit_live_world_projection_lease'), {
                'site_id': SITE, 'activity_id': SITE + '#fixture_active_pressure',
                'owner': 'local', 'generation': 1, 'handoff_epoch': 1,
                'last_advanced_minutes': 8159,
            })
            before[member_id].pop('bandit_live_world_projection_lease', None)
            self.assertEqual(after[member_id], before[member_id])

    def test_projection_lease_rejection_preserves_preexisting_plain(self):
        unrelated = self._prepare_unrelated_plain_overmap()
        plain, _, _ = harness.extract_overmap_payload(self.world / 'overmaps/o.0.0.zzip')
        originals = {path: path.read_bytes() for path in (plain, unrelated)}
        invalid = dict(self.leases, npc_ids=[98, 99])
        with self.assertRaises(SystemExit):
            harness.apply_fixture_save_transforms(self.world, [invalid])
        for path, contents in originals.items():
            self.assertEqual(path.read_bytes(), contents)

    def test_melee_variant_changes_only_existing_wielded_weapons(self):
        harness.apply_fixture_save_transforms(self.world, [self.footing, self.leases])
        old_bodies = read_bodies(self.world)
        dimension = (self.world / 'dimension_data.gsav').read_bytes()
        player = (self.world / '#Wm9yYWlkYSBWaWNr.sav.zzip').read_bytes()
        path = Path(__file__).parent / 'fixtures/saves/live-debug/travelling_ordinary_melee_follow_001/manifest.json'
        raw = json.loads(path.read_text())['save_transforms']
        transforms = harness.normalize_fixture_save_transforms(raw, manifest_path=path)
        reports = harness.apply_fixture_save_transforms(self.world, transforms)
        after = read_bodies(self.world)
        self.assertEqual((self.world / 'dimension_data.gsav').read_bytes(), dimension)
        self.assertEqual((self.world / '#Wm9yYWlkYSBWaWNr.sav.zzip').read_bytes(), player)
        for member, old_weapon, new_weapon in ((4, 'm17', 'bat'), (5, 'p365', 'machete')):
            self.assertEqual(old_bodies[member].pop('weapon')['typeid'], old_weapon)
            self.assertEqual(after[member].pop('weapon')['typeid'], new_weapon)
            self.assertEqual(after[member], old_bodies[member])
        self.assertTrue(all(not report['ensured_follower'] for report in reports))

    def test_manifest_rejects_collision_and_unexpected_fields(self):
        raw = json.loads(FIXTURE.read_text())['save_transforms'][-2]
        for change in ({'member_positions_ms': {'4': [3388, 1005, 0], '5': [3388, 1005, 0]}}, {'target': 4}):
            bad = dict(raw, **change)
            with self.subTest(change=change), self.assertRaises(SystemExit):
                harness.normalize_fixture_save_transforms([bad], manifest_path=FIXTURE)


if __name__ == '__main__':
    unittest.main()
