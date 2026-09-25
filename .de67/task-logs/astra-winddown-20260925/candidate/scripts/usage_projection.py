"""Incremental own-response usage in the existing disposable work-context index."""
from __future__ import annotations

from datetime import datetime, timezone
import json
import math
from pathlib import Path
import sqlite3
import time

FIELDS = ('input_tokens', 'cached_input_tokens', 'cache_write_input_tokens',
          'output_tokens', 'reasoning_output_tokens', 'total_tokens')


def _schema(db: sqlite3.Connection) -> None:
    db.executescript('''
        CREATE TABLE IF NOT EXISTS usage_sources (
            id TEXT PRIMARY KEY, path TEXT, file_identity TEXT, offset INTEGER,
            mtime_ns INTEGER, size INTEGER, window_start TEXT, last_event TEXT,
            issues INTEGER DEFAULT 0);
        CREATE TABLE IF NOT EXISTS usage_turn_models (
            id TEXT, turn_id TEXT, model TEXT, PRIMARY KEY(id,turn_id));
        CREATE TABLE IF NOT EXISTS usage_responses (
            id TEXT, response_id TEXT, turn_id TEXT, observed_at TEXT, usage TEXT,
            PRIMARY KEY(id,response_id));
    ''')


def _usage(value: object) -> dict | None:
    if not isinstance(value, dict) or not {'input_tokens','cached_input_tokens','output_tokens','total_tokens'} <= value.keys():
        return None
    result = {key: value.get(key, 0) for key in FIELDS}
    if any(isinstance(v, bool) or not isinstance(v, int) or v < 0 for v in result.values()):
        return None
    if result['cached_input_tokens'] > result['input_tokens']:
        return None
    if result['reasoning_output_tokens'] > result['output_tokens']:
        return None
    if result['total_tokens'] != result['input_tokens'] + result['output_tokens']:
        return None
    return result


def _ingest(db: sqlite3.Connection, record: dict) -> dict:
    identity = record['id']
    path = Path(record['rollout_path'])
    old = db.execute('SELECT * FROM usage_sources WHERE id=?', (identity,)).fetchone()
    previous = dict(old) if old else None
    result = {'session_id': identity, 'source': str(path), 'bytes_read': 0, 'available': False}
    try:
        stat = path.stat()
        file_identity = f'{stat.st_dev}:{stat.st_ino}'
        reset = previous is not None and (previous['path'] != str(path)
            or previous['file_identity'] != file_identity or stat.st_size < previous['offset']
            or (stat.st_size == previous['offset'] and stat.st_mtime_ns != previous['mtime_ns']))
        if reset:
            for table in ('usage_sources', 'usage_turn_models', 'usage_responses'):
                db.execute('DELETE FROM '+table+' WHERE id=?', (identity,))
            previous = None
        offset = previous['offset'] if previous else 0
        window = previous['window_start'] if previous else None
        last = previous['last_event'] if previous else None
        issues = previous['issues'] if previous else 0
        with path.open('rb') as stream:
            stream.seek(offset)
            # Read the observed prefix; an incomplete last record is retried next time.
            while stream.tell() < stat.st_size:
                line = stream.readline(stat.st_size - stream.tell())
                if not line.endswith(b'\n'):
                    break
                offset = stream.tell()
                result['bytes_read'] += len(line)
                try:
                    event = json.loads(line)
                except (ValueError, UnicodeError):
                    issues += 1
                    continue
                if not isinstance(event, dict):
                    issues += 1
                    continue
                kind = event.get('type')
                payload = event.get('payload')
                if not isinstance(payload, dict):
                    if kind in {'token_usage_record','turn_context','session_meta'}: issues += 1
                    continue
                stamp = event.get('timestamp')
                if kind in {'token_usage_record','session_meta'} and not (isinstance(stamp,str) and stamp):
                    issues += 1
                    stamp = None
                if kind == 'session_meta' and payload.get('id') == identity:
                    window = window or (payload.get('timestamp') if isinstance(payload.get('timestamp'),str) else None) or stamp
                elif kind == 'turn_context':
                    if not all(isinstance(payload.get(k), str) and payload[k] for k in ('turn_id','model')):
                        issues += 1
                        continue
                    known = db.execute('SELECT model FROM usage_turn_models WHERE id=? AND turn_id=?', (identity,payload['turn_id'])).fetchone()
                    model = payload['model'] if known is None or known[0] == payload['model'] else None
                    if model is None: issues += 1
                    db.execute('INSERT OR REPLACE INTO usage_turn_models VALUES (?,?,?)',
                               (identity, payload['turn_id'], model))
                elif kind == 'token_usage_record':
                    # A parent/root rollup or inherited record is not own-response usage.
                    if payload.get('thread_id') != identity:
                        continue
                    value = _usage(payload.get('usage'))
                    response = payload.get('response_id')
                    if value is None or not all(isinstance(payload.get(k), str) and payload[k] for k in ('response_id','turn_id')):
                        issues += 1
                        continue
                    encoded = json.dumps(value, sort_keys=True)
                    stored = db.execute('SELECT usage,turn_id FROM usage_responses WHERE id=? AND response_id=?',
                                        (identity, response)).fetchone()
                    if stored and (stored['usage'] != encoded or stored['turn_id'] != payload['turn_id']):
                        issues += 1
                        continue
                    db.execute('INSERT OR IGNORE INTO usage_responses VALUES (?,?,?,?,?)',
                               (identity, response, payload['turn_id'], stamp, encoded))
                    window = window or stamp
                    last = stamp or last
        db.execute('INSERT OR REPLACE INTO usage_sources (id,path,file_identity,offset,mtime_ns,size,window_start,last_event,issues) VALUES (?,?,?,?,?,?,?,?,?)',
                   (identity, str(path), file_identity, offset, stat.st_mtime_ns,
                    stat.st_size, window, last, issues))
        result.update(available=True, reset=reset, source_offset=offset, source_bytes=stat.st_size,
                      source_mtime_ns=stat.st_mtime_ns, partial_record=offset < stat.st_size)
    except OSError as error:
        result['error'] = str(error)
    return result


