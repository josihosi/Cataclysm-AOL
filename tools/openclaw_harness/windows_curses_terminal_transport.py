"""Staged native Windows backend for the existing run-bound terminal interface.

The broker creates and retains ConPTY; Windows cannot pass a POSIX PTY slave to
Popen. Input/status/cleanup keep the existing API and identity vocabulary.
Named-pipe framing is bytes-only JSON, with stdlib HMAC authentication (no pickle).
"""
from __future__ import annotations

from dataclasses import asdict
import hashlib
import json
from multiprocessing.connection import Client, Listener
from multiprocessing import AuthenticationError
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid

from windows_native_process import (NativeConPTY, WindowsProcessInspector, cim_process,
    adopt_native_lease, create_semantic_wake_event, lease_kernel, replace_with_readers)

OWNER_SCHEMA = 'caol-curses-terminal-owner-v1'
REQUEST_SCHEMA = 'caol-curses-terminal-request-v1'


def _host_identity():
    return socket.gethostname().strip()


def _process_generation(pid):
    observed = WindowsProcessInspector().inspect(int(pid))
    return dict(schema='caol-owned-process-generation-v1', **asdict(observed))


def _same_generation(expected, observed):
    try:
        return bool(int(expected.get('pid', 0)) == int(observed.get('pid', 0)) > 0
            and expected.get('birth_identity') and expected.get('command')
            and expected.get('executable_path')
            and expected['birth_identity'] == observed.get('birth_identity')
            and expected['command'] == observed.get('command')
            and os.path.normcase(expected['executable_path']) == os.path.normcase(observed.get('executable_path', '')))
    except (TypeError, ValueError, AttributeError):
        return False


def _key_bytes(keys):
    named = {'return': b'\r', 'enter': b'\r', 'tab': b'\t', 'escape': b'\x1b',
        'space': b' ', 'F1': b'\x1bOP', 'up': b'\x1bOA', 'down': b'\x1bOB',
        'right': b'\x1bOC', 'left': b'\x1bOD'}
    if not isinstance(keys, list) or not all(isinstance(key, str) for key in keys):
        raise ValueError('keys must be a list of strings')
    output = bytearray()
    for key in keys:
        if key in named:
            output.extend(named[key])
        elif len(key) == 1 and key.isprintable():
            output.extend(key.encode('utf-8'))
        else:
            raise ValueError('unsupported terminal-native key: ' + repr(key))
    return bytes(output)


