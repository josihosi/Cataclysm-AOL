#!/usr/bin/env python3
"""Deterministic orchestration for hostile-camp performance measurements.

The tool deliberately does not implement a benchmark workload.  It launches an
instrumented game/test binary in a fresh process for each repetition and keeps
the A/B order, hashes, validation, RSS samples, raw child result, and summary in
machine-readable JSON.

Child contract (``caol-hostile-camp-benchmark-result-v1``)
==========================================================

For every invocation the runner sets these environment variables::

    CAOL_HOSTILE_BENCHMARK_FIXTURE
    CAOL_HOSTILE_BENCHMARK_FIXTURE_SHA256
    CAOL_HOSTILE_BENCHMARK_FIXTURE_HASH_KIND
    CAOL_HOSTILE_BENCHMARK_WORKLOAD
    CAOL_HOSTILE_BENCHMARK_REPETITION
    CAOL_HOSTILE_BENCHMARK_PAIR_INDEX
    CAOL_HOSTILE_BENCHMARK_VARIANT
    CAOL_HOSTILE_BENCHMARK_SEED
    CAOL_HOSTILE_BENCHMARK_OUTPUT

The child must restore the named pristine fixture, verify its content hash, run
the named deterministic workload, and write one JSON object to the output path.
If the path is left empty, stdout must contain exactly that JSON object.  The
object echoes ``fixture``, ``workload``, ``repetition``, and ``variant``.  It may
also echo ``fixture_sha256``; when present, that hash must match the matrix.  Its
``metrics`` object must contain non-negative
``wall_time_ns``, an ``update_latency_sample_count`` matching the declared
updates, and bounded latency/clock summaries with count, total, exact min/max,
conservative p50/p95/p99 upper bounds, resolution, and overflow metadata.
Further numeric counters/lists/maps are allowed.
``allocation_count`` and ``live_heap_bytes`` may be integers or null until a
platform allocation trace is available.

Each matrix case has ``fixture`` and ``workload``, with optional ``id``,
``fixture_sha256``, ``arguments``, ``env``, and ``thresholds``.  If no ID is
present, ``fixture::workload`` is the stable case ID.  If no fixture hash is
present, the canonical case-object hash identifies the generated fixture spec.
A threshold names a metric and summary statistic and may set ``ratio_max``,
``delta_max``, ``absolute_max``, or ``absolute_min``.  Example::

    {
      "schema": "caol-hostile-camp-benchmark-matrix-v1",
      "cases": [{
        "id": "legacy-10-idle",
        "fixture": "legacy-10",
        "fixture_sha256": "<64 lowercase hex characters>",
        "workload": "idle-24h",
        "arguments": ["[hostile_camp_benchmark]"],
        "thresholds": [{
          "metric": "wall_time_ns", "statistic": "mean",
          "ratio_max": 1.05
        }]
      }]
    }

``run`` executes one child at a time in a minimal allowlisted environment.  A
matrix may name the binary's user-directory flag so every child receives an
isolated temporary profile.  With two binaries the runner creates balanced,
seeded AB/BA pair order (ten pairs by default).  Valid slow runs are never
discarded.  Update counts and retained RSS observations are capped per child
and packet; update, scoped, and clock-floor series remain bounded streaming
summaries in the child result.  A packet is
rejected only for the explicit binary/hash, matrix or child schema, child
execution, concurrent-build, state-equivalence, or observation-cap categories.
"""

from __future__ import annotations

import argparse
import datetime as _datetime
import hashlib
import json
import math
import os
import pathlib
import platform
import signal
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from collections import defaultdict
from typing import Any, Callable, Iterable, Mapping, Sequence


MATRIX_SCHEMA = "caol-hostile-camp-benchmark-matrix-v1"
CHILD_SCHEMA = "caol-hostile-camp-benchmark-result-v1"
RAW_SCHEMA = "caol-hostile-camp-benchmark-raw-v2"
SUMMARY_SCHEMA = "caol-hostile-camp-benchmark-summary-v2"
COMPARE_SCHEMA = "caol-hostile-camp-benchmark-comparison-v1"
_HEX_DIGITS = frozenset("0123456789abcdef")
_NULLABLE_METRICS = ("allocation_count", "live_heap_bytes")
_THRESHOLD_STATISTICS = frozenset(("mean", "min", "max", "p50", "p95", "p99"))
_FIXTURE_HASH_KINDS = frozenset(("serialized_state_sha256", "opaque_sha256",
                                 "generated_case_spec_sha256"))
_CHILD_ENV_ALLOWLIST = frozenset(("PATH", "LANG", "LC_ALL", "LC_CTYPE", "TZ", "TERM"))
_PROBE_SECTIONS = frozenset((
    "world_serialize", "world_deserialize", "structural_maintenance",
    "structural_outings", "structural_scan", "structural_dispatch",
    "live_dispatch_plan", "live_dispatch_apply", "live_return_apply",
))
_PROBE_COUNTERS = frozenset((
    "world_serialize_calls", "world_deserialize_calls", "structural_maintenance_updates",
    "structural_scan_sites_considered", "structural_scan_candidates_sampled",
    "structural_scan_sites_skipped_not_camp", "structural_outing_sites_considered",
    "structural_dispatch_sites_considered", "live_dispatch_plans", "live_dispatch_applies",
    "live_return_applies",
))
_STREAMING_SUMMARY_FIELDS = frozenset((
    "count", "total", "min", "p50", "p95", "p99", "max",
    "quantiles_are_upper_bounds", "relative_resolution_ppm", "overflow",
))
_MAX_CHILD_UPDATE_OBSERVATIONS = 100_000
_MAX_PACKET_UPDATE_OBSERVATIONS = 4_000_000
_MAX_CHILD_RSS_SAMPLES = 1_024
_MAX_PACKET_RSS_SAMPLES = 500_000
_RSS_READ_TIMEOUT_SECONDS = 2.0
_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS = 5.0
_UINT32_MAX = (1 << 32) - 1
_INT32_MAX = (1 << 31) - 1
_DEFAULT_CALENDAR_TURN = 5_220_000
_DEFAULT_SEASON_LENGTH_DAYS = 91
_MAX_SEASON_LENGTH_DAYS = _INT32_MAX // (24 * 60 * 60)
_WARMUP_DIAGNOSTIC_TAIL_BYTES = 4096
_MAX_WARMUP_STREAM_BYTES = 16 * 1024 * 1024
_MAX_CHILD_RESULT_BYTES = 16 * 1024 * 1024
_MAX_MEASURED_STREAM_BYTES = 16 * 1024 * 1024
_PAIRED_CI_MINIMUM_PAIRS = 3
_MAX_PAIRED_BOOTSTRAP_DRAWS = 10_000_000
_PROCESS_CPU_TIME_SOURCES = frozenset((
    "posix_wait4_rusage_v1", "windows_get_process_times_v1",
))
_PAIRED_RUN_METRICS = (
    "benchmark_wall_time_ns", "runner_wall_time_ns", "runner_process_cpu_user_ns",
    "runner_process_cpu_system_ns", "runner_process_cpu_total_ns",
)
_PROCESS_MEMORY_PHASES = (
    "before_fixture_construction", "after_fixture_construction",
    "before_initial_serialization", "after_initial_serialization",
    "before_timing_replay", "after_timing_replay",
    "before_terminal_serialization", "after_terminal_serialization",
    "before_fairness_fixture_construction", "after_fairness_fixture_construction",
    "before_fairness_replay", "after_fairness_replay",
    "before_fairness_serialization", "after_fairness_serialization",
)


class BenchmarkError(RuntimeError):
    """A declared benchmark-packet rejection.

    ``code`` is stable machine-readable policy; the message and details are
    diagnostic only.  Callers should not turn a slow-but-valid result into an
    error.
    """

    def __init__(self, code: str, message: str, details: Mapping[str, Any] | None = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.details = dict(details or {})

    def as_dict(self) -> dict[str, Any]:
        return {"code": self.code, "message": self.message, "details": self.details}


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _is_nonnegative_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and value >= 0


def _require(condition: bool, code: str, message: str, **details: Any) -> None:
    if not condition:
        raise BenchmarkError(code, message, details)


def canonical_json_bytes(value: Any) -> bytes:
    """Return the canonical UTF-8 representation used for content hashes."""

    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
                       allow_nan=False) + "\n").encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: os.PathLike[str] | str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _read_json_bytes(path: os.PathLike[str] | str, code: str) -> tuple[bytes, Any]:
    try:
        data = pathlib.Path(path).read_bytes()
    except OSError as error:
        raise BenchmarkError(code, f"cannot read {path}: {error}") from error
    try:
        return data, json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BenchmarkError(code, f"invalid JSON in {path}: {error}") from error


def _valid_sha256(value: Any) -> bool:
    return isinstance(value, str) and len(value) == 64 and all(ch in _HEX_DIGITS for ch in value)


def _validate_threshold(rule: Any, case_id: str, index: int) -> None:
    context = f"case {case_id!r} threshold {index}"
    _require(isinstance(rule, dict), "matrix_schema", f"{context} must be an object")
    _require(isinstance(rule.get("metric"), str) and rule["metric"], "matrix_schema",
             f"{context} needs a metric")
    _require(rule.get("statistic") in _THRESHOLD_STATISTICS, "matrix_schema",
             f"{context} has an unsupported statistic")
    limits = ("ratio_max", "delta_max", "absolute_max", "absolute_min")
    present = [name for name in limits if name in rule]
    _require(bool(present), "matrix_schema", f"{context} needs at least one limit")
    for name in present:
        _require(_is_number(rule[name]), "matrix_schema", f"{context} {name} must be finite")
    if "ratio_max" in rule:
        _require(rule["ratio_max"] >= 0, "matrix_schema", f"{context} ratio_max is negative")


def _case_id(case: Mapping[str, Any]) -> str:
    return case.get("id") or f"{case['fixture']}::{case['workload']}"


def _generated_case_spec_sha256(case: Mapping[str, Any]) -> str:
    canonical_case = dict(case)
    canonical_case.pop("fixture_sha256", None)
    return sha256_bytes(canonical_json_bytes(canonical_case))


def _case_fixture_sha256(case: Mapping[str, Any], fixture_hash_kind: str | None = None) -> str:
    kind = fixture_hash_kind
    if kind is None:
        kind = "opaque_sha256" if "fixture_sha256" in case else "generated_case_spec_sha256"
    if kind == "generated_case_spec_sha256":
        return _generated_case_spec_sha256(case)
    explicit = case.get("fixture_sha256")
    _require(_valid_sha256(explicit), "matrix_schema",
             f"case {_case_id(case)!r} requires an explicit fixture_sha256 for {kind}")
    return explicit


def _case_expected_count(case: Mapping[str, Any], environment_name: str) -> int:
    value = case.get("env", {}).get(environment_name)
    _require(isinstance(value, str) and value.isdigit() and int(value) > 0,
             "matrix_schema", f"case {_case_id(case)!r} needs positive {environment_name}")
    return int(value)


def _case_calendar(case: Mapping[str, Any]) -> dict[str, Any]:
    environment = case.get("env", {})
    turn_text = environment.get("CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN",
                                str(_DEFAULT_CALENDAR_TURN))
    season_text = environment.get("CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS",
                                  str(_DEFAULT_SEASON_LENGTH_DAYS))
    _require(isinstance(turn_text, str) and turn_text.isdigit() and
             int(turn_text) <= _INT32_MAX, "matrix_schema",
             f"case {_case_id(case)!r} needs a non-negative int32 calendar turn")
    _require(isinstance(season_text, str) and season_text.isdigit() and
             0 < int(season_text) <= _MAX_SEASON_LENGTH_DAYS, "matrix_schema",
             f"case {_case_id(case)!r} needs season days representable as int32 turns")
    return {
        "turn": int(turn_text),
        "start_of_cataclysm_turn": 0,
        "start_of_game_turn": 0,
        "initial_season": "spring",
        "season_length_days": int(season_text),
        "eternal_season": False,
        "eternal_night": False,
        "eternal_day": False,
        "reset_before_timing_replay": True,
        "reset_before_fairness_replay": True,
    }


def _matrix_fixture_hash_kind(matrix: Mapping[str, Any], case: Mapping[str, Any]) -> str:
    configured = matrix.get("fixture_hash_kind")
    if configured is not None:
        return configured
    return "opaque_sha256" if "fixture_sha256" in case else "generated_case_spec_sha256"


def validate_matrix(document: Any) -> dict[str, Any]:
    """Validate and return a benchmark matrix document."""

    _require(isinstance(document, dict), "matrix_schema", "matrix must be an object")
    _require(document.get("schema") == MATRIX_SCHEMA, "matrix_schema",
             f"matrix schema must be {MATRIX_SCHEMA}")
    fixture_hash_kind = document.get("fixture_hash_kind")
    _require(fixture_hash_kind is None or fixture_hash_kind in _FIXTURE_HASH_KINDS,
             "matrix_schema", "matrix fixture_hash_kind is unsupported")
    require_equivalent = document.get("require_equivalent_terminal_state", False)
    _require(isinstance(require_equivalent, bool), "matrix_schema",
             "require_equivalent_terminal_state must be boolean")
    isolated_user_dir_argument = document.get("isolated_user_dir_argument")
    _require(isolated_user_dir_argument is None or
             isinstance(isolated_user_dir_argument, str) and isolated_user_dir_argument,
             "matrix_schema", "isolated_user_dir_argument must be a non-empty string")
    cases = document.get("cases")
    _require(isinstance(cases, list) and cases, "matrix_schema", "matrix cases must be non-empty")
    seen_ids: set[str] = set()
    for index, case in enumerate(cases):
        _require(isinstance(case, dict), "matrix_schema", f"case {index} must be an object")
        explicit_id = case.get("id")
        _require(explicit_id is None or isinstance(explicit_id, str) and explicit_id,
                 "matrix_schema", f"case {index} id must be a non-empty string when present")
        for name in ("fixture", "workload"):
            _require(isinstance(case.get(name), str) and case[name], "matrix_schema",
                     f"case {index} needs non-empty {name}")
        case_id = _case_id(case)
        _require(case_id not in seen_ids, "matrix_schema", f"duplicate case id {case_id!r}")
        seen_ids.add(case_id)
        if "fixture_sha256" in case:
            _require(_valid_sha256(case.get("fixture_sha256")), "matrix_schema",
                     f"case {case_id!r} fixture_sha256 must be lowercase hex")
        case_hash_kind = _matrix_fixture_hash_kind(document, case)
        if case_hash_kind == "generated_case_spec_sha256":
            generated_digest = _generated_case_spec_sha256(case)
            if "fixture_sha256" in case:
                _require(case["fixture_sha256"] == generated_digest, "matrix_schema",
                         f"case {case_id!r} explicit digest contradicts generated case-spec hash",
                         expected=generated_digest, actual=case["fixture_sha256"])
        else:
            _require("fixture_sha256" in case, "matrix_schema",
                     f"case {case_id!r} needs explicit fixture_sha256 for {case_hash_kind}")
        arguments = case.get("arguments", [])
        _require(isinstance(arguments, list) and all(isinstance(arg, str) for arg in arguments),
                 "matrix_schema", f"case {case_id!r} arguments must be strings")
        environment = case.get("env", {})
        _require(isinstance(environment, dict), "matrix_schema",
                 f"case {case_id!r} env must be an object")
        for name, value in environment.items():
            _require(isinstance(name, str) and name and isinstance(value, str), "matrix_schema",
                     f"case {case_id!r} env entries must be string pairs")
            upper_name = name.upper()
            _require(not any(word in upper_name for word in ("PASSWORD", "SECRET", "TOKEN", "API_KEY")),
                     "matrix_schema", f"case {case_id!r} env may not contain secret-like names")
        updates = _case_expected_count(case, "CAOL_HOSTILE_BENCHMARK_UPDATES")
        _require(updates <= _MAX_CHILD_UPDATE_OBSERVATIONS, "matrix_schema",
                 f"case {case_id!r} exceeds the per-child update observation cap")
        _case_expected_count(case, "CAOL_HOSTILE_BENCHMARK_CLOCK_FLOOR_SAMPLES")
        _case_calendar(case)
        thresholds = case.get("thresholds", [])
        _require(isinstance(thresholds, list), "matrix_schema",
                 f"case {case_id!r} thresholds must be a list")
        for threshold_index, rule in enumerate(thresholds):
            _validate_threshold(rule, case_id, threshold_index)
    return document


