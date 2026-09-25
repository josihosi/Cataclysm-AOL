"""Bound process measurements, retained independently of the game request pipe.

No background process, game input, or inferred gameplay outcomes.
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


TURN_TRACE_PREFIX = "openclaw_harness_semantic_step: "
TURN_CONFIG_NAME = "performance-turn-config.json"
TURN_STATE_NAME = "performance-turn-state.json"
TURN_WINDOW_LIMIT = 256
WAITING_WINDOW_TURNS = 100
WAITING_LIMIT_SECONDS = 0.100


def update_waiting_performance(state: dict, measurement: dict) -> tuple[dict, dict | None]:
    """Keep current waiting cost and emit only threshold crossings.

    Josef's selected alarm threshold is 100 milliseconds per game turn.
    Native simulation time excludes input, load, save and transport pauses.
    """
    generation = measurement.get("waiting_generation")
    if not generation:
        return {}, None
    identity = [measurement.get("native_process_instance"), generation]
    if state.get("identity") != identity:
        state = {"identity": identity, "seconds": [], "alarmed": False}
    if state.get("last_measurement") == measurement["measurement_id"]:
        return state, None
    seconds = (state["seconds"] + [measurement["simulation_seconds"]])[-WAITING_WINDOW_TURNS:]
    state = {**state, "seconds": seconds, "last_measurement": measurement["measurement_id"]}
    if len(seconds) < WAITING_WINDOW_TURNS:
        return state, None
    mean = math.fsum(seconds) / len(seconds)
    alarmed = mean > WAITING_LIMIT_SECONDS
    change = None
    if alarmed != state["alarmed"]:
        change = {"kind": "waiting_slow" if alarmed else "waiting_recovery",
                  "mean_seconds": mean, "sample_count": len(seconds),
                  "limit_seconds": WAITING_LIMIT_SECONDS,
                  "message": ("Waiting is too slow. Tell the coordinator: waiting performance needs attention."
                              if alarmed else "Waiting performance recovered.")}
    return {**state, "alarmed": alarmed, "mean_seconds": mean}, change


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return {}


def write_json(path: Path, value: dict):
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    temporary.write_text(json.dumps(value))
    os.replace(temporary, path)


def _finite_number(value: Any, *, positive: bool = False) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and \
        math.isfinite(value) and (value > 0 if positive else True)


def _turn_config(directory: Path) -> tuple[dict | None, str | None]:
    """Read an explicit, scenario-owned turn-alarm configuration.

    There are intentionally no fallback millisecond thresholds here.  A run
    without a scenario expectation is still measurable, but it is not silently
    judged against an empty-map or developer-machine default.
    """
    config = read_json(directory / TURN_CONFIG_NAME)
    required_numbers = ("spike_seconds", "slow_turn_seconds", "incomplete_turn_seconds",
                        "stalled_observation_seconds")
    if config.get("schema") != "caol-performance-turn-config-v1":
        return None, "missing_or_invalid_turn_configuration"
    if not isinstance(config.get("provenance"), str) or not config["provenance"].strip():
        return None, "turn_configuration_has_no_provenance"
    if any(not _finite_number(config.get(key), positive=True) for key in required_numbers):
        return None, "turn_configuration_needs_finite_positive_thresholds"
    if not isinstance(config.get("window_turns"), int) or isinstance(config["window_turns"], bool) \
            or not 1 <= config["window_turns"] <= TURN_WINDOW_LIMIT:
        return None, "turn_configuration_window_turns_out_of_range"
    if not _finite_number(config.get("tail_percentile")) or not 0 < config["tail_percentile"] <= 1:
        return None, "turn_configuration_tail_percentile_invalid"
    if not isinstance(config.get("sustained_turns"), int) or isinstance(config["sustained_turns"], bool) \
            or not 1 <= config["sustained_turns"] <= config["window_turns"]:
        return None, "turn_configuration_sustained_turns_invalid"
    if not isinstance(config.get("expected_progress"), bool):
        return None, "turn_configuration_expected_progress_invalid"
    return config, None


def _trace_path(directory: Path) -> Path | None:
    """Use only the launch-published, run-owned semantic trace."""
    owner = read_json(directory / "performance-owner.json")
    process = read_json(directory / "game-process.json")
    logs = process.get("log_paths") if isinstance(process.get("log_paths"), dict) else {}
    published = logs.get("native_semantic_events") if isinstance(logs.get("native_semantic_events"), dict) else {}
    candidate = Path(str(published.get("path", ""))) if published.get("path") else \
        directory / "semantic.native.events.jsonl"
    if not owner.get("run_id") or process and process.get("run_id") not in {None, owner.get("run_id")}:
        return None
    if published and (published.get("scope") not in {None, "run_bound"} or
                      process.get("binding_id") not in {None, owner.get("binding_id")}):
        return None
    # The producer's game-process receipt may point from the bridge session to
    # its separate launch-owned run directory.  Its binding/run checks above,
    # rather than a lexical path relation, are the authority boundary.
    return candidate


def _parse_turn_trace(path: Path, *, start_offset: int, run_id: str) -> tuple[list[dict], int, str | None]:
    """Incrementally parse complete native records without treating a hot tail as corruption."""
    try:
        size = path.stat().st_size
        if start_offset < 0 or start_offset > size:
            return [], 0, "trace_rewound_or_replaced"
        with path.open("rb") as stream:
            stream.seek(start_offset)
            data = stream.read()
    except OSError:
        return [], start_offset, "native_turn_trace_unavailable"
    complete = data.rsplit(b"\n", 1)
    if len(complete) == 1:
        return [], start_offset, None
    body, _partial = complete
    cursor = start_offset
    events: list[dict] = []
    for raw in body.splitlines(keepends=True):
        line = raw.rstrip(b"\r\n")
        marker = line.find(TURN_TRACE_PREFIX.encode("utf-8"))
        if marker >= 0:
            try:
                event = json.loads(line[marker + len(TURN_TRACE_PREFIX):])
            except (UnicodeDecodeError, json.JSONDecodeError):
                return [], start_offset, "malformed_native_turn_trace"
            if not isinstance(event, dict) or event.get("event") not in {
                    "turn", "turn_phase", "request_transport", "receipt", "surface_receipt",
                    "surface_descriptor"}:
                cursor += len(raw)
                continue
            if event.get("run_id") != run_id:
                return [], start_offset, "native_turn_trace_run_mismatch"
            event["_evidence_handle"] = {"path": str(path), "offset": cursor + marker,
                                         "end": cursor + len(raw)}
            events.append(event)
        cursor += len(raw)
    # ``body`` excludes the delimiter after its final complete record.  Advance
    # over it as well, otherwise the next idempotent poll re-reads that record.
    return events, start_offset + len(data) - len(_partial), None


def _percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, math.ceil(fraction * len(ordered)) - 1))
    return ordered[index]


def _finite_rate(numerator: Any, denominator: Any) -> float | None:
    """Return a rate only when both values are finite and the denominator is positive."""
    if not _finite_number(numerator) or not _finite_number(denominator, positive=True):
        return None
    return float(numerator) / float(denominator)


def _turn_identity(owner: Mapping[str, Any], event: Mapping[str, Any]) -> str:
    return ":".join(str(event.get(key, "")) for key in
                    ("run_id", "process_instance", "turn_id", "stage")) + \
        ":" + str(owner.get("process_identity", ""))


def _active_turn_identity(owner: Mapping[str, Any], event: Mapping[str, Any]) -> str:
    return ":".join(str(event.get(key, "")) for key in
                    ("run_id", "process_instance", "turn_id")) + ":" + \
        str(owner.get("process_identity", ""))


def _event_wall_seconds(event: Mapping[str, Any]) -> float | None:
    value = event.get("wall_time_seconds")
    if _finite_number(value):
        return float(value)
    # Existing semantic transport/receipt records predate turn tracing and
    # expose their producer clock in milliseconds.
    value = event.get("wall_time")
    if _finite_number(value):
        return float(value) / 1000.0
    return None


def _event_summary(event: Mapping[str, Any]) -> dict:
    return {"event": event.get("event"), "stage": event.get("stage"),
            "accepted": event.get("accepted"), "request_id": event.get("request_id"),
            "turn_id": event.get("turn_id"), "game_turn": event.get("game_turn"),
            "phase": event.get("phase"), "wall_time_seconds": _event_wall_seconds(event),
            "evidence": event.get("_evidence_handle")}


def _bound_resource_metric(directory: Path, owner: Mapping[str, Any]) -> dict:
    """Project the existing process-performance owner, never sample or write here."""
    record = read_json(directory / "performance.latest.json")
    if record.get("owner") != owner:
        return {"status": "unavailable", "reason": "no_current_bound_process_record"}
    resources = record.get("resources") if isinstance(record.get("resources"), Mapping) else {}
    wall = resources.get("interval_wall_seconds")
    cpu = resources.get("interval_cpu_seconds")
    if not _finite_number(wall) or not _finite_number(cpu):
        return {"status": "unavailable", "reason": "bound_process_record_has_no_wall_cpu_interval",
                "record_id": record.get("record_id")}
    return {"status": "available", "elapsed_wall_seconds": wall, "cpu_seconds": cpu,
            "record_id": record.get("record_id"), "journal": str(directory / "performance.jsonl"),
            "scope": "latest existing ProcessPerformance interval; it is reported separately from native turn wall time"}


def _owner_is_live(owner: Mapping[str, Any]) -> bool:
    """Refresh the PID/birth binding only when a stall decision needs it."""
    try:
        sample = sample_child_resources(int(owner["pid"]))
    except (OSError, TypeError, ValueError):
        return False
    return sample.get("pid") == owner.get("pid") and \
        sample.get("process_identity") == owner.get("process_identity")


def collect_turn_assessment(directory: Path, binding_id: str, *, pending: Mapping[str, Any] | None = None,
                            now_unix_seconds: float | None = None) -> dict:
    """Read newly published native turn boundaries and update one compact assessment.

    It is safe to call from every pending-operation poll: collection does not
    send input, alter pending ownership, or replay a response.  The complete
    trace is the journal; this state is just its bounded, resumable projection.
    """
    owner = read_json(directory / "performance-owner.json")
    if owner.get("binding_id") != binding_id or not owner.get("run_id") or not owner.get("process_identity"):
        return {"status": "unavailable", "reason": "bound_performance_owner_unavailable"}
    config, config_error = _turn_config(directory)
    window_turns = config["window_turns"] if config else TURN_WINDOW_LIMIT
    path = _trace_path(directory)
    if path is None:
        return {"status": "unavailable", "reason": "native_turn_trace_not_run_bound"}
    measurement_binding = {
        "source_binding": owner.get("source_binding"), "host": owner.get("host"),
        "machine": owner.get("machine"), "platform": owner.get("platform"),
        # The launcher/scenario can supply save, seed, actor/load counts,
        # renderer and warm-up facts here.  Absence is deliberately visible
        # rather than silently treating two arbitrary runs as comparable.
        "workload": config.get("workload") if config and isinstance(config.get("workload"), dict) else None,
    }
    baseline = config.get("baseline") if config else None
    if not isinstance(baseline, dict):
        baseline_assessment = {"status": "unavailable", "reason": "missing_baseline",
                               "measurement_binding": measurement_binding}
    elif baseline.get("binding") != measurement_binding:
        baseline_assessment = {"status": "incompatible", "reason": "baseline_binding_differs",
                               "measurement_binding": measurement_binding,
                               "baseline_reference": baseline.get("reference")}
    else:
        baseline_assessment = {"status": "comparable", "reference": baseline.get("reference"),
                               "measurement_binding": measurement_binding}
    state = read_json(directory / TURN_STATE_NAME)
    if state.get("owner") != owner:
        state = {"schema": "caol-performance-turn-state-v1", "owner": owner,
                 "trace_path": str(path), "offset": 0, "completed": [], "active": {},
                 "emitted": []}
    events, next_offset, trace_error = _parse_turn_trace(path, start_offset=int(state.get("offset", 0)),
                                                          run_id=str(owner["run_id"]))
    if trace_error == "trace_rewound_or_replaced":
        # A save/load or trace generation reset cannot inherit the old window.
        state.update(offset=0, completed=[], active={}, emitted=[], waiting={},
                     reset_reason="native_trace_rewound_or_replaced")
        events, next_offset, trace_error = _parse_turn_trace(path, start_offset=0,
                                                              run_id=str(owner["run_id"]))
    if trace_error:
        return {"status": "unavailable", "reason": trace_error,
                "trace_path": str(path), "native_receipts_preserved": True}

    completed = list(state.get("completed", []))[-window_turns:]
    waiting_state = state.get("waiting", {})
    waiting_changes = []
    active = dict(state.get("active", {}))
    added: list[dict] = []
    game_turn_rewound = False
    native_process_replaced = False
    native_process_instance = state.get("native_process_instance")
    last_source_event = state.get("last_source_event")
    last_accepted_receipt = state.get("last_accepted_receipt")
    for event in events:
        event_type = event.get("event")
        last_source_event = _event_summary(event)
        if event_type in {"receipt", "surface_receipt"} and event.get("accepted") is True:
            # A surface descriptor is a successor fact, not a receipt.  Keep
            # the accepted receipt separately so a descriptor published after
            # it cannot erase the operation's acceptance from this poll (or a
            # later poll that only contains the descriptor).
            last_accepted_receipt = last_source_event
        if event_type not in {"turn", "turn_phase"}:
            continue
        event_process_instance = event.get("process_instance")
        if not isinstance(event_process_instance, str) or not event_process_instance:
            return {"status": "unavailable", "reason": "native_turn_trace_process_identity_missing",
                    "trace_path": str(path), "native_receipts_preserved": True}
        if native_process_instance is None:
            native_process_instance = event_process_instance
        elif native_process_instance != event_process_instance:
            completed = []
            waiting_state = {}
            active = {}
            native_process_instance = event_process_instance
            native_process_replaced = True
        active_identity = _active_turn_identity(owner, event)
        if event_type == "turn_phase":
            if active_identity in active:
                current_active = dict(active[active_identity])
                phase_wall = _event_wall_seconds(event)
                if event.get("stage") == "begin":
                    current_active.update(phase=event.get("phase"),
                                          phase_evidence=event.get("_evidence_handle"),
                                          outside_simulation_started_wall_time_seconds=phase_wall)
                elif event.get("stage") == "end":
                    outside_started = current_active.get("outside_simulation_started_wall_time_seconds")
                    if _finite_number(outside_started) and _finite_number(phase_wall):
                        current_active["outside_simulation_seconds"] = max(0.0, float(
                            current_active.get("outside_simulation_seconds", 0.0))) + max(
                                0.0, phase_wall - outside_started)
                    current_active.update(phase=event.get("phase", "simulation"),
                                          phase_evidence=event.get("_evidence_handle"))
                    current_active.pop("outside_simulation_started_wall_time_seconds", None)
                active[active_identity] = current_active
            continue
        stage = event.get("stage")
        identity = _turn_identity(owner, event)
        if stage == "start":
            active[active_identity] = {"start": event, "phase": event.get("phase", "simulation"),
                                       "phase_evidence": event.get("_evidence_handle")}
        elif stage == "end":
            active_turn = active.pop(active_identity, None)
            start = active_turn.get("start") if isinstance(active_turn, Mapping) else None
            duration = event.get("simulation_seconds")
            if start is not None and _finite_number(duration) and duration >= 0:
                if completed and isinstance(event.get("game_turn"), int) and \
                        isinstance(completed[-1].get("game_turn"), int) and \
                        event["game_turn"] < completed[-1]["game_turn"]:
                    completed = []
                    waiting_state = {}
                    game_turn_rewound = True
                measurement = {"measurement_id": identity, "run_id": owner["run_id"],
                               "process_identity": owner["process_identity"],
                               "game_turn": event.get("game_turn"), "game_minutes": event.get("game_minutes"),
                               "simulation_seconds": duration, "start": start.get("_evidence_handle"),
                               "end": event.get("_evidence_handle"), "owner": event.get("owner"),
                               "phase": event.get("phase"), "sequence": event.get("sequence"),
                               "waiting_generation": start.get("waiting_generation"),
                               "native_process_instance": event_process_instance,
                               "start_wall_time_seconds": _event_wall_seconds(start),
                               "end_wall_time_seconds": _event_wall_seconds(event)}
                if not any(item.get("measurement_id") == identity for item in completed):
                    completed.append(measurement)
                    added.append(measurement)
                    waiting_state, waiting_change = update_waiting_performance(waiting_state, measurement)
                    if waiting_change:
                        waiting_changes.append(waiting_change)
    completed = completed[-window_turns:]
    values = [item["simulation_seconds"] for item in completed if _finite_number(item.get("simulation_seconds"))]
    native_wall_seconds = None
    if completed and _finite_number(completed[0].get("start_wall_time_seconds")) and \
            _finite_number(completed[-1].get("end_wall_time_seconds")):
        native_wall_seconds = max(0.0, completed[-1]["end_wall_time_seconds"] -
                                  completed[0]["start_wall_time_seconds"])
    completed_turns = len(completed)
    resource_metric = _bound_resource_metric(directory, owner)
    simulation_elapsed_seconds = sum(values) if values else None
    game_minutes_advanced = (completed[-1].get("game_minutes", 0) - completed[0].get("game_minutes", 0)) \
        if len(completed) > 1 and all(isinstance(item.get("game_minutes"), int) for item in completed) else None
    metric = {"sample_count": len(values), "window_turns": window_turns,
              "window_first": completed[0].get("measurement_id") if completed else None,
              "window_last": completed[-1].get("measurement_id") if completed else None,
              "mean_seconds": sum(values) / len(values) if values else None,
              "tail_percentile": config["tail_percentile"] if config else None,
              "tail_seconds": _percentile(values, config["tail_percentile"]) if config and values else None,
              "max_seconds": max(values) if values else None,
              "simulation_elapsed_seconds": simulation_elapsed_seconds,
              "elapsed_wall_seconds": native_wall_seconds,
              "elapsed_wall_scope": "native_turn_window_including_inter_turn_input_gaps",
              "cpu_seconds": resource_metric.get("cpu_seconds"),
              "cpu_scope": resource_metric.get("scope"),
              "cpu_evidence": {key: resource_metric.get(key) for key in ("record_id", "journal")
                               if resource_metric.get(key) is not None},
              "completed_turns": completed_turns,
              "game_turns_advanced": completed_turns,
              # This wall-span rate is retained as an operator/input timing
              # signal. It includes inter-turn input, pause, transport and
              # other gaps, so it is not simulation throughput.
              "game_turns_per_wall_second": _finite_rate(completed_turns, native_wall_seconds),
              "game_turns_per_wall_second_scope": "window_wall_span_including_inter_turn_input_gaps_not_simulation_throughput",
              "game_minutes_advanced": game_minutes_advanced,
              "game_minutes_per_wall_second": _finite_rate(game_minutes_advanced, native_wall_seconds),
              "game_minutes_per_wall_second_scope": "window_wall_span_including_inter_turn_input_gaps_not_simulation_throughput",
              # Simulation boundaries are the comparable advancing-work
              # denominator; input gaps never enter this rate.
              "game_turns_per_simulation_second": _finite_rate(completed_turns, simulation_elapsed_seconds),
              "game_minutes_per_simulation_second": _finite_rate(game_minutes_advanced,
                                                                  simulation_elapsed_seconds),
              "simulation_throughput_scope": "completed_native_turn_simulation_seconds_only"}
    alarms: list[dict] = []
    for measurement in added:
        if measurement.get("waiting_generation"):
            continue
        if config and measurement["simulation_seconds"] > config["spike_seconds"]:
            alarms.append({"kind": "spike", "key": "spike:" + measurement["measurement_id"],
                           "observed": measurement["simulation_seconds"], "expected": config["spike_seconds"],
                           "evidence": measurement["end"]})
        if config and measurement["simulation_seconds"] > config["slow_turn_seconds"]:
            alarms.append({"kind": "slow_turn", "key": "slow:" + measurement["measurement_id"],
                           "observed": measurement["simulation_seconds"], "expected": config["slow_turn_seconds"],
                           "evidence": measurement["end"]})
    tail = completed[-config["sustained_turns"]:] if config else []
    if config and len(tail) == config["sustained_turns"] and all(not item.get("waiting_generation") and item["simulation_seconds"] > config["slow_turn_seconds"] for item in tail):
        alarms.append({"kind": "sustained_regression", "key": "sustained:" + str(tail[-1]["measurement_id"]),
                       "observed": [item["simulation_seconds"] for item in tail],
                       "expected": config["slow_turn_seconds"], "evidence": tail[-1]["end"]})
    current = max(active.values(), key=lambda value: _event_wall_seconds(value.get("start", {})) or 0,
                  default=None)
    now = time.time() if now_unix_seconds is None else now_unix_seconds
    operation = dict(pending) if pending else None
    operation_key = str(operation.get("request_id", "")) if operation else ""
    pending_observation = state.get("pending_observation") if isinstance(state.get("pending_observation"), dict) else {}
    if operation_key and pending_observation.get("request_id") != operation_key:
        submitted = operation.get("submitted_unix_seconds")
        pending_observation = {"request_id": operation_key,
                               "observed_unix_seconds": submitted if _finite_number(submitted) else now,
                               "first_trace_offset": next_offset}
    elif not operation_key:
        pending_observation = {}
    # A request can be accepted and then hand ownership to a native menu or
    # prompt before the advancing work is selected.  Retain the descriptor
    # as an owner fact so the no-start check does not call an input wait a
    # simulation stall.  This is deliberately keyed to the native surface
    # owner, never to rendered/prose text.
    surface_descriptor = state.get("last_surface_descriptor")
    for event in events:
        if event.get("event") == "surface_descriptor":
            surface_descriptor = {key: event.get(key) for key in
                                  ("kind", "surface_id", "frame_id", "breadcrumbs")}
            surface_descriptor["wall_time_seconds"] = _event_wall_seconds(event)
    state["last_surface_descriptor"] = surface_descriptor
    active_age = None
    observation_uncertainty = None
    current_start = current.get("start") if isinstance(current, Mapping) else None
    current_phase = str(current.get("phase", "unknown")) if isinstance(current, Mapping) else None
    if current_start and _finite_number(current_start.get("wall_time_seconds")):
        outside_simulation_seconds = max(0.0, float(current.get("outside_simulation_seconds", 0.0))) \
            if isinstance(current, Mapping) and _finite_number(current.get("outside_simulation_seconds", 0.0)) else 0.0
        active_age = max(0.0, now - current_start["wall_time_seconds"] - outside_simulation_seconds)
        if config and current_phase == "simulation" and active_age > config["incomplete_turn_seconds"]:
            alarms.append({"kind": "incomplete_turn", "key": "incomplete:" + _turn_identity(owner, current_start),
                           "observed": active_age, "expected": config["incomplete_turn_seconds"],
                           "evidence": current_start.get("_evidence_handle")})
        if config and config["expected_progress"] and pending and current_phase == "simulation" and \
                active_age > config["stalled_observation_seconds"]:
            if _owner_is_live(owner):
                alarms.append({"kind": "stalled_progress", "key": "stalled:" + _turn_identity(owner, current_start),
                               "observed": active_age, "expected": config["stalled_observation_seconds"],
                               "evidence": current_start.get("_evidence_handle"),
                               "progress_state": "incomplete_native_turn"})
            else:
                observation_uncertainty = "owned_process_not_live_or_pid_reused"
    elif config and config["expected_progress"] and operation_key:
        pending_age = max(0.0, now - float(pending_observation["observed_unix_seconds"]))
        source_time = last_source_event.get("wall_time_seconds") if isinstance(last_source_event, Mapping) else None
        source_is_fresh = _finite_number(source_time) and source_time >= pending_observation["observed_unix_seconds"]
        if pending_age > config["stalled_observation_seconds"]:
            if not source_is_fresh:
                observation_uncertainty = "no_fresh_native_facts_or_transport_delay"
            elif last_source_event.get("event") == "request_transport":
                observation_uncertainty = "native_transport_seen_but_not_accepted"
            else:
                accepted_time = last_accepted_receipt.get("wall_time_seconds") \
                    if isinstance(last_accepted_receipt, Mapping) else None
                surface_time = surface_descriptor.get("wall_time_seconds") \
                    if isinstance(surface_descriptor, Mapping) else None
                surface_kind = str(surface_descriptor.get("kind", "")).strip().lower() \
                    if isinstance(surface_descriptor, Mapping) else ""
                accepted_fresh = _finite_number(accepted_time) and \
                    accepted_time >= pending_observation["observed_unix_seconds"]
                surface_fresh = _finite_number(surface_time) and \
                    surface_time >= pending_observation["observed_unix_seconds"]
                if accepted_fresh and surface_fresh and surface_kind in {"menu", "prompt", "input"}:
                    observation_uncertainty = "native_input_owner_no_simulation_progress"
                elif last_source_event.get("event") in {"receipt", "surface_receipt"} and \
                        last_source_event.get("accepted") is True:
                    if _owner_is_live(owner):
                        alarms.append({"kind": "stalled_progress", "key": "stalled_no_turn_start:" + operation_key,
                                       "observed": pending_age, "expected": config["stalled_observation_seconds"],
                                       "evidence": last_source_event.get("evidence"),
                                       "progress_state": "accepted_operation_no_new_turn"})
                    else:
                        observation_uncertainty = "owned_process_not_live_or_pid_reused"
                else:
                    observation_uncertainty = "fresh_native_event_has_no_turn_progress_fact"
    state_reset = game_turn_rewound or native_process_replaced
    emitted = set() if state_reset else set(state.get("emitted", []))
    changed = [alarm for alarm in alarms if alarm["key"] not in emitted]
    previous_active = set() if state_reset else set(state.get("active_alarm_keys", []))
    current_active = {alarm["key"] for alarm in alarms}
    recoveries = [{"kind": "recovery", "recovered_alarm": key} for key in previous_active - current_active]
    for alarm in alarms:
        alarm.update(run_id=owner["run_id"], pid=owner["pid"], process_identity=owner["process_identity"],
                     operation=operation, window={key: metric[key] for key in
                                                   ("sample_count", "window_turns", "window_first", "window_last")},
                     configuration_reference=config.get("reference") if config else None,
                     configuration_provenance=config["provenance"] if config else None)
    state.update(offset=next_offset, completed=completed, active=active, emitted=list((emitted | current_active))[-512:],
                 waiting=waiting_state,
                 active_alarm_keys=sorted(current_active), last_source_event=last_source_event,
                 pending_observation=pending_observation,
                 native_process_instance=native_process_instance,
                 last_accepted_receipt=last_accepted_receipt, updated_unix_seconds=now)
    if game_turn_rewound:
        state["reset_reason"] = "native_game_turn_rewound_or_loaded"
    elif native_process_replaced:
        state["reset_reason"] = "native_process_instance_changed"
    assessment = {"status": "measured" if config else "measured_unassessed", "owner": owner, "configuration": {
        "path": str(directory / TURN_CONFIG_NAME), "provenance": config["provenance"] if config else None,
        "reference": config.get("reference") if config else None, "status": "available" if config else "unavailable",
        "reason": config_error}, "trace": {"path": str(path), "offset": next_offset},
        "baseline": baseline_assessment,
        "metric": metric, "current_incomplete_turn": {"event": current_start, "age_seconds": active_age,
        "phase": current_phase, "phase_evidence": current.get("phase_evidence")} if current else None,
        "source_freshness": {"last_event": last_source_event, "pending_observation": pending_observation},
        "alarms": changed + [item for item in waiting_changes if item["kind"] == "waiting_slow"],
        "recoveries": recoveries + [item for item in waiting_changes if item["kind"] == "waiting_recovery"],
        "waiting": {key: value for key, value in waiting_state.items() if key not in {"seconds", "identity", "last_measurement"}},
        "pending_operation": operation, "observation_uncertainty": observation_uncertainty,
        "note": "Native turn boundaries measure simulation only. Input, pause, load, save and transport phases do not create simulation alarms."}
    state["assessment"] = assessment
    try:
        write_json(directory / TURN_STATE_NAME, state)
    except OSError as error:
        return {"status": "unavailable", "reason": "turn_assessment_journal_failure",
                "detail": str(error), "native_receipts_preserved": True}
    return assessment


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
