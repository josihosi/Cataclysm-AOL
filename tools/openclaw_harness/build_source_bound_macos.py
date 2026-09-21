#!/usr/bin/env python3
"""Build cataclysm-tiles and emit a fail-closed dirty-source binding receipt."""

from __future__ import annotations

import json
import argparse
import hashlib
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

import startup_harness  # noqa: E402


def pch_recovery_paths(root: Path, build_prefix: str, *, tiles: bool) -> tuple[Path, ...]:
    """Return only the generated PCH files owned by this Makefile configuration.

    Makefile defines ``ODIR = $(BUILD_PREFIX)obj`` and
    ``ODIRTILES = $(BUILD_PREFIX)obj/tiles``.  BUILD_PREFIX is therefore a
    literal prefix, not always a directory: ``r033-headless-`` owns
    ``r033-headless-obj`` while ``isolated/`` owns ``isolated/obj``.
    """
    object_dir = root / f'{build_prefix}obj' / ('tiles' if tiles else '')
    # ``Path`` intentionally normalizes the empty curses component.  The PCH
    # and its make-generated dependency file are the narrow stale-cache pair;
    # object files and libraries remain reusable.
    pch = object_dir / 'pch' / 'main-pch.hpp.pch'
    return pch, pch.with_suffix('.pch.d')


def incompatible_pch_diagnostic(text: str) -> bool:
    """Recognize compiler evidence that retrying after the owned PCH purge helps."""
    lowered = text.lower()
    return ('precompiled header' in lowered and
            any(marker in lowered for marker in ('mismatch', 'different', 'incompatible', 'invalid'))) or \
        '__optimize_size__' in lowered