def load_matrix(path: os.PathLike[str] | str) -> tuple[dict[str, Any], bytes, str]:
    data, document = _read_json_bytes(path, "matrix_schema")
    return validate_matrix(document), data, sha256_bytes(data)


def _validate_metric_value(value: Any, path: str) -> None:
    if value is None:
        return
    if isinstance(value, bool):
        return
    if _is_number(value):
        _require(value >= 0, "child_schema", f"metric {path} must be non-negative")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _require(_is_number(item) and item >= 0, "child_schema",
                     f"metric {path}[{index}] must be a non-negative finite number")
        return
    if isinstance(value, dict):
        for name, item in value.items():
            _require(isinstance(name, str) and name, "child_schema",
                     f"metric {path} contains an invalid key")
            _validate_metric_value(item, f"{path}.{name}")
        return
    raise BenchmarkError("child_schema", f"metric {path} has unsupported type")


def _validate_streaming_summary(value: Any, path: str, expected_count: int | None = None,
                                expected_total: int | None = None) -> None:
    _require(isinstance(value, dict), "child_schema", f"{path} must be an object")
    _require(set(value) == _STREAMING_SUMMARY_FIELDS, "child_schema",
             f"{path} must contain the complete bounded streaming summary",
             expected=sorted(_STREAMING_SUMMARY_FIELDS), actual=sorted(value))
    for name in ("count", "total", "min", "p50", "p95", "p99", "max"):
        _require(_is_nonnegative_int(value[name]), "child_schema",
                 f"{path}.{name} must be a non-negative integer")
    _require(value["quantiles_are_upper_bounds"] is True, "child_schema",
             f"{path} must declare conservative quantile upper bounds")
    _require(value["relative_resolution_ppm"] == 15625, "child_schema",
             f"{path} has an unexpected histogram resolution")
    _require(value["overflow"] is False, "child_schema",
             f"{path} reported histogram overflow")
    if expected_count is not None:
        _require(value["count"] == expected_count, "child_schema",
                 f"{path}.count does not match its enclosing count",
                 expected=expected_count, actual=value["count"])
    if expected_total is not None:
        _require(value["total"] == expected_total, "child_schema",
                 f"{path}.total does not match its enclosing total",
                 expected=expected_total, actual=value["total"])
    if value["count"] == 0:
        _require(not any(value[name] for name in (
            "total", "min", "p50", "p95", "p99", "max",
        )), "child_schema", f"{path} empty summary must contain zero measurements")
    else:
        _require(value["min"] <= value["max"], "child_schema",
                 f"{path} exact bounds are inconsistent")
        _require(value["min"] * value["count"] <= value["total"] <=
                 value["max"] * value["count"], "child_schema",
                 f"{path} total is impossible for its count and exact bounds")
        if value["count"] == 1:
            _require(value["min"] == value["max"] == value["total"], "child_schema",
                     f"{path} single observation must equal its min, max, and total")
        else:
            minimum_total = (value["count"] - 1) * value["min"] + value["max"]
            maximum_total = value["min"] + (value["count"] - 1) * value["max"]
            _require(minimum_total <= value["total"] <= maximum_total, "child_schema",
                     f"{path} total cannot contain both declared exact bounds",
                     expected_min=minimum_total, expected_max=maximum_total,
                     actual=value["total"])
        _require(value["min"] <= value["p50"] <= value["p95"] <= value["p99"] <=
                 value["max"],
                 "child_schema", f"{path} quantile upper bounds must be ordered")
        for quantile_name, percentile in (("p50", 50), ("p95", 95), ("p99", 99)):
            nearest_rank = (percentile * value["count"] + 99) // 100
            if nearest_rank == value["count"]:
                _require(value[quantile_name] == value["max"], "child_schema",
                         f"{path}.{quantile_name} must equal exact max at its nearest rank",
                         count=value["count"], rank=nearest_rank,
                         expected=value["max"], actual=value[quantile_name])


def _validate_probe_section(value: Any, path: str) -> None:
    required = {"calls", "inclusive_total_ns", "inclusive_summary_ns",
                "self_total_ns", "self_summary_ns"}
    _require(isinstance(value, dict) and set(value) == required, "child_schema",
             f"{path} must contain the complete scoped-section summary",
             expected=sorted(required), actual=sorted(value) if isinstance(value, dict) else None)
    for name in ("calls", "inclusive_total_ns", "self_total_ns"):
        _require(_is_nonnegative_int(value[name]), "child_schema",
                 f"{path}.{name} must be a non-negative integer")
    _require(value["self_total_ns"] <= value["inclusive_total_ns"], "child_schema",
             f"{path} self time exceeds inclusive time")
    _validate_streaming_summary(value["inclusive_summary_ns"],
                                f"{path}.inclusive_summary_ns", value["calls"],
                                value["inclusive_total_ns"])
    _validate_streaming_summary(value["self_summary_ns"], f"{path}.self_summary_ns",
                                value["calls"], value["self_total_ns"])
    _require(value["inclusive_summary_ns"]["count"] ==
             value["self_summary_ns"]["count"], "child_schema",
             f"{path} inclusive and self sample counts differ")


def _validate_probe_and_modes(document: Mapping[str, Any]) -> None:
    probe = document.get("probe")
    _require(isinstance(probe, dict), "child_schema", "child probe must be an object")
    _require(probe.get("timings_collected") is True, "child_schema",
             "child timing probe must report timings_collected=true")
    _require(probe.get("site_services_collected") is False, "child_schema",
             "child timing probe must report site_services_collected=false")
    _require(probe.get("stack_overflow") is False, "child_schema",
             "child timing probe reported a scoped-section stack overflow")
    sections = probe.get("sections")
    _require(isinstance(sections, dict) and set(sections) == _PROBE_SECTIONS,
             "child_schema", "child probe must contain every scoped section exactly once",
             expected=sorted(_PROBE_SECTIONS),
             actual=sorted(sections) if isinstance(sections, dict) else None)
    for name in sorted(_PROBE_SECTIONS):
        _validate_probe_section(sections[name], f"probe.sections.{name}")
    counters = probe.get("counters")
    _require(isinstance(counters, dict) and set(counters) == _PROBE_COUNTERS,
             "child_schema", "child probe must contain every fixed counter exactly once",
             expected=sorted(_PROBE_COUNTERS),
             actual=sorted(counters) if isinstance(counters, dict) else None)
    for name in _PROBE_COUNTERS:
        _require(_is_nonnegative_int(counters[name]), "child_schema",
                 f"probe.counters.{name} must be a non-negative integer")

    modes = document.get("measurement_modes")
    _require(isinstance(modes, dict) and set(modes) == {
        "latency", "fairness", "terminal_state_match",
    }, "child_schema", "child measurement_modes must describe both replays and state matching")
    _require(isinstance(modes["latency"], str) and modes["latency"], "child_schema",
             "measurement_modes.latency must be non-empty")
    _require(isinstance(modes["fairness"], str) and modes["fairness"], "child_schema",
             "measurement_modes.fairness must be non-empty")
    _require(modes["terminal_state_match"] is True, "child_schema",
             "timing and fairness replay terminal states must match")


def _validate_calendar(document: Any, expected: Mapping[str, Any] | None = None) -> None:
    required = {
        "turn", "start_of_cataclysm_turn", "start_of_game_turn", "initial_season",
        "season_length_days", "eternal_season", "eternal_night", "eternal_day",
        "reset_before_timing_replay", "reset_before_fairness_replay",
    }
    _require(isinstance(document, dict) and set(document) == required, "child_schema",
             "child calendar must contain the complete deterministic configuration")
    _require(_is_nonnegative_int(document["turn"]) and document["turn"] <= _INT32_MAX,
             "child_schema", "child calendar turn must be a non-negative int32")
    _require(_is_nonnegative_int(document["season_length_days"]) and
             0 < document["season_length_days"] <= _MAX_SEASON_LENGTH_DAYS,
             "child_schema",
             "child calendar season length must be representable as int32 turns")
    _require(document["start_of_cataclysm_turn"] == 0 and
             document["start_of_game_turn"] == 0 and
             document["initial_season"] == "spring", "child_schema",
             "child calendar origin or initial season is not deterministic")
    for name in ("eternal_season", "eternal_night", "eternal_day"):
        _require(document[name] is False, "child_schema",
                 f"child calendar {name} must be false")
    for name in ("reset_before_timing_replay", "reset_before_fairness_replay"):
        _require(document[name] is True, "child_schema",
                 f"child calendar {name} must be true")
    if expected is not None:
        _require(document == expected, "child_schema",
                 "child echoed wrong calendar configuration",
                 expected=dict(expected), actual=document)


def validate_child_result(document: Any, expected: Mapping[str, Any] | None = None) -> dict[str, Any]:
    """Validate a child result, including its echoed invocation identity."""

    _require(isinstance(document, dict), "child_schema", "child result must be an object")
    _require(document.get("schema") == CHILD_SCHEMA, "child_schema",
             f"child schema must be {CHILD_SCHEMA}")
    for name in ("fixture", "workload", "variant"):
        _require(isinstance(document.get(name), str) and document[name], "child_schema",
                 f"child result needs non-empty {name}")
    if "fixture_sha256" in document:
        _require(_valid_sha256(document.get("fixture_sha256")), "child_schema",
                 "child fixture_sha256 must be lowercase hex")
    repetition = document.get("repetition")
    _require(_is_nonnegative_int(repetition) or
             isinstance(repetition, str) and repetition.isdigit(), "child_schema",
             "child repetition must be a non-negative integer or decimal string")
    rng_seed = document.get("rng_seed")
    _require(_is_nonnegative_int(rng_seed) and 0 < rng_seed <= _UINT32_MAX,
             "child_schema", "child rng_seed must be a nonzero uint32")
    updates = document.get("updates")
    clock_floor_samples = document.get("clock_floor_samples")
    _require(_is_nonnegative_int(updates) and updates > 0, "child_schema",
             "child updates must be a positive integer")
    _require(_is_nonnegative_int(clock_floor_samples) and clock_floor_samples > 0,
             "child_schema", "child clock_floor_samples must be a positive integer")
    metrics = document.get("metrics")
    _require(isinstance(metrics, dict), "child_schema", "child metrics must be an object")
    _require(_is_nonnegative_int(metrics.get("wall_time_ns")), "child_schema",
             "metrics.wall_time_ns must be a non-negative integer")
    _require(metrics.get("update_latency_sample_count") == updates, "child_schema",
             "update latency count does not match child updates",
             expected=updates, actual=metrics.get("update_latency_sample_count"))
    _require(metrics.get("clock_floor_sample_count") == clock_floor_samples,
             "child_schema", "clock-floor count does not match child contract",
             expected=clock_floor_samples, actual=metrics.get("clock_floor_sample_count"))
    _validate_streaming_summary(metrics.get("update_latency_summary_ns"),
                                "metrics.update_latency_summary_ns", updates)
    _validate_streaming_summary(metrics.get("clock_floor_summary_ns"),
                                "metrics.clock_floor_summary_ns", clock_floor_samples)
    for name, value in metrics.items():
        _validate_metric_value(value, name)
    for name in _NULLABLE_METRICS:
        if name in metrics:
            _require(metrics[name] is None or _is_nonnegative_int(metrics[name]), "child_schema",
                     f"metrics.{name} must be a non-negative integer or null")
    _validate_probe_and_modes(document)
    _validate_calendar(document.get("calendar"), expected.get("calendar") if expected else None)
    deterministic_state = document.get("deterministic_state")
    _require(isinstance(deterministic_state, dict), "child_schema",
             "child deterministic_state must be an object")
    _require(deterministic_state.get("hash_algorithm") == "sha256", "child_schema",
             "child deterministic state must use sha256")
    for name in ("initial_sha256", "terminal_sha256"):
        _require(_valid_sha256(deterministic_state.get(name)), "child_schema",
                 f"child deterministic_state.{name} must be lowercase hex")
    if expected:
        for name in ("fixture", "workload", "variant"):
            _require(document.get(name) == expected.get(name), "child_schema",
                     f"child echoed wrong {name}", expected=expected.get(name),
                     actual=document.get(name))
        _require(str(document.get("repetition")) == str(expected.get("repetition")), "child_schema",
                 "child echoed wrong repetition", expected=expected.get("repetition"),
                 actual=document.get("repetition"))
        _require(rng_seed == expected.get("rng_seed"), "child_schema",
                 "child echoed wrong rng_seed", expected=expected.get("rng_seed"),
                 actual=rng_seed)
        _require(updates == expected.get("updates"), "child_schema",
                 "child echoed wrong update count", expected=expected.get("updates"), actual=updates)
        _require(clock_floor_samples == expected.get("clock_floor_samples"), "child_schema",
                 "child echoed wrong clock-floor count",
                 expected=expected.get("clock_floor_samples"), actual=clock_floor_samples)
        if "fixture_sha256" in document:
            _require(document["fixture_sha256"] == expected.get("fixture_sha256"), "child_schema",
                     "child echoed wrong fixture_sha256", expected=expected.get("fixture_sha256"),
                     actual=document["fixture_sha256"])
        if expected.get("fixture_hash_kind") == "serialized_state_sha256":
            _require(deterministic_state["initial_sha256"] == expected.get("fixture_sha256"),
                     "child_schema", "serialized fixture hash does not match initial state",
                     expected=expected.get("fixture_sha256"),
                     actual=deterministic_state["initial_sha256"])
    return document


