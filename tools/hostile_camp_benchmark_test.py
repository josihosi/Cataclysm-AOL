"""Unit tests for the deterministic hostile-camp benchmark orchestrator."""

from __future__ import annotations

import copy
import errno
import json
import os
import pathlib
import tempfile
import unittest
from unittest import mock

from tools import hostile_camp_benchmark as benchmark


FIXTURE_HASH = "1" * 64
BINARY_A_HASH = "a" * 64
BINARY_B_HASH = "b" * 64
CHILD_HASH = "c" * 64
MATRIX_HASH = "d" * 64


def data_root_record(label):
    root = str((pathlib.Path(tempfile.gettempdir()) / f"caol-data-root-{label}").resolve())
    return {
        "path": root,
        "data_path": str(pathlib.Path(root) / "data"),
        "manifest_kind": "recursive_file_content_sha256_v1",
        "manifest_sha256": ("7" if label == "A" else "8") * 64,
        "file_count": 2,
        "total_bytes": 14,
    }


def source_data_root_record(label):
    record = data_root_record(label)
    record["manifest_kind"] = "recursive_source_file_content_sha256_excluding_cache_v1"
    record["manifest_sha256"] = "5" * 64
    record["file_count"] = 1
    record["total_bytes"] = 10
    return record


def streaming_summary(values):
    if values:
        measurements = {
            "count": len(values), "total": sum(values), "min": min(values),
            "p50": benchmark.nearest_rank_percentile(values, 50),
            "p95": benchmark.nearest_rank_percentile(values, 95),
            "p99": benchmark.nearest_rank_percentile(values, 99), "max": max(values),
        }
    else:
        measurements = {
            "count": 0, "total": 0, "min": 0, "p50": 0, "p95": 0, "p99": 0, "max": 0,
        }
    return {
        **measurements, "quantiles_are_upper_bounds": True,
        "relative_resolution_ppm": 15625, "overflow": False,
    }


