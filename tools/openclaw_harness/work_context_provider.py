#!/usr/bin/env python3
"""Read current session metadata at exact existing evidence references; never send input."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys
import time


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
        result.append({'session':str(session),'files':files,'recorded_status':status,
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