class _StableRandom:
    """Small fixed PRNG so benchmark order/bootstrap do not depend on Python's RNG."""

    _MASK = (1 << 64) - 1

    def __init__(self, seed: int):
        self._state = seed & self._MASK

    def next_u64(self) -> int:
        self._state = (self._state + 0x9E3779B97F4A7C15) & self._MASK
        value = self._state
        value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & self._MASK
        value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & self._MASK
        return (value ^ (value >> 31)) & self._MASK

    def randbelow(self, upper: int) -> int:
        if upper <= 0:
            raise ValueError("upper must be positive")
        cutoff = ((1 << 64) // upper) * upper
        while True:
            value = self.next_u64()
            if value < cutoff:
                return value % upper

    def shuffle(self, values: list[Any]) -> None:
        for index in range(len(values) - 1, 0, -1):
            other = self.randbelow(index + 1)
            values[index], values[other] = values[other], values[index]


def paired_orders(seed: int, pair_count: int, labels: Sequence[str] = ("A", "B")) -> list[tuple[str, ...]]:
    """Return seeded balanced AB/BA order; imbalance is at most one for odd counts."""

    if pair_count <= 0:
        raise ValueError("pair_count must be positive")
    if len(labels) == 1:
        return [(labels[0],)] * pair_count
    if len(labels) != 2 or labels[0] == labels[1]:
        raise ValueError("paired ordering needs two distinct labels")
    forward = (labels[0], labels[1])
    reverse = (labels[1], labels[0])
    orders = [forward if index % 2 == 0 else reverse for index in range(pair_count)]
    _StableRandom(seed).shuffle(orders)
    return orders


def nearest_rank_percentile(values: Iterable[float | int], percentile: float) -> float | int:
    """Nearest-rank percentile with one-based ``ceil(p*n/100)`` indexing.

    p=0 selects the minimum.  This explicit rule avoids interpolation or an
    accidental zero-based tail index and is used for both observations and
    bootstrap distributions.
    """

    ordered = sorted(values)
    if not ordered:
        raise ValueError("percentile needs at least one value")
    if not 0 <= percentile <= 100:
        raise ValueError("percentile must be in [0, 100]")
    if percentile == 0:
        return ordered[0]
    rank = math.ceil(percentile * len(ordered) / 100.0)
    return ordered[max(0, min(len(ordered) - 1, rank - 1))]


def bootstrap_mean_ci(values: Sequence[float | int], seed: int, samples: int = 5000,
                      confidence: float = 0.95) -> list[float]:
    """Return a deterministic non-parametric bootstrap CI for the arithmetic mean."""

    if not values:
        raise ValueError("bootstrap needs at least one value")
    if samples <= 0:
        raise ValueError("samples must be positive")
    if not 0 < confidence < 1:
        raise ValueError("confidence must be in (0, 1)")
    numeric = [float(value) for value in values]
    rng = _StableRandom(seed)
    means: list[float] = []
    count = len(numeric)
    for _ in range(samples):
        means.append(sum(numeric[rng.randbelow(count)] for _ in range(count)) / count)
    tail = (1.0 - confidence) * 50.0
    return [float(nearest_rank_percentile(means, tail)),
            float(nearest_rank_percentile(means, 100.0 - tail))]


def _derived_seed(seed: int, case_index: int, pair_index: int) -> int:
    value = f"{seed}:{case_index}:{pair_index}".encode("ascii")
    digest = int.from_bytes(hashlib.sha256(value).digest()[:4], "big")
    return digest % (_UINT32_MAX - 1) + 1


def _warmup_seed(seed: int) -> int:
    value = f"warmup:{seed}".encode("ascii")
    digest = int.from_bytes(hashlib.sha256(value).digest()[:4], "big")
    return digest % (_UINT32_MAX - 1) + 1


def _binary_identity(path: os.PathLike[str] | str) -> dict[str, Any]:
    resolved = pathlib.Path(path).expanduser().resolve()
    _require(resolved.is_file(), "binary", f"binary does not exist: {resolved}")
    _require(os.access(resolved, os.X_OK), "binary", f"binary is not executable: {resolved}")
    stat = resolved.stat()
    return {"path": str(resolved), "size_bytes": stat.st_size, "sha256": sha256_file(resolved)}


def _data_tree_identity(path: os.PathLike[str] | str,
                        include_runtime_cache: bool) -> dict[str, Any]:
    root = pathlib.Path(path).expanduser().resolve()
    _require(root.is_dir(), "data_root", f"data root does not exist: {root}")
    data_path = root / "data"
    _require(data_path.is_dir(), "data_root", f"data root has no data directory: {root}")
    _require(not data_path.is_symlink(), "data_root",
             "data root contains a symlink at relative path .",
             relative_path=".")
    digest = hashlib.sha256()
    file_count = 0
    total_bytes = 0

    def walk_error(error: OSError) -> None:
        relative_path = "<unknown>"
        error_filename = getattr(error, "filename", None)
        if isinstance(error_filename, (str, os.PathLike)):
            candidate = pathlib.Path(error_filename)
            if candidate.is_absolute():
                try:
                    relative_path = candidate.relative_to(data_path).as_posix() or "."
                except ValueError:
                    pass
            elif ".." not in candidate.parts:
                relative_path = candidate.as_posix() or "."
        raise BenchmarkError(
            "data_root",
            f"cannot traverse data root at relative path {relative_path}",
            {"relative_path": relative_path, "errno": getattr(error, "errno", None)},
        )

    for current_root, directory_names, file_names in os.walk(
            data_path, followlinks=False, onerror=walk_error):
        directory_names.sort()
        file_names.sort()
        current = pathlib.Path(current_root)
        retained_directories: list[str] = []
        for name in directory_names:
            source = current / name
            if source.is_symlink():
                relative = source.relative_to(data_path).as_posix()
                raise BenchmarkError(
                    "data_root",
                    f"data root contains a symlink at relative path {relative}",
                    {"relative_path": relative},
                )
            if (not include_runtime_cache and current == data_path and name == "cache"):
                continue
            retained_directories.append(name)
        directory_names[:] = retained_directories
        for name in file_names:
            source = current / name
            relative = source.relative_to(data_path).as_posix()
            if source.is_symlink():
                raise BenchmarkError(
                    "data_root",
                    f"data root contains a symlink at relative path {relative}",
                    {"relative_path": relative},
                )
            _require(source.is_file(), "data_root",
                     f"data root contains unsupported entry: {source}")
            stat = source.stat()
            record = {"kind": "file", "path": relative, "size_bytes": stat.st_size,
                      "sha256": sha256_file(source)}
            file_count += 1
            total_bytes += stat.st_size
            digest.update(canonical_json_bytes(record))
    return {
        "path": str(root),
        "data_path": str(data_path),
        "manifest_kind": ("recursive_file_content_sha256_v1" if include_runtime_cache else
                          "recursive_source_file_content_sha256_excluding_cache_v1"),
        "manifest_sha256": digest.hexdigest(),
        "file_count": file_count,
        "total_bytes": total_bytes,
    }


def _data_root_identity(path: os.PathLike[str] | str) -> dict[str, Any]:
    return _data_tree_identity(path, include_runtime_cache=True)


def _data_source_identity(path: os.PathLike[str] | str) -> dict[str, Any]:
    return _data_tree_identity(path, include_runtime_cache=False)


def _require_warmed_data_root_transition(label: str, pre_warm_identity: Mapping[str, Any],
                                         source_identity: Mapping[str, Any],
                                         warmed_identity: Mapping[str, Any]) -> None:
    _require(
        warmed_identity["file_count"] > source_identity["file_count"] and
        (warmed_identity["manifest_sha256"], warmed_identity["file_count"],
         warmed_identity["total_bytes"]) !=
        (pre_warm_identity["manifest_sha256"], pre_warm_identity["file_count"],
         pre_warm_identity["total_bytes"]),
        "warmup", f"variant {label!r} warmup did not populate the runtime data cache",
        variant=label,
        pre_warm_manifest_sha256=pre_warm_identity["manifest_sha256"],
        warmed_manifest_sha256=warmed_identity["manifest_sha256"],
        source_file_count=source_identity["file_count"],
        warmed_file_count=warmed_identity["file_count"],
    )


def find_concurrent_build_processes() -> list[dict[str, Any]]:
    """Return active compiler/build processes without recording their arguments."""

    if os.name != "posix" or not shutil.which("ps"):
        return []
    try:
        completed = subprocess.run(["ps", "-axo", "pid=,comm="], check=True,
                                   text=True, capture_output=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return []
    own_pid = os.getpid()
    build_names = {
        "make", "gmake", "ninja", "clang", "clang++", "gcc", "g++", "cc", "c++",
        "ld", "ld64", "xcodebuild", "msbuild", "link.exe",
    }
    found: list[dict[str, Any]] = []
    for line in completed.stdout.splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) != 2:
            continue
        try:
            pid = int(fields[0])
        except ValueError:
            continue
        command = pathlib.Path(fields[1]).name
        if pid != own_pid and command.lower() in build_names:
            found.append({"pid": pid, "command": command})
    return found


def _read_rss_bytes(pid: int) -> int | None:
    if sys.platform == "darwin":
        try:
            completed = subprocess.run(["/bin/ps", "-o", "rss=", "-p", str(pid)],
                                       check=False, text=True, capture_output=True,
                                       timeout=_RSS_READ_TIMEOUT_SECONDS)
            value = completed.stdout.strip()
            return int(value) * 1024 if value else None
        except (OSError, ValueError, subprocess.SubprocessError):
            return None
    status = pathlib.Path(f"/proc/{pid}/status")
    if status.exists():
        try:
            for line in status.read_text(encoding="utf-8").splitlines():
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) * 1024
        except (OSError, ValueError, IndexError):
            return None
    return None


def _sample_rss(pid: int, stop: threading.Event, interval_seconds: float,
                samples: list[dict[str, int]], state: dict[str, int | None]) -> None:
    while True:
        rss = _read_rss_bytes(pid)
        if rss is not None:
            state["observation_count"] = int(state["observation_count"] or 0) + 1
            current_peak = state["peak_bytes"]
            state["peak_bytes"] = rss if current_peak is None else max(int(current_peak), rss)
            stride = int(state["retention_stride"] or 1)
            if (int(state["observation_count"]) - 1) % stride == 0:
                samples.append({"monotonic_ns": time.monotonic_ns(), "rss_bytes": rss})
                if len(samples) > _MAX_CHILD_RSS_SAMPLES:
                    samples[:] = samples[::2]
                    state["retention_stride"] = stride * 2
        if stop.wait(interval_seconds):
            return


def _quiesce_rss_sampler(sampler: threading.Thread, stop: threading.Event) -> None:
    stop.set()
    sampler.join(timeout=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)
    _require(not sampler.is_alive(), "child",
             "RSS sampler did not stop within its bounded quiescence timeout",
             timeout_seconds=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)


def _process_cpu_time_source() -> str:
    if sys.platform == "win32":
        return "windows_get_process_times_v1"
    _require(os.name == "posix" and hasattr(os, "wait4"), "child",
             "exact child process CPU measurement is unavailable on this platform")
    return "posix_wait4_rusage_v1"


def _process_cpu_time_record(source: str, user_ns: int, system_ns: int) -> dict[str, Any]:
    _require(source in _PROCESS_CPU_TIME_SOURCES, "child",
             "unknown child process CPU measurement source")
    _require(_is_nonnegative_int(user_ns) and _is_nonnegative_int(system_ns), "child",
             "child process CPU measurements must be non-negative integer nanoseconds")
    return {
        "source": source,
        "user_ns": user_ns,
        "system_ns": system_ns,
        "total_ns": user_ns + system_ns,
    }


def _process_cpu_time_from_rusage(usage: Any) -> dict[str, Any]:
    user_seconds = float(usage.ru_utime)
    system_seconds = float(usage.ru_stime)
    _require(math.isfinite(user_seconds) and user_seconds >= 0 and
             math.isfinite(system_seconds) and system_seconds >= 0, "child",
             "wait4 returned invalid child process CPU measurements")
    return _process_cpu_time_record(
        "posix_wait4_rusage_v1", int(round(user_seconds * 1_000_000_000)),
        int(round(system_seconds * 1_000_000_000)))


def _windows_filetime_to_ns(value: Any) -> int:
    ticks_100ns = (int(value.dwHighDateTime) << 32) | int(value.dwLowDateTime)
    return ticks_100ns * 100


def _windows_process_cpu_time(process: subprocess.Popen[bytes]) -> dict[str, Any]:
    try:
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        get_process_times = kernel32.GetProcessTimes
        get_process_times.argtypes = [
            wintypes.HANDLE, ctypes.POINTER(wintypes.FILETIME),
            ctypes.POINTER(wintypes.FILETIME), ctypes.POINTER(wintypes.FILETIME),
            ctypes.POINTER(wintypes.FILETIME),
        ]
        get_process_times.restype = wintypes.BOOL
        creation = wintypes.FILETIME()
        exit_time = wintypes.FILETIME()
        kernel = wintypes.FILETIME()
        user = wintypes.FILETIME()
        handle = getattr(process, "_handle", None)
        _require(handle is not None, "child",
                 "Windows child process handle is unavailable for CPU measurement")
        if not get_process_times(handle, ctypes.byref(creation), ctypes.byref(exit_time),
                                 ctypes.byref(kernel), ctypes.byref(user)):
            error_code = ctypes.get_last_error()
            raise BenchmarkError(
                "child", "GetProcessTimes failed for measured child",
                {"windows_error": error_code})
        return _process_cpu_time_record(
            "windows_get_process_times_v1", _windows_filetime_to_ns(user),
            _windows_filetime_to_ns(kernel))
    except BenchmarkError:
        raise
    except (AttributeError, OSError, TypeError, ValueError) as error:
        raise BenchmarkError(
            "child", f"could not read exact Windows child process CPU time: {error}") from error


def _signal_measured_process(process: subprocess.Popen[bytes], terminate: bool) -> None:
    try:
        if os.name == "posix":
            os.kill(process.pid, signal.SIGTERM if terminate else signal.SIGKILL)
        elif terminate:
            process.terminate()
        else:
            process.kill()
    except (OSError, ProcessLookupError):
        pass


