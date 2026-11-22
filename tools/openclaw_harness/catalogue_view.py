"""Presentation of explicit scenario catalogue metadata; never selection authority."""
from __future__ import annotations

from typing import Any, Mapping, Sequence
import json
import sqlite3
import os
from pathlib import Path


def attach_registry_status(rows: Sequence[dict[str, Any]], registry: str | None = None) -> None:
    """Attach current source-bound registry facts without creating or changing an index."""
    from scenario_registry_store import open_registry, build_registry_query_candidate_snapshot, ScenarioRegistryStoreError

    unknown = {'lifecycle': 'unknown', 'token_eligible': None,
               'availability': 'Registry status unknown: index unavailable'}
    for row in rows:
        row['registry_status'] = dict(unknown)
    ids = tuple(row['scenario_id'] for row in rows if row.get('scenario_id'))
    if not ids:
        return
    try:
        connection = open_registry(registry, writable=False)
        try:
            snapshots = build_registry_query_candidate_snapshot(
                connection, include_lifecycle_states=('absent', 'quarantined', 'retired'), manifest_ids=ids)
        finally:
            connection.close()
    except (OSError, sqlite3.Error, ValueError, ScenarioRegistryStoreError):
        return
    by_id = {snapshot.scenario_id: snapshot for snapshot in snapshots}
    for row in rows:
        snapshot = by_id.get(row.get('scenario_id'))
        if snapshot is None:
            row['registry_status']['availability'] = 'Registry status unknown: scenario not in registry'
            continue
        manifest = snapshot.explanation.get('manifest', {})
        source = row.get('scenario_manifest', {}).get('source', {})
        if not source.get('sha256') or not manifest.get('present') or \
                manifest.get('source_path') != source.get('path') or manifest.get('sha256') != source.get('sha256'):
            row['registry_status']['availability'] = 'Registry status unknown: source not current in registry'
            continue
        lifecycle = snapshot.lifecycle_state
        eligible = bool(snapshot.token_eligible)
        availability = ('Unavailable: ' + lifecycle if lifecycle != 'active' else
                        'Registry eligible; qualification unknown' if eligible else 'Unavailable: not eligible')
        row['registry_status'] = {'lifecycle': lifecycle, 'token_eligible': eligible, 'availability': availability}


def recipe_families(root: Path) -> list[dict[str, Any]]:
    """Read family-only declarations from the owning recipes, not a second index."""
    marker = '<!-- caol-catalogue-family: '
    result = []
    if not root.is_dir():
        return result
    for path in sorted(root.glob('*.md')):
        with path.open(encoding='utf-8') as stream:
            for _, line in zip(range(12), stream):
                if not line.startswith(marker):
                    continue
                if not line.rstrip().endswith(' -->'):
                    raise ValueError('invalid recipe family metadata: ' + str(path))
                value = json.loads(line[len(marker):].rstrip()[:-4])
                fields = ('family_id', 'title', 'purpose', 'kind', 'starting_save')
                if not isinstance(value, dict) or any(not isinstance(value.get(k), str) or not value[k].strip() for k in fields) or value['kind'] != 'maintained':
                    raise ValueError('invalid recipe family metadata: ' + str(path))
                result.append({'id': value['family_id'], 'title': value['title'], 'purpose': value['purpose'],
                               'kind': value['kind'], 'starting_save': value['starting_save'],
                               'recipe': path.as_posix(), 'variants': [], 'variant_count': 0})
    return result


def family_view(rows: Sequence[Mapping[str, Any]], *, kind: str = 'maintained',
                family: str = '', search: str = '', recipes: Sequence[Mapping[str, Any]] = ()) -> dict[str, Any]:
    """Group source-bound rows without inferring membership or qualification."""
    if kind not in ('maintained', 'diagnostic', 'all'):
        raise ValueError('invalid catalogue view')
    families: dict[str, dict[str, Any]] = {}
    others = []
    query = search.strip().casefold()
    for source in rows:
        row = dict(source)
        metadata = row.get('catalogue')
        membership = metadata.get('kind', 'unknown') if isinstance(metadata, dict) else 'unknown'
        if membership not in ('maintained', 'diagnostic', 'historical', 'continuation'):
            membership = 'unknown'
        if membership in ('maintained', 'diagnostic') and any(
                not isinstance(metadata.get(k), str) or not metadata[k].strip()
                for k in ('family_id', 'family_title', 'family_purpose', 'variant')):
            membership = 'unknown'
        if kind != 'all' and membership != kind:
            continue
        if query and query not in ' '.join(str(row.get(k, '')) for k in
                ('name', 'description', 'scenario_id')).casefold() and not (
                isinstance(metadata, dict) and query in ' '.join(str(v) for v in metadata.values()).casefold()):
            continue
        if membership not in ('maintained', 'diagnostic'):
            if not family:
                row['membership'] = membership
                others.append(row)
            continue
        fid = metadata['family_id']
        if family and fid != family:
            continue
        descriptor = (metadata['family_title'], metadata['family_purpose'], membership)
        entry = families.setdefault(fid, {'id': fid, 'title': descriptor[0], 'purpose': descriptor[1],
                                          'kind': membership, 'variants': []})
        if (entry['title'], entry['purpose'], entry['kind']) != descriptor:
            raise ValueError('inconsistent catalogue family: ' + fid)
        row['membership'] = membership
        row['variant'] = metadata['variant']
        row.pop('catalogue', None)
        binding = row.get('scenario_manifest')
        if isinstance(binding, dict):
            row['scenario_manifest'] = {key: binding[key] for key in ('source', 'validation') if key in binding}
        if 'status' in row:
            row['declared_status'] = row.pop('status')
        entry['variants'].append(row)
    for recipe in recipes:
        fid = recipe['id']
        if fid in families:
            raise ValueError('duplicate recipe and scenario family: ' + fid)
        if kind == 'diagnostic' or (family and family != fid):
            continue
        if query and query not in ' '.join(str(recipe.get(k, '')) for k in ('id', 'title', 'purpose', 'starting_save')).casefold():
            continue
        families[fid] = dict(recipe)
    result = []
    for key in sorted(families):
        entry = families[key]
        entry['variant_count'] = len(entry['variants'])
        if not family and not query:
            entry['variants'] = [{**{k: row[k] for k in ('scenario_id', 'name', 'variant', 'opportunity') if k in row},
                                  'qualification': 'unknown', 'registry_status': row.get('registry_status',
                                  {'lifecycle': 'unknown', 'token_eligible': None, 'availability': 'Registry status unknown'})} for row in entry['variants']]
        result.append(entry)
    return {'families': result, 'other_scenarios': others}


