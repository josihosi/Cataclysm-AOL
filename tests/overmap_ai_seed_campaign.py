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
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SCENARIO = 'first-sight-v1'
JOURNEY = 'scout-journey-v1'
CROSSING = 'scout-split-reversal-v1'
CUSTODY = 'bandit-payment-custody-v1'
CUSTODY_AUTH = 'bandit-payment-authenticated-v1'
BROOD = 'migo-earned-brood-v1'
BROOD_RELOAD = 'migo-earned-brood-reload-v1'
BROOD_SCENARIOS = (BROOD, BROOD_RELOAD)
BROOD_TEST = 'seed campaign migo loaded unloaded frozen active clock'
CROSSING_TEST = 'structural staged arrival leaves the distant peer on shared travel'
CROSSING_SECTION = 'player leaves and the staged scout resumes distant travel'
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
    if scenario in BROOD_SCENARIOS:
        names = ['earned_active_time'] + (['world_reload'] if scenario == BROOD_RELOAD else []) + ['birth_placement']
        return {'scenario': scenario, 'setup_seed': seed, 'game_seed': game_seed,
                'setup_rng': 'fixed-fixture-v1',
                'setup': {'source': 'one pod and resident', 'execution': 'direct native service calls',
                          'observer': 'explicit avatar relocation'},
                'initial_world': 'fresh native test world', 'mods': ['dda', 'test_data'], 'options': {},
                'events': [{'transition': name, 'predicate': 'verified native observation', 'action': 'observe'} for name in names]}
    if scenario in (CUSTODY, CUSTODY_AUTH):
        return {'scenario': scenario, 'setup_seed': seed, 'game_seed': game_seed,
                'setup_rng': 'fixed-fixture-v1',
                'setup': {'faction': 'bandit', 'physical_item': 'knife_combat',
                          'approach': 'declared fixture report and approach',
                          'observer_action': 'move player away after payment',
                          'opportunity_setup': 'declared seven-member roster, alert lead, opportunity and production claim before payment' if scenario == CUSTODY_AUTH else 'not supplied'},
                'initial_world': 'fresh native test world', 'mods': ['dda', 'test_data'],
                'options': {'LLM_INTENT_ENABLE': 'false'},
                'events': [{'transition': 'physical_payment', 'predicate': 'source item in home stash', 'action': 'trade'},
                           {'transition': 'authenticated_return', 'predicate': 'same operation and generation settled paid', 'action': 'observe'},
                           {'transition': 'once_only_credit', 'predicate': 'settlement credit and repeat service checked', 'action': 'observe'},
                           {'transition': 'next_decision', 'predicate': 'post-settlement cooldown observed', 'action': 'observe'}]}
    if scenario == CROSSING:
        return {'scenario': CROSSING, 'setup_seed': seed, 'game_seed': game_seed,
                'setup_rng': 'fixed-fixture-v1',
                'setup': {'faction': 'bandit', 'initial_split': 'fixture relocates first scout along selected route'},
                'initial_world': 'fresh native test world', 'mods': ['dda', 'test_data'],
                'options': {'NO_NPC_FOOD': 'true', 'LLM_INTENT_ENABLE': 'false'},
                'initial_time': 'start of cataclysm, fixture sets daytime',
                'events': [{'transition': 'one_member_staged', 'predicate': 'exactly one original member staged', 'action': 'observe'},
                           {'transition': 'player_departure', 'predicate': 'one member staged', 'action': 'move player away'},
                           {'transition': 'resumed_travel', 'predicate': 'lease released and production travel commits', 'action': 'observe'}]}
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
                           'native_source_sha256': digest(ROOT / ('tests/migo_campaign_active_clock_test.cpp' if scenario in BROOD_SCENARIOS else ('tests/bandit_live_world_test.cpp' if scenario in (JOURNEY, CROSSING, CUSTODY, CUSTODY_AUTH) else 'tests/overmap_ai_seed_campaign_test.cpp'))),
                           'native_source_scope': 'working-checkout-only' if scenario in BROOD_SCENARIOS else 'working-checkout',
                           'compiled_fixture_sha256': None,
                           'data_identity': data_digest() if scenario in (JOURNEY, CROSSING, CUSTODY, CUSTODY_AUTH, *BROOD_SCENARIOS) else 'nogame listener skips init_global_game_state; no game data loaded', 'attempts': attempts})


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