def _drain_bounded_measured_stream(stream: Any, process: subprocess.Popen[bytes],
                                   state: dict[str, Any]) -> None:
    digest = hashlib.sha256()
    total_bytes = 0
    content = bytearray()
    tail = bytearray()
    try:
        while True:
            chunk = stream.read(64 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            total_bytes += len(chunk)
            tail.extend(chunk)
            if len(tail) > _WARMUP_DIAGNOSTIC_TAIL_BYTES:
                del tail[:-_WARMUP_DIAGNOSTIC_TAIL_BYTES]
            remaining = _MAX_MEASURED_STREAM_BYTES - len(content)
            if remaining > 0:
                content.extend(chunk[:remaining])
            if total_bytes > _MAX_MEASURED_STREAM_BYTES and not state.get("exceeded", False):
                state["exceeded"] = True
                _signal_measured_process(process, True)
    finally:
        stream.close()
        state.update({
            "sha256": digest.hexdigest(), "bytes": total_bytes,
            "content": bytes(content), "tail": bytes(tail),
        })


def _wait_posix_child_cpu(process: subprocess.Popen[bytes],
                          timeout_seconds: float) -> tuple[dict[str, Any], bool]:
    state: dict[str, Any] = {}

    def wait_exact_child() -> None:
        try:
            state["wait4"] = os.wait4(process.pid, 0)
        except BaseException as error:  # communicated to the coordinator thread
            state["error"] = error

    waiter = threading.Thread(target=wait_exact_child, name="hostile-benchmark-wait4",
                              daemon=True)
    waiter.start()
    waiter.join(timeout=timeout_seconds)
    timed_out = waiter.is_alive()
    if timed_out:
        _signal_measured_process(process, True)
        waiter.join(timeout=5)
        if waiter.is_alive():
            _signal_measured_process(process, False)
            waiter.join(timeout=5)
    _require(not waiter.is_alive(), "child",
             "measured child did not exit after bounded termination")
    if "error" in state:
        raise BenchmarkError("child", f"wait4 failed for measured child: {state['error']}")
    waited_pid, status, usage = state["wait4"]
    _require(waited_pid == process.pid, "child", "wait4 returned the wrong child process")
    process.returncode = os.waitstatus_to_exitcode(status)
    return _process_cpu_time_from_rusage(usage), timed_out


def _wait_windows_child_cpu(process: subprocess.Popen[bytes],
                            timeout_seconds: float) -> tuple[dict[str, Any], bool]:
    timed_out = False
    try:
        process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        timed_out = True
        _signal_measured_process(process, True)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            _signal_measured_process(process, False)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired as error:
                raise BenchmarkError(
                    "child", "Windows measured child did not exit after bounded termination") \
                    from error
    return _windows_process_cpu_time(process), timed_out


def _wait_exact_child_cpu(process: subprocess.Popen[bytes],
                          timeout_seconds: float) -> tuple[dict[str, Any], bool]:
    if _process_cpu_time_source() == "windows_get_process_times_v1":
        return _wait_windows_child_cpu(process, timeout_seconds)
    return _wait_posix_child_cpu(process, timeout_seconds)


def _minimal_child_environment(temp_dir: str,
                               case_environment: Mapping[str, str]) -> dict[str, str]:
    environment = {name: os.environ[name] for name in _CHILD_ENV_ALLOWLIST
                   if name in os.environ}
    environment.update(case_environment)
    environment["TMPDIR"] = temp_dir
    if sys.platform == "win32":
        system_root = next((value for name, value in os.environ.items()
                            if name.lower() == "systemroot"), None)
        _require(bool(system_root), "child", "Windows child requires parent SystemRoot")
        environment["SystemRoot"] = str(system_root)
        environment["TEMP"] = temp_dir
        environment["TMP"] = temp_dir
    return environment


def _child_expected(case: Mapping[str, Any], label: str, repetition: int,
                    child_seed: int, fixture_hash_kind: str) -> dict[str, Any]:
    return {
        "fixture": case["fixture"],
        "fixture_sha256": _case_fixture_sha256(case, fixture_hash_kind),
        "fixture_hash_kind": fixture_hash_kind,
        "workload": case["workload"],
        "repetition": repetition,
        "variant": label,
        "rng_seed": child_seed,
        "updates": _case_expected_count(case, "CAOL_HOSTILE_BENCHMARK_UPDATES"),
        "clock_floor_samples": _case_expected_count(
            case, "CAOL_HOSTILE_BENCHMARK_CLOCK_FLOOR_SAMPLES"),
        "calendar": _case_calendar(case),
    }


def _warmup_result_binding(result: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "schema": result["schema"],
        "fixture": result["fixture"],
        "fixture_sha256": result.get("fixture_sha256"),
        "workload": result["workload"],
        "repetition": result["repetition"],
        "variant": result["variant"],
        "rng_seed": result["rng_seed"],
        "updates": result["updates"],
        "clock_floor_samples": result["clock_floor_samples"],
        "calendar": result["calendar"],
        "initial_state_sha256": result["deterministic_state"]["initial_sha256"],
        "terminal_state_sha256": result["deterministic_state"]["terminal_sha256"],
    }


def _drain_bounded_stream(stream: Any, process: subprocess.Popen[bytes],
                          state: dict[str, Any], byte_limit: int,
                          tail_bytes: int = 0) -> None:
    digest = hashlib.sha256()
    total_bytes = 0
    tail = bytearray()
    try:
        while True:
            chunk = stream.read(64 * 1024)
            if not chunk:
                break
            digest.update(chunk)
            total_bytes += len(chunk)
            if tail_bytes:
                tail.extend(chunk)
                if len(tail) > tail_bytes:
                    del tail[:-tail_bytes]
            if total_bytes > byte_limit and not state.get("exceeded", False):
                state["exceeded"] = True
                try:
                    process.terminate()
                except OSError:
                    pass
    finally:
        stream.close()
        state.update({"sha256": digest.hexdigest(), "bytes": total_bytes,
                      "tail": bytes(tail)})


def _read_bounded_child_result(path: pathlib.Path) -> bytes:
    _require(path.is_file(), "child", "child produced no benchmark output file")
    size = path.stat().st_size
    _require(0 < size <= _MAX_CHILD_RESULT_BYTES, "child",
             "child benchmark output file is empty or exceeds its byte cap",
             size_bytes=size, limit_bytes=_MAX_CHILD_RESULT_BYTES)
    return path.read_bytes()


def _run_warmup(binary: Mapping[str, Any], label: str, case: Mapping[str, Any],
                warmup_index: int, child_seed: int, common_arguments: Sequence[str],
                timeout_seconds: float, fixture_hash_kind: str = "opaque_sha256",
                isolated_user_dir_argument: str | None = None,
                source_data_root: Mapping[str, Any] | None = None) -> dict[str, Any]:
    """Run one untimed sacrificial data load without retaining child output text."""

    _require(isinstance(source_data_root, Mapping), "data_root",
             f"variant {label!r} needs an explicit source data-root identity")
    expected = _child_expected(case, label, 0, child_seed, fixture_hash_kind)
    with tempfile.TemporaryDirectory(prefix="caol-hostile-warmup-") as temp_dir:
        output_path = pathlib.Path(temp_dir) / "child-result.json"
        environment = _minimal_child_environment(temp_dir, case.get("env", {}))
        environment.update({
            "CAOL_HOSTILE_BENCHMARK_FIXTURE": case["fixture"],
            "CAOL_HOSTILE_BENCHMARK_FIXTURE_SHA256": _case_fixture_sha256(
                case, fixture_hash_kind),
            "CAOL_HOSTILE_BENCHMARK_FIXTURE_HASH_KIND": fixture_hash_kind,
            "CAOL_HOSTILE_BENCHMARK_WORKLOAD": case["workload"],
            "CAOL_HOSTILE_BENCHMARK_REPETITION": "0",
            "CAOL_HOSTILE_BENCHMARK_PAIR_INDEX": "0",
            "CAOL_HOSTILE_BENCHMARK_VARIANT": label,
            "CAOL_HOSTILE_BENCHMARK_SEED": str(child_seed),
            "CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN": str(expected["calendar"]["turn"]),
            "CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS": str(
                expected["calendar"]["season_length_days"]),
            "CAOL_HOSTILE_BENCHMARK_OUTPUT": str(output_path),
        })
        command = [binary["path"], "--rng-seed", str(child_seed), *common_arguments,
                   *case.get("arguments", [])]
        if isolated_user_dir_argument is not None:
            user_dir = pathlib.Path(temp_dir) / "user"
            user_dir.mkdir()
            command.extend((isolated_user_dir_argument, str(user_dir)))
        started_ns = time.perf_counter_ns()
        failure_code: str | None = None
        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       env=environment, cwd=source_data_root["path"])
        except OSError as error:
            raise BenchmarkError("warmup", f"could not launch warmup: {error}") from error
        _require(process.stdout is not None and process.stderr is not None, "warmup",
                 "warmup stream pipes were not created")
        stdout_state: dict[str, Any] = {"exceeded": False}
        stderr_state: dict[str, Any] = {"exceeded": False}
        stdout_thread = threading.Thread(
            target=_drain_bounded_stream,
            args=(process.stdout, process, stdout_state, _MAX_WARMUP_STREAM_BYTES, 0),
            name="hostile-warmup-stdout", daemon=True)
        stderr_thread = threading.Thread(
            target=_drain_bounded_stream,
            args=(process.stderr, process, stderr_state, _MAX_WARMUP_STREAM_BYTES,
                  _WARMUP_DIAGNOSTIC_TAIL_BYTES),
            name="hostile-warmup-stderr", daemon=True)
        stdout_thread.start()
        stderr_thread.start()
        try:
            process.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            failure_code = "timeout"
        stdout_thread.join(timeout=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)
        stderr_thread.join(timeout=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)
        _require(not stdout_thread.is_alive() and not stderr_thread.is_alive(), "warmup",
                 "warmup stream drain did not quiesce within its bounded timeout")
        if failure_code is None and (stdout_state["exceeded"] or stderr_state["exceeded"]):
            failure_code = "stream_limit"
        elapsed_ns = time.perf_counter_ns() - started_ns
        stdout_sha256 = stdout_state["sha256"]
        stdout_bytes = stdout_state["bytes"]
        stderr_sha256 = stderr_state["sha256"]
        stderr_bytes = stderr_state["bytes"]
        stderr_tail = stderr_state["tail"]
        raw = b""
        result_source = "output_file"
        if failure_code is None and process.returncode == 0:
            try:
                raw = _read_bounded_child_result(output_path)
            except BenchmarkError as error:
                failure_code = error.code
        binding: dict[str, Any] | None = None
        if failure_code is None and process.returncode == 0:
            try:
                binding = _warmup_result_binding(_decode_child_result(raw, expected))
            except BenchmarkError as error:
                failure_code = error.code
        elif failure_code is None:
            failure_code = "child"
        return {
            "status": "accepted" if failure_code is None else "failed",
            "warmup_index": warmup_index,
            "case_id": _case_id(case),
            "fixture_sha256": _case_fixture_sha256(case, fixture_hash_kind),
            "variant": label,
            "child_seed": child_seed,
            "binary_sha256": binary["sha256"],
            "working_directory": source_data_root["path"],
            "source_manifest_sha256": source_data_root["manifest_sha256"],
            "command": command,
            "exit_code": process.returncode,
            "runner_wall_time_ns": elapsed_ns,
            "stdout_sha256": stdout_sha256,
            "stdout_bytes": stdout_bytes,
            "stderr_sha256": stderr_sha256,
            "stderr_bytes": stderr_bytes,
            "diagnostic_stderr_tail": (stderr_tail.decode("utf-8", errors="replace")
                                       if failure_code is not None else ""),
            "child_result_source": result_source,
            "child_result_sha256": sha256_bytes(raw),
            "child_result_binding": binding,
            "failure_code": failure_code,
        }


def _decode_child_result(raw: bytes, expected: Mapping[str, Any]) -> dict[str, Any]:
    _require(bool(raw.strip()), "child", "child produced no benchmark JSON")
    try:
        document = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise BenchmarkError("child_schema", f"invalid child JSON: {error}") from error
    return validate_child_result(document, expected)


def _run_child(binary: Mapping[str, Any], label: str, case: Mapping[str, Any],
               pair_index: int, order_index: int, child_seed: int,
               common_arguments: Sequence[str], timeout_seconds: float,
               rss_interval_seconds: float, fixture_hash_kind: str = "opaque_sha256",
               isolated_user_dir_argument: str | None = None,
               data_root: Mapping[str, Any] | None = None) -> dict[str, Any]:
    _require(isinstance(data_root, Mapping), "data_root",
             f"variant {label!r} needs an explicit data root identity")
    expected = _child_expected(case, label, pair_index, child_seed, fixture_hash_kind)
    with tempfile.TemporaryDirectory(prefix="caol-hostile-benchmark-") as temp_dir:
        output_path = pathlib.Path(temp_dir) / "child-result.json"
        environment = _minimal_child_environment(temp_dir, case.get("env", {}))
        environment.update({
            "CAOL_HOSTILE_BENCHMARK_FIXTURE": case["fixture"],
            "CAOL_HOSTILE_BENCHMARK_FIXTURE_SHA256": _case_fixture_sha256(
                case, fixture_hash_kind),
            "CAOL_HOSTILE_BENCHMARK_FIXTURE_HASH_KIND": fixture_hash_kind,
            "CAOL_HOSTILE_BENCHMARK_WORKLOAD": case["workload"],
            "CAOL_HOSTILE_BENCHMARK_REPETITION": str(pair_index),
            "CAOL_HOSTILE_BENCHMARK_PAIR_INDEX": str(pair_index),
            "CAOL_HOSTILE_BENCHMARK_VARIANT": label,
            "CAOL_HOSTILE_BENCHMARK_SEED": str(child_seed),
            "CAOL_HOSTILE_BENCHMARK_CALENDAR_TURN": str(expected["calendar"]["turn"]),
            "CAOL_HOSTILE_BENCHMARK_SEASON_LENGTH_DAYS": str(
                expected["calendar"]["season_length_days"]),
            "CAOL_HOSTILE_BENCHMARK_OUTPUT": str(output_path),
        })
        command = [binary["path"], "--rng-seed", str(child_seed), *common_arguments,
                   *case.get("arguments", [])]
        if isolated_user_dir_argument is not None:
            user_dir = pathlib.Path(temp_dir) / "user"
            user_dir.mkdir()
            command.extend((isolated_user_dir_argument, str(user_dir)))
        started_ns = time.perf_counter_ns()
        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       env=environment, cwd=data_root["path"])
        except OSError as error:
            raise BenchmarkError("child", f"could not launch child: {error}") from error
        _require(process.stdout is not None and process.stderr is not None, "child",
                 "measured child stream pipes were not created")
        stdout_state: dict[str, Any] = {"exceeded": False}
        stderr_state: dict[str, Any] = {"exceeded": False}
        stdout_thread = threading.Thread(
            target=_drain_bounded_measured_stream,
            args=(process.stdout, process, stdout_state),
            name="hostile-benchmark-stdout", daemon=True)
        stderr_thread = threading.Thread(
            target=_drain_bounded_measured_stream,
            args=(process.stderr, process, stderr_state),
            name="hostile-benchmark-stderr", daemon=True)
        rss_samples: list[dict[str, int]] = []
        rss_state: dict[str, int | None] = {
            "observation_count": 0, "retention_stride": 1, "peak_bytes": None,
        }
        stop = threading.Event()
        sampler = threading.Thread(target=_sample_rss,
                                   args=(process.pid, stop, rss_interval_seconds, rss_samples,
                                         rss_state),
                                   name="hostile-benchmark-rss", daemon=True)
        stdout_thread.start()
        stderr_thread.start()
        sampler.start()
        wait_error: BenchmarkError | None = None
        process_cpu_time: dict[str, Any] | None = None
        timed_out = False
        try:
            process_cpu_time, timed_out = _wait_exact_child_cpu(process, timeout_seconds)
        except BenchmarkError as error:
            wait_error = error
        finally:
            _quiesce_rss_sampler(sampler, stop)
        stdout_thread.join(timeout=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)
        stderr_thread.join(timeout=_RSS_SAMPLER_JOIN_TIMEOUT_SECONDS)
        _require(not stdout_thread.is_alive() and not stderr_thread.is_alive(), "child",
                 "measured child stream drain did not quiesce within its bounded timeout")
        if wait_error is not None:
            raise wait_error
        _require(process_cpu_time is not None, "child",
                 "measured child CPU time was not captured")
        frozen_rss_samples = [dict(sample) for sample in rss_samples]
        frozen_rss_state = dict(rss_state)
        elapsed_ns = time.perf_counter_ns() - started_ns
        if timed_out:
            raise BenchmarkError("child", "child exceeded declared timeout",
                                 {"timeout_seconds": timeout_seconds})
        _require(not stdout_state["exceeded"] and not stderr_state["exceeded"], "child",
                 "measured child exceeded its stream byte cap",
                 limit_bytes=_MAX_MEASURED_STREAM_BYTES,
                 stdout_bytes=stdout_state["bytes"], stderr_bytes=stderr_state["bytes"],
                 stdout_sha256=stdout_state["sha256"], stderr_sha256=stderr_state["sha256"])
        stdout = stdout_state["content"]
        stderr = stderr_state["content"]
        _require(process.returncode == 0, "child", "child exited non-zero",
                 exit_code=process.returncode,
                 stderr_sha256=sha256_bytes(stderr),
                 stderr_tail=stderr[-4000:].decode("utf-8", errors="replace"))
        if output_path.is_file() and output_path.stat().st_size:
            raw = _read_bounded_child_result(output_path)
            result_source = "output_file"
        else:
            raw = stdout
            result_source = "stdout"
        result = _decode_child_result(raw, expected)
        return {
            "case_id": _case_id(case),
            "fixture_sha256": _case_fixture_sha256(case, fixture_hash_kind),
            "pair_index": pair_index,
            "order_index": order_index,
            "variant": label,
            "child_seed": child_seed,
            "binary_sha256": binary["sha256"],
            "working_directory": data_root["path"],
            "data_root_manifest_sha256": data_root["manifest_sha256"],
            "command": command,
            "exit_code": process.returncode,
            "runner_wall_time_ns": elapsed_ns,
            "process_cpu_time": process_cpu_time,
            "stream_byte_limit": _MAX_MEASURED_STREAM_BYTES,
            "stdout_bytes": stdout_state["bytes"],
            "stderr_bytes": stderr_state["bytes"],
            "rss_samples": frozen_rss_samples,
            "rss_observation_count": frozen_rss_state["observation_count"],
            "rss_sample_limit": _MAX_CHILD_RSS_SAMPLES,
            "rss_retention_stride": frozen_rss_state["retention_stride"],
            "rss_peak_bytes": frozen_rss_state["peak_bytes"],
            "stdout_sha256": sha256_bytes(stdout),
            "stderr_sha256": sha256_bytes(stderr),
            "child_result_source": result_source,
            "child_result_sha256": sha256_bytes(raw),
            "result": result,
        }


def _utc_now() -> str:
    return _datetime.datetime.now(_datetime.timezone.utc).isoformat()


def wrap_envelope(schema: str, payload: Mapping[str, Any]) -> dict[str, Any]:
    return {"schema": schema, "content_sha256": sha256_bytes(canonical_json_bytes(payload)),
            "payload": dict(payload)}


def _write_json(path: os.PathLike[str] | str, document: Any) -> None:
    destination = pathlib.Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    temporary.write_bytes(canonical_json_bytes(document))
    os.replace(temporary, destination)


