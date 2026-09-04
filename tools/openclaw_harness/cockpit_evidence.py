"""Read-only projections of retained cockpit evidence (never gameplay proof inference)."""
from __future__ import annotations

import base64
import hashlib
import json
from pathlib import Path
from typing import Any


def decode(value: Any) -> Any:
    if isinstance(value, str) and value.lstrip().startswith(("{", "[")):
        try:
            return json.loads(value)
        except ValueError:
            pass
    return value


def select(value: Any, selector: str) -> Any:
    """Dot fields and numeric array indices; decode native JSON-string facts on demand."""
    for part in selector.split("."):
        value = decode(value)
        if isinstance(value, dict):
            value = value[part]
        elif isinstance(value, list) and part.isdecimal():
            value = value[int(part)]
        else:
            raise KeyError(selector)
    return decode(value)


def describe(value: Any, path: str) -> dict[str, Any]:
    decoded = decode(value)
    result = {"omitted": True, "selector": path, "type": type(decoded).__name__,
              "json_bytes": len(json.dumps(value, ensure_ascii=False).encode())}
    if isinstance(decoded, (list, dict, str)):
        result["count"] = len(decoded)
    if isinstance(decoded, dict):
        result["fields"] = list(decoded)
    return result


def compact(value: Any, path: str = "") -> Any:
    """Keep decision scalars and action availability; expose bulky values as selectors.

    The string preview is a presentation default, not an evidence or acceptance limit.
    Structured native facts are discoverable without rendering maps or repeated messages.
    """
    if isinstance(value, dict):
        result = {}
        for key, child in value.items():
            child_path = f"{path}.{key}" if path else key
            if key in {"advertised_action_details", "next_frame", "transition_event"}:
                result[key] = describe(child, child_path)
            elif key in {"facts", "payload"} and isinstance(child, dict):
                result[key] = {
                    k: describe(v, f"{child_path}.{k}")
                    if isinstance(decode(v), (dict, list)) else compact(v, f"{child_path}.{k}")
                    for k, v in child.items()
                }
            else:
                result[key] = compact(child, child_path)
        return result
    if isinstance(value, list):
        # These lists carry available operations or contradictions, not map payloads.
        if path.rsplit(".", 1)[-1] in {
            "actions", "advertised_actions", "valid_actions", "breadcrumbs",
            "contradictory_evidence", "errors", "evidence_refs",
        }:
            return [compact(v, f"{path}.{i}") for i, v in enumerate(value)]
        return describe(value, path) if value else []
    if isinstance(value, str) and path.rsplit(".", 1)[-1].endswith(("_id", "_sha256")):
        return value
    if isinstance(value, str) and (isinstance(decode(value), (dict, list)) or len(value) > 512):
        result = describe(value, path)
        if not isinstance(decode(value), (dict, list)):
            result["preview"] = value[:160]
        return result
    return value


def parse_record(raw: bytes) -> dict[str, Any]:
    text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
    start = text.find("{")
    if start >= 0:
        try:
            value = json.loads(text[start:])
            if isinstance(value, dict):
                return value
        except ValueError:
            return {"event": "unparsed", "error": "invalid_json_record", "text": text}
    return {"event": "text", "text": text}


def record_artifact(path: Path, offset: int, length: int, sha256: str,
                    selectors: list[str]) -> dict[str, Any]:
    try:
        if offset < 0 or length <= 0:
            raise ValueError("invalid_record_range")
        with path.open("rb") as source:
            source.seek(offset)
            raw = source.read(length)
        if hashlib.sha256(raw).hexdigest() != sha256:
            return {"ok": False, "error": "record_artifact_hash_mismatch"}
        record = parse_record(raw)
        if selectors:
            return {"ok": True, "sha256": sha256,
                    "fields": {s: select(record, s) for s in selectors}}
        result = {"ok": True, "sha256": sha256, "record": record}
        try:
            result["raw"] = raw.decode("utf-8")
        except UnicodeDecodeError:
            result["raw_base64"] = base64.b64encode(raw).decode("ascii")
            result["decoding_error"] = "invalid_utf8; parsed projection uses replacement characters"
        return result
    except (OSError, ValueError, KeyError, IndexError) as error:
        return {"ok": False, "error": str(error)}