def probe_section(calls=0, inclusive_total=0, self_total=0):
    return {
        "calls": calls,
        "inclusive_total_ns": inclusive_total,
        "inclusive_summary_ns": streaming_summary(
            [inclusive_total // calls] * calls if calls else []),
        "self_total_ns": self_total,
        "self_summary_ns": streaming_summary([self_total // calls] * calls if calls else []),
    }


def sample_case(thresholds=None):
    case = {
        "id": "legacy-10-idle",
        "fixture": "legacy-10",
        "fixture_sha256": FIXTURE_HASH,
        "workload": "idle-24h",
        "env": {
            "CAOL_HOSTILE_BENCHMARK_UPDATES": "4",
            "CAOL_HOSTILE_BENCHMARK_CLOCK_FLOOR_SAMPLES": "4",
        },
    }
    if thresholds is not None:
        case["thresholds"] = thresholds
    return case


def sample_matrix(thresholds=None):
    return {"schema": benchmark.MATRIX_SCHEMA, "cases": [sample_case(thresholds)]}


def child_result(label="A", repetition=0, wall_time=100, latencies=None,
                 include_nullable=True, include_fixture_hash=False, rng_seed=None):
    latency_values = list(latencies or [10, 20, 30, 40])
    metrics = {
        "wall_time_ns": wall_time,
        "update_latency_sample_count": len(latency_values),
        "update_latency_summary_ns": streaming_summary(latency_values),
        "clock_floor_summary_ns": streaming_summary([1, 1, 1, 1]),
        "hostile_update_calls": 4,
        "serialized": {"total_bytes": 128, "camps": {"count": 10, "bytes": 100}},
    }
    if include_nullable:
        metrics.update({"allocation_count": None, "live_heap_bytes": None})
    result = {
        "schema": benchmark.CHILD_SCHEMA,
        "fixture": "legacy-10",
        "workload": "idle-24h",
        "repetition": str(repetition),
        "variant": label,
        "rng_seed": rng_seed if rng_seed is not None else 123 + repetition,
        "updates": 4,
        "clock_floor_samples": 4,
        "calendar": {
            "turn": benchmark._DEFAULT_CALENDAR_TURN,
            "start_of_cataclysm_turn": 0,
            "start_of_game_turn": 0,
            "initial_season": "spring",
            "season_length_days": benchmark._DEFAULT_SEASON_LENGTH_DAYS,
            "eternal_season": False,
            "eternal_night": False,
            "eternal_day": False,
            "reset_before_timing_replay": True,
            "reset_before_fairness_replay": True,
        },
        "metrics": metrics,
        "probe": {
            "timings_collected": True,
            "site_services_collected": False,
            "stack_overflow": False,
            "sections": {
                name: probe_section(1, 2, 1) if name == "world_serialize" else probe_section()
                for name in benchmark._PROBE_SECTIONS
            },
            "counters": {
                name: 2 if name == "world_serialize_calls" else 0
                for name in benchmark._PROBE_COUNTERS
            },
        },
        "serialization": {"initial_bytes": 128, "terminal_bytes": 128, "growth_bytes": 0},
        "fairness": {"site_count": 2, "serviced_sites": 2,
                     "per_site": [{"site_id": "a", "scan_samples": 5},
                                  {"site_id": "b", "scan_samples": 5}]},
        "deterministic_state": {
            "hash_algorithm": "sha256",
            "initial_sha256": FIXTURE_HASH,
            "terminal_sha256": "2" * 64,
        },
        "measurement_modes": {
            "latency": "timing replay",
            "fairness": "untimed deterministic replay",
            "terminal_state_match": True,
        },
    }
    metrics["clock_floor_sample_count"] = 4
    if include_fixture_hash:
        result["fixture_sha256"] = FIXTURE_HASH
    return result


def sample_run(label, repetition, wall_time, binary_hash, latencies=None, rss=None,
               order_index=None, child_seed=None):
    if child_seed is None:
        child_seed = 123 + repetition
    rss_values = rss or [1000, 1100]
    return {
        "case_id": "legacy-10-idle",
        "fixture_sha256": FIXTURE_HASH,
        "pair_index": repetition,
        "order_index": (0 if label == "A" else 1) if order_index is None else order_index,
        "variant": label,
        "child_seed": child_seed,
        "binary_sha256": binary_hash,
        "working_directory": data_root_record(label)["path"],
        "data_root_manifest_sha256": data_root_record(label)["manifest_sha256"],
        "command": [f"/{label}", "--rng-seed", str(child_seed)],
        "exit_code": 0,
        "runner_wall_time_ns": wall_time + 50,
        "rss_samples": [
            {"monotonic_ns": index + 1, "rss_bytes": value}
            for index, value in enumerate(rss_values)
        ],
        "rss_observation_count": len(rss_values),
        "rss_sample_limit": benchmark._MAX_CHILD_RSS_SAMPLES,
        "rss_retention_stride": 1,
        "rss_peak_bytes": max(rss_values) if rss_values else None,
        "stdout_sha256": "e" * 64,
        "stderr_sha256": "f" * 64,
        "child_result_source": "output_file",
        "child_result_sha256": CHILD_HASH,
        "result": child_result(label, repetition, wall_time, latencies,
                               rng_seed=child_seed),
    }


def warmup_record(label, warmup_index, orchestration_seed=77):
    child_seed = benchmark._warmup_seed(orchestration_seed)
    result = child_result(label, 0, rng_seed=child_seed)
    return {
        "status": "accepted",
        "warmup_index": warmup_index,
        "case_id": "legacy-10-idle",
        "fixture_sha256": FIXTURE_HASH,
        "variant": label,
        "child_seed": child_seed,
        "binary_sha256": BINARY_A_HASH if label == "A" else BINARY_B_HASH,
        "working_directory": data_root_record(label)["path"],
        "source_manifest_sha256": source_data_root_record(label)["manifest_sha256"],
        "command": [f"/{label}", "--rng-seed", str(child_seed)],
        "exit_code": 0,
        "runner_wall_time_ns": 50,
        "stdout_sha256": "3" * 64,
        "stdout_bytes": 10,
        "stderr_sha256": "4" * 64,
        "stderr_bytes": 0,
        "diagnostic_stderr_tail": "",
        "child_result_source": "output_file",
        "child_result_sha256": benchmark.sha256_bytes(benchmark.canonical_json_bytes(result)),
        "child_result_binding": benchmark._warmup_result_binding(result),
        "failure_code": None,
    }


def raw_packet(a_walls=(100, 100), b_walls=(105, 105), thresholds=None,
               include_nullable=True):
    matrix = sample_matrix(thresholds)
    runs = []
    orders = []
    seeded_orders = benchmark.paired_orders(77, len(a_walls), ("A", "B"))
    for repetition, (a_wall, b_wall) in enumerate(zip(a_walls, b_walls)):
        order = list(seeded_orders[repetition])
        orders.append(order)
        child_seed = benchmark._derived_seed(77, 0, repetition)
        a_run = sample_run("A", repetition, a_wall, BINARY_A_HASH,
                           order_index=order.index("A"), child_seed=child_seed)
        b_run = sample_run("B", repetition, b_wall, BINARY_B_HASH,
                           order_index=order.index("B"), child_seed=child_seed)
        if not include_nullable:
            a_run["result"] = child_result(
                "A", repetition, a_wall, include_nullable=False, rng_seed=child_seed)
            b_run["result"] = child_result(
                "B", repetition, b_wall, include_nullable=False, rng_seed=child_seed)
        run_by_label = {"A": a_run, "B": b_run}
        runs.extend(run_by_label[label] for label in order)
    payload = {
        "status": "accepted",
        "created_utc": "2026-08-02T00:00:00+00:00",
        "seed": 77,
        "pair_count": len(a_walls),
        "common_arguments": [],
        "label_order": ["A", "B"],
        "pair_orders": orders,
        "update_observation_limit": benchmark._MAX_PACKET_UPDATE_OBSERVATIONS,
        "expected_update_observations": 4 * len(a_walls) * 2,
        "rss_sample_limit_per_child": benchmark._MAX_CHILD_RSS_SAMPLES,
        "rss_sample_limit_per_packet": benchmark._MAX_PACKET_RSS_SAMPLES,
        "expected_max_retained_rss_samples": (
            len(matrix["cases"]) * 2 * len(a_walls) * benchmark._MAX_CHILD_RSS_SAMPLES),
        "matrix_path": "/matrix.json",
        "matrix_sha256": MATRIX_HASH,
        "matrix": matrix,
        "binaries": {
            "A": {"path": "/A", "size_bytes": 1, "sha256": BINARY_A_HASH},
            "B": {"path": "/B", "size_bytes": 1, "sha256": BINARY_B_HASH},
        },
        "source_data_roots": {
            "A": source_data_root_record("A"), "B": source_data_root_record("B"),
        },
        "pre_warm_data_roots": {
            "A": source_data_root_record("A") | {
                "manifest_kind": "recursive_file_content_sha256_v1"
            },
            "B": source_data_root_record("B") | {
                "manifest_kind": "recursive_file_content_sha256_v1"
            },
        },
        "data_roots": {"A": data_root_record("A"), "B": data_root_record("B")},
        "host": {"platform": "test", "machine": "arm64", "python": "test"},
        "warmups": [warmup_record("A", 0), warmup_record("B", 1)],
        "runs": runs,
        "failures": [],
    }
    return benchmark.wrap_envelope(benchmark.RAW_SCHEMA, payload)


class OrderingTests(unittest.TestCase):
    def test_pair_order_is_seeded_balanced_and_deterministic(self):
        first = benchmark.paired_orders(830204929, 10)
        second = benchmark.paired_orders(830204929, 10)
        self.assertEqual(first, second)
        self.assertEqual(first.count(("A", "B")), 5)
        self.assertEqual(first.count(("B", "A")), 5)

    def test_odd_pair_order_has_at_most_one_imbalance(self):
        orders = benchmark.paired_orders(19, 11)
        counts = [orders.count(("A", "B")), orders.count(("B", "A"))]
        self.assertEqual(sum(counts), 11)
        self.assertLessEqual(abs(counts[0] - counts[1]), 1)

    def test_single_binary_order_repeats_one_label(self):
        self.assertEqual(benchmark.paired_orders(3, 3, ("baseline",)),
                         [("baseline",), ("baseline",), ("baseline",)])


class StatisticsTests(unittest.TestCase):
    def test_nearest_rank_percentiles_use_explicit_one_based_index(self):
        values = list(range(1, 101))
        self.assertEqual(benchmark.nearest_rank_percentile(values, 0), 1)
        self.assertEqual(benchmark.nearest_rank_percentile(values, 50), 50)
        self.assertEqual(benchmark.nearest_rank_percentile(values, 95), 95)
        self.assertEqual(benchmark.nearest_rank_percentile(values, 99), 99)
        self.assertEqual(benchmark.nearest_rank_percentile(values, 100), 100)

    def test_nearest_rank_small_sample_does_not_invent_interpolation(self):
        self.assertEqual(benchmark.nearest_rank_percentile([1, 2, 100], 95), 100)

    def test_bootstrap_ci_is_deterministic_and_contains_sample_mean(self):
        first = benchmark.bootstrap_mean_ci([1, 2, 3, 4], 42, samples=1000)
        second = benchmark.bootstrap_mean_ci([1, 2, 3, 4], 42, samples=1000)
        self.assertEqual(first, second)
        self.assertLessEqual(first[0], 2.5)
        self.assertGreaterEqual(first[1], 2.5)

    def test_bootstrap_single_value_is_exact(self):
        self.assertEqual(benchmark.bootstrap_mean_ci([17], 4, samples=20), [17.0, 17.0])


class HashTests(unittest.TestCase):
    def test_canonical_hash_ignores_dictionary_insertion_order(self):
        left = {"a": 1, "b": {"c": 2}}
        right = {"b": {"c": 2}, "a": 1}
        self.assertEqual(benchmark.sha256_bytes(benchmark.canonical_json_bytes(left)),
                         benchmark.sha256_bytes(benchmark.canonical_json_bytes(right)))

    def test_file_hash_is_exact_raw_bytes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = pathlib.Path(temp_dir) / "raw.bin"
            path.write_bytes(b"a\r\nb\n")
            self.assertEqual(benchmark.sha256_file(path), benchmark.sha256_bytes(b"a\r\nb\n"))

    def test_envelope_detects_payload_tampering(self):
        document = raw_packet()
        document["payload"]["seed"] += 1
        with self.assertRaisesRegex(benchmark.BenchmarkError, "hash mismatch"):
            benchmark.validate_raw(document)


class SchemaTests(unittest.TestCase):
    def test_packaged_matrices_fit_default_ten_pair_observation_cap(self):
        repository = pathlib.Path(__file__).resolve().parent.parent
        matrix_directory = repository / "tests" / "data" / "hostile_camp_benchmark"
        for name in ("legacy_matrix_v1.json", "legacy_lead_saturation_matrix_v1.json"):
            with self.subTest(name=name):
                matrix, _raw, _digest = benchmark.load_matrix(matrix_directory / name)
                observations = sum(
                    benchmark._case_expected_count(
                        case, "CAOL_HOSTILE_BENCHMARK_UPDATES")
                    for case in matrix["cases"]
                ) * 2 * 10
                self.assertLessEqual(observations,
                                     benchmark._MAX_PACKET_UPDATE_OBSERVATIONS)

    def test_matrix_accepts_portable_case(self):
        document = sample_matrix([{
            "metric": "wall_time_ns", "statistic": "mean", "ratio_max": 1.05,
        }])
        self.assertIs(benchmark.validate_matrix(document), document)

    def test_matrix_accepts_cpp_generated_fixture_without_explicit_id_or_hash(self):
        document = {
            "schema": benchmark.MATRIX_SCHEMA,
            "cases": [{"fixture": "legacy_idle_sites_0_v1", "workload": "idle",
                       "env": {"CAOL_HOSTILE_BENCHMARK_SITE_COUNT": "0",
                               "CAOL_HOSTILE_BENCHMARK_UPDATES": "4",
                               "CAOL_HOSTILE_BENCHMARK_CLOCK_FLOOR_SAMPLES": "4"}}],
        }
        self.assertIs(benchmark.validate_matrix(document), document)

    def test_generated_fixture_digest_is_computed_without_digest_declaration(self):
        case = sample_case()
        case.pop("fixture_sha256")
        expected = benchmark.sha256_bytes(benchmark.canonical_json_bytes(case))
        self.assertEqual(benchmark._case_fixture_sha256(
            case, "generated_case_spec_sha256"), expected)

    def test_generated_fixture_rejects_contradictory_explicit_digest(self):
        document = sample_matrix()
        document["fixture_hash_kind"] = "generated_case_spec_sha256"
        with self.assertRaisesRegex(benchmark.BenchmarkError, "contradicts"):
            benchmark.validate_matrix(document)

    def test_generated_fixture_accepts_matching_explicit_digest(self):
        document = sample_matrix()
        document["fixture_hash_kind"] = "generated_case_spec_sha256"
        document["cases"][0].pop("fixture_sha256")
        digest = benchmark._generated_case_spec_sha256(document["cases"][0])
        document["cases"][0]["fixture_sha256"] = digest
        self.assertIs(benchmark.validate_matrix(document), document)
        self.assertEqual(benchmark._case_fixture_sha256(
            document["cases"][0], "generated_case_spec_sha256"), digest)

    def test_serialized_fixture_requires_explicit_digest(self):
        document = sample_matrix()
        document["fixture_hash_kind"] = "serialized_state_sha256"
        document["cases"][0].pop("fixture_sha256")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "needs explicit"):
            benchmark.validate_matrix(document)

    def test_opaque_fixture_requires_explicit_digest(self):
        document = sample_matrix()
        document["fixture_hash_kind"] = "opaque_sha256"
        document["cases"][0].pop("fixture_sha256")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "needs explicit"):
            benchmark.validate_matrix(document)

    def test_matrix_rejects_duplicate_case_ids(self):
        document = sample_matrix()
        document["cases"].append(copy.deepcopy(document["cases"][0]))
        with self.assertRaisesRegex(benchmark.BenchmarkError, "duplicate case"):
            benchmark.validate_matrix(document)

    def test_matrix_rejects_secret_like_archived_environment(self):
        document = sample_matrix()
        document["cases"][0]["env"] = {"SERVICE_TOKEN": "not-a-real-secret"}
        with self.assertRaisesRegex(benchmark.BenchmarkError, "secret-like"):
            benchmark.validate_matrix(document)

    def test_child_accepts_nullable_allocation_fields(self):
        document = child_result()
        self.assertEqual(document["schema"], "caol-hostile-camp-benchmark-result-v1")
        self.assertNotIn("fixture_sha256", document)
        self.assertIs(benchmark.validate_child_result(document), document)

    def test_child_accepts_matching_optional_fixture_hash(self):
        document = child_result(include_fixture_hash=True)
        expected = {
            "fixture": "legacy-10", "fixture_sha256": FIXTURE_HASH,
            "fixture_hash_kind": "serialized_state_sha256",
            "workload": "idle-24h", "repetition": 0, "variant": "A",
            "rng_seed": 123,
            "updates": 4, "clock_floor_samples": 4,
        }
        self.assertIs(benchmark.validate_child_result(document, expected), document)

    def test_child_accepts_missing_forward_allocation_fields(self):
        document = child_result(include_nullable=False)
        self.assertIs(benchmark.validate_child_result(document), document)

    def test_child_rejects_wrong_echo_identity(self):
        document = child_result()
        expected = {
            "fixture": "legacy-10", "fixture_sha256": FIXTURE_HASH,
            "fixture_hash_kind": "serialized_state_sha256",
            "workload": "idle-24h", "repetition": 0, "variant": "B",
            "rng_seed": 123,
            "updates": 4, "clock_floor_samples": 4,
        }
        with self.assertRaisesRegex(benchmark.BenchmarkError, "wrong variant"):
            benchmark.validate_child_result(document, expected)

    def test_child_rejects_zero_update_sample_count(self):
        document = child_result()
        document["metrics"]["update_latency_sample_count"] = 0
        with self.assertRaisesRegex(benchmark.BenchmarkError, "does not match"):
            benchmark.validate_child_result(document)

    def test_child_rejects_latency_count_that_does_not_match_updates(self):
        document = child_result()
        document["metrics"]["update_latency_sample_count"] = 1
        with self.assertRaisesRegex(benchmark.BenchmarkError, "does not match"):
            benchmark.validate_child_result(document)

    def test_child_rejects_streaming_summary_overflow(self):
        document = child_result()
        document["metrics"]["update_latency_summary_ns"]["overflow"] = True
        with self.assertRaisesRegex(benchmark.BenchmarkError, "histogram overflow"):
            benchmark.validate_child_result(document)

    def test_child_rejects_total_below_minimum_possible_for_count(self):
        document = child_result()
        summary = document["metrics"]["update_latency_summary_ns"]
        summary.update({"min": 10, "max": 20, "total": 20})
        with self.assertRaisesRegex(benchmark.BenchmarkError, "total is impossible"):
            benchmark.validate_child_result(document)

    def test_child_rejects_total_above_maximum_possible_for_count(self):
        document = child_result()
        summary = document["metrics"]["update_latency_summary_ns"]
        summary.update({"min": 10, "max": 20, "total": 100})
        with self.assertRaisesRegex(benchmark.BenchmarkError, "total is impossible"):
            benchmark.validate_child_result(document)

    def test_streaming_summary_requires_reported_min_and_max_to_both_occur(self):
        summary = streaming_summary([10, 10, 20, 20])
        summary["total"] = 45
        with self.assertRaisesRegex(benchmark.BenchmarkError, "both declared exact bounds"):
            benchmark._validate_streaming_summary(summary, "test.summary")
        summary["total"] = 75
        with self.assertRaisesRegex(benchmark.BenchmarkError, "both declared exact bounds"):
            benchmark._validate_streaming_summary(summary, "test.summary")

    def test_single_streaming_observation_must_equal_all_exact_fields(self):
        summary = streaming_summary([10])
        summary.update({"total": 15, "max": 20})
        with self.assertRaisesRegex(benchmark.BenchmarkError, "single observation"):
            benchmark._validate_streaming_summary(summary, "test.summary")

    def test_two_sample_tail_quantiles_must_equal_exact_max(self):
        summary = streaming_summary([10, 20])
        summary["p95"] = 19
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "p95 must equal exact max"):
            benchmark._validate_streaming_summary(summary, "test.summary")

    def test_p95_nearest_rank_max_boundary_is_19_samples(self):
        nineteen = streaming_summary([10] * 18 + [20])
        nineteen["p95"] = 19
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "p95 must equal exact max"):
            benchmark._validate_streaming_summary(nineteen, "test.summary")
        twenty = streaming_summary([10] * 19 + [20])
        benchmark._validate_streaming_summary(twenty, "test.summary")
        self.assertLess(twenty["p95"], twenty["max"])

    def test_p99_nearest_rank_max_boundary_is_99_samples(self):
        ninety_nine = streaming_summary([10] * 98 + [20])
        ninety_nine["p99"] = 19
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "p99 must equal exact max"):
            benchmark._validate_streaming_summary(ninety_nine, "test.summary")
        one_hundred = streaming_summary([10] * 99 + [20])
        benchmark._validate_streaming_summary(one_hundred, "test.summary")
        self.assertLess(one_hundred["p99"], one_hundred["max"])

    def test_child_rejects_quantile_upper_bound_above_exact_maximum(self):
        document = child_result()
        summary = document["metrics"]["update_latency_summary_ns"]
        summary["p99"] = summary["max"] + 1
        with self.assertRaisesRegex(benchmark.BenchmarkError, "must be ordered"):
            benchmark.validate_child_result(document)

    def test_child_rejects_missing_clock_floor_summary(self):
        document = child_result()
        document["metrics"].pop("clock_floor_summary_ns")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "clock_floor_summary_ns"):
            benchmark.validate_child_result(document)

    def test_child_rejects_calendar_without_both_replay_resets(self):
        document = child_result()
        document["calendar"]["reset_before_fairness_replay"] = False
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "reset_before_fairness_replay must be true"):
            benchmark.validate_child_result(document)

    def test_matrix_and_child_reject_season_length_that_overflows_turns(self):
        matrix = sample_matrix()
        matrix["cases"][0]["env"]["CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS"] = str(
            benchmark._MAX_SEASON_LENGTH_DAYS + 1)
        with self.assertRaisesRegex(benchmark.BenchmarkError, "representable as int32 turns"):
            benchmark.validate_matrix(matrix)

        document = child_result()
        document["calendar"]["season_length_days"] = benchmark._MAX_SEASON_LENGTH_DAYS + 1
        with self.assertRaisesRegex(benchmark.BenchmarkError, "representable as int32 turns"):
            benchmark.validate_child_result(document)

    def test_child_rejects_incomplete_scoped_probe(self):
        document = child_result()
        document["probe"]["sections"].pop("structural_scan")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "every scoped section"):
            benchmark.validate_child_result(document)

    def test_child_rejects_probe_overflow(self):
        document = child_result()
        document["probe"]["stack_overflow"] = True
        with self.assertRaisesRegex(benchmark.BenchmarkError, "stack overflow"):
            benchmark.validate_child_result(document)

    def test_child_rejects_wrong_rng_seed(self):
        document = child_result()
        expected = {
            "fixture": "legacy-10", "fixture_sha256": FIXTURE_HASH,
            "fixture_hash_kind": "opaque_sha256", "workload": "idle-24h",
            "repetition": 0, "variant": "A", "rng_seed": 999,
            "updates": 4, "clock_floor_samples": 4,
        }
        with self.assertRaisesRegex(benchmark.BenchmarkError, "wrong rng_seed"):
            benchmark.validate_child_result(document, expected)

    def test_child_rejects_non_finite_metric(self):
        document = child_result()
        document["metrics"]["bad"] = float("nan")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "unsupported type"):
            benchmark.validate_child_result(document)

    def test_accepted_raw_packet_validates(self):
        document = raw_packet()
        self.assertEqual(benchmark.validate_raw(document)["status"], "accepted")

    def test_accepted_raw_packet_cannot_omit_a_run(self):
        document = raw_packet()
        document["payload"]["runs"].pop()
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "incomplete"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_rejects_pair_order_mismatch(self):
        document = raw_packet()
        document["payload"]["runs"][0]["order_index"] = 1
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "declared pair order"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_binds_pair_orders_to_seed_and_label_order(self):
        document = raw_packet()
        document["payload"]["pair_orders"] = [["A", "B"]] * document["payload"]["pair_count"]
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "recorded seed"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_rejects_data_root_mismatch(self):
        document = raw_packet()
        document["payload"]["runs"][0]["working_directory"] = "/wrong-root"
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "working directory"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_binds_recorded_warmup_seed(self):
        document = raw_packet()
        document["payload"]["warmups"][0]["child_seed"] += 1
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "warmup seed mismatch"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_binds_recorded_warmup_command(self):
        document = raw_packet()
        document["payload"]["warmups"][0]["command"].append("unexpected")
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "command binding"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_uses_same_warmup_seed_for_both_variants(self):
        document = raw_packet()
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["warmups"][0]["child_seed"],
                         payload["warmups"][1]["child_seed"])

    def test_accepted_raw_packet_rejects_different_source_manifests(self):
        document = raw_packet()
        document["payload"]["source_data_roots"]["B"]["manifest_sha256"] = "6" * 64
        document["payload"]["warmups"][1]["source_manifest_sha256"] = "6" * 64
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "identical non-cache source data"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_requires_populated_runtime_cache(self):
        document = raw_packet()
        payload = document["payload"]
        payload["data_roots"]["A"] = copy.deepcopy(payload["pre_warm_data_roots"]["A"])
        for run in payload["runs"]:
            if run["variant"] == "A":
                run["data_root_manifest_sha256"] = payload["data_roots"]["A"][
                    "manifest_sha256"
                ]
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, payload)
        with self.assertRaisesRegex(benchmark.BenchmarkError, "did not populate"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_binds_measured_command_and_exit(self):
        document = raw_packet()
        document["payload"]["runs"][0]["command"].append("unexpected")
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "run command binding"):
            benchmark.validate_raw(document)

        document = raw_packet()
        document["payload"]["runs"][0]["exit_code"] = 9
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "exited non-zero"):
            benchmark.validate_raw(document)

        document = raw_packet()
        document["payload"]["runs"][0]["exit_code"] = False
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "must be an integer"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_rejects_missing_rss_accounting(self):
        document = raw_packet()
        document["payload"]["runs"][0].pop("rss_observation_count")
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "RSS observation count"):
            benchmark.validate_raw(document)

    def test_accepted_raw_packet_binds_child_seed_to_orchestration_seed(self):
        document = raw_packet()
        document["payload"]["runs"][0]["child_seed"] += 1
        document["payload"]["runs"][0]["result"]["rng_seed"] += 1
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "orchestration seed"):
            benchmark.validate_raw(document)

    def test_equivalent_packet_rejects_terminal_state_divergence(self):
        document = raw_packet()
        document["payload"]["matrix"]["require_equivalent_terminal_state"] = True
        document["payload"]["runs"][0]["result"]["deterministic_state"][
            "terminal_sha256"] = "3" * 64
        document = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, document["payload"])
        with self.assertRaisesRegex(benchmark.BenchmarkError, "terminal states diverged"):
            benchmark.validate_raw(document)

    def test_rejected_raw_packet_preserves_declared_failure(self):
        document = raw_packet()
        payload = document["payload"]
        payload["status"] = "rejected"
        payload["runs"] = payload["runs"][:1]
        payload["failures"] = [{"code": "child", "message": "exit 2", "details": {}}]
        rejected = benchmark.wrap_envelope(benchmark.RAW_SCHEMA, payload)
        self.assertEqual(benchmark.validate_raw(rejected)["status"], "rejected")


