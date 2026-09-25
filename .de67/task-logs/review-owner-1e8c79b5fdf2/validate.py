from pathlib import Path
import sys,json,ast,tempfile,shutil,importlib.util
p=Path(__file__).resolve().parent;sys.path.insert(0,str(p/'candidate/scripts'))
import coordinator_supervisor as cs,policy_kernel as pk,codex_runner as cr
import mutation_guard
mutation_guard.SKILL_ROOT=Path("/Users/josefhorvath/.codex/skills/de67/de-67-3")
from mutation_guard import validate_method_mutation
changes=validate_method_mutation(p/'baseline',p/'candidate',universal=False)
contract=cs.worker_selection_contract();assert 'Terra' not in contract and 'not an ordinary worker' not in contract
assert 'max on Luna/Sol and low on Astra' in contract
prompt=cs.coordinator_prompt(Path.cwd(),Path.cwd()/'.de67/state/deadlines.sqlite3','semantic-surface-cockpit','test-gpt6',None)
assert contract in prompt
fresh=cs._fresh_prompt_module();assert contract in fresh.coordinator_prompt(Path.cwd(),Path.cwd()/'.de67/state/deadlines.sqlite3','semantic-surface-cockpit','test-gpt6',None)
with tempfile.TemporaryDirectory() as tmp:
 w=Path(tmp);(w/'.de67/state').mkdir(parents=True);shutil.copy2(p/'workspace.candidate.json',w/'.de67/state/workspace.json')
 choices=pk.worker_model_choices(w);assert len(choices)==17
 for model in ['gpt-5.6-sol','gpt-5.6-terra','gpt-5.6-luna','gpt-6-terra','unknown']:
  for resume in ['', 'old-thread']:
   try:cr._command('codex',w,{'DE67_AGENT_TRANSPORT':'cli','DE67_COORDINATOR_RUN_ID':'check','DE67_COORDINATOR_MODEL':model,'DE67_COORDINATOR_RESUME_SESSION':resume})
   except cr.RunnerError:pass
   else:raise AssertionError(model)
 for choice in choices:
  argv=cr._command('codex',w,{'DE67_AGENT_TRANSPORT':'cli','DE67_COORDINATOR_RUN_ID':'check','DE67_COORDINATOR_MODEL':choice['model'],'DE67_COORDINATOR_REASONING_EFFORT':choice['reasoning_effort'],'DE67_COORDINATOR_RESUME_SESSION':'old-thread'})
  assert choice['model'] in argv and 'model_reasoning_effort='+choice['reasoning_effort'] in argv
for e in json.loads((p/'extra-edits.json').read_text()):
 if e['target'].endswith('.py'):ast.parse(Path(e['candidate']).read_text())
 if e['target'].endswith('scripts/workspace_setup.py'):
  spec=importlib.util.spec_from_file_location('setup_check',e['candidate']);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
  assert len(mod._worker_capabilities([(c['model'],c['reasoning_effort']) for c in choices],required=True))==17
  for model in ['gpt-5.6-sol','gpt-5.6-terra','gpt-6-terra']:
   try:mod._worker_capabilities([(model,'low')],required=False)
   except mod.SetupError:pass
   else:raise AssertionError(model)
# Compare the lab's guidance and roster function bodies to the tested installed candidate.
for e in json.loads((p/'extra-edits.json').read_text()):
 if '/lab/' not in e['candidate']:continue
 if e['target'].endswith('coordinator_supervisor.py'):
  tree=ast.parse(Path(e['candidate']).read_text());fn=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='worker_selection_contract');space={};exec(compile(ast.Module(body=[fn],type_ignores=[]),'lab-contract','exec'),space);assert space['worker_selection_contract']()==contract
 if e['target'].endswith('policy_kernel.py'):
  tree=ast.parse(Path(e['candidate']).read_text());assert not any(isinstance(n,ast.FunctionDef) and n.name=='worker_selection_contract' for n in tree.body)
result=dict(method_changed=changes,delivered_coordinator_prompt=True,available_pairs=choices,retired_cli_start_and_resume_rejected=True,setup_rejects_retired=True,lab_contract_duplicate_removed=True)
(p/'validation.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
