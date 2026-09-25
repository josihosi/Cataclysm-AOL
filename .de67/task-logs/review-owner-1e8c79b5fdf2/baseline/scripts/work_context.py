#!/usr/bin/env python3
"""Rebuildable, task-keyed discovery index. Source receipts remain authoritative."""
from __future__ import annotations
from contextlib import closing
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
from worker_receipt import compact_worker_receipt, receipt_envelope


class ContextError(RuntimeError):
    pass


def connect_index(workspace: Path) -> sqlite3.Connection:
    path = workspace / '.de67/state/work-context.sqlite3'
    path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(path, timeout=30)
    db.row_factory = sqlite3.Row
    db.executescript('''
        CREATE TABLE IF NOT EXISTS receipts (
          source TEXT, lineage TEXT, task TEXT, sequence INTEGER, claim TEXT,
          receipt_id TEXT, recorded_at REAL, payload TEXT, search_text TEXT,
          PRIMARY KEY(source,lineage,task,sequence));
        CREATE INDEX IF NOT EXISTS receipts_claim ON receipts(source,lineage,claim,task);
        CREATE INDEX IF NOT EXISTS receipts_identity ON receipts(receipt_id);
        CREATE TABLE IF NOT EXISTS dispatches (
          source TEXT, lineage TEXT, task TEXT, digest TEXT, path TEXT,
          context_json TEXT, recorded_at REAL,
          PRIMARY KEY(source,lineage,task,digest));
        CREATE TABLE IF NOT EXISTS runs (
          path TEXT PRIMARY KEY, role TEXT, run_id TEXT, lineage TEXT,
          session_id TEXT, metadata TEXT, recorded_at REAL);
    ''')
    return db


def sync_receipts(index: sqlite3.Connection, source: sqlite3.Connection,
                  state: Path, lineage: str) -> tuple[int, set[tuple[str, int]]]:
    """Inspect identities first; decode only contributions absent from the index."""
    source_key = str(state.resolve())
    known = {(r['task'], r['sequence']) for r in index.execute(
        'SELECT task,sequence FROM receipts WHERE source=? AND lineage=?', (source_key,lineage))}
    rows = source.execute('''SELECT c.task_id,c.sequence,c.recorded_at,t.claim_id
        FROM worker_checkpoints c JOIN tasks t
        ON t.lineage_id=c.lineage_id AND t.task_id=c.task_id
        WHERE c.lineage_id=? AND c.kind='result-receipt-v1'
        ORDER BY c.recorded_at,c.task_id,c.sequence''', (lineage,)).fetchall()
    added = 0
    with index:
        for row in rows:
            if (row['task_id'],row['sequence']) in known:
                continue
            raw = source.execute('''SELECT evidence FROM worker_checkpoints
                WHERE lineage_id=? AND task_id=? AND sequence=?''',
                (lineage,row['task_id'],row['sequence'])).fetchone()[0]
            envelope = json.loads(raw)
            receipt = envelope['receipt']
            identity = receipt_envelope(receipt)['receipt_id']
            if identity != envelope['receipt_id'] or any(receipt.get(k) != v for k,v in
                [('lineage_id',lineage),('task_id',row['task_id']),('claim_id',row['claim_id'])]):
                raise ContextError('Source receipt identity mismatch')
            index.execute('INSERT OR IGNORE INTO receipts VALUES (?,?,?,?,?,?,?,?,?)',
                (source_key,lineage,row['task_id'],row['sequence'],row['claim_id'],identity,
                 row['recorded_at'],json.dumps(receipt,ensure_ascii=False),
                 json.dumps(receipt,ensure_ascii=False).casefold()))
            added += 1
    return added, {(r['task_id'],r['sequence']) for r in rows}


