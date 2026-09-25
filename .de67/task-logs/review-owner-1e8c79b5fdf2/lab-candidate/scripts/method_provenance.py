#!/usr/bin/env python3
"""Report DE67 method and workspace provenance without changing either."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import sqlite3
import subprocess
import sys
from typing import Any
from urllib.parse import quote

import mutation_guard
SCRIPT_ROOT = str(Path(__file__).resolve().parent)
if SCRIPT_ROOT not in sys.path:
    sys.path.insert(0, SCRIPT_ROOT)
from specification import SpecificationError, resolve


def _git_state(path: Path) -> dict[str, Any]:
    def run(*arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", "-C", str(path), *arguments],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )

    head = run("rev-parse", "HEAD")
    root = run("rev-parse", "--show-toplevel")
    if head.returncode or root.returncode:
        return {"available": False, "head": None, "root": None, "uncheckpointed": None}
    status = run("status", "--porcelain", "--untracked-files=all", "--", ".")
    lines = status.stdout.splitlines() if status.returncode == 0 else []
    return {
        "available": True,
        "head": head.stdout.strip(),
        "root": root.stdout.strip(),
        "uncheckpointed": bool(lines) if status.returncode == 0 else None,
        "changed_entries": lines,
    }


def _sha256(path: Path) -> str | None:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def _clock_state(path: Path) -> dict[str, Any]:
    if not path.is_file():
        return {"available": False, "restart_generation": None, "mutation_receipts": None}
    uri = f"file:{quote(str(path.resolve()).replace(os.sep, '/'), safe='/:')}?mode=ro"
    connection = sqlite3.connect(uri, uri=True, timeout=0)
    try:
        tables = {
            row[0] for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table'"
            )
        }
        generation = None
        if "coordinator_restart_requests" in tables:
            row = connection.execute(
                "SELECT MAX(generation) FROM coordinator_restart_requests"
            ).fetchone()
            generation = row[0] if row else None
        receipts = 0
        for table in ("normal_method_receipts", "universal_review_receipts"):
            if table in tables:
                receipts += int(connection.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0])
        return {
            "available": True,
            "restart_generation": generation,
            "mutation_receipts": receipts,
        }
    finally:
        connection.close()


def _workspace_clock(workspace: Path) -> Path:
    config_path = workspace / ".de67/state/workspace.json"
    try:
        configured = json.loads(config_path.read_text(encoding="utf-8"))["clock"]["state"]
        if not isinstance(configured, str) or not configured.strip():
            raise ValueError("clock.state must be a non-empty path")
        path = Path(configured).expanduser()
        return path if path.is_absolute() else (workspace / path).resolve()
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError):
        return workspace / ".de67/state/deadlines.sqlite3"


def report(method_root: Path, workspace: Path | None) -> dict[str, Any]:
    method_root = method_root.resolve()
    result: dict[str, Any] = {
        "machine": platform.node(),
        "method_root": str(method_root),
        "method_git": _git_state(method_root),
        "method_tree_sha256": mutation_guard.method_tree_digest(method_root),
        "protected_method_sha256": mutation_guard.protected_method_digest(method_root),
    }
    if workspace is not None:
        workspace = workspace.resolve()
        local = workspace / ".de67"
        try:
            specification = resolve(local)
            guidance = {
                "FS.md": _sha256(specification.path),
            }
            identity = {
                "canonical": specification.path.name,
            }
        except SpecificationError as error:
            guidance = {"FS.md": None}
            identity = {"error": str(error)}
        result["workspace"] = {
            "path": str(workspace),
            "git": _git_state(workspace),
            "guidance_sha256": {
                **guidance,
                **{
                    name: _sha256(local / name)
                    for name in ("test-and-task-guidelines.md",)
                },
            },
            "specification": identity,
            "clock": _clock_state(_workspace_clock(workspace)),
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--method-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--workspace", type=Path)
    arguments = parser.parse_args()
    print(json.dumps(report(arguments.method_root, arguments.workspace), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
