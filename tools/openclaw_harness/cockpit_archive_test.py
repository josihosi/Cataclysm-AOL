import gc
import io
import contextlib
import os
import sqlite3
from unittest.mock import patch
import startup_harness
import cockpit
from cockpit_macro_interruption_test import MacroInterruptionTest
import hashlib
import json
from pathlib import Path
import tempfile
import tracemalloc
import unittest
from unittest import mock

from cockpit_archive import (Archive, ArchiveMap, ArchiveSequence, json_chunks, resolve_wire,
                             value_digest, write_json_stream)
from cockpit import CockpitRunChannel, CockpitService
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge
from cockpit_memory_profile import PROFILE_PATH_ENV, snapshot as memory_snapshot, released as memory_released
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

    def test_reobserved_consumed_world_frame_cannot_dispatch_twice(self):
        for archived in (False, True):
            with self.subTest(archived=archived):
                current = {"event": "surface_descriptor", "schema_version": 1,
                           "run_id": "run-a", "frame_id": "world:1", "surface_id": "world",
                           "kind": "world", "breadcrumbs": ["World"], "payload": {},
                           "valid_actions": [{"id": "world.move.west", "stable_id": "",
                                              "label": "West", "enabled": True}]}
                calls = []

                def dispatch(issuing, action):
                    calls.append(issuing["frame_id"])
                    successor = {**current, "frame_id": "world:2" if len(calls) == 1 else "world:3"}
                    return {"native_receipt": {
                        "run_id": "run-a", "requested_run_id": "run-a", "accepted": True,
                        "requested_frame_id": issuing["frame_id"], "consuming_frame_id": issuing["frame_id"],
                        "requested_surface_id": "world", "consuming_surface_id": "world", "action_id": action},
                        "next_frame": successor}

                channel = CockpitRunChannel(lambda: current, dispatch,
                    archive=self.archive if archived else None, binding_id="binding-a")
                service = CockpitService(run_channel=channel)
                first = service.call({"action": "game.observe"})["result"]
                action = {"action": "game.act", "observation_id": first["observation_id"],
                          "action_id": "world.move.west"}
                self.assertTrue(service.call(action)["ok"])
                self.assertEqual(service.call({"action": "game.observe"})["result"]["observation_id"], "world:1")
                self.assertEqual(service.call(action)["error"], "duplicate_submission")
                self.assertEqual(calls, ["world:1"])
                current["frame_id"] = "world:2"
                fresh = service.call({"action": "game.observe"})["result"]
                self.assertTrue(service.call({**action, "observation_id": fresh["observation_id"]})["ok"])
                self.assertEqual(calls, ["world:1", "world:2"])

    def test_ordinary_scenario_reuses_its_published_final_without_history_rescans(self):
        run_dir = self.directory
        (run_dir / "process.json").write_text(json.dumps({"run_id": "run-a"}), encoding="utf-8")
        (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
            json.dumps({"run_id": "run-a"}), encoding="utf-8")
        final_path = startup_harness.cockpit_live_final_path(run_dir, "ordinary")
        process = {"pid": 42, "run_id": "run-a", "alive": True}
        channel = CockpitRunChannel(frame, binding_id="binding-a", archive=self.archive,
            read_process_state=lambda: dict(process),
            finalize_session=lambda report: startup_harness.finalize_cockpit_live_session(
                run_dir, 42, report, cleanup_process=False, final_path=final_path))
        service = CockpitService(run_channel=channel)
        real_serve = startup_harness.serve_cockpit_live
        with contextlib.ExitStack() as guards:
            def serve(_service, _stdin, output):
                observed = _service.call({"action": "game.observe"})["result"]
                process["alive"] = False
                observed = _service.call({"action": "game.observe"})["result"]
                request = {"action": "run.finish", "observation_id": observed["observation_id"],
                           "stop_reason": "outcome_reported", "unused_authority": "released",
                           "outcome": {"summary": "Ordinary result retained", "verdict": "positive"}}
                status = real_serve(_service, io.StringIO(json.dumps(request) + "\n"), output)
                self.assertEqual(status, 0, output.getvalue())
                guards.enter_context(mock.patch.object(startup_harness, "resolve_wire",
                    side_effect=AssertionError("producer recovered its own final again")))
                guards.enter_context(mock.patch.object(ArchiveSequence, "__iter__",
                    side_effect=AssertionError("ordinary terminal metadata enumerated history")))
                return status

            with contextlib.redirect_stdout(io.StringIO()), \
                 mock.patch.object(startup_harness, "semantic_step_trace_start_for_source", return_value=0), \
                 mock.patch.object(startup_harness, "cockpit_controller_binding_id", return_value="binding-a"), \
                 mock.patch.object(startup_harness, "semantic_wake_pipe_contract", return_value={"status": "bound"}), \
                 mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=None), \
                 mock.patch.object(startup_harness, "open_cockpit_game_service", return_value=service), \
                 mock.patch.object(startup_harness, "current_owned_process_generation",
                                   return_value={"pid": 42, "status": "exited", "expected": {"pid": 42}}), \
                 mock.patch.object(startup_harness, "serve_cockpit_live", side_effect=serve):
                reports = startup_harness.execute_probe_steps(42, run_dir, [{
                    "kind": "cockpit_live_session", "label": "ordinary",
                    "r014_step_evidence": True, "stop_after_live_session": True,
                }], profile="profile-a", world="Test00")
            self.assertEqual(len(reports), 1)
            final = reports[0]["cockpit_live_session"]["final"]
            self.assertEqual(final["stop_detail"]["gameplay_outcome"]["verdict"], "positive")
            self.assertIs(final["action_observation_sequence"], channel._transcript)
            self.assertNotIn("target_receipt", final["stop_detail"])
            verdict, issues = startup_harness.cockpit_live_session_step_verdict(
                reports[0], wec_authority={}, report_binding_id="binding-a", scenario_source_sha256="source-a")
            self.assertTrue(verdict.startswith("yellow"))  # Reporting never mints formal proof.
            self.assertIn("cockpit_target_not_explicitly_proved", issues)
        # An independent consumer still verifies the exact published archive.
        from cockpit_report_reference import load_report
        _, _, recovered = load_report(final_path)
        self.addCleanup(recovered["action_observation_sequence"].archive.close)
        self.assertEqual(recovered["stop_detail"]["gameplay_outcome"], final["stop_detail"]["gameplay_outcome"])
        final_path.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "identity_mismatch"):
            load_report(final_path)

    def test_owned_final_reuse_preserves_identity_publication_and_cold_integrity_checks(self):
        from types import SimpleNamespace
        final_path = self.directory / "ordinary.final.json"
        channel = CockpitRunChannel(frame, binding_id="binding-a", archive=self.archive,
            read_process_state=lambda: {"pid": 42, "run_id": "run-a", "alive": False},
            finalize_session=lambda report: startup_harness.finalize_cockpit_live_session(
                self.directory, 42, report, cleanup_process=False, final_path=final_path))
        service = CockpitService(run_channel=channel)
        observed = service.call({"action": "game.observe"})["result"]
        with mock.patch.object(startup_harness, "current_owned_process_generation",
                               return_value={"pid": 42, "status": "exited", "expected": {"pid": 42}}):
            result = service.call({"action": "run.finish", "observation_id": observed["observation_id"],
                                   "stop_reason": "outcome_reported", "unused_authority": "released",
                                   "outcome": {"summary": "Reported negative result", "verdict": "negative"}})
        self.assertTrue(result["ok"], result)
        for run_id, binding in (("foreign", "binding-a"), ("run-a", "foreign")):
            with self.subTest(run_id=run_id, binding=binding), self.assertRaisesRegex(
                    ValueError, "owned_cockpit_final_identity_mismatch"):
                startup_harness.read_completed_cockpit_final(
                    service, final_path, run_id=run_id, binding_id=binding)
        stale_owner = SimpleNamespace(run_channel=SimpleNamespace(status=lambda: {
            **channel.status(), "run_id": "replacement-run"}))
        with self.assertRaisesRegex(ValueError, "owned_cockpit_final_identity_mismatch"):
            startup_harness.read_completed_cockpit_final(
                stale_owner, final_path, run_id="run-a", binding_id="binding-a")
        raw = final_path.read_bytes()
        final_path.unlink()
        with self.assertRaisesRegex(ValueError, "owned_cockpit_final_not_persisted"):
            startup_harness.read_completed_cockpit_final(
                service, final_path, run_id="run-a", binding_id="binding-a")
        final_path.write_bytes(raw)
        cold = SimpleNamespace(run_channel=SimpleNamespace(status=lambda: {}))
        recovered = startup_harness.read_completed_cockpit_final(
            cold, final_path, run_id="run-a", binding_id="binding-a")
        self.addCleanup(recovered["action_observation_sequence"].archive.close)
        self.assertEqual(recovered["stop_detail"], result["result"]["stop_detail"])
        final_path.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "identity_mismatch"):
            startup_harness.read_completed_cockpit_final(
                cold, final_path, run_id="run-a", binding_id="binding-a")

    def test_ordinary_metadata_is_bound_and_never_promotes_a_formal_claim(self):
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        request = {"action": "run.finish", "observation_id": observed["observation_id"],
                   "stop_reason": "target_predicate_proved", "unused_authority": "released"}
        self.assertEqual(service.call(request)["error"], "target_predicate_proved_requires_validated_witness")
        response = service.call({**request, "outcome": {
            "summary": "Ordinary inconclusive result", "verdict": "inconclusive"}})
        self.assertTrue(response["ok"], response)
        final = response["result"]
        self.assertEqual(final["stop_reason"], "outcome_reported")
        self.assertNotIn("target_receipt", final["stop_detail"])
        final_path = self.directory / "ordinary.final.json"
        startup_harness.write_json(final_path, final)
        with mock.patch.object(ArchiveSequence, "__iter__", side_effect=AssertionError("history scan")):
            metadata = startup_harness.r014_cockpit_live_session_metadata(
                final, artifact_path=final_path, run_id="run-a", binding_id="binding-a", live_status=0)
            self.assertEqual(metadata["completion_kind"], "outcome_reported")
            self.assertFalse(metadata["gameplay_credit"])
            self.assertEqual(metadata["action_statistics"], "not_requested")
            for changed in ({"run_id": "foreign"}, {"binding_id": "foreign"}, {"state": "active"}):
                rejected = startup_harness.r014_cockpit_live_session_metadata(
                    {**final, **changed}, artifact_path=final_path,
                    run_id="run-a", binding_id="binding-a", live_status=0)
                self.assertEqual(rejected["status"], "scanned")
            # A formal-proof-shaped record still takes the existing history
            # path; the ordinary result must not become a shortcut for proof.
            with self.assertRaisesRegex(AssertionError, "history scan"):
                startup_harness.r014_cockpit_live_session_metadata(
                    {**final, "stop_reason": "target_predicate_proved"},
                    artifact_path=final_path, run_id="run-a", binding_id="binding-a", live_status=0)

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

    def test_final_reference_reuses_unchanged_sequence_but_cold_read_verifies(self):
        from cockpit_report_reference import load_report
        sequence = self.archive.sequence()
        for index in range(30):
            sequence.append({"index": index, "detail": "z" * 10000})
        report = {"schema": "caol-cockpit-live-final-v1", "run_id": "run-a", "binding_id": "binding-a",
                  "action_observation_sequence": sequence}
        path = self.directory / "final.json"
        with mock.patch.object(sequence, "json_chunks", wraps=sequence.json_chunks) as chunks, \
                mock.patch.object(startup_harness, "current_owned_process_generation",
                                  return_value={"pid": 42, "status": "exited", "expected": {"pid": 42}}):
            startup_harness.finalize_cockpit_live_session(self.directory, 42, report,
                                                         cleanup_process=False, final_path=path)
            wire = self.archive.wire({"ok": True, "result": report})
            self.assertEqual(chunks.call_count, 1)  # terminal artifact and response, one expansion
        self.assertLess(len(json.dumps(wire)), len("".join(json_chunks(sequence))) // 10)
        _, _, recovered = load_report(path)
        self.addCleanup(recovered["action_observation_sequence"].archive.close)
        self.assertEqual(list(recovered["action_observation_sequence"]), list(sequence))
        sequence.append({"index": 30, "detail": "new"})
        self.assertEqual(sequence.reference()["count"], 31)
        # Independent public recovery must not reuse a producer's cache.
        with sqlite3.connect(self.archive.path) as other:
            other.execute("DELETE FROM records WHERE stream=? AND seq=0", (sequence.stream,))
        with self.assertRaisesRegex(ValueError, "archive_sequence_gap"):
            sequence.reference()
        with self.assertRaisesRegex(ValueError, "archive_sequence_gap"):
            load_report(path)

    def test_reference_does_not_cache_legacy_sidecar(self):
        sequence = self.archive.sequence()
        sequence.append({"value": "first"})
        sidecar = sequence._sidecar_path(sort_keys=True)
        sidecar.write_text('{"value":"first"}', encoding="utf-8")
        first = sequence.reference()
        sidecar.write_text('{"value":"second"}', encoding="utf-8")
        self.assertNotEqual(sequence.reference()["sha256"], first["sha256"])

    def test_nested_legacy_sidecar_change_invalidates_outer_reference(self):
        inner = self.archive.sequence()
        inner.append({"value": "first"})
        outer = self.archive.sequence()
        outer.append({"nested": inner})
        sidecar = inner._sidecar_path(sort_keys=True)
        sidecar.write_text('{"value":"first"}', encoding="utf-8")
        first = outer.reference()
        sidecar.write_text('{"value":"second"}', encoding="utf-8")
        updated = outer.reference()
        self.assertNotEqual(updated["sha256"], first["sha256"])
        wire = self.archive.wire({"value": outer})
        recovered = resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        self.addCleanup(recovered["value"].archive.close)
        self.assertEqual(json.loads("".join(json_chunks(recovered["value"], sort_keys=True))),
                         [{"nested": [{"value": "second"}]}])
        # The reference describes the canonical sidecar bytes, and a cold
        # recovery verifies them independently after another sidecar change.
        sidecar.write_text('{"value":"third"}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "archive_reference_digest_mismatch"):
            resolve_wire(wire, directory=self.directory, binding_id="binding-a")

    def test_ordinary_finish_cites_consumed_facts_without_restoring_authority(self):
        for archived in (False, True):
            with self.subTest(archived=archived):
                current = {"event": "surface_descriptor", "schema_version": 1,
                           "run_id": "run-a", "frame_id": "frame:prior", "surface_id": "inventory:prior",
                           "kind": "inventory", "breadcrumbs": ["Inventory"],
                           "payload": {"title": "Before native redraw"},
                           "valid_actions": [{"id": "inventory.toggle", "stable_id": "item:1",
                                              "label": "Select physical item", "enabled": True}]}
                dispatched = []

                def dispatch(issuing, action_id, stable_id):
                    dispatched.append((action_id, stable_id))
                    current.update(frame_id="frame:current", surface_id="inventory:current",
                                   payload={"title": "Current native fact"})
                    return {"native_receipt": {
                        "run_id": "run-a", "requested_run_id": "run-a", "accepted": True,
                        "requested_frame_id": issuing["frame_id"], "consuming_frame_id": issuing["frame_id"],
                        "requested_surface_id": issuing["surface_id"], "consuming_surface_id": issuing["surface_id"],
                        "action_id": action_id, "stable_id": stable_id}, "next_frame": dict(current)}

                channel = CockpitRunChannel(lambda: current, dispatch,
                    archive=self.archive if archived else None, binding_id="binding-a")
                service = CockpitService(run_channel=channel)
                service.call({"action": "game.observe"})
                current["payload"] = {"title": "Prior native fact"}
                prior = service.call({"action": "game.observe"})["result"]
                action = {"action": "game.act", "observation_id": prior["observation_id"],
                          "action_id": "inventory.toggle", "stable_id": "item:1"}
                accepted = service.call(action)
                self.assertTrue(accepted["ok"], accepted)
                self.assertEqual(service.call(action)["error"], "duplicate_submission")
                if archived:
                    self.assertNotIn("public_state", channel._observations[prior["observation_id"]])
                channel._read_process_state = lambda: {"run_id": "run-a", "pid": 123, "alive": False}
                terminal = service.call({"action": "game.observe"})["result"]
                finish = {"action": "run.finish", "observation_id": terminal["observation_id"],
                          "stop_reason": "reported", "unused_authority": "released"}
                self.assertFalse(channel._observations[terminal["observation_id"]]["used"])
                channel._read_binding_id = lambda: "foreign-binding"
                self.assertEqual(service.call(finish)["error"], "native_exit_required_before_reporting")
                channel._read_binding_id = lambda: "binding-a"
                for source_id, field in (("other-run:prior", "surface.facts.title"),
                                         ("frame:missing", "surface.facts.title"),
                                         (prior["observation_id"], "fabricated_fact")):
                    refused = service.call({**finish, "outcome": {
                        "summary": "Player report", "verdict": "positive",
                        "citations": [{"observation_id": source_id, "select": [field]}]}})
                    self.assertFalse(refused["ok"], refused)
                    self.assertEqual(channel.status()["state"], "active")
                    self.assertFalse(channel._observations[terminal["observation_id"]]["used"], refused)
                stale_terminal = service.call({**finish, "observation_id": prior["observation_id"]})
                self.assertEqual(stale_terminal["error"], "unknown_or_stale_observation")
                retrieve = channel._outcome_citation_state

                def indexed_citation(source_id):
                    with patch.object(ArchiveSequence, "__iter__", side_effect=AssertionError("citation history scan")):
                        return retrieve(source_id)

                with patch("cockpit.build_evidence_journal", side_effect=AssertionError("unrequested journal")), \
                     patch.object(channel, "_outcome_citation_state", side_effect=indexed_citation):
                    finished = service.call({**finish, "outcome": {
                        "summary": "Selected item once", "verdict": "positive",
                        "citations": [{"observation_id": prior["observation_id"],
                                       "select": ["surface.facts.title"]}]}})
                self.assertTrue(finished["ok"], finished)
                report = finished["result"]["stop_detail"]["gameplay_outcome"]
                self.assertEqual(report["citations"][0]["values"],
                                 {"surface.facts.title": "Prior native fact"})
                self.assertEqual(report["citations"][0]["binding_id"], "binding-a")
                self.assertFalse(report["proof_promotion_authority"])
                self.assertNotIn("target_receipt", finished["result"]["stop_detail"])
                self.assertEqual(dispatched, [("inventory.toggle", "item:1")])

    def test_native_catalogue_snapshots_are_stored_once_and_keep_current_authority(self):
        actions = [{"id": "inventory.toggle", "stable_id": f"item:{i}",
                    "label": "same label", "enabled": True} for i in range(300)]
        facts = {"title": "Inventory", "contents": "native facts " * 1000}
        current = {"event": "surface_descriptor", "schema_version": 1,
                   "run_id": "run-a", "frame_id": "frame:0", "surface_id": "inventory:0",
                   "kind": "inventory", "breadcrumbs": ["Inventory"],
                   "payload": facts, "valid_actions": actions}
        dispatched = []

        def dispatch(issuing, action_id, stable_id):
            dispatched.append((action_id, stable_id))
            old = dict(issuing)
            current.update(frame_id="frame:selected", surface_id="inventory:selected")
            return {"native_receipt": {
                "run_id": "run-a", "requested_run_id": "run-a", "accepted": True,
                "requested_frame_id": old["frame_id"], "consuming_frame_id": old["frame_id"],
                "requested_surface_id": old["surface_id"], "consuming_surface_id": old["surface_id"],
                "action_id": action_id, "stable_id": stable_id}, "next_frame": dict(current)}

        channel = CockpitRunChannel(lambda: current, dispatch, archive=self.archive,
                                    binding_id="binding-a")
        service = CockpitService(run_channel=channel)
        originals = []
        for i in range(6):
            current.update(frame_id=f"frame:{i}", surface_id=f"inventory:{i}")
            result = service.call({"action": "game.observe"})["result"]
            originals.append(json.loads("".join(json_chunks(result))))
        identity = {"scenario_id": "scenario-a", "source_identity": "source-a",
                    "executable_identity": "exe-a", "run_id": "run-a", "binding_id": "binding-a"}
        journal_args = dict(charter=CHARTER, identity=identity,
                            terminal={"run_id": "run-a", "stop_reason": "selected detail"},
                            evidence_ceiling="focused")
        legacy_journal = build_evidence_journal(
            **journal_args, transcript=list(channel._transcript))
        journal = build_evidence_journal(**journal_args, transcript=channel._transcript)
        self.assertEqual(json.loads("".join(json_chunks(journal))), legacy_journal)
        # Repeated labels retain all distinct physical identities, not just the
        # preview. The current native receipt consumes one exact item once.
        chosen = {"action": "game.act", "observation_id": result["observation_id"],
                  "action_id": "inventory.toggle", "stable_id": "item:299"}
        accepted = service.call(chosen)
        self.assertTrue(accepted["ok"], accepted)
        self.assertFalse(service.call(chosen)["ok"])
        self.assertEqual(dispatched, [("inventory.toggle", "item:299")])
        payloads = self.archive.connection.execute(
            "SELECT count(*),sum(length(value)) FROM objects WHERE namespace='payloads'").fetchone()
        record_bytes = self.archive.connection.execute(
            "SELECT sum(length(value)) FROM records").fetchone()[0]
        logical_bytes = sum(len(json.dumps(item)) for item in originals)
        self.assertLess(record_bytes + (payloads[1] or 0), logical_bytes // 2)
        # Two shared native payloads and the action's distinct successor
        # observation. The latter references those same facts and catalogue.
        self.assertEqual(payloads[0], 3)
        for i, original in enumerate(originals):
            self.assertEqual(channel._transcript[i]["value"], original)
        # A live mutation cannot change any historical snapshot, and a native
        # disabled action in a new publication cannot inherit old authority.
        current["payload"]["title"] = "Changed"
        current["valid_actions"][299]["enabled"] = False
        current.update(frame_id="frame:changed", surface_id="inventory:changed")
        changed = service.call({"action": "game.observe"})["result"]
        denied = service.call({**chosen, "observation_id": changed["observation_id"]})
        self.assertFalse(denied["ok"])
        self.assertEqual(dispatched, [("inventory.toggle", "item:299")])
        self.assertEqual(channel._transcript[0]["value"], originals[0])
        self.assertEqual(journal["entries"][1]["value"]["value"]["surface"]["facts"]["title"],
                         "Inventory")
        wire = self.archive.wire({"entries": channel._transcript})
        recovered = resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        self.addCleanup(recovered["entries"].archive.close)
        recovered["entries"][0]["value"]["surface"]["facts"]["title"] = "caller mutation"
        self.assertEqual(recovered["entries"][0]["value"], originals[0])
        with self.archive.connection:
            self.archive.connection.execute(
                "UPDATE objects SET value='{}' WHERE namespace='payloads' AND id="
                "(SELECT id FROM objects WHERE namespace='payloads' LIMIT 1)")
        with self.assertRaisesRegex(ValueError, "payload_digest_mismatch"):
            resolve_wire(wire, directory=self.directory, binding_id="binding-a")

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
        self.assertEqual(list(recovered["action_observation_sequence"]), list(report["action_observation_sequence"]))
        self.assertIn("caol-archive-sequence-ref-v1", final_path.read_text(encoding="utf-8"))
        scenario = {"steps": [{"cockpit_live_session": {"final": recovered}}]}
        output = self.directory / "probe.report.json"
        startup_harness.write_json(output, scenario)
        startup_harness.write_json(output, scenario)  # Existing mutable report writer may overwrite.
        from cockpit_report_reference import load_report
        _, _, loaded = load_report(output)
        self.assertEqual(json.loads("".join(json_chunks(loaded))), json.loads("".join(json_chunks(scenario))))

    def test_live_eof_records_disconnect_without_finalizing_or_materializing(self):
        service = self.service()
        service.call({"action": "game.observe"})
        output = io.StringIO()
        self.assertEqual(startup_harness.serve_cockpit_live(service, io.StringIO(), output), 1)
        wire = json.loads(output.getvalue())
        self.assertEqual(wire["error"], "client_disconnected")
        self.assertEqual(wire["session_state"], "active")
        self.assertIsNone(service.run_channel.status()["final"])
        self.assertEqual(service.run_channel._transcript[-1]["kind"], "operation_failure")

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

    def test_live_wire_export_is_bounded_while_archive_sequence_remains_lazy(self):
        sequence = self.archive.sequence()
        for index in range(5000):
            sequence.append({"kind": "observation", "frame_id": f"frame:{index}",
                             "payload": "repeated-native-fact" * 8})
        response = {"ok": True, "result": {
            "schema": "caol-cockpit-live-final-v1",
            "action_observation_sequence": sequence,
        }}
        wire = self.archive.wire(response)
        envelope = wire["artifact_reference_envelope"]
        self.assertEqual(envelope["schema"], "caol-live-artifact-reference-v2")
        exported = Path(envelope["exported_response"]["path"])
        self.assertLess(exported.stat().st_size, 10000)
        recovered = resolve_wire(wire, directory=self.directory, binding_id="binding-a")
        restored = recovered["result"]["action_observation_sequence"]
        self.addCleanup(restored.archive.close)
        self.assertEqual(len(restored), 5000)
        self.assertEqual(restored[4999]["frame_id"], "frame:4999")

    def test_terminal_export_streams_records_without_duplicate_sidecars(self):
        """A terminal export must scale with one record, not its full history."""
        sequence = self.archive.sequence()
        payload = "terminal-native-observation" * 4096
        for index in range(80):
            sequence.append({"kind": "observation", "frame_id": f"frame:{index}",
                             "payload": payload + str(index)})
        output = self.directory / "terminal-export.json"
        tracemalloc.start()
        try:
            tracemalloc.reset_peak()
            exported = write_json_stream(output, sequence)
            reference = sequence.reference()
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        self.assertEqual(exported["bytes"], output.stat().st_size)
        self.assertEqual(reference["count"], 80)
        # Eighty records total more than 8 MiB.  The terminal path must not
        # decode that sequence merely to export or calculate its reference.
        self.assertGreater(output.stat().st_size, 8_000_000)
        self.assertLess(peak, 2_000_000)
        self.assertEqual(list(self.directory.glob("*.canonical.jsonl")), [])
        self.assertEqual(list(self.directory.glob("*.export.jsonl")), [])

    def test_consumed_observation_releases_duplicate_authority_payload(self):
        records = ArchiveMap(self.archive, "observations")
        payload = "native-frame-payload" * 8192
        records["frame:1"] = {
            "run_id": "run-a", "used": False, "actions": {"world.wait"},
            "action_stable_ids": {}, "handles": set(), "native_interruption": False,
            "public_state": {"payload": payload}, "issuing_frame": {"payload": payload},
        }
        records["frame:1"]["used"] = True
        stored = records["frame:1"]
        self.assertTrue(stored["used"])
        self.assertNotIn("public_state", stored)
        self.assertNotIn("issuing_frame", stored)
        size = self.archive.connection.execute(
            "SELECT length(value) FROM objects WHERE namespace=? AND id=?", ("observations", "frame:1")
        ).fetchone()[0]
        self.assertLess(size, len(payload) // 8)

    def test_action_receipt_excludes_private_successor_frame(self):
        self.assertEqual(
            CockpitRunChannel._public_action_receipt({
                "accepted": True, "native_receipt": {"accepted": True},
                "next_frame": {"payload": "private"}, "_next_frame": {"payload": "private"},
            }),
            {"accepted": True, "native_receipt": {"accepted": True}},
        )

    def test_large_repeated_action_delivery_releases_consumed_authority_payloads(self):
        """Exercise construct → wire → client decode → release for large frames."""
        count, payload = 160, "native-observation-payload" * 2048
        frames = []
        for index in range(count + 1):
            value = frame()
            value["frame_id"] = f"run-a:{index}"
            value["observed_turn"] = index
            value["game_minutes"] = 100 + index
            value["observation"]["large_payload"] = payload + str(index)
            frames.append(value)
        index = [0]

        def dispatch(issuing, action_id):
            index[0] += 1
            return {
                "accepted": True,
                "native_receipt": {"accepted": True, "run_id": "live-proof",
                                   "frame_id": issuing["frame_id"], "action_id": action_id},
                "next_frame": frames[index[0]], "_next_frame": frames[index[0]],
            }

        channel = CockpitRunChannel(lambda: frames[index[0]], dispatch, binding_id="binding-a",
                                    archive=self.archive)
        observed = channel.observe()
        samples = []
        tracemalloc.start()
        try:
            for _ in range(count):
                result = channel.act(observation_id=observed["observation_id"], action_id="world.wait")
                self.assertTrue(result["ok"], result)
                self.assertNotIn("next_frame", result["receipt"])
                # The bridge persists bytes, and a later client decodes only
                # that one response.  Neither operation may retain it.
                delivered = "".join(json_chunks(self.archive.wire(result)))
                collected = json.loads(delivered)
                self.assertTrue(collected["ok"])
                observed = result["observation"]
                del collected, delivered, result
                gc.collect()
                samples.append(tracemalloc.get_traced_memory()[0])
        finally:
            tracemalloc.stop()
        authority_bytes = self.archive.connection.execute(
            "SELECT coalesce(sum(length(value)),0) FROM objects WHERE namespace='observations'"
        ).fetchone()[0]
        # Only the one current frame can retain a complete authority payload;
        # completed frames are indexed tombstones.  Transcript growth remains
        # linear and is the deliberately retained final-export history.
        self.assertLess(authority_bytes, len(payload) * 4)
        self.assertLess(max(samples) - min(samples), len(payload) * 8)

    def test_profiled_large_delivery_distinguishes_release_from_archive_growth(self):
        """The profiler must describe scalars, not retain the delivered payload."""
        profile = self.directory / "memory.jsonl"
        payload = "native-delivery" * 131072
        with mock.patch.dict(os.environ, {PROFILE_PATH_ENV: str(profile)}):
            memory_snapshot("test", "before_construction", archive=self.archive)
            response = {"ok": True, "result": {"payload": payload}}
            memory_snapshot("test", "after_construction", archive=self.archive)
            delivered = "".join(json_chunks(response))
            collected = json.loads(delivered)
            memory_snapshot("test", "after_collection", archive=self.archive,
                            response_bytes=len(delivered.encode("utf-8")))
            del response, delivered, collected
            memory_released("test", "after_reference_release", archive=self.archive)
        rows = [json.loads(line) for line in profile.with_name("memory.test.jsonl").read_text().splitlines()]
        released_row = rows[-1]
        self.assertEqual(released_row["stage"], "after_reference_release_post_gc")
        self.assertLess(released_row["tracemalloc_current_bytes"], len(payload) // 4)
        self.assertLess(released_row["archive_bytes"], len(payload) // 4)


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
