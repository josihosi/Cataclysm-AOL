#!/usr/bin/env python3
"""Run all deterministic action-status fixtures."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    fixtures_dir = repo_root / "tools" / "llm_runner" / "fixtures"
    checker = repo_root / "tools" / "llm_runner" / "action_status_check.py"
    expect_files = sorted(fixtures_dir.glob("*.expect.json"))
    if not expect_files:
        print("No expectation files found.")
        return 1

    failures = []
    for expect_file in expect_files:
        stem = expect_file.name[: -len(".expect.json")]
        log_file = fixtures_dir / f"{stem}.txt"
        if not log_file.exists():
            log_file = fixtures_dir / f"{stem}.log"
        if not log_file.exists():
            failures.append((expect_file.name, f"missing matching log fixture for {stem}"))
            continue
        cmd = [sys.executable, str(checker), "--log-file", str(log_file), "--expect-file", str(expect_file)]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        label = f"{stem}"
        if proc.returncode == 0:
            print(f"PASS {label}")
        else:
            print(f"FAIL {label}")
            failures.append((label, proc.stdout + proc.stderr))

    if failures:
        print("\nFailures:")
        for label, output in failures:
            print(f"--- {label} ---")
            print(output.rstrip())
        return 1

    print(f"\nAll {len(expect_files)} action-status fixtures passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