def invalidate_owned_pch(root: Path, build_prefix: str, *, tiles: bool) -> list[dict[str, str]]:
    """Remove the exact generated PCH pair for this configuration, if present."""
    removed = []
    for path in pch_recovery_paths(root, build_prefix, tiles=tiles):
        if path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            path.unlink()
            removed.append({'path': str(path), 'sha256': digest})
    return removed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build one macOS renderer and record its exact source binding."
    )
    parser.add_argument("--renderer", choices=("tiles", "curses"), default="tiles")
    parser.add_argument(
        "--build-prefix", default="",
        help="Optional literal Make BUILD_PREFIX; use a trailing / only for a directory-style prefix.",
    )
    parser.add_argument(
        "--log-dir", default="",
        help="Directory for complete version/build stdout, stderr, and combined logs.",
    )
    args = parser.parse_args()
    tiles = args.renderer == "tiles"
    build_prefix = str(args.build_prefix).strip()
    command = [
        "make", "-j8", f"TILES={int(tiles)}", "SOUND=1", "RELEASE=1", "LOCALIZE=1", "LANGUAGES=all",
        "LINTJSON=0", "ASTYLE=0", "TESTS=0",
    ]
    if build_prefix:
        command.append(f"BUILD_PREFIX={build_prefix}")
    invocation = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    log_dir = Path(args.log_dir).expanduser() if str(args.log_dir).strip() else (
        ROOT / "build_logs" / "source_bound_macos" / invocation
    )
    log_dir.mkdir(parents=True, exist_ok=True)

    def run_logged(argv: list[str], label: str) -> dict[str, object]:
        """Run one phase while retaining exact output and a stable handle to it."""
        completed = subprocess.run(argv, cwd=ROOT, capture_output=True, check=False)
        stdout = completed.stdout if isinstance(completed.stdout, bytes) else str(completed.stdout or "").encode()
        stderr = completed.stderr if isinstance(completed.stderr, bytes) else str(completed.stderr or "").encode()
        stdout_path = log_dir / f"{label}.stdout.log"
        stderr_path = log_dir / f"{label}.stderr.log"
        full_path = log_dir / f"{label}.full.log"
        stdout_path.write_bytes(stdout)
        stderr_path.write_bytes(stderr)
        full_path.write_bytes(
            b"===== stdout =====\n" + stdout + b"\n===== stderr =====\n" + stderr
        )
        diagnostic = (stderr or stdout).decode("utf-8", errors="replace").strip()
        return {
            "label": label,
            "command": argv,
            "exit_status": completed.returncode,
            "stdout_log": str(stdout_path),
            "stderr_log": str(stderr_path),
            "full_log": str(full_path),
            "diagnostic": diagnostic,
        }

    def emit_failure(phase: dict[str, object], *, source: dict[str, object] | None = None,
                     pch_recovery: dict[str, object] | None = None) -> int:
        payload: dict[str, object] = {
            "ok": False,
            "phase": phase["label"],
            "exit_status": phase["exit_status"],
            "diagnostic": phase["diagnostic"],
            "logs": phase,
        }
        if source is not None:
            payload["source"] = source
        if pch_recovery is not None:
            payload['pch_recovery'] = pch_recovery
        print(json.dumps(payload, sort_keys=True), file=sys.stderr)
        return int(phase["exit_status"] or 1)
    # The direct product target does not depend on Makefile's phony ``version``
    # target.  Refresh it first so the executable's embedded revision cannot
    # remain at a prior checkout while its build receipt claims current source.
    version_command = [*command, "version"]
    version_run = run_logged(version_command, "version")
    if version_run["exit_status"] != 0:
        return emit_failure(version_run)
    command.append(f"{build_prefix}{'cataclysm-tiles' if tiles else 'cataclysm'}")
    build_run = run_logged(command, "build")
    pch_recovery = None
    if build_run["exit_status"] != 0 and incompatible_pch_diagnostic(str(build_run["diagnostic"])):
        removed = invalidate_owned_pch(ROOT, build_prefix, tiles=tiles)
        if removed:
            retry = run_logged(command, "build-after-pch-refresh")
            pch_recovery = {
                'reason': 'compiler reported an incompatible generated PCH for this build configuration',
                'removed': removed,
                'retry': retry,
            }
            build_run = retry
    if build_run["exit_status"] != 0:
        failure = emit_failure(build_run, pch_recovery=pch_recovery)
        return failure

    executable = (ROOT / f"{build_prefix}{'cataclysm-tiles' if tiles else 'cataclysm'}").resolve()
    source = startup_harness.product_source_binding()
    executable_sha256, error = startup_harness.sha256_file(executable)
    captured_head = startup_harness.current_head_short()
    if not source.get("ok") or error or not captured_head:
        print(json.dumps({
            "ok": False,
            "phase": "source-binding",
            "source": source,
            "executable_error": error,
            "logs": {"version": version_run, "build": build_run},
        }, sort_keys=True), file=sys.stderr)
        return 1
    receipt = {
        "schema": startup_harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
        "captured_head": captured_head,
        "executable_path": str(executable),
        "executable_sha256": executable_sha256,
        "product_source_sha256": source["sha256"],
        "built_at": datetime.now(timezone.utc).isoformat(),
        "command": command,
        "version_command": version_command,
        "logs": {"version": version_run, "build": build_run},
        "build_configuration": {
            "renderer": args.renderer,
            "build_prefix": build_prefix,
            "make_variables": command[2:-1],
            "pch_recovery": pch_recovery,
        },
        "log_dir": str(log_dir),
    }
    serialized = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    receipt_identity = hashlib.sha256(serialized.encode("utf-8")).hexdigest()
    receipt_path = startup_harness.product_build_receipt_archive_path(
        executable, executable_sha256, source["sha256"], receipt_identity
    )
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    # Content-addressed paths are immutable.  A repeated build of the same
    # bytes is a no-op; a different build receives a distinct archive member.
    try:
        with receipt_path.open("x", encoding="utf-8") as stream:
            stream.write(serialized)
    except FileExistsError:
        existing = receipt_path.read_text(encoding="utf-8")
        if existing != serialized:
            raise RuntimeError(f"immutable product receipt collision: {receipt_path}")
    print(json.dumps({"ok": True, "receipt_path": str(receipt_path), "receipt": receipt}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
