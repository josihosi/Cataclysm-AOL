"""End-to-end proof for the published play_cli evidence route."""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cockpit_evidence
from evidence_display import DEFAULT_BYTES, encoded, recover


CLI = Path(__file__).with_name("play_cli.py")


class PlayCliEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.session = Path(self.temp.name)
        (self.session / "responses").mkdir()
        (self.session / "bridge.manifest.json").write_text(json.dumps({"binding_id": "binding-proof"}))
        self.cli_measurements = []
        self.run_id = "evidence-proof-run"
        self.process = "process-proof-1"
        self.request = "request-proof-7"
        self.actor = "npc-proof-4"
        self.log = self.session / "native-events.jsonl"
        verbose_trace = "trace-" + ("x" * 10000)
        records = [
            # Adjacent but unrelated: same actor/run, different request.
            {"event": "rejection", "run_id": self.run_id, "process_instance": self.process,
             "request_id": "adjacent-request", "actor_id": self.actor, "actor_name": "Mira",
             "payload": {"accepted": False, "rejection_reason": "unrelated"}},
            # The relevant tuple deliberately contains a contradiction.
            {"event": "request_accepted", "run_id": self.run_id, "process_instance": self.process,
             "request_id": self.request, "actor_id": self.actor, "actor_name": "Mira",
             "payload": {"accepted": True, "verbose_trace": verbose_trace}},
            {"event": "rejection", "run_id": self.run_id, "process_instance": self.process,
             "request_id": self.request, "actor_id": self.actor, "actor_name": "Mira",
             "payload": {"accepted": False, "rejection_reason": "frame_mismatch", "verbose_trace": verbose_trace}},
            {"event": "outcome", "run_id": self.run_id, "process_instance": self.process,
             "request_id": self.request, "actor_id": self.actor, "actor_name": "Mira",
             "payload": {"outcome": "rejected", "verbose_trace": verbose_trace}},
            # Same request/actor but a different process must not join this tuple.
            {"event": "outcome", "run_id": self.run_id, "process_instance": "other-process",
             "request_id": self.request, "actor_id": self.actor, "actor_name": "Mira",
             "payload": {"outcome": "accepted"}},
        ]
        self.log.write_bytes(b"".join(json.dumps(record, separators=(",", ":")).encode() + b"\n"
                                      for record in records))
        (self.session / "status.json").write_text(json.dumps({
            "binding_id": "binding-proof", "state": "ready",
            "session_descriptor": {"run_id": self.run_id},
        }))
        (self.session / "game-process.json").write_text(json.dumps({
            "binding_id": "binding-proof", "run_id": self.run_id,
            "log_paths": {
                "native_semantic_events": {"path": str(self.log), "scope": "run_bound"},
                "native_semantic_snapshot": {"path": str(self.session / "missing.json"), "scope": "run_bound"},
            },
        }))

    def cli(self, *arguments):
        command = [sys.executable, str(CLI), "--session", str(self.session), *map(str, arguments)]
        invocation_bytes = len(" ".join(command).encode())
        process = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(process.returncode, 0, process.stderr + process.stdout)
        output_bytes = len(process.stdout.encode())
        self.assertLessEqual(output_bytes, DEFAULT_BYTES)
        self.cli_measurements.append({"command": list(arguments),
                                      "invocation_bytes": invocation_bytes,
                                      "output_bytes": output_bytes})
        shown = json.loads(process.stdout)
        return recover(shown["presentation"]["full_evidence"]["sha256"])

    def test_published_exact_query_proves_projection_parity_identity_unknowns_and_bytes(self):
        identity = ("--run-id", self.run_id, "--process-instance", self.process,
                    "--request-id", self.request, "--actor-id", self.actor,
                    "--actor-name", "Mira")
        selectors = ("event,run_id,process_instance,request_id,actor_id,actor_name,"
                     "payload.payload.accepted,payload.payload.rejection_reason,payload.payload.outcome")

        # Initial compact projection: exact tuple, no guessed cross-process join.
        projected = self.cli("evidence", *identity, "--select", selectors, "--limit", 20)
        # Independent bulk retrieval for parity, not used to construct the projection.
        full = self.cli("evidence", *identity, "--limit", 20)

        self.assertEqual(projected["status"], "partial")
        self.assertTrue(projected["unavailable_sources"])
        self.assertEqual(projected["matched"], 3)
        self.assertEqual(full["matched"], 3)
        self.assertEqual(projected["scanned_records"], full["scanned_records"])
        self.assertEqual(projected["scanned_bytes"], full["scanned_bytes"])
        snapshot = recover(projected["snapshot"]["sha256"])
        self.assertEqual(snapshot["rows"], projected["rows"])
        self.assertEqual(snapshot["scanned_bytes"], projected["scanned_bytes"])

        full_by_id = {row["event_id"]: row for row in full["rows"]}
        projected_by_id = {row["event_id"]: row for row in projected["rows"]}
        self.assertEqual(set(projected_by_id), set(full_by_id))
        for event_id, row in projected_by_id.items():
            full_row = full_by_id[event_id]
            fields = row["fields"]
            for field in ("event", "run_id", "process_instance", "request_id", "actor_id", "actor_name"):
                self.assertEqual(fields[field], full_row[field])
            for field in ("accepted", "rejection_reason", "outcome"):
                expected = full_row["payload"]["payload"].get(field, {"unavailable": True})
                self.assertEqual(fields["payload.payload." + field], expected)

        relevant_payloads = [row["payload"]["payload"] for row in full["rows"]]
        self.assertIn(True, [payload.get("accepted") for payload in relevant_payloads])
        self.assertIn(False, [payload.get("accepted") for payload in relevant_payloads])
        self.assertIn("rejected", [payload.get("outcome") for payload in relevant_payloads])
        link = next(link for link in full["links"]
                    if link["run_id"] == self.run_id and link["process_instance"] == self.process
                    and link["request_id"] == self.request)
        self.assertEqual(link["status"], "complete")

        # Every cited row resolves to the original bytes and a retained copy.
        handles = []
        citation_returned_bytes = 0
        for row in projected["rows"]:
            source = row["source"]
            handles.append(source)
            raw = Path(source["path"]).read_bytes()[source["offset"]:
                                                   source["offset"] + source["length"]]
            self.assertEqual(hashlib.sha256(raw).hexdigest(), source["sha256"])
            artifact = cockpit_evidence.record_artifact(
                Path(source["path"]), source["offset"], source["length"], source["sha256"], [])
            self.assertTrue(artifact["ok"])
            citation_returned_bytes += len(artifact["raw"].encode())
            retained = recover(source["retained_raw"]["sha256"])
            self.assertEqual(base64.b64decode(retained["raw_base64"]), raw)

        # A source mutation invalidates the path citation, while the retained snapshot copy stays exact.
        original = self.log.read_bytes()
        self.log.write_bytes(original.replace(b"frame_mismatch", b"frame_mismatcX", 1))
        self.assertFalse(cockpit_evidence.record_artifact(
            Path(handles[1]["path"]), handles[1]["offset"], handles[1]["length"],
            handles[1]["sha256"], [])["ok"])
        retained = recover(handles[1]["retained_raw"]["sha256"])
        self.assertEqual(base64.b64decode(retained["raw_base64"]),
                         original[handles[1]["offset"]:handles[1]["offset"] + handles[1]["length"]])

        # S-EFF-COMPARE: the selected fields settle the decision, so no
        # operational follow-up read is necessary. The three exact reads above
        # are separately accounted as citation-integrity verification.
        projection_call, bulk_call = self.cli_measurements
        projection_io_bytes = projection_call["invocation_bytes"] + projection_call["output_bytes"]
        bulk_io_bytes = bulk_call["invocation_bytes"] + bulk_call["output_bytes"]
        efficiency = {
            "subprocess_cli_turns": len(self.cli_measurements),
            "projection_invocation_bytes": projection_call["invocation_bytes"],
            "projection_output_bytes": projection_call["output_bytes"],
            "operational_follow_up_reads": 0,
            "operational_follow_up_returned_bytes": 0,
            "targeted_decision_route_bytes": projection_io_bytes,
            "citation_integrity_verification_reads": len(handles),
            "citation_integrity_returned_bytes": citation_returned_bytes,
            "combined_targeted_route_bytes": projection_io_bytes + citation_returned_bytes,
            "bulk_invocation_bytes": bulk_call["invocation_bytes"],
            "bulk_output_bytes": bulk_call["output_bytes"],
            "bulk_route_bytes": bulk_io_bytes,
            "in_process_follow_up_calls": len(handles),
            "projected_result_bytes": len(encoded(projected)),
            "bulk_result_bytes": len(encoded(full)),
        }
        self.assertEqual(efficiency["subprocess_cli_turns"], 2)
        self.assertEqual(efficiency["operational_follow_up_reads"], 0)
        self.assertEqual(efficiency["citation_integrity_verification_reads"], 3)
        self.assertGreater(efficiency["citation_integrity_returned_bytes"], 0)
        self.assertEqual(efficiency["combined_targeted_route_bytes"],
                         efficiency["targeted_decision_route_bytes"] +
                         efficiency["citation_integrity_returned_bytes"])
        self.assertEqual(efficiency["bulk_route_bytes"],
                         efficiency["bulk_invocation_bytes"] + efficiency["bulk_output_bytes"])
        if os.environ.get("CAOL_EVIDENCE_PRINT_METRICS"):
            print("S-EFF-COMPARE " + json.dumps(efficiency, sort_keys=True))


if __name__ == "__main__":
    unittest.main()
