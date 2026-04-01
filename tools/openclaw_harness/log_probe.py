#!/usr/bin/env python3
"""Small helper for recent llm_intent.log probing.

Why this exists:
- line-count based probing turned out flaky in practice
- for live harness probes we usually want either
  1. a byte-size checkpoint, then the appended block, or
  2. a fallback to the last N lines
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser(description="Checkpoint/read helper for llm_intent.log")
    sub = parser.add_subparsers(dest="command", required=True)

    mark = sub.add_parser("mark", help="Print current file size in bytes as a checkpoint.")
    mark.add_argument("log_file")

    read = sub.add_parser("read", help="Read appended content since a byte checkpoint.")
    read.add_argument("log_file")
    read.add_argument("--since-bytes", type=int, default=0)
    read.add_argument("--fallback-tail", type=int, default=40)
    read.add_argument("--json", action="store_true")

    args = parser.parse_args()
    log_path = Path(args.log_file)

    if args.command == "mark":
        size = log_path.stat().st_size if log_path.exists() else 0
        print(size)
        return 0

    size = log_path.stat().st_size if log_path.exists() else 0
    since = max(0, int(args.since_bytes))
    appended = ""
    if log_path.exists() and size > since:
        with log_path.open("r", encoding="utf-8", errors="replace") as fh:
            fh.seek(since)
            appended = fh.read()
    tail_lines = read_text(log_path).splitlines()[-args.fallback_tail :] if log_path.exists() else []
    payload = {
        "log_file": str(log_path),
        "since_bytes": since,
        "current_bytes": size,
        "appended": appended,
        "had_appended": bool(appended),
        "fallback_tail_lines": tail_lines,
    }
    if args.json:
        print(json.dumps(payload, indent=2, ensure_ascii=False))
    else:
        if appended:
            print(appended.rstrip("\n"))
        else:
            print("\n".join(tail_lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