def _timestamp(value: object) -> float | None:
    if not isinstance(value, str):
        return None
    try:
        stamp = datetime.fromisoformat(value.replace('Z', '+00:00'))
        # A local/naive timestamp cannot establish an assignment boundary.
        return stamp.timestamp() if stamp.utcoffset() is not None else None
    except (ValueError, OverflowError, OSError):
        return None


def _windows(value: object) -> tuple[list[dict], int]:
    """Validate half-open assignment windows without inventing missing bounds."""
    if not isinstance(value, list):
        return [], 1
    windows = []
    invalid = 0
    for window in value:
        if not isinstance(window, dict) or not {'start', 'end'} <= window.keys():
            invalid += 1
            continue
        start, end = window['start'], window['end']
        if (isinstance(start, bool) or not isinstance(start, (int, float)) or not math.isfinite(start)
                or (end is not None and (isinstance(end, bool) or not isinstance(end, (int, float))
                    or not math.isfinite(end) or end < start))):
            invalid += 1
            continue
        try:
            datetime.fromtimestamp(start, timezone.utc)
            if end is not None:
                datetime.fromtimestamp(end, timezone.utc)
        except (ValueError, OverflowError, OSError):
            invalid += 1
            continue
        windows.append({'start': start, 'end': end})
    return windows, invalid


def _select_rows(rows: list, windows: list[dict]) -> tuple[list, int, int]:
    selected = []
    excluded = unknown = 0
    for row in rows:
        stamp = _timestamp(row['observed_at'])
        if stamp is None:
            unknown += 1
        elif any(window['start'] <= stamp and (window['end'] is None or stamp < window['end'])
                 for window in windows):
            selected.append(row)
        else:
            excluded += 1
    return selected, excluded, unknown


