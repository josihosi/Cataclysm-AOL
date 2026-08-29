#!/usr/bin/env python3
"""Focused R-009 observations for integrated waits and platform witnesses.

The game owns its transition stream.  This module only records the direct
facts around an existing wait action.  It never turns wall time, OCR, or a
missing host metric into product progress.
"""

from __future__ import annotations

import os
import platform as host_platform_module
import json
import subprocess
import time
from typing import Any, Callable, Mapping, Optional, Sequence


SEMANTIC_CONTRACT = "r009-integrated-wait-v1"
FOCUSED_EVIDENCE_CLASS = "focused-qualification"


def host_platform() -> str:
    """Return the supported-platform name used in witness records."""
    system = host_platform_module.system().lower()
    if system == "darwin":
        return "macos"
    if system == "windows":
        return "windows"
    if system == "linux":
        release = host_platform_module.release().lower()
        return "linux-wsl" if "microsoft" in release or "wsl" in release else "linux"
    return "unavailable"


def unavailable_metric(reason: str) -> dict[str, Any]:
    return {"status": "unavailable", "value": None, "reason": reason}


def available_metric(value: float | int, unit: str, source: str) -> dict[str, Any]:
    return {"status": "available", "value": value, "unit": unit, "source": source}


def _linux_resource_sample(pid: int) -> dict[str, Any]:
    stat_path = f"/proc/{pid}/stat"
    statm_path = f"/proc/{pid}/statm"
    try:
        stat_fields = open(stat_path, encoding="utf-8").read().split()
        cpu_ticks = int(stat_fields[13]) + int(stat_fields[14])
    except (OSError, IndexError, ValueError):
        cpu_ticks = None
    try:
        resident_pages = int(open(statm_path, encoding="utf-8").read().split()[1])
        resident_bytes = resident_pages * os.sysconf("SC_PAGE_SIZE")
    except (OSError, IndexError, ValueError):
        resident_bytes = None
    return {
        "cpu_ticks": cpu_ticks,
        "resident_memory": (
            available_metric(resident_bytes, "bytes", "/proc/<pid>/statm")
            if resident_bytes is not None else
            unavailable_metric("the Linux resident-memory source was not readable")
        ),
    }


def _macos_resource_sample(pid: int) -> dict[str, Any]:
    try:
        result = subprocess.run(
            ["ps", "-o", "%cpu=", "-o", "rss=", "-p", str(pid)],
            capture_output=True, text=True, check=False,
        )
        fields = result.stdout.split()
        if result.returncode != 0 or len(fields) < 2:
            raise ValueError("ps did not return a process row")
        cpu_percent = float(fields[0])
        resident_bytes = int(fields[1]) * 1024
    except (OSError, ValueError):
        return {
            "cpu_percent": unavailable_metric("the macOS ps process sample was not available"),
            "resident_memory": unavailable_metric("the macOS ps resident-memory sample was not available"),
        }
    return {
        "cpu_percent": available_metric(cpu_percent, "percent", "ps %cpu"),
        "resident_memory": available_metric(resident_bytes, "bytes", "ps rss"),
    }


def _windows_resource_sample(pid: int) -> dict[str, Any]:
    command = (
        f"Get-Process -Id {pid} -ErrorAction Stop | "
        "Select-Object CPU,WorkingSet64 | ConvertTo-Json -Compress"
    )
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", command],
            capture_output=True, text=True, check=False,
        )
        fields = json.loads(result.stdout)
        if result.returncode != 0 or not isinstance(fields, dict):
            raise ValueError("PowerShell did not return a process row")
        cpu_seconds = float(fields["CPU"])
        resident_bytes = int(fields["WorkingSet64"])
    except (KeyError, OSError, TypeError, ValueError):
        return {
            "cpu_seconds": None,
            "cpu_percent": unavailable_metric("the Windows process sample was not available"),
            "resident_memory": unavailable_metric("the Windows resident-memory sample was not available"),
        }
    return {
        "cpu_seconds": cpu_seconds,
        "cpu_percent": unavailable_metric("a Windows CPU interval needs two direct samples"),
        "resident_memory": available_metric(resident_bytes, "bytes", "Get-Process WorkingSet64"),
    }


def sample_child_resources(
    pid: int,
    *,
    platform_name: Optional[str] = None,
    monotonic_seconds: Optional[float] = None,
) -> dict[str, Any]:
    """Sample the child without inventing a numeric metric on unsupported hosts."""
    platform_name = platform_name or host_platform()
    sampled_at = time.monotonic() if monotonic_seconds is None else monotonic_seconds
    sample: dict[str, Any] = {
        "pid": int(pid),
        "platform": platform_name,
        "sampled_monotonic_seconds": sampled_at,
        "cpu_percent": unavailable_metric("a comparable CPU sample is not available yet"),
        "resident_memory": unavailable_metric("the host did not expose resident memory"),
    }
    if platform_name in {"linux", "linux-wsl"}:
        linux = _linux_resource_sample(pid)
        sample["cpu_ticks"] = linux["cpu_ticks"]
        sample["resident_memory"] = linux["resident_memory"]
        if linux["cpu_ticks"] is None:
            sample["cpu_percent"] = unavailable_metric("the Linux CPU tick source was not readable")
        return sample
    if platform_name == "macos":
        sample.update(_macos_resource_sample(pid))
        return sample
    if platform_name == "windows":
        sample.update(_windows_resource_sample(pid))
        return sample
    sample["cpu_percent"] = unavailable_metric("this host is outside the supported platform witness set")
    return sample


