#!/usr/bin/env python3
"""Run an optional external owner-contact adapter without owning its transport."""

from __future__ import annotations

import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


class BlockerAdapterError(RuntimeError):
    """Raised when an external blocker adapter violates the small JSON contract."""


@dataclass(frozen=True)
class BlockerReply:
    blocker_digest: str
    notification_message_id: str
    reply_message_id: str
    reply_text: str
    author_id: str


def parse_adapter_command(encoded: str) -> tuple[str, ...]:
    """Decode one shell-independent JSON argument array."""

    try:
        value = json.loads(encoded)
    except json.JSONDecodeError as error:
        raise BlockerAdapterError(
            "Blocker adapter command must be a JSON argument array"
        ) from error
    if (
        not isinstance(value, list)
        or not value
        or any(not isinstance(argument, str) or not argument for argument in value)
    ):
        raise BlockerAdapterError(
            "Blocker adapter command must contain one or more nonempty strings"
        )
    return tuple(value)


def _reply(value: object) -> BlockerReply | None:
    if value is None:
        return None
    if not isinstance(value, dict):
        raise BlockerAdapterError("Blocker adapter returned non-object JSON")
    required = (
        "blocker_digest",
        "notification_message_id",
        "reply_message_id",
        "reply_text",
        "author_id",
    )
    if any(not isinstance(value.get(field), str) or not value[field] for field in required):
        raise BlockerAdapterError(
            "Blocker adapter reply must contain five nonempty string fields"
        )
    return BlockerReply(*(value[field] for field in required))


class SubprocessBlockerAdapter:
    """Invoke a configured adapter only when core supervision is already quiescent."""

    def __init__(self, command: Sequence[str]) -> None:
        if not command or any(not argument for argument in command):
            raise BlockerAdapterError("Blocker adapter command must not be empty")
        self.command = tuple(command)

    def wait_for_reply(self, *, workspace: Path, lineage_id: str) -> BlockerReply | None:
        completed = subprocess.run(
            [
                *self.command,
                "wait",
                "--workspace",
                str(workspace),
                "--lineage",
                lineage_id,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        try:
            value = json.loads(completed.stdout)
        except json.JSONDecodeError as error:
            raise BlockerAdapterError("Blocker adapter returned invalid JSON") from error
        return _reply(value)


def safe_wait_for_reply(
    adapter: SubprocessBlockerAdapter,
    *,
    workspace: Path,
    lineage_id: str,
) -> BlockerReply | None:
    """Fail an optional contact route closed without failing ordinary DE67 supervision."""

    try:
        return adapter.wait_for_reply(workspace=workspace, lineage_id=lineage_id)
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or "").strip().splitlines()
        suffix = f": {detail[-1]}" if detail else ""
        print(
            "coordinator supervisor: optional blocker adapter unavailable: "
            f"exit {error.returncode}{suffix}",
            file=sys.stderr,
        )
        return None
    except (BlockerAdapterError, OSError, subprocess.SubprocessError) as error:
        print(
            f"coordinator supervisor: optional blocker adapter unavailable: {error}",
            file=sys.stderr,
        )
        return None