class SummaryAndComparisonTests(unittest.TestCase):
    def test_summary_keeps_slow_valid_run_and_reports_streaming_tail(self):
        document = raw_packet(a_walls=(100, 10_000), b_walls=(105, 10_500))
        summary = benchmark.summarize_raw(document, "9" * 64, bootstrap_samples=100)
        payload = benchmark.validate_summary(summary)
        a_metrics = payload["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"]
        self.assertEqual(a_metrics["wall_time_ns"]["count"], 2)
        self.assertEqual(a_metrics["wall_time_ns"]["max"], 10_000)
        self.assertEqual(a_metrics["update_latency_summary_ns.p95"]["count"], 2)
        self.assertEqual(a_metrics["update_latency_summary_ns.p95"]["p95"], 40)

    def test_summary_marks_missing_allocation_trace_unavailable(self):
        summary = benchmark.summarize_raw(raw_packet(include_nullable=False),
                                          bootstrap_samples=50)
        payload = benchmark.validate_summary(summary)
        metrics = payload["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"]
        self.assertFalse(metrics["allocation_count"]["available"])
        self.assertEqual(metrics["allocation_count"]["count"], 0)
        self.assertFalse(metrics["live_heap_bytes"]["available"])

    def test_summary_records_periodic_rss_as_observations(self):
        summary = benchmark.summarize_raw(raw_packet(), bootstrap_samples=50)
        payload = benchmark.validate_summary(summary)
        rss = payload["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"][
            "runner_rss_bytes"]
        self.assertEqual(rss["kind"], "observations")
        self.assertEqual(rss["count"], 4)
        self.assertEqual(rss["max"], 1100)
        peak = payload["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"][
            "runner_peak_rss_bytes"]
        self.assertEqual(peak["kind"], "run_scalar")
        self.assertEqual(peak["mean"], 1100)

    def test_summary_includes_probe_serialization_and_fairness_sections(self):
        summary = benchmark.summarize_raw(raw_packet(), bootstrap_samples=50)
        payload = benchmark.validate_summary(summary)
        metrics = payload["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"]
        self.assertEqual(metrics["probe.counters.world_serialize_calls"]["mean"], 2)
        self.assertEqual(metrics["serialization.growth_bytes"]["mean"], 0)
        self.assertEqual(metrics["fairness.per_site[].scan_samples"]["count"], 4)

    def test_compare_passes_ratio_and_absolute_thresholds(self):
        thresholds = [{
            "metric": "wall_time_ns", "statistic": "mean",
            "ratio_max": 1.10, "absolute_max": 110,
        }]
        summary = benchmark.summarize_raw(
            raw_packet(a_walls=(100, 100), b_walls=(105, 105), thresholds=thresholds),
            bootstrap_samples=50)
        comparison = benchmark.compare_summary(summary, "A", "B")
        self.assertTrue(comparison["payload"]["overall_pass"])
        self.assertEqual(comparison["payload"]["threshold_count"], 1)

    def test_compare_fails_threshold_without_rejecting_raw_run(self):
        thresholds = [{
            "metric": "wall_time_ns", "statistic": "mean", "ratio_max": 1.05,
        }]
        raw = raw_packet(a_walls=(100, 100), b_walls=(110, 110), thresholds=thresholds)
        self.assertEqual(benchmark.validate_raw(raw)["status"], "accepted")
        summary = benchmark.summarize_raw(raw, bootstrap_samples=50)
        comparison = benchmark.compare_summary(summary, "A", "B")
        self.assertFalse(comparison["payload"]["overall_pass"])
        self.assertAlmostEqual(comparison["payload"]["results"][0]["checks"][0]["actual"], 1.1)

    def test_compare_fails_honestly_when_nullable_metric_is_unavailable(self):
        thresholds = [{
            "metric": "allocation_count", "statistic": "mean", "absolute_max": 1,
        }]
        summary = benchmark.summarize_raw(
            raw_packet(thresholds=thresholds, include_nullable=False), bootstrap_samples=50)
        comparison = benchmark.compare_summary(summary, "A", "B")
        result = comparison["payload"]["results"][0]
        self.assertFalse(result["pass"])
        self.assertEqual(result["checks"],
                         [{"name": "candidate_availability", "pass": False}])

    def test_compare_absolute_threshold_needs_only_candidate(self):
        thresholds = [{
            "metric": "wall_time_ns", "statistic": "mean", "absolute_max": 110,
        }]
        summary = benchmark.summarize_raw(
            raw_packet(a_walls=(100, 100), b_walls=(105, 105), thresholds=thresholds),
            bootstrap_samples=50)
        summary["payload"]["cases"]["legacy-10-idle"]["variants"]["A"]["metrics"].pop(
            "wall_time_ns")
        summary = benchmark.wrap_envelope(benchmark.SUMMARY_SCHEMA, summary["payload"])
        comparison = benchmark.compare_summary(summary, "A", "B")
        self.assertTrue(comparison["payload"]["overall_pass"])

    def test_compare_rejects_same_label_for_relative_thresholds(self):
        for limit_name, limit in (("ratio_max", 1.0), ("delta_max", 0)):
            with self.subTest(limit_name=limit_name):
                thresholds = [{
                    "metric": "wall_time_ns", "statistic": "mean", limit_name: limit,
                }]
                summary = benchmark.summarize_raw(
                    raw_packet(thresholds=thresholds), bootstrap_samples=50)
                with self.assertRaisesRegex(benchmark.BenchmarkError, "distinct baseline"):
                    benchmark.compare_summary(summary, "B", "B")

    def test_compare_allows_same_single_label_for_absolute_threshold(self):
        thresholds = [{
            "metric": "wall_time_ns", "statistic": "mean", "absolute_max": 110,
        }]
        summary = benchmark.summarize_raw(
            raw_packet(a_walls=(100, 100), b_walls=(105, 105), thresholds=thresholds),
            bootstrap_samples=50)
        payload = summary["payload"]
        payload["binaries"] = {"B": payload["binaries"]["B"]}
        payload["cases"]["legacy-10-idle"]["variants"].pop("A")
        single_variant_summary = benchmark.wrap_envelope(benchmark.SUMMARY_SCHEMA, payload)
        comparison = benchmark.compare_summary(single_variant_summary, "B", "B")
        self.assertTrue(comparison["payload"]["overall_pass"])

    def test_compare_without_thresholds_is_not_a_pass(self):
        summary = benchmark.summarize_raw(raw_packet(), bootstrap_samples=50)
        comparison = benchmark.compare_summary(summary, "A", "B")
        self.assertEqual(comparison["payload"]["status"], "no_thresholds")
        self.assertEqual(comparison["payload"]["threshold_count"], 0)
        self.assertFalse(comparison["payload"]["overall_pass"])


class RunPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp_dir.name)
        self.matrix_path = self.root / "matrix.json"
        self.matrix_path.write_text(json.dumps(sample_matrix()), encoding="utf-8")
        self.binary_a = self.root / "a-bin"
        self.binary_b = self.root / "b-bin"
        self.binary_a.write_text("fake a", encoding="utf-8")
        self.binary_b.write_text("fake b", encoding="utf-8")
        os.chmod(self.binary_a, 0o700)
        os.chmod(self.binary_b, 0o700)
        self.data_root_a = self.root / "a-worktree"
        self.data_root_b = self.root / "b-worktree"
        (self.data_root_a / "data").mkdir(parents=True)
        (self.data_root_b / "data").mkdir(parents=True)
        (self.data_root_a / "data" / "identity.json").write_text("same", encoding="utf-8")
        (self.data_root_b / "data" / "identity.json").write_text("same", encoding="utf-8")

    def tearDown(self):
        self.temp_dir.cleanup()

    def declared_data_roots(self, *labels):
        available = {"A": self.data_root_a, "B": self.data_root_b}
        return {label: available[label] for label in labels}

    def make_directory_symlink_or_skip(self, target, link):
        try:
            os.symlink(target, link, target_is_directory=True)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"directory symlinks unavailable: {error}")

    @staticmethod
    def fake_child(identity, label, case, pair_index, order_index, child_seed,
                   common_arguments, timeout_seconds, rss_interval_seconds,
                   fixture_hash_kind, isolated_user_dir_argument, data_root):
        del timeout_seconds, rss_interval_seconds, fixture_hash_kind
        result = child_result(label, pair_index, 100 + pair_index, rng_seed=child_seed)
        result["calendar"] = benchmark._case_calendar(case)
        raw = benchmark.canonical_json_bytes(result)
        command = [identity["path"], "--rng-seed", str(child_seed), *common_arguments,
                   *case.get("arguments", [])]
        if isolated_user_dir_argument is not None:
            command.extend((isolated_user_dir_argument,
                            str(pathlib.Path(data_root["path"]) / "measured-user")))
        return {
            "case_id": benchmark._case_id(case), "pair_index": pair_index,
            "fixture_sha256": benchmark._case_fixture_sha256(case),
            "order_index": order_index, "variant": label, "child_seed": child_seed,
            "binary_sha256": identity["sha256"], "command": command,
            "working_directory": data_root["path"],
            "data_root_manifest_sha256": data_root["manifest_sha256"],
            "exit_code": 0, "runner_wall_time_ns": 150 + pair_index,
            "rss_samples": [], "stdout_sha256": "0" * 64,
            "rss_observation_count": 0,
            "rss_sample_limit": benchmark._MAX_CHILD_RSS_SAMPLES,
            "rss_retention_stride": 1,
            "rss_peak_bytes": None,
            "stderr_sha256": "0" * 64, "child_result_source": "stdout",
            "child_result_sha256": benchmark.sha256_bytes(raw), "result": result,
        }

    @staticmethod
    def fake_warmup(identity, label, case, warmup_index, child_seed,
                    common_arguments, timeout_seconds, fixture_hash_kind,
                    isolated_user_dir_argument, source_data_root):
        del timeout_seconds, fixture_hash_kind
        cache = pathlib.Path(source_data_root["data_path"]) / "cache"
        cache.mkdir(parents=True, exist_ok=True)
        (cache / "generated.fb").write_bytes(b"warm")
        result = child_result(label, 0, rng_seed=child_seed)
        result["calendar"] = benchmark._case_calendar(case)
        raw = benchmark.canonical_json_bytes(result)
        command = [identity["path"], "--rng-seed", str(child_seed), *common_arguments,
                   *case.get("arguments", [])]
        if isolated_user_dir_argument is not None:
            command.extend((isolated_user_dir_argument,
                            str(pathlib.Path(source_data_root["path"]) / "warmup-user")))
        return {
            "status": "accepted", "warmup_index": warmup_index,
            "case_id": benchmark._case_id(case),
            "fixture_sha256": benchmark._case_fixture_sha256(case),
            "variant": label, "child_seed": child_seed,
            "binary_sha256": identity["sha256"],
            "working_directory": source_data_root["path"],
            "source_manifest_sha256": source_data_root["manifest_sha256"],
            "command": command, "exit_code": 0, "runner_wall_time_ns": 50,
            "stdout_sha256": "3" * 64, "stdout_bytes": 10,
            "stderr_sha256": "4" * 64, "stderr_bytes": 0,
            "diagnostic_stderr_tail": "",
            "child_result_source": "output_file",
            "child_result_sha256": benchmark.sha256_bytes(raw),
            "child_result_binding": benchmark._warmup_result_binding(result),
            "failure_code": None,
        }

    def test_run_executes_all_valid_children_in_seeded_serial_order(self):
        calls = []

        def recording_child(*args):
            calls.append((args[3], args[1]))
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a, "B": self.binary_b}, 10, 123,
            data_roots=self.declared_data_roots("A", "B"),
            build_detector=lambda: [], warmup_runner=self.fake_warmup,
            child_runner=recording_child)
        payload = benchmark.validate_raw(document, verify_files=True)
        self.assertEqual(payload["status"], "accepted")
        self.assertEqual(len(calls), 20)
        for pair_index, order in enumerate(payload["pair_orders"]):
            self.assertEqual([label for pair, label in calls if pair == pair_index], order)

    def test_run_binds_explicit_calendar_environment_to_warmup_and_measurement(self):
        matrix = sample_matrix()
        matrix["cases"][0]["env"].update({
            "CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN": "6000000",
            "CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS": "120",
        })
        self.matrix_path.write_text(json.dumps(matrix), encoding="utf-8")
        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 17,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=self.fake_warmup, child_runner=self.fake_child)
        payload = benchmark.validate_raw(document)
        expected_calendar = benchmark._case_calendar(matrix["cases"][0])
        self.assertEqual(payload["warmups"][0]["child_result_binding"]["calendar"],
                         expected_calendar)
        self.assertEqual(payload["runs"][0]["result"]["calendar"], expected_calendar)

    def test_cold_cache_is_created_by_recorded_warmup_before_full_identity_capture(self):
        template = json.dumps(child_result(rng_seed=1), sort_keys=True)
        self.binary_a.write_text(
            "#!/usr/bin/python3\n"
            "import json, os, pathlib\n"
            "secret_names = ('CATA_API_KEY', 'OPENAI_API_KEY', 'GH_TOKEN', "
            "'AWS_SECRET_ACCESS_KEY', 'ANTHROPIC_API_KEY')\n"
            "raise_code = 91 if any(name in os.environ for name in secret_names) else 0\n"
            "if raise_code: raise SystemExit(raise_code)\n"
            "os.write(1, b'x' * (1024 * 1024))\n"
            "os.write(2, b'y' * (1024 * 1024))\n"
            f"result = json.loads({template!r})\n"
            "result['fixture'] = os.environ['CAOL_HOSTILE_BENCHMARK_FIXTURE']\n"
            "result['workload'] = os.environ['CAOL_HOSTILE_BENCHMARK_WORKLOAD']\n"
            "result['repetition'] = os.environ['CAOL_HOSTILE_BENCHMARK_REPETITION']\n"
            "result['variant'] = os.environ['CAOL_HOSTILE_BENCHMARK_VARIANT']\n"
            "result['rng_seed'] = int(os.environ['CAOL_HOSTILE_BENCHMARK_SEED'])\n"
            "result['calendar']['turn'] = int(os.environ['CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN'])\n"
            "result['calendar']['season_length_days'] = int(os.environ['CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS'])\n"
            "cache = pathlib.Path('data/cache')\n"
            "cache.mkdir()\n"
            "(cache / 'generated.fb').write_bytes(b'warm')\n"
            "pathlib.Path(os.environ['CAOL_HOSTILE_BENCHMARK_OUTPUT']).write_text(json.dumps(result))\n",
            encoding="utf-8")
        os.chmod(self.binary_a, 0o700)

        with mock.patch.dict(os.environ, {"CATA_API_KEY": "sentinel-a",
                                          "OPENAI_API_KEY": "sentinel-b",
                                          "GH_TOKEN": "sentinel-c"}):
            document = benchmark.run_benchmarks(
                self.matrix_path, {"A": self.binary_a}, 1, 19,
                data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
                child_runner=self.fake_child)
        payload = benchmark.validate_raw(document, verify_files=True)
        self.assertEqual(payload["status"], "accepted")
        self.assertEqual(len(payload["warmups"]), 1)
        self.assertEqual(payload["source_data_roots"]["A"]["file_count"], 1)
        self.assertEqual(payload["data_roots"]["A"]["file_count"], 2)
        self.assertEqual(payload["warmups"][0]["stdout_bytes"], 1024 * 1024)
        self.assertEqual(payload["warmups"][0]["stderr_bytes"], 1024 * 1024)
        self.assertEqual(payload["warmups"][0]["diagnostic_stderr_tail"], "")

    def test_warmup_rejects_stream_that_exceeds_byte_cap(self):
        template = json.dumps(child_result(rng_seed=1), sort_keys=True)
        self.binary_a.write_text(
            "#!/usr/bin/python3\n"
            "import json, os, pathlib\n"
            f"result = json.loads({template!r})\n"
            "result['fixture'] = os.environ['CAOL_HOSTILE_BENCHMARK_FIXTURE']\n"
            "result['workload'] = os.environ['CAOL_HOSTILE_BENCHMARK_WORKLOAD']\n"
            "result['repetition'] = os.environ['CAOL_HOSTILE_BENCHMARK_REPETITION']\n"
            "result['variant'] = os.environ['CAOL_HOSTILE_BENCHMARK_VARIANT']\n"
            "result['rng_seed'] = int(os.environ['CAOL_HOSTILE_BENCHMARK_SEED'])\n"
            "os.write(1, b'x' * 4096)\n"
            "pathlib.Path(os.environ['CAOL_HOSTILE_BENCHMARK_OUTPUT']).write_text(json.dumps(result))\n",
            encoding="utf-8")
        os.chmod(self.binary_a, 0o700)
        identity = benchmark._binary_identity(self.binary_a)
        source_identity = benchmark._data_source_identity(self.data_root_a)
        with mock.patch.object(benchmark, "_MAX_WARMUP_STREAM_BYTES", 1024):
            record = benchmark._run_warmup(
                identity, "A", sample_case(), 0, benchmark._warmup_seed(41), (), 10,
                source_data_root=source_identity)
        self.assertEqual(record["status"], "failed")
        self.assertEqual(record["failure_code"], "stream_limit")
        self.assertGreater(record["stdout_bytes"], 1024)
        self.assertIsNone(record["child_result_binding"])

    def test_already_warm_cache_rejects_before_warmup_or_measurement(self):
        cache = self.data_root_a / "data" / "cache"
        cache.mkdir()
        (cache / "existing.fb").write_bytes(b"already warm")
        warmed = False
        measured = False

        def warmup(*args):
            nonlocal warmed
            warmed = True
            return self.fake_warmup(*args)

        def child(*args):
            nonlocal measured
            measured = True
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 21,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=warmup, child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "warmup")
        self.assertIn("empty runtime data cache", payload["failures"][0]["message"])
        self.assertFalse(warmed)
        self.assertFalse(measured)

    def test_noop_warmup_rejects_before_measurement(self):
        measured = False

        def noop_warmup(*args):
            source_data_root = args[-1]
            result = self.fake_warmup(*args)
            cache = pathlib.Path(source_data_root["data_path"]) / "cache"
            (cache / "generated.fb").unlink()
            cache.rmdir()
            return result

        def child(*args):
            nonlocal measured
            measured = True
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 22,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=noop_warmup, child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "warmup")
        self.assertIn("did not populate", payload["failures"][0]["message"])
        self.assertFalse(measured)

    def test_source_mutation_during_warmup_rejects_before_measurement(self):
        measured = False

        def source_mutating_warmup(*args):
            source_data_root = args[-1]
            (pathlib.Path(source_data_root["data_path"]) / "identity.json").write_text(
                "mutated", encoding="utf-8")
            return self.fake_warmup(*args)

        def child(*args):
            nonlocal measured
            measured = True
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 23,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=source_mutating_warmup, child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "hash")
        self.assertIn("changed during warmup", payload["failures"][0]["message"])
        self.assertFalse(measured)

    def test_cache_mutation_after_warmed_identity_capture_rejects_packet(self):
        def cache_creating_warmup(*args):
            source_data_root = args[-1]
            cache = pathlib.Path(source_data_root["data_path"]) / "cache"
            cache.mkdir()
            (cache / "generated.fb").write_bytes(b"warm")
            return self.fake_warmup(*args)

        def cache_mutating_child(*args):
            data_root = args[-1]
            (pathlib.Path(data_root["data_path"]) / "cache" / "late.fb").write_bytes(b"late")
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 29,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=cache_creating_warmup, child_runner=cache_mutating_child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "hash")
        self.assertIn("data root 'A' changed", payload["failures"][0]["message"])

    def test_failed_warmup_is_recorded_and_rejects_before_measurement(self):
        measured = False

        def failed_warmup(*args):
            record = self.fake_warmup(*args)
            record.update({"status": "failed", "exit_code": 9,
                           "child_result_binding": None, "failure_code": "child"})
            return record

        def child(*args):
            nonlocal measured
            measured = True
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 31,
            data_roots=self.declared_data_roots("A"), build_detector=lambda: [],
            warmup_runner=failed_warmup, child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "warmup")
        self.assertEqual(payload["warmups"][0]["exit_code"], 9)
        self.assertTrue(benchmark._valid_sha256(payload["warmups"][0]["stdout_sha256"]))
        self.assertFalse(measured)

    def test_concurrent_build_rejects_before_first_child(self):
        called = False

        def child(*args):
            nonlocal called
            called = True
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 1, 1,
            data_roots=self.declared_data_roots("A"),
            build_detector=lambda: [{"pid": 9, "command": "clang++"}],
            child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "concurrent_build")
        self.assertFalse(called)

    def test_run_requires_explicit_data_root_for_every_binary(self):
        with self.assertRaisesRegex(benchmark.BenchmarkError, "cover every binary label"):
            benchmark.run_benchmarks(
                self.matrix_path, {"A": self.binary_a, "B": self.binary_b}, 1, 1,
                data_roots={"A": self.data_root_a}, build_detector=lambda: [],
                child_runner=self.fake_child)

    def test_run_rejects_shared_data_root_between_variants(self):
        with self.assertRaisesRegex(benchmark.BenchmarkError, "own declared data root"):
            benchmark.run_benchmarks(
                self.matrix_path, {"A": self.binary_a, "B": self.binary_b}, 1, 1,
                data_roots={"A": self.data_root_a, "B": self.data_root_a},
                build_detector=lambda: [], child_runner=self.fake_child)

    def test_data_root_rejects_root_level_directory_symlink(self):
        target = self.root / "root-level-target"
        target.mkdir()
        link = self.data_root_a / "data" / "linked-directory"
        self.make_directory_symlink_or_skip(target, link)
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "relative path linked-directory") as raised:
            benchmark._data_root_identity(self.data_root_a)
        self.assertEqual(raised.exception.details["relative_path"], "linked-directory")

    def test_data_root_rejects_data_directory_symlink_itself(self):
        data_path = self.data_root_a / "data"
        real_data_path = self.data_root_a / "real-data"
        data_path.rename(real_data_path)
        self.make_directory_symlink_or_skip(real_data_path, data_path)
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "symlink at relative path \\.") as raised:
            benchmark._data_root_identity(self.data_root_a)
        self.assertEqual(raised.exception.details["relative_path"], ".")

    def test_data_root_rejects_nested_directory_symlink(self):
        target = self.root / "nested-target"
        target.mkdir()
        nested = self.data_root_a / "data" / "ordinary-directory"
        nested.mkdir()
        link = nested / "linked-directory"
        self.make_directory_symlink_or_skip(target, link)
        with self.assertRaisesRegex(
                benchmark.BenchmarkError,
                "relative path ordinary-directory/linked-directory") as raised:
            benchmark._data_root_identity(self.data_root_a)
        self.assertEqual(raised.exception.details["relative_path"],
                         "ordinary-directory/linked-directory")

    def test_data_root_rejects_ordinary_file_symlink(self):
        link = self.data_root_a / "data" / "identity-link.json"
        try:
            os.symlink("identity.json", link)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"file symlinks unavailable: {error}")
        with self.assertRaisesRegex(benchmark.BenchmarkError,
                                    "relative path identity-link.json") as raised:
            benchmark._data_root_identity(self.data_root_a)
        self.assertEqual(raised.exception.details["relative_path"], "identity-link.json")

    def test_data_root_walk_error_rejects_without_exposing_absolute_path(self):
        denied_path = self.data_root_a.resolve() / "data" / "private" / "subtree"

        def failing_walk(_path, *, followlinks, onerror):
            self.assertFalse(followlinks)
            onerror(PermissionError(errno.EACCES, "sensitive operating-system detail",
                                     str(denied_path)))
            return iter(())

        with mock.patch.object(benchmark.os, "walk", side_effect=failing_walk):
            with self.assertRaisesRegex(
                    benchmark.BenchmarkError,
                    "relative path private/subtree") as raised:
                benchmark._data_root_identity(self.data_root_a)
        self.assertEqual(raised.exception.code, "data_root")
        self.assertEqual(raised.exception.details,
                         {"relative_path": "private/subtree", "errno": errno.EACCES})
        self.assertNotIn(str(self.data_root_a), raised.exception.message)
        self.assertNotIn("sensitive operating-system detail", raised.exception.message)

    def test_run_rejects_packet_above_raw_observation_cap(self):
        matrix = sample_matrix()
        matrix["cases"][0]["env"]["CAOL_HOSTILE_BENCHMARK_UPDATES"] = "100000"
        self.matrix_path.write_text(json.dumps(matrix), encoding="utf-8")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "observation packet cap"):
            benchmark.run_benchmarks(
                self.matrix_path, {"A": self.binary_a, "B": self.binary_b}, 21, 1,
                data_roots=self.declared_data_roots("A", "B"),
                build_detector=lambda: [], child_runner=self.fake_child)

    def test_declared_child_failure_rejects_and_keeps_prior_valid_runs(self):
        count = 0

        def child(*args):
            nonlocal count
            count += 1
            if count == 2:
                raise benchmark.BenchmarkError("child", "declared failure")
            return self.fake_child(*args)

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a}, 3, 1,
            data_roots=self.declared_data_roots("A"),
            build_detector=lambda: [], warmup_runner=self.fake_warmup,
            child_runner=child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(len(payload["runs"]), 1)
        self.assertEqual(payload["failures"][0]["code"], "child")

    def test_run_rejects_divergent_terminal_state_before_reporting_success(self):
        matrix = sample_matrix()
        matrix["require_equivalent_terminal_state"] = True
        self.matrix_path.write_text(json.dumps(matrix), encoding="utf-8")

        def divergent_child(*args):
            run = self.fake_child(*args)
            if run["variant"] == "B":
                run["result"]["deterministic_state"]["terminal_sha256"] = "9" * 64
            return run

        document = benchmark.run_benchmarks(
            self.matrix_path, {"A": self.binary_a, "B": self.binary_b}, 1, 1,
            data_roots=self.declared_data_roots("A", "B"),
            build_detector=lambda: [], warmup_runner=self.fake_warmup,
            child_runner=divergent_child)
        payload = benchmark.validate_raw(document)
        self.assertEqual(payload["status"], "rejected")
        self.assertEqual(payload["failures"][0]["code"], "hash")
        self.assertIn("terminal states diverged", payload["failures"][0]["message"])

    def test_run_rejects_matrix_above_retained_rss_packet_cap(self):
        matrix = sample_matrix()
        original = matrix["cases"][0]
        case_count = benchmark._MAX_PACKET_RSS_SAMPLES // benchmark._MAX_CHILD_RSS_SAMPLES + 1
        matrix["cases"] = []
        for index in range(case_count):
            case = copy.deepcopy(original)
            case["id"] = f"case-{index}"
            matrix["cases"].append(case)
        self.matrix_path.write_text(json.dumps(matrix), encoding="utf-8")
        with self.assertRaisesRegex(benchmark.BenchmarkError, "RSS-sample packet cap"):
            benchmark.run_benchmarks(
                self.matrix_path, {"A": self.binary_a}, 1, 1,
                data_roots=self.declared_data_roots("A"),
                build_detector=lambda: [], child_runner=self.fake_child)

    def test_windows_child_environment_preserves_only_required_root_and_isolated_temp(self):
        with mock.patch.object(benchmark.sys, "platform", "win32"), mock.patch.dict(
                os.environ, {"SystemRoot": "C:\\Windows", "TEMP": "C:\\parent-temp",
                             "CATA_API_KEY": "sentinel"}, clear=True):
            environment = benchmark._minimal_child_environment(
                "C:\\isolated", {"SystemRoot": "C:\\wrong", "TEMP": "C:\\wrong"})
        self.assertEqual(environment["SystemRoot"], "C:\\Windows")
        self.assertEqual(environment["TEMP"], "C:\\isolated")
        self.assertEqual(environment["TMP"], "C:\\isolated")
        self.assertEqual(environment["TMPDIR"], "C:\\isolated")
        self.assertNotIn("CATA_API_KEY", environment)

    def test_non_windows_child_environment_does_not_copy_system_root(self):
        with mock.patch.object(benchmark.sys, "platform", "darwin"), mock.patch.dict(
                os.environ, {"SystemRoot": "not-for-this-platform"}, clear=True):
            environment = benchmark._minimal_child_environment("/isolated", {})
        self.assertEqual(environment, {"TMPDIR": "/isolated"})

    def test_rss_sampler_retains_a_bounded_systematic_series_and_exact_count(self):
        sample_total = benchmark._MAX_CHILD_RSS_SAMPLES * 3

        class FiniteStop:
            def __init__(self):
                self.waits = 0

            def wait(self, _interval):
                self.waits += 1
                return self.waits >= sample_total

        samples = []
        state = {"observation_count": 0, "retention_stride": 1, "peak_bytes": None}
        values = iter(range(1, sample_total + 1))
        with mock.patch.object(benchmark, "_read_rss_bytes", side_effect=lambda _pid: next(values)):
            benchmark._sample_rss(1, FiniteStop(), 0.001, samples, state)
        self.assertEqual(state["observation_count"], sample_total)
        self.assertEqual(state["peak_bytes"], sample_total)
        self.assertGreater(state["retention_stride"], 1)
        self.assertLessEqual(len(samples), benchmark._MAX_CHILD_RSS_SAMPLES)
        self.assertEqual(samples[0]["rss_bytes"], 1)

    def test_rss_sampler_quiescence_fails_if_thread_remains_alive(self):
        stop = mock.Mock()
        sampler = mock.Mock()
        sampler.is_alive.return_value = True
        with self.assertRaisesRegex(benchmark.BenchmarkError, "did not stop"):
            benchmark._quiesce_rss_sampler(sampler, stop)
        stop.set.assert_called_once_with()
        sampler.join.assert_called_once_with(
            timeout=benchmark._RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)

    def test_launched_fake_child_does_not_receive_parent_api_keys(self):
        parent_had_cata_key = "CATA_API_KEY" in os.environ
        parent_had_openai_key = "OPENAI_API_KEY" in os.environ
        fake_child = self.root / "fake-child.py"
        template = json.dumps(child_result(rng_seed=1), sort_keys=True)
        fake_child.write_text(
            "#!/usr/bin/python3\n"
            "import json, os, pathlib\n"
            "secret_names = ('CATA_API_KEY', 'OPENAI_API_KEY', 'GH_TOKEN', "
            "'AWS_SECRET_ACCESS_KEY', 'ANTHROPIC_API_KEY')\n"
            "present = int(any(name in os.environ for name in secret_names))\n"
            f"result = json.loads({template!r})\n"
            "result['fixture'] = os.environ['CAOL_HOSTILE_BENCHMARK_FIXTURE']\n"
            "result['workload'] = os.environ['CAOL_HOSTILE_BENCHMARK_WORKLOAD']\n"
            "result['repetition'] = os.environ['CAOL_HOSTILE_BENCHMARK_REPETITION']\n"
            "result['variant'] = os.environ['CAOL_HOSTILE_BENCHMARK_VARIANT']\n"
            "result['rng_seed'] = int(os.environ['CAOL_HOSTILE_BENCHMARK_SEED'])\n"
            "result['observed_cwd'] = os.getcwd()\n"
            "result['metrics']['api_keys_present'] = present\n"
            "pathlib.Path(os.environ['CAOL_HOSTILE_BENCHMARK_OUTPUT']).write_text(json.dumps(result))\n",
            encoding="utf-8")
        os.chmod(fake_child, 0o700)
        identity = benchmark._binary_identity(fake_child)
        data_root_identity = benchmark._data_root_identity(self.data_root_a)
        with mock.patch.dict(os.environ, {"CATA_API_KEY": "sentinel-a",
                                          "OPENAI_API_KEY": "sentinel-b",
                                          "GH_TOKEN": "sentinel-c",
                                          "AWS_SECRET_ACCESS_KEY": "sentinel-d",
                                          "ANTHROPIC_API_KEY": "sentinel-e"}):
            run = benchmark._run_child(identity, "A", sample_case(), 0, 0, 1, (), 10, 0.01,
                                       data_root=data_root_identity)
        self.assertEqual(run["result"]["metrics"]["api_keys_present"], 0)
        self.assertEqual(run["result"]["observed_cwd"], str(self.data_root_a.resolve()))
        self.assertEqual(run["working_directory"], str(self.data_root_a.resolve()))
        self.assertEqual(run["command"][1:3], ["--rng-seed", "1"])
        self.assertGreaterEqual(run["rss_observation_count"], len(run["rss_samples"]))
        self.assertEqual("CATA_API_KEY" in os.environ, parent_had_cata_key)
        self.assertEqual("OPENAI_API_KEY" in os.environ, parent_had_openai_key)


if __name__ == "__main__":
    unittest.main()
