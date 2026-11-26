#!/usr/bin/env python3
"""Run the real-smoke Mi-go response fixture as a fixed-setup seeded pilot."""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile

from overmap_ai_seed_campaign import ROOT, data_digest, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--build-receipt', type=Path, help='completed worker receipt binding the executable and compiled fixture')
    parser.add_argument('--setup-seed', type=int, required=True)
    parser.add_argument('--game-seed', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--continue-visible', action='store_true')
    parser.add_argument('--sample-party', action='store_true',
                        help='track the freshly selected roster instead of requiring the original replay IDs')
    parser.add_argument('--recloak-after-contact', action='store_true')
    parser.add_argument('--world-reload', action='store_true')
    args = parser.parse_args()
    if args.sample_party and not args.continue_visible:
        parser.error('sample party requires visible continuation')
    if args.world_reload and not args.recloak_after_contact:
        parser.error('world reload requires the recloaked continuation')
    if args.recloak_after_contact and not args.continue_visible:
        parser.error('recloak requires visible continuation')
    if not (0 <= args.setup_seed < 2 ** 64 and 0 < args.game_seed < 2 ** 32):
        parser.error('invalid setup or game seed')
    if args.output.exists():
        parser.error('refusing to overwrite a previous result')
    binary = args.binary.resolve(strict=True)
    binary_sha256 = digest(binary)
    fixture_sha256 = None
    receipt_sha256 = None
    if args.build_receipt:
        try:
            receipt = json.loads(args.build_receipt.read_text())
            if not isinstance(receipt, dict):
                raise ValueError('expected a receipt object')
            bindings = receipt['bindings']
            if receipt.get('schema') != 'de67.worker-result-receipt.v1' or receipt.get('disposition') != 'completed':
                raise ValueError('expected a completed worker receipt')
            fixture_sha256 = bindings['fixture_sha256']
            if not isinstance(fixture_sha256, str) or not re.fullmatch(r'[0-9a-f]{64}', fixture_sha256):
                raise ValueError('invalid compiled fixture hash')
            if bindings['binary_sha256'] != binary_sha256:
                raise ValueError('build receipt executable hash does not match binary')
            receipt_sha256 = digest(args.build_receipt)
        except (OSError, ValueError, KeyError, TypeError) as error:
            parser.error('invalid build receipt: ' + str(error))
    identity = {'binary_sha256': binary_sha256,
                'fixture_sha256': fixture_sha256,
                'working_fixture_sha256': digest(ROOT / 'tests/migo_tower_test.cpp'),
                'fixture_binding': 'receipt-declared' if receipt_sha256 else 'working-source-only',
                'build_receipt_sha256': receipt_sha256,
                'native_sha256': digest(ROOT / 'src/migo_tower_native.cpp'),
                'runner_sha256': digest(Path(__file__)), 'data_sha256': data_digest(),
                'setup_seed': args.setup_seed, 'game_seed': args.game_seed,
                'setup': 'fixed fresh tower, real generated roster and daytime smoke, avatar cloaked',
                'sample_party': args.sample_party, 'continue_visible': args.continue_visible, 'recloak_after_contact': args.recloak_after_contact, 'world_reload': args.world_reload}
    env = {k: v for k, v in os.environ.items() if not k.startswith(('CAOL_', 'OPENCLAW_'))}
    env['CAOL_MIGO_RESPONSE_PILOT'] = '1'
    if args.sample_party:
        env['CAOL_MIGO_RESPONSE_SAMPLE_PARTY'] = '1'
    if args.continue_visible:
        env['CAOL_MIGO_RESPONSE_VISIBLE'] = '1'
    if args.recloak_after_contact:
        env['CAOL_MIGO_RESPONSE_RECLOAK'] = '1'
    if args.world_reload:
        env['CAOL_MIGO_RESPONSE_WORLD_RELOAD'] = '1'
    with tempfile.TemporaryDirectory(prefix='migo-response-') as userdir:
        proc = subprocess.run(['/usr/bin/nice', '-n', '10', str(binary),
                               'migo_real_signal_receiver_and_map_spawn_share_saved_residents',
                               '--rng-seed', str(args.game_seed), '--user-dir', userdir, '--drop-world'],
                              cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    lines = [line[line.index('MIGO_RESPONSE '):] for line in proc.stdout.splitlines()
             if 'MIGO_RESPONSE ' in line]
    result = {'binding': identity, 'exit': proc.returncode,
              'termination': 'signal' if proc.returncode < 0 else 'exited',
              'binary_unchanged': digest(binary) == identity['binary_sha256'],
              'data_unchanged': data_digest() == identity['data_sha256'],
              'observation': lines, 'output_tail': proc.stdout[-8000:] if proc.returncode else None,
              'limit': 'Native scheduler, horde and local updates; avatar moved at step 181; optional cloak removal at 301 and restoration at 311; no complete simulation-turn timing.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    complete = any(line.startswith('MIGO_RESPONSE boundary ') for line in lines)
    reload_observed = any(line.startswith('MIGO_RESPONSE world_reload ') for line in lines)
    reload_complete = not args.world_reload or reload_observed
    print(json.dumps({'exit': proc.returncode, 'observations': len(lines), 'boundary': complete,
                      'world_reload': reload_observed, 'world_reload_required': args.world_reload,
                      'output': str(args.output)}))
    return 0 if proc.returncode == 0 and complete and reload_complete and result['binary_unchanged'] and result['data_unchanged'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
