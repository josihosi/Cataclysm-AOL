import json
from pathlib import Path
import tempfile
import unittest
from work_context_provider import session_context

class SessionContextTests(unittest.TestCase):
    def test_exact_sessions_missing_status_and_reentry_freshness(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);session=root/'.userdata/openclaw_harness/bridge-sessions/branch-a'
            session.mkdir(parents=True);status=session/'status.json'
            evidence={'entrypoints':[str(status)], 'receipts':[{'bindings':{'session':str(session.parent/'historical-b')}}]}
            missing=session_context(root,evidence)['sessions'][0]
            self.assertFalse(missing['files']['status.json']['available'])
            status.write_text(json.dumps({'state':'ready','session_generation':0}))
            first=session_context(root,evidence)['sessions'][0]
            status.write_text(json.dumps({'state':'ready','session_generation':1,'session_descriptor':{'run_id':'new'}}))
            (session/'active-request.json').write_text('{"request_id":"pending-7"}')
            next=session_context(root,evidence)['sessions'][0]
            self.assertEqual(next['recorded_status']['session_generation'],1)
            self.assertEqual(next['recorded_status']['pending_request']['request_id'],'pending-7')
            self.assertNotEqual(first['files']['status.json']['sha256'],next['files']['status.json']['sha256'])
            self.assertIn('liveness',next['evidence_limit'])
            self.assertEqual(len(session_context(root,evidence)['sessions']),1)
            status.write_text('corrupt')
            self.assertFalse(session_context(root,evidence)['sessions'][0]['files']['status.json']['available'])

if __name__=='__main__':unittest.main()