def _record_run(workspace: Path, path: Path, environment: dict, *, session_id=None) -> None:
    """Runner-owned metadata, without loading or copying prompts and traces."""
    db = connect_index(workspace)
    try:
        files = {}
        for name in ('prompt.txt','events.jsonl','status.json'):
            p = path/name
            files[name] = {'path':str(p), 'available':p.is_file()}
            if p.is_file():
                s=p.stat(); files[name].update(bytes=s.st_size,mtime_ns=s.st_mtime_ns)
        with db:
            db.execute('''INSERT INTO runs VALUES (?,?,?,?,?,?,?)
                ON CONFLICT(path) DO UPDATE SET session_id=COALESCE(excluded.session_id,runs.session_id),
                metadata=excluded.metadata,recorded_at=excluded.recorded_at''',
                (str(path),environment.get('DE67_PROCESS_ROLE'),
                 environment.get('DE67_COORDINATOR_RUN_ID'),environment.get('DE67_LINEAGE'),
                 session_id,json.dumps(files),time.time()))
    finally:
        db.close()


def record_run(workspace: Path, path: Path, environment: dict, *, session_id=None) -> None:
    try:
        _record_run(workspace,path,environment,session_id=session_id)
    except (sqlite3.Error,OSError) as error:
        print(f'DE67 context index unavailable for {path}: {error}; full run artifacts remain at that path',
              file=sys.stderr,flush=True)


def record_dispatch(workspace: Path, state: Path, lineage: str, task: str,
                    path: Path, digest: str, context: dict) -> None:
    db=connect_index(workspace)
    try:
        with db:
            db.execute('''INSERT INTO dispatches VALUES (?,?,?,?,?,?,?)
                ON CONFLICT(source,lineage,task,digest) DO UPDATE SET
                path=excluded.path,context_json=excluded.context_json,recorded_at=excluded.recorded_at''',
                (str(state.resolve()),lineage,task,digest,str(path),
                 json.dumps(context,sort_keys=True),time.time()))
    finally:
        db.close()


def _related(receipt: dict, text: str) -> list[str]:
    # Match complete identities; generation numbers and PID fragments are not links.
    candidates = [receipt['task_id'],receipt['receipt_id']]
    candidates += [value for key,value in receipt.get('bindings',{}).items() if key in
                   {'run_id','scenario_id','binding_id','bridge_binding_id','session',
                    'session_dir','session_path','world_id','manifest_id'}]
    candidates += [a['path'] for a in receipt.get('artifact_refs',[])]
    return sorted({v for v in candidates if isinstance(v,str) and v and re.search(
        r'(?<![A-Za-z0-9_-])'+re.escape(v)+r'(?![A-Za-z0-9_-])',text)})


def dispatch_evidence_index(context: dict, *, detailed: bool = False) -> list[dict]:
    """Keep independent proof ceilings visible; history never becomes current instruction."""
    catalogue = {item["receipt_id"]: item for item in context["related_results"]}
    cli = context["full_history_argv"][:-1]
    result = []
    for item in context["selected_context"]:
        receipt = item["receipt"]
        entry = {key: receipt.get(key) for key in
                 ("receipt_id", "task_id", "summary", "evidence_ceiling")}
        entry.update(relationship=item["relationship"],
                     source_bytes=catalogue[receipt["receipt_id"]]["source_bytes"],
                     original_scope={"accepted_no_replay": receipt.get("accepted_no_replay", []),
                                     "authority": "Historical evidence; apply only where still relevant to the current assignment."})
        if detailed:
            entry.update({key: receipt.get(key) for key in
                          ("worker_id", "recorded_at", "disposition", "verdict", "first_divergence",
                           "first_open_boundary")})
            entry["retrieve_argv"] = [*cli, "--receipt", receipt["receipt_id"], "--full"]
        result.append(entry)
    return result


