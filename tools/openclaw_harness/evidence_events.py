"""One query envelope over retained native, NPC and runner evidence.

An index is an immutable query snapshot, not an alternative game recorder.
Unpublished correlations remain null; names never become actor identities.
"""
import base64
import hashlib
import json
from pathlib import Path
import re
import shlex

from cockpit_evidence import decode, parse_record, select
from cockpit_archive import ArchiveSequence
from evidence_display import retain, encoded


def parse(raw):
    record = parse_record(raw)
    if record.get("event") != "text":
        return record
    text = record["text"]
    # Native CAOL key=value records use quoted strings. Do not attribute free
    # prose or malformed quoted fields to an actor/request.
    match = re.match(r"^(?:\[CAOL_EVENT\]\s*)?(action_status|camp routing check)\s+(\w+=.*)$", text)
    if not match:
        return record
    try:
        fields = dict(part.split("=", 1) for part in shlex.split(match[2]))
    except ValueError:
        return {"event": "unparsed", "text": text}
    return {"event": match[1].replace(" ", "_"), **fields}


def envelopes(record, source):
    receipt = record.get("receipt", {}) if isinstance(record, dict) else {}
    receipt = receipt if isinstance(receipt, dict) else {}
    native = receipt.get("native_receipt", {})
    native = native if isinstance(native, dict) else {}
    observed = record.get("observation", record.get("result", {})) if isinstance(record, dict) else {}
    observed = observed if isinstance(observed, dict) else {}
    base = {"schema": "caol-evidence-event-v1", "producer": source["producer"],
            "event_id": hashlib.sha256((source.get("path", source["producer"]) + ":" + source["sha256"] + ":" + str(source["offset"])).encode()).hexdigest(),
            "sequence": record.get("sequence", record.get("seq")),
            "run_id": record.get("run_id", observed.get("run_id", native.get("run_id"))),
            "binding_id": record.get("binding_id", receipt.get("binding_id", native.get("binding_id"))),
            "session_generation": record.get("session_generation", receipt.get("session_generation")),
            "process_instance": record.get("process_instance", native.get("process_instance")),
            "frame_id": record.get("frame_id", observed.get("frame_id", observed.get("observation_id", native.get("frame_id")))),
            "process_pid": record.get("runner_pid", record.get("pid")),
            "game_time": {"minutes": record.get("game_minutes", observed.get("game_minutes")),
                           "turn": record.get("game_turn", observed.get("game_turn", observed.get("turn")))},
            "wall_time": {"unix_ms": record.get("wall_time"), "unix_seconds": record.get("timestamp"), "iso8601": record.get("created_at")},
            "actor_id": record.get("actor_id", record.get("npc_id")),
            "actor_name": record.get("actor_name", record.get("npc", record.get("npc_name"))),
            "request_id": record.get("request_id", receipt.get("request_id", native.get("request_id", record.get("request")))),
            "action": record.get("action", record.get("kind")),
            "causal_ref": record.get("causal_ref", record.get("prompt_sha256")),
            "event": record.get("event", "cockpit_response" if receipt else record.get("schema", "record")),
            "changed_fields": record.get("changed_fields", record.get("delta")),
            "source": source, "payload": record}
    yield base

    def observations(value, path=""):
        if isinstance(value, dict):
            if isinstance(value.get("surface"), dict):
                yield path, value
            for key, child in value.items():
                if isinstance(child, (dict, list, ArchiveSequence)):
                    yield from observations(child, path + "." + key if path else key)
        elif isinstance(value, (list, ArchiveSequence)):
            for index, child in enumerate(value):
                yield from observations(child, path + "." + str(index))
    observed_record = record
    if record.get("event") == "surface_descriptor":
        observed_record = {**record, "observation_id": record.get("frame_id"),
                           "surface": {"facts": record.get("payload", {})}}
    for path, observed in observations(observed_record):
        facts = observed["surface"].get("facts", {})
        entities = decode(facts.get("visible_entities", []))
        avatar = decode(facts.get("avatar", {}))
        status = decode(facts.get("avatar_status", {}))
        rows = list(entities) if isinstance(entities, list) else []
        if isinstance(avatar, dict) and isinstance(status, dict) and status.get("actor_id"):
            rows.append({**avatar, "identity": {"id": status["actor_id"]}, "status": status})
        for index, entity in enumerate(rows):
            if not isinstance(entity, dict):
                continue
            identity = entity.get("identity", {})
            yield {**base, "event_id": base["event_id"] + ":" + path + ":" + str(index),
                   "event": "actor_observed", "run_id": observed.get("run_id"),
                   "sequence": observed.get("sequence"),
                   "process_instance": observed.get("process_instance", base["process_instance"]),
                   "game_time": {"minutes": observed.get("game_minutes"),
                                 "turn": observed.get("game_turn", status.get("observed_turn") if isinstance(status, dict) else None)},
                   "actor_id": identity.get("id") if isinstance(identity, dict) else None,
                   "actor_name": entity.get("name"), "changed_fields": None,
                   "payload": entity, "observation_id": observed.get("observation_id")}