def usage_projection(db: sqlite3.Connection, metadata: dict, *, root_id: str, details: bool=False) -> dict:
    """Count own responses once by turn model, optionally within assignment windows."""
    db.row_factory = sqlite3.Row
    _schema(db)
    sources = []
    models = {}
    complete = not (metadata.get('errors') or metadata.get('unavailable_thread_ids'))
    seen = set()
    with db:
        for record in metadata.get('records', []):
            identity = record['id']
            if identity in seen:
                continue
            seen.add(identity)
            source = _ingest(db, record)
            cached = db.execute('SELECT * FROM usage_sources WHERE id=?', (identity,)).fetchone()
            rows = list(db.execute('''SELECT r.usage,r.observed_at,m.model FROM usage_responses r
                LEFT JOIN usage_turn_models m ON r.id=m.id AND r.turn_id=m.turn_id WHERE r.id=?''', (identity,)))
            source['accounting'] = 'own_response_usage'
            source.update(window_started_at=cached['window_start'] if cached else None,
                          counters_observed_at=cached['last_event'] if cached else None,
                          invalid_or_conflicting_records=cached['issues'] if cached else 0,
                          response_count=len(rows))
            if 'usage_windows' in record:
                windows, invalid = _windows(record['usage_windows'])
                source['usage_windows'] = windows
                source['invalid_usage_windows'] = invalid
                source['source_response_count'] = len(rows)
                rows, excluded, unknown = _select_rows(rows, windows)
                started = (datetime.fromtimestamp(min(w['start'] for w in windows), timezone.utc)
                           .isoformat().replace('+00:00', 'Z')) if windows else None
                source.update(response_count=len(rows), excluded_response_count=excluded,
                              unknown_timestamp_response_count=unknown, window_started_at=started,
                              counters_observed_at=max((row['observed_at'] for row in rows),
                                                       key=_timestamp, default=None))
            source_complete = (bool(rows) and source['available'] and not source['invalid_or_conflicting_records']
                               and not source.get('partial_record') and not source.get('invalid_usage_windows')
                               and not source.get('unknown_timestamp_response_count'))
            for row in rows:
                model = row['model'] or 'unattributed'
                if model == 'unattributed':
                    source_complete = False
                aggregate = models.setdefault(model, {**{key:0 for key in FIELDS}, 'session_ids':set(),
                                                      'counters_observed_at':None, 'window_started_at':None})
                value = json.loads(row['usage'])
                for field in FIELDS:
                    aggregate[field] += value[field]
                aggregate['session_ids'].add(identity)
                if row['observed_at']:
                    aggregate['counters_observed_at'] = max(aggregate['counters_observed_at'] or row['observed_at'], row['observed_at'])
                if source['window_started_at']:
                    aggregate['window_started_at'] = min(aggregate['window_started_at'] or source['window_started_at'], source['window_started_at'])
            source['complete'] = source_complete
            complete = complete and source_complete
            sources.append(source)
    for aggregate in models.values():
        aggregate['session_count'] = len(aggregate.pop('session_ids'))
        aggregate['uncached_input_tokens'] = aggregate['input_tokens'] - aggregate['cached_input_tokens']
        aggregate['uncached_input_plus_output_tokens'] = aggregate['uncached_input_tokens'] + aggregate['output_tokens']
    result = {'root_session_id':root_id, 'observed_at':time.time(), 'by_model':models,
              'session_count':len(sources), 'complete':bool(sources) and complete,
              'unavailable_thread_ids':metadata.get('unavailable_thread_ids', []),
              'errors':metadata.get('errors', []),
              'new_source_bytes_read':sum(s['bytes_read'] for s in sources),
              'evidence_limit':'Selected coordinator tree, including helpers; each own response counted once by turn model. '
                               'Cumulative counters and descendant rollups excluded; sources without selected own-response records are gaps. '
                               'Assignment windows, when supplied, use response timestamps and half-open [start,end) bounds; unknown timestamps are gaps. '
                               'Otherwise windows cover session lifetimes, not a billing period. '
                               'Cached input is part of input; reasoning is part of output. No cost or quota is inferred.'}
    if details:
        result['sources'] = sources
    else:
        result['source_gaps'] = [s for s in sources if not s['complete']]
    return result
