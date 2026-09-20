#!/usr/bin/env python3
"""Read current session metadata at exact existing evidence references; never send input."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys
import time
from typing import Any, Mapping, Sequence


class CurrentResultsError(ValueError):
    """The coordinator's selected result set is not safely reusable."""


_CONTINUATION_FIELDS = (
    'session_generation', 'process_generation', 'binding',
    'pending_request_id', 'retained_frame_handles',
    'retained_evidence_handles', 'unresolved_question', 'next_decision',
)


def continuation_note(*, session_generation: int | None,
                      process_generation: int | None,
                      binding: str | None,
                      pending_request_id: str | None,
                      retained_frame_handles: Sequence[str] = (),
                      retained_evidence_handles: Sequence[str] = (),
                      unresolved_question: str = '',
                      next_decision: str = '') -> dict:
    """Build a replaceable current-work note without inferring state.

    Generations, binding and handles are caller-supplied exact values.  A
    missing pending request is represented explicitly by ``None``.
    """
    if session_generation is not None and (
            not isinstance(session_generation, int) or isinstance(session_generation, bool)):
        raise CurrentResultsError('session_generation must be an integer or null')
    if process_generation is not None and (
            not isinstance(process_generation, int) or isinstance(process_generation, bool)):
        raise CurrentResultsError('process_generation must be an integer or null')
    return {
        'session_generation': session_generation,
        'process_generation': process_generation,
        'binding': None if binding is None else str(binding),
        'pending_request_id': None if pending_request_id is None else str(pending_request_id),
        'retained_frame_handles': [str(item) for item in retained_frame_handles],
        'retained_evidence_handles': [str(item) for item in retained_evidence_handles],
        'unresolved_question': str(unresolved_question),
        'next_decision': str(next_decision),
    }


def _continuation_value(value: Mapping[str, Any] | None) -> dict | None:
    if value is None:
        return None
    if not isinstance(value, Mapping):
        raise CurrentResultsError('continuation_note must be an object')
    missing = [field for field in _CONTINUATION_FIELDS if field not in value]
    if missing:
        raise CurrentResultsError('continuation_note is missing: ' + ', '.join(missing))
    return continuation_note(
        session_generation=value['session_generation'],
        process_generation=value['process_generation'],
        binding=value['binding'],
        pending_request_id=value['pending_request_id'],
        retained_frame_handles=value['retained_frame_handles'],
        retained_evidence_handles=value['retained_evidence_handles'],
        unresolved_question=value['unresolved_question'],
        next_decision=value['next_decision'],
    )


def _receipt_id(receipt: Mapping[str, Any]) -> str:
    """Return an explicitly durable receipt identity; never invent one."""
    identity = receipt.get('receipt_id', receipt.get('id'))
    if identity is None and isinstance(receipt.get('identity'), Mapping):
        identity = receipt['identity'].get('receipt_id', receipt['identity'].get('id'))
    identity = str(identity or '').strip()
    if not identity:
        raise CurrentResultsError('receipt is missing an explicit durable receipt_id')
    return identity


def select_current_results(evidence: Mapping[str, Any], receipt_ids: Sequence[str],
                           conclusions: Sequence[str] = (),
                           boundaries: Sequence[str] = (),
                           dependencies: Mapping[str, str] | None = None,
                           continuation: Mapping[str, Any] | None = None) -> dict:
    """Select coordinator-named receipts and conclusions without judging them.

    Receipt objects are copied verbatim and retain their exact identity.  This
    function deliberately does not infer acceptance, proof, or a latest result.
    """
    receipts = evidence.get('receipts', ())
    by_id = {}
    for receipt in receipts:
        if not isinstance(receipt, Mapping):
            continue
        rid = _receipt_id(receipt)
        if rid in by_id:
            raise CurrentResultsError('duplicate durable receipt identity: ' + rid)
        by_id[rid] = dict(receipt)
    ids = [str(item) for item in receipt_ids]
    if len(set(ids)) != len(ids):
        raise CurrentResultsError('selected receipt identities must be distinct')
    missing = [rid for rid in ids if rid not in by_id]
    if missing:
        raise CurrentResultsError('selected receipt identity is not in the supplied evidence: ' + ', '.join(missing))
    deps = {str(Path(path).resolve()): str(digest) for path, digest in (dependencies or {}).items()}
    for path, expected in deps.items():
        source = Path(path)
        if not source.is_file():
            raise CurrentResultsError('source dependency is unavailable: ' + path)
        actual = hashlib.sha256(source.read_bytes()).hexdigest()
        if actual != expected:
            raise CurrentResultsError('stale source dependency: ' + path)
    selected = {
        'schema': 'caol-current-results-v1',
        'selected_receipts': [by_id[rid] for rid in ids],
        'current_conclusions': [str(item) for item in conclusions],
        'remaining_boundaries': [str(item) for item in boundaries],
        'source_dependencies': deps,
    }
    note = _continuation_value(continuation)
    if note is not None:
        selected['continuation_note'] = note
    return selected