def crossing_observation(output, events):
    """Require actual successful Catch assertions and run-bound production events."""
    def line_containing(fragment):
        for line in output.splitlines():
            if 'PASSED:' in line and fragment in line:
                return line
        raise ValueError('missing successful assertion: ' + fragment)
    ids_line = line_containing('outing.member_ids == party_ids')
    ids_match = re.search(r'for: \{\s*(\d+),\s*(\d+)\s*\}', ids_line)
    if not ids_match:
        raise ValueError('original member IDs unavailable')
    ids = [int(ids_match.group(1)), int(ids_match.group(2))]
    staged_line = line_containing('structural_admission.staged.size() == 1')
    id_line = line_containing('structural_admission.staged.front().npc_id == first->getID()')
    staged = re.search(r'for: (\d+) == (\d+)', id_line)
    lease = line_containing('lease.owner == "admission"')
    lease_info = re.search(r'minute=(\d+) phase=(\w+) owner=(\w+) epoch=(\d+).*native id=(\d+) at=(\([^)]*\))', lease)
    work_line = line_containing('serviced_active_turns == journey.site().active_outing.activation.active_turns')
    work = re.search(r'for: (\d+) .*? == (\d+)', work_line)
    line_containing('structural_admission_lease_matches( camp, id')
    line_containing('!(first->get_bandit_live_world_projection_lease().present)')
    reversal = line_containing('first->pos_abs() == departure_position')
    movement = line_containing('first->pos_abs() != departure_position')
    position = re.search(r'for: (\([^)]*\)) != (\([^)]*\))', movement)
    if not (staged and lease_info and work and position and int(staged.group(1)) == ids[0] == int(staged.group(2)) == int(lease_info.group(5))):
        raise ValueError('cannot bind the staged member and its boundary')
    selected = [e for e in events if e.get('transition') in ('active_sortie_dispatch', 'scout_travel_prepare', 'scout_motor_service')]
    dispatch = next((e for e in selected if e.get('transition') == 'active_sortie_dispatch' and e.get('actor_ids') == ids), None)
    initial_prepares = [next((e for e in selected if e.get('transition') == 'scout_travel_prepare' and
                              e.get('actor_ids') == [member] and e.get('game_minutes', -1) < int(lease_info.group(1))), None)
                        for member in ids]
    peer = next((e for e in selected if e.get('transition') == 'scout_motor_service' and e.get('actor_ids') == [ids[1]] and
                 e.get('outcome') == 'shared_travel_committed' and e.get('game_minutes', -1) >= int(lease_info.group(1))), None)
    resumed = next((e for e in selected if e.get('transition') == 'scout_motor_service' and e.get('actor_ids') == [ids[0]] and
                    e.get('outcome') == 'shared_travel_committed' and e.get('game_minutes', -1) >= int(lease_info.group(1))), None)
    if not (dispatch and all(initial_prepares) and peer and resumed):
        raise ValueError('run-bound dispatch or actual member movement missing')
    return {'member_ids': ids, 'staged_id': ids[0], 'stage_minute': int(lease_info.group(1)),
            'phase': lease_info.group(2), 'owner': lease_info.group(3), 'epoch': int(lease_info.group(4)),
            'admission_position': lease_info.group(6), 'admission_lease': 'matched admission',
            'after_departure_lease': 'absent', 'serviced_active_turns': int(work.group(1)),
            'active_turns': int(work.group(2)), 'resume_from': position.group(2), 'resume_to': position.group(1),
            'setup_intervention': 'fixture relocates first scout along selected route',
            'assertion_evidence': [staged_line, id_line, lease, reversal, work_line, movement],
            'production_events': [dispatch, *initial_prepares, peer, resumed]}