def run_benchmarks(matrix_path: os.PathLike[str] | str,
                   binaries: Mapping[str, os.PathLike[str] | str], pair_count: int,
                   seed: int, common_arguments: Sequence[str] = (),
                   timeout_seconds: float = 300.0, rss_interval_seconds: float = 0.1,
                   data_roots: Mapping[str, os.PathLike[str] | str] | None = None,
                   build_detector: Callable[[], list[dict[str, Any]]] = find_concurrent_build_processes,
                   warmup_runner: Callable[..., dict[str, Any]] = _run_warmup,
                   child_runner: Callable[..., dict[str, Any]] = _run_child) -> dict[str, Any]:
    """Execute a complete serial benchmark packet and return its raw envelope."""

    _require(bool(binaries) and len(binaries) <= 2, "binary", "run needs one or two binaries")
    _require(pair_count > 0, "matrix_schema", "pair count must be positive")
    _require(timeout_seconds > 0, "matrix_schema", "timeout must be positive")
    _require(rss_interval_seconds > 0, "matrix_schema", "RSS interval must be positive")
    process_cpu_time_source = _process_cpu_time_source()
    _require(isinstance(data_roots, Mapping) and set(data_roots) == set(binaries),
             "data_root", "data roots must cover every binary label exactly",
             expected=sorted(binaries),
             actual=sorted(data_roots) if isinstance(data_roots, Mapping) else None)
    matrix, _matrix_raw, matrix_hash = load_matrix(matrix_path)
    identities = {label: _binary_identity(path) for label, path in binaries.items()}
    source_data_root_identities = {
        label: _data_source_identity(data_roots[label]) for label in binaries
    }
    pre_warm_data_root_identities = {
        label: _data_root_identity(data_roots[label]) for label in binaries
    }
    _require(len(identities) == len(set(identities)), "binary", "binary labels must be distinct")
    labels = list(identities)
    _require(len({identity["path"] for identity in
                  pre_warm_data_root_identities.values()}) == len(labels),
             "data_root", "each benchmark variant needs its own declared data root")
    orders = paired_orders(seed, pair_count, labels)
    total_update_observations = sum(
        _case_expected_count(case, "CAOL_HOSTILE_BENCHMARK_UPDATES")
        for case in matrix["cases"] ) * len(identities) * pair_count
    _require(total_update_observations <= _MAX_PACKET_UPDATE_OBSERVATIONS,
             "matrix_schema", "matrix exceeds the raw update-observation packet cap",
             expected_max=_MAX_PACKET_UPDATE_OBSERVATIONS,
             actual=total_update_observations)
    maximum_retained_rss_samples = (len(matrix["cases"]) * len(identities) * pair_count *
                                    _MAX_CHILD_RSS_SAMPLES)
    _require(maximum_retained_rss_samples <= _MAX_PACKET_RSS_SAMPLES,
             "matrix_schema", "matrix exceeds the retained RSS-sample packet cap",
             expected_max=_MAX_PACKET_RSS_SAMPLES, actual=maximum_retained_rss_samples)
    payload: dict[str, Any] = {
        "status": "accepted",
        "created_utc": _utc_now(),
        "seed": seed,
        "pair_count": pair_count,
        "common_arguments": list(common_arguments),
        "label_order": labels,
        "pair_orders": [list(order) for order in orders],
        "update_observation_limit": _MAX_PACKET_UPDATE_OBSERVATIONS,
        "expected_update_observations": total_update_observations,
        "rss_sample_limit_per_child": _MAX_CHILD_RSS_SAMPLES,
        "rss_sample_limit_per_packet": _MAX_PACKET_RSS_SAMPLES,
        "expected_max_retained_rss_samples": maximum_retained_rss_samples,
        "process_cpu_time_source": process_cpu_time_source,
        "matrix_path": str(pathlib.Path(matrix_path).expanduser().resolve()),
        "matrix_sha256": matrix_hash,
        "matrix": matrix,
        "binaries": identities,
        "source_data_roots": source_data_root_identities,
        "pre_warm_data_roots": pre_warm_data_root_identities,
        "data_roots": pre_warm_data_root_identities,
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "python_platform": sys.platform,
            "os_name": os.name,
        },
        "warmups": [],
        "runs": [],
        "failures": [],
    }
    try:
        source_manifests = {
            (identity["manifest_sha256"], identity["file_count"], identity["total_bytes"])
            for identity in source_data_root_identities.values()
        }
        _require(len(source_manifests) == 1, "hash",
                 "paired variants do not have identical non-cache source data",
                 manifests=sorted(source_manifests))
        for label, pre_warm_identity in pre_warm_data_root_identities.items():
            source_identity = source_data_root_identities[label]
            _require(
                (pre_warm_identity["manifest_sha256"], pre_warm_identity["file_count"],
                 pre_warm_identity["total_bytes"]) ==
                (source_identity["manifest_sha256"], source_identity["file_count"],
                 source_identity["total_bytes"]),
                "warmup", f"variant {label!r} must start with an empty runtime data cache",
                variant=label,
                pre_warm_manifest_sha256=pre_warm_identity["manifest_sha256"],
                source_manifest_sha256=source_identity["manifest_sha256"],
                pre_warm_file_count=pre_warm_identity["file_count"],
                source_file_count=source_identity["file_count"],
            )
        warmup_case = matrix["cases"][0]
        warmup_fixture_hash_kind = _matrix_fixture_hash_kind(matrix, warmup_case)
        for warmup_index, label in enumerate(labels):
            active_builds = build_detector()
            _require(not active_builds, "concurrent_build",
                     "compiler/build process is active", processes=active_builds)
            _require(sha256_bytes(pathlib.Path(matrix_path).read_bytes()) == matrix_hash,
                     "hash", "matrix changed before warmup")
            identity = identities[label]
            _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                     f"binary {label!r} changed before warmup")
            child_seed = _warmup_seed(seed)
            warmup = warmup_runner(identity, label, warmup_case, warmup_index, child_seed,
                                   common_arguments, timeout_seconds,
                                   warmup_fixture_hash_kind,
                                   matrix.get("isolated_user_dir_argument"),
                                   source_data_root_identities[label])
            payload["warmups"].append(warmup)
            _require(isinstance(warmup, dict) and warmup.get("variant") == label, "schema",
                     "warmup variant does not match stable label order")
            _validate_warmup_record(
                warmup, identity, source_data_root_identities[label], warmup_case,
                warmup_fixture_hash_kind, seed, warmup_index, common_arguments,
                matrix.get("isolated_user_dir_argument"))
            _require(warmup["status"] == "accepted", "warmup",
                     f"sacrificial data-load warmup failed for variant {label!r}",
                     variant=label, exit_code=warmup["exit_code"],
                     failure_code=warmup["failure_code"],
                     stdout_sha256=warmup["stdout_sha256"],
                     stderr_sha256=warmup["stderr_sha256"],
                     diagnostic_stderr_tail=warmup["diagnostic_stderr_tail"])
            _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                     f"binary {label!r} changed during warmup")
            _require(_data_source_identity(source_data_root_identities[label]["path"]) ==
                     source_data_root_identities[label], "hash",
                     f"source data root {label!r} changed during warmup")
        _require(sha256_bytes(pathlib.Path(matrix_path).read_bytes()) == matrix_hash, "hash",
                 "matrix changed during warmup")
        data_root_identities = {
            label: _data_root_identity(source_data_root_identities[label]["path"])
            for label in labels
        }
        payload["data_roots"] = data_root_identities
        for label, warmed_identity in data_root_identities.items():
            _require_warmed_data_root_transition(
                label, pre_warm_data_root_identities[label],
                source_data_root_identities[label], warmed_identity)
        for pair_index, order in enumerate(orders):
            for case_index, case in enumerate(matrix["cases"]):
                for order_index, label in enumerate(order):
                    active_builds = build_detector()
                    _require(not active_builds, "concurrent_build",
                             "compiler/build process is active", processes=active_builds)
                    _require(sha256_bytes(pathlib.Path(matrix_path).read_bytes()) == matrix_hash,
                             "hash", "matrix changed during the run")
                    identity = identities[label]
                    _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                             f"binary {label!r} changed during the run")
                    child_seed = _derived_seed(seed, case_index, pair_index)
                    run = child_runner(identity, label, case, pair_index, order_index,
                                       child_seed, common_arguments, timeout_seconds,
                                       rss_interval_seconds,
                                       _matrix_fixture_hash_kind(matrix, case),
                                       matrix.get("isolated_user_dir_argument"),
                                       data_root_identities[label])
                    payload["runs"].append(run)
                    _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                             f"binary {label!r} changed during child execution")
        _require(sha256_bytes(pathlib.Path(matrix_path).read_bytes()) == matrix_hash, "hash",
                 "matrix changed during the run")
        for label, identity in identities.items():
            _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                     f"binary {label!r} changed during the run")
        for label, identity in source_data_root_identities.items():
            _require(_data_source_identity(identity["path"]) == identity, "hash",
                     f"source data root {label!r} changed during the run")
        for label, identity in data_root_identities.items():
            _require(_data_root_identity(identity["path"]) == identity, "hash",
                     f"data root {label!r} changed during the run")
    except BenchmarkError as error:
        payload["status"] = "rejected"
        payload["failures"].append(error.as_dict())
    document = wrap_envelope(RAW_SCHEMA, payload)
    if payload["status"] == "accepted":
        try:
            validate_raw(document)
        except BenchmarkError as error:
            payload["status"] = "rejected"
            payload["failures"].append(error.as_dict())
            document = wrap_envelope(RAW_SCHEMA, payload)
    return document


def _verify_envelope(document: Any, schema: str) -> dict[str, Any]:
    _require(isinstance(document, dict), "schema", "artifact must be an object")
    _require(document.get("schema") == schema, "schema", f"artifact schema must be {schema}")
    payload = document.get("payload")
    _require(isinstance(payload, dict), "schema", "artifact payload must be an object")
    expected_hash = sha256_bytes(canonical_json_bytes(payload))
    _require(document.get("content_sha256") == expected_hash, "hash",
             "artifact payload hash mismatch", expected=expected_hash,
             actual=document.get("content_sha256"))
    return payload


def _validate_data_root_identity(identity: Any, verify_files: bool) -> dict[str, Any]:
    required = {"path", "data_path", "manifest_kind", "manifest_sha256", "file_count",
                "total_bytes"}
    _require(isinstance(identity, dict) and set(identity) == required, "schema",
             "invalid data-root identity")
    _require(isinstance(identity["path"], str) and pathlib.Path(identity["path"]).is_absolute(),
             "schema", "data-root path must be absolute")
    _require(identity["data_path"] == str(pathlib.Path(identity["path"]) / "data"),
             "schema", "data-root data path does not match its working directory")
    _require(identity["manifest_kind"] == "recursive_file_content_sha256_v1", "schema",
             "unsupported data-root manifest kind")
    _require(_valid_sha256(identity["manifest_sha256"]), "schema",
             "invalid data-root manifest hash")
    _require(_is_nonnegative_int(identity["file_count"]) and
             _is_nonnegative_int(identity["total_bytes"]), "schema",
             "invalid data-root manifest counts")
    if verify_files:
        _require(_data_root_identity(identity["path"]) == identity, "hash",
                 "data-root content hash changed", path=identity["path"])
    return identity


def _validate_source_data_root_identity(identity: Any, verify_files: bool) -> dict[str, Any]:
    required = {"path", "data_path", "manifest_kind", "manifest_sha256", "file_count",
                "total_bytes"}
    _require(isinstance(identity, dict) and set(identity) == required, "schema",
             "invalid source data-root identity")
    _require(isinstance(identity["path"], str) and pathlib.Path(identity["path"]).is_absolute(),
             "schema", "source data-root path must be absolute")
    _require(identity["data_path"] == str(pathlib.Path(identity["path"]) / "data"),
             "schema", "source data-root data path does not match its working directory")
    _require(identity["manifest_kind"] ==
             "recursive_source_file_content_sha256_excluding_cache_v1", "schema",
             "unsupported source data-root manifest kind")
    _require(_valid_sha256(identity["manifest_sha256"]), "schema",
             "invalid source data-root manifest hash")
    _require(_is_nonnegative_int(identity["file_count"]) and
             _is_nonnegative_int(identity["total_bytes"]), "schema",
             "invalid source data-root manifest counts")
    if verify_files:
        _require(_data_source_identity(identity["path"]) == identity, "hash",
                 "source data-root content hash changed", path=identity["path"])
    return identity


def _validate_warmup_result_binding(binding: Any, expected: Mapping[str, Any]) -> None:
    required = {
        "schema", "fixture", "fixture_sha256", "workload", "repetition", "variant",
        "rng_seed", "updates", "clock_floor_samples", "calendar", "initial_state_sha256",
        "terminal_state_sha256",
    }
    _require(isinstance(binding, dict) and set(binding) == required, "schema",
             "invalid warmup child-result binding")
    _require(binding["schema"] == CHILD_SCHEMA, "schema", "warmup child schema mismatch")
    for name in ("fixture", "workload", "variant"):
        _require(binding[name] == expected[name], "hash",
                 f"warmup child {name} binding mismatch")
    _require(str(binding["repetition"]) == str(expected["repetition"]), "hash",
             "warmup child repetition binding mismatch")
    for name in ("rng_seed", "updates", "clock_floor_samples"):
        _require(binding[name] == expected[name], "hash",
                 f"warmup child {name} binding mismatch")
    _require(binding["fixture_sha256"] in (None, expected["fixture_sha256"]), "hash",
             "warmup child fixture hash binding mismatch")
    _validate_calendar(binding["calendar"], expected["calendar"])
    for name in ("initial_state_sha256", "terminal_state_sha256"):
        _require(_valid_sha256(binding[name]), "schema",
                 f"invalid warmup child {name}")
    if expected["fixture_hash_kind"] == "serialized_state_sha256":
        _require(binding["initial_state_sha256"] == expected["fixture_sha256"], "hash",
                 "warmup serialized fixture binding mismatch")


def _validate_warmup_record(record: Any, binary: Mapping[str, Any],
                            source_data_root: Mapping[str, Any], case: Mapping[str, Any],
                            fixture_hash_kind: str, orchestration_seed: int,
                            warmup_index: int, common_arguments: Sequence[str],
                            isolated_user_dir_argument: str | None) -> None:
    required = {
        "status", "warmup_index", "case_id", "fixture_sha256", "variant", "child_seed",
        "binary_sha256", "working_directory", "source_manifest_sha256", "command",
        "exit_code", "runner_wall_time_ns", "stdout_sha256", "stdout_bytes",
        "stderr_sha256", "stderr_bytes", "diagnostic_stderr_tail",
        "child_result_source", "child_result_sha256", "child_result_binding",
        "failure_code",
    }
    _require(isinstance(record, dict) and set(record) == required, "schema",
             "invalid warmup record")
    label = record["variant"]
    expected_seed = _warmup_seed(orchestration_seed)
    expected = _child_expected(case, label, 0, expected_seed, fixture_hash_kind)
    _require(record["status"] in ("accepted", "failed"), "schema",
             "invalid warmup status")
    _require(record["warmup_index"] == warmup_index, "schema",
             "warmup index mismatch")
    _require(record["case_id"] == _case_id(case), "hash", "warmup case binding mismatch")
    _require(record["fixture_sha256"] == expected["fixture_sha256"], "hash",
             "warmup fixture hash mismatch")
    _require(record["child_seed"] == expected_seed, "hash", "warmup seed mismatch")
    _require(record["binary_sha256"] == binary["sha256"], "hash",
             "warmup binary hash mismatch")
    _require(record["working_directory"] == source_data_root["path"], "hash",
             "warmup working directory mismatch")
    _require(record["source_manifest_sha256"] == source_data_root["manifest_sha256"], "hash",
             "warmup source manifest mismatch")
    command = record["command"]
    command_prefix = [binary["path"], "--rng-seed", str(expected_seed), *common_arguments,
                      *case.get("arguments", [])]
    _require(isinstance(command, list) and all(isinstance(value, str) for value in command),
             "schema", "warmup command must be a string list")
    if isolated_user_dir_argument is None:
        _require(command == command_prefix, "hash", "warmup command binding mismatch")
    else:
        _require(len(command) == len(command_prefix) + 2 and
                 command[:-2] == command_prefix and command[-2] == isolated_user_dir_argument and
                 pathlib.Path(command[-1]).is_absolute(), "hash",
                 "warmup isolated user-directory command binding mismatch")
    _require(isinstance(record["exit_code"], int) and
             not isinstance(record["exit_code"], bool), "schema", "invalid warmup exit code")
    _require(_is_nonnegative_int(record["runner_wall_time_ns"]), "schema",
             "invalid warmup runner wall time")
    for name in ("stdout_sha256", "stderr_sha256", "child_result_sha256"):
        _require(_valid_sha256(record[name]), "schema", f"invalid warmup {name}")
    for name in ("stdout_bytes", "stderr_bytes"):
        _require(_is_nonnegative_int(record[name]), "schema", f"invalid warmup {name}")
    _require(isinstance(record["diagnostic_stderr_tail"], str) and
             len(record["diagnostic_stderr_tail"]) <= _WARMUP_DIAGNOSTIC_TAIL_BYTES,
             "schema", "warmup diagnostic stderr tail exceeds its character cap")
    _require(record["child_result_source"] == "output_file", "schema",
             "warmup child result must use its bounded output file")
    if record["status"] == "accepted":
        _require(record["exit_code"] == 0 and record["failure_code"] is None, "schema",
                 "accepted warmup has failure state")
        _require(record["stdout_bytes"] <= _MAX_WARMUP_STREAM_BYTES and
                 record["stderr_bytes"] <= _MAX_WARMUP_STREAM_BYTES, "schema",
                 "accepted warmup exceeded its stream byte cap")
        _require(record["diagnostic_stderr_tail"] == "", "schema",
                 "accepted warmup must not retain stderr text")
        _validate_warmup_result_binding(record["child_result_binding"], expected)
    else:
        _require(isinstance(record["failure_code"], str) and record["failure_code"], "schema",
                 "failed warmup needs a failure code")
        _require(record["child_result_binding"] is None, "schema",
                 "failed warmup must not retain a child-result binding")


