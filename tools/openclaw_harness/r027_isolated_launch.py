"""Fail-closed named-runtime contract for the R-027 observation route.

This is deliberately narrower than the legacy launcher: it never searches for
a game binary and it never chooses a process.  The scenario names one
repository-relative executable and seals its digest; the certification lease
owner subsequently owns the exact PID.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Dict, Mapping


class R027IsolatedLaunchError(ValueError):
    """The isolated R-027 launch declaration is not safe to execute."""


_REQUIRED = frozenset({"name", "path", "sha256", "profile", "world", "fixture",
                       "run_identity", "control_endpoint", "receipt_sidecar", "cleanup_token",
                       "build_receipt"})
_SHARED_NAMES = frozenset({"cataclysm-tiles", "cataclysm-tiles.exe", "cataclysm-tlg-tiles",
                           "cataclysm-tlg-tiles.exe", "cataclysm-aol", "cataclysm-aol.exe"})
_BUILD_RECEIPT_SCHEMA = "caol-r027-isolated-build-receipt-v1"


def _digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def _receipt_path(value: str, repository: Path) -> Path:
    relative = Path(value)
    if relative.is_absolute() or ".." in relative.parts:
        raise R027IsolatedLaunchError("R-027 build receipt path must stay repository-relative")
    receipt = (repository.resolve() / relative).resolve()
    if repository.resolve() not in receipt.parents:
        raise R027IsolatedLaunchError("R-027 build receipt path escaped the repository")
    return receipt


def _validate_completed_build_receipt(receipt_path: Path, executable: Path,
                                     executable_sha256: str) -> None:
    try:
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise R027IsolatedLaunchError("R-027 named executable lacks a completed-build receipt") from exc
    except json.JSONDecodeError as exc:
        raise R027IsolatedLaunchError("R-027 completed-build receipt is malformed") from exc
    if not isinstance(receipt, Mapping) or receipt.get("schema") != _BUILD_RECEIPT_SCHEMA:
        raise R027IsolatedLaunchError("R-027 completed-build receipt has an unsupported schema")
    if receipt.get("status") != "completed" or receipt.get("build_returncode") != 0:
        raise R027IsolatedLaunchError("R-027 named executable was not sealed after a successful build")
    if receipt.get("executable_path") != str(executable) or \
            receipt.get("executable_sha256") != executable_sha256:
        raise R027IsolatedLaunchError("R-027 completed-build receipt does not bind the named executable")


def select_r027_isolated_executable(declaration: Mapping[str, Any], *, repository: Path) -> Dict[str, str]:
    """Validate and resolve the only executable permitted for R-027.

    The returned values are safe to put into a registry receipt.  Missing
    fields, absolute/escaping paths, shared executable aliases, a stale build,
    or incomplete launch controls are hard errors before process launch.
    """
    raw = declaration.get("r027_isolated_launch")
    if not isinstance(raw, Mapping) or set(raw) != _REQUIRED:
        raise R027IsolatedLaunchError("R-027 requires one complete isolated named-executable declaration")
    values = {key: str(raw[key]).strip() for key in _REQUIRED}
    if any(not value for value in values.values()):
        raise R027IsolatedLaunchError("R-027 isolated launch fields must be non-empty strings")
    if len(values["sha256"]) != 64 or any(char not in "0123456789abcdef" for char in values["sha256"].lower()):
        raise R027IsolatedLaunchError("R-027 executable digest must be a SHA-256 hex digest")
    relative = Path(values["path"])
    if relative.is_absolute() or ".." in relative.parts:
        raise R027IsolatedLaunchError("R-027 executable path must stay repository-relative")
    executable = (repository.resolve() / relative).resolve()
    if repository.resolve() not in executable.parents or executable.name.lower() in _SHARED_NAMES:
        raise R027IsolatedLaunchError("R-027 rejects the shared game executable")
    if not executable.is_file() or not executable.stat().st_mode & 0o111:
        raise R027IsolatedLaunchError("R-027 named executable is not runnable")
    observed = _digest(executable)
    if observed != values["sha256"].lower():
        raise R027IsolatedLaunchError("R-027 named executable digest drifted")
    _validate_completed_build_receipt(
        _receipt_path(values["build_receipt"], repository), executable, observed
    )
    return {
        "name": values["name"], "executable_path": str(executable), "executable_sha256": observed,
        "profile": values["profile"], "world": values["world"], "fixture": values["fixture"],
        "run_identity": values["run_identity"], "control_endpoint": values["control_endpoint"],
        "receipt_sidecar": values["receipt_sidecar"], "cleanup_token": values["cleanup_token"],
        "build_receipt": values["build_receipt"],
    }