def _fingerprint(keys, delay_ms):
    return hashlib.sha256(json.dumps({'keys': keys, 'delay_ms': delay_ms},
        sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def _write_json_atomic(path, value):
    path = Path(path)
    temp = path.with_name(path.name + '.tmp-' + uuid.uuid4().hex)
    try:
        with temp.open('x', encoding='utf-8') as sink:
            json.dump(value, sink, sort_keys=True)
            sink.flush()
            os.fsync(sink.fileno())
        replace_with_readers(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def _read_owner(endpoint, owner_path=None):
    path = Path(owner_path) if owner_path else Path(endpoint).parent / 'owner.json'
    try:
        owner = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        raise RuntimeError('terminal dispatcher owner record is unavailable') from error
    if not isinstance(owner, dict) or owner.get('schema') != OWNER_SCHEMA:
        raise RuntimeError('terminal dispatcher owner record is invalid')
    return owner


def _public_owner(owner):
    return {k: v for k, v in owner.items() if k not in {'auth_key', 'native_lease'}}


class DeliveryJournal:
    """One per-run transport journal, not a registry or semantic action ledger.

    In-flight is fsynced before any input write. A partial, interrupted or
    unrecorded completion never earns replay permission, even after reconnect.
    """
    def __init__(self, path):
        self.path = Path(path)
        self.records = {}
        if self.path.exists():
            for line in self.path.read_text(encoding='utf-8').splitlines():
                row = json.loads(line)
                if not isinstance(row, dict) or not row.get('request_id'):
                    raise ValueError('terminal request journal is ambiguous')
                self.records[row['request_id']] = row

    def append(self, row):
        with self.path.open('a', encoding='utf-8') as sink:
            sink.write(json.dumps(row, sort_keys=True) + '\n')
            sink.flush()
            os.fsync(sink.fileno())
        self.records[row['request_id']] = row

    def previous(self, request_id, fingerprint):
        row = self.records.get(request_id)
        if row is None:
            return None
        if row['fingerprint'] != fingerprint:
            return {'ok': False, 'request_id': request_id, 'error': 'duplicate_request_payload_mismatch'}
        if row.get('state') != 'delivered':
            return {'ok': False, 'request_id': request_id, 'error': 'request_delivery_ambiguous',
                'delivery_state': row.get('state', 'unknown'), 'replay_permitted': False}
        return dict(row['receipt'], duplicate_request=True, receipt_source='persisted_transport_journal')


def _request(owner, run_id, pid, keys, delay_ms, host, process_generation, request_id):
    if owner.get('endpoint') is None or owner.get('transport') != 'windows_conpty':
        raise RuntimeError('terminal owner transport mismatch')
    if owner.get('host') != host or owner.get('run_id') != run_id or owner.get('game_pid') != pid:
        raise RuntimeError('terminal dispatcher rejected host, run, or PID identity')
    expected = owner.get('game_process_generation', {})
    if process_generation is not None and not _same_generation(process_generation, expected):
        raise RuntimeError('terminal dispatcher rejected expected process generation')
    _key_bytes(keys)
    if not isinstance(delay_ms, int) or delay_ms < 0:
        raise ValueError('delay_ms must be a nonnegative integer')
    identity = request_id if request_id is not None else uuid.uuid4().hex
    if not isinstance(identity, str) or not identity.strip():
        raise ValueError('terminal request ID must not be empty')
    return {'schema': REQUEST_SCHEMA, 'request_id': identity, 'host': host,
        'run_id': run_id, 'pid': pid, 'process_generation': expected,
        'keys': keys, 'delay_ms': delay_ms}


def _exchange(owner, request, *, disconnect=False):
    with Client(owner['pipe_name'], family='AF_PIPE', authkey=bytes.fromhex(owner['auth_key'])) as conn:
        conn.send_bytes(json.dumps(request, sort_keys=True).encode())
        if disconnect:
            return {'request_id': request['request_id'], 'response_collected': False}
        timeout = 2.0 + len(request['keys']) * request['delay_ms'] / 1000
        if not conn.poll(timeout):
            raise RuntimeError('terminal receipt unavailable; reconnect with the SAME request ID, never replay a new ID')
        return json.loads(conn.recv_bytes(65536))


def dispatch_input(endpoint, *, run_id, pid, keys, delay_ms=0, host=None,
                   process_generation=None, request_id=None):
    endpoint = Path(endpoint)
    owner = _read_owner(endpoint)
    if owner.get('endpoint') != str(endpoint):
        raise RuntimeError('terminal endpoint identity mismatch')
    actual_host = _host_identity() if host is None else str(host)
    request = _request(owner, run_id, pid, keys, delay_ms, actual_host, process_generation, request_id)
    # Lost receipts remain retrievable after native exit, until explicit cleanup.
    previous = DeliveryJournal(owner['request_journal']).previous(request['request_id'],
        _fingerprint(keys, delay_ms))
    if previous:
        record = DeliveryJournal(owner['request_journal']).records[request['request_id']]
        if record.get('run_id') != run_id or not _same_generation(record.get('process_generation', {}), owner['game_process_generation']):
            raise RuntimeError('terminal request journal identity mismatch')
    if previous and previous.get('ok'):
        return previous
    if previous and previous.get('error') != 'request_delivery_ambiguous':
        raise RuntimeError('terminal input dispatcher rejected request: ' + json.dumps(previous))
    if previous and previous.get('delivery_state') == 'ambiguous':
        raise RuntimeError('terminal input dispatcher rejected request: ' + json.dumps(previous))
    # An in-flight live broker may finish and provide the committed receipt;
    # connecting with the same ID never writes again. A dead broker stays ambiguous.
    game = _process_generation(pid)
    broker = _process_generation(owner.get('broker_pid', 0))
    if not broker.get('alive') or not _same_generation(owner.get('broker_process_generation', {}), broker):
        raise RuntimeError('terminal broker stale or unavailable; request ownership retained')
    if previous is None and (not game.get('alive') or not _same_generation(owner['game_process_generation'], game)):
        raise RuntimeError('terminal game process generation is stale or unavailable')
    receipt = _exchange(owner, request)
    if receipt.get('request_id') != request['request_id'] or not receipt.get('ok'):
        raise RuntimeError('terminal input dispatcher rejected request: ' + json.dumps(receipt))
    return receipt


def dispatcher_status(endpoint, *, owner_path=None):
    endpoint = Path(endpoint)
    owner = _read_owner(endpoint, owner_path)
    if owner.get('endpoint') != str(endpoint):
        return {'schema': OWNER_SCHEMA, 'status': 'endpoint_identity_mismatch', 'endpoint': str(endpoint)}
    if owner.get('host') != _host_identity():
        return {'schema': OWNER_SCHEMA, 'status': 'wrong_host', 'endpoint': str(endpoint)}
    game = _process_generation(owner.get('game_pid', 0))
    broker = _process_generation(owner.get('broker_pid', 0))
    return {'schema': OWNER_SCHEMA, 'host': owner['host'], 'run_id': owner['run_id'],
        'endpoint': str(endpoint), 'endpoint_present': endpoint.exists(),
        'game_status': 'alive' if game.get('alive') and _same_generation(owner.get('game_process_generation', {}), game)
            else 'identity_changed_or_exited',
        'broker_status': 'alive' if broker.get('alive') and _same_generation(owner.get('broker_process_generation', {}), broker)
            else 'identity_changed_or_exited', 'owner': _public_owner(owner)}


def cleanup_dispatcher(endpoint, *, timeout=2.0, owner_path=None):
    endpoint = Path(endpoint)
    try:
        owner = _read_owner(endpoint, owner_path)
    except RuntimeError:
        return {'status': 'endpoint_absent_owner_unavailable', 'endpoint': str(endpoint)}
    if owner.get('host') != _host_identity():
        return {'status': 'retained_wrong_host', 'endpoint': str(endpoint)}
    if owner.get('endpoint') != str(endpoint):
        return {'status': 'retained_endpoint_identity_mismatch', 'endpoint': str(endpoint)}
    deadline = time.monotonic() + max(0, timeout)
    while True:
        for role, expected in [('game', owner.get('game_process_generation', {})),
                ('broker', owner.get('broker_process_generation', {})),
                *[('conhost', row) for row in owner.get('conhost_process_generations', [])]]:
            current = _process_generation(expected.get('pid', 0))
            if current.get('alive'):
                if not _same_generation(expected, current):
                    return {'status': 'retained_ambiguous_' + role + '_identity',
                        'endpoint': str(endpoint), 'pid': current['pid']}
                if role == 'game':
                    return {'status': 'retained_live_game', 'endpoint': str(endpoint), 'pid': current['pid']}
                if time.monotonic() >= deadline:
                    return {'status': role + '_exit_unobserved', 'endpoint': str(endpoint), 'pid': current['pid']}
                break
        else:
            return _cleanup_private(endpoint, owner)
        time.sleep(.05)


def _cleanup_private(endpoint, owner):
    private_dir = endpoint.parent
    if not private_dir.exists():
        return {'status': 'cleaned_after_game_exit', 'endpoint_present': False, 'private_directory': str(private_dir)}
    current = _read_owner(endpoint)
    if (current.get('instance_id') != owner.get('instance_id')
            or not _same_generation(current.get('broker_process_generation', {}), owner.get('broker_process_generation', {}))
            or current.get('endpoint') != str(endpoint)):
        return {'status': 'retained_private_owner_changed', 'endpoint': str(endpoint)}
    # Never delete unknown residue, another endpoint or the run's evidence.
    if endpoint.name != 'input.pipe' or not private_dir.name.startswith('caol-conpty-'):
        return {'status': 'retained_private_path_mismatch', 'endpoint': str(endpoint)}
    marker = json.loads(endpoint.read_text()) if endpoint.exists() else {}
    if marker and marker.get('instance_id') != owner.get('instance_id'):
        return {'status': 'retained_endpoint_identity_mismatch', 'endpoint': str(endpoint)}
    unknown = [p for p in private_dir.iterdir() if p.name not in {'input.pipe', 'launch.json', 'owner.json'}]
    if unknown:
        return {'status': 'private_directory_retained_after_game_exit', 'endpoint_present': endpoint.exists(),
            'private_directory': str(private_dir), 'retained_paths': sorted(str(p) for p in private_dir.iterdir())}
    for path in (endpoint, private_dir / 'launch.json'):
        path.unlink(missing_ok=True)
    (private_dir / 'owner.json').unlink()
    try:
        private_dir.rmdir()
    except OSError:
        return {'status': 'private_directory_retained_after_game_exit', 'endpoint_present': endpoint.exists(),
            'private_directory': str(private_dir), 'retained_paths': sorted(str(p) for p in private_dir.iterdir())}
    return {'status': 'cleaned_after_game_exit', 'endpoint_present': False, 'private_directory': str(private_dir)}


class LaunchOwnershipError(RuntimeError):
    def __init__(self, message, ownership):
        super().__init__(message)
        self.ownership = ownership


class WindowsCursesTerminalTransport:
    """Factory boundary: detached broker owns ConPTY before creating the child."""
    @classmethod
    def launch(cls, transcript_path, *, run_id, argv, cwd, env=None, private_root=None,
               lease_fds=(), lease_fd=None, native_lease=None):
        if os.name != 'nt':
            raise OSError('Windows ConPTY launch requires Windows')
        if lease_fds or lease_fd is not None:
            raise OSError('POSIX leases cannot be ignored or inherited by this Windows candidate; native writable-root integration required')
        if native_lease is None:
            raise OSError('native Windows terminal launch requires its writable-root lease export')
        if not run_id or not argv or not Path(argv[0]).is_absolute():
            raise ValueError('nonempty run ID and explicit executable argv required')
        transcript_path = Path(transcript_path).absolute()
        run_dir = transcript_path.parent
        if run_dir.resolve() != run_dir or Path(cwd).resolve() != Path(cwd).absolute():
            raise ValueError('terminal launch root aliases are unavailable')
        run_dir.mkdir(parents=True, exist_ok=True)
        run_owner = run_dir / 'terminal.owner.json'
        if transcript_path.exists() or (run_dir / 'terminal.requests.jsonl').exists():
            raise RuntimeError('terminal artifacts already exist; ownership reconciliation required')
        identity = uuid.uuid4().hex
        private_dir = Path(tempfile.mkdtemp(prefix='caol-conpty-', dir=private_root)).absolute()
        endpoint = private_dir / 'input.pipe'
        config = {'schema': OWNER_SCHEMA, 'instance_id': identity, 'host': _host_identity(),
            'run_id': run_id, 'endpoint': str(endpoint), 'run_owner_path': str(run_owner),
            'transcript': str(transcript_path), 'request_journal': str(run_dir / 'terminal.requests.jsonl'),
            'argv': list(argv), 'cwd': str(Path(cwd).absolute()), 'transport': 'windows_conpty',
            'pipe_name': '\\\\.\\pipe\\caol-conpty-' + identity, 'auth_key': secrets.token_hex(32),
            'launch_state': 'reserved', 'native_lease': native_lease,
            'wake_event_name': (env if env is not None else os.environ).get('OPENCLAW_HARNESS_SEMANTIC_WAKE_EVENT', '')}
        try:
            with run_owner.open('x', encoding='utf-8') as sink:
                json.dump(_public_owner(config), sink)
                sink.flush(); os.fsync(sink.fileno())
        except BaseException:
            private_dir.rmdir()
            raise
        _write_json_atomic(private_dir / 'launch.json', config)
        command = [sys.executable, str(Path(__file__).resolve()), '--broker', str(private_dir / 'launch.json')]
        try:
            with (run_dir / 'terminal.broker.stderr.log').open('xb') as stderr:
                broker = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                    stderr=stderr, env=env, close_fds=True,
                    creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_BREAKAWAY_FROM_JOB)
        except BaseException:
            # No child/broker was created: only our exclusive reservation may go.
            if _read_owner(endpoint, run_owner).get('instance_id') == identity:
                run_owner.unlink()
            (private_dir / 'launch.json').unlink(missing_ok=True)
            private_dir.rmdir()
            raise
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            try:
                owner = _read_owner(endpoint)
                if owner.get('launch_state') == 'ready' and endpoint.exists():
                    return owner['game_process_generation'], endpoint
                if owner.get('launch_state') == 'failed':
                    raise LaunchOwnershipError('Windows terminal startup failed', _public_owner(owner))
            except RuntimeError as error:
                if isinstance(error, LaunchOwnershipError):
                    raise
            if broker.poll() is not None:
                break
            time.sleep(.05)
        raise LaunchOwnershipError('Windows terminal startup unestablished; inspect and retain ownership',
            {'broker_pid': broker.pid, 'endpoint': str(endpoint), 'run_owner_path': str(run_owner), 'instance_id': identity})


def _conhosts(broker_pid):
    # Retrieve only actual headless conhost children of this exact broker.
    import base64
    script = (f"$p=@(Get-CimInstance Win32_Process -Filter 'ParentProcessId={int(broker_pid)}'|"
        "Where-Object {$_.Name -eq 'conhost.exe'}|ForEach-Object {[int]$_.ProcessId});"
        "ConvertTo-Json -InputObject $p -Compress")
    result = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-EncodedCommand',
        base64.b64encode(script.encode('utf-16le')).decode()], capture_output=True, text=True, timeout=10)
    if result.returncode:
        raise OSError('ConPTY host attribution unavailable')
    return [_process_generation(pid) for pid in json.loads(result.stdout)]