def validate_raw(document: Any, verify_files: bool = False) -> dict[str, Any]:
    """Validate a raw packet's envelope, identities, order, and child results."""

    payload = _verify_envelope(document, RAW_SCHEMA)
    _require(payload.get("status") in ("accepted", "rejected"), "schema", "invalid raw status")
    matrix = validate_matrix(payload.get("matrix"))
    _require(_valid_sha256(payload.get("matrix_sha256")), "schema", "invalid matrix hash")
    binaries = payload.get("binaries")
    _require(isinstance(binaries, dict) and 1 <= len(binaries) <= 2, "schema",
             "invalid raw binary identities")
    for label, identity in binaries.items():
        _require(isinstance(label, str) and isinstance(identity, dict), "schema",
                 "invalid binary identity")
        _require(_valid_sha256(identity.get("sha256")), "schema", "invalid binary hash")
        _require(_is_nonnegative_int(identity.get("size_bytes")), "schema", "invalid binary size")
        if verify_files:
            _require(pathlib.Path(identity.get("path", "")).is_file(), "binary",
                     f"binary missing: {identity.get('path')}")
            _require(sha256_file(identity["path"]) == identity["sha256"], "hash",
                     f"binary hash changed: {label}")
    pre_warm_data_roots = payload.get("pre_warm_data_roots")
    _require(isinstance(pre_warm_data_roots, dict) and
             set(pre_warm_data_roots) == set(binaries), "schema",
             "pre-warm data-root identities must cover every binary label exactly")
    for identity in pre_warm_data_roots.values():
        _validate_data_root_identity(identity, False)
    data_roots = payload.get("data_roots")
    _require(isinstance(data_roots, dict) and set(data_roots) == set(binaries), "schema",
             "data-root identities must cover every binary label exactly")
    for identity in data_roots.values():
        _validate_data_root_identity(identity, verify_files)
    _require(len({identity["path"] for identity in data_roots.values()}) == len(binaries),
             "schema", "benchmark variants must not share a data root")
    source_data_roots = payload.get("source_data_roots")
    _require(isinstance(source_data_roots, dict) and
             set(source_data_roots) == set(binaries), "schema",
             "source data-root identities must cover every binary label exactly")
    for label, identity in source_data_roots.items():
        _validate_source_data_root_identity(identity, verify_files)
        _require(identity["path"] == data_roots[label]["path"], "hash",
                 "source and warmed data-root paths differ")
        _require(identity["path"] == pre_warm_data_roots[label]["path"], "hash",
                 "source and pre-warm data-root paths differ")
    pair_count = payload.get("pair_count")
    _require(_is_nonnegative_int(pair_count) and pair_count > 0, "schema", "invalid pair count")
    _require(isinstance(payload.get("seed"), int) and not isinstance(payload["seed"], bool),
             "schema", "invalid orchestration seed")
    common_arguments = payload.get("common_arguments")
    _require(isinstance(common_arguments, list) and
             all(isinstance(value, str) for value in common_arguments), "schema",
             "invalid common argument list")
    expected_update_observations = sum(
        _case_expected_count(case, "CAOL_HOSTILE_BENCHMARK_UPDATES")
        for case in matrix["cases"] ) * len(binaries) * pair_count
    _require(payload.get("update_observation_limit") == _MAX_PACKET_UPDATE_OBSERVATIONS,
             "schema", "raw packet update-observation limit mismatch")
    _require(payload.get("expected_update_observations") == expected_update_observations,
             "schema", "raw packet expected update-observation count mismatch")
    _require(expected_update_observations <= _MAX_PACKET_UPDATE_OBSERVATIONS,
             "schema", "raw packet exceeds update-observation cap")
    expected_max_retained_rss_samples = (len(matrix["cases"]) * len(binaries) * pair_count *
                                         _MAX_CHILD_RSS_SAMPLES)
    _require(payload.get("rss_sample_limit_per_child") == _MAX_CHILD_RSS_SAMPLES,
             "schema", "raw packet per-child RSS sample limit mismatch")
    _require(payload.get("rss_sample_limit_per_packet") == _MAX_PACKET_RSS_SAMPLES,
             "schema", "raw packet RSS sample packet limit mismatch")
    _require(payload.get("expected_max_retained_rss_samples") ==
             expected_max_retained_rss_samples,
             "schema", "raw packet expected retained RSS sample count mismatch")
    _require(expected_max_retained_rss_samples <= _MAX_PACKET_RSS_SAMPLES,
             "schema", "raw packet exceeds retained RSS-sample cap")
    process_cpu_time_source = payload.get("process_cpu_time_source")
    _require(process_cpu_time_source in _PROCESS_CPU_TIME_SOURCES, "schema",
             "invalid process CPU time source")
    host = payload.get("host")
    _require(isinstance(host, dict), "schema", "raw packet host identity is invalid")
    _require(isinstance(host.get("python_platform"), str) and host["python_platform"] and
             isinstance(host.get("os_name"), str) and host["os_name"], "schema",
             "raw packet host platform identity is incomplete")
    if process_cpu_time_source == "windows_get_process_times_v1":
        _require(host.get("python_platform") == "win32" and host.get("os_name") == "nt",
                 "schema", "Windows process CPU source disagrees with host identity")
    else:
        _require(host.get("python_platform") != "win32" and host.get("os_name") == "posix",
                 "schema", "POSIX process CPU source disagrees with host identity")
    labels = payload.get("label_order")
    _require(isinstance(labels, list) and len(labels) == len(binaries) and
             all(isinstance(label, str) for label in labels) and len(set(labels)) == len(labels) and
             set(labels) == set(binaries), "schema", "invalid stable label order")
    orders = payload.get("pair_orders")
    expected_orders = [list(order) for order in paired_orders(payload["seed"], pair_count, labels)]
    _require(orders == expected_orders, "schema",
             "pair orders do not match the recorded seed and stable label order",
             expected=expected_orders, actual=orders)
    warmups = payload.get("warmups")
    _require(isinstance(warmups, list) and len(warmups) <= len(labels), "schema",
             "invalid warmup record count")
    warmup_case = matrix["cases"][0]
    warmup_fixture_hash_kind = _matrix_fixture_hash_kind(matrix, warmup_case)
    for warmup_index, warmup in enumerate(warmups):
        label = labels[warmup_index]
        _require(isinstance(warmup, dict) and warmup.get("variant") == label, "schema",
                 "warmup variants must form the stable label-order prefix")
        _validate_warmup_record(
            warmup, binaries[label], source_data_roots[label], warmup_case,
            warmup_fixture_hash_kind, payload["seed"], warmup_index, common_arguments,
            matrix.get("isolated_user_dir_argument"))
    runs = payload.get("runs")
    _require(isinstance(runs, list), "schema", "runs must be a list")
    case_by_id = {_case_id(case): case for case in matrix["cases"]}
    case_index_by_id = {_case_id(case): index for index, case in enumerate(matrix["cases"])}
    seen: set[tuple[int, str, str]] = set()
    for run in runs:
        _require(isinstance(run, dict), "schema", "run must be an object")
        case = case_by_id.get(run.get("case_id"))
        _require(case is not None, "schema", "run refers to unknown case")
        pair_index = run.get("pair_index")
        order_index = run.get("order_index")
        label = run.get("variant")
        _require(_is_nonnegative_int(pair_index) and pair_index < pair_count, "schema",
                 "run pair index out of range")
        _require(_is_nonnegative_int(order_index) and order_index < len(orders[pair_index]),
                 "schema", "run order index out of range")
        _require(label in binaries, "schema", "run variant is unknown")
        _require(orders[pair_index][order_index] == label, "schema",
                 "run variant does not match declared pair order",
                 expected=orders[pair_index][order_index], actual=label)
        key = (pair_index, _case_id(case), label)
        _require(key not in seen, "schema", "duplicate run identity")
        seen.add(key)
        _require(run.get("binary_sha256") == binaries[label]["sha256"], "hash",
                 "run binary hash mismatch")
        _require(run.get("working_directory") == data_roots[label]["path"], "hash",
                 "run working directory does not match its variant data root")
        _require(run.get("data_root_manifest_sha256") ==
                 data_roots[label]["manifest_sha256"], "hash",
                 "run data-root manifest hash mismatch")
        derived_child_seed = _derived_seed(payload["seed"], case_index_by_id[_case_id(case)],
                                           pair_index)
        _require(run.get("child_seed") == derived_child_seed, "hash",
                 "run child seed does not match the recorded orchestration seed",
                 expected=derived_child_seed, actual=run.get("child_seed"))
        expected = _child_expected(case, label, pair_index, run.get("child_seed"),
                                   _matrix_fixture_hash_kind(matrix, case))
        _require(run.get("fixture_sha256") == expected["fixture_sha256"], "hash",
                 "run fixture hash mismatch", expected=expected["fixture_sha256"],
                 actual=run.get("fixture_sha256"))
        command = run.get("command")
        command_prefix = [binaries[label]["path"], "--rng-seed",
                          str(derived_child_seed), *common_arguments,
                          *case.get("arguments", [])]
        _require(isinstance(command, list) and
                 all(isinstance(value, str) for value in command), "schema",
                 "run command must be a string list")
        isolated_user_dir_argument = matrix.get("isolated_user_dir_argument")
        if isolated_user_dir_argument is None:
            _require(command == command_prefix, "hash", "run command binding mismatch")
        else:
            _require(len(command) == len(command_prefix) + 2 and
                     command[:-2] == command_prefix and
                     command[-2] == isolated_user_dir_argument and
                     pathlib.Path(command[-1]).is_absolute(), "hash",
                     "run isolated user-directory command binding mismatch")
        _require(isinstance(run.get("exit_code"), int) and
                 not isinstance(run.get("exit_code"), bool), "schema",
                 "recorded run exit code must be an integer")
        _require(run["exit_code"] == 0, "child", "recorded run exited non-zero")
        for stream_name in ("stdout_sha256", "stderr_sha256", "child_result_sha256"):
            _require(_valid_sha256(run.get(stream_name)), "schema",
                     f"invalid run {stream_name}")
        _require(run.get("child_result_source") in ("output_file", "stdout"), "schema",
                 "invalid run child-result source")
        validate_child_result(run.get("result"), expected)
        _require(_is_nonnegative_int(run.get("runner_wall_time_ns")), "schema",
                 "invalid runner wall time")
        process_cpu_time = run.get("process_cpu_time")
        required_cpu_fields = {"source", "user_ns", "system_ns", "total_ns"}
        _require(isinstance(process_cpu_time, dict) and
                 set(process_cpu_time) == required_cpu_fields, "schema",
                 "invalid measured child process CPU record")
        _require(process_cpu_time["source"] == process_cpu_time_source, "schema",
                 "run process CPU source does not match packet provenance")
        for cpu_field in ("user_ns", "system_ns", "total_ns"):
            _require(_is_nonnegative_int(process_cpu_time[cpu_field]), "schema",
                     f"invalid measured child process CPU {cpu_field}")
        _require(process_cpu_time["total_ns"] ==
                 process_cpu_time["user_ns"] + process_cpu_time["system_ns"], "schema",
                 "measured child process CPU total does not match user plus system")
        _require(run.get("stream_byte_limit") == _MAX_MEASURED_STREAM_BYTES, "schema",
                 "measured child stream byte limit mismatch")
        for stream_bytes_name in ("stdout_bytes", "stderr_bytes"):
            _require(_is_nonnegative_int(run.get(stream_bytes_name)) and
                     run[stream_bytes_name] <= _MAX_MEASURED_STREAM_BYTES, "schema",
                     f"invalid measured child {stream_bytes_name}")
        samples = run.get("rss_samples")
        _require(isinstance(samples, list), "schema", "RSS samples must be a list")
        observation_count = run.get("rss_observation_count")
        retention_stride = run.get("rss_retention_stride")
        peak_bytes = run.get("rss_peak_bytes")
        _require(run.get("rss_sample_limit") == _MAX_CHILD_RSS_SAMPLES, "schema",
                 "run RSS sample limit mismatch")
        _require(_is_nonnegative_int(observation_count), "schema",
                 "run RSS observation count is invalid")
        _require(_is_nonnegative_int(retention_stride) and retention_stride > 0 and
                 retention_stride & (retention_stride - 1) == 0, "schema",
                 "run RSS retention stride must be a positive power of two")
        _require(len(samples) <= _MAX_CHILD_RSS_SAMPLES and len(samples) <= observation_count,
                 "schema", "run retained too many RSS samples")
        expected_retained = ((observation_count - 1) // retention_stride + 1
                             if observation_count else 0)
        _require(len(samples) == expected_retained, "schema",
                 "run RSS retained count does not match systematic stride",
                 expected=expected_retained, actual=len(samples))
        if observation_count <= _MAX_CHILD_RSS_SAMPLES:
            _require(len(samples) == observation_count and retention_stride == 1, "schema",
                     "uncapped RSS observations must all be retained")
        else:
            _require(retention_stride > 1, "schema",
                     "capped RSS observations must declare downsampling")
        _require((observation_count == 0 and peak_bytes is None) or
                 observation_count > 0 and _is_nonnegative_int(peak_bytes), "schema",
                 "run RSS peak does not match its observation count")
        previous_timestamp = -1
        for sample in samples:
            _require(isinstance(sample, dict) and _is_nonnegative_int(sample.get("monotonic_ns"))
                     and _is_nonnegative_int(sample.get("rss_bytes")), "schema",
                     "invalid RSS sample")
            _require(sample["monotonic_ns"] >= previous_timestamp, "schema",
                     "RSS sample timestamps must be monotonic")
            previous_timestamp = sample["monotonic_ns"]
            _require(peak_bytes is not None and sample["rss_bytes"] <= peak_bytes, "schema",
                     "retained RSS sample exceeds recorded peak")
    if payload["status"] == "accepted":
        source_manifests = {
            (identity["manifest_sha256"], identity["file_count"], identity["total_bytes"])
            for identity in source_data_roots.values()
        }
        _require(len(source_manifests) == 1, "hash",
                 "paired variants do not have identical non-cache source data",
                 manifests=sorted(source_manifests))
        for label, pre_warm_identity in pre_warm_data_roots.items():
            source_identity = source_data_roots[label]
            _require(
                (pre_warm_identity["manifest_sha256"], pre_warm_identity["file_count"],
                 pre_warm_identity["total_bytes"]) ==
                (source_identity["manifest_sha256"], source_identity["file_count"],
                 source_identity["total_bytes"]),
                "warmup", "accepted packet did not start from an empty runtime data cache",
                variant=label,
            )
            _require_warmed_data_root_transition(
                label, pre_warm_identity, source_identity, data_roots[label])
        _require(len(warmups) == len(labels) and
                 all(warmup["status"] == "accepted" for warmup in warmups), "schema",
                 "accepted packet needs one accepted warmup per variant")
        expected_runs = pair_count * len(matrix["cases"]) * len(binaries)
        _require(len(runs) == expected_runs, "schema", "accepted packet is incomplete",
                 expected=expected_runs, actual=len(runs))
        _require(not payload.get("failures"), "schema", "accepted packet contains failures")
        for case in matrix["cases"]:
            case_runs = [run for run in runs if run["case_id"] == _case_id(case)]
            initial_hashes = {
                run["result"]["deterministic_state"]["initial_sha256"]
                for run in case_runs
            }
            _require(len(initial_hashes) == 1, "hash",
                     "deterministic fixture initial states diverged across runs",
                     case_id=_case_id(case), hashes=sorted(initial_hashes))
            if matrix.get("require_equivalent_terminal_state", False):
                terminal_hashes = {
                    run["result"]["deterministic_state"]["terminal_sha256"]
                    for run in case_runs
                }
                _require(len(terminal_hashes) == 1, "hash",
                         "deterministic terminal states diverged across variants or repetitions",
                         case_id=_case_id(case), hashes=sorted(terminal_hashes))
    else:
        _require(isinstance(payload.get("failures"), list) and payload["failures"], "schema",
                 "rejected packet needs a failure")
    if verify_files:
        matrix_path = pathlib.Path(payload.get("matrix_path", ""))
        _require(matrix_path.is_file(), "matrix_schema", "matrix file is missing")
        _require(sha256_file(matrix_path) == payload["matrix_sha256"], "hash",
                 "matrix file hash changed")
    return payload


def _flatten_metrics(value: Any, prefix: str = "") -> Iterable[tuple[str, Any]]:
    if isinstance(value, dict):
        for key in sorted(value):
            nested = f"{prefix}.{key}" if prefix else key
            yield from _flatten_metrics(value[key], nested)
    elif isinstance(value, list) and value and all(isinstance(item, dict) for item in value):
        for item in value:
            yield from _flatten_metrics(item, f"{prefix}[]")
    else:
        yield prefix, value


