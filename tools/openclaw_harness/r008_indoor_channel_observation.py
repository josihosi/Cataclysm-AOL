"""Run-bound production channel observations for the R-008 indoor comparison.

The game producer writes one record for each channel considered by a local scan.
This module is deliberately small and read-only: it is the focused observation
surface, not a classifier or a source of gameplay state.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, Iterable, Mapping, Optional


R008_CHANNEL_RECORD_FILENAME = "r008.production.channels.jsonl"
R008_CHANNEL_SCHEMA = "caol-r008-production-channel-v1"
R008_CHANNELS = (
    "light",
    "smoke",
    "sound",
    "scent",
    "prior_knowledge",
    "incidental_contact",
)


def _contract_scenario(run_dir: Path) -> str:
    path = run_dir / "contract.preflight.json"
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return ""
    return str(payload.get("scenario", "")).strip() if isinstance(payload, Mapping) else ""


def _records(path: Path) -> Iterable[Mapping[str, Any]]:
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            yield {"_invalid": True}
            continue
        yield value if isinstance(value, Mapping) else {"_invalid": True}


def read_r008_indoor_channel_observation(
    run_dir: Path,
    *,
    run_id: str,
    source_sha256: str = "",
    executable_sha256: str = "",
    scenario_id: str = "",
    binding_id: str = "",
    observed_game_minutes: Optional[int] = None,
) -> Dict[str, Any]:
    """Read and validate the run-owned production channel stream.

    Every row must name the same run, source, executable, scenario and runtime
    binding, and must be explicitly fresh and isolated.  Missing or malformed
    rows are never silently dropped; the caller receives an ineligible result.
    """
    run_dir = Path(run_dir)
    path = run_dir / R008_CHANNEL_RECORD_FILENAME
    expected_run = str(run_id).strip()
    expected_scenario = str(scenario_id).strip() or _contract_scenario(run_dir)
    issues = []
    if not expected_run:
        issues.append("missing_run_id")
    if not str(source_sha256).strip():
        issues.append("missing_expected_runtime_source_sha256")
    if not str(executable_sha256).strip():
        issues.append("missing_expected_executable_sha256")
    if not expected_scenario:
        issues.append("missing_expected_scenario_id")
    if not str(binding_id).strip():
        issues.append("missing_expected_binding_id")
    if not path.is_file():
        issues.append("channel_records_missing")
        return {
            "schema": R008_CHANNEL_SCHEMA,
            "status": "unavailable",
            "eligible": False,
            "path": str(path),
            "records": [],
            "channels": [],
            "issues": issues,
        }

    records = []
    expected_sequence = 1
    scan_minutes = None
    for row in _records(path):
        if row.get("_invalid"):
            issues.append("malformed_record")
            continue
        required = ("schema", "sequence", "run_id", "binding", "scan", "scan_id", "channel",
                    "signal_origin", "consumer", "observed", "isolated")
        missing = [key for key in required if key not in row]
        if missing:
            issues.append("missing_fields:" + ",".join(missing))
            continue
        if row.get("schema") != R008_CHANNEL_SCHEMA:
            issues.append("wrong_schema")
        sequence = row.get("sequence")
        if isinstance(sequence, bool) or not isinstance(sequence, int) or sequence != expected_sequence:
            issues.append("stale_or_noncontiguous_sequence")
        expected_sequence = sequence + 1 if isinstance(sequence, int) else expected_sequence
        if str(row.get("run_id", "")) != expected_run:
            issues.append("wrong_run")
        if not str(row.get("scan_id", "")).strip() or not str(row.get("scan_id", "")).startswith(expected_run + ":"):
            issues.append("stale_or_wrong_scan")
        binding = row.get("binding")
        if not isinstance(binding, Mapping):
            issues.append("malformed_binding")
            binding = {}
        for key, expected in (("runtime_source_sha256", source_sha256),
                              ("executable_sha256", executable_sha256),
                              ("scenario_id", expected_scenario)):
            observed = str(binding.get(key, "")).strip()
            if not observed:
                issues.append("missing_" + key)
            elif expected and observed != str(expected):
                issues.append("wrong_" + key)
        observed_binding_id = str(binding.get("binding_id", "")).strip()
        if not observed_binding_id:
            issues.append("missing_binding_id")
        elif binding_id and observed_binding_id != str(binding_id):
            issues.append("wrong_binding_id")
        scan = row.get("scan")
        if not isinstance(scan, Mapping):
            issues.append("malformed_scan")
            scan = {}
        minutes = scan.get("game_minutes")
        if isinstance(minutes, bool) or not isinstance(minutes, int) or minutes < 0:
            issues.append("invalid_scan_time")
        elif scan_minutes is not None and minutes < scan_minutes:
            issues.append("stale_scan_time")
        else:
            scan_minutes = minutes
        if scan.get("fresh") is not True:
            issues.append("stale_scan")
        if scan.get("isolated") is not True or row.get("isolated") is not True:
            issues.append("unisolated_record")
        if not str(row.get("signal_origin", "")).strip():
            issues.append("missing_signal_origin")
        if not str(row.get("consumer", "")).strip():
            issues.append("missing_consumer")
        if row.get("channel") not in R008_CHANNELS:
            issues.append("unknown_channel")
        if not isinstance(row.get("observed"), bool):
            issues.append("invalid_observed_flag")
        records.append(dict(row))

    if observed_game_minutes is not None and scan_minutes is not None and scan_minutes > observed_game_minutes:
        issues.append("future_scan")
    channels = sorted({str(row.get("channel")) for row in records if row.get("channel") in R008_CHANNELS})
    missing_channels = sorted(set(R008_CHANNELS) - set(channels))
    if missing_channels:
        issues.append("missing_channels:" + ",".join(missing_channels))
    unique_issues = list(dict.fromkeys(issues))
    return {
        "schema": R008_CHANNEL_SCHEMA,
        "status": "green" if not unique_issues else "blocked",
        "eligible": not unique_issues,
        "path": str(path),
        "records": records,
        "channels": channels,
        "missing_channels": missing_channels,
        "issues": unique_issues,
        "record_count": len(records),
        "latest_game_minutes": scan_minutes,
    }


validate_r008_indoor_channel_observation = read_r008_indoor_channel_observation


__all__ = [
    "R008_CHANNEL_RECORD_FILENAME",
    "R008_CHANNEL_SCHEMA",
    "R008_CHANNELS",
    "read_r008_indoor_channel_observation",
    "validate_r008_indoor_channel_observation",
]