def saved_camp_opportunity(dimension: Path) -> dict[str, Any]:
    """Inspect one fixture's saved facts; never interpret them as dispatch authority."""
    import hashlib
    unknown = {'status': 'unknown', 'reason': 'saved camp facts unavailable', 'sites': []}
    try:
        with dimension.open('rb') as stream:
            before = stream.fileno()
            stat_before = os.fstat(before)
            if stat_before.st_size > 64 * 1024 * 1024:
                return {**unknown, 'reason': 'saved dimension exceeds inspection limit'}
            raw = stream.read()
            stat_after = os.fstat(before)
        current = dimension.stat()
        # Windows can expose different ctime semantics through fstat and stat.
        # Compare change time on the open handle and compare file identity across paths.
        def identity(stat):
            return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns
        if (stat_before.st_ctime_ns != stat_after.st_ctime_ns or
                identity(stat_before) != identity(stat_after) or identity(stat_after) != identity(current)):
            return {**unknown, 'reason': 'saved dimension changed during inspection'}
        header, sep, body = raw.partition(b'\n')
        if not sep or header.removesuffix(b'\r') != b'# version 39':
            return {**unknown, 'reason': 'unsupported dimension header'}
        payload = json.loads(body)
        world = payload['overmapbuffer']['bandit_live_world']
        sites = world['sites']
        if world.get('schema_version') != 7:
            return {**unknown, 'reason': 'unsupported saved camp world schema'}
        if not isinstance(sites, list):
            raise ValueError('sites is not a list')
    except (OSError, ValueError, TypeError, KeyError, UnicodeError):
        return unknown

    def minute(value: Any) -> bool:
        return type(value) is int and -1 <= value <= 2147483647

    output = []
    for site in sites:
        if not isinstance(site, dict):
            return {**unknown, 'reason': 'malformed site'}
        decision = site.get('camp_decision')
        report = site.get('current_scout_report')
        state = decision.get('state') if isinstance(decision, dict) else None
        routine = site.get('next_routine_dispatch_eligible_minutes')
        result = {'site_id': site.get('site_id'), 'decision': None,
                  'routine_next_eligible_minutes': None, 'report': {'status': 'unknown'}}
        if not isinstance(result['site_id'], str) or not result['site_id']:
            return {**unknown, 'reason': 'site identity unavailable'}
        if site.get('schema_version') == 12 and isinstance(decision, dict) and decision.get('schema_version') == 2 and state in {
                'idle', 'report_awaiting_assessment', 'preparing_follow_on', 'cooldown', 'abandoned'}:
            result['decision'] = state
        if site.get('schema_version') == 12 and minute(routine):
            result['routine_next_eligible_minutes'] = {'status': 'unset' if routine == -1 else 'known',
                                                        'value': None if routine == -1 else routine}
        if site.get('schema_version') == 12 and isinstance(report, dict) and report.get('schema_version') == 5:
            rev, gen = report.get('revision'), report.get('source_generation')
            activity, key = report.get('source_activity_id'), report.get('application_key')
            if type(rev) is int and type(gen) is int and isinstance(activity, str) and isinstance(key, str):
                if rev == 0 and gen == 0 and not activity and not key:
                    result['report'] = {'status': 'absent'}
                elif rev > 0 and gen > 0 and activity and key:
                    delivered = report.get('delivered_minutes')
                    assessment = report.get('assessment')
                    report_out = {'status': 'present', 'delivered_minutes': delivered if minute(delivered) and delivered >= 0 else None,
                                  'authorization_expiry_minutes': None}
                    if minute(delivered) and delivered >= 0 and isinstance(assessment, dict) and assessment.get('schema_version') in (1, 2, 3):
                        threshold = assessment.get('threshold_class')
                        if threshold in ('none', 'normal', 'burned'):
                            report_out['authorization_expiry_minutes'] = min(2147483647, delivered + 48 * 60)
                        elif threshold == 'site_signal':
                            observed, source = assessment.get('site_signal_observed_minutes'), assessment.get('site_signal_source_id')
                            if minute(observed) and 0 <= observed <= delivered and isinstance(source, str) and source:
                                report_out['authorization_expiry_minutes'] = min(2147483647, observed + 48 * 60, delivered + 48 * 60)
                    result['report'] = report_out
        output.append(result)
    return {'status': 'known', 'source': {'path': str(dimension.resolve()), 'sha256': hashlib.sha256(raw).hexdigest()},
            'basis': 'Saved source facts only; not current live state, qualification, motivation or guaranteed dispatch.',
            'time_unit': 'minutes since cataclysm start; expiry is exclusive', 'sites': output}