def brood_observation(output, reload_requested):
    def lines(prefix):
        return [line[line.index(prefix):] for line in output.splitlines() if prefix in line]
    rows = lines('MIGO_CLOCK ')
    samples = [dict(token.split('=', 1) for token in row.split()[1:]) for row in rows]
    previous = None
    failure = None
    for sample in samples:
        if previous and sample['phase'] == 'active_unloaded' and int(sample['delta']) > 0:
            production_delta = int(sample['elapsed']) - int(previous['elapsed'])
            activity_delta = int(sample['delta'])
            if production_delta != activity_delta:
                failure = {'group': {'invariant': 'active_unloaded_clock'},
                           'phase': sample['phase'], 'step': int(sample['step']),
                           'world_turn': int(sample['world']), 'production_delta': production_delta,
                           'activity_delta': activity_delta}
                break
        previous = sample
    cycles = lines('MIGO_CYCLE ')
    reloads = lines('MIGO_RELOAD ')
    if failure:
        return {'samples': rows, 'cycle': cycles, 'reload': reloads, 'first_failure': failure,
                'scope': 'direct native services and explicit avatar relocation; not full simulation turns'}
    if not {'loaded', 'active_unloaded', 'frozen', 'loaded_again'}.issubset({s['phase'] for s in samples}):
        raise ValueError('missing clock phase')
    if not any('earned=604800 birth_sequence=0 ' in line for line in cycles):
        raise ValueError('missing earned birth')
    if reload_requested and not reloads:
        raise ValueError('missing requested reload event')
    if not any('placed=1 roster=2 native_bodies=1 ' in line for line in cycles):
        raise ValueError('missing physical placement')
    return {'samples': rows, 'cycle': cycles, 'reload': reloads,
            'first_failure': failure,
            'scope': 'direct native services and explicit avatar relocation; not full simulation turns'}


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
        if state['attempts'][0]['recipe']['scenario'] in (JOURNEY, CROSSING, CUSTODY, CUSTODY_AUTH, *BROOD_SCENARIOS) and data_digest() != state['data_identity']:
            raise RuntimeError('game data changed; create a new campaign')
        if state['attempts'][0]['recipe']['scenario'] in BROOD_SCENARIOS and digest(ROOT / 'tests/migo_campaign_active_clock_test.cpp') != state['native_source_sha256']:
            raise RuntimeError('working fixture changed; create a new campaign')
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
            elif scenario == CROSSING:
                env['OPENCLAW_HARNESS_RUN_ID'] = 'seed-crossing-' + str(a['id'])
                env['OPENCLAW_HARNESS_TRANSITION_EVENT_PATH'] = str((userdir / 'transitions.jsonl').resolve())
            elif scenario in BROOD_SCENARIOS:
                env['CAOL_MIGO_EARN_CYCLE'] = '1'
                if scenario == BROOD_RELOAD:
                    env['CAOL_MIGO_WORLD_RELOAD'] = '1'
            elif scenario == CUSTODY_AUTH:
                env['CAOL_CAMPAIGN_AUTH_SETUP'] = '1'
            elif scenario != CUSTODY:
                env['CAOL_CAMPAIGN_ROLLING'] = str(setup['rolling'])
            test_name = 'seed campaign bandit payment custody journey' if scenario in (CUSTODY, CUSTODY_AUTH) else ('seed campaign connected scout journey' if scenario == JOURNEY else (CROSSING_TEST if scenario == CROSSING else TEST))
            if scenario in BROOD_SCENARIOS:
                test_name = BROOD_TEST
            command = [str(binary), test_name, '--rng-seed', str(a['recipe']['game_seed']),
                       '--user-dir', str(userdir.resolve()), '--drop-world']
            if scenario in (CUSTODY, CUSTODY_AUTH):
                command = ['/usr/bin/nice', '-n', '10'] + command
            if scenario == CROSSING:
                command = ['/usr/bin/nice', '-n', '10'] + command + ['--section', CROSSING_SECTION, '--success', '--reporter', 'compact']
            proc = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, pass_fds=(lock.fileno(),))
            crossing_events = []
            if scenario == CROSSING and (userdir / 'transitions.jsonl').exists():
                crossing_events = [json.loads(line) for line in (userdir / 'transitions.jsonl').read_text().splitlines()]
            shutil.rmtree(userdir)
            if digest(binary) != state['binary_sha256']:
                raise RuntimeError('native executable changed during attempt; attempt remains incomplete')
            if proc.returncode < 0:
                a['last_error'] = {'exit': proc.returncode, 'reason': 'native process terminated by signal'}
                atomic_json(path, state)
                raise RuntimeError('native process terminated by signal; attempt remains incomplete')
            if scenario in BROOD_SCENARIOS:
                if data_digest() != state['data_identity'] or digest(ROOT / 'tests/migo_campaign_active_clock_test.cpp') != state['native_source_sha256']:
                    raise RuntimeError('brood inputs changed during attempt; attempt remains incomplete')
                try:
                    observed = brood_observation(proc.stdout, scenario == BROOD_RELOAD)
                except (KeyError, ValueError) as error:
                    a['last_error'] = {'exit': proc.returncode, 'reason': str(error), 'output_tail': proc.stdout[-4000:]}
                    atomic_json(path, state)
                    raise RuntimeError('brood observation incomplete: ' + str(error)) from error
                failure = observed.pop('first_failure')
                outcome = 'native_failure' if proc.returncode else ('invariant_failure' if failure else 'pass')
                reached = outcome == 'pass'
                a['result'] = {'exit': proc.returncode, 'outcome': outcome, 'observation': observed,
                               'first_failure': failure if not proc.returncode else None,
                               'events': [{'transition': event['transition'], 'reached': reached, 'fired': reached}
                                          for event in a['recipe']['events']]}
                a['status'] = 'completed'
                atomic_json(path, state)
                count += 1
                if outcome != 'pass':
                    break
                continue
            if scenario in (CUSTODY, CUSTODY_AUTH):
                if data_digest() != state['data_identity']:
                    raise RuntimeError('game data changed during attempt; attempt remains incomplete')
                lines = [line for line in proc.stdout.splitlines() if line.startswith('CUSTODY ')]
                payment = next((line for line in lines if line.startswith('CUSTODY payment ')), None)
                boundary = next((line for line in lines if line.startswith('CUSTODY boundary ')), None)
                settled = bool(boundary and 'settled=1 ' in boundary)
                repeat = next((line for line in lines if line.startswith('CUSTODY repeat ')), None)
                repeated = repeat is not None
                decision = bool(repeat and re.search(r'(?:^|\s)decision=cooldown(?:\s|$)', repeat))
                a['result'] = {'exit': proc.returncode,
                               'outcome': 'native_failure' if proc.returncode else ('settled' if settled else 'boundary_not_reached'),
                               'observation': {'native_lines': lines, 'credit': 'abstract shakedown value',
                                               'custody': 'physical item deposited directly to home stash before return',
                                               'output_tail_on_failure': proc.stdout[-6000:] if proc.returncode else None},
                               'events': [{'transition': 'physical_payment', 'reached': bool(payment), 'fired': bool(payment)},
                                          {'transition': 'authenticated_return', 'reached': settled, 'fired': settled},
                                          {'transition': 'once_only_credit', 'reached': settled and repeated and proc.returncode == 0,
                                           'fired': settled and repeated and proc.returncode == 0},
                                          {'transition': 'next_decision', 'reached': settled and decision,
                                           'fired': settled and decision}]}
                a['status'] = 'completed'
                atomic_json(path, state)
                count += 1
                continue
            if scenario == CROSSING:
                if data_digest() != state['data_identity']:
                    raise RuntimeError('game data changed during attempt; attempt remains incomplete')
                try:
                    observed = crossing_observation(proc.stdout, crossing_events)
                except ValueError as error:
                    a['last_error'] = {'exit': proc.returncode, 'reason': str(error), 'output_tail': proc.stdout[-4000:]}
                    atomic_json(path, state)
                    raise RuntimeError('crossing observation incomplete: ' + str(error)) from error
                a['result'] = {'exit': proc.returncode, 'outcome': 'pass' if proc.returncode == 0 else 'native_failure',
                               'observation': observed, 'events': [
                                   {'transition': name, 'reached': True, 'fired': True}
                                   for name in ('one_member_staged', 'player_departure', 'resumed_travel')]}
                a['status'] = 'completed'
                atomic_json(path, state)
                count += 1
                continue
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
    init.add_argument('--scenario', choices=[SCENARIO, JOURNEY, CROSSING, CUSTODY, CUSTODY_AUTH, *BROOD_SCENARIOS], default=SCENARIO,
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
