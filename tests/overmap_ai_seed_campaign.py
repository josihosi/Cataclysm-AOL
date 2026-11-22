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


def recipe_digest(recipe):
    return hashlib.sha256(json.dumps(recipe, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


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
    groups = {}
    for a in done:
        failure = a['result'].get('first_failure')
        if failure:
            key = json.dumps(failure['group'], sort_keys=True, separators=(',', ':'))
            groups.setdefault(key, {'key': failure['group'], 'attempt_ids': [], 'setup_ids': []})
            groups[key]['attempt_ids'].append(a['id'])
            groups[key]['setup_ids'].append(a['recipe']['setup_seed'])
    result['failure_groups'] = list(groups.values())
    return result


def initialize(directory, binary, setup_seeds, game_seeds, scenario=SCENARIO, failure_control='none'):
    lock = acquire(directory)
    with lock:
        path = directory / 'campaign.json'
        if path.exists():
            raise RuntimeError('campaign already exists')
        binary = binary.resolve(strict=True)
        controls = failure_control if isinstance(failure_control, list) else [failure_control] * len(setup_seeds)
        if len(controls) != len(setup_seeds):
            raise ValueError('supply one failure control per attempt')
        if any(control != 'none' for control in controls) and scenario != JOURNEY:
            raise ValueError('failure controls require the scout journey scenario')
        attempts = []
        for i, (setup, game) in enumerate(zip(setup_seeds, game_seeds)):
            recipe = resolve(setup, game, scenario)
            if scenario == JOURNEY:
                recipe['reporting_control'] = controls[i]
            attempts.append({'id': i, 'status': 'pending', 'recipe': recipe, 'recipe_sha256': recipe_digest(recipe)})
        if not attempts or len(setup_seeds) != len(game_seeds):
            raise ValueError('supply equally many setup and game seeds')
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        dirty = subprocess.check_output(['git', 'diff', '--binary', 'HEAD', '--', 'src', 'tests/bandit_live_world_test.cpp'], cwd=ROOT)
        atomic_json(path, {'version': 1, 'checkout_commit': commit, 'checkout_dirty_sha256': hashlib.sha256(dirty).hexdigest(),
                           'runner_sha256': digest(Path(__file__)), 'binary': str(binary), 'binary_sha256': digest(binary),
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
        if attempt.get('recipe_sha256') and attempt['recipe_sha256'] != recipe_digest(attempt['recipe']):
            raise RuntimeError('recorded recipe changed')
        state['attempts'] = [{'id': 0, 'status': 'pending', 'recipe': attempt['recipe'],
                              'recipe_sha256': recipe_digest(attempt['recipe'])}]
        atomic_json(path, state)


def journey_invariant_result(recipe, observation):
    """Evaluate the native final summary, optionally injecting a labelled reporting control.

    These controls alter a disposable copy, never the native game or its output.
    Earlier fixture-internal failures remain incomplete, not attributed here.
    """
    actual = dict(observation)
    observed = dict(actual)
    control = recipe.get('reporting_control', 'none')
    if control == 'return_before_watch':
        observed['return_minute'] = observed['watch_minute'] - 1
    elif control == 'missing_physical_return':
        observed['physical_returns'] = 0
    elif control != 'none':
        raise ValueError('unknown reporting control')
    checks = [
        ('watch_reached', observed['watch_minute'] >= 0, 'watch_minute', 'nonnegative'),
        ('departure_matches_schedule', observed['departure_fired'] == recipe['setup']['acquire'],
         'departure_fired', recipe['setup']['acquire']),
        ('return_after_watch', observed['return_minute'] >= observed['watch_minute'],
         'return_minute', 'at or after watch_minute'),
        ('return_generation_matches', observed['generation'] > 0 and
         observed['return_generation'] == observed['generation'], 'return_generation', 'active generation'),
        ('physical_return_committed', observed['physical_returns'] > 0, 'physical_returns', 'positive'),
    ]
    passed = []
    first = None
    for invariant, valid, field, expected in checks:
        if not valid:
            first = {'invariant': invariant,
                     'boundary': 'fixture CAMPAIGN_JOURNEY final summary after finish_return',
                     'last_valid': {'checked_invariants': passed.copy(), 'native_summary': actual},
                     'first_invalid': {'field': field, 'value': observed[field], 'expected': expected,
                                       'evaluated_summary': observed},
                     'group': {'invariant': invariant,
                               'faction': 'cannibal' if recipe['setup']['cannibal'] else 'bandit',
                               'phase': 'return_summary', 'owner': 'not observed',
                               'acquire': recipe['setup']['acquire']},
                     'classification': 'test_control' if control != 'none' else 'invariant_contradiction'}
            break
        passed.append(invariant)
    return {'native_summary': actual, 'evaluated_summary': observed, 'control': control,
            'first_failure': first,
            'executed_actions': ['reach_watch'] + (['depart_during_watch'] if actual['departure_fired'] else []) +
                                ['complete_watch', 'finish_return'],
            'observation_boundary': 'fixture CAMPAIGN_JOURNEY final summary after finish_return'}


def run(directory, limit=None):
    lock = acquire(directory)
    with lock:
        path = directory / 'campaign.json'
        state = json.loads(path.read_text())
        if state.get('runner_sha256') and digest(Path(__file__)) != state['runner_sha256']:
            raise RuntimeError('campaign runner changed; restore the recorded runner for replay')
        for attempt in state['attempts']:
            if attempt.get('recipe_sha256') and attempt['recipe_sha256'] != recipe_digest(attempt['recipe']):
                raise RuntimeError('recorded recipe changed')
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
                if set(obs) != fields or obs['departure_fired'] not in (0, 1):
                    raise RuntimeError('invalid journey observation; attempt remains incomplete')
                watch = obs['watch_minute'] >= 0
                returned = obs['generation'] > 0 and obs['return_generation'] == obs['generation'] and obs['physical_returns'] > 0
                report = journey_invariant_result(a['recipe'], obs)
                if proc.returncode:
                    report['unconfirmed_failure'] = report['first_failure']
                    report['first_failure'] = None
                outcome = 'native_failure' if proc.returncode else ('invariant_failure' if report['first_failure'] else 'pass')
                a['result'] = {'exit': proc.returncode, 'outcome': outcome,
                               'observation': obs, **report, 'events': [
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
    init.add_argument('--failure-control', choices=['none', 'return_before_watch', 'missing_physical_return'],
                      default='none', help='test-only reporting mutation, never changes native behavior')
    init.add_argument('--failure-controls', nargs='+', choices=['none', 'return_before_watch', 'missing_physical_return'],
                      help='one test-only control per attempt')
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
            initialize(args.directory, args.binary, args.setup_seeds, args.game_seeds, args.scenario, args.failure_controls or args.failure_control)
        elif args.command == 'replay':
            replay(args.source, args.attempt_id, args.destination)
        elif args.command == 'run':
            result = run(args.directory, args.limit)
            print(json.dumps(result, sort_keys=True))
            state = json.loads((args.directory / 'campaign.json').read_text())
            if any(a.get('result', {}).get('outcome') in ('invariant_failure', 'native_failure')
                   for a in state['attempts']):
                return 1
        else:
            state = json.loads((args.directory / 'campaign.json').read_text())
            print(json.dumps({'coverage': coverage(state), 'attempts': state['attempts']}, sort_keys=True))
    except (OSError, ValueError, RuntimeError) as e:
        print(str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
