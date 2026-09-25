#!/usr/bin/env python3
"""Small revisioned working-context library; originals and dispatch packets remain intact.

put imports one named bundle; reuse pins an existing revision for another task; drop
removes an active name. prepare replaces the task's written brief/selection/handoff.
catalog is metadata only, show retrieves one revision or a named Markdown section,
and assemble previews exactly the context that policy dispatch will inject.
Sizes are UTF-8 bytes, not claimed tokenizer or billing measurements.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile

DEFAULT_LIMITS = dict(items=12, item_bytes=4096, active_bytes=49152, selected_bytes=24576)


class ContextError(ValueError):
    pass


def _bytes(text):
    return len(text.encode('utf-8'))


def _identity(value):
    if len(value) > 128 or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', value):
        raise ContextError('Expected a simple task or bundle name')
    return value


def _root(workspace):
    return Path(workspace) / '.de67/state/context-library'


def _task_path(workspace, task):
    # Task identities belong to the deadline harness, not the bundle-name grammar.
    digest = hashlib.sha256(task.encode('utf-8')).hexdigest()
    return _root(workspace) / 'tasks' / ('task-' + digest + '.json')


def _save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    data = json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + '\n'
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix='.context-')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def task_view(workspace, task):
    path = _task_path(workspace, task)
    if not path.exists() and len(task) <= 128 and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', task):
        # Read already-issued library state; the next write migrates it to the hashed path.
        legacy = _root(workspace) / 'tasks' / (task + '.json')
        if legacy.is_file():
            value = json.loads(legacy.read_text(encoding='utf-8'))
            if value.get('task') == task:
                return value
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {
        'version': 1, 'task': task, 'limits': dict(DEFAULT_LIMITS), 'active': {},
        'selected': [], 'brief': '', 'handoff': ''}


def revision(workspace, digest):
    if not re.fullmatch(r'[a-f0-9]{64}', digest):
        raise ContextError('Revision must be a full SHA-256')
    raw = (_root(workspace) / 'revisions' / (digest + '.json')).read_bytes()
    if hashlib.sha256(raw).hexdigest() != digest:
        raise ContextError('Context revision digest mismatch: ' + digest)
    return json.loads(raw)


def _fresh(bundle):
    sources = {bundle['source']: bundle['source_sha256'], **bundle.get('dependencies', {})}
    for path, expected in sources.items():
        source = Path(path)
        if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise ContextError('Stale context source or dependency; refresh or replace explicitly: ' + str(source))


def _render(name, digest, bundle):
    return (f"Selected {bundle['kind']} bundle {name}@{digest}\nSource: {bundle['source']}\n"
            + bundle['text'].strip() + '\nEvidence references: ' + json.dumps(bundle['references']))


def _validate(workspace, view):
    limits = view['limits']
    if set(limits) != set(DEFAULT_LIMITS) or any(type(v) is not int or v <= 0 for v in limits.values()):
        raise ContextError('Context limits must be positive integers in UTF-8 bytes and item count')
    sizes = [_bytes(_render(name, digest, revision(workspace, digest))) for name, digest in view['active'].items()]
    if len(sizes) > limits['items'] or any(s > limits['item_bytes'] for s in sizes) or sum(sizes) > limits['active_bytes']:
        raise ContextError('Active context capacity exceeded; replace, merge or drop irrelevant bundles; nothing truncated')
    selected = view['selected']
    if len(set(selected)) != len(selected) or any(n not in view['active'] for n in selected):
        raise ContextError('Selection must name distinct active bundles')
    if sum(_bytes(_render(n, view['active'][n], revision(workspace, view['active'][n]))) + 2 for n in selected) > limits['selected_bytes']:
        raise ContextError('Selected context capacity exceeded; select a relevant subset; nothing truncated')


def put(workspace, task, name, source, kind='facts', references=(), dependencies=()):
    name = _identity(name)
    source = Path(source).resolve()
    raw = source.read_bytes()
    text = raw.decode('utf-8')
    if not text.strip() or kind not in ('facts', 'skill'):
        raise ContextError('Bundle needs nonempty UTF-8 text and kind facts or skill')
    bundle = dict(kind=kind, text=text, bytes=len(raw), source=str(source),
                  source_sha256=hashlib.sha256(raw).hexdigest(), references=list(references))
    bundle['dependencies'] = {str(Path(p).resolve()): hashlib.sha256(Path(p).read_bytes()).hexdigest()
                              for p in dependencies}
    encoded = json.dumps(bundle, sort_keys=True, ensure_ascii=False).encode('utf-8')
    digest = hashlib.sha256(encoded).hexdigest()
    path = _root(workspace) / 'revisions' / (digest + '.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() != encoded:
        raise ContextError('Revision collision')
    if not path.exists():
        path.write_bytes(encoded)
    reuse(workspace, task, name, digest)
    return digest


def reuse(workspace, task, name, digest):
    view = task_view(workspace, task)
    revision(workspace, digest)
    view['active'][_identity(name)] = digest
    _validate(workspace, view)
    _save(_task_path(workspace, task), view)


def drop(workspace, task, name):
    view = task_view(workspace, task)
    view['active'].pop(name, None)
    view['selected'] = [n for n in view['selected'] if n != name]
    _save(_task_path(workspace, task), view)


def prepare(workspace, task, brief, selected=(), handoff=''):
    if not brief.strip():
        raise ContextError('A prepared packet needs the coordinator written assignment')
    view = task_view(workspace, task)
    view.update(brief=brief, selected=list(selected), handoff=handoff, automatic=False,
                assignment_revision=_assignment_revision(workspace, task), outcome_revision=None)
    _validate(workspace, view)
    for name in selected:
        _fresh(revision(workspace, view['active'][name]))
    _save(_task_path(workspace, task), view)


def _assignment_revision(workspace, task):
    """Bind current task text, not the entire changing ledger or other assignments."""
    ledger = Path(workspace) / '.de67/work-ledger.md'
    if not ledger.is_file():
        return None
    lines = ledger.read_text(encoding='utf-8').splitlines()
    prefix = '  - Assignment ' + task + ':'
    matches = [i for i, line in enumerate(lines) if line.startswith(prefix)]
    if len(matches) > 1:
        raise ContextError('Duplicate ledger assignment for ' + task)
    if not matches:
        return None
    start = matches[0]
    body = [lines[start][len(prefix):].strip()]
    for line in lines[start + 1:]:
        if line.startswith(('  - ', '- ', '#')):
            break
        body.append(line.strip())
    return hashlib.sha256('\n'.join(body).strip().encode('utf-8')).hexdigest()


def dispatch_context(workspace, task, outcome, frontier=''):
    """Use one prepared route, including a legitimate brief with no reusable bundles.

    Existing issued packets remain immutable. A changed explicit assignment requires
    the coordinator to reselect custom context; unrelated ledger work does not.
    """
    view = task_view(workspace, task)
    current = _assignment_revision(workspace, task)
    outcome_revision = hashlib.sha256(outcome.encode('utf-8')).hexdigest()
    if not view['brief'] or view.get('automatic'):
        view.update(brief=outcome, handoff=frontier, automatic=True,
                    assignment_revision=current, outcome_revision=outcome_revision)
        _validate(workspace, view)
        _save(_task_path(workspace, task), view)
    elif (view.get('assignment_revision') != current or
          view.get('outcome_revision') not in (None, outcome_revision)):
        raise ContextError('Task assignment changed; prepare updated context or use a successor task: ' + task)
    elif view.get('outcome_revision') is None:
        view['outcome_revision'] = outcome_revision
        _save(_task_path(workspace, task), view)
    return selected_context(workspace, task)


def catalog(workspace, task):
    view = task_view(workspace, task)
    rows = []
    for name, digest in view['active'].items():
        bundle = revision(workspace, digest)
        try:
            _fresh(bundle)
            fresh = True
        except ContextError:
            fresh = False
        rows.append(dict(name=name, revision=digest, kind=bundle['kind'], bytes=_bytes(_render(name, digest, bundle)),
                         fresh=fresh, selected=name in view['selected'], source=bundle['source']))
    return dict(task=task, limits=view['limits'], bundles=rows)


def selected_sources(workspace, task):
    view = task_view(workspace, task)
    return {revision(workspace, view['active'][name])['source'] for name in view['selected']}


def selected_context(workspace, task):
    """No catalogue/history ingestion: render only the coordinator's current selection."""
    view = task_view(workspace, task)
    _validate(workspace, view)
    if not view['brief']:
        return ''
    label = 'Assignment outcome: ' if view.get('automatic') else 'Coordinator written task brief:\n'
    parts = [label + view['brief'].strip()]
    if view['handoff'].strip():
        label = ('Current proof frontier:\n' if view.get('automatic') else
                 'Current predecessor handoff (results and uncertainty, not history):\n')
        parts.append(label + view['handoff'].strip())
    for name in view['selected']:
        digest = view['active'][name]
        bundle = revision(workspace, digest)
        _fresh(bundle)
        parts.append(_render(name, digest, bundle))
    return '\n\n'.join(parts) + '\n'


