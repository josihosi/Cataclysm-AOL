"""Bound process measurements, retained independently of the game request pipe.

No background process, game input, thresholds, or inferred gameplay outcomes.
"""
from __future__ import annotations

import json
import math
import os
from pathlib import Path
import platform
import time
from typing import Any, Mapping
import uuid

from r009_technical_witness import sample_child_resources, complete_child_resource_interval


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return {}


def write_json(path: Path, value: dict):
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    temporary.write_text(json.dumps(value))
    os.replace(temporary, path)


def native_context(frame: Mapping[str, Any], action_id: str = "") -> dict:
    kind = str(frame.get("kind", frame.get("state", "unknown")))
    if action_id:
        phase = "native_action"
        if action_id == "world.pause" or action_id.startswith("wait."):
            phase = "waiting_or_fast_forward"
    elif kind == "world":
        phase = "waiting_for_input"
    elif kind in {"wait_activity", "activity_resumed"}:
        phase = "waiting_or_fast_forward"
    elif frame.get("event") == "surface_descriptor" and kind != "unknown":
        phase = "menu_or_nested_input"
    else:
        phase = "unknown"
    return {"phase": phase, "action_id": action_id or None, "surface_kind": kind,
            "frame_id": frame.get("frame_id"), "source": "native_owner_and_dispatch",
            "note": "A stable last input owner is context, not proof the process is idle."}


def resource_record(owner: dict, before: dict, after: dict, context: dict,
                    before_minutes=None, after_minutes=None, latency=None) -> dict:
    resources = complete_child_resource_interval(before, after)
    progress = None
    if all(isinstance(value, (int, float)) and not isinstance(value, bool)
           for value in (before_minutes, after_minutes)):
        progress = after_minutes - before_minutes
    return {"schema": "caol-process-performance-v1", "record_id": uuid.uuid4().hex,
            "recorded_unix_seconds": time.time(), "owner": owner,
            "context": context, "resources": resources, "action_latency_seconds": latency,
            "game_time": {"before_minutes": before_minutes, "after_minutes": after_minutes,
                          "delta_minutes": progress, "source": "native_frame"},
            "interpretation": "High CPU during simulation is not itself a problem. Compare elapsed work, game-time progress and memory against an explicitly comparable baseline. This does not identify a slowdown's cause."}