def context_view(workspace: Path, state: Path, lineage: str, *, claim: str | None=None,
                 task: str | None=None, contains: str | None=None,
                 receipt_id: str | None=None, route: str='', full: bool=False, rebuild: bool=False) -> dict:
    source=sqlite3.connect(f'file:{state.resolve()}?mode=ro',uri=True)
    source.row_factory=sqlite3.Row
    db=connect_index(workspace)
    try:
        # One source snapshot includes task lifecycle and checkpoints; a concurrent
        # commit becomes visible on the next query, never as a mixed generation.
        if rebuild:
            with db:db.execute('DELETE FROM receipts WHERE source=? AND lineage=?',
                               (str(state.resolve()),lineage))
        source.execute('BEGIN')
        if task and claim is None:
            bound=source.execute('SELECT claim_id FROM tasks WHERE lineage_id=? AND task_id=?',
                                 (lineage,task)).fetchone()
            if bound is None:raise ContextError(f'Unknown task: {task}')
            claim=bound[0]
        added,visible_receipts=sync_receipts(db,source,state,lineage)
        from worker_library import catalog as worker_catalog
        workers = worker_catalog(workspace, state=state, lineage=lineage)
        if claim is None and task is None and contains is None and receipt_id is None and not full:
            claims=[dict(r) for r in source.execute('''SELECT t.claim_id,COUNT(DISTINCT t.task_id) AS tasks,
                COUNT(DISTINCT CASE WHEN t.attempt_terminal_at IS NULL THEN t.task_id END) AS live_tasks,
                MAX(t.started_at) AS latest_started_at FROM tasks t WHERE t.lineage_id=?
                GROUP BY t.claim_id ORDER BY latest_started_at DESC''',(lineage,))]
            return {'schema':'de67.work-context.v1','observed_at':time.time(),'lineage':lineage,
                    'claims':claims,'worker_library':workers,'current_ledger':str(workspace/'.de67/work-ledger.md'),
                    'query_argv':[sys.executable,str(Path(__file__).resolve()),'--workspace',str(workspace),
                                  '--state',str(state),'--lineage',lineage,'--claim','CLAIM_ID'],
                    'evidence_limit':'Claim inventory only; inspect --claim, --task, --receipt or --contains. '
                                     '--full retrieves all receipt history without truncation.'}
        if receipt_id is not None and claim is None:
            bound=db.execute('SELECT claim FROM receipts WHERE source=? AND lineage=? AND receipt_id=?',
                             (str(state.resolve()),lineage,receipt_id)).fetchone()
            if bound is not None:claim=bound[0]
        tasks=[dict(r) for r in source.execute('''SELECT t.task_id,t.claim_id,t.phase_at_dispatch,
            t.closure_gap_id,t.closure_gap_revision,t.started_at,t.attempt_terminal_kind,
            t.attempt_terminal_at,w.worker_id,w.coordinator_session_id,w.released_at
            FROM tasks t LEFT JOIN worker_claims w
            ON t.lineage_id=w.lineage_id AND t.task_id=w.task_id
            WHERE t.lineage_id=? AND (? IS NULL OR t.claim_id=?)
            ORDER BY t.started_at,t.task_id''',(lineage,claim,claim))]
        by_task={r['task_id']:r for r in tasks}
        if task and task not in by_task:
            raise ContextError(f'Unknown task in requested lineage/claim: {task}')
        sql='SELECT * FROM receipts WHERE source=? AND lineage=?'
        args=[str(state.resolve()),lineage]
        for column,value in [('claim',claim),('receipt_id',receipt_id)]:
            if value is not None: sql+=f' AND {column}=?';args.append(value)
        if contains is not None:
            sql+=' AND instr(search_text,?)>0';args.append(contains.casefold())
        sql+=' ORDER BY recorded_at,task,sequence'
        rows=[r for r in db.execute(sql,args) if (r['task'],r['sequence']) in visible_receipts]
        if contains is not None and claim is None:
            matched_tasks={r['task'] for r in rows}
            tasks=[t for t in tasks if t['task_id'] in matched_tasks]
            by_task={t['task_id']:t for t in tasks}
        decoded={}
        for row in rows:
            value=json.loads(row['payload'])
            if receipt_envelope(value)['receipt_id'] != row['receipt_id']:
                raise ContextError('Derived receipt index is corrupt; rerun with --rebuild from the unchanged source')
            decoded[row['receipt_id']]=value
        heads={}
        for row in rows:
            heads[row['receipt_id'] if contains is not None or receipt_id is not None else row['task']]=row
        # A referenced older checkpoint remains eligible even when its task has
        # a newer checkpoint. Recency is a browsing projection, not supersession.
        if route:
            for row in rows:
                value=compact_worker_receipt(decoded[row['receipt_id']])
                if _related(value,route):heads[row['receipt_id']]=row
        heads={row['receipt_id']:row for row in heads.values()}
        catalogue=[]; selected=[]
        assigned=by_task.get(task,{})
        for row in heads.values():
            value=decoded[row['receipt_id']]
            receipt=compact_worker_receipt(value,recorded_at=row['recorded_at'])
            related=_related(receipt,route)
            prior=by_task.get(row['task'],{})
            if assigned.get('closure_gap_id') is not None and prior.get('closure_gap_id') == assigned['closure_gap_id']:
                related.append('same closure gap; revisions remain distinct')
            if task == row['task']: related.append('assigned task')
            if len(heads)==1: related.append('only task with a receipt for this query')
            if contains is not None or receipt_id is not None: related.append('explicit history query')
            catalogue.append({k:receipt[k] for k in ('receipt_id','task_id','recorded_at','summary',
                                                    'first_open_boundary','evidence_ceiling')})
            catalogue[-1].update(relationship=related,artifact_count=len(receipt['artifact_refs']),
                                source_bytes=len(row['payload'].encode('utf-8')))
            if related: selected.append({'relationship':related,'receipt':receipt})
        cli=[sys.executable,str(Path(__file__).resolve()),'--workspace',str(workspace.resolve()),
             '--state',str(state.resolve()),'--lineage',lineage]
        if claim: cli+=['--claim',claim]
        runs=[]
        session_ids={t['coordinator_session_id'] for t in tasks if t['coordinator_session_id']}
        for r in db.execute('SELECT * FROM runs WHERE lineage=?',(lineage,)):
            if r['session_id'] in session_ids:
                runs.append(dict(r)|{'metadata':json.loads(r['metadata'])})
        dispatches=[dict(r) for r in db.execute('''SELECT task,path,digest,context_json FROM dispatches
            WHERE source=? AND lineage=?''',(str(state.resolve()),lineage)) if r['task'] in by_task]
        for d in dispatches: d['context']=json.loads(d.pop('context_json'))
        result={'schema':'de67.work-context.v1','observed_at':time.time(),'lineage':lineage,
                'claim':claim,'task':task,'tasks':tasks,'worker_library':workers,'related_results':catalogue,
                'selected_context':selected,'receipt_count':len(rows),'task_head_count':len({r['task'] for r in rows}),
                'newly_indexed':added,'runner_records':runs,'thread_records':thread_records(tasks),'dispatches':dispatches,
                'history_query_argv':cli+['--contains','SEARCH TEXT'],
                'full_history_argv':cli+['--full'],
                'evidence_limit':'Relationships are exact references or shared gaps, not inferred ancestry. '
                'Task heads do not supersede other tasks or older proof. Full receipts and source artifacts remain available.'}
        # A specifically queried live task may expose the supported continuation
        # command.  These bindings come from the same read-only source snapshot
        # as ``tasks``; kind and evidence are intentionally left to the caller.
        # The handle is optional convenience, never a claim of authority.  The
        # harness rechecks ownership when this argv is executed, so a stale
        # template cannot bypass a changed or released worker claim.
        if task is not None:
            live = by_task.get(task)
            if (live is not None and live.get('worker_id') and
                    live.get('released_at') is None and
                    live.get('attempt_terminal_at') is None):
                result['checkpoint_worker_template'] = {
                    'optional': True,
                    'argv': [sys.executable, str(Path(__file__).with_name('deadline_harness.py')),
                             '--state', str(state.resolve()), 'checkpoint-worker',
                             '--lineage', lineage, '--task', task,
                             '--worker', live['worker_id']],
                    'bindings': {'state': str(state.resolve()), 'lineage': lineage,
                                 'task': task, 'worker': live['worker_id']},
                    'note': 'Optional checkpoint continuation; supply --kind and --evidence.'
                }
        if full:
            result['receipts']=[{'receipt_id':r['receipt_id'],'sequence':r['sequence'],
                                'recorded_at':r['recorded_at'],'receipt':decoded[r['receipt_id']]} for r in rows]
        return result
    finally:
        source.close();db.close()


