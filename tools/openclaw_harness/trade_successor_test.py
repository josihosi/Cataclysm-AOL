"""Real R051 oversized receipt/descriptor pairs, no game/input replay."""
import copy
import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
import startup_harness as harness

FIXTURE = Path(__file__).parent / 'fixtures/controls/r067_trade_successors.json.gz'

def original_cases():
    return json.loads(gzip.decompress(FIXTURE.read_bytes()))['cases']

class TradeSuccessorTest(unittest.TestCase):
    def dispatch(self, case, *, mutation=None, missing=False):
        prepare_start = time.perf_counter()
        case = copy.deepcopy(case)
        before, after, receipt = (case[key] for key in ('before', 'after', 'receipt'))
        session = receipt['request_id'].split(':')[1]
        action = receipt['action_id']
        target = None
        candidates = [None, *{r['stable_id'] for r in before['valid_actions'] if r['id'] == action}]
        for candidate in candidates:
            digest = hashlib.sha256(json.dumps({'stable_id': candidate, 'parameters': {}}, sort_keys=True,
                                               separators=(',', ':')).encode()).hexdigest()[:16]
            if receipt['request_id'].endswith(':' + digest):
                target = candidate
                break
        else:
            self.fail('original target not recoverable')
        if mutation:
            mutation(receipt, after)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'source.log'
            rows = [row for row in json.loads(gzip.decompress(FIXTURE.read_bytes()))['context']
                    if row['sequence'] <= receipt['sequence']][-64:]
            rows = [receipt if row['sequence'] == receipt['sequence'] else
                    after if row.get('frame_id') == after['frame_id'] else row for row in rows]
            if missing:
                rows = [row for row in rows if row.get('frame_id') != after['frame_id']]
            source.write_text(''.join(harness.SEMANTIC_STEP_PREFIX + json.dumps(row) + '\n' for row in rows))
            start = time.perf_counter()
            self.last_fixture_preprocess_seconds = start - prepare_start
            self.last_source_bytes = source.stat().st_size
            with patch.object(harness, 'semantic_step_source_trace', return_value=source), \
                 patch.object(harness, 'semantic_wake_pipe_contract', return_value={'status': 'bound', 'path': 'test'}), \
                 patch.object(harness, 'write_semantic_wake_pipe', return_value=1) as wake:
                result = harness.execute_semantic_act(run_dir=root, profile='none', run_id=before['run_id'],
                    trace_start_offset=0, pid=17, session_id=session, frame_id=before['frame_id'],
                    action_id=action, stable_id=target, observed_frame=before,
                    transition_timeout_seconds=0.02, observe_interval_seconds=0.001)
            elapsed = time.perf_counter() - start
            self.assertEqual(wake.call_count, 1)
            return result, after, elapsed

    def test_all_eight_original_successors_survive_oversized_channel_reordering(self):
        cases = original_cases()
        for case in cases:
            with self.subTest(sequence=case['receipt']['sequence']):
                result, after, _ = self.dispatch(case)
                self.assertTrue(result['accepted'], result)
                self.assertEqual(result['next_frame']['frame_id'], after['frame_id'])
                self.assertEqual(result['next_frame']['valid_actions'], after['valid_actions'])
                self.assertEqual(result['native_receipt']['request_id'], case['receipt']['request_id'])

    def test_missing_successor_does_not_replay_or_invent(self):
        case = original_cases()[0]
        result, _, _ = self.dispatch(case, missing=True)
        self.assertFalse(result['accepted'])
        self.assertEqual(result['reason'], 'native_surface_successor_timeout')

    def test_wrong_requested_owner_is_not_authenticated(self):
        case = original_cases()[0]
        for field in ('requested_surface_id', 'requested_frame_id', 'action_id'):
            result, _, _ = self.dispatch(case, mutation=lambda receipt, after: receipt.update({field: 'foreign'}))
            self.assertFalse(result['accepted'])
            self.assertEqual(result['reason'], 'native_surface_receipt_correlation_mismatch')