def complete_child_resource_interval(
    before: Mapping[str, Any], after: Mapping[str, Any], *,
    clock_ticks_per_second: Optional[int] = None,
) -> dict[str, Any]:
    """Attach an interval CPU value only when two direct samples exist."""
    completed = dict(after)
    platform_name = str(after.get("platform", ""))
    if platform_name == "macos":
        return completed
    counter_name = "cpu_ticks" if platform_name in {"linux", "linux-wsl"} else "cpu_seconds"
    before_counter = before.get(counter_name)
    after_counter = after.get(counter_name)
    elapsed = float(after.get("sampled_monotonic_seconds", 0.0)) - float(
        before.get("sampled_monotonic_seconds", 0.0)
    )
    if platform_name in {"linux", "linux-wsl"} and clock_ticks_per_second is None:
        try:
            clock_ticks_per_second = int(os.sysconf("SC_CLK_TCK"))
        except (ValueError, OSError, AttributeError):
            clock_ticks_per_second = None
    if not isinstance(before_counter, (int, float)) or not isinstance(after_counter, (int, float)) or elapsed <= 0:
        completed["cpu_percent"] = unavailable_metric("two readable child CPU samples were not available")
        return completed
    if platform_name in {"linux", "linux-wsl"} and not clock_ticks_per_second:
        completed["cpu_percent"] = unavailable_metric("the Linux clock tick rate was not available")
        return completed
    cpu_seconds = (
        (after_counter - before_counter) / clock_ticks_per_second
        if platform_name in {"linux", "linux-wsl"} else after_counter - before_counter
    )
    completed["cpu_percent"] = available_metric(
        cpu_seconds * 100.0 / elapsed,
        "percent", "/proc/<pid>/stat interval" if platform_name in {"linux", "linux-wsl"} else "Get-Process CPU interval",
    )
    return completed


def _event_brief(event: Mapping[str, Any]) -> dict[str, Any]:
    return {
        key: event.get(key)
        for key in ("sequence", "game_minutes", "domain", "transition", "outcome", "site_id", "operation_id")
        if event.get(key) not in (None, "")
    }


def summarize_repeated_events(events: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    """Represent a repeated transition by its count and causal endpoints."""
    grouped: dict[tuple[Any, ...], list[Mapping[str, Any]]] = {}
    for event in events:
        key = tuple(event.get(name) for name in ("domain", "transition", "outcome", "site_id", "operation_id"))
        grouped.setdefault(key, []).append(event)
    return [
        {
            "transition": _event_brief(values[-1]),
            "count": len(values),
            "first_sequence": values[0].get("sequence"),
            "last_sequence": values[-1].get("sequence"),
        }
        for values in grouped.values()
        if len(values) > 1
    ]


def observe_integrated_wait(
    *,
    label: str,
    before_events: Sequence[Mapping[str, Any]],
    after_events: Sequence[Mapping[str, Any]],
    resources_before: Mapping[str, Any],
    resources_after: Mapping[str, Any],
    wait_completion: Mapping[str, Any],
) -> dict[str, Any]:
    """Record semantic game-time progress and the latest causal transition."""
    before_game_minutes = before_events[-1].get("game_minutes") if before_events else None
    after_game_minutes = after_events[-1].get("game_minutes") if after_events else None
    new_events = list(after_events[len(before_events):])
    if isinstance(before_game_minutes, (int, float)) and isinstance(after_game_minutes, (int, float)):
        game_time = {
            "status": "advancing" if after_game_minutes > before_game_minutes else "stalled",
            "before_minutes": before_game_minutes,
            "after_minutes": after_game_minutes,
            "delta_minutes": after_game_minutes - before_game_minutes,
            "source": "native_transition_stream",
        }
    else:
        game_time = {
            "status": "unavailable", "before_minutes": before_game_minutes,
            "after_minutes": after_game_minutes,
            "delta_minutes": None, "source": "native_transition_stream",
            "reason": "the native transition stream did not expose both game-time endpoints",
        }
    latest_transition: dict[str, Any]
    if new_events:
        latest_transition = {"status": "available", "value": _event_brief(new_events[-1])}
    else:
        latest_transition = unavailable_metric("the wait produced no new native transition")
    return {
        "schema": SEMANTIC_CONTRACT,
        "label": label,
        "wait_completion": {
            "status": str(wait_completion.get("status", "unavailable")),
            "verdict": str(wait_completion.get("verdict", "")),
        },
        "product_game_time": game_time,
        "latest_transition": latest_transition,
        "transition_events_observed": len(new_events),
        "repeated_transition_summary": summarize_repeated_events(new_events),
        "child_resources_before": dict(resources_before),
        "child_resources_after": dict(resources_after),
        "evidence_class": FOCUSED_EVIDENCE_CLASS,
        "continuous_final_certification_credit": 0,
        "summary": "This wait records game-time progress and process resources from direct sources.",
    }


def technical_witness(
    *, platform_name: str, build_runtime_binding: Mapping[str, Any], route: Mapping[str, Any],
    direct_result: Mapping[str, Any], limitation: str,
) -> dict[str, Any]:
    """Describe one platform's focused technical result without certification credit."""
    return {
        "schema": "r009-platform-technical-witness-v1",
        "platform": platform_name,
        "semantic_contract": SEMANTIC_CONTRACT,
        "build_runtime_binding": dict(build_runtime_binding),
        "exercised_route": dict(route),
        "direct_result": dict(direct_result),
        "platform_limitation": limitation,
        "evidence_class": FOCUSED_EVIDENCE_CLASS,
        "continuous_final_certification_credit": 0,
        "summary": "This is focused technical evidence and it earns no continuous certification credit.",
    }