def _request_result_links(events):
    """Group only explicit request/run/process identities; never infer joins by time."""
    groups = {}
    for event in events:
        request_id = event.get("request_id")
        if not request_id:
            continue
        key = (event.get("run_id"), event.get("process_instance"), request_id)
        groups.setdefault(key, []).append(event)
    links = []
    for (run_id, process_instance, request_id), matching in groups.items():
        stages = {"acceptance": [], "rejection": [], "result": []}
        writers = set()
        for event in matching:
            payload = event.get("payload", {})
            if isinstance(payload, dict) and isinstance(payload.get("payload"), dict):
                payload = payload["payload"]
            accepted = payload.get("accepted") if isinstance(payload, dict) else None
            rejection = payload.get("rejection_reason") if isinstance(payload, dict) else None
            kind = str(event.get("event", "")).casefold()
            if accepted is True:
                stages["acceptance"].append(event["event_id"])
            if accepted is False or rejection:
                stages["rejection"].append(event["event_id"])
            if any(token in kind for token in ("result", "completed", "applied", "outcome")):
                stages["result"].append(event["event_id"])
            writer = (event.get("producer"), event.get("actor_id"))
            if writer != (None, None):
                writers.add(writer)
        missing = [name for name, ids in stages.items() if not ids]
        links.append({"request_id": request_id, "run_id": run_id,
                      "process_instance": process_instance,
                      "status": "complete" if not missing else "partial",
                      "stages": stages, "missing": missing,
                      "competing_writers": [{"producer": producer, "actor_id": actor}
                                             for producer, actor in sorted(writers, key=str)] if len(writers) > 1 else [],
                      "event_ids": [event["event_id"] for event in matching],
                      "note": "Linkage uses explicit request/run/process identity only; no time-adjacency or spoken-OK causality is inferred."})
    return links


def query(sources, filters, contains=None, selectors=(), limit=20):
    rows, events, unavailable = [], [], []
    scanned = 0
    scanned_bytes = 0
    for metadata in sources:
        path = Path(metadata["path"])
        try:
            # Freeze exact bytes before querying. Appends cannot alter this result.
            raw = path.read_bytes()
        except OSError as error:
            unavailable.append({"path": str(path), "error": str(error)})
            continue
        scanned_bytes += len(raw)
        if metadata.get("response"):
            from cockpit_file_bridge import FileBackedCockpitBridge as Bridge
            session = path.parent.parent
            receipt = Bridge.response_status(session, path.stem, summary=False)
            if not receipt.get("ok"):
                unavailable.append({"path": str(path), "error": receipt})
                continue
            result = Bridge.response_artifact(session, path.stem, receipt["receipt"]["response_sha256"])
            if not result.get("ok"):
                unavailable.append({"path": str(path), "error": result})
                continue
            records = [(0, raw, result["response"])]
        else:
            records, offset = [], 0
            for line in raw.splitlines(keepends=True):
                if line.strip():
                    records.append((offset, line, parse(line)))
                offset += len(line)
        for offset, original, record in records:
            scanned += 1
            source = {"path": str(path), "offset": offset, "length": len(original),
                      "sha256": hashlib.sha256(original).hexdigest(),
                      "producer": metadata.get("producer", path.name)}
            for event in envelopes(record, source):
                events.append(event)
                try:
                    if any(select(event, k) != v for k, v in filters.items()):
                        continue
                except (KeyError, IndexError):
                    continue
                if contains is not None and contains.casefold() not in encoded(event).decode("utf-8").casefold():
                    continue
                if "retained_raw" not in source:
                    source["retained_raw"] = retain({"raw_base64": base64.b64encode(original).decode("ascii")})
                if selectors:
                    fields = {}
                    for selector in selectors:
                        try:
                            fields[selector] = select(event, selector)
                        except (KeyError, IndexError):
                            fields[selector] = {"unavailable": True}
                    rows.append({"event_id": event["event_id"], "source": source, "fields": fields})
                else:
                    rows.append(event)
    links = _request_result_links(events)
    snapshot = {"rows": rows, "links": links, "unavailable_sources": unavailable, "filters": filters,
                "contains": contains, "scanned_records": scanned, "scanned_bytes": scanned_bytes,
                "correlation": "Null means unavailable. Shared request IDs or actor names alone do not establish cross-process identity. Observations are not inferred changes."}
    artifact = retain(snapshot)
    return {"ok": True, "status": "partial" if unavailable else "matched" if rows else "no_match",
            "matched": len(rows), "rows": rows[:limit], "links": links, "unavailable_sources": unavailable,
            "scanned_records": scanned, "scanned_bytes": scanned_bytes,
            "snapshot": artifact, "next": {"sha256": artifact["sha256"], "selector": "rows",
            "offset": limit} if len(rows) > limit else None, "correlation": snapshot["correlation"]}
