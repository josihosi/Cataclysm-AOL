"""Opt-in, bounded memory telemetry for the live cockpit transport.

The profiler deliberately records scalar process facts only.  It is enabled
solely by ``OPENCLAW_COCKPIT_MEMORY_PROFILE_PATH`` and never retains a request,
response, archive row, or Python object graph in order to describe one.
"""
from __future__ import annotations

import gc
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import time
import tracemalloc
from typing import Any, Mapping


PROFILE_PATH_ENV = "OPENCLAW_COCKPIT_MEMORY_PROFILE_PATH"


def enabled() -> bool:
    return bool(os.environ.get(PROFILE_PATH_ENV, "").strip())


def _rss_bytes() -> int | None:
    """Read current RSS rather than treating ru_maxrss as live retention."""
    try:
        completed = subprocess.run(
            ["ps", "-o", "rss=", "-p", str(os.getpid())],
            check=False, capture_output=True, text=True,
        )
        value = completed.stdout.strip()
        return int(value) * 1024 if value.isdigit() else None
    except (OSError, ValueError):
        return None


def _archive_metrics(archive: Any) -> dict[str, Any]:
    if archive is None:
        return {}
    try:
        path = Path(archive.path)
        page_count = int(archive.connection.execute("PRAGMA page_count").fetchone()[0])
        page_size = int(archive.connection.execute("PRAGMA page_size").fetchone()[0])
        cache_size = int(archive.connection.execute("PRAGMA cache_size").fetchone()[0])
        return {
            "archive_bytes": path.stat().st_size if path.is_file() else 0,
            "sqlite_page_count": page_count,
            "sqlite_page_size": page_size,
            "sqlite_cache_size": cache_size,
        }
    except Exception:
        return {"archive_metrics": "unavailable"}


def snapshot(component: str, stage: str, *, archive: Any = None,
             response_bytes: int | None = None, extra: Mapping[str, Any] | None = None) -> None:
    """Append one scalar snapshot for a single lifecycle point."""
    raw_path = os.environ.get(PROFILE_PATH_ENV, "").strip()
    if not raw_path:
        return
    if not tracemalloc.is_tracing():
        tracemalloc.start(10)
    current, peak = tracemalloc.get_traced_memory()
    try:
        # macOS reports ru_maxrss in bytes; Linux reports KiB.
        high_water = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        if sys.platform != "darwin":
            high_water *= 1024
    except (AttributeError, ValueError):
        high_water = None
    payload: dict[str, Any] = {
        "schema": "caol-cockpit-memory-snapshot-v1",
        "component": component,
        "stage": stage,
        "pid": os.getpid(),
        "monotonic_seconds": time.monotonic(),
        "rss_bytes": _rss_bytes(),
        "rss_high_water_bytes": high_water,
        "tracemalloc_current_bytes": current,
        "tracemalloc_peak_bytes": peak,
        "gc_tracked_objects": len(gc.get_objects()),
        **_archive_metrics(archive),
    }
    if response_bytes is not None:
        payload["response_bytes"] = int(response_bytes)
    if extra:
        payload["extra"] = {str(key): value for key, value in extra.items()
                            if isinstance(value, (str, int, float, bool, type(None)))}
    path = Path(raw_path)
    target = path.with_name(path.stem + "." + component + path.suffix)
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n")


def released(component: str, stage: str, *, archive: Any = None,
             response_bytes: int | None = None) -> None:
    """Record pre/post-GC values without retaining the released references."""
    if not enabled():
        return
    snapshot(component, stage + "_pre_gc", archive=archive, response_bytes=response_bytes)
    gc.collect()
    snapshot(component, stage + "_post_gc", archive=archive, response_bytes=response_bytes)
