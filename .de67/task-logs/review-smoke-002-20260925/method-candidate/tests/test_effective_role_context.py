"""Common instruction ownership at the three actual prompt-producing boundaries."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from instruction_context import FALLBACK_GUIDANCE
from deadline_harness import DeadlineHarness
from coordinator_supervisor import coordinator_prompt, mutation_reviewer_prompt, MutationGate
from policy_kernel import unbound_worker_spawns
from context_library import prepare


class EffectiveRoleContextTests(unittest.TestCase):
    def test_baseline_present_absent_and_prepared_or_default_worker(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            de67 = workspace / '.de67'
            (de67 / 'state').mkdir(parents=True)
            (de67 / 'work-ledger.md').write_text('- [ ] R-CAMP — Prove a native camp.\n')
            (de67 / 'FS.md').write_text(
                '<!-- DE67:DFS-SLICE:BEGIN id=R-CAMP-S001 claim=R-CAMP -->\n'
                '- [ ] 🔴 R-CAMP — Prove a native camp.\n'
                '<!-- DE67:DFS-SLICE:END id=R-CAMP-S001 claim=R-CAMP -->\n')
            (de67 / 'WEC.md').write_text('Preserve the owner outcome.')
            state = workspace / 'clock.sqlite3'
            with DeadlineHarness(state) as harness:
                harness.start_task('project', 'camp-task', 'R-CAMP', 100, now=1)
            def prompts():
                call = unbound_worker_spawns(workspace, state, 'project')[0]
                return [
                    coordinator_prompt(workspace, state, 'project', 'coordinator', None),
                    mutation_reviewer_prompt(workspace, state, 'project', MutationGate('owner-suggestion', 'test', None)),
                    Path(call['dispatch_packet']['path']).read_text(),
                ]
            for text in prompts():
                self.assertEqual(text.count(FALLBACK_GUIDANCE), 1)
                self.assertNotIn('capture build/test output in logs', text)  # removed duplicate worker contract
            baseline = workspace / 'AGENTS.md'
            baseline.write_text(FALLBACK_GUIDANCE)
            config = {'guidance': {'source': str(baseline), 'sha256': hashlib.sha256(baseline.read_bytes()).hexdigest(), 'effective': True}}
            (de67 / 'state/workspace.json').write_text(json.dumps(config))
            for custom in (False, True):
                if custom:
                    prepare(workspace, 'camp-task', 'Use the native camp route; no setup proof.')
                for text in prompts():
                    # The audited runtime input supplies the baseline once; producers do not repeat it.
                    self.assertNotIn(FALLBACK_GUIDANCE, text)
                    self.assertEqual((baseline.read_text() + text).count(FALLBACK_GUIDANCE), 1)
            baseline.unlink()
            for text in prompts():
                self.assertEqual(text.count(FALLBACK_GUIDANCE), 1)
            self.assertIn('if none is available', FALLBACK_GUIDANCE)


if __name__ == '__main__':
    unittest.main()
