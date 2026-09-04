"""Lossless compact receipts for CAOL command payloads.

The command receipt is deliberately a filesystem artifact rather than a
database row: a read-only status command must not mutate registry authority.
The digest-named artifact makes retrieval exact and verifies it before use.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Mapping


def _canonical_bytes(payload: Mapping[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def write_command_artifact(*, artifact_root: Path, command: str,
                           payload: Mapping[str, Any]) -> Mapping[str, Any]:
    """Persist the complete payload and return its small, digest-bound receipt."""
    content = _canonical_bytes(payload)
    digest = hashlib.sha256(content).hexdigest()
    directory = artifact_root / command
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / (digest + ".json")
    if path.exists():
        if path.read_bytes() != content:
            raise RuntimeError("digest-named command artifact content drift")
    else:
        path.write_bytes(content)
    return {
        "schema": "caol-command-receipt-v1",
        "command": command,
        "artifact": {
            "path": str(path.resolve()),
            "sha256": digest,
            "bytes": len(content),
        },
    }


def read_command_artifact(*, artifact_root: Path, command: str,
                          sha256: str) -> Mapping[str, Any]:
    """Retrieve one exact artifact; fail closed on missing or digest drift."""
    digest = str(sha256).strip().lower()
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise ValueError("artifact SHA-256 must be a lowercase hexadecimal digest")
    path = (artifact_root / command / (digest + ".json")).resolve()
    expected_parent = (artifact_root / command).resolve()
    if path.parent != expected_parent:
        raise ValueError("artifact path escapes its command namespace")
    content = path.read_bytes()
    observed = hashlib.sha256(content).hexdigest()
    if observed != digest:
        raise ValueError("artifact digest drift")
    value = json.loads(content)
    if not isinstance(value, Mapping):
        raise ValueError("command artifact must contain a JSON object")
    return value
