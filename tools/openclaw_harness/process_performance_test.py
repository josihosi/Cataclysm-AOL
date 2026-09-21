import copy
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
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

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

    def test_native_turn_producer_emits_parseable_precise_json(self):
        source = (Path(__file__).resolve().parents[2] / "src" / "do_turn.cpp").read_text(encoding="utf-8")
        start = source.index('stream << "openclaw_harness_semantic_step: ')
        end = source.index(";\n    }", start)
        chain = source[start + len("stream << "):end]
        values = {
            "event": "turn", "stage": "start", "openclaw_harness_turn_trace_quote( run_id )": "run-a",
            "process_instance": "native-a", "++sequence": "1", "turn_id": "turn-1",
            "game_turn": "100", "game_minutes": "600", "wall_time": format(1700000000.125, ".17g"),
            "simulation_seconds": format(.125, ".17g"), "phase": "simulation",
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
        source = (Path(__file__).resolve().parents[2] / "src" / "do_turn.cpp").read_text(encoding="utf-8")
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
