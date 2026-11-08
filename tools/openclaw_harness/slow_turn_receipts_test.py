"""Run-bound slow receipt replacement and failure behavior."""
import copy
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

import process_performance as perf
import process_performance_test as performance_fixture


def packet(sequence=1):
    ident = {"turn_id": 2, "game_turn": 102, "start_event_sequence": 3, "end_event_sequence": 4}
    return {"event": "native_slow_turn_receipts", "schema": "caol-native-slow-turn-receipts-v1",
            "run_id": "run-a", "native_process_instance": "native-a", "native_pid": 73,
            "snapshot_sequence": sequence, "publication_interval_seconds": 30,
            "prior_run_pending_unpublished": False,
            "monitoring_enabled": True,
            "generic_config_status": "available",
            "unpublished_updates": "unknown_after_publication_until_next_success",
            "dropped_alarm_count": 0, "configured_slow_turn_seconds": 1.0,
            "configured_spike_seconds": 2.0, "configured_sustained_turns": 2,
            "cpu_scope": "process_simulation_excluding_tracked_input_and_save",
            "stage_scope": "inclusive_wall_seconds_not_additive", "records": [{
                "alarm_kind": 1, "dominant_stage": 2, "metric_epoch": 1, "count": 1,
                "first": ident, "last": ident, "worst_first": ident, "worst_last": ident,
                "dominant": ident, "sample_count": 1, "threshold_seconds": 2.0,
                "simulation_seconds": 3.0, "dominant_simulation_seconds": 3.0,
                "simulation_scope": "individual_turn", "other_metrics_scope": "dominant_turn",
                "count_scope": "qualifying_native_turns",
                "start_wall_seconds": 10.0, "end_wall_seconds": 13.0,
                "process_cpu_seconds": None, "input_seconds": None, "save_seconds": None,
                "metric_unavailable_reason": "native_diagnostic_metric_unavailable",
                "stage_ids": [2, 0, 1], "stage_names": ["population", "operations", "hordes"],
                "stage_seconds": [2.0, 0.2, 0.1],
                "workload_count_names": ["discovered_sources", "horde_packets", "human_nearby_candidates",
                                         "watch_record_view_calls", "debug_logical_bytes", "debug_stream_flushes"],
                "workload_counts": [0, 0, 2, 1, 10, 1]}]}


class SlowReceiptsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        perf.ProcessPerformance(self.root, pid=73, run_id="run-a", binding_id="binding-a",
                                source_binding={"runtime_binding_sha256": "binary-a"},
                                sampler=lambda _: performance_fixture.sample(1, 0))
        self.owner = perf.read_json(self.root / "performance-owner.json")
        self.trace = self.root / "semantic.native.events.jsonl"

    def append(self, value):
        with self.trace.open("a") as stream:
            stream.write(json.dumps(value, separators=(",", ":")) + "\n")

    def test_rotation_replacement_identity_and_nullable_cpu(self):
        first = packet()
        self.append(first)
        result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual(result["native_slow_receipts"]["snapshot"], first)
        self.assertIsNone(first["records"][0]["process_cpu_seconds"])
        updated = packet(2)
        updated["records"][0]["count"] = 9000
        self.append(updated)
        result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual(result["native_slow_receipts"]["snapshot"], updated)
        self.trace.unlink()
        self.trace.write_text("")
        result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual(result["native_slow_receipts"]["snapshot"], updated)
        foreign = copy.deepcopy(updated)
        foreign["snapshot_sequence"] = 3
        foreign["native_process_instance"] = "foreign"
        self.append(foreign)
        result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual(result["native_slow_receipts"]["snapshot"], updated)
        self.assertEqual(result["native_slow_receipts"]["warning"], "foreign_or_invalid_native_slow_receipts")
        for invalid in (dict(updated, native_pid=74), dict(updated, records=updated["records"] * 9),
                        dict(updated, native_process_instance="x" * 129)):
            self.assertFalse(perf._valid_native_slow_receipts(invalid, self.owner, None))
        from cockpit_archive import Archive, ArchiveMap
        archive = Archive(self.root / "cockpit-evidence.sqlite", run_id="run-a", binding_id="binding-a")
        try:
            self.assertEqual(len(ArchiveMap(archive, "native_slow_turn_receipts")), 1)
        finally:
            archive.close()

    def test_failed_final_write_preserves_old_cursor_and_retry(self):
        self.append(packet())
        self.assertEqual(perf.collect_turn_assessment(self.root, "binding-a")["native_slow_receipts"]["status"], "retained")
        state_path = self.root / perf.TURN_STATE_NAME
        before = state_path.read_bytes()
        self.append(packet(2))
        final = packet(3)
        self.append(final)
        from cockpit_archive import ArchiveMap
        old = ArchiveMap.__setitem__
        writes = []
        def fail_selected(archive_map, key, value):
            if isinstance(value, dict) and value.get("snapshot", {}).get("schema") == "caol-native-slow-turn-receipts-v1":
                writes.append(value["snapshot"]["snapshot_sequence"])
                raise OSError("controlled failure")
            return old(archive_map, key, value)
        with patch.object(ArchiveMap, "__setitem__", new=fail_selected):
            result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual(result["reason"], "native_slow_receipts_retention_failed")
        self.assertEqual(writes, [3])
        self.assertEqual(state_path.read_bytes(), before)
        self.assertEqual(perf.collect_turn_assessment(self.root, "binding-a")["native_slow_receipts"]["snapshot"], final)

    def test_completed_alarms_use_native_replacement_without_per_alarm_archive(self):
        performance_fixture.PerformanceTest.turn_config(self, directory=self.root)
        self.append(packet())
        for value in (performance_fixture.PerformanceTest.turn_event(self, 2, "start", sequence=3, wall=10),
                      performance_fixture.PerformanceTest.turn_event(self, 2, "end", sequence=4, duration=3, wall=13)):
            with self.trace.open("a") as out:
                out.write(perf.TURN_TRACE_PREFIX + json.dumps(value) + "\n")
        with patch("evidence_display.retain", side_effect=AssertionError("unexpected per-alarm write")):
            result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual({a["kind"] for a in result["alarms"]}, {"spike", "slow_turn"})
        self.assertEqual(result["alarm_retention"]["native_coalesced_alarm_count_this_collection"], 2)
        self.assertEqual(result["native_slow_receipts"]["summary"]["recurrence_count"], 1)
        self.assertEqual(result["native_slow_receipts"]["freshness"],
                         "last_successful_publication_pending_updates_unknown")

    def test_changed_config_is_bounded_and_reports_unavailable_evidence(self):
        performance_fixture.PerformanceTest.turn_config(self, directory=self.root)
        stale = packet()
        stale["configured_spike_seconds"] = 3.5
        self.append(stale)
        for value in (performance_fixture.PerformanceTest.turn_event(self, 2, "start", sequence=3, wall=10),
                      performance_fixture.PerformanceTest.turn_event(self, 2, "end", sequence=4, duration=3, wall=13)):
            with self.trace.open("a") as out:
                out.write(perf.TURN_TRACE_PREFIX + json.dumps(value) + "\n")
        with patch("evidence_display.retain", side_effect=AssertionError("unexpected per-alarm write")):
            result = perf.collect_turn_assessment(self.root, "binding-a")
        self.assertEqual({a["kind"] for a in result["alarms"]}, {"spike", "slow_turn"})
        self.assertEqual(result["alarm_retention"]["status"], "configuration_unmatched")
        self.assertEqual(result["alarm_retention"]["native_configuration_unmatched_count_this_collection"], 2)
        self.assertEqual(result["alarm_retention"]["native_configuration_unmatched_reason"],
                         "native_and_assessment_config_differ_evidence_unavailable")

    def test_active_alarm_configuration_change_counts_each_poll_until_refresh(self):
        performance_fixture.PerformanceTest.turn_config(self, directory=self.root)
        config_path = self.root / perf.TURN_CONFIG_NAME
        config = perf.read_json(config_path)
        config["window_turns"] = 256
        config["sustained_turns"] = 256
        perf.write_json(config_path, config)
        initial = packet()
        initial["configured_sustained_turns"] = 256
        self.append(initial)

        def add_turn(turn, duration):
            with self.trace.open("a") as out:
                for event in (performance_fixture.PerformanceTest.turn_event(
                        self, turn, "start", sequence=turn * 2 + 1, wall=turn * 4),
                              performance_fixture.PerformanceTest.turn_event(
                        self, turn, "end", sequence=turn * 2 + 2,
                        duration=duration, wall=turn * 4 + duration)):
                    out.write(perf.TURN_TRACE_PREFIX + json.dumps(event) + "\n")

        with patch("evidence_display.retain", side_effect=AssertionError("unexpected per-alarm write")):
            add_turn(1, 3)
            first = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(first["alarm_retention"]["native_coalesced_alarm_count_this_collection"], 2)
            self.assertEqual(len(first["alarms"]), 2)
            config["spike_seconds"] = 2.5
            perf.write_json(config_path, config)
            add_turn(2, 3)
            mismatch = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(mismatch["alarms"], [])  # active delivery remains deduplicated
            self.assertEqual(mismatch["alarm_retention"]["status"], "configuration_unmatched")
            self.assertEqual(mismatch["alarm_retention"]["native_configuration_unmatched_count_this_collection"], 2)
            idle = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(idle["alarm_retention"]["native_configuration_unmatched_count_this_collection"], 0)
            add_turn(3, 3)
            repeated = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(repeated["alarms"], [])
            self.assertEqual(repeated["alarm_retention"]["native_configuration_unmatched_count_this_collection"], 2)
            refreshed = packet(sequence=2)
            refreshed["configured_sustained_turns"] = 256
            refreshed["configured_spike_seconds"] = 2.5
            self.append(refreshed)
            add_turn(4, 3)
            matched = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(matched["alarms"], [])
            self.assertEqual(matched["alarm_retention"]["native_configuration_unmatched_count_this_collection"], 0)
            self.assertEqual(matched["alarm_retention"]["native_coalesced_alarm_count_this_collection"], 2)
            add_turn(5, .1)
            recovered = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertEqual(recovered["alarms"], [])
            self.assertEqual(len(recovered["recoveries"]), 2)

    def test_sustained_window_and_bounded_detail_eviction_are_explicit(self):
        value = packet()
        row = value["records"][0]
        row.update(alarm_kind=4, sample_count=2, count_scope="qualifying_native_windows",
                   simulation_scope="sliding_sustained_window_mean_per_turn",
                   dominant_stage=65535, stage_ids=[65535] * 3,
                   stage_names=["unavailable"] * 3, stage_seconds=None, workload_counts=None,
                   metric_unavailable_reason="bounded_candidate_detail_evicted")
        row["worst_first"] = {"turn_id": 1, "game_turn": 101,
                              "start_event_sequence": 1, "end_event_sequence": 2}
        self.assertTrue(perf._valid_native_slow_receipts(value, self.owner, None))
        row["metric_unavailable_reason"] = None
        self.assertFalse(perf._valid_native_slow_receipts(value, self.owner, None))

    def test_long_repeated_selected_alarms_do_not_emit_per_turn_artifacts(self):
        performance_fixture.PerformanceTest.turn_config(self, directory=self.root)
        self.append(packet())
        with self.trace.open("a") as out:
            for turn in range(1, 1001):
                for value in (performance_fixture.PerformanceTest.turn_event(self, turn, "start", sequence=turn * 2 - 1, wall=turn * 4),
                              performance_fixture.PerformanceTest.turn_event(self, turn, "end", sequence=turn * 2, duration=3, wall=turn * 4 + 3)):
                    out.write(perf.TURN_TRACE_PREFIX + json.dumps(value) + "\n")
        with patch("evidence_display.retain", side_effect=AssertionError("unexpected per-turn artifact")):
            result = perf.collect_turn_assessment(self.root, "binding-a")
            kinds = {a["kind"]: a for a in result["alarms"]}
            self.assertEqual(kinds["spike"]["coalesced_observed_count_this_poll"], 1000)
            self.assertEqual(kinds["slow_turn"]["coalesced_observed_count_this_poll"], 1000)
            self.assertLessEqual(len(result["alarms"]), 3)
            self.assertEqual(result["alarm_retention"]["native_coalesced_alarm_count_this_collection"], 2001)
            unchanged = perf.collect_turn_assessment(self.root, "binding-a")
            self.assertFalse(unchanged["recoveries"])
            self.assertEqual(unchanged["alarms"], [])