def thread_records(tasks: list[dict], *, codex_home: Path | None=None) -> dict:
    """Join known worker/parent IDs to runtime metadata, never scan trace contents."""
    root=codex_home or Path(os.environ.get('CODEX_HOME',str(Path.home()/'.codex')))
    candidates=sorted(root.glob('state_*.sqlite'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
    identities={}
    for task in tasks:
        for field,role in [('worker_id','worker'),('coordinator_session_id','coordinator')]:
            if task.get(field):identities.setdefault(task[field],set()).add((task['task_id'],role))
    missing=set(identities);records=[];errors=[]
    for path in candidates:
        db=sqlite3.connect(f'file:{path.resolve()}?mode=ro',uri=True);db.row_factory=sqlite3.Row
        try:
            columns={r[1] for r in db.execute('PRAGMA table_info(threads)')}
            required={'id','rollout_path','source'}
            if not required<=columns:
                errors.append({'source':str(path),'error':'unsupported thread metadata schema'});continue
            fields=sorted(required|({'model','reasoning_effort','agent_role','agent_path'} & columns))
            for identity in sorted(missing):
                row=db.execute('SELECT '+','.join(fields)+' FROM threads WHERE id=?',(identity,)).fetchone()
                if row is None:continue
                value=dict(row);trace=Path(value['rollout_path']);value['metadata_source']=str(path)
                value['relationships']=[{'task_id':t,'role':r} for t,r in sorted(identities[identity])]
                value['trace']={'path':str(trace),'available':trace.is_file()}
                if trace.is_file():
                    stat=trace.stat();value['trace'].update(bytes=stat.st_size,mtime_ns=stat.st_mtime_ns)
                records.append(value);missing.remove(identity)
        except (sqlite3.Error,OSError) as error:errors.append({'source':str(path),'error':str(error)})
        finally:db.close()
        if not missing:break
    return {'records':records,'unavailable_thread_ids':sorted(missing),'errors':errors,
            'evidence_limit':'Exact task/parent runtime metadata only; trace contents were not loaded.'}


def token_usage_view(workspace: Path, state: Path, lineage: str, *, task: str | None=None,
                     details: bool=False, codex_home: Path | None=None) -> dict:
    """Project the latest assigned coordinator tree into the existing context index."""
    from usage_projection import usage_projection
    with closing(sqlite3.connect(f'file:{state.resolve()}?mode=ro',uri=True)) as source:
        row=source.execute("""SELECT w.coordinator_session_id FROM worker_claims w JOIN tasks t
            ON w.lineage_id=t.lineage_id AND w.task_id=t.task_id
            WHERE w.lineage_id=? AND (? IS NULL OR w.task_id=?)
            AND w.coordinator_session_id IS NOT NULL ORDER BY t.started_at DESC LIMIT 1""",
            (lineage,task,task)).fetchone()
    root_id=row[0] if row else None
    metadata=thread_tree(root_id,codex_home=codex_home) if root_id else {
        'records':[], 'errors':['No assigned coordinator session for this selection']}
    if root_id:
        # Persistent workers have no native parent edge to the current Sol. Their
        # durable task claims supply exact ownership and usage windows instead.
        with closing(sqlite3.connect(f'file:{state.resolve()}?mode=ro',uri=True)) as source:
            claims = source.execute("""SELECT worker_id,claimed_at,released_at FROM worker_claims
                WHERE lineage_id=? AND coordinator_session_id=?""", (lineage,root_id)).fetchall()
        windows = {}
        native_ids = {r['id'] for r in metadata['records']}
        for worker, start, end in claims:
            if worker not in native_ids:
                windows.setdefault(worker, []).append({'start':start, 'end':end})
        for worker, spans in windows.items():
            tree = thread_tree(worker, codex_home=codex_home)
            for record in tree['records']:
                existing = next((r for r in metadata['records'] if r['id'] == record['id']), None)
                if existing is None:
                    metadata['records'].append(record | {'usage_windows':spans})
                elif 'usage_windows' in existing:
                    existing['usage_windows'] = existing['usage_windows'] + spans
            metadata.setdefault('errors', []).extend(tree.get('errors', []))
            metadata.setdefault('unavailable_thread_ids', []).extend(tree.get('unavailable_thread_ids', []))
    db=connect_index(workspace)
    try: result=usage_projection(db,metadata,root_id=root_id,details=details)
    finally: db.close()
    result['selection_basis']='Coordinator owning the selected task, or latest assigned task in this lineage, plus persistent workers/helpers during its assignment windows; not an assertion of a live process.'
    config=workspace/'.de67/state/workspace.json'
    if config.is_file():
        result['allocation_preference']=json.loads(config.read_text(encoding='utf-8')).get('token_allocation_preference')
    return result


def thread_tree(root_id: str, *, codex_home: Path | None=None) -> dict:
    """Discover only one root and its descendants from runtime parent/child metadata."""
    root=codex_home or Path(os.environ.get('CODEX_HOME',str(Path.home()/'.codex')))
    candidates=sorted(root.glob('state_*.sqlite'),key=lambda p:p.stat().st_mtime_ns,reverse=True)
    identities={root_id}; records={}; errors=[]; found_edges=False
    for path in candidates:
        db=sqlite3.connect(f'file:{path.resolve()}?mode=ro',uri=True);db.row_factory=sqlite3.Row
        try:
            tables={r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
            if 'thread_spawn_edges' in tables:
                found_edges=True
                pending=list(identities); visited=set()
                while pending:
                    parent=pending.pop()
                    if parent in visited: continue
                    visited.add(parent)
                    for row in db.execute('SELECT child_thread_id FROM thread_spawn_edges WHERE parent_thread_id=?',(parent,)):
                        identities.add(row[0]);pending.append(row[0])
            for identity in sorted(identities-records.keys()):
                row=db.execute('SELECT id,rollout_path FROM threads WHERE id=?',(identity,)).fetchone()
                if row is not None: records[identity]=dict(row)
        except sqlite3.Error as error: errors.append({'source':str(path),'error':str(error)})
        finally: db.close()
        if found_edges and identities <= records.keys(): break
    if not found_edges: errors.append('Runtime parent/child metadata unavailable; helper coverage unknown')
    return {'records':list(records.values()),'unavailable_thread_ids':sorted(identities-records.keys()),'errors':errors}


def provider_context(workspace: Path, evidence: dict) -> dict:
    config=workspace/'.de67/state/workspace.json'
    if not config.is_file():return {}
    argv=json.loads(config.read_text(encoding='utf-8')).get('context_provider_argv')
    if argv is None:return {}
    if not isinstance(argv,list) or not argv or not all(isinstance(v,str) for v in argv):
        raise ContextError('context_provider_argv must be a nonempty argument array')
    result=subprocess.run(argv,cwd=workspace,input=json.dumps(evidence),text=True,capture_output=True)
    if result.returncode:
        return {'available':False,'argv':argv,'error':result.stderr or result.stdout,
                'evidence_limit':'Provider failed; session status is unknown.'}
    try:return json.loads(result.stdout)
    except json.JSONDecodeError as e:raise ContextError('Context provider returned invalid JSON') from e


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--state',type=Path,required=True)
    p.add_argument('--lineage',required=True)
    for flag in ('claim','task','contains','receipt'):
        p.add_argument('--'+flag)
    p.add_argument('--full',action='store_true')
    p.add_argument('--usage',action='store_true',help='Only current coordinator-tree token usage; --full includes source details')
    p.add_argument('--rebuild',action='store_true',help='Rebuild only derived receipt index; retain source, runs and dispatch relationships')
    a=p.parse_args()
    if a.usage:
        print(json.dumps(token_usage_view(a.workspace,a.state,a.lineage,task=a.task,details=a.full),indent=2))
        return
    result=context_view(a.workspace,a.state,a.lineage,claim=a.claim,task=a.task,
                        contains=a.contains,receipt_id=a.receipt,full=a.full,rebuild=a.rebuild)
    if not any((a.claim,a.task,a.contains,a.receipt,a.full)):
        result['token_usage']=token_usage_view(a.workspace,a.state,a.lineage)
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
