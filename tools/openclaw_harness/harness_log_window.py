"""Bound noisy, disposable logs for a live harness session.

The bridge calls this only between completed native requests.  It keeps the
same inode so Cataclysm's already-open debug.log stream can keep appending.
Receipts, transitions, and saved worlds are deliberately outside this window.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import stat
from typing import Any


MAX_LOG_BYTES = 24 * 1024 * 1024
KEEP_LOG_BYTES = 12 * 1024 * 1024
WINDOWED_LOGS = {
    "native_semantic_events": "semantic.native.events.jsonl",
    "profile_diagnostic_debug": "debug.log",
}
_DECISION_TRACE_EVENTS = frozenset({
    "raid_trace_scope", "raid_actor_action", "raid_site_search",
    "raid_trace_repeat", "raid_trace_truncated", "raid_actor_damage",
    "raid_actor_death", "raid_actor_sleep",
})


def _pinned_trace_rows(stream, cutoff: int, budget: int,
                       run_id: str | None) -> tuple[bytes, int, int, bool]:
    """Keep combat edges and their action bases ahead of generic trace rows."""
    candidates: list[tuple[int, int, bytes]] = []
    stream.seek(0)
    while stream.tell() < cutoff:
        raw = stream.readline(cutoff - stream.tell())
        if not raw:
            break
        if b"raid_" not in raw or not raw.endswith(b"\n"):
            continue
        try:
            event = json.loads(raw).get("event")
        except (ValueError, AttributeError):
            continue
        if event not in _DECISION_TRACE_EVENTS:
            continue
        priority = (0 if event in {"raid_actor_damage", "raid_actor_death",
                                   "raid_actor_sleep", "raid_trace_truncated"}
                    else 1 if event in {"raid_trace_scope", "raid_actor_action"}
                    else 2 if event == "raid_trace_repeat" else 3)
        candidates.append((priority, len(candidates), raw))
    selected: list[tuple[int, int, bytes]] = []
    used = 0
    for candidate in sorted(candidates):
        if used + len(candidate[2]) <= budget:
            selected.append(candidate)
            used += len(candidate[2])
    dropped = len(candidates) - len(selected)
    marker_written = False
    marker = (json.dumps({"event": "raid_trace_truncated", "run_id": run_id,
                          "scope": "window", "unpreserved_rows": dropped},
                         separators=(",", ":")).encode() + b"\n")
    if dropped:
        while selected and used + len(marker) > budget:
            removed = max(selected)
            selected.remove(removed)
            used -= len(removed[2])
            dropped += 1
            marker = (json.dumps({"event": "raid_trace_truncated", "run_id": run_id,
                                  "scope": "window", "unpreserved_rows": dropped},
                                 separators=(",", ":")).encode() + b"\n")
        marker_written = used + len(marker) <= budget
    pinned = b"".join(row for _, _, row in sorted(selected, key=lambda row: row[1]))
    if marker_written:
        pinned += marker
    return pinned, len(selected), dropped, marker_written


def roll_complete_lines(path: Path, *, maximum: int = MAX_LOG_BYTES,
                        keep: int = KEEP_LOG_BYTES,
                        preserve_decisions: bool = False,
                        trace_run_id: str | None = None) -> dict[str, Any] | None:
    """Retain a complete-line suffix in-place when a harness log gets large."""
    if keep <= 0 or maximum <= keep or path.is_symlink():
        return None
    with path.open("r+b", buffering=0) as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size <= maximum:
            return None
        stream.seek(max(0, before.st_size - keep - 1))
        suffix = stream.read()
        boundary = suffix.find(b"\n")
        if boundary < 0:
            return None
        retained = suffix[boundary + 1:]
        pinned = b""
        preserved_rows = unpreserved_rows = 0
        marker_written = False
        if preserve_decisions:
            cutoff = before.st_size - len(suffix) + boundary + 1
            pinned, preserved_rows, unpreserved_rows, marker_written = _pinned_trace_rows(
                stream, cutoff, maximum - keep, trace_run_id)
        retained = pinned + retained
        # Keep a trailing partial record too.  The producer may finish it after
        # this response boundary; removing its prefix would corrupt the line.
        stream.seek(0)
        stream.write(retained)
        stream.truncate()
        os.fsync(stream.fileno())
        result = {"path": str(path), "before_bytes": before.st_size,
                  "after_bytes": len(retained), "discarded_bytes": before.st_size - len(retained)}
        if preserve_decisions:
            result.update(preserved_trace_rows=preserved_rows,
                          preserved_trace_bytes=len(pinned),
                          trace_preservation_incomplete=bool(unpreserved_rows),
                          unpreserved_trace_rows=unpreserved_rows,
                          trace_truncation_marker_written=marker_written)
        return result


def roll_bound_session_logs(session_dir: Path, run_id: str, *,
                            maximum: int = MAX_LOG_BYTES,
                            keep: int = KEEP_LOG_BYTES) -> list[dict[str, Any]]:
    """Use the launch-owned path manifest; never inspect a normal game profile."""
    try:
        process = json.loads((session_dir / "game-process.json").read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return []
    if not run_id or process.get("run_id") != run_id:
        return []
    log_paths = process.get("log_paths", {})
    if not isinstance(log_paths, dict):
        return []
    results = []
    for key, basename in WINDOWED_LOGS.items():
        declared = log_paths.get(key, {})
        if not isinstance(declared, dict):
            continue
        path = Path(str(declared.get("path", "")))
        expected_scope = "run_bound" if key == "native_semantic_events" else "profile_shared"
        if path.name != basename or declared.get("scope") != expected_scope or not path.is_file():
            continue
        try:
            result = roll_complete_lines(path, maximum=maximum, keep=keep,
                                         preserve_decisions=key == "native_semantic_events",
                                         trace_run_id=run_id)
        except OSError:
            continue
        if result is not None:
            results.append({"run_id": run_id, "stream": key, **result})
    if results:
        try:
            with (session_dir / "log-window.events.jsonl").open("a", encoding="utf-8") as stream:
                for result in results:
                    stream.write(json.dumps(result, separators=(",", ":")) + "\n")
        except OSError:
            pass
    return results