def _broker(config_path):
    config_path = Path(config_path)
    owner = json.loads(config_path.read_text())
    endpoint = Path(owner['endpoint'])
    private_owner = endpoint.parent / 'owner.json'
    run_owner = Path(owner['run_owner_path'])
    native = None
    listener = None
    held_native_leases = []
    wake_handle = None
    input_lock = threading.Lock()
    inspector = WindowsProcessInspector()
    def publish():
        # Detect replacement of the original run reservation before publication.
        if _read_owner(endpoint, run_owner).get('instance_id') != owner['instance_id']:
            raise RuntimeError('run owner changed; exact native ownership retained')
        _write_json_atomic(private_owner, owner)
        _write_json_atomic(run_owner, _public_owner(owner))
    try:
        owner['broker_pid'] = os.getpid()
        owner['broker_process_generation'] = dict(schema='caol-owned-process-generation-v1', **asdict(inspector.inspect(os.getpid())))
        if not _same_generation(owner['broker_process_generation'], owner['broker_process_generation']):
            raise RuntimeError('broker identity incomplete')
        held_native_leases = adopt_native_lease(owner['native_lease'], run_id=owner['run_id'])
        owner['native_lease_handle_count'] = len(held_native_leases)
        if owner.get('wake_event_name'):
            wake_handle = create_semantic_wake_event(owner['wake_event_name'])
        # Bind IPC and journal before child creation; no post-create surprise
        # should relinquish a live HPCON owner.
        listener = Listener(owner['pipe_name'], family='AF_PIPE', authkey=bytes.fromhex(owner['auth_key']))
        journal = DeliveryJournal(owner['request_journal'])
        native = NativeConPTY(Path(owner['transcript']))
        owner['conhost_process_generations'] = _conhosts(os.getpid())
        if not owner['conhost_process_generations'] or not all(_same_generation(row, row) for row in owner['conhost_process_generations']):
            raise RuntimeError('ConPTY host identity incomplete')
        game = native.launch(owner['argv'], Path(owner['cwd']))
        owner.update(game_pid=game['pid'], game_process_generation=game, launch_state='ready')
        publish()
        _write_json_atomic(endpoint, {'instance_id': owner['instance_id'], 'transport': 'windows_conpty'})

        def deliver(request):
            if not isinstance(request, dict):
                return {'ok': False, 'request_id': '', 'error': 'request_must_be_an_object'}
            request_id = str(request.get('request_id', ''))
            try:
                if request.get('schema') != REQUEST_SCHEMA:
                    raise ValueError('request_schema_mismatch')
                if request.get('host') != owner['host'] or request.get('run_id') != owner['run_id'] or request.get('pid') != game['pid']:
                    raise ValueError('host_run_or_pid_mismatch')
                if not _same_generation(request.get('process_generation', {}), game):
                    raise ValueError('process_generation_mismatch')
                keys = request.get('keys')
                delay = request.get('delay_ms', 0)
                _key_bytes(keys)
                if not request_id.strip() or not isinstance(delay, int) or delay < 0:
                    raise ValueError('invalid_request_id_or_delay')
                fingerprint = _fingerprint(keys, delay)
                with input_lock:
                    previous = journal.previous(request_id, fingerprint)
                    if previous:
                        return previous
                    current_owner = _read_owner(endpoint)
                    current_run_owner = _read_owner(endpoint, run_owner)
                    if any(row.get('instance_id') != owner['instance_id'] or not _same_generation(row.get('game_process_generation', {}), game)
                            for row in (current_owner, current_run_owner)):
                        raise ValueError('persisted_owner_identity_changed')
                    if not native.alive():
                        raise ValueError('game_process_generation_stale')
                    pending = {'request_id': request_id, 'fingerprint': fingerprint,
                        'state': 'in_flight', 'process_generation': game, 'run_id': owner['run_id']}
                    journal.append(pending)
                    try:
                        if delay and len(keys) > 1:
                            for index, key in enumerate(keys):
                                if index:
                                    time.sleep(delay / 1000)
                                native.write(_key_bytes([key]))
                        else:
                            native.write(_key_bytes(keys))
                        receipt = {'ok': True, 'request_id': request_id, 'host': owner['host'],
                            'run_id': owner['run_id'], 'pid': game['pid'], 'process_generation': game,
                            'keys': keys, 'payload_sha256': hashlib.sha256(_key_bytes(keys)).hexdigest(),
                            'owner': 'run_bound_conpty'}
                        journal.append(dict(pending, state='delivered', receipt=receipt))
                        return receipt
                    except Exception as error:
                        # Includes failure to persist completion after a write.
                        try:
                            journal.append(dict(pending, state='ambiguous', error=type(error).__name__))
                        except OSError:
                            pass  # The fsynced in-flight row still forbids replay.
                        return {'ok': False, 'request_id': request_id, 'error': 'request_delivery_ambiguous', 'replay_permitted': False}
            except Exception as error:
                return {'ok': False, 'request_id': request_id, 'error': str(error)}

        def serve():
            while native.alive():
                try:
                    conn = listener.accept()
                except (OSError, EOFError, ValueError, AuthenticationError):
                    continue
                with conn:
                    try:
                        if not conn.poll(2):
                            continue
                        request = json.loads(conn.recv_bytes(65536))
                        receipt = deliver(request)
                        conn.send_bytes(json.dumps(receipt, sort_keys=True).encode())
                    except (OSError, EOFError, ValueError):
                        # Lost-response callers collect the persisted SAME-ID
                        # receipt. A connection loss is never an idle declaration.
                        continue
        service = threading.Thread(target=serve, name='caol-conpty-input', daemon=True)
        service.start()
        while native.alive():
            time.sleep(.05)
        # Complete any interrupted input journal while output keeps draining.
        with input_lock:
            owner.update(launch_state='exited', child_exit_code=native.exit_code())
            # The broker survives launcher/SSH return and therefore owns the
            # existing bridge closeout receipt as well as the native handles.
            from startup_harness import record_bridge_game_exit, append_semantic_wake_observation
            from types import SimpleNamespace
            record_bridge_game_exit(SimpleNamespace(pid=owner['game_pid']), os.environ, owner['child_exit_code'])
            append_semantic_wake_observation(Path(owner['transcript']).parent, {
                'schema': 'caol-semantic-wake-observation-v1', 'event': 'child_exit',
                'run_id': owner['run_id'], 'transport': 'windows_event',
                'pid': owner['game_pid'], 'returncode': owner['child_exit_code']})
            native.close_after_exit()
            owner['native_handles_closed'] = True
            for handle in held_native_leases:
                lease_kernel().CloseHandle(handle)
            held_native_leases.clear()
            if wake_handle:
                lease_kernel().CloseHandle(wake_handle)
                wake_handle = None
            owner['native_lease_handles_closed'] = True
            publish()
        listener.close()
        return 0
    except BaseException as error:
        owner.update(launch_state='failed', error=type(error).__name__ + ': ' + str(error))
        if native and native.pi.process:
            # Do not unwind a live HPCON. Preserve its exact native ownership
            # and allow a human/coordinator to reconcile the recorded blocker.
            owner.setdefault('game_pid', int(native.pi.pid))
            owner.setdefault('game_process_generation', {
                'schema': 'caol-owned-process-generation-v1', **asdict(inspector.inspect(int(native.pi.pid)))})
            owner['cleanup_blocker'] = 'live_or_unobserved_child_after_broker_error'
        try:
            publish()
        except Exception:
            _write_json_atomic(private_owner, owner)
        while native and native.alive():
            time.sleep(.1)
        if native:
            native.close_after_exit()
        if listener:
            listener.close()
        for handle in held_native_leases:
            lease_kernel().CloseHandle(handle)
        if wake_handle:
            lease_kernel().CloseHandle(wake_handle)
        return 1


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--broker', required=True)
    args = parser.parse_args()
    raise SystemExit(_broker(args.broker))
