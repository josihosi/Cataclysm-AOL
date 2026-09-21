"""Keep the delivered continuation commands aligned with PlayerClient's parser."""
from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import play_cli


class ContinuationGuidanceTest(unittest.TestCase):
    def session(self, root: Path, name: str) -> Path:
        session = root / name
        session.mkdir()
        (session / 'requests').mkdir()
        (session / 'responses').mkdir()
        (session / 'controls').mkdir()
        (session / 'bridge.manifest.json').write_text(json.dumps({'binding_id': 'bound'}))
        (session / 'status.json').write_text(json.dumps({'state': 'ready', 'binding_id': 'bound'}))
        return session

    def invoke(self, argv: list[str]) -> dict:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            play_cli.main(argv)
        return json.loads(output.getvalue())

    def test_documented_wait_collect_resume_and_inspect_shapes_reach_the_client(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            session = self.session(root, 'wait')
            wait = self.invoke([
                '--session', str(session), 'wait', '--target-delta-game-minutes', '5',
                '--duration-action', 'wait.5m', '--bound-maximum', '5',
                '--bound-basis', 'scheduler_boundary', '--bound-source', 'chosen observation window',
            ])
            self.assertEqual(wait['state'], 'pending')
            session = self.session(root, 'readers')
            collect = self.invoke([
                '--session', str(session), '--wait-seconds', '3', 'collect',
                '--request-id', 'request-1',
            ])
            self.assertEqual(collect['error'], 'request_artifact_unavailable')
            resume = self.invoke([
                '--session', str(session), 'resume', '--request-id', 'request-1',
            ])
            self.assertEqual(resume['error'], 'request_artifact_unavailable')
            inspect = self.invoke([
                '--session', str(session), 'inspect', 'observation.surface.actions',
                '--request-id', 'request-1',
            ])
            self.assertEqual(inspect['error'], 'response_not_available_or_stale')

    def test_guidance_does_not_restore_the_superseded_tab_or_wrong_global_option_order(self) -> None:
        root = Path(__file__).resolve().parents[2]
        live = (root / '.agents/skills/caol-harness/references/live-operation.md').read_text()
        controls = (Path(__file__).resolve().parent / 'CONTROL_LOOKUP.md').read_text()
        self.assertNotIn('collect --wait-seconds', live)
        self.assertIn('--wait-seconds REMAINING_SECONDS collect --request-id REQUEST_ID', live)
        self.assertNotIn('then usually `Tab`', controls)
        self.assertNotIn('pass turns with `Tab`', controls)


if __name__ == '__main__':
    unittest.main()
