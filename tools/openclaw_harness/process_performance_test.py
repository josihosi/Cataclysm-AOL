import copy
import json
import os
from pathlib import Path
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
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def recorder(self, samples):
        return perf.ProcessPerformance(self.directory, pid=73, run_id="run-a", binding_id="binding-a",
                                       source_binding={"runtime_binding_sha256": "binary-a"},
                                       sampler=lambda _: samples.pop(0))

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

    def test_failed_telemetry_write_preserves_native_outcome(self):
        recorder = self.recorder([sample(1, 0), sample(2, 1), sample(3, 2)])
        frame = {"kind": "world", "game_minutes": 100}
        recorder.begin_action(frame, "world.pause")
        with patch("process_performance.append_record", side_effect=OSError("disk unavailable")):
            recorder.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": 101}})
        self.assertEqual(recorder.brief()["collection_error"], "disk unavailable")
        self.assertTrue(recorder.stopped)

    def test_paging_keeps_exact_records_and_next_offset(self):
        for index in range(12):
            perf.append_record(self.directory, {"index": index})
        page = perf.read_records(self.directory, 5, 5)
        self.assertEqual([item["index"] for item in page["records"]], list(range(5, 10)))
        self.assertEqual(page["next_offset"], 10)

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
                patch("process_performance.ProcessPerformance", return_value=recorder), \
                patch.object(startup, "execute_semantic_act", return_value=outcome), \
                patch.object(startup, "finalize_cockpit_live_session", return_value={"cleaned": True}):
            service = startup.open_cockpit_game_service(profile="test", run_dir=self.directory,
                        run_id="run-a", trace_start_offset=0, pid=73, session_id="test", live_session=True)
            result = service.live_channel._dispatch_advertised_action(frame, "world.pause")
            self.assertEqual(result, outcome)
            recorder.begin_action.assert_called_once_with(frame, "world.pause")
            recorder.end_action.assert_called_once_with(frame, outcome)
            service.live_channel._finalize_session({})
            recorder.stop.assert_called_once()


if __name__ == "__main__":
    unittest.main()
