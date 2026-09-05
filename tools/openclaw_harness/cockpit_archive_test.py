import gc
import io
from unittest.mock import patch
import startup_harness
import cockpit
from cockpit_macro_interruption_test import MacroInterruptionTest
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from cockpit_archive import (Archive, ArchiveMap, ArchiveSequence, json_chunks, resolve_wire,
                             value_digest, write_json_stream)
from cockpit import CockpitRunChannel, CockpitService
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge
from cockpit_witness_test import CHARTER, frame
from playtest_witness import WitnessError, build_evidence_journal, validate_witness_statement


def statement():
    return {"verdict": "proved", "smallest_supported_claim": "Minute 100 was observed.",
            "causal_account": "The native frame supplies the minute.",
            "citations": [{"citation_id": "J0002", "meaning": "native minute", "checks": {"value.game_minutes": 100}}],
            "recommended_disposition": "accept", "evidence_ceiling": "focused"}


class ArchiveTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.archive = Archive(self.directory / "cockpit-evidence.sqlite", run_id="run-a", binding_id="binding-a")
        self.addCleanup(self.archive.close)

    def service(self, archived=True):
        channel = CockpitRunChannel(frame, binding_id="binding-a", archive=self.archive if archived else None,
             witness_charter=CHARTER, witness_identity={"scenario_id": "scenario-a", "source_identity": "source-a", "executable_identity": "exe-a"})
        return CockpitService(run_channel=channel)

    def test_observe_seal_finish_values_order_and_hash_match_legacy(self):
        sealed = []
        for archived in (False, True):
            service = self.service(archived)
            observed = service.call({"action": "game.observe"})["result"]
            response = service.call({"action": "run.witness", "observation_id": observed["observation_id"],
                                     "stop_reason": "settled", "unused_authority": "none"})
            self.assertTrue(response["ok"], response)
            journal = response["result"]["evidence_journal"]
            sealed.append(json.loads("".join(json_chunks(journal))))
            finished = service.call({"action": "run.finish", "observation_id": observed["observation_id"],
                                    "stop_reason": "settled", "unused_authority": "none", "witness": statement()})
            self.assertTrue(finished["ok"], finished)
            if archived:
                wire = self.archive.wire(finished)
                self.assertIn("artifact_reference_envelope", wire)
                self.assertLess(len(json.dumps(wire)), len("".join(json_chunks(finished))) + 5000)
                recovered = resolve_wire(wire, directory=self.directory, binding_id="binding-a")
                sequence = recovered["result"]["action_observation_sequence"]
                self.addCleanup(sequence.archive.close)
                self.assertEqual(sequence[0]["kind"], "observation")
                self.assertEqual(recovered["result"]["stop_detail"]["evidence_journal"]["entries"][1]["citation_id"], "J0002")
        self.assertEqual(sealed[0], sealed[1])

    def test_canonical_hash_handles_native_unicode_and_sequence_exactly(self):
        sequence = self.archive.sequence()
        for value in ({"z": "ö\n", "a": [True, None, 1.25]}, {"x": "{\"native\":true}"}):
            sequence.append(value)
        legacy = list(sequence)
        expected = hashlib.sha256(("label:" + json.dumps(legacy, sort_keys=True, ensure_ascii=False, separators=(",", ":"))).encode()).hexdigest()
        self.assertEqual(value_digest("label", sequence), expected)
        path = self.directory / "export.json"
        write_json_stream(path, sequence)
        self.assertEqual(json.loads(path.read_text(encoding="utf-8")), legacy)

    def test_old_authority_flags_and_payloads_are_exact_without_hot_history(self):
        records = ArchiveMap(self.archive, "observations")
        for index in range(50):
            records[str(index)] = {"used": False, "actions": {"world.wait"}, "issuing_frame": {"frame_id": str(index)}}
            records[str(index)]["used"] = True
        self.assertEqual(records.hot_key, "49")
        self.assertTrue(records["0"]["used"])
        self.assertEqual(records["0"]["actions"], {"world.wait"})
        self.assertEqual(records.hot_key, "49")
        records["0"]["macro_stop_decision"] = True
        self.assertTrue(records["0"]["macro_stop_decision"])
        records.revoke_all()
        self.assertTrue(all(record["used"] for record in records.values()))

    def test_tamper_and_wrong_binding_reject_before_citation_use(self):
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        response = service.call({"action": "run.witness", "observation_id": observed["observation_id"], "stop_reason": "settled", "unused_authority": "none"})
        wire = self.archive.wire(response)
        with self.assertRaisesRegex(ValueError, "binding"):
            resolve_wire(wire, directory=self.directory, binding_id="other")
        journal = response["result"]["evidence_journal"]
        entries = journal["entries"]
        row = entries[1]
        row["value"]["value"]["game_minutes"] = 999
        with self.archive.connection:
            self.archive.connection.execute("UPDATE records SET value=? WHERE stream=? AND seq=1", (self.archive.encode(row), entries.stream))
        with self.assertRaisesRegex(WitnessError, "digest_mismatch"):
            validate_witness_statement(charter=CHARTER, journal=journal, statement=statement())
        with self.assertRaisesRegex(ValueError, "digest_mismatch"):
            resolve_wire(wire, directory=self.directory, binding_id="binding-a")

    def test_bridge_pages_exact_archived_citation_without_full_response_decode(self):
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        response = service.call({"action": "run.witness", "observation_id": observed["observation_id"], "stop_reason": "settled", "unused_authority": "none"})
        wire = self.archive.wire(response)
        (self.directory / "responses").mkdir()
        raw = json.dumps(wire).encode()
        (self.directory / "responses/r.json").write_bytes(raw)
        receipt = {"request_id": "r", "binding_id": "binding-a", "response_sha256": hashlib.sha256(raw).hexdigest(), "response_artifact": "responses/r.json"}
        (self.directory / "responses/r.receipt.json").write_text(json.dumps(receipt))
        result = Bridge.response_slice(self.directory, "r", "result.evidence_journal.entries", 1, 1)
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["slice"][0]["citation_id"], "J0002")
        self.assertEqual(result["slice"][0]["value"]["value"]["game_minutes"], 100)
        self.assertEqual(result["page"]["next_offset"], 2)

    def test_final_export_and_scenario_report_preserve_lazy_history(self):
        service = self.service()
        service.call({"action": "game.observe"})
        report = {"action_observation_sequence": service.run_channel._transcript}
        final_path = self.directory / "cockpit.live.final.json"
        startup_harness.finalize_cockpit_live_session(self.directory, 42, report, cleanup_process=False)
        wire = json.loads(final_path.with_suffix(".ref.json").read_text())
        recovered = resolve_wire(wire, directory=self.directory, binding_id="binding-a", exported_path=final_path)
        self.addCleanup(recovered["action_observation_sequence"].archive.close)
        self.assertEqual(json.loads(final_path.read_text(encoding="utf-8")), json.loads("".join(json_chunks(recovered))))
        scenario = {"steps": [{"cockpit_live_session": {"final": recovered}}]}
        output = self.directory / "probe.report.json"
        startup_harness.write_json(output, scenario)
        startup_harness.write_json(output, scenario)  # Existing mutable report writer may overwrite.
        self.assertEqual(json.loads(output.read_text()), json.loads("".join(json_chunks(scenario))))

    def test_live_eof_exports_actionless_finished_response_without_materializing(self):
        service = self.service()
        service.call({"action": "game.observe"})
        output = io.StringIO()
        self.assertEqual(startup_harness.serve_cockpit_live(service, io.StringIO(), output), 1)
        wire = json.loads(output.getvalue())
        self.assertIn("artifact_reference_envelope", wire)
        restored = resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        self.assertIn("finished", str(restored.get("state", restored.get("final", {}))))

    def test_live_envelope_authenticates_scalar_cleanup_and_export_identity(self):
        sequence = self.archive.sequence()
        sequence.append({"fact": "native"})
        response = {"state": "finished", "cleanup": {"status": "already_exited"}, "entries": sequence}
        original = self.archive.wire(response)
        for field, value in (("state", "forged"), ("cleanup", {"status": "killed"})):
            wire = json.loads(json.dumps(original))
            wire[field] = value
            with self.assertRaisesRegex(ValueError, "reconstruction"):
                resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        for field, value in (("path", str(self.directory / "other.json")), ("sha256", "wrong"), ("bytes", 0)):
            wire = json.loads(json.dumps(original))
            wire["artifact_reference_envelope"]["exported_response"][field] = value
            with self.assertRaisesRegex(ValueError, "export_path|export_identity"):
                resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        Path(original["artifact_reference_envelope"]["exported_response"]["path"]).write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "export_identity"):
            resolve_wire(original, directory=self.directory, binding_id="binding-a")


class ArchivedMacroInterruptionTest(MacroInterruptionTest):
    """Exercise the existing native-owner interruption contract on disk state."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        original = cockpit.CockpitRunChannel
        counter = [0]
        outer = self
        class ArchivedChannel(original):
            def __init__(self, read_frame, *args, **kwargs):
                counter[0] += 1
                directory = Path(outer.temp.name) / str(counter[0])
                directory.mkdir()
                archive = Archive(directory / "cockpit-evidence.sqlite", run_id=read_frame()["run_id"],
                                  binding_id=kwargs.get("binding_id", ""))
                outer.addCleanup(archive.close)
                kwargs["archive"] = archive
                super().__init__(read_frame, *args, **kwargs)
        patcher = patch.object(cockpit, "CockpitRunChannel", ArchivedChannel)
        patcher.start()
        self.addCleanup(patcher.stop)


if __name__ == "__main__":
    unittest.main()
