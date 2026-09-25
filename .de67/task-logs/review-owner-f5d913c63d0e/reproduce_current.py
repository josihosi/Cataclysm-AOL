"""Read-only production-function diagnostics; synthetic inputs, no native proof."""
import copy
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/openclaw_harness'))
import evidence_events
import evidence_display
from gameplay_display import display
from registry_query_output import _run_observation

out = {}
with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    source = directory / 'events.jsonl'
    events = [
        {'event':'accepted','accepted':True,'run_id':'r','process_instance':'p','request_id':'wanted','actor_id':'npc-a'},
        {'event':'completed','run_id':'r','process_instance':'p','request_id':'wanted','actor_id':'npc-a'},
        {'event':'accepted','accepted':True,'run_id':'r','process_instance':'p','request_id':'unrelated','actor_id':'npc-b'},
        {'event':'completed','run_id':'r','process_instance':'p','request_id':'unrelated','actor_id':'npc-b'},
    ]
    source.write_text(''.join(json.dumps(row)+'\n' for row in events))
    with patch.object(evidence_events, 'retain', lambda value: evidence_display.retain(value,directory/'retained')):
        result=evidence_events.query([{'path':str(source),'producer':'synthetic-native'}],{'request_id':'wanted'},limit=1)
    out['exact_query']={'matched':result['matched'],'returned_rows':len(result['rows']),
        'returned_link_request_ids':[x['request_id'] for x in result['links']],
        'wanted_link':{k:v for k,v in next(x for x in result['links'] if x['request_id']=='wanted').items() if k in ['status','missing']},
        'wanted_outcome':next(x for x in result['links'] if x['request_id']=='wanted')['outcome'],
        'scanned_records':result['scanned_records'],'displayed_records':result['displayed_records'],
        'displayed_link_bytes':result['displayed_link_bytes'],
        'evidence_ceiling':'Synthetic records through current production query; no live cost measurement'}
    for count in [10,100]:
        expanded=events[:2]+[{'event':'completed','run_id':'r','process_instance':'p','request_id':f'unrelated-{i}'} for i in range(count)]
        source.write_text(''.join(json.dumps(row)+'\n' for row in expanded))
        with patch.object(evidence_events, 'retain', lambda value: evidence_display.retain(value,directory/'retained')):
            result=evidence_events.query([{'path':str(source),'producer':'synthetic-native'}],{'request_id':'wanted'},limit=1)
        out[f'query_growth_{count}']={'matched':result['matched'],'links':len(result['links']),
            'link_bytes':len(json.dumps(result['links']).encode()),
            'scanned_records':result['scanned_records'],'displayed_records':result['displayed_records'],
            'displayed_link_bytes':result['displayed_link_bytes']}

response={'ok':True,'observation':{'run_id':'r','surface_id':'world','observation_id':'o1','game_minutes':100,
    'surface':{'kind':'world','actions':[{'id':'world.wait','enabled':True}],
               'facts':{'visible_entities':[{'identity':{'id':str(i)},'name':f'NPC {i}','state':'idle'} for i in range(6)]}}}}
first,state=display(response,refresh=True)
changed=copy.deepcopy(response);changed['observation']['observation_id']='o2';changed['observation']['surface']['facts']['visible_entities'][5]['state']='attacking'
second,_=display(changed,previous=state)
out['display_delta']={'changed_entity':'5','shown_entities':[x['identity']['id'] for x in second['facts_changed']['visible_entities']['preview']],
    'omitted':second['facts_changed']['visible_entities']['omitted'],
    'input_fields':list(first['current_input']),
    'advertised_doc_selectors_present':all(k in first['current_input'] for k in ['source_selector','actions_selector']),
    'evidence_ceiling':'Synthetic observations through production display; original full facts remain retrievable'}
report={'proof_classification':{'verdict':'proved'},'steps':[{'action_id':'world.move','accepted':True}],
    'scenario_manifest':{'normalized':{'scenario_id':'movement-control'}}}
view=_run_observation(report,run_id='synthetic-movement')
out['generic_run_summary']={k:view[k] for k in ['claim_scope','verdict','proof_depth','missing_fields']}
path=Path(__file__).with_name('current-reproductions.json');path.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