def append_record(directory: Path, record: dict):
    # Each writer appends one complete short record with one O_APPEND write.
    # No in-memory history and no rewriting the growing journal.
    raw = (json.dumps(record, separators=(",", ":")) + "\n").encode()
    fd = os.open(directory / "performance.jsonl", os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        if os.write(fd, raw) != len(raw):
            raise OSError("incomplete performance journal append")
    finally:
        os.close(fd)
    write_json(directory / "performance.latest.json", record)


def telemetry_only(operation):
    """Instrumentation failure must not erase a dispatched native receipt."""
    def measured(self, *args, **kwargs):
        try:
            return operation(self, *args, **kwargs)
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.failure = str(error)
            self.stopped = True
            return None
    return measured


class ProcessPerformance:
    def __init__(self, directory: Path, *, pid: int, run_id: str, binding_id: str,
                 source_binding: dict, sampler=sample_child_resources):
        self.directory = directory
        self.sampler = sampler
        self.previous = sampler(pid)
        self.owner = {"pid": pid, "run_id": run_id, "binding_id": binding_id,
                      "process_identity": self.previous.get("process_identity"),
                      "platform": self.previous.get("platform"),
                      "host": platform.node(), "machine": platform.machine(),
                      "source_binding": source_binding}
        self.stopped = False
        self.failure = None
        self.latest = None
        self.before = None
        self.started = None
        write_json(directory / "performance-owner.json", self.owner)
        self.context = {"phase": "unknown", "source": "no_native_owner_observed"}
        self.set_context(self.context)

    def set_context(self, context: dict):
        self.context = dict(context)
        write_json(self.directory / "performance-context.json", {
            "owner": self.owner, "context": self.context,
            "context_id": uuid.uuid4().hex, "session_ended": self.stopped})

    def sample(self):
        if self.stopped:
            return None
        sample = self.sampler(self.owner["pid"])
        if not self.owner.get("process_identity") or sample.get("process_identity") != self.owner["process_identity"]:
            self.failure = "owned process identity unavailable or changed"
            self.stopped = True
            self.set_context({"phase": "unknown", "source": "process_identity_unavailable_or_changed"})
            return None
        return sample

    @telemetry_only
    def begin_action(self, frame: Mapping[str, Any], action_id: str):
        self.before = self.sample()
        if self.stopped:
            return
        self.started = time.monotonic()
        self.set_context({**native_context(frame, action_id), "game_minutes": frame.get("game_minutes")})

    @telemetry_only
    def end_action(self, frame: Mapping[str, Any], outcome: Mapping[str, Any]):
        latency = time.monotonic() - self.started if self.started is not None else None
        after = self.sample()
        successor = outcome.get("next_frame") or {}
        if self.before is not None and after is not None:
            record = resource_record(self.owner, self.before, after,
                                     {**self.context, "accepted": outcome.get("accepted"),
                                      "outcome": outcome.get("outcome", outcome.get("error"))},
                                     frame.get("game_minutes"), successor.get("game_minutes"), latency)
            append_record(self.directory, record)
            self.latest = record
            self.previous = after
        self.before = None
        self.started = None
        if not self.stopped:
            self.set_context(native_context(successor) if successor else {
                "phase": "unknown", "source": "dispatch_returned_without_successor"})

    @telemetry_only
    def observe(self, frame: Mapping[str, Any]):
        if self.stopped or self.before is not None:
            return
        after = self.sample()
        if after is not None:
            context = {"phase": "between_observations", "start_context": self.context,
                       "end_context": native_context(frame),
                       "source": "observation_endpoints", "note": "Interval may include unobserved simulation; not certified idle."}
            record = resource_record(self.owner, self.previous, after, context)
            append_record(self.directory, record)
            self.latest = record
            self.previous = after
            self.set_context(native_context(frame))

    @telemetry_only
    def stop(self):
        if not self.stopped:
            self.stopped = True
            self.set_context({"phase": "session_or_process_ended", "source": "lifecycle"})

    def brief(self):
        return {"latest": self.latest, "collection_error": self.failure, "stopped": self.stopped,
                "records_path": str(self.directory / "performance.jsonl"),
                "next": "play_cli performance; use --sample-seconds 1 during pending work"}


def sample_owned_session(directory: Path, binding_id: str, seconds: float,
                         *, sampler=sample_child_resources, sleep=time.sleep) -> dict:
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("sample_seconds_must_be_positive_and_finite")
    owner = read_json(directory / "performance-owner.json")
    if owner.get("binding_id") != binding_id or not owner.get("process_identity"):
        raise ValueError("bound_performance_owner_unavailable")
    def check():
        context = read_json(directory / "performance-context.json")
        status = read_json(directory / "status.json")
        if context.get("owner") != owner or context.get("session_ended") or status.get("state") in {
            "process_dead", "bridge_failed", "terminalization_failed", "safe_to_cleanup"} or \
                read_json(directory / "game-process-exit.json").get("run_id") == owner.get("run_id"):
            raise ValueError("performance_session_ended_or_identity_changed")
        sample = sampler(owner["pid"])
        if sample.get("process_identity") != owner["process_identity"] or sample.get("pid") != owner["pid"]:
            raise ValueError("owned_process_ended_or_pid_reused")
        return sample, context
    before, context_before = check()
    sleep(seconds)
    after, context_after = check()
    context = context_before.get("context", {"phase": "unknown"})
    if context_before.get("context_id") != context_after.get("context_id"):
        context = {"phase": "mixed", "start_context": context,
                   "end_context": context_after.get("context"), "source": "context_changed_during_sample"}
    else:
        context = {**context, "source": "session_context_stable_across_sample",
                   "note": "Stable dispatch context; CPU does not prove idle, progress, or a hang."}
    record = resource_record(owner, before, after, context)
    record["sample_kind"] = "on_demand"
    append_record(directory, record)
    return record


def read_records(directory: Path, offset: int, limit: int) -> dict:
    if offset < 0 or limit <= 0:
        raise ValueError("performance_page_needs_nonnegative_offset_and_positive_limit")
    items = []
    more = False
    path = directory / "performance.jsonl"
    if path.exists():
        with path.open() as source:
            for index, line in enumerate(source):
                if index < offset:
                    continue
                if len(items) == limit:
                    more = True
                    break
                try:
                    items.append(json.loads(line))
                except ValueError:
                    items.append({"error": "incomplete_record", "line": index + 1})
    return {"records": items, "offset": offset,
            "next_offset": offset + len(items) if more else None, "records_path": str(path)}


def compare_records(current: dict, baseline: dict, tag: str) -> dict:
    previous = baseline.get("record", {})
    if not isinstance(previous, dict) or any(
            not record.get("owner", {}).get(key)
            for record in (current, previous)
            for key in ("pid", "run_id", "binding_id", "process_identity", "host", "platform")):
        return {"status": "unavailable", "reason": "both records need retained process/run bindings"}
    if not tag or baseline.get("comparison_tag") != tag:
        return {"status": "unavailable", "reason": "explicit matching comparison tag required"}
    keys = ("host", "machine", "platform")
    if any(current.get("owner", {}).get(key) != previous.get("owner", {}).get(key) for key in keys):
        return {"status": "incomparable", "reason": "host/platform differs"}
    context_keys = ("phase", "action_id", "surface_kind")
    if current.get("context", {}).get("phase") in {None, "unknown", "mixed", "between_observations"} or any(
            current.get("context", {}).get(key) != previous.get("context", {}).get(key) for key in context_keys):
        return {"status": "incomparable", "reason": "operation contexts differ or are unknown/mixed"}
    def metrics(record):
        resources = record.get("resources", {})
        values = {"process_core_percent": resources.get("cpu_percent", {}).get("value"),
                  "rss_bytes": resources.get("resident_memory", {}).get("value")}
        progress = record.get("game_time", {}).get("delta_minutes")
        if isinstance(progress, (int, float)) and progress > 0:
            for source, target in (("interval_wall_seconds", "wall_seconds_per_game_minute"),
                                   ("interval_cpu_seconds", "cpu_seconds_per_game_minute")):
                if isinstance(resources.get(source), (int, float)):
                    values[target] = resources[source] / progress
        elif progress == 0:
            values["action_latency_seconds"] = record.get("action_latency_seconds")
        return values
    old, new = metrics(previous), metrics(current)
    changes = {}
    for key, value in new.items():
        reference = old.get(key)
        if isinstance(value, (int, float)) and isinstance(reference, (int, float)):
            changes[key] = {"current": value, "baseline": reference, "delta": value - reference,
                            "ratio": value / reference if reference > 0 else None,
                            "direction": "higher" if value > reference else "lower" if value < reference else "equal"}
    regressions = [key for key, change in changes.items() if change["direction"] == "higher" and key in {
        "wall_seconds_per_game_minute", "action_latency_seconds", "rss_bytes"}]
    return {"status": "compared", "comparison_tag": tag, "baseline_record_id": previous.get("record_id"),
            "current_record_id": current.get("record_id"), "metrics": changes,
            "measured_relative_increases": regressions,
            "note": "The tag asserts workload comparability; retain both bindings. Single intervals show measured differences, not statistical significance or a cause. High process CPU alone is not a regression."}