def _result_measurements(result: Mapping[str, Any]) -> Iterable[tuple[str, Any]]:
    """Expose child metrics plus probe/serialization/fairness/memory evidence."""

    yield from _flatten_metrics(result["metrics"])
    for section in ("probe", "serialization", "fairness"):
        if section in result:
            yield from _flatten_metrics(result[section], section)
    process_memory = result.get("process_memory")
    if isinstance(process_memory, Mapping):
        yield ("process_memory.maximum_sampled_resident_bytes",
               process_memory.get("maximum_sampled_resident_bytes"))
        for sample in process_memory.get("samples", []):
            if isinstance(sample, Mapping) and isinstance(sample.get("phase"), str):
                phase = sample["phase"]
                yield f"process_memory.{phase}.resident_bytes", sample.get("resident_bytes")
                yield (f"process_memory.{phase}.delta_from_previous_bytes",
                       sample.get("delta_from_previous_bytes"))


def _stats(values: Sequence[float | int], seed: int, bootstrap_samples: int,
           kind: str) -> dict[str, Any]:
    if not values:
        return {"kind": kind, "available": False, "count": 0,
                "confidence_status": "insufficient_sample", "mean_ci95": None}
    numeric = [float(value) for value in values]
    confidence_eligible = kind != "run_scalar" or len(numeric) >= _PAIRED_CI_MINIMUM_PAIRS
    return {
        "kind": kind,
        "available": True,
        "count": len(numeric),
        "mean": sum(numeric) / len(numeric),
        "confidence_status": ("available" if confidence_eligible else
                              "insufficient_sample"),
        "mean_ci95": (bootstrap_mean_ci(numeric, seed, bootstrap_samples)
                      if confidence_eligible else None),
        "min": min(numeric),
        "p50": nearest_rank_percentile(numeric, 50),
        "p95": nearest_rank_percentile(numeric, 95),
        "p99": nearest_rank_percentile(numeric, 99),
        "max": max(numeric),
    }


def _summarize_variant(runs: Sequence[Mapping[str, Any]], seed: int,
                       bootstrap_samples: int) -> dict[str, Any]:
    scalar_values: dict[str, list[float | int]] = defaultdict(list)
    list_values: dict[str, list[float | int]] = defaultdict(list)
    list_run_means: dict[str, list[float]] = defaultdict(list)
    null_counts: dict[str, int] = defaultdict(int)
    present_paths: set[str] = set()
    for run in runs:
        for path, value in _result_measurements(run["result"]):
            present_paths.add(path)
            if value is None:
                null_counts[path] += 1
            elif isinstance(value, list):
                list_values[path].extend(value)
                if value:
                    list_run_means[path].append(sum(value) / len(value))
            elif _is_number(value):
                scalar_values[path].append(value)
        scalar_values["benchmark_wall_time_ns"].append(run["result"]["metrics"]["wall_time_ns"])
        scalar_values["runner_wall_time_ns"].append(run["runner_wall_time_ns"])
        scalar_values["runner_process_cpu_user_ns"].append(
            run["process_cpu_time"]["user_ns"])
        scalar_values["runner_process_cpu_system_ns"].append(
            run["process_cpu_time"]["system_ns"])
        scalar_values["runner_process_cpu_total_ns"].append(
            run["process_cpu_time"]["total_ns"])
        if run["rss_peak_bytes"] is not None:
            scalar_values["runner_peak_rss_bytes"].append(run["rss_peak_bytes"])
        rss = [sample["rss_bytes"] for sample in run["rss_samples"]]
        present_paths.add("runner_rss_bytes")
        list_values["runner_rss_bytes"].extend(rss)
        if rss:
            list_run_means["runner_rss_bytes"].append(sum(rss) / len(rss))
    summaries: dict[str, Any] = {}
    all_paths = present_paths | set(scalar_values) | set(list_values) | set(_NULLABLE_METRICS)
    for index, path in enumerate(sorted(all_paths)):
        metric_seed = seed ^ int.from_bytes(hashlib.sha256(path.encode()).digest()[:8], "big")
        if path in list_values or path in list_run_means:
            summary = _stats(list_values[path], metric_seed, bootstrap_samples, "observations")
            if list_run_means[path]:
                summary["per_run_mean"] = sum(list_run_means[path]) / len(list_run_means[path])
                per_run_confidence_eligible = (
                    len(list_run_means[path]) >= _PAIRED_CI_MINIMUM_PAIRS)
                summary["per_run_confidence_status"] = (
                    "available" if per_run_confidence_eligible else "insufficient_sample")
                summary["per_run_mean_ci95"] = (
                    bootstrap_mean_ci(list_run_means[path], metric_seed ^ index,
                                      bootstrap_samples)
                    if per_run_confidence_eligible else None)
        else:
            summary = _stats(scalar_values[path], metric_seed, bootstrap_samples, "run_scalar")
        summary["null_count"] = null_counts[path]
        summaries[path] = summary
    return {"run_count": len(runs), "metrics": summaries}


def _run_scalar_metric(run: Mapping[str, Any], metric: str) -> int:
    if metric == "benchmark_wall_time_ns":
        return int(run["result"]["metrics"]["wall_time_ns"])
    if metric == "runner_wall_time_ns":
        return int(run["runner_wall_time_ns"])
    cpu_field = metric.removeprefix("runner_process_cpu_")
    return int(run["process_cpu_time"][cpu_field])


def _distribution(values: Sequence[float | int]) -> dict[str, Any]:
    _require(bool(values), "schema", "distribution needs at least one value")
    numeric = [float(value) for value in values]
    mean = sum(numeric) / len(numeric)
    sample_stddev = None
    if len(numeric) >= 2:
        sample_stddev = math.sqrt(
            sum((value - mean) ** 2 for value in numeric) / (len(numeric) - 1))
    total: float | int = sum(values)
    return {
        "count": len(numeric), "total": total, "mean": mean,
        "sample_stddev": sample_stddev, "min": min(values), "max": max(values),
    }


def _paired_effect(values: Sequence[float | int], seed: int, bootstrap_samples: int,
                   confidence_eligible: bool) -> dict[str, Any]:
    return {
        "status": "available" if confidence_eligible else "insufficient_sample",
        "distribution": _distribution(values),
        "mean_ci95": (bootstrap_mean_ci(values, seed, bootstrap_samples)
                      if confidence_eligible else None),
    }


def _summarize_paired_runs(grouped: Mapping[str, Sequence[Mapping[str, Any]]],
                           labels: Sequence[str], pair_count: int, seed: int,
                           bootstrap_samples: int) -> dict[str, Any]:
    if len(labels) == 1:
        return {
            "status": "not_applicable_single_variant",
            "minimum_pair_count_for_ci": _PAIRED_CI_MINIMUM_PAIRS,
            "pair_count": pair_count,
            "metrics": {},
        }
    baseline_label, candidate_label = labels
    runs_by_label_and_pair = {
        label: {run["pair_index"]: run for run in grouped[label]} for label in labels
    }
    _require(all(set(runs_by_label_and_pair[label]) == set(range(pair_count))
                 for label in labels), "schema", "paired summary input is incomplete")
    confidence_eligible = pair_count >= _PAIRED_CI_MINIMUM_PAIRS
    metrics: dict[str, Any] = {}
    for metric in _PAIRED_RUN_METRICS:
        baseline_values = [
            _run_scalar_metric(runs_by_label_and_pair[baseline_label][index], metric)
            for index in range(pair_count)
        ]
        candidate_values = [
            _run_scalar_metric(runs_by_label_and_pair[candidate_label][index], metric)
            for index in range(pair_count)
        ]
        deltas = [candidate - baseline for baseline, candidate in
                  zip(baseline_values, candidate_values)]
        metric_seed = seed ^ int.from_bytes(
            hashlib.sha256(f"paired:{metric}".encode()).digest()[:8], "big")
        zero_baseline_pair_count = sum(value == 0 for value in baseline_values)
        if zero_baseline_pair_count:
            ratio = {
                "status": "undefined_zero_baseline",
                "zero_baseline_pair_count": zero_baseline_pair_count,
                "distribution": None,
                "mean_ci95": None,
            }
        else:
            ratios = [candidate / baseline for baseline, candidate in
                      zip(baseline_values, candidate_values)]
            ratio = {
                **_paired_effect(ratios, metric_seed ^ 0xA5A5A5A5,
                                 bootstrap_samples, confidence_eligible),
                "zero_baseline_pair_count": 0,
            }
        metrics[metric] = {
            "kind": "paired_run_scalar",
            "pair_count": pair_count,
            "baseline": _distribution(baseline_values),
            "candidate": _distribution(candidate_values),
            "delta": _paired_effect(deltas, metric_seed, bootstrap_samples,
                                    confidence_eligible),
            "ratio": ratio,
        }
    return {
        "status": "available" if confidence_eligible else "insufficient_sample",
        "minimum_pair_count_for_ci": _PAIRED_CI_MINIMUM_PAIRS,
        "pair_count": pair_count,
        "baseline_label": baseline_label,
        "candidate_label": candidate_label,
        "metrics": metrics,
    }


def summarize_raw(document: Any, source_raw_sha256: str | None = None,
                  bootstrap_samples: int = 5000) -> dict[str, Any]:
    """Summarize valid raw runs without filtering any latency or run total."""

    payload = validate_raw(document)
    _require(payload["status"] == "accepted", "schema", "cannot summarize a rejected packet")
    _require(_is_nonnegative_int(bootstrap_samples) and bootstrap_samples > 0, "schema",
             "bootstrap sample count must be a positive integer")
    seed = payload["seed"]
    labels = payload["label_order"]
    paired_bootstrap_draws = (payload["pair_count"] * bootstrap_samples *
                              len(payload["matrix"]["cases"]) *
                              len(_PAIRED_RUN_METRICS) * 2 if len(labels) == 2 else 0)
    _require(paired_bootstrap_draws <= _MAX_PAIRED_BOOTSTRAP_DRAWS, "schema",
             "paired bootstrap request exceeds its deterministic work cap",
             requested_draws=paired_bootstrap_draws,
             maximum_draws=_MAX_PAIRED_BOOTSTRAP_DRAWS)
    grouped: dict[str, dict[str, list[Mapping[str, Any]]]] = defaultdict(lambda: defaultdict(list))
    for run in payload["runs"]:
        grouped[run["case_id"]][run["variant"]].append(run)
    cases: dict[str, Any] = {}
    for case_index, case in enumerate(payload["matrix"]["cases"]):
        case_id = _case_id(case)
        case_result = {"fixture": case["fixture"],
                       "fixture_sha256": _case_fixture_sha256(case),
                       "workload": case["workload"],
                       "thresholds": case.get("thresholds", []), "variants": {}}
        for label in labels:
            variant_runs = sorted(grouped[case_id][label], key=lambda run: run["pair_index"])
            case_result["variants"][label] = _summarize_variant(
                variant_runs, seed ^ case_index, bootstrap_samples)
        case_result["paired"] = _summarize_paired_runs(
            grouped[case_id], labels, payload["pair_count"],
            seed ^ case_index, bootstrap_samples)
        cases[case_id] = case_result
    summary_payload = {
        "created_utc": _utc_now(),
        "source_raw_sha256": source_raw_sha256,
        "source_payload_sha256": document["content_sha256"],
        "seed": seed,
        "pair_count": payload["pair_count"],
        "label_order": labels,
        "process_cpu_time_source": payload["process_cpu_time_source"],
        "paired_bootstrap_draw_limit": _MAX_PAIRED_BOOTSTRAP_DRAWS,
        "expected_paired_bootstrap_draws": paired_bootstrap_draws,
        "bootstrap_samples": bootstrap_samples,
        "percentile_method": "nearest-rank: sorted[ceil(p*n/100)-1], p=0 selects minimum",
        "matrix_sha256": payload["matrix_sha256"],
        "matrix": payload["matrix"],
        "binaries": payload["binaries"],
        "cases": cases,
    }
    return wrap_envelope(SUMMARY_SCHEMA, summary_payload)


def _validate_distribution_summary(value: Any, path: str, expected_count: int) -> None:
    required = {"count", "total", "mean", "sample_stddev", "min", "max"}
    _require(isinstance(value, dict) and set(value) == required, "schema",
             f"{path} must be a complete distribution")
    _require(value["count"] == expected_count, "schema", f"{path} count mismatch")
    for field in ("total", "mean", "min", "max"):
        _require(_is_number(value[field]), "schema", f"{path}.{field} is invalid")
    _require(value["min"] <= value["mean"] <= value["max"], "schema",
             f"{path} mean is outside its range")
    _require(math.isclose(float(value["mean"]),
                          float(value["total"]) / expected_count,
                          rel_tol=1e-12, abs_tol=1e-9), "schema",
             f"{path} total and mean disagree")
    if expected_count == 1:
        _require(value["sample_stddev"] is None, "schema",
                 f"{path} one-sample dispersion must be unavailable")
    else:
        _require(_is_number(value["sample_stddev"]) and
                 value["sample_stddev"] >= 0, "schema",
                 f"{path} sample dispersion is invalid")


def _validate_run_scalar_summary(value: Any, path: str, pair_count: int) -> None:
    _require(isinstance(value, dict) and value.get("kind") == "run_scalar" and
             value.get("available") is True and value.get("count") == pair_count and
             _is_number(value.get("mean")), "schema",
             f"{path} must be an available run-scalar summary")
    confidence_eligible = pair_count >= _PAIRED_CI_MINIMUM_PAIRS
    _require(value.get("confidence_status") ==
             ("available" if confidence_eligible else "insufficient_sample"), "schema",
             f"{path} confidence status does not match pair count")
    ci = value.get("mean_ci95")
    if confidence_eligible:
        _require(isinstance(ci, list) and len(ci) == 2 and
                 all(_is_number(bound) for bound in ci) and ci[0] <= ci[1], "schema",
                 f"{path} confidence interval is invalid")
    else:
        _require(ci is None, "schema",
                 f"{path} must not invent a run-level confidence interval below the declared minimum")


def _validate_paired_effect(value: Any, path: str, pair_count: int,
                            confidence_eligible: bool) -> None:
    required = {"status", "distribution", "mean_ci95"}
    _require(isinstance(value, dict) and set(value) == required, "schema",
             f"{path} must be a complete paired effect")
    expected_status = "available" if confidence_eligible else "insufficient_sample"
    _require(value["status"] == expected_status, "schema",
             f"{path} confidence status does not match pair count")
    _validate_distribution_summary(value["distribution"], f"{path}.distribution", pair_count)
    if confidence_eligible:
        ci = value["mean_ci95"]
        _require(isinstance(ci, list) and len(ci) == 2 and
                 all(_is_number(bound) for bound in ci) and ci[0] <= ci[1], "schema",
                 f"{path} confidence interval is invalid")
    else:
        _require(value["mean_ci95"] is None, "schema",
                 f"{path} must not invent a confidence interval below the declared minimum")