def show(workspace, digest, section=None):
    text = revision(workspace, digest)['text']
    if section is None:
        return text
    lines = text.splitlines()
    for start, line in enumerate(lines):
        match = re.match(r'^(#{1,6})\s+(.+?)\s*$', line)
        if match and match[2] == section:
            depth = len(match[1])
            end = start + 1
            while end < len(lines):
                next_heading = re.match(r'^(#{1,6})\s+', lines[end])
                if next_heading and len(next_heading[1]) <= depth:
                    break
                end += 1
            return '\n'.join(lines[start:end]) + '\n'
    raise ContextError('No Markdown section: ' + section)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--task', required=True)
    commands = parser.add_subparsers(dest='command', required=True)
    add = commands.add_parser('put'); add.add_argument('--name', required=True); add.add_argument('--source', type=Path, required=True)
    add.add_argument('--kind', choices=['facts', 'skill'], default='facts'); add.add_argument('--reference', action='append', default=[])
    add.add_argument('--dependency', type=Path, action='append', default=[], help='Underlying source/evidence whose change invalidates this summary')
    reuse_parser = commands.add_parser('reuse'); reuse_parser.add_argument('--name', required=True); reuse_parser.add_argument('--revision', required=True)
    remove = commands.add_parser('drop'); remove.add_argument('--name', required=True)
    build = commands.add_parser('prepare'); build.add_argument('--brief', type=Path, required=True)
    build.add_argument('--bundle', action='append', default=[]); build.add_argument('--handoff', type=Path)
    commands.add_parser('catalog'); commands.add_parser('assemble')
    read = commands.add_parser('show'); read.add_argument('--revision', required=True); read.add_argument('--section')
    budget = commands.add_parser('limits')
    for key in DEFAULT_LIMITS:
        budget.add_argument('--' + key.replace('_', '-'), type=int)
    args = parser.parse_args(argv)
    try:
        w, t = args.workspace.resolve(), args.task
        if args.command == 'put':
            print(put(w, t, args.name, args.source, args.kind, args.reference, args.dependency))
        elif args.command == 'reuse': reuse(w, t, args.name, args.revision)
        elif args.command == 'drop': drop(w, t, args.name)
        elif args.command == 'prepare':
            prepare(w, t, args.brief.read_text(encoding='utf-8'), args.bundle,
                    args.handoff.read_text(encoding='utf-8') if args.handoff else '')
        elif args.command == 'catalog': print(json.dumps(catalog(w, t), indent=2))
        elif args.command == 'assemble': print(selected_context(w, t))
        elif args.command == 'show': print(show(w, args.revision, args.section))
        elif args.command == 'limits':
            view = task_view(w, t)
            view['limits'].update({k: getattr(args, k) for k in DEFAULT_LIMITS if getattr(args, k) is not None})
            _validate(w, view); _save(_task_path(w, t), view)
        return 0
    except (ContextError, OSError, ValueError, KeyError) as error:
        print('context error: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