class FinishAndControlClarityTest(unittest.TestCase):
    def client(self):
        import play_cli
        client = object.__new__(play_cli.PlayerClient)
        client.state = {"sealed_terminal": {"observation_id": "exact-frame", "stop_reason": "saved", "unused_authority": "released"}}
        return client

    def test_checkpoint_wrong_shape_does_not_submit_or_quit(self):
        client = self.client()
        for value in ({"confirmed_turn": 5248280}, None, 5248280, True):
            witness = {"citations": [{"checks": {"value.surface.facts.last_save_checkpoint": value}}]}
            with patch.object(client, "submit") as submit:
                result = client.finish(witness, 0)
                self.assertEqual(result["error"], "witness_checkpoint_requires_string")
                self.assertEqual(result["expected_shape"]["type"], "string")
                submit.assert_not_called()
                self.assertEqual(client.state["sealed_terminal"]["observation_id"], "exact-frame")

    def test_exact_checkpoint_string_is_submitted_once_without_coercion(self):
        client = self.client()
        value = '{"confirmed_turn":5248280}'
        witness = {"citations": [{"checks": {"value.surface.facts.last_save_checkpoint": value}}]}
        with patch.object(client, "submit", return_value={"ok": True}) as submit:
            self.assertTrue(client.finish(witness, 0)["ok"])
            submit.assert_called_once_with({"action": "run.finish", **client.state["sealed_terminal"], "witness": witness}, 0)

    def test_bundle_checkpoint_shape_is_checked(self):
        client = self.client()
        witness = {"schema": "caol-playtest-witness-bundle-v1", "claims": [{"statement": {
            "citations": [{"checks": {"value.surface.facts.last_save_checkpoint": {}}}]}}]}
        with patch.object(client, "submit") as submit:
            self.assertFalse(client.finish(witness, 0)["ok"])
            submit.assert_not_called()

    def test_finish_collection_distinguishes_pending_cleanup_from_failed_and_complete(self):
        client = self.client()
        client.state = {"finished": True}
        with tempfile.TemporaryDirectory() as temp:
            client.session = Path(temp)
            with patch.object(client, "_reentry_pending", return_value=False):
                for state, expected, ok in [("terminalizing", "finishing", True),
                                             ("terminalization_failed", "cleanup_failed", False),
                                             ("safe_to_cleanup", "finished", True)]:
                    (client.session / "status.json").write_text(json.dumps({"state": state, "terminalization": {"cleanup": "observed"}}))
                    result = client.collect()
                    self.assertEqual(result["state"], expected)
                    self.assertEqual(result["ok"], ok)
                    self.assertEqual(result["cleanup_pending"], expected == "finishing")

    def test_nested_close_is_not_labelled_basket_commit_and_balance_is_native_action(self):
        from gameplay_display import _plain_controls
        nested = _plain_controls([{"id": "inventory.commit", "stable_id": "", "label": "Close contents", "enabled": True}])
        self.assertTrue("Close contents" in nested)
        self.assertFalse(("Confirm highlighted" in nested or "Confirm marked" in nested))
        trade = _plain_controls([{"id": "trade.auto_balance", "stable_id": "", "label": "Auto Balance with highlighted group", "enabled": True}])
        self.assertTrue("play act trade.auto_balance" in trade)

    def test_selected_inspection_retains_exact_artifact_authentication_and_unknowns(self):
        import play_cli
        client = self.client()
        client.state["last_request_id"] = "accepted-original"
        client.binding = "bound"
        client.session = Path("/not-a-game")
        response = {"observation": {"surface": {"facts": {
            "selected_items": '{"coin-1":{"count":1}}', "player_offer_value": "$764.86"}}}}
        receipt = {"ok": True, "receipt": {"binding_id": "bound", "response_sha256": "verified-hash"}}
        with patch.object(play_cli.Bridge, "response_status", return_value=receipt), \
             patch.object(play_cli.Bridge, "response_artifact", return_value={"ok": True, "response": response}) as artifact, \
             patch.object(client, "submit") as submit:
            result = client.inspect("observation.surface.facts", 0, None, None,
                                    selected_fields=["selected_items.coin-1.count,player_offer_value,unavailable_actor"])
            self.assertEqual(result["response_sha256"], "verified-hash")
            fields = result["projection"]["selectors"]
            self.assertEqual(fields["observation.surface.facts.selected_items.coin-1.count"]["value"], 1)
            self.assertFalse(fields["observation.surface.facts.unavailable_actor"]["available"])
            artifact.assert_called_once_with(client.session, "accepted-original", "verified-hash")
            submit.assert_not_called()
        receipt["receipt"]["binding_id"] = "foreign"
        with patch.object(play_cli.Bridge, "response_status", return_value=receipt), \
             patch.object(play_cli.Bridge, "response_artifact") as artifact:
            self.assertEqual(client.inspect("", 0, None, None, selected_fields=["observation"])["error"],
                             "response_binding_mismatch")
            artifact.assert_not_called()

if __name__ == '__main__':
    unittest.main()
