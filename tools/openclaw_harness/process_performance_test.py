import copy
import errno
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import time
import unittest
from unittest.mock import patch

import process_performance as perf
from r009_technical_witness import (sample_child_resources, complete_child_resource_interval,
                                    parse_ps_cpu_time, _macos_resource_sample)


def sample(t, cpu, identity="start-a"):
    return {"pid": 73, "platform": "macos", "process_identity": identity,
            "sampled_monotonic_seconds": t, "cpu_seconds": cpu,
            "cpu_counter_resolution_seconds": 0.01,
            "resident_memory": {"status": "available", "value": 1000000, "unit": "bytes"}}


class PerformanceTest(unittest.TestCase):
    def test_reduced_interval_evidence_discloses_original_scope(self):
        from evidence_events import envelopes
        source = {"producer": "native_semantic_events", "sha256": "a" * 64, "offset": 0}
        packet = {"event": "raid_performance_interval", "run_id": "run-a",
                  "retention_scope": "max_simulation_row_only", "original_row_count": 3000,
                  "rows": [{"turn": 5271300, "simulation_seconds": 3.047}]}
        interval = next(envelopes(packet, source))["interval"]
        self.assertEqual(interval, {"first_turn": 5271300, "last_turn": 5271300,
                                   "row_count": 1, "retention_scope": "max_simulation_row_only",
                                   "original_row_count": 3000})

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.evidence_store = self.directory / "retained-evidence"
        store_patch = patch("evidence_display.STORE", self.evidence_store)
        store_patch.start()
        self.addCleanup(store_patch.stop)

    def recorder(self, samples):
        return perf.ProcessPerformance(self.directory, pid=73, run_id="run-a", binding_id="binding-a",
                                       source_binding={"runtime_binding_sha256": "binary-a"},
                                       sampler=lambda _: samples.pop(0))

    def turn_config(self, *, workload=None, baseline=None, directory=None):
        config = {
            "schema": "caol-performance-turn-config-v1",
            "provenance": "controlled instrumentation fixture",
            "reference": "process_performance_test",
            "window_turns": 4,
            "tail_percentile": 0.95,
            "spike_seconds": 2.0,
            "slow_turn_seconds": 1.0,
            "sustained_turns": 2,
            "incomplete_turn_seconds": 0.5,
            "stalled_observation_seconds": 0.5,
            "expected_progress": True,
        }
        if workload is not None:
            config["workload"] = workload
        if baseline is not None:
            config["baseline"] = baseline
        perf.write_json((directory or self.directory) / perf.TURN_CONFIG_NAME, config)

    def turn_event(self, turn_id, stage, *, sequence, duration=0.0, phase="simulation", wall=10.0):
        return {"event": "turn", "stage": stage, "run_id": "run-a", "process_instance": "native-a",
                "turn_id": str(turn_id), "sequence": sequence, "game_turn": 100 + int(turn_id),
                "game_minutes": 100 + int(turn_id), "wall_time_seconds": wall,
                "simulation_seconds": duration, "phase": phase, "owner": "game::do_turn"}

    def append_trace(self, *events, directory=None):
        path = (directory or self.directory) / "semantic.native.events.jsonl"
        with path.open("a", encoding="utf-8") as trace:
            for event in events:
                trace.write(perf.TURN_TRACE_PREFIX + json.dumps(event) + "\n")

    def semantic_event(self, event, *, request_id="p1", wall=10.1, accepted=True):
        value = {"event": event, "run_id": "run-a", "request_id": request_id,
                 "wall_time": wall * 1000, "accepted": accepted}
        if event == "request_transport":
            value.update(stage="queued", offset_before=1, offset_after=2, transport_end=2,
                         queued=True, wake_pending=True)
        return value

    def surface_descriptor_event(self, kind="menu", *, wall=10.2, frame_id="frame-menu"):
        return {"event": "surface_descriptor", "run_id": "run-a", "request_id": "p1",
                "kind": kind, "surface_id": "surface-menu", "frame_id": frame_id,
                "breadcrumbs": ["World", "Wait duration"], "wall_time": wall * 1000}

    def test_selected_interval_retained_at_collection_then_survives_rotation(self):
        import base64
        import hashlib
        from evidence_display import recover
        from cockpit_archive import Archive, ArchiveMap
        self.recorder([sample(1, 0)])
        self.turn_config()
        path = self.directory / "semantic.native.events.jsonl"
        raw = json.dumps({"event": "raid_performance_interval", "run_id": "run-a",
                          "rows": [[11, .01], [12, .02]]}, separators=(",", ":")).encode() + b"\n"
        path.write_bytes(raw)
        result = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "r1"})
        handle = result["selected_native_packets"][0]
        self.assertEqual(handle["length"], len(raw))
        self.assertEqual(handle["sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(handle["collected_during_request"], {"request_id": "r1"})
        files = set(self.evidence_store.iterdir())
        # Replayed same packet after producer rewind must not make another copy.
        perf.write_json(self.directory / perf.TURN_STATE_NAME, {})
        perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "r2"})
        self.assertEqual(set(self.evidence_store.iterdir()), files)
        archive = Archive(self.directory / "cockpit-evidence.sqlite", run_id="run-a", binding_id="binding-a")
        try:
            self.assertEqual(len(ArchiveMap(archive, "selected_native_packets")), 1)
        finally:
            archive.close()
        path.write_bytes(b"rotated\n")
        self.assertEqual(base64.b64decode(recover(handle["retained_raw"]["sha256"], self.evidence_store)["raw_base64"]), raw)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "foreign")["status"], "unavailable")

    def test_explicit_max_retains_only_selected_turn_and_survives_rotation(self):
        import base64
        from evidence_display import recover
        self.recorder([sample(1, 0)])
        start = self.turn_event(1, "start", sequence=1)
        phase = {**self.turn_event(1, "begin", sequence=2, phase="input"), "event": "turn_phase"}
        end = self.turn_event(1, "end", sequence=3, duration=2.5)
        self.append_trace(start, phase, end, self.turn_event(2, "start", sequence=4),
                          self.turn_event(2, "end", sequence=5, duration=.02))
        unselected = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertNotIn("selected_max_turn", unselected)
        self.assertFalse(self.evidence_store.exists())
        selected = perf.collect_turn_assessment(self.directory, "binding-a", retain_max=True)
        handle = selected["selected_max_turn"]
        self.assertEqual(handle["status"], "retained")
        self.assertEqual(handle["game_turn"], 101)
        artifact = recover(handle["evidence"]["sha256"], self.evidence_store)
        self.assertEqual(len(artifact["records"]), 3)
        raw = [base64.b64decode(item["raw_base64"]) for item in artifact["records"]]
        self.assertEqual([json.loads(item[len(perf.TURN_TRACE_PREFIX):])["stage"] for item in raw],
                         ["start", "begin", "end"])
        path = self.directory / "semantic.native.events.jsonl"
        path.write_bytes(b"rotated\n")
        self.assertEqual(recover(handle["evidence"]["sha256"], self.evidence_store), artifact)
        # An old selected handle cannot be silently substituted after rotation.
        missing = perf.retain_selected_max_turn(self.directory, unselected["owner"], artifact["measurement"])
        self.assertEqual(missing["status"], "unavailable")

    def test_public_performance_explicit_max_selection(self):
        from types import SimpleNamespace
        from play_cli import PlayerClient
        self.recorder([sample(1, 0)])
        perf.write_json(self.directory / "bridge.manifest.json", {"binding_id": "binding-a"})
        perf.write_json(self.directory / "status.json", {"binding_id": "binding-a", "state": "ready",
                        "session_descriptor": {"run_id": "run-a"}})
        self.append_trace(self.turn_event(1, "start", sequence=1),
                          self.turn_event(1, "end", sequence=2, duration=1.7))
        args = SimpleNamespace(sample_seconds=None, retain_max=True, offset=None, limit=5,
                               baseline=None, save_baseline=None, tag="")
        result = PlayerClient(self.directory).performance(args)
        self.assertEqual(result["turn_assessment"]["selected_max_turn"]["status"], "retained")
        self.assertEqual(result["turn_assessment"]["selected_max_turn"]["game_turn"], 101)

    def test_selected_packet_failure_does_not_advance_and_foreign_packet_refused(self):
        self.recorder([sample(1, 0)])
        path = self.directory / "semantic.native.events.jsonl"
        path.write_text('{"event":"raid_performance_interval","run_id":"run-a","rows":[]}\n')
        with patch("evidence_display.retain", side_effect=OSError("full")):
            result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(result["reason"], "selected_packet_retention_failed")
        self.assertFalse((self.directory / perf.TURN_STATE_NAME).exists())
        self.assertEqual(len(perf.collect_turn_assessment(self.directory, "binding-a")["selected_native_packets"]), 1)
        path.write_text('{"event":"raid_performance_interval","run_id":"foreign","rows":[]}\n')
        perf.write_json(self.directory / perf.TURN_STATE_NAME, {})
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["reason"], "native_interval_run_mismatch")

    def test_public_evidence_finds_delayed_collected_interval_after_rotation(self):
        import base64
        import hashlib
        from types import SimpleNamespace
        import evidence_display
        from play_cli import PlayerClient

        self.recorder([sample(1, 0)])
        perf.write_json(self.directory / "bridge.manifest.json", {"binding_id": "binding-a"})
        perf.write_json(self.directory / "status.json", {
            "binding_id": "binding-a", "state": "ready",
            "session_descriptor": {"run_id": "run-a"}})
        path = self.directory / "semantic.native.events.jsonl"
        perf.write_json(self.directory / "game-process.json", {
            "binding_id": "binding-a", "run_id": "run-a", "log_paths": {
                "native_semantic_events": {"path": str(path), "scope": "run_bound"}}})
        client = PlayerClient(self.directory)
        client.state["pending"] = {"request_id": "wait-a", "request": {"action": "game.wait"},
                                   "submitted_unix_seconds": 9.0}
        self.waiting_turns(1, [.2] * 100)
        unavailable = {"ok": False, "error": "response_not_available_or_stale"}
        with patch("play_cli.Bridge.response_status", return_value=unavailable), \
                patch("play_cli.Bridge.send_request") as send:
            alarm = client.collect(wait_seconds=0)
        send.assert_not_called()
        self.assertTrue(alarm["performance_alarm"])
        self.assertEqual(alarm["turn_assessment"]["selected_native_packets"], [])
        # Complete that request without inventing a packet during the native
        # activity. The later input-stop callback is a separate collection.
        with patch("play_cli.Bridge.response_status", return_value={
                "ok": True, "receipt": {"binding_id": "binding-a", "response_sha256": "a" * 64}}), \
                patch("play_cli.Bridge.response_artifact", return_value={
                    "ok": True, "response": {"ok": False, "error": "keep_watch_unsafe_condition"}}):
            self.assertEqual(client.collect()["state"], "rejected")
        client.state["pending"] = {"request_id": "stop-input-b",
                                   "request": {"action": "game.act", "action_id": "prompt.choose"},
                                   "submitted_unix_seconds": 20.0}
        packet = {"event": "raid_performance_interval", "run_id": "run-a",
                  "rows": [{"turn": turn, "simulation_seconds": .2, "wait_begin": 1,
                            "wait_end": 0 if turn == 201 else 1}
                           for turn in range(100, 202)]}
        raw = json.dumps(packet, separators=(",", ":")).encode() + b"\n"
        with path.open("ab") as stream:
            stream.write(raw)
        with patch("play_cli.Bridge.response_status", return_value=unavailable):
            collected = client.collect(wait_seconds=0)
        handle = collected["turn_assessment"]["selected_native_packets"][0]
        self.assertEqual(handle["collected_during_request"]["request_id"], "stop-input-b")
        self.assertEqual(base64.b64decode(evidence_display.recover(
            handle["retained_raw"]["sha256"], self.evidence_store)["raw_base64"]), raw)
        path.write_text('{"event":"later","run_id":"run-a"}\n')
        arguments = SimpleNamespace(
            decisions=False, operation_id=None, group_id=None, actor=[], offset=0, snapshot=None,
            actor_id=None, actor_name=None, request_id=None, run_id="run-a",
            event="raid_performance_interval", process_instance=None, contains=None,
            from_turn=None, to_turn=None, where=["interval.first_turn<=150", "interval.last_turn>=199"],
            select=["event,run_id,request_id,interval"], limit=5)
        def query():
            with patch("evidence_events.retain", side_effect=lambda value: evidence_display.retain(
                    value, self.evidence_store)), patch("play_cli.Bridge.send_request") as send:
                result = client.evidence(arguments)
            send.assert_not_called()
            return result

        archive_path = self.directory / "cockpit-evidence.sqlite"
        before_archive = (archive_path.read_bytes(), archive_path.stat().st_mtime_ns)
        before_client = copy.deepcopy(client.state)
        result = query()
        self.assertEqual(result["matched"], 1, result)
        row = result["rows"][0]
        self.assertIsNone(row["fields"]["request_id"])
        self.assertEqual(row["fields"]["interval"], {"first_turn": 100, "last_turn": 201,
                                                     "row_count": 102})
        self.assertEqual(row["source"]["retained_raw"], handle["retained_raw"])
        self.assertEqual(row["source"]["sha256"], handle["sha256"])
        self.assertEqual(row["source"]["collection_context"]["request_id"], "stop-input-b")
        self.assertEqual((archive_path.read_bytes(), archive_path.stat().st_mtime_ns), before_archive)
        self.assertEqual(client.state, before_client)
        # A journal copy and its retained original are one packet, even when
        # compaction moved its coordinates. The original remains the citation.
        path.write_bytes(raw)
        self.assertEqual(query()["matched"], 1)
        coverage = arguments.where
        arguments.where = ["source.offset=0"]
        self.assertEqual(query()["matched"], 1)
        arguments.where = coverage
        arguments.request_id = "stop-input-b"
        self.assertEqual(query()["matched"], 0)
        arguments.request_id = None
        path.write_text('{"event":"later","run_id":"run-a"}\n')
        from cockpit_archive import Archive, ArchiveMap
        foreign_raw = raw.replace(b'"run_id":"run-a"', b'"run_id":"foreign"')
        foreign_artifact = evidence_display.retain(
            {"raw_base64": base64.b64encode(foreign_raw).decode()}, self.evidence_store)
        for changes, reason in (({"binding_id": "foreign"}, "binding_mismatch"),
                                ({"length": handle["length"] + 1}, "bytes_mismatch"),
                                ({"retained_raw": foreign_artifact,
                                  "length": len(foreign_raw),
                                  "sha256": hashlib.sha256(foreign_raw).hexdigest()},
                                 "identity_mismatch")):
            with self.subTest(changes=changes):
                archive = Archive(archive_path, run_id="run-a", binding_id="binding-a")
                try:
                    ArchiveMap(archive, "selected_native_packets")[handle["sha256"]] = {**handle, **changes}
                finally:
                    archive.close()
                refused = query()
                self.assertEqual(refused["matched"], 0)
                self.assertTrue(any(reason in row["error"] for row in refused["unavailable_sources"]))

    def test_mac_cumulative_time_and_rss_are_not_smoothed_cpu(self):
        with patch("r009_technical_witness.subprocess.run") as run:
            run.return_value.returncode = 0
            run.return_value.stdout = "1:02.34 4000 Sat Sep 5 14:00:00 2026"
            result = _macos_resource_sample(73)
        self.assertEqual(result["cpu_seconds"], 62.34)
        self.assertEqual(result["resident_memory"]["value"], 4096000)
        self.assertEqual(result["cpu_percent"]["status"], "unavailable")
        self.assertEqual(parse_ps_cpu_time("2-01:02:03.40"), 176523.4)
        interval = complete_child_resource_interval(sample(1, 2), sample(3, 7))
        self.assertEqual(interval["cpu_percent"]["value"], 250)
        self.assertEqual(interval["cpu_percent"]["unit"], "percent_of_one_cpu_core")
        self.assertEqual(interval["interval_cpu_seconds"], 5)

    def test_reused_pid_or_decreasing_counter_cannot_make_cpu_claim(self):
        for after in (sample(2, 4, "start-b"), sample(2, 0)):
            result = complete_child_resource_interval(sample(1, 1), after)
            self.assertEqual(result["cpu_percent"]["status"], "unavailable")

    def test_native_action_records_progress_context_and_stops_sampling(self):
        samples = [sample(1, 0), sample(2, 1), sample(4, 3)]
        recorder = self.recorder(samples)
        frame = {"kind": "world", "frame_id": "f1", "game_minutes": 100}
        recorder.begin_action(frame, "world.pause")
        recorder.end_action(frame, {"accepted": True, "next_frame": {
            "kind": "world", "frame_id": "f2", "game_minutes": 101}})
        record = recorder.latest
        self.assertEqual(record["game_time"]["delta_minutes"], 1)
        self.assertEqual(record["context"]["phase"], "waiting_or_fast_forward")
        self.assertEqual(record["resources"]["cpu_percent"]["value"], 100)
        self.assertIsNotNone(record["action_latency_seconds"])
        self.assertEqual(len(perf.read_records(self.directory, 0, 5)["records"]), 1)
        recorder.stop()
        recorder.observe(frame)
        self.assertTrue(perf.read_json(self.directory / "performance-context.json")["session_ended"])
        self.assertEqual(samples, [])

    def test_on_demand_reads_pending_context_without_bridge_input_and_marks_mixed(self):
        recorder = self.recorder([sample(1, 0)])
        recorder.set_context({"phase": "native_action", "action_id": "world.pause"})
        perf.write_json(self.directory / "status.json", {"state": "busy"})
        samples = [sample(2, 1), sample(3, 3)]
        record = perf.sample_owned_session(self.directory, "binding-a", 1,
            sampler=lambda _: samples.pop(0), sleep=lambda _: recorder.set_context({"phase": "waiting_for_input"}))
        self.assertEqual(record["context"]["phase"], "mixed")
        self.assertEqual(record["resources"]["cpu_percent"]["value"], 200)
        self.assertFalse((self.directory / "requests").exists())
        self.assertFalse((self.directory / "play-client.json").exists())

    def test_on_demand_rejects_end_or_identity_change_during_sample(self):
        recorder = self.recorder([sample(1, 0)])
        with self.assertRaisesRegex(ValueError, "pid_reused"):
            samples = [sample(2, 1), sample(3, 2, "reused")]
            perf.sample_owned_session(self.directory, "binding-a", 1,
                                      sampler=lambda _: samples.pop(0), sleep=lambda _: None)
        with self.assertRaisesRegex(ValueError, "session_ended"):
            perf.sample_owned_session(self.directory, "binding-a", 1,
                                      sampler=lambda _: sample(2, 1), sleep=lambda _: recorder.stop())
        self.assertFalse((self.directory / "performance.jsonl").exists())

    def test_comparison_requires_explicit_tag_and_comparable_bound_context(self):
        recorder = self.recorder([sample(1, 0)])
        record = perf.resource_record(recorder.owner, sample(1, 0), sample(3, 2),
                                      {"phase": "waiting_or_fast_forward", "action_id": "world.pause", "surface_kind": "world"}, 100, 101, 2)
        baseline = {"comparison_tag": "same fixture one minute", "record": copy.deepcopy(record)}
        record["resources"]["cpu_percent"]["value"] = 200
        result = perf.compare_records(record, baseline, "same fixture one minute")
        self.assertEqual(result["measured_relative_increases"], [])
        record["resources"]["interval_wall_seconds"] = 4
        result = perf.compare_records(record, baseline, "same fixture one minute")
        self.assertIn("wall_seconds_per_game_minute", result["measured_relative_increases"])
        self.assertEqual(result["metrics"]["wall_seconds_per_game_minute"]["ratio"], 2)
        self.assertEqual(perf.compare_records(record, baseline, "")["status"], "unavailable")
        record["context"]["phase"] = "mixed"
        self.assertEqual(perf.compare_records(record, baseline, "same fixture one minute")["status"], "incomparable")

    def test_unreadable_identity_reports_unknown_without_claiming_idle_or_exit(self):
        recorder = self.recorder([sample(1, 0), sample(2, 1, None)])
        recorder.begin_action({"kind": "world"}, "world.pause")
        self.assertEqual(recorder.context["phase"], "unknown")
        self.assertIn("identity unavailable", recorder.failure)
        self.assertIsNone(recorder.latest)
        self.assertEqual(perf.compact_status(recorder.brief(), "run-a", "binding-a")
                         ["collection_failure"]["reason"], "owned_process_identity_unavailable_or_changed")

    def test_sampler_foreign_pid_cannot_record_under_existing_birth(self):
        recorder = self.recorder([sample(1, 0), {**sample(2, 1), "pid": 74}])
        recorder.begin_action({"kind": "world"}, "world.pause")
        self.assertTrue(recorder.stopped)
        self.assertIsNone(recorder.latest)
        self.assertFalse((self.directory / "performance.jsonl").exists())

    def test_failed_telemetry_write_preserves_native_outcome(self):
        recorder = self.recorder([sample(1, 0), sample(2, 1), sample(3, 2)])
        frame = {"kind": "world", "game_minutes": 100}
        recorder.begin_action(frame, "world.pause")
        with patch("process_performance.append_record", side_effect=OSError("disk unavailable")):
            recorder.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": 101}})
        self.assertEqual(recorder.brief()["collection_error"], "disk unavailable")
        self.assertTrue(recorder.stopped)

    def test_paging_keeps_exact_records_and_next_offset(self):
        recorder = self.recorder([sample(1, 0)])
        for index in range(12):
            perf.append_record(self.directory, {"index": index, "owner": recorder.owner})
        page = perf.read_records(self.directory, 5, 5)
        self.assertEqual([item["index"] for item in page["records"]], list(range(5, 10)))
        self.assertEqual(page["next_offset"], 10)

    def test_latest_replace_failure_keeps_committed_advancing_records_and_recovers(self):
        recorder = self.recorder([sample(t, t) for t in range(1, 12)])
        recorder.observe({"kind": "world", "game_minutes": 100})
        original = perf.read_json(self.directory / "performance.latest.json")
        replace = os.replace
        def blocked(source, destination):
            if Path(destination).name == "performance.latest.json":
                raise PermissionError(errno.EACCES, "controlled latest sharing refusal")
            return replace(source, destination)
        with patch("process_performance.os.replace", side_effect=blocked):
            for minute in (100, 101, 102):
                frame = {"kind": "world", "game_minutes": minute}
                recorder.begin_action(frame, "world.pause")
                recorder.end_action(frame, {"accepted": True, "next_frame": {
                    "kind": "world", "game_minutes": minute + 1}})
                self.assertFalse(recorder.stopped)
                self.assertEqual(recorder.latest_publication["status"], "unavailable")
                self.assertEqual(recorder.latest["game_time"]["delta_minutes"], 1)
        rows = perf.read_records(self.directory, 0, 20)["records"]
        self.assertEqual(len(rows), 4)
        self.assertEqual(len({r["record_id"] for r in rows}), 4)
        self.assertEqual(perf.read_json(self.directory / "performance.latest.json"), original)
        self.assertEqual(list(self.directory.glob("*.tmp")), [])
        recorder.observe({"kind": "world", "game_minutes": 103})
        self.assertEqual(recorder.latest_publication["status"], "published")
        self.assertEqual(perf.read_json(self.directory / "performance.latest.json"), recorder.latest)
        self.assertEqual(len(perf.read_records(self.directory, 0, 20)["records"]), 5)
        self.assertIsNone(recorder.latest["game_time"]["delta_minutes"])

    def test_context_replace_failure_does_not_erase_native_action_and_is_disclosed(self):
        recorder = self.recorder([sample(t, t) for t in range(1, 7)])
        replace = os.replace
        def blocked(source, destination):
            if Path(destination).name == "performance-context.json":
                raise PermissionError(errno.EACCES, "controlled context sharing refusal")
            return replace(source, destination)
        frame = {"kind": "world", "game_minutes": 100}
        with patch("process_performance.os.replace", side_effect=blocked):
            recorder.begin_action(frame, "world.pause")
            recorder.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": 101}})
            status = perf.compact_status(recorder.brief(), "run-a", "binding-a")
            self.assertEqual(status["context_publication"]["status"], "unavailable")
            self.assertEqual(status["latest_publication"]["status"], "published")
            self.assertEqual(status["latest_game_time"]["delta_minutes"], 1)
            self.assertFalse(recorder.stopped)
        recorder.observe({"kind": "world", "game_minutes": 101})
        self.assertEqual(recorder.context_publication["status"], "published")
        self.assertEqual(list(self.directory.glob("*.tmp")), [])

    def test_append_failure_still_stops_without_replaying_uncertain_write(self):
        recorder = self.recorder([sample(1, 0), sample(2, 1), sample(3, 2)])
        frame = {"kind": "world", "game_minutes": 100}
        recorder.begin_action(frame, "world.pause")
        with patch("process_performance.os.write", return_value=0):
            recorder.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": 101}})
        self.assertTrue(recorder.stopped)
        self.assertIsNone(recorder.latest)
        self.assertEqual(recorder.failure_details["operation"], "end_action")
        self.assertEqual((self.directory / "performance.jsonl").read_bytes(), b"")
        recorder.observe(frame)
        self.assertEqual((self.directory / "performance.jsonl").read_bytes(), b"")

    def test_foreign_persisted_owner_cannot_append_or_publish(self):
        for key, value in (("pid", 74), ("process_identity", "reused"), ("run_id", "foreign"),
                           ("binding_id", "foreign"), ("source_binding", {"runtime_binding_sha256": "other"})):
            with self.subTest(key=key), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                recorder = perf.ProcessPerformance(directory, pid=73, run_id="run-a", binding_id="binding-a",
                    source_binding={"runtime_binding_sha256": "binary-a"}, sampler=lambda _: sample(1, 0))
                foreign = {**recorder.owner, key: value}
                perf.write_json(directory / "performance-owner.json", foreign)
                with self.assertRaisesRegex(ValueError, "owner_missing_or_changed"):
                    perf.append_record(directory, {"record_id": "not-authorized", "owner": recorder.owner})
                self.assertFalse((directory / "performance.jsonl").exists())
                self.assertFalse((directory / "performance.latest.json").exists())

    def test_owner_change_during_latest_preparation_cannot_replace(self):
        recorder = self.recorder([sample(1, 0)])
        record = perf.resource_record(recorder.owner, sample(2, 1), sample(3, 2), {}, 100, 101, 1)
        write = Path.write_text
        def change_owner(path, *args, **kwargs):
            result = write(path, *args, **kwargs)
            if path.name.startswith("performance.latest.json."):
                write(self.directory / "performance-owner.json", json.dumps({**recorder.owner, "binding_id": "foreign"}))
            return result
        with patch.object(Path, "write_text", new=change_owner):
            with self.assertRaisesRegex(ValueError, "owner_missing_or_changed"):
                perf.append_record(self.directory, record)
        self.assertEqual(len(perf.read_records(self.directory, 0, 5)["records"]), 1)
        self.assertFalse((self.directory / "performance.latest.json").exists())
        self.assertEqual(list(self.directory.glob("*.tmp")), [])

    def test_failed_publication_removes_only_its_own_temporary(self):
        foreign = self.directory / "performance.latest.json.foreign.tmp"
        foreign.write_bytes(b"unrelated")
        with patch("process_performance.os.replace", side_effect=PermissionError(errno.EACCES, "held")):
            with self.assertRaises(PermissionError):
                perf.write_json(self.directory / "performance.latest.json", {"fixture": True})
        self.assertEqual(list(self.directory.glob("*.tmp")), [foreign])
        self.assertEqual(foreign.read_bytes(), b"unrelated")

    def test_compact_status_rejects_foreign_or_missing_identity_and_omits_raw_error(self):
        recorder = self.recorder([sample(1, 0), sample(2, 1)])
        recorder.observe({"kind": "world"})
        brief = recorder.brief()
        brief["collection_error"] = "raw command must never be projected" * 100
        status = perf.compact_status(brief, "run-a", "binding-a")
        self.assertNotIn("raw command", json.dumps(status))
        self.assertLess(len(json.dumps(status)), 1500)
        for malformed in (None, [], "not a clock"):
            changed = copy.deepcopy(brief); changed["latest"]["game_time"] = malformed
            self.assertIsNone(perf.compact_status(changed, "run-a", "binding-a")["latest_game_time"])
        for key, value in (("run_id", "foreign"), ("binding_id", "foreign"),
                           ("pid", None), ("process_identity", None), ("source_binding", None)):
            changed = copy.deepcopy(brief); changed["owner"][key] = value
            self.assertEqual(perf.compact_status(changed, "run-a", "binding-a")["status"], "unavailable")
        changed = copy.deepcopy(brief)
        changed["latest"]["owner"] = {**changed["latest"]["owner"], "process_identity": "reused"}
        self.assertEqual(perf.compact_status(changed, "run-a", "binding-a")["reason"], "performance_record_owner_mismatch")

    @unittest.skipUnless(os.name == "nt", "real Windows non-delete-sharing handle")
    def test_windows_real_sharing_refusal_keeps_journal_then_recovers(self):
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]; kernel.CloseHandle.restype = wintypes.BOOL
        recorder = self.recorder([sample(t, t) for t in range(1, 12)])
        recorder.observe({"kind": "world", "game_minutes": 100})
        latest = self.directory / "performance.latest.json"
        prior = perf.read_json(latest)
        handle = kernel.CreateFileW(str(latest), 0x80000000, 0x1 | 0x2, None, 3, 0x80, None)
        self.assertNotIn(handle, (None, wintypes.HANDLE(-1).value))
        try:
            for minute in (100, 101, 102):
                frame = {"kind": "world", "game_minutes": minute}
                recorder.begin_action(frame, "world.pause")
                recorder.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": minute + 1}})
                self.assertFalse(recorder.stopped)
                self.assertEqual(recorder.latest_publication["winerror"], 5)
                self.assertEqual(perf.read_json(latest), prior)
        finally:
            self.assertTrue(kernel.CloseHandle(handle))
        rows = perf.read_records(self.directory, 0, 20)["records"]
        self.assertEqual(len(rows), 4)
        self.assertEqual(sum(r["game_time"]["delta_minutes"] == 1 for r in rows), 3)
        recorder.observe({"kind": "world", "game_minutes": 103})
        self.assertEqual(perf.read_json(latest), recorder.latest)
        self.assertEqual(recorder.latest_publication["status"], "published")
        self.assertEqual(len(perf.read_records(self.directory, 0, 20)["records"]), 5)
        self.assertEqual(list(self.directory.glob("*.tmp")), [])

    def test_real_host_counter_measures_this_test_process_without_game(self):
        before = sample_child_resources(os.getpid())
        end = time.monotonic() + 0.12
        while time.monotonic() < end:
            sum(range(100))
        after = sample_child_resources(os.getpid())
        result = complete_child_resource_interval(before, after)
        self.assertEqual(result["cpu_percent"]["status"], "available", result)
        self.assertGreater(result["interval_cpu_seconds"], 0)
        self.assertGreater(result["resident_memory"]["value"], 0)

    def test_native_turn_assessment_is_incremental_idempotent_and_recovers(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.turn_event(1, "start", sequence=1, wall=10),
                          self.turn_event(1, "end", sequence=2, duration=.25, wall=10.25))
        first = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"},
                                             now_unix_seconds=10.3)
        self.assertEqual(first["status"], "measured")
        self.assertEqual(first["metric"]["sample_count"], 1)
        self.assertEqual(first["alarms"], [])
        self.assertEqual(first["metric"]["window_last"].split(":")[3], "end")
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"},
                                                      now_unix_seconds=10.4)["alarms"], [])

        self.append_trace(self.turn_event(2, "start", sequence=3, wall=11),
                          {**self.turn_event(2, "begin", sequence=4, phase="input", wall=11),
                           "event": "turn_phase"})
        input_wait = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"},
                                                  now_unix_seconds=20)
        self.assertEqual(input_wait["alarms"], [])
        self.assertEqual(input_wait["current_incomplete_turn"]["phase"], "input")

        self.append_trace({**self.turn_event(2, "end", sequence=5, phase="simulation", wall=20),
                           "event": "turn_phase"})
        with patch("process_performance.sample_child_resources", return_value=sample(20, 0)):
            stalled = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"},
                                                   now_unix_seconds=21)
        self.assertEqual({item["kind"] for item in stalled["alarms"]}, {"incomplete_turn", "stalled_progress"})
        self.append_trace(self.turn_event(2, "end", sequence=6, duration=.2, wall=21.2))
        recovered = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"},
                                                 now_unix_seconds=21.3)
        self.assertEqual({item["kind"] for item in recovered["recoveries"]}, {"recovery"})

    def test_long_input_is_not_charged_to_resumed_simulation(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(
            self.turn_event(3, "start", sequence=1, wall=30.0),
            {**self.turn_event(3, "begin", sequence=2, phase="input", wall=30.1), "event": "turn_phase"},
            {**self.turn_event(3, "end", sequence=3, phase="simulation", wall=90.1), "event": "turn_phase"},
        )
        result = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p3"},
                                              now_unix_seconds=90.3)
        self.assertEqual(result["alarms"], [])
        self.assertAlmostEqual(result["current_incomplete_turn"]["age_seconds"], .3)

    def test_journal_compaction_keeps_phase_samples_without_duplicate_alarms(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        header = {"event": "trace_generation", "run_id": "run-a", "generation": "first"}
        start = self.turn_event(1, "start", sequence=1, wall=10)
        phase_begin = {**self.turn_event(1, "begin", sequence=2, phase="input", wall=10.1),
                       "event": "turn_phase"}
        self.append_trace(header, start, phase_begin)
        first = perf.collect_turn_assessment(self.directory, "binding-a", now_unix_seconds=10.2)
        self.assertEqual(first["metric"]["sample_count"], 0)
        phase_end = {**self.turn_event(1, "end", sequence=3, wall=10.2), "event": "turn_phase"}
        end = self.turn_event(1, "end", sequence=4, duration=3, wall=13.2)
        path = self.directory / "semantic.native.events.jsonl"
        temporary = path.with_suffix(".next")
        # Replayed beginnings can appear before the old offset in a larger
        # compacted file. Native sequence/identity, not physical bytes, dedupes.
        temporary.write_text("".join(perf.TURN_TRACE_PREFIX + json.dumps(event) + "\n" for event in
                            ({**header, "generation": "second"}, start, phase_begin, phase_end, end)))
        temporary.replace(path)
        result = perf.collect_turn_assessment(self.directory, "binding-a", now_unix_seconds=13.2)
        self.assertEqual(result["metric"]["sample_count"], 1)
        state = perf.read_json(self.directory / perf.TURN_STATE_NAME)
        self.assertEqual(len(state["completed"][0]["phase_events"]), 2)
        self.assertTrue(any(alarm["kind"] == "spike" for alarm in result["alarms"]))
        repeat = perf.collect_turn_assessment(self.directory, "binding-a", now_unix_seconds=13.2)
        self.assertEqual(repeat["metric"]["sample_count"], 1)
        self.assertEqual(repeat["alarms"], [])

    def test_native_turn_producer_emits_parseable_precise_json(self):
        from startup_harness import repo_root
        source = (repo_root() / "src" / "do_turn.cpp").read_text(encoding="utf-8")
        start = source.index('stream << "{\\"event\\":')
        end = source.index(";\n    }", start)
        chain = source[start + len("stream << "):end]
        values = {
            "event": "turn", "stage": "start", "openclaw_harness_turn_trace_quote( run_id )": "run-a",
            "process_instance": "native-a", "++sequence": "1", "turn_id": "turn-1",
            "game_turn": "100", "game_minutes": "600", "wall_time": format(1700000000.125, ".17g"),
            "simulation_seconds": format(.125, ".17g"), "phase": "simulation",
            "waiting_generation": "1",
        }
        produced = []
        for expression in re.split(r"\s*<<\s*", chain):
            expression = " ".join(expression.strip().split())
            if expression.startswith('"'):
                produced.append(json.loads(expression))
            else:
                produced.append(values[expression])
        event = json.loads("".join(produced).removeprefix(perf.TURN_TRACE_PREFIX))
        self.assertEqual(event["turn_id"], "turn-1")
        self.assertEqual(event["game_turn"], 100)
        self.assertEqual(event["wall_time_seconds"], 1700000000.125)
        self.assertIn("std::setprecision( std::numeric_limits<double>::max_digits10 )", source)

    def test_activity_distraction_query_is_inside_the_input_phase(self):
        from startup_harness import repo_root
        source = (repo_root() / "src" / "do_turn.cpp").read_text(encoding="utf-8")
        start = source.index("if( u.activity ) {\n                for( std::pair<const distraction_type")
        end = source.index("\n            }\n        }\n    }", start)
        owner = source[start:end]
        self.assertIn("turn_trace.input_begin();\n                    const bool activity_cancelled =", owner)
        self.assertIn("cancel_activity_or_ignore_query( dist.first, dist.second );\n                    turn_trace.input_end();", owner)

    def test_native_spike_and_sustained_alarm_keep_exact_trace_handles(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.turn_event(1, "start", sequence=1),
                          self.turn_event(1, "end", sequence=2, duration=3.0),
                          self.turn_event(2, "start", sequence=3),
                          self.turn_event(2, "end", sequence=4, duration=1.5))
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual({alarm["kind"] for alarm in result["alarms"]},
                         {"spike", "slow_turn", "sustained_regression"})
        self.assertEqual(result["alarms"][0]["evidence"]["path"],
                         str(self.directory / "semantic.native.events.jsonl"))
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])

    def waiting_turns(self, first, durations, generation=1):
        for turn, duration in enumerate(durations, first):
            self.append_trace(
                {**self.turn_event(turn, "start", sequence=turn * 2, wall=turn * 60.0),
                 "waiting_generation": generation},
                {**self.turn_event(turn, "end", sequence=turn * 2 + 1,
                                   duration=duration, wall=turn * 60.0 + 59),
                 "waiting_generation": generation})

    def test_alarm_collect_ordinary_finish_and_cleanup_keep_original_bound_evidence(self):
        self.collect_finish_and_retrieve_alarm(early_spike=False)

    def test_early_spike_phase_survives_window_trim_finish_and_cleanup(self):
        self.collect_finish_and_retrieve_alarm(early_spike=True)

    def collect_finish_and_retrieve_alarm(self, *, early_spike):
        from cockpit import CockpitRunChannel, CockpitService
        from cockpit_witness_test import frame
        from play_cli import PlayerClient
        from evidence_display import recover, encoded, bounded
        import startup_harness

        session = self.directory / "session"
        session.mkdir()
        self.directory = session
        self.recorder([sample(1, 0)])
        self.turn_config()
        perf.write_json(session / "bridge.manifest.json", {"binding_id": "binding-a"})
        perf.write_json(session / "status.json", {"binding_id": "binding-a", "state": "ready"})
        perf.write_json(session / "play-client.json", {"binding_id": "binding-a", "pending": {
            "request_id": "trade-yes-a", "request": {"action": "game.act", "action_id": "menu.confirm"},
            "submitted_unix_seconds": 9.0,
        }})
        waiting = {} if early_spike else {"waiting_generation": 1}
        start = {**self.turn_event(1, "start", sequence=1, wall=10), **waiting}
        self.append_trace(start,
                          {**self.turn_event(1, "begin", sequence=2, phase="input", wall=10.1),
                           "event": "turn_phase"},
                          {**self.turn_event(1, "end", sequence=3, wall=12), "event": "turn_phase"},
                          {**self.turn_event(1, "end", sequence=4,
                                            duration=3 if early_spike else .2, wall=15), **waiting})
        if early_spike:
            for turn in range(2, 11):
                self.append_trace(self.turn_event(turn, "start", sequence=turn * 2 + 1),
                                  self.turn_event(turn, "end", sequence=turn * 2 + 2, duration=.01))
        else:
            self.waiting_turns(2, [.2] * 99)
        packet_raw = b'{"event":"raid_performance_interval","run_id":"run-a","rows":[[1,0.1]]}\n'
        with (session / "semantic.native.events.jsonl").open("ab") as stream:
            stream.write(packet_raw)
        client = PlayerClient(session)
        with patch("play_cli.Bridge.response_status", return_value={"ok": False, "error": "response_not_available_or_stale"}), \
                patch("play_cli.Bridge.send_request") as send:
            result = client.collect(wait_seconds=0)
        send.assert_not_called()
        packet_handle = result["turn_assessment"]["selected_native_packets"][0]
        import base64
        self.assertEqual(base64.b64decode(recover(packet_handle["retained_raw"]["sha256"], self.evidence_store)["raw_base64"]), packet_raw)
        self.assertTrue(result["performance_alarm"])
        kind = "spike" if early_spike else "waiting_slow"
        original = next(a for a in result["turn_assessment"]["alarms"] if a["kind"] == kind)
        if early_spike:
            self.assertEqual(result["turn_assessment"]["metric"]["sample_count"], 4)
            self.assertEqual(result["turn_assessment"]["metric"]["max_seconds"], .01)
        report_dir = session.parent / "reports"
        report_dir.mkdir()
        run_alive = {"pid": 73, "run_id": "run-a", "alive": True}
        channel = CockpitRunChannel(frame, binding_id="binding-a", read_process_state=lambda: run_alive,
            finalize_session=lambda report: startup_harness.finalize_cockpit_live_session(
                report_dir, 73, report, cleanup_process=False))
        service = CockpitService(run_channel=channel)
        service.call({"action": "game.observe"})
        run_alive["alive"] = False
        observed = service.call({"action": "game.observe"})["result"]
        with patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(session)}):
            reply = service.call({"action": "run.finish", "observation_id": observed["observation_id"],
                                  "stop_reason": "outcome_reported", "unused_authority": "released",
                                  "outcome": {"summary": "Paid goods home", "verdict": "positive"}})
        self.assertTrue(reply.get("ok"), reply)
        final = reply["result"]
        handle = final["performance_evidence"]["evidence"]
        self.assertLess(len(encoded(final["performance_evidence"])), 1200)
        self.assertLess(len(encoded(bounded(reply, budget=4096, directory=self.evidence_store))), 4097)
        self.assertEqual(client.state["pending"]["request_id"], "trade-yes-a")
        shutil.rmtree(session)
        cold_final = json.loads((report_dir / "cockpit.live.final.json").read_text())
        self.assertEqual(cold_final["performance_evidence"], final["performance_evidence"])
        index = recover(handle["sha256"], self.evidence_store)
        handle = next(handle for handle in index["alarms"] if handle["kind"] == kind)
        alarm = recover(handle["evidence"]["sha256"], self.evidence_store)
        self.assertEqual(alarm["alarm"], original)
        self.assertEqual(alarm["owner"]["run_id"], "run-a")
        self.assertEqual(alarm["owner"]["binding_id"], "binding-a")
        self.assertEqual(alarm["owner"]["source_binding"], {"runtime_binding_sha256": "binary-a"})
        self.assertEqual(alarm["operation"]["request_id"], "trade-yes-a")
        self.assertEqual(alarm["window"]["sample_count"], 1 if early_spike else 100)
        self.assertEqual(len(alarm["measurements"]), 1 if early_spike else 100)
        self.assertEqual(alarm["measurements"][-1]["game_turn"], 101 if early_spike else 200)
        if early_spike:
            self.assertEqual(alarm["measurements"][0]["simulation_seconds"], 3)
            slow_handle = next(handle for handle in index["alarms"] if handle["kind"] == "slow_turn")
            slow = recover(slow_handle["evidence"]["sha256"], self.evidence_store)
            self.assertEqual(slow["measurements"], alarm["measurements"])
            self.assertEqual(slow["window"], alarm["window"])
            self.assertIn(slow["alarm"], result["turn_assessment"]["alarms"])
        self.assertEqual([event["stage"] for event in alarm["measurements"][0]["phase_events"]],
                         ["begin", "end"])
        self.assertEqual(alarm["measurements"][0]["phase_events"][0]["phase"], "input")
        self.assertIn("end", alarm["measurements"][-1])
        self.assertEqual(alarm["phase_scope"], "Only native phase facts observed in this alarm window; not a complete subphase profile.")
        self.assertEqual(final["stop_detail"]["gameplay_outcome"]["verdict"], "positive")
        self.assertNotIn("target_predicate_receipt", final["stop_detail"])

    def test_retained_alarm_is_original_across_recovery_duplicate_poll_and_foreign_publish(self):
        from evidence_display import recover
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.waiting_turns(1, [.2] * 100)
        original = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"})
        first = perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")
        index = recover(first["evidence"]["sha256"], self.evidence_store)
        before = recover(index["alarms"][0]["evidence"]["sha256"], self.evidence_store)
        self.assertEqual(before["alarm"], original["alarms"][0])
        self.assertEqual(before["measurements"][0]["phase_events"], [])
        self.assertEqual(before["window"]["first"], before["measurements"][0]["measurement_id"])
        files = set(self.evidence_store.iterdir())
        duplicate = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p2"})
        self.assertEqual(duplicate["alarms"], [])
        self.assertEqual(perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a"), first)
        self.assertEqual(set(self.evidence_store.iterdir()), files)
        self.waiting_turns(101, [.001] * 100)
        recovered = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p3"})
        self.assertEqual(recovered["recoveries"][0]["kind"], "waiting_recovery")
        self.assertEqual(perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a"), first)
        self.assertEqual(recover(index["alarms"][0]["evidence"]["sha256"], self.evidence_store), before)
        for run, binding in [("foreign", "binding-a"), ("run-a", "foreign")]:
            result = perf.publish_alarm_evidence(self.directory, run_id=run, binding_id=binding)
            self.assertEqual(result["status"], "unavailable")
            self.assertNotIn("evidence", result)

    def test_alarm_retention_failure_is_honest_and_retry_keeps_original_context(self):
        from evidence_display import recover
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.waiting_turns(1, [.2] * 100)
        with patch("evidence_display.retain", side_effect=OSError("fixture storage refused")):
            failed = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p1"})
            published = perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")
        self.assertEqual(failed["alarms"][0]["kind"], "waiting_slow")
        self.assertEqual(failed["alarm_retention"]["status"], "failed")
        self.assertEqual(published["status"], "failed")
        self.assertNotIn("evidence", published)
        retried = perf.collect_turn_assessment(self.directory, "binding-a", pending={"request_id": "p2"})
        self.assertEqual(retried["alarms"], [])
        self.assertEqual(retried["alarm_retention"]["status"], "retained")
        final = perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")
        index = recover(final["evidence"]["sha256"], self.evidence_store)
        record = recover(index["alarms"][0]["evidence"]["sha256"], self.evidence_store)
        self.assertEqual(record["operation"], {"request_id": "p1"})
        self.assertEqual(record["alarm"], failed["alarms"][0])
        with patch("evidence_display.retain", side_effect=OSError("fixture index publication refused")):
            self.assertEqual(perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")["status"], "failed")

    def test_one_poll_keeps_each_original_waiting_crossing_window(self):
        from evidence_display import recover
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.waiting_turns(1, [.2] * 100 + [.001] * 100 + [.4] * 100)
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        alarms = [alarm for alarm in result["alarms"] if alarm["kind"] == "waiting_slow"]
        self.assertEqual(len(alarms), 2)
        final = perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")
        index = recover(final["evidence"]["sha256"], self.evidence_store)
        records = [recover(handle["evidence"]["sha256"], self.evidence_store)
                   for handle in index["alarms"] if handle["kind"] == "waiting_slow"]
        self.assertEqual(len(records), 2)
        for record in records:
            self.assertIn(record["alarm"], alarms)
            self.assertEqual(record["window"]["sample_count"], 100)
            self.assertAlmostEqual(sum(item["simulation_seconds"] for item in record["measurements"]) / 100,
                                   record["alarm"]["mean_seconds"])
        first = min(records, key=lambda record: record["measurements"][-1]["game_turn"])
        self.assertEqual(first["measurements"][0]["game_turn"], 101)
        self.assertEqual(first["measurements"][-1]["game_turn"], 200)
        self.assertNotEqual(records[0]["identity"], records[1]["identity"])

    def test_no_alarm_retains_no_payload_and_single_turn_alarm_has_real_window(self):
        from evidence_display import recover
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.turn_event(1, "start", sequence=1),
                          self.turn_event(1, "end", sequence=2, duration=.02))
        quiet = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(quiet["alarm_retention"]["status"], "none_observed")
        self.assertFalse(self.evidence_store.exists())
        self.append_trace(self.turn_event(2, "start", sequence=3),
                          self.turn_event(2, "end", sequence=4, duration=3))
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        final = perf.publish_alarm_evidence(self.directory, run_id="run-a", binding_id="binding-a")
        index = recover(final["evidence"]["sha256"], self.evidence_store)
        self.assertEqual(len(index["alarms"]), 2)
        for handle in index["alarms"]:
            record = recover(handle["evidence"]["sha256"], self.evidence_store)
            self.assertIn(record["alarm"], result["alarms"])
            self.assertEqual(record["window"]["sample_count"], 1)
            self.assertEqual(record["measurements"][0]["simulation_seconds"], 3)
            self.assertEqual(record["operation"], None)

    def test_waiting_alarm_crossing_recovery_and_duplicate_collection(self):
        self.recorder([sample(1, 0)])
        self.turn_config()  # Four-turn generic window must not shorten waiting's 100 turns.
        self.waiting_turns(1, [.122] * 99)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])
        self.waiting_turns(100, [.122])
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual([a["kind"] for a in result["alarms"]], ["waiting_slow"])
        self.assertAlmostEqual(result["alarms"][0]["mean_seconds"], .122)
        self.assertIn("coordinator", result["alarms"][0]["message"])
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])
        self.waiting_turns(101, [.122] * 10)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])
        self.waiting_turns(111, [.001] * 100)
        recovered = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual([a["kind"] for a in recovered["recoveries"]], ["waiting_recovery"])
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["recoveries"], [])

    def test_waiting_threshold_spike_and_input_gaps(self):
        self.recorder([sample(1, 0)])
        self.waiting_turns(1, [.100] * 100)
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(result["alarms"], [])
        self.assertAlmostEqual(result["waiting"]["mean_seconds"], .100)
        self.waiting_turns(101, [.001] * 99 + [.050], generation=2)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])

    def test_waiting_does_not_include_other_activity_or_previous_wait(self):
        self.recorder([sample(1, 0)])
        self.waiting_turns(1, [2.0] * 100, generation=0)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["waiting"], {})
        self.waiting_turns(101, [.020] * 99, generation=1)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])
        self.waiting_turns(200, [.020], generation=2)
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["alarms"], [])

    def test_missing_configuration_and_trace_rewind_are_honest(self):
        self.recorder([sample(1, 0)])
        self.append_trace(self.turn_event(1, "start", sequence=1),
                          self.turn_event(1, "end", sequence=2, duration=.2))
        missing = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(missing["status"], "measured_unassessed")
        self.assertEqual(missing["configuration"]["status"], "unavailable")
        self.turn_config()
        self.append_trace(self.turn_event(2, "start", sequence=3),
                          self.turn_event(2, "end", sequence=4, duration=.2))
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["metric"]["sample_count"], 2)
        (self.directory / "semantic.native.events.jsonl").write_text("")
        rewound = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(rewound["metric"]["sample_count"], 0)

    def test_changed_native_process_instance_resets_the_compact_window(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.turn_event(1, "start", sequence=1),
                          self.turn_event(1, "end", sequence=2, duration=.2))
        self.assertEqual(perf.collect_turn_assessment(self.directory, "binding-a")["metric"]["sample_count"], 1)
        fresh_start = {**self.turn_event(2, "start", sequence=1), "process_instance": "native-b"}
        fresh_end = {**self.turn_event(2, "end", sequence=2, duration=.3), "process_instance": "native-b"}
        self.append_trace(fresh_start, fresh_end)
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(result["metric"]["sample_count"], 1)
        self.assertEqual(perf.read_json(self.directory / perf.TURN_STATE_NAME)["reset_reason"],
                         "native_process_instance_changed")

    def test_no_turn_start_after_accepted_native_operation_alarms_but_transport_only_is_uncertain(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.turn_event(1, "start", sequence=1, wall=9.0),
                          {**self.turn_event(1, "begin", sequence=2, phase="input", wall=9.1),
                           "event": "turn_phase"},
                          self.turn_event(1, "end", sequence=3, duration=.2, wall=9.5),
                          self.semantic_event("surface_receipt", wall=10.1))
        pending = {"request_id": "p1", "submitted_unix_seconds": 10.0}
        with patch("process_performance.sample_child_resources", return_value=sample(12, 0)):
            no_start = perf.collect_turn_assessment(self.directory, "binding-a", pending=pending,
                                                    now_unix_seconds=12.0)
        self.assertEqual(no_start["alarms"][0]["kind"], "stalled_progress")
        self.assertEqual(no_start["alarms"][0]["progress_state"], "accepted_operation_no_new_turn")
        self.assertIsNone(no_start["observation_uncertainty"])
        self.assertIsNone(no_start["current_incomplete_turn"])

        self.append_trace(self.semantic_event("request_transport", request_id="p2", wall=20.1))
        transport = perf.collect_turn_assessment(self.directory, "binding-a",
                                                 pending={"request_id": "p2", "submitted_unix_seconds": 20.0},
                                                 now_unix_seconds=22.0)
        self.assertEqual(transport["alarms"], [])
        self.assertEqual(transport["observation_uncertainty"], "native_transport_seen_but_not_accepted")

    def test_accepted_operation_to_native_menu_is_input_owner_not_stalled_simulation(self):
        self.recorder([sample(1, 0)])
        self.turn_config()
        self.append_trace(self.semantic_event("surface_receipt", wall=10.1),
                          self.surface_descriptor_event(kind="menu", wall=10.2))
        result = perf.collect_turn_assessment(
            self.directory, "binding-a",
            pending={"request_id": "p1", "submitted_unix_seconds": 10.0},
            now_unix_seconds=12.0,
        )
        self.assertEqual(result["alarms"], [])
        self.assertEqual(result["observation_uncertainty"],
                         "native_input_owner_no_simulation_progress")
        self.assertEqual(perf.read_json(self.directory / perf.TURN_STATE_NAME)
                         ["last_surface_descriptor"]["kind"], "menu")

    def test_metric_reports_native_wall_turn_throughput_and_bound_cpu_without_new_sampling(self):
        recorder = self.recorder([sample(1, 0)])
        self.turn_config()
        perf.append_record(self.directory, {"record_id": "resource-1", "owner": recorder.owner,
                                            "resources": {"interval_wall_seconds": 0.75,
                                                          "interval_cpu_seconds": 0.5}})
        self.append_trace(self.turn_event(1, "start", sequence=1, wall=10.0),
                          self.turn_event(1, "end", sequence=2, duration=.2, wall=10.25),
                          self.turn_event(2, "start", sequence=3, wall=10.25),
                          self.turn_event(2, "end", sequence=4, duration=.3, wall=10.75))
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        metric = result["metric"]
        self.assertEqual(metric["elapsed_wall_seconds"], .75)
        self.assertEqual(metric["cpu_seconds"], .5)
        self.assertEqual(metric["game_turns_advanced"], 2)
        self.assertEqual(metric["game_turns_per_wall_second"], 2 / .75)
        self.assertEqual(metric["game_turns_per_simulation_second"], 2 / .5)
        self.assertEqual(metric["game_turns_per_wall_second_scope"],
                         "window_wall_span_including_inter_turn_input_gaps_not_simulation_throughput")
        self.assertEqual(metric["simulation_throughput_scope"],
                         "completed_native_turn_simulation_seconds_only")
        self.assertEqual(metric["cpu_evidence"]["record_id"], "resource-1")
        self.assertEqual(metric["cpu_evidence"]["journal"], str(self.directory / "performance.jsonl"))

    def test_representative_workload_comparison_preserves_provenance_and_metrics(self):
        recorder = self.recorder([sample(1, 0)])
        workload = {
            "scenario": "loaded-hostile-representative",
            "save": "McWilliams",
            "seed": "fixture-seed-1",
            "actor_count": 7,
            "source_count": 12,
            "diagnostic": "off",
            "phase": "warm",
            "operation": "advancing_turn",
        }
        binding = {
            "source_binding": recorder.owner["source_binding"],
            "host": recorder.owner["host"],
            "machine": recorder.owner["machine"],
            "platform": recorder.owner["platform"],
            "workload": workload,
        }
        self.turn_config(workload=workload, baseline={"reference": "representative-baseline-v1",
                                                        "binding": binding})
        perf.append_record(self.directory, {"record_id": "resource-representative", "owner": recorder.owner,
                                            "resources": {"interval_wall_seconds": 0.75,
                                                          "interval_cpu_seconds": 0.5}})
        self.append_trace(self.turn_event(1, "start", sequence=1, wall=10.0),
                          self.turn_event(1, "end", sequence=2, duration=.2, wall=10.25),
                          self.turn_event(2, "start", sequence=3, wall=10.25),
                          self.turn_event(2, "end", sequence=4, duration=.3, wall=10.75))
        result = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(result["status"], "measured")
        self.assertEqual(result["baseline"]["status"], "comparable")
        self.assertEqual(result["baseline"]["reference"], "representative-baseline-v1")
        self.assertEqual(result["baseline"]["measurement_binding"]["workload"], workload)
        metric = result["metric"]
        self.assertEqual(metric["sample_count"], 2)
        self.assertAlmostEqual(metric["mean_seconds"], .25)
        self.assertEqual(metric["tail_percentile"], .95)
        self.assertEqual(metric["tail_seconds"], .3)
        self.assertEqual(metric["max_seconds"], .3)
        self.assertEqual(metric["elapsed_wall_seconds"], .75)
        self.assertEqual(metric["cpu_seconds"], .5)
        self.assertEqual(metric["game_turns_per_wall_second"], 2 / .75)
        self.assertEqual(metric["game_minutes_per_wall_second"], 1 / .75)
        self.assertEqual(metric["game_turns_per_simulation_second"], 2 / .5)
        self.assertEqual(metric["game_minutes_per_simulation_second"], 1 / .5)

        # Changing one workload premise must not silently become a comparison.
        self.turn_config(workload=workload, baseline={
            "reference": "changed-baseline",
            "binding": {**binding, "workload": {**workload, "actor_count": 8}},
        })
        incompatible = perf.collect_turn_assessment(self.directory, "binding-a")
        self.assertEqual(incompatible["baseline"]["status"], "incompatible")
        self.assertEqual(incompatible["baseline"]["reason"], "baseline_binding_differs")

    def test_simulation_throughput_ignores_input_gaps_and_wall_span_exposes_them(self):
        def measure(gap):
            directory = Path(tempfile.mkdtemp(dir=self.temp.name))
            recorder = perf.ProcessPerformance(directory, pid=73, run_id="run-a", binding_id="binding-a",
                                               source_binding={"runtime_binding_sha256": "binary-a"},
                                               sampler=lambda _: sample(1, 0))
            self.addCleanup(lambda path=directory: shutil.rmtree(path, ignore_errors=True))
            self.turn_config(directory=directory)
            perf.append_record(directory, {"record_id": "resource", "owner": recorder.owner,
                                           "resources": {"interval_wall_seconds": .75,
                                                         "interval_cpu_seconds": .5}})
            self.append_trace(self.turn_event(1, "start", sequence=1, wall=10.0),
                              self.turn_event(1, "end", sequence=2, duration=.2, wall=10.25),
                              self.turn_event(2, "start", sequence=3, wall=10.25 + gap),
                              self.turn_event(2, "end", sequence=4, duration=.3, wall=10.75 + gap),
                              directory=directory)
            return perf.collect_turn_assessment(directory, "binding-a")["metric"]

        short_gap = measure(0.0)
        long_gap = measure(100.0)
        self.assertEqual(short_gap["game_turns_per_simulation_second"],
                         long_gap["game_turns_per_simulation_second"])
        self.assertEqual(short_gap["game_minutes_per_simulation_second"],
                         long_gap["game_minutes_per_simulation_second"])
        self.assertNotEqual(short_gap["game_turns_per_wall_second"],
                            long_gap["game_turns_per_wall_second"])
        self.assertNotEqual(short_gap["game_minutes_per_wall_second"],
                            long_gap["game_minutes_per_wall_second"])
        self.assertIsNone(perf._finite_rate(2, 0))
        self.assertIsNone(perf._finite_rate(float("inf"), 1))

    def test_startup_wraps_exact_native_dispatch_and_finalizer(self):
        import startup_harness as startup
        from unittest.mock import MagicMock
        recorder = MagicMock()
        frame = {"frame_id": "frame-a", "game_minutes": 100}
        outcome = {"accepted": True, "next_frame": {"frame_id": "frame-b", "game_minutes": 101}}
        with patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding-a",
                                     "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(self.directory)}), \
                patch.object(startup, "pid_is_alive", return_value=True), \
                patch.object(startup, "pid_command", return_value="owned-game"), \
                patch.object(startup, "current_owned_process_generation", return_value={
                    "status": "alive", "pid": 73, "expected": {
                        "pid": 73, "birth_identity": "birth-a", "command": "owned-game"}}), \
                patch("process_performance.ProcessPerformance", return_value=recorder), \
                patch.object(startup, "execute_semantic_act", return_value=outcome), \
                patch.object(startup, "finalize_cockpit_live_session", return_value={"cleaned": True}):
            service = startup.open_cockpit_game_service(profile="test", run_dir=self.directory,
                        run_id="run-a", trace_start_offset=0, pid=73, session_id="test", live_session=True)
            self.addCleanup(service.live_channel.archive.close)
            result = service.live_channel._dispatch_advertised_action(frame, "world.pause")
            self.assertEqual(result, outcome)
            recorder.begin_action.assert_called_once_with(frame, "world.pause")
            recorder.end_action.assert_called_once_with(frame, outcome)
            service.live_channel._finalize_session({})
            recorder.stop.assert_called_once()

    def test_startup_advancing_dispatch_append_latest_failure_and_compact_observation(self):
        import startup_harness as startup
        recorder = self.recorder([sample(t, t) for t in range(1, 20)])
        frame = {"event": "surface_descriptor", "schema_version": 1, "run_id": "run-a",
            "frame_id": "frame-a", "surface_id": "world-a", "kind": "world", "game_minutes": 100, "breadcrumbs": ["World"],
            "payload": {}, "valid_actions": [{"id": "world.pause", "label": "Pause", "enabled": True}]}
        successor = {**frame, "frame_id": "frame-b", "surface_id": "world-b", "game_minutes": 101}
        outcome = {"accepted": True, "next_frame": successor}
        replace = os.replace
        def blocked(source, destination):
            if Path(destination).name == "performance.latest.json":
                raise PermissionError(errno.EACCES, "controlled filesystem publication refusal")
            return replace(source, destination)
        with patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "binding-a",
                                     "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(self.directory)}), \
                patch.object(startup, "pid_is_alive", return_value=True), \
                patch.object(startup, "pid_command", return_value="owned-game"), \
                patch.object(startup, "current_owned_process_generation", return_value={
                    "status": "alive", "pid": 73, "expected": {
                        "pid": 73, "birth_identity": "birth-a", "command": "owned-game"}}), \
                patch("process_performance.ProcessPerformance", return_value=recorder), \
                patch.object(startup, "compact_cockpit_live_evidence", return_value={}), \
                patch.object(startup, "execute_semantic_act", return_value=outcome):
            service = startup.open_cockpit_game_service(profile="test", run_dir=self.directory,
                run_id="run-a", trace_start_offset=0, pid=73, session_id="test", live_session=True)
            self.addCleanup(service.live_channel.archive.close)
            with patch("process_performance.os.replace", side_effect=blocked):
                self.assertEqual(service.live_channel._dispatch_advertised_action(frame, "world.pause"), outcome)
                service.live_channel._read_native_frame = lambda: successor
                observed = service.live_channel.observe()
            status = observed["compact_log"]["performance"]
            self.assertEqual(status["owner"]["run_id"], "run-a")
            self.assertEqual(status["latest_publication"]["status"], "unavailable")
            self.assertEqual(status["status"], "recording")
            rows = perf.read_records(self.directory, 0, 20)["records"]
            advancing = [r for r in rows if r["game_time"]["delta_minutes"] == 1]
            self.assertEqual(len(advancing), 1)
            self.assertEqual(advancing[0]["owner"], recorder.owner)
            self.assertEqual(advancing[0]["context"]["action_id"], "world.pause")
            self.assertFalse((self.directory / "performance.latest.json").exists())
            recovered = service.live_channel.observe()["compact_log"]["performance"]
            self.assertEqual(recovered["latest_publication"]["status"], "published")
            self.assertEqual(perf.read_json(self.directory / "performance.latest.json"), recorder.latest)
            self.assertEqual(sum(r["game_time"]["delta_minutes"] == 1 for r in
                perf.read_records(self.directory, 0, 20)["records"]), 1)


if __name__ == "__main__":
    unittest.main()