def _validate_paired_summary(value: Any, labels: Sequence[str], pair_count: int,
                             path: str) -> None:
    if len(labels) == 1:
        required = {"status", "minimum_pair_count_for_ci", "pair_count", "metrics"}
        _require(isinstance(value, dict) and set(value) == required, "schema",
                 f"{path} must be a complete single-variant pairing status")
        _require(value["status"] == "not_applicable_single_variant" and
                 value["metrics"] == {}, "schema",
                 f"{path} must not invent a single-variant paired comparison")
    else:
        required = {
            "status", "minimum_pair_count_for_ci", "pair_count", "baseline_label",
            "candidate_label", "metrics",
        }
        _require(isinstance(value, dict) and set(value) == required, "schema",
                 f"{path} must be a complete paired comparison")
        confidence_eligible = pair_count >= _PAIRED_CI_MINIMUM_PAIRS
        _require(value["status"] ==
                 ("available" if confidence_eligible else "insufficient_sample"), "schema",
                 f"{path} status does not match pair count")
        _require(value["baseline_label"] == labels[0] and
                 value["candidate_label"] == labels[1], "schema",
                 f"{path} labels do not match stable variant order")
        metrics = value["metrics"]
        _require(isinstance(metrics, dict) and set(metrics) == set(_PAIRED_RUN_METRICS),
                 "schema", f"{path} paired metric set is incomplete")
        for metric in _PAIRED_RUN_METRICS:
            metric_value = metrics[metric]
            metric_path = f"{path}.metrics.{metric}"
            required_metric = {"kind", "pair_count", "baseline", "candidate", "delta", "ratio"}
            _require(isinstance(metric_value, dict) and
                     set(metric_value) == required_metric, "schema",
                     f"{metric_path} is incomplete")
            _require(metric_value["kind"] == "paired_run_scalar" and
                     metric_value["pair_count"] == pair_count, "schema",
                     f"{metric_path} identity is invalid")
            _validate_distribution_summary(
                metric_value["baseline"], f"{metric_path}.baseline", pair_count)
            _validate_distribution_summary(
                metric_value["candidate"], f"{metric_path}.candidate", pair_count)
            _validate_paired_effect(
                metric_value["delta"], f"{metric_path}.delta", pair_count,
                confidence_eligible)
            baseline_total = metric_value["baseline"]["total"]
            candidate_total = metric_value["candidate"]["total"]
            delta_total = metric_value["delta"]["distribution"]["total"]
            _require(math.isclose(float(delta_total),
                                  float(candidate_total) - float(baseline_total),
                                  rel_tol=1e-12, abs_tol=1e-9), "schema",
                     f"{metric_path} paired delta total disagrees with variant totals")
            ratio = metric_value["ratio"]
            required_ratio = {"status", "zero_baseline_pair_count", "distribution", "mean_ci95"}
            _require(isinstance(ratio, dict) and set(ratio) == required_ratio, "schema",
                     f"{metric_path}.ratio is incomplete")
            zero_count = ratio["zero_baseline_pair_count"]
            _require(_is_nonnegative_int(zero_count) and zero_count <= pair_count, "schema",
                     f"{metric_path}.ratio zero-baseline count is invalid")
            if zero_count:
                _require(ratio["status"] == "undefined_zero_baseline" and
                         ratio["distribution"] is None and ratio["mean_ci95"] is None,
                         "schema", f"{metric_path}.ratio must be unavailable for zero baselines")
            else:
                _validate_paired_effect(
                    {name: ratio[name] for name in ("status", "distribution", "mean_ci95")},
                    f"{metric_path}.ratio", pair_count, confidence_eligible)
        for side in ("baseline", "candidate"):
            _require(math.isclose(
                float(metrics["runner_process_cpu_total_ns"][side]["total"]),
                float(metrics["runner_process_cpu_user_ns"][side]["total"]) +
                float(metrics["runner_process_cpu_system_ns"][side]["total"]),
                rel_tol=1e-12, abs_tol=1e-9), "schema",
                f"{path} {side} process CPU component totals disagree")
        _require(math.isclose(
            float(metrics["runner_process_cpu_total_ns"]["delta"]["distribution"]["total"]),
            float(metrics["runner_process_cpu_user_ns"]["delta"]["distribution"]["total"]) +
            float(metrics["runner_process_cpu_system_ns"]["delta"]["distribution"]["total"]),
            rel_tol=1e-12, abs_tol=1e-9), "schema",
            f"{path} process CPU delta component totals disagree")
    _require(value["minimum_pair_count_for_ci"] == _PAIRED_CI_MINIMUM_PAIRS and
             value["pair_count"] == pair_count, "schema",
             f"{path} pairing policy mismatch")


def validate_summary(document: Any) -> dict[str, Any]:
    payload = _verify_envelope(document, SUMMARY_SCHEMA)
    matrix = validate_matrix(payload.get("matrix"))
    binaries = payload.get("binaries")
    _require(isinstance(binaries, dict) and 1 <= len(binaries) <= 2, "schema",
             "summary binaries must contain one or two variants")
    pair_count = payload.get("pair_count")
    _require(_is_nonnegative_int(pair_count) and pair_count > 0, "schema",
             "invalid summary pair count")
    labels = payload.get("label_order")
    _require(isinstance(labels, list) and len(labels) == len(binaries) and
             len(set(labels)) == len(labels) and set(labels) == set(binaries), "schema",
             "invalid summary stable label order")
    bootstrap_samples = payload.get("bootstrap_samples")
    _require(_is_nonnegative_int(bootstrap_samples) and bootstrap_samples > 0, "schema",
             "invalid bootstrap count")
    expected_paired_bootstrap_draws = (pair_count * bootstrap_samples *
                                       len(matrix["cases"]) * len(_PAIRED_RUN_METRICS) * 2
                                       if len(labels) == 2 else 0)
    _require(payload.get("paired_bootstrap_draw_limit") == _MAX_PAIRED_BOOTSTRAP_DRAWS and
             payload.get("expected_paired_bootstrap_draws") ==
             expected_paired_bootstrap_draws and
             expected_paired_bootstrap_draws <= _MAX_PAIRED_BOOTSTRAP_DRAWS, "schema",
             "summary paired bootstrap work accounting is invalid")
    _require(payload.get("process_cpu_time_source") in _PROCESS_CPU_TIME_SOURCES, "schema",
             "invalid summary process CPU time source")
    cases = payload.get("cases")
    expected_case_ids = {_case_id(case) for case in matrix["cases"]}
    _require(isinstance(cases, dict) and set(cases) == expected_case_ids, "schema",
             "summary cases do not match the matrix")
    for case_id, case_summary in cases.items():
        _require(isinstance(case_summary, dict), "schema",
                 f"summary case {case_id!r} must be an object")
        variants = case_summary.get("variants")
        _require(isinstance(variants, dict) and set(variants) == set(labels), "schema",
                 f"summary case {case_id!r} variants are incomplete")
        for label in labels:
            _require(isinstance(variants[label], dict) and
                     variants[label].get("run_count") == pair_count, "schema",
                     f"summary variant {label!r} run count mismatch")
            metrics = variants[label].get("metrics")
            _require(isinstance(metrics, dict), "schema",
                     f"summary variant {label!r} metrics are invalid")
            for metric in _PAIRED_RUN_METRICS:
                _validate_run_scalar_summary(
                    metrics.get(metric), f"cases.{case_id}.variants.{label}.{metric}",
                    pair_count)
            _require(math.isclose(
                float(metrics["runner_process_cpu_total_ns"]["mean"]),
                float(metrics["runner_process_cpu_user_ns"]["mean"]) +
                float(metrics["runner_process_cpu_system_ns"]["mean"]),
                rel_tol=1e-12, abs_tol=1e-9), "schema",
                f"summary variant {label!r} process CPU components disagree")
        _validate_paired_summary(case_summary.get("paired"), labels, pair_count,
                                 f"cases.{case_id}.paired")
        if len(labels) == 2:
            paired_metrics = case_summary["paired"]["metrics"]
            for metric in _PAIRED_RUN_METRICS:
                for side, label in (("baseline", labels[0]), ("candidate", labels[1])):
                    _require(math.isclose(
                        float(paired_metrics[metric][side]["mean"]),
                        float(variants[label]["metrics"][metric]["mean"]),
                        rel_tol=1e-12, abs_tol=1e-9), "schema",
                        f"summary paired {side} mean disagrees with variant {label!r}")
    return payload


def compare_summary(document: Any, baseline_label: str, candidate_label: str) -> dict[str, Any]:
    """Apply every declared matrix threshold to a summary."""

    payload = validate_summary(document)
    _require(baseline_label in payload["binaries"], "schema", "unknown baseline label")
    _require(candidate_label in payload["binaries"], "schema", "unknown candidate label")
    has_relative_threshold = any(
        "ratio_max" in rule or "delta_max" in rule
        for case in payload["matrix"]["cases"]
        for rule in case.get("thresholds", [])
    )
    _require(baseline_label != candidate_label or not has_relative_threshold, "schema",
             "relative thresholds require distinct baseline and candidate labels")
    results: list[dict[str, Any]] = []
    for case in payload["matrix"]["cases"]:
        case_id = _case_id(case)
        summarized = payload["cases"].get(case_id, {})
        variants = summarized.get("variants", {})
        baseline_metrics = variants.get(baseline_label, {}).get("metrics", {})
        candidate_metrics = variants.get(candidate_label, {}).get("metrics", {})
        for rule in case.get("thresholds", []):
            metric = rule["metric"]
            statistic = rule["statistic"]
            baseline = baseline_metrics.get(metric, {}).get(statistic)
            candidate = candidate_metrics.get(metric, {}).get(statistic)
            checks: list[dict[str, Any]] = []
            candidate_available = _is_number(candidate)
            baseline_required = "ratio_max" in rule or "delta_max" in rule
            baseline_available = _is_number(baseline)
            if not candidate_available:
                checks.append({"name": "candidate_availability", "pass": False})
            if baseline_required and not baseline_available:
                checks.append({"name": "baseline_availability", "pass": False})
            if candidate_available:
                if "ratio_max" in rule:
                    if baseline_available:
                        ratio = (candidate / baseline if baseline != 0 else
                                 (1.0 if candidate == 0 else None))
                        checks.append({"name": "ratio_max", "actual": ratio,
                                       "limit": rule["ratio_max"],
                                       "pass": ratio is not None and ratio <= rule["ratio_max"]})
                if "delta_max" in rule:
                    if baseline_available:
                        delta = candidate - baseline
                        checks.append({"name": "delta_max", "actual": delta,
                                       "limit": rule["delta_max"], "pass": delta <= rule["delta_max"]})
                if "absolute_max" in rule:
                    checks.append({"name": "absolute_max", "actual": candidate,
                                   "limit": rule["absolute_max"],
                                   "pass": candidate <= rule["absolute_max"]})
                if "absolute_min" in rule:
                    checks.append({"name": "absolute_min", "actual": candidate,
                                   "limit": rule["absolute_min"],
                                   "pass": candidate >= rule["absolute_min"]})
            results.append({
                "case_id": case_id, "metric": metric, "statistic": statistic,
                "baseline": baseline, "candidate": candidate, "checks": checks,
                "pass": bool(checks) and all(check["pass"] for check in checks),
            })
    threshold_count = len(results)
    comparison_payload = {
        "created_utc": _utc_now(),
        "source_summary_payload_sha256": document["content_sha256"],
        "baseline_label": baseline_label,
        "candidate_label": candidate_label,
        "status": "evaluated" if threshold_count else "no_thresholds",
        "overall_pass": threshold_count > 0 and all(result["pass"] for result in results),
        "threshold_count": threshold_count,
        "results": results,
    }
    return wrap_envelope(COMPARE_SCHEMA, comparison_payload)


def _load_artifact(path: os.PathLike[str] | str) -> tuple[bytes, Any]:
    return _read_json_bytes(path, "schema")


def _run_command(args: argparse.Namespace) -> int:
    binaries = {args.label_a: args.binary_a}
    data_roots = {args.label_a: args.data_root_a}
    if args.binary_b:
        _require(args.label_b != args.label_a, "binary", "A and B labels must differ")
        _require(bool(args.data_root_b), "data_root", "binary B needs --data-root-b")
        binaries[args.label_b] = args.binary_b
        data_roots[args.label_b] = args.data_root_b
    else:
        _require(args.data_root_b is None, "data_root",
                 "--data-root-b requires --binary-b")
    document = run_benchmarks(args.matrix, binaries, args.pairs, args.seed,
                              args.child_arg, args.timeout, args.rss_interval,
                              data_roots=data_roots)
    payload = validate_raw(document)
    _write_json(args.output, document)
    print(json.dumps({"status": payload["status"], "output": str(pathlib.Path(args.output).resolve()),
                      "artifact_sha256": sha256_file(args.output),
                      "valid_runs": len(payload["runs"]), "failures": payload["failures"]},
                     sort_keys=True))
    return 0 if payload["status"] == "accepted" else 2


def _summarize_command(args: argparse.Namespace) -> int:
    raw_bytes, document = _load_artifact(args.raw)
    summary = summarize_raw(document, sha256_bytes(raw_bytes), args.bootstrap_samples)
    _write_json(args.output, summary)
    print(json.dumps({"status": "accepted", "output": str(pathlib.Path(args.output).resolve()),
                      "artifact_sha256": sha256_file(args.output)}, sort_keys=True))
    return 0


def _compare_command(args: argparse.Namespace) -> int:
    _, document = _load_artifact(args.summary)
    if document.get("schema") == RAW_SCHEMA:
        document = summarize_raw(document, sha256_file(args.summary), args.bootstrap_samples)
    comparison = compare_summary(document, args.baseline_label, args.candidate_label)
    _write_json(args.output, comparison)
    comparison_payload = comparison["payload"]
    passed = comparison_payload["overall_pass"]
    status = ("no_thresholds" if comparison_payload["threshold_count"] == 0 else
              ("pass" if passed else "threshold_failure"))
    print(json.dumps({"status": status,
                      "output": str(pathlib.Path(args.output).resolve()),
                      "artifact_sha256": sha256_file(args.output)}, sort_keys=True))
    return 0 if passed else 1


def _validate_command(args: argparse.Namespace) -> int:
    _, document = _load_artifact(args.artifact)
    schema = document.get("schema") if isinstance(document, dict) else None
    if schema == RAW_SCHEMA:
        payload = validate_raw(document, args.verify_files)
        accepted = payload["status"] == "accepted"
    elif schema == SUMMARY_SCHEMA:
        validate_summary(document)
        accepted = True
    elif schema == CHILD_SCHEMA:
        validate_child_result(document)
        accepted = True
    elif schema == MATRIX_SCHEMA:
        validate_matrix(document)
        accepted = True
    elif schema == COMPARE_SCHEMA:
        payload = _verify_envelope(document, COMPARE_SCHEMA)
        _require(isinstance(payload.get("overall_pass"), bool), "schema",
                 "comparison needs overall_pass")
        _require(payload.get("status") in ("evaluated", "no_thresholds"), "schema",
                 "comparison has invalid evaluation status")
        _require(_is_nonnegative_int(payload.get("threshold_count")), "schema",
                 "comparison has invalid threshold count")
        if payload["threshold_count"] == 0:
            _require(payload["status"] == "no_thresholds" and not payload["overall_pass"],
                     "schema", "empty comparison cannot pass")
        accepted = payload["overall_pass"]
    else:
        raise BenchmarkError("schema", f"unsupported artifact schema {schema!r}")
    print(json.dumps({"valid": True, "accepted": accepted, "schema": schema,
                      "artifact_sha256": sha256_file(args.artifact)}, sort_keys=True))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="run a serial one- or two-binary packet")
    run_parser.add_argument("--matrix", required=True, help="v1 matrix JSON")
    run_parser.add_argument("--binary-a", required=True, help="baseline or only instrumented binary")
    run_parser.add_argument("--data-root-a", required=True,
                            help="A worktree/root containing data/; used as child cwd")
    run_parser.add_argument("--label-a", default="A", help="stable first variant label")
    run_parser.add_argument("--binary-b", help="candidate instrumented binary for paired runs")
    run_parser.add_argument("--data-root-b",
                            help="B worktree/root containing data/; required with --binary-b")
    run_parser.add_argument("--label-b", default="B", help="stable second variant label")
    run_parser.add_argument("--pairs", type=int, default=10, help="repetition pairs (default: 10)")
    run_parser.add_argument("--seed", type=int, default=830204929,
                            help="ordering/child seed (default: 830204929)")
    run_parser.add_argument("--child-arg", action="append", default=[],
                            help="argument before matrix case arguments; repeat as needed")
    run_parser.add_argument("--timeout", type=float, default=300.0,
                            help="declared per-child timeout seconds; a timeout rejects the packet")
    run_parser.add_argument("--rss-interval", type=float, default=0.1,
                            help="bounded child RSS sample interval seconds")
    run_parser.add_argument("--output", required=True, help="raw packet JSON destination")
    run_parser.set_defaults(handler=_run_command)

    summary_parser = subparsers.add_parser("summarize", help="summarize all valid raw runs")
    summary_parser.add_argument("raw", help="accepted raw packet")
    summary_parser.add_argument("--bootstrap-samples", type=int, default=5000)
    summary_parser.add_argument("--output", required=True)
    summary_parser.set_defaults(handler=_summarize_command)

    compare_parser = subparsers.add_parser("compare", help="apply matrix thresholds")
    compare_parser.add_argument("summary", help="accepted raw packet or summary")
    compare_parser.add_argument("--baseline-label", default="A")
    compare_parser.add_argument("--candidate-label", default="B")
    compare_parser.add_argument("--bootstrap-samples", type=int, default=5000,
                                help="used only when input is raw")
    compare_parser.add_argument("--output", required=True)
    compare_parser.set_defaults(handler=_compare_command)

    validate_parser = subparsers.add_parser("validate", help="validate a matrix/child/raw/summary/result")
    validate_parser.add_argument("artifact")
    validate_parser.add_argument("--verify-files", action="store_true",
                                 help="also re-hash current binary and matrix paths for raw packets")
    validate_parser.set_defaults(handler=_validate_command)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.handler(args)
    except BenchmarkError as error:
        print(json.dumps({"status": "rejected", "failure": error.as_dict()}, sort_keys=True),
              file=sys.stderr)
        return 2
    except (OSError, ValueError) as error:
        failure = BenchmarkError("schema", str(error))
        print(json.dumps({"status": "rejected", "failure": failure.as_dict()}, sort_keys=True),
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
