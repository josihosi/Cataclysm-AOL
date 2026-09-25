from pathlib import Path
p=Path(__file__).parent;c=p/'candidate'
f=c/'scripts/coordinator_supervisor.py';s=f.read_text().replace('ordinary repairs; Terra for ','ordinary repairs; GPT-6 Sol for ').replace('Model choice: use Luna','Model choice: use GPT-6 Luna').replace('diagnosis; Astra when','diagnosis; GPT-6 Astra when').replace('may reduce uncertainty or rework. Sol coordinates and is not an ordinary worker. After a ','may reduce uncertainty or rework. Sol also owns coordination in its separate coordinator role. After a ')
# This is read-only liveness discovery, not model authorization. Preserve old model discovery.
s=s.replace('("luna", "terra", "astra")','("luna", "sol", "astra", "terra")')
f.write_text(s)
f=c/'scripts/policy_kernel.py';s=f.read_text();s=s.replace('"gpt-6-sol": ("low", "medium", "high", "xhigh", "max"),','"gpt-6-sol": ("low", "medium", "high", "xhigh", "max", "ultra"),').replace('"gpt-6-astra": ("low", "medium", "high", "xhigh", "max"),','"gpt-6-astra": ("low", "medium", "high", "xhigh", "max", "ultra"),')
s=s.replace('if not isinstance(choice["reasoning_effort"], str) or not re.fullmatch(\n            r"[A-Za-z0-9][A-Za-z0-9._-]*", choice["reasoning_effort"]\n        ):','if choice["reasoning_effort"] not in efforts_by_model[choice["model"]]:')
f.write_text(s)
f=c/'scripts/worker_library.py';s=f.read_text();old='''                if worker["model"] not in {"gpt-6-luna", "gpt-6-sol", "gpt-6-astra"}:
                    raise WorkerLibraryError("Worker model is retired; use GPT-6 Luna, Sol or Astra")''';new=old+'''
                from policy_kernel import worker_model_choices
                if {"model": worker["model"], "reasoning_effort": worker["effort"]} not in worker_model_choices(self.workspace):
                    raise WorkerLibraryError("Worker model/effort is no longer available in the current roster")''';assert old in s;s=s.replace(old,new);f.write_text(s)
f=c/'assets/environment/mutation-suggestions.md';f.write_text(f.read_text().replace('gpt-5.6-sol','gpt-6-sol'))
# Existing positive execution fixtures represent new work; historical receipt fixtures stay intact.
for name in ['test_worker_library.py','test_policy_kernel.py','test_coordinator_supervisor.py']:
 f=c/'tests'/name;s=f.read_text().replace('gpt-5.6-luna','gpt-6-luna').replace('gpt-5.6-terra','gpt-6-sol').replace('gpt-5.6-sol','gpt-6-sol').replace('terra-coder','sol-coder');f.write_text(s)
f=c/'tests/test_worker_library.py';s=f.read_text();idx=s.index('    def prepared_text(');s=s[:idx]+'''    def test_stored_retired_worker_cannot_start_or_resume(self):
        self.worker()
        self.assign()
        with library._connect(self.workspace, write=True) as db:
            db.execute("UPDATE workers SET model='gpt-5.6-terra' WHERE name='pilot'")
        self.dispatcher.process_pending()
        self.assertFalse(self.rpc.calls)
        self.assertIn("retired", library.describe(self.workspace, "pilot")["assignment"]["error"])

    def test_queued_worker_rechecks_current_effort_roster(self):
        self.worker(effort="max")
        self.assign()
        (self.workspace / ".de67/state/workspace.json").write_text(json.dumps({
            "worker_capabilities": [{"model": "gpt-6-luna", "reasoning_effort": "low"}]}))
        self.dispatcher.process_pending()
        self.assertFalse(self.rpc.calls)
        self.assertIn("no longer available", library.describe(self.workspace, "pilot")["assignment"]["error"])

'''+s[idx:];f.write_text(s)
f=c/'tests/test_policy_kernel.py';s=f.read_text();idx=s.index('\n\nCASES =');s=s[:idx]+'''
    def test_retired_choices_are_excluded_and_unknown_effort_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            config = workspace / '.de67/state/workspace.json'
            config.parent.mkdir(parents=True)
            config.write_text(json.dumps({'worker_capabilities': [
                {'model': 'gpt-5.6-terra', 'reasoning_effort': 'max'},
                {'model': 'gpt-6-sol', 'reasoning_effort': 'max'}]}))
            self.assertEqual(kernel.worker_model_choices(workspace), [
                {'model': 'gpt-6-sol', 'reasoning_effort': 'max'}])
            config.write_text(json.dumps({'worker_capabilities': [
                {'model': 'gpt-6-luna', 'reasoning_effort': 'ultra'}]}))
            with self.assertRaises(kernel.PolicyError):
                kernel.worker_model_choices(workspace)

'''+s[idx:];f.write_text(s)