def query(paths: list[Path], filters: dict[str, Any], selectors: list[str],
          offset: int, limit: int, contains: str | None = None) -> dict[str, Any]:
    """Filter parsed records before projection; pages never cut a JSON record in half."""
    rows = []
    matched = scanned = unparsed = unscoped = 0
    scanned_bytes = 0
    sources = []
    for path in paths:
        try:
            with path.open("rb") as source:
                snapshot_bytes = source.seek(0, 2)
                scanned_bytes += snapshot_bytes
                source.seek(0)
                if path.parent.name == "responses" and path.suffix == ".json":
                    try:
                        receipt = json.loads(path.with_suffix(".receipt.json").read_bytes())
                        raw_response = source.read(snapshot_bytes)
                        if (receipt.get("request_id") != path.stem or
                                receipt.get("response_sha256") != hashlib.sha256(raw_response).hexdigest()):
                            return {"ok": False, "error": "response_artifact_identity_or_hash_mismatch", "path": str(path)}
                        source.seek(0)
                    except (OSError, ValueError):
                        return {"ok": False, "error": "response_receipt_unavailable_or_invalid", "path": str(path)}
                while source.tell() < snapshot_bytes:
                    position = source.tell()
                    raw = source.readline(snapshot_bytes - position)
                    scanned += 1
                    record = parse_record(raw)
                    unparsed += record.get("event") == "unparsed"
                    identity = record.get("observation", record.get("result", record))
                    if not isinstance(identity, dict):
                        identity = record
                    unscoped += not identity.get("run_id")
                    try:
                        # Responses envelope the same native identities found at log roots.
                        def matches(key: str, expected: Any) -> bool:
                            if key == "request_id" and path.parent.name == "responses" and path.stem == expected:
                                return True
                            source = identity if key in {"run_id", "frame_id"} else record
                            return select(source, key) == expected
                        if any(not matches(k, v) for k, v in filters.items()):
                            continue
                    except (KeyError, IndexError):
                        continue
                    if contains is not None and contains.casefold() not in raw.decode("utf-8", errors="replace").casefold():
                        continue
                    matched += 1
                    if not offset <= matched - 1 < offset + limit:
                        continue
                    if not sources or sources[-1]["path"] != str(path):
                        sources.append({"path": str(path), "snapshot_bytes": snapshot_bytes})
                    handle = {"path": str(path), "offset": position, "length": len(raw),
                              "sha256": hashlib.sha256(raw).hexdigest()}
                    if selectors:
                        fields = {}
                        for selector in selectors:
                            try:
                                fields[selector] = select(record, selector)
                            except (KeyError, IndexError):
                                fields[selector] = {"error": "field_unavailable"}
                        projected = fields
                    else:
                        projected = compact(record)
                    rows.append({"artifact": handle, "record": projected})
        except OSError as error:
            return {"ok": False, "error": str(error), "sources": sources}
    return {"ok": True, "sources_on_page": sources, "source_count": len(paths),
            "source_bytes": scanned_bytes, "filters": filters, "contains": contains, "scanned": scanned,
            "matched": matched, "unparsed_records": unparsed, "records_without_run_id": unscoped,
            "rows": rows, "page": {"offset": offset, "limit": limit,
            "omitted_matches": matched - len(rows),
            "next_offset": offset + limit if offset + limit < matched else None},
            "retrieval": "record-artifact --path PATH --offset OFFSET --length LENGTH --sha256 SHA256 [--select FIELD]",
            "snapshot": "Each page queries current source lengths. Later appends may change counts; record handles bind exact bytes, not a mutable page number.",
            "diagnostics": "Unparsed/unscoped records are counted, not attributed to a run. Query event=unparsed or event=text to inspect them; raw files are unchanged."}
