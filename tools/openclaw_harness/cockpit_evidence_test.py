"""CLI counterexamples for nested observations and giant single-line native logs."""
import hashlib
import base64
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CLI = Path(__file__).with_name("cockpit_file_bridge.py")


class CockpitEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.session = self.root / "session"
        (self.session / "responses").mkdir(parents=True)
        # Same causal shape as R-029-exploration-038: accepted confirmation, but
        # the resulting world still says unattempted. A positive receipt is not exit proof.
        self.response = {
            "ok": True, "receipt": {"accepted": True, "native_receipt": {
                "event": "surface_receipt", "run_id": "run-a", "request_id": "native-confirm",
                "requested_frame_id": "run-a:frame:2", "consuming_frame_id": "run-a:frame:2",
                "resulting_frame_id": "run-a:frame:3", "accepted": True,
                "action_id": "prompt.choose", "rejection_reason": ""}},
            "observation": {"run_id": "run-a", "observation_id": "run-a:frame:3",
                "frame_id": "run-a:frame:3", "surface_id": "run-a:surface:3",
                "surface": {"kind": "world", "facts": {"last_save_result": "unattempted",
                    "messages": json.dumps([{"text": "warm arm", "time": n} for n in range(5000)]),
                    "minimap": json.dumps({"cells": [{"terrain": "open air"}] * 5000})},
                    "actions": [{"id": "world.save_quit", "enabled": True}]},
                "compact_log": {"first_divergence": None, "contradictory_evidence": []}}}
        self.persist(self.response)

    def persist(self, response):
        raw = json.dumps(response).encode()
        self.digest = hashlib.sha256(raw).hexdigest()
        self.response_path = self.session / "responses" / "confirm.json"
        self.response_path.write_bytes(raw)
        (self.session / "responses" / "confirm.receipt.json").write_text(json.dumps({
            "request_id": "confirm", "binding_id": "bound-a", "response_sha256": self.digest,
            "response_artifact": "responses/confirm.json"}))

    def cli(self, *args, success=True):
        result = subprocess.run([sys.executable, str(CLI), *map(str, args)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        return json.loads(result.stdout)

    def status(self):
        return self.cli("response-status", "--session-dir", self.session, "--request-id", "confirm")

    def test_default_identifies_divergence_and_full_retrieval_agrees(self):
        status = self.status()
        projected = status["response"]
        self.assertTrue(projected["receipt"]["accepted"])
        self.assertEqual(projected["observation"]["surface"]["facts"]["last_save_result"], "unattempted")
        self.assertEqual(projected["observation"]["surface"]["actions"], self.response["observation"]["surface"]["actions"])
        self.assertNotIn("warm arm", json.dumps(status))
        full = self.cli(*status["retrieval"]["full"])
        self.assertEqual(full["response"], self.response)
        self.assertEqual(projected["receipt"]["native_receipt"], full["response"]["receipt"]["native_receipt"])
        field = self.cli("response-slice", "--session-dir", self.session, "--request-id", "confirm",
                         "--selector", "observation.surface.facts.messages", "--offset", 2, "--limit", 1)
        self.assertEqual(field["slice"], [{"text": "warm arm", "time": 2}])
        self.assertEqual(field["response_sha256"], self.digest)
        self.assertEqual(field["page"]["next_offset"], 3)

    def test_errors_mismatched_frames_and_corrupt_artifacts_stay_visible(self):
        self.response["ok"] = False
        self.response["error"] = "native_receipt_frame_mismatch"
        self.response["receipt"]["accepted"] = False
        native = self.response["receipt"]["native_receipt"]
        native.update(accepted=False, consuming_frame_id="run-a:frame:9", rejection_reason="frame_mismatch")
        self.response["observation"]["compact_log"]["first_divergence"] = "frame_mismatch"
        self.persist(self.response)
        status = self.status()
        self.assertFalse(status["response"]["ok"])
        self.assertEqual(status["response"]["error"], "native_receipt_frame_mismatch")
        self.assertEqual(status["response"]["receipt"]["native_receipt"], native)
        self.response_path.write_text("{}")
        self.assertEqual(self.status()["error"], "response_artifact_hash_mismatch")
        missing = self.cli("response-slice", "--session-dir", self.session, "--request-id", "missing", "--selector", "x")
        self.assertFalse(missing["ok"])

    def test_message_search_retains_failure_among_repeated_flavour(self):
        facts = self.response["observation"]["surface"]["facts"]
        messages = json.loads(facts["messages"])
        messages.insert(217, {"text": "Save failed: disk full", "time": "failure-time"})
        facts["messages"] = json.dumps(messages)
        self.persist(self.response)
        result = self.cli("response-slice", "--session-dir", self.session, "--request-id", "confirm",
                          "--selector", "observation.surface.facts.messages", "--contains", "save", "--limit", 1)
        self.assertEqual(result["slice"], [messages[217]])
        self.assertEqual(result["source_indices"], [217])
        self.assertEqual(result["filter"], {"contains": "save", "total": 5001, "matched": 1})

    def test_identity_is_never_previewed_and_invalid_utf8_is_losslessly_retrievable(self):
        self.response["receipt"]["native_receipt"]["request_id"] = "native:" + "r" * 600
        self.persist(self.response)
        self.assertEqual(self.status()["response"]["receipt"]["native_receipt"]["request_id"],
                         self.response["receipt"]["native_receipt"]["request_id"])
        path = self.root / "invalid.log"
        raw = b'ERROR : save failed \xff\n'
        path.write_bytes(raw)
        result = self.cli("log-query", "--path", path, "--event", "text", "--contains", "save")
        handle = result["rows"][0]["artifact"]
        args = [part for k, v in handle.items() for part in ("--" + k, v)]
        recovered = self.cli("record-artifact", *args)
        self.assertEqual(base64.b64decode(recovered["raw_base64"]), raw)
        receipt_path = self.session / "responses" / "confirm.receipt.json"
        receipt = json.loads(receipt_path.read_text())
        receipt["request_id"] = "other"
        receipt_path.write_text(json.dumps(receipt))
        self.assertEqual(self.status()["error"], "response_receipt_identity_mismatch")

    def test_debug_log_filters_before_rendering_and_exact_record_retrieval(self):
        path = self.root / "debug.log"
        descriptor = {"event": "surface_descriptor", "run_id": "run-a", "frame_id": "run-a:frame:3",
                      "kind": "world", "payload": self.response["observation"]["surface"]["facts"]}
        receipt = self.response["receipt"]["native_receipt"]
        records = [dict(descriptor, run_id="unrelated"), descriptor, receipt,
                   dict(receipt, accepted=False, rejection_reason="frame_mismatch", consuming_frame_id="wrong")]
        path.write_bytes(b"".join(b"01:14 INFO : openclaw_harness_semantic_step: " + json.dumps(r).encode() + b"\n" for r in records)
                         + b'01:15 ERROR : {broken\n' + b'01:16 ERROR : disk write failed\n')
        before = path.read_bytes()
        page = self.cli("log-query", "--path", path, "--run-id", "run-a", "--limit", 1)
        self.assertEqual(page["matched"], 3)
        self.assertEqual(page["unparsed_records"], 1)
        self.assertEqual(page["records_without_run_id"], 2)
        self.assertEqual(page["page"]["next_offset"], 1)
        self.assertNotIn("warm arm", json.dumps(page))
        self.assertEqual(page["rows"][0]["record"]["payload"]["last_save_result"], "unattempted")
        handle = page["rows"][0]["artifact"]
        args = [part for key, value in handle.items() for part in ("--" + key, value)]
        full = self.cli("record-artifact", *args)
        self.assertEqual(full["record"], descriptor)
        self.assertEqual(hashlib.sha256(full["raw"].encode()).hexdigest(), handle["sha256"])
        bad = self.cli("log-query", "--path", path, "--run-id", "run-a", "--event", "surface_receipt",
                       "--where", "accepted=false")
        self.assertEqual(bad["matched"], 1)
        self.assertEqual(bad["rows"][0]["record"]["consuming_frame_id"], "wrong")
        errors = self.cli("log-query", "--path", path, "--event", "unparsed")
        self.assertEqual(errors["rows"][0]["record"]["error"], "invalid_json_record")
        self.assertEqual(path.read_bytes(), before)
        path.write_bytes(b"corrupt" + before[7:])
        # The selected range is still valid after unrelated earlier bytes change.
        self.assertTrue(self.cli("record-artifact", *args)["ok"])
        path.write_bytes(b"")
        self.assertEqual(self.cli("record-artifact", *args, success=False)["error"], "record_artifact_hash_mismatch")

    def test_session_search_and_semantic_fields_do_not_dump_whole_responses(self):
        result = self.cli("log-query", "--session-dir", self.session, "--run-id", "run-a",
                          "--request-id", "confirm", "--where", 'observation.surface.facts.last_save_result="unattempted"',
                          "--select", "receipt.native_receipt", "--select", "observation.frame_id")
        self.assertEqual(result["matched"], 1)
        self.assertEqual(result["rows"][0]["record"]["receipt.native_receipt"], self.response["receipt"]["native_receipt"])
        self.assertNotIn("warm arm", json.dumps(result))
        absent = self.cli("log-query", "--session-dir", self.session, "--run-id", "different")
        self.assertEqual(absent["matched"], 0)
        self.response_path.write_text("{}")
        tampered = self.cli("log-query", "--session-dir", self.session, success=False)
        self.assertEqual(tampered["error"], "response_artifact_identity_or_hash_mismatch")


if __name__ == "__main__":
    unittest.main()