class NativeProducedSlowReceiptsTest(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("CAOL_NATIVE_SLOW_FIXTURE"), "native producer fixture not supplied")
    def test_native_snapshot_survives_consumer_rotation(self):
        source = Path(os.environ["CAOL_NATIVE_SLOW_FIXTURE"])
        rows = [json.loads(line) for line in source.read_text().splitlines()
                if line.startswith('{"event":"native_slow_turn_receipts"')]
        self.assertTrue(rows)
        native = rows[-1]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            trace = root / "native.jsonl"
            shutil.copyfile(source, trace)
            perf.ProcessPerformance(root, pid=native["native_pid"], run_id=native["run_id"], binding_id="binding",
                                    source_binding={"runtime_binding_sha256": "fixture"},
                                    sampler=lambda _: {**performance_fixture.sample(1, 0), "pid": native["native_pid"]})
            with patch.object(perf, "_trace_path", return_value=trace):
                first = perf.collect_turn_assessment(root, "binding")
                self.assertEqual(first["native_slow_receipts"]["snapshot"], native)
                self.assertLessEqual(len(json.dumps(native).encode()), 32768)
                self.assertLessEqual(len(native["records"]), 8)
                trace.unlink()
                trace.write_text("")
                after = perf.collect_turn_assessment(root, "binding")
                self.assertEqual(after["native_slow_receipts"]["snapshot"], native)


if __name__ == "__main__":
    unittest.main()
