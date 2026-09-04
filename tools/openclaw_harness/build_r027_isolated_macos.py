#!/usr/bin/env python3
"""Build and atomically seal the only macOS executable allowed by R-027."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

import startup_harness


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "cataclysm-tiles"
TARGET = ROOT / "build" / "r027-closure-007-tiles"
RECEIPT = ROOT / ".userdata" / "openclaw_harness" / "r027-build-receipts" / "r027-closure-007-tiles.json"
COMMAND = [
    "make", "-j8", "TILES=1", "SOUND=1", "RELEASE=1", "LOCALIZE=1", "LANGUAGES=all",
    "LINTJSON=0", "ASTYLE=0", "TESTS=0", "cataclysm-tiles",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def valid_macho(path: Path) -> bool:
    """Reject a truncated linker output before it can become the sealed target."""
    check = subprocess.run(["otool", "-l", str(path)], capture_output=True, text=True, check=False)
    return check.returncode == 0


def write_receipt(payload: dict) -> None:
    RECEIPT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=RECEIPT.parent, delete=False) as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")
        temporary = Path(stream.name)
    os.replace(temporary, RECEIPT)


def main() -> int:
    completed = subprocess.run(COMMAND, cwd=ROOT, check=False)
    if completed.returncode != 0:
        return completed.returncode
    if not SOURCE.is_file() or not os.access(SOURCE, os.X_OK) or not valid_macho(SOURCE):
        print("completed build did not produce a valid Mach-O executable", file=sys.stderr)
        return 1
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=TARGET.parent, delete=False) as stream:
        temporary = Path(stream.name)
    try:
        shutil.copyfile(SOURCE, temporary)
        shutil.copymode(SOURCE, temporary)
        if not valid_macho(temporary):
            print("copied R-027 executable failed Mach-O validation", file=sys.stderr)
            return 1
        os.replace(temporary, TARGET)
    finally:
        temporary.unlink(missing_ok=True)
    digest = sha256(TARGET)
    product_source = startup_harness.product_source_binding()
    captured_head = startup_harness.current_head_short()
    if not product_source.get("ok") or not captured_head:
        print("could not bind isolated R-027 executable to current product source", file=sys.stderr)
        return 1
    write_receipt({
        "schema": "caol-r027-isolated-build-receipt-v1", "status": "completed",
        "build_returncode": completed.returncode, "executable_path": str(TARGET.resolve()),
        "executable_sha256": digest, "source_executable_path": str(SOURCE.resolve()),
        "source_executable_sha256": sha256(SOURCE), "command": COMMAND,
        "sealed_at": datetime.now(timezone.utc).isoformat(),
    })
    product_receipt = {
        "schema": startup_harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
        "captured_head": captured_head,
        "executable_path": str(TARGET.resolve()),
        "executable_sha256": digest,
        "product_source_sha256": product_source["sha256"],
        "built_at": datetime.now(timezone.utc).isoformat(),
        "command": COMMAND,
    }
    product_receipt_path = startup_harness.product_build_receipt_path(TARGET)
    product_receipt_path.parent.mkdir(parents=True, exist_ok=True)
    product_receipt_path.write_text(
        json.dumps(product_receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps({"ok": True, "executable": str(TARGET), "sha256": digest,
                      "receipt": str(RECEIPT)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
