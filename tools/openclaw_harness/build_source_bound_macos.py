#!/usr/bin/env python3
"""Build cataclysm-tiles and emit a fail-closed dirty-source binding receipt."""

from __future__ import annotations

import json
import argparse
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

import startup_harness  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build one macOS renderer and record its exact source binding."
    )
    parser.add_argument("--renderer", choices=("tiles", "curses"), default="tiles")
    parser.add_argument(
        "--build-prefix", default="",
        help="Optional isolated build prefix; required when preserving another renderer binary.",
    )
    args = parser.parse_args()
    tiles = args.renderer == "tiles"
    build_prefix = str(args.build_prefix).strip()
    command = [
        "make", "-j8", f"TILES={int(tiles)}", "SOUND=1", "RELEASE=1", "LOCALIZE=1", "LANGUAGES=all",
        "LINTJSON=0", "ASTYLE=0", "TESTS=0",
    ]
    if build_prefix:
        if not build_prefix.endswith("/"):
            build_prefix += "/"
        command.append(f"BUILD_PREFIX={build_prefix}")
    command.append(f"{build_prefix}{'cataclysm-tiles' if tiles else 'cataclysm'}")
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode != 0:
        return completed.returncode

    executable = (ROOT / build_prefix / ("cataclysm-tiles" if tiles else "cataclysm")).resolve()
    source = startup_harness.product_source_binding()
    executable_sha256, error = startup_harness.sha256_file(executable)
    captured_head = startup_harness.current_head_short()
    if not source.get("ok") or error or not captured_head:
        print(json.dumps({"ok": False, "source": source, "executable_error": error}), file=sys.stderr)
        return 1
    receipt = {
        "schema": startup_harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
        "captured_head": captured_head,
        "executable_path": str(executable),
        "executable_sha256": executable_sha256,
        "product_source_sha256": source["sha256"],
        "built_at": datetime.now(timezone.utc).isoformat(),
        "command": command,
    }
    receipt_path = startup_harness.product_build_receipt_path(executable)
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"ok": True, "receipt_path": str(receipt_path), "receipt": receipt}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
