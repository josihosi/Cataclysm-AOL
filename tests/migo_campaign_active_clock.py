#!/usr/bin/env python3
"""Run the fixed-setup native Mi-go clock pilot with a distinct game RNG seed."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from overmap_ai_seed_campaign import ROOT, data_digest, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--game-seed', type=int, required=True)
    parser.add_argument('--setup-seed', type=int, required=True,
                        help='recorded independently; fixture setup is fixed')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--earn-cycle', action='store_true')
    parser.add_argument('--world-reload', action='store_true')
    args = parser.parse_args()
    if not (0 <= args.setup_seed < 2 ** 64 and 0 < args.game_seed < 2 ** 32):
        parser.error('setup seed must be uint64 and game seed a nonzero uint32')
    if args.world_reload and not args.earn_cycle:
        parser.error('--world-reload requires --earn-cycle')
    binary = args.binary.resolve(strict=True)
    source = ROOT / 'tests/migo_campaign_active_clock_test.cpp'
    binding = {'binary_sha256': digest(binary), 'source_sha256': digest(source),
               'runner_sha256': digest(Path(__file__)), 'data_sha256': data_digest(),
               'setup_seed': args.setup_seed,
               'setup': 'fixed single pod and declared resident',
               'game_seed': args.game_seed, 'earn_cycle': args.earn_cycle,
               'world_reload': args.world_reload}
    with tempfile.TemporaryDirectory(prefix='migo-clock-') as userdir:
        env = {k: v for k, v in os.environ.items() if not k.startswith(('CAOL_', 'OPENCLAW_'))}
        if args.earn_cycle:
            env['CAOL_MIGO_EARN_CYCLE'] = '1'
        if args.world_reload:
            env['CAOL_MIGO_WORLD_RELOAD'] = '1'
        proc = subprocess.run(['/usr/bin/nice', '-n', '10', str(binary),
                               'seed campaign migo loaded unloaded frozen active clock',
                               '--rng-seed', str(args.game_seed), '--user-dir', userdir, '--drop-world'],
                              cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    def events(prefix):
        return [line[line.index(prefix):] for line in proc.stdout.splitlines() if prefix in line]

    rows = events('MIGO_CLOCK ')
    samples = []
    for line in rows:
        fields = dict(token.split('=', 1) for token in line.split()[1:])
        samples.append(fields)
    previous = None
    first_discrepancy = None
    for sample in samples:
        if previous and sample['phase'] == 'active_unloaded' and int(sample['delta']) > 0:
            supplied = int(sample['elapsed']) - int(previous['elapsed'])
            if supplied != int(sample['delta']):
                first_discrepancy = {'phase': sample['phase'], 'step': int(sample['step']),
                                     'world_turn': int(sample['world']),
                                     'activity_delta': int(sample['delta']),
                                     'production_delta': supplied,
                                     'boundary': 'source unloaded; native production service skips source'}
                break
        previous = sample
    required = {'loaded', 'active_unloaded', 'frozen', 'loaded_again'}
    complete = required.issubset({s['phase'] for s in samples})
    if args.world_reload:
        complete = complete and bool(events('MIGO_RELOAD '))
    result = {'binding': binding, 'exit': proc.returncode,
              'observation_status': 'complete' if proc.returncode == 0 and complete else 'incomplete',
              'first_discrepancy': first_discrepancy,
              'termination': 'signal' if proc.returncode < 0 else 'exited',
              'binary_unchanged': digest(binary) == binding['binary_sha256'],
              'data_unchanged': data_digest() == binding['data_sha256'],
              'samples': rows,
              'cost_and_cycle': [event for prefix in ('MIGO_COST ', 'MIGO_CYCLE ', 'MIGO_RELOAD ')
                                 for event in events(prefix)],
              'output_tail': proc.stdout[-6000:] if proc.returncode or not complete else None,
              'evidence_limit': 'Fixed setup and direct native service calls; this is not a full-turn timing benchmark.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        raise SystemExit('refusing to overwrite a completed observation')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'exit': proc.returncode, 'samples': len(rows), 'output': str(args.output)}))
    return 0 if proc.returncode == 0 and complete and result['binary_unchanged'] and result['data_unchanged'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
