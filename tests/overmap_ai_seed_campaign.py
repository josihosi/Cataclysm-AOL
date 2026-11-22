#!/usr/bin/env python3
"""Fresh-process seeded campaigns; see --help for campaign operations.

The lock is inherited by the native child. If the runner dies, another runner
cannot retry until that child exits. A claimed attempt without a result is
incomplete and is retried only after the lock can be acquired.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SCENARIO = 'first-sight-v1'
JOURNEY = 'scout-journey-v1'
TEST = 'seed campaign first sight pilot'


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def data_digest():
    """Hash paths, file content and symlink targets for the data tree."""
    root = ROOT / 'data'
    h = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix().encode()
        if path.is_symlink():
            h.update(b'L' + len(name).to_bytes(4, 'big') + name + os.readlink(path).encode() + b'\0')
        elif path.is_file():
            h.update(b'F' + len(name).to_bytes(4, 'big') + name + bytes.fromhex(digest(path)))
    return h.hexdigest()


def resolve(seed, game_seed, scenario=SCENARIO):
    if not (0 <= seed < 2**64 and 0 < game_seed < 2**32):
        raise ValueError('setup seed must be uint64; game seed must be nonzero uint32')
    if scenario == JOURNEY:
        bits = hashlib.sha256(b'scout-journey-v1:' + str(seed).encode('ascii')).digest()[0]
        return {'scenario': JOURNEY, 'setup_seed': seed, 'game_seed': game_seed,
                'setup_rng': 'sha256-first-byte-bits-v1',
                'setup': {'cannibal': bits & 1, 'acquire': (bits >> 1) & 1},
                'initial_world': 'fresh native test world; fixture creates camp and signal',
                'mods': ['dda', 'test_data'], 'options': {'NO_NPC_FOOD': 'true', 'LLM_INTENT_ENABLE': 'false'},
                'initial_time': 'start of cataclysm, fixture sets daytime',
                'events': [{'transition': 'watch_arrival', 'predicate': 'both scouts reach selected watch', 'action': 'observe'},
                           {'transition': 'departure', 'predicate': 'acquire setup and watching', 'action': 'leave source during watch'},
                           {'transition': 'physical_return', 'predicate': 'committed physical return event', 'action': 'complete return'}]}
    if scenario != SCENARIO:
        raise ValueError('unknown scenario')
    # SHA-256 and the exact byte encoding are part of the scenario revision.
    rolling = hashlib.sha256(b'first-sight-v1:' + str(seed).encode('ascii')).digest()[0] & 1
    return {'scenario': SCENARIO, 'setup_seed': seed, 'game_seed': game_seed,
            'setup_rng': 'sha256-first-byte-low-bit-v1', 'setup': {'rolling': rolling},
            'initial_world': 'none (nogame fixture)', 'mods': {'selected': ['dda', 'test_data'], 'loaded': []}, 'options': {},
            'initial_time': 'not initialized (nogame fixture)',
            'events': [{'transition': 'first_sight_decision', 'predicate': 'player or follower',
                        'action': 'evaluate first-sight parley'},
                       {'transition': 'unreachable_control', 'predicate': 'never', 'action': 'none'}]}


def atomic_json(path, obj):
    path = Path(path)
    fd, name = tempfile.mkstemp(prefix='.write-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as f:
            json.dump(obj, f, sort_keys=True, indent=2)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        os.replace(name, path)
        d = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(d)
        finally:
            os.close(d)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def acquire(directory):
    directory.mkdir(parents=True, exist_ok=True)
    lock = open(directory / 'writer.lock', 'a+')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        lock.close()
        raise RuntimeError('campaign or its native child still owns the writer lock')
    return lock


def coverage(state):
    done = [a for a in state['attempts'] if a['status'] == 'completed']
    family = state['attempts'][0]['recipe']['scenario']
    result = {'attempted': sum(a['status'] != 'pending' for a in state['attempts']),
              'completed': len(done), 'distinct_setups': len({json.dumps(a['recipe']['setup'], sort_keys=True) for a in done}),
              'family': family, 'transitions': {}}
    for event in state['attempts'][0]['recipe']['events']:
        transition = event['transition']
        events = [e for a in done for e in a['result']['events'] if e['transition'] == transition]
        result['transitions'][transition] = {'reached': sum(e['reached'] for e in events),
                                             'missed': sum(not e['reached'] for e in events)}
    return result


def initialize(directory, binary, setup_seeds, game_seeds, scenario=SCENARIO):
    lock = acquire(directory)
    with lock:
        path = directory / 'campaign.json'
        if path.exists():
            raise RuntimeError('campaign already exists')
        binary = binary.resolve(strict=True)
        attempts = []
        for i, (setup, game) in enumerate(zip(setup_seeds, game_seeds)):
            attempts.append({'id': i, 'status': 'pending', 'recipe': resolve(setup, game, scenario)})
        if not attempts or len(setup_seeds) != len(game_seeds):
            raise ValueError('supply equally many setup and game seeds')
        atomic_json(path, {'version': 1, 'binary': str(binary), 'binary_sha256': digest(binary),
                           'native_source_sha256': digest(ROOT / ('tests/bandit_live_world_test.cpp' if scenario == JOURNEY else 'tests/overmap_ai_seed_campaign_test.cpp')),
                           'data_identity': data_digest() if scenario == JOURNEY else 'nogame listener skips init_global_game_state; no game data loaded', 'attempts': attempts})


def replay(source, attempt_id, destination):
    state = json.loads((source / 'campaign.json').read_text())
    attempt = next((a for a in state['attempts'] if a['id'] == attempt_id), None)
    if attempt is None:
        raise ValueError('attempt not found')
    lock = acquire(destination)
    with lock:
        path = destination / 'campaign.json'
        if path.exists():
            raise RuntimeError('campaign already exists')
        state['attempts'] = [{'id': 0, 'status': 'pending', 'recipe': attempt['recipe']}]
        atomic_json(path, state)


def run(directory, limit=None):
    lock = acquire(directory)
    with lock:
        path = directory / 'campaign.json'
        state = json.loads(path.read_text())
        binary = Path(state['binary'])
        if digest(binary) != state['binary_sha256']:
            raise RuntimeError('native executable changed; create a new campaign')
        if state['attempts'][0]['recipe']['scenario'] == JOURNEY and data_digest() != state['data_identity']:
            raise RuntimeError('game data changed; create a new campaign')
        count = 0
        for a in state['attempts']:
            if a['status'] == 'completed':
                continue
            if limit is not None and count >= limit:
                break
            a['status'] = 'claimed'
            a['launches'] = a.get('launches', 0) + 1
            atomic_json(path, state)
            userdir = directory / ('user-' + str(a['id']))
            # This directory belongs only to this attempt; remove after a known exit.
            if userdir.exists():
                shutil.rmtree(userdir)
            userdir.mkdir()
            env = {k: v for k, v in os.environ.items() if not k.startswith(('CAOL_', 'OPENCLAW_'))}
            scenario = a['recipe']['scenario']
            setup = a['recipe']['setup']
            if scenario == JOURNEY:
                env['CAOL_CAMPAIGN_CANNIBAL'] = str(setup['cannibal'])
                env['CAOL_CAMPAIGN_ACQUIRE'] = str(setup['acquire'])
            else:
                env['CAOL_CAMPAIGN_ROLLING'] = str(setup['rolling'])
            command = [str(binary), 'seed campaign connected scout journey' if scenario == JOURNEY else TEST, '--rng-seed', str(a['recipe']['game_seed']),
                       '--user-dir', str(userdir.resolve()), '--drop-world']
            proc = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, pass_fds=(lock.fileno(),))
            shutil.rmtree(userdir)
            if digest(binary) != state['binary_sha256']:
                raise RuntimeError('native executable changed during attempt; attempt remains incomplete')
            if proc.returncode < 0:
                a['last_error'] = {'exit': proc.returncode, 'reason': 'native process terminated by signal'}
                atomic_json(path, state)
                raise RuntimeError('native process terminated by signal; attempt remains incomplete')
            if scenario == JOURNEY:
                if data_digest() != state['data_identity']:
                    raise RuntimeError('game data changed during attempt; attempt remains incomplete')
                lines = [line.split('CAMPAIGN_JOURNEY ', 1)[1] for line in proc.stdout.splitlines()
                         if 'CAMPAIGN_JOURNEY ' in line]
                if len(lines) != 1:
                    a['last_error'] = {'exit': proc.returncode, 'reason': 'missing or duplicate journey observation',
                                       'output_tail': proc.stdout[-4000:]}
                    atomic_json(path, state)
                    raise RuntimeError('journey observation missing or duplicated; attempt remains incomplete')
                obs = json.loads(lines[0])
                fields = {'watch_minute', 'departure_fired', 'return_minute', 'return_generation', 'generation', 'physical_returns'}
                if set(obs) != fields or obs['departure_fired'] not in (0, 1) or obs['departure_fired'] != setup['acquire']:
                    raise RuntimeError('invalid journey observation; attempt remains incomplete')
                watch = obs['watch_minute'] >= 0
                returned = obs['generation'] > 0 and obs['return_generation'] == obs['generation'] and obs['physical_returns'] > 0
                a['result'] = {'exit': proc.returncode, 'outcome': 'pass' if proc.returncode == 0 else 'native_failure',
                               'observation': obs, 'events': [
                                   {'transition': 'watch_arrival', 'reached': watch, 'fired': watch},
                                   {'transition': 'departure', 'reached': bool(obs['departure_fired']), 'fired': bool(obs['departure_fired'])},
                                   {'transition': 'physical_return', 'reached': returned, 'fired': returned}]}
                a['status'] = 'completed'
                atomic_json(path, state)
                count += 1
                continue
            lines = [line.split('CAMPAIGN_EVENT ', 1)[1] for line in proc.stdout.splitlines()
                     if 'CAMPAIGN_EVENT ' in line]
            if len(lines) != 1:
                a['last_error'] = {'exit': proc.returncode, 'reason': 'missing or duplicate native observation',
                                   'output_tail': proc.stdout[-4000:]}
                atomic_json(path, state)
                raise RuntimeError('native observation missing or duplicated; attempt remains incomplete')
            obs = json.loads(lines[0])
            if set(obs) != {'player', 'follower', 'rolling', 'reached', 'parley'} or any(
                    obs[k] not in (0, 1) for k in obs) or obs['rolling'] != a['recipe']['setup']['rolling'] or (
                    obs['reached'] != bool(obs['player'] or obs['follower'])):
                raise RuntimeError('invalid native observation; attempt remains incomplete')
            reached = bool(obs['reached'])
            a['result'] = {'exit': proc.returncode, 'outcome': 'pass' if proc.returncode == 0 else 'native_failure',
                           'observation': obs,
                           'events': [{'transition': 'first_sight_decision', 'reached': reached,
                                       'fired': reached},
                                      {'transition': 'unreachable_control', 'reached': False, 'fired': False}]}
            a['status'] = 'completed'
            atomic_json(path, state)
            count += 1
        return coverage(state)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    init = sub.add_parser('init')
    init.add_argument('directory', type=Path)
    init.add_argument('--binary', type=Path, required=True)
    init.add_argument('--scenario', choices=[SCENARIO, JOURNEY], default=SCENARIO,
                      help='scout-journey-v1 runs a connected native faction journey')
    init.add_argument('--setup-seeds', type=int, nargs='+', required=True)
    init.add_argument('--game-seeds', type=int, nargs='+', required=True)
    clone = sub.add_parser('replay', help='copy an exact resolved recipe into a fresh campaign')
    clone.add_argument('source', type=Path)
    clone.add_argument('attempt_id', type=int)
    clone.add_argument('destination', type=Path)
    execute = sub.add_parser('run')
    execute.add_argument('directory', type=Path)
    execute.add_argument('--limit', type=int)
    status = sub.add_parser('status')
    status.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'init':
            initialize(args.directory, args.binary, args.setup_seeds, args.game_seeds, args.scenario)
        elif args.command == 'replay':
            replay(args.source, args.attempt_id, args.destination)
        elif args.command == 'run':
            print(json.dumps(run(args.directory, args.limit), sort_keys=True))
        else:
            state = json.loads((args.directory / 'campaign.json').read_text())
            print(json.dumps({'coverage': coverage(state), 'attempts': state['attempts']}, sort_keys=True))
    except (OSError, ValueError, RuntimeError) as e:
        print(str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
