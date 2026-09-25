#!/usr/bin/env python3
"""Resolve FS.md, the single canonical functional specification.

Durable slice marker spellings remain unchanged so existing claim identities and
receipts survive a filename cleanup. Delivery state belongs in work-ledger.md.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

FS_FILE = "FS.md"
_STATUS = re.compile(
    r"(?ms)^Implementation status:\s*\n.*?(?=^<!-- DE67:DFS-SLICE:END )"
)
_EMPTY_SLICE = re.compile(
    r"(?m)(<!-- DE67:DFS-SLICE:BEGIN[^>]*-->\n)(?=<!-- DE67:DFS-SLICE:END )"
)


class SpecificationError(RuntimeError):
    """The workspace has no canonical functional specification."""


@dataclass(frozen=True)
class Specification:
    path: Path
    text: str

    @property
    def label(self) -> str:
        return f".de67/{self.path.name}"


def render_functional_specification(text: str) -> str:
    """Remove delivery projections while preserving functional/slice bytes."""
    functional = _STATUS.sub("", text)
    return _EMPTY_SLICE.sub(
        r"\1<!-- DE67:FS-SLICE-IDENTITY delivery=work-ledger.md -->\n",
        functional,
    )


def resolve(de67_root: Path) -> Specification:
    """Read FS.md directly, with no compatibility file or alternate filename."""
    return resolve_path(de67_root / FS_FILE)


def resolve_path(path: Path) -> Specification:
    """Read a canonical FS or an explicitly supplied isolated candidate file."""
    if path.name == "DFS.md":
        raise SpecificationError("Use FS.md as the canonical functional specification")
    if not path.is_file():
        raise SpecificationError(f"Missing functional specification: {path}")
    return Specification(path, path.read_text(encoding="utf-8"))