def export_current_results(selection: Mapping[str, Any], destination: Path) -> dict:
    """Write the current coordinator selection and return its immutable digest.

    Destination is the replaceable current projection; context_library.put
    creates the immutable revision, so prior projections remain retrievable.
    """
    if selection.get('schema') != 'caol-current-results-v1':
        raise CurrentResultsError('unsupported current-results schema')
    receipts = selection.get('selected_receipts', [])
    if not isinstance(receipts, list):
        raise CurrentResultsError('selected_receipts must be a list')
    ids = [_receipt_id(item) for item in receipts if isinstance(item, Mapping)]
    if len(ids) != len(receipts) or len(ids) != len(set(ids)):
        raise CurrentResultsError('selected receipts must retain distinct explicit identities')
    note = _continuation_value(selection.get('continuation_note'))
    lines = ['# Current results', '', '## Current conclusions']
    lines += ['- ' + str(item) for item in selection.get('current_conclusions', [])]
    lines += ['', '## Remaining boundaries']
    lines += ['- ' + str(item) for item in selection.get('remaining_boundaries', [])]
    lines += ['', '## Selected durable receipts']
    for receipt in receipts:
        lines.append('### receipt ' + _receipt_id(receipt))
        lines.append('```json')
        lines.append(json.dumps(receipt, ensure_ascii=False, sort_keys=True, indent=2))
        lines.append('```')
    if note is not None:
        lines += ['', '## Current continuation note', '```json',
                  json.dumps(note, ensure_ascii=False, sort_keys=True, indent=2),
                  '```']
    lines += ['', '## Source dependencies', '```json',
              json.dumps(selection.get('source_dependencies', {}), ensure_ascii=False, sort_keys=True, indent=2),
              '```', '']
    raw = '\n'.join(lines).encode('utf-8')
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name('.' + destination.name + '.tmp')
    temporary.write_bytes(raw)
    temporary.replace(destination)
    return {'path': str(destination), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
            'receipt_ids': ids}


def prepare_current_results(workspace: Path, task: str, selection: Mapping[str, Any],
                            destination: Path, brief: str, handoff: str = '') -> dict:
    """Export then feed the saved projection through context_library put/prepare."""
    result = export_current_results(selection, destination)
    library = Path('/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/context_library.py')
    import importlib.util
    spec = importlib.util.spec_from_file_location('context_library', library)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    revision = module.put(workspace, task, 'current-results', destination,
                          references=tuple(result['receipt_ids']),
                          dependencies=tuple(selection.get('source_dependencies', {}).keys()))
    module.prepare(workspace, task, brief, ('current-results',), handoff)
    result['revision'] = revision
    return result


def session_context(workspace: Path, evidence: dict) -> dict:
    paths=set(evidence.get('entrypoints',[]))
    # A session explicitly named by the current route owns the status projection.
    # Receipt sessions are a fallback; historical siblings stay discoverable in receipts.
    direct_sessions=any('bridge-sessions' in Path(p).parts for p in paths if isinstance(p,str))
    for receipt in ([] if direct_sessions else evidence.get('receipts',[])):
        paths.update(receipt.get('bindings',{}).values())
        paths.update(a['path'] for a in receipt.get('artifact_refs',[]))
        paths.update(receipt.get('entrypoints',[]))
    sessions=set()
    for value in paths:
        if not isinstance(value,str): continue
        path=Path(value)
        if not path.is_absolute():path=workspace/path
        # Reference resolution is lexical: no archive walk or guessed newest run.
        parts=path.parts
        if 'bridge-sessions' in parts:
            pos=parts.index('bridge-sessions')
            if pos+1<len(parts):sessions.add(Path(*parts[:pos+2]))
    result=[]
    for session in sorted(sessions):
        try:session.resolve().relative_to(workspace.resolve())
        except ValueError:continue
        files={};status={}
        for name in ('status.json','bridge.manifest.json','active-request.json'):
            path=session/name
            try:
                raw=path.read_bytes();stat=path.stat();value=json.loads(raw)
                files[name]={'path':str(path),'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),
                             'mtime_ns':stat.st_mtime_ns,'available':True}
                if name=='status.json':status=value
                elif name=='active-request.json':status['pending_request']=value
            except FileNotFoundError:files[name]={'path':str(path),'available':False,'reason':'missing'}
            except (OSError,ValueError) as e:files[name]={'path':str(path),'available':False,'reason':str(e)}
        pending = status.get('pending_request')
        current_note = continuation_note(
            session_generation=status.get('session_generation'),
            process_generation=status.get('process_generation'),
            binding=status.get('binding_id'),
            pending_request_id=(pending.get('request_id')
                                if isinstance(pending, Mapping) else None),
            unresolved_question='', next_decision='')
        result.append({'session':str(session),'files':files,'recorded_status':status,
            'current_continuation': current_note,
            'refresh_argv':[sys.executable,'tools/openclaw_harness/play_cli.py','--session',str(session),'look'],
            'controls_argv':[sys.executable,'tools/openclaw_harness/play_cli.py','--session',str(session),'controls'],
            'log_query_argv':[sys.executable,'tools/openclaw_harness/cockpit_file_bridge.py','log-query',
                              '--session-dir',str(session)],
            'evidence_limit':'Recorded bridge state and pending request only. Process liveness and '
                             'current input ownership require a fresh look; missing state is unknown, not finished.'})
    return {'observed_at':time.time(),'sessions':result,
            'registry_history_argv':[sys.executable,'tools/openclaw_harness/scenario_registry_cli.py','registry-status'],
            'evidence_limit':'Only exact referenced sessions are inspected. No session selection, launch, or input occurred.'}

if __name__=='__main__':
    print(json.dumps(session_context(Path.cwd(),json.load(sys.stdin)),ensure_ascii=False))
