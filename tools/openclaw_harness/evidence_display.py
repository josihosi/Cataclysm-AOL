"""Byte-bounded CLI presentation backed by immutable, exactly recoverable JSON.

This is a display boundary, never a retention or gameplay limit. Internal callers
continue to receive complete objects. All CLI output uses this serializer.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

from cockpit_archive import json_chunks

DEFAULT_BYTES = 8192
STORE = Path(__file__).resolve().parents[2] / ".userdata/openclaw_harness/evidence-display"


def encoded(value):
    return "".join(json_chunks(value)).encode("utf-8")


def retain(value, directory=STORE):
    raw = encoded(value)
    digest = hashlib.sha256(raw).hexdigest()
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / (digest + ".json")
    if not path.exists():
        fd, temporary = tempfile.mkstemp(dir=directory, prefix=".writing-")
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
    return {"sha256": digest, "json_bytes": len(raw)}


def recover(digest, directory=STORE):
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise ValueError("invalid_evidence_digest")
    raw = (directory / (digest + ".json")).read_bytes()
    if hashlib.sha256(raw).hexdigest() != digest:
        raise ValueError("evidence_hash_mismatch")
    return json.loads(raw)


def bounded(value, budget=DEFAULT_BYTES, directory=STORE):
    """Preserve structure where it fits; every omission is an exact value handle."""
    original = retain(value, directory)

    def project(item, allowance):
        raw = encoded(item)
        if len(raw) <= allowance:
            return item
        ref = {"omitted": True, "evidence": retain(item, directory)}
        if isinstance(item, (dict, list)) and item:
            pairs = list(item.items()) if isinstance(item, dict) else list(enumerate(item))
            empty = {k: None for k, _ in pairs} if isinstance(item, dict) else [None] * len(item)
            if len(encoded(empty)) + 150 * len(pairs) > allowance:
                return ref
            candidate = dict(item) if isinstance(item, dict) else list(item)
            sizes = {k: len(encoded(v)) for k, v in pairs}
            while len(encoded(candidate)) > allowance and sizes:
                key = max(sizes, key=sizes.get)
                size = sizes.pop(key)
                deficit = len(encoded(candidate)) - allowance
                candidate[key] = project(candidate[key], max(150, size - deficit - 16))
            if len(encoded(candidate)) <= allowance:
                return candidate
        return ref

    metadata = {"budget_bytes": budget, "full_evidence": original,
                "retrieve": "python3 tools/openclaw_harness/evidence_display.py --sha256 HASH [--selector FIELD] [--offset N --limit N] [--export FILE]"}
    envelope = {"presentation": metadata}
    # Budget includes the final newline and all metadata, including errors.
    room = budget - len(encoded(envelope)) - 32
    result = project(value, room)
    if isinstance(result, dict):
        result = {**result, "presentation": metadata}
    else:
        result = {"value": result, "presentation": metadata}
    if len(encoded(result)) + 1 > budget:
        result = {"omitted": True, "presentation": metadata}
    if len(encoded(result)) + 1 > budget:
        raise ValueError("presentation_budget_too_small")
    return result


def emit(value, directory=STORE):
    sys.stdout.write(encoded(bounded(value, directory=directory)).decode("utf-8") + "\n")


def exact_select(value, selector):
    # Artifact recovery preserves JSON types, including strings containing JSON.
    for part in selector.split("."):
        value = value[int(part)] if isinstance(value, list) and part.isdecimal() else value[part]
    return value


class PresentationParser(argparse.ArgumentParser):
    def error(self, message):
        emit({"ok": False, "error": message})
        raise SystemExit(2)

    def print_help(self, file=None):
        emit({"ok": True, "help": self.format_help()})


def page(digest, selector="", offset=0, limit=20, directory=STORE):
    if offset < 0 or limit < 1:
        raise ValueError("invalid_evidence_page")
    value = recover(digest, directory)
    if selector:
        value = exact_select(value, selector)
    if isinstance(value, dict):
        entries = list(value.items())
        selected = dict(entries[offset:offset + limit])
    elif isinstance(value, (list, str)):
        selected = value[offset:offset + limit]
    else:
        return {"ok": True, "value": value}
    return {"ok": True, "value": selected, "page": {"sha256": digest,
            "selector": selector, "offset": offset, "total": len(value),
            "next_offset": offset + limit if offset + limit < len(value) else None}}


def main(argv=None):
    parser = PresentationParser(description=__doc__)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--selector", default="")
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument("--export", type=Path, help="Write complete selected JSON to a new file")
    args = parser.parse_args(argv)
    try:
        if args.export:
            value = recover(args.sha256)
            if args.selector:
                value = exact_select(value, args.selector)
            with args.export.open("xb") as stream:
                stream.write(encoded(value))
            result = {"ok": True, "exported": str(args.export), "evidence": retain(value)}
        else:
            result = page(args.sha256, args.selector, args.offset, args.limit)
    except (OSError, ValueError, KeyError, IndexError) as error:
        result = {"ok": False, "error": str(error)}
    emit(result)
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
