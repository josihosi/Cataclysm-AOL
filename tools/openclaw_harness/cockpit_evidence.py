"""Read-only projections of retained cockpit evidence (never gameplay proof inference)."""
from __future__ import annotations

import base64
import hashlib
import json
from pathlib import Path
from typing import Any

from cockpit_archive import ArchiveSequence


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
        elif isinstance(value, (list, ArchiveSequence)) and part.isdecimal():
            value = value[int(part)]
        else:
            raise KeyError(selector)
    return decode(value)


def select_optional(value: Any, selector: str) -> dict[str, Any]:
    """Read one optional field without turning an unavailable field into a fact.

    Retained native views intentionally have different schemas (and older
    records may not contain newer fields).  Callers need a stable result shape
    that distinguishes an absent field from a false/empty/zero value.
    """
    selector = str(selector).strip()
    if not selector:
        return {"selector": selector, "available": False, "reason": "selector_required"}
    try:
        return {"selector": selector, "available": True, "value": select(value, selector)}
    except (KeyError, IndexError, TypeError, ValueError):
        return {"selector": selector, "available": False, "reason": "field_unavailable"}


def selected_view(value: Any, selectors: list[str] | tuple[str, ...] = ()) -> dict[str, Any]:
    """Project explicitly selected fields while retaining the source identity.

    This is presentation-only: ``value`` is never rewritten and omitted
    fields remain explicitly unavailable.  The complete source can still be
    recovered through the caller's artifact handle.
    """
    normalized = []
    for selector in selectors:
        selector = str(selector).strip()
        if selector and selector not in normalized:
            normalized.append(selector)
    binding = observation_binding(value)
    return {"schema": "caol-selected-view-v1",
            "identity": {key: binding[key] for key in
                          ("run_id", "binding_id", "process_instance", "session_generation", "frame_id")},
            "freshness": {key: binding[key] for key in
                           ("turn", "game_minutes", "timestamp")},
            "selectors": {selector: select_optional(value, selector) for selector in normalized},
            "note": "Selected values are from one retained response; absent identity/freshness is unknown."}


_MISSING = object()


def _first_mapping(value: Any, *paths: str) -> Any:
    for path in paths:
        try:
            found = select(value, path)
        except (KeyError, IndexError, TypeError, ValueError):
            continue
        if found is not None:
            return found
    return None


def observation_binding(value: Any) -> dict[str, Any]:
    """Extract comparable identity from a response/observation without inference."""
    observed = value
    if isinstance(observed, dict):
        observed = observed.get("observation", observed.get("result", observed))
        if isinstance(observed, dict) and not observed.get("observation_id") and isinstance(observed.get("terminal_observation"), dict):
            observed = observed["terminal_observation"]
    observed = observed if isinstance(observed, dict) else {}
    receipt = value.get("receipt", {}) if isinstance(value, dict) else {}
    authority = value.get("authority", {}) if isinstance(value, dict) else {}
    if not isinstance(receipt, dict):
        receipt = {}
    if not isinstance(authority, dict):
        authority = {}
    native = receipt.get("native_receipt", {})
    native = native if isinstance(native, dict) else {}
    wall_time = observed.get("wall_time")
    if isinstance(wall_time, dict):
        timestamp = observed.get("timestamp", observed.get("created_at", wall_time.get("iso8601", wall_time.get("unix_ms", wall_time.get("unix_seconds")))))
    else:
        timestamp = observed.get("timestamp", observed.get("created_at"))
    process_instance = observed.get("process_instance", authority.get("process_instance", native.get("process_instance")))
    return {
        "run_id": observed.get("run_id", authority.get("run_id", native.get("run_id"))),
        "binding_id": observed.get("binding_id", receipt.get("binding_id", native.get("binding_id"))),
        "process_instance": process_instance,
        "session_generation": observed.get("session_generation", receipt.get("session_generation")),
        "frame_id": observed.get("frame_id", observed.get("observation_id", authority.get("observation_id"))),
        "turn": observed.get("game_turn", observed.get("turn")),
        "game_minutes": observed.get("game_minutes"),
        "timestamp": timestamp,
    }


def compare_selected(before: Any, after: Any, selectors: list[str] | tuple[str, ...] = ()) -> dict[str, Any]:
    """Compare selected native facts and classify uncertainty explicitly.

    A process/run/binding mismatch makes the views incompatible; no transient
    actor IDs are joined across such a boundary.  Same-content observations
    remain ``unchanged`` even when their timestamps differ.
    """
    before_binding, after_binding = observation_binding(before), observation_binding(after)
    binding_fields = ("run_id", "binding_id", "process_instance", "session_generation")
    incompatibilities = []
    for field in binding_fields:
        left, right = before_binding.get(field), after_binding.get(field)
        if left is not None and right is not None and left != right:
            incompatibilities.append({"field": field, "before": left, "after": right, "reason": "unequal"})
    compatible = not incompatibilities
    identity_unknown = [field for field in binding_fields
                        if before_binding.get(field) is None or after_binding.get(field) is None]
    normalized = []
    for selector in selectors:
        selector = str(selector).strip()
        if selector and selector not in normalized:
            normalized.append(selector)
    fields = {}
    summary = {name: [] for name in ("changed", "unchanged", "added", "removed", "unknown", "incompatible")}
    for selector in normalized:
        left, right = select_optional(before, selector), select_optional(after, selector)
        if not compatible:
            status = "incompatible"
        elif not left["available"] and not right["available"]:
            status = "unknown"
        elif not left["available"]:
            status = "added"
        elif not right["available"]:
            status = "removed"
        elif left["value"] == right["value"]:
            status = "unchanged"
        else:
            status = "changed"
        row = {"status": status, "before": left, "after": right}
        fields[selector] = row
        summary[status].append(selector)
    timestamp_changed = (before_binding.get("timestamp") is not None and
                         after_binding.get("timestamp") is not None and
                         before_binding.get("timestamp") != after_binding.get("timestamp"))
    return {
        "schema": "caol-selected-comparison-v1",
        "status": "compatible" if compatible else "incompatible",
        "binding": {"status": "compatible" if compatible and not identity_unknown else
                     "unknown" if compatible else "incompatible",
                     "before": before_binding, "after": after_binding,
                     "incompatibilities": incompatibilities,
                     "unknown_fields": identity_unknown},
        "fields": fields,
        "summary": summary,
        "timestamp_changed": timestamp_changed,
        "atomic": False,
        "note": "Different-time observations are not atomic snapshots; unchanged content with a changed timestamp remains unchanged.",
    }


# Descriptive aliases keep the helper usable from focused inspectors without
# creating another evidence/query framework.
select_view = selected_view
compare_observations = compare_selected


def describe(value: Any, path: str) -> dict[str, Any]:
    if isinstance(value, ArchiveSequence):
        return {"omitted": True, "selector": path, "type": "list",
                "count": len(value), "json_bytes": value.json_bytes}
    decoded = decode(value)
    result = {"omitted": True, "selector": path, "type": type(decoded).__name__,
              "json_bytes": len(json.dumps(value, ensure_ascii=False).encode())}
    if isinstance(decoded, (list, dict, str)):
        result["count"] = len(decoded)
    if isinstance(decoded, dict):
        result["fields"] = list(decoded)
    return result


def gameplay_fact(value: Any, path: str) -> Any:
    """Expose playable facts with pageable previews; full source stays retrievable."""
    decoded = decode(value)
    key = path.rsplit(".", 1)[-1]
    if not isinstance(decoded, (dict, list)):
        return compact(value, path)
    result = describe(value, path)
    if key in {"avatar", "avatar_status"} and isinstance(decoded, dict):
        result.update(preview=decoded, omitted=False)
    elif key == "avatar_effects" and isinstance(decoded, dict):
        entries = decoded.get("entries", {})
        if isinstance(entries, dict):
            names = [{"effect_id": effect_id, "body_part_id": part, "name": facts.get("name")}
                     for effect_id, parts in entries.items() if isinstance(parts, dict)
                     for part, facts in parts.items() if isinstance(facts, dict)]
            result.update(named_effects=names, effect_count=len(names),
                          detail="Exact descriptions and modifier sources remain at this selector.")
    elif key == "messages" and isinstance(decoded, list):
        groups = {}
        for index, message in enumerate(decoded):
            text = message.get("text") if isinstance(message, dict) else str(message)
            group = groups.setdefault(text, {"text": text, "count": 0, "first_index": index})
            group.update(count=group["count"] + 1, last_index=index,
                         last_time=message.get("time") if isinstance(message, dict) else None)
        ordered = sorted(groups.values(), key=lambda row: row["last_index"])
        result.update(preview=ordered[-5:], unique_messages=len(ordered),
                      preview_order="latest occurrences; identical text grouped",
                      omitted_groups=max(0, len(ordered) - 5))
    elif key == "visible_local" and isinstance(decoded, list):
        # These are the native immediate neighbours, needed to choose movement.
        result.update(preview=decoded, omitted=False)
    elif key in {"visible_entities", "visible_zones"} and isinstance(decoded, list):
        result.update(preview=decoded[:5], omitted=len(decoded) > 5,
                      next_offset=5 if len(decoded) > 5 else None)
    elif key == "minimap" and isinstance(decoded, dict) and isinstance(decoded.get("cells"), list):
        cells = decoded["cells"]
        if cells and all(isinstance(c, dict) and isinstance(c.get("dx"), int) and
                         isinstance(c.get("dy"), int) for c in cells):
            terrain_keys = sorted({(str(c.get("visibility", "unknown")), str(c.get("terrain", "unknown")))
                                   for c in cells})
            symbols = {key: str(index) for index, key in enumerate(terrain_keys)}
            grid = {(c["dx"], c["dy"]): symbols[(str(c.get("visibility", "unknown")),
                                                str(c.get("terrain", "unknown")))] for c in cells}
            xs = sorted({c["dx"] for c in cells}); ys = sorted({c["dy"] for c in cells})
            result["terrain_view"] = {
                "coordinate_system": "avatar-relative tiles; x east, y south",
                "x_offsets": xs, "y_offsets": ys,
                "rows": [" ".join(grid.get((x, y), "?") for x in xs) for y in ys],
                "legend": {symbol: {"visibility": key[0], "terrain": key[1]}
                           for key, symbol in symbols.items()},
                "details": "Terrain and visibility only; inspect the full cells for other fields. This does not assert passability.",
            }
    return result


def action_catalog(actions: list, path: str) -> Any:
    """Keep navigation visible while paging large native target catalogs."""
    targets = {}
    controls = []
    for index, action in enumerate(actions):
        if not isinstance(action, dict):
            return None
        identity = str(action.get("stable_id", ""))
        if not identity or identity == action.get("id"):
            controls.append(action)
        else:
            row = targets.setdefault(identity, {"target": identity, "actions": [], "source_indices": []})
            row["actions"].append(action)
            row["source_indices"].append(index)
    if len(targets) <= 5:
        return None
    rows = list(targets.values())
    preview = rows[:5]
    next_index = rows[5]["source_indices"][0]
    return {**describe(actions, path), "controls": controls, "target_count": len(rows),
            "targets_preview": preview, "next_offset": next_index,
            "paging": "inspect this selector with --contains NAME to find a target, or --offset/--limit to page original action rows. Controls remain visible; preview is five distinct targets."}


def current_input(value: dict, path: str) -> Any:
    for key in ("observation", "terminal_observation", "result"):
        observed = value.get(key)
        if not isinstance(observed, dict) or not isinstance(observed.get("surface"), dict):
            continue
        surface = observed["surface"]
        base = f"{path}.{key}" if path else key
        actions = surface.get("actions", [])
        navigation = [action for action in actions if isinstance(action, dict) and
                      action.get("enabled") is True and
                      str(action.get("id", "")).rsplit(".", 1)[-1] in {"cancel", "close", "back", "done"}]
        facts = surface.get("facts", {})
        facts = facts if isinstance(facts, dict) else {}
        selection = {key: decode(facts[key]) for key in
                     ("highlighted_item", "selected_items", "selected", "selected_index")
                     if key in facts}
        if selection and "selection_source" in facts:
            selection["selection_source"] = facts["selection_source"]
        controls = [action for action in actions if isinstance(action, dict) and
                    surface.get("kind") != "world" and
                    (surface.get("kind") in {"direction", "prompt", "string_prompt"} or not action.get("stable_id") or
                     action.get("stable_id") == action.get("id"))]
        return {"owner": surface.get("kind"), "frame_id": observed.get("observation_id"),
                "prompt": {key: facts[key] for key in ("title", "prompt", "description", "text", "filter")
                           if key in facts},
                "selection": selection or {"available": False,
                    "reason": "Current native owner supplies no selection fact; do not infer it from action order or filter text."},
                "controls": controls,
                "breadcrumbs": surface.get("breadcrumbs", observed.get("breadcrumbs", [])),
                "navigation": navigation, "actions_selector": base + ".surface.actions",
                "source_selector": base,
                "action_rule": "Choose actions from this current owner. World actions become usable after returning to World."}
    return None


def compact(value: Any, path: str = "") -> Any:
    """Keep decision scalars and action availability; expose bulky values as selectors.

    The string preview is a presentation default, not an evidence or acceptance limit.
    Structured native facts are discoverable without rendering maps or repeated messages.
    """
    if isinstance(value, ArchiveSequence):
        return describe(value, path)
    if isinstance(value, dict):
        active = current_input(value, path)
        result = {"current_input": active} if active is not None else {}
        for key, child in value.items():
            child_path = f"{path}.{key}" if path else key
            if key == "advertised_actions" and isinstance(value.get("surface"), dict):
                result[key] = {**describe(child, child_path), "available_in": (f"{path}.surface.actions" if path else "surface.actions")}
            elif key in {"advertised_action_details", "next_frame", "transition_event"}:
                result[key] = describe(child, child_path)
            elif key in {"facts", "payload"} and isinstance(child, dict):
                result[key] = {
                    k: gameplay_fact(v, f"{child_path}.{k}")
                    for k, v in child.items()
                }
            else:
                result[key] = compact(child, child_path)
        return result
    if isinstance(value, list):
        if path.rsplit(".", 1)[-1] in {"actions", "valid_actions"}:
            catalog = action_catalog(value, path)
            if catalog is not None:
                return catalog
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
          offset: int, limit: int, contains: str | None = None, snapshot: str | None = None) -> dict[str, Any]:
    """Filter parsed records before projection; pages never cut a JSON record in half."""
    from evidence_display import retain, recover
    try:
        if snapshot:
            manifest = recover(snapshot)
            if manifest.get("schema") != "caol-log-snapshot-v1":
                raise ValueError("invalid_log_snapshot")
            if manifest["filters"] != filters or manifest["selectors"] != selectors or manifest["contains"] != contains:
                raise ValueError("snapshot_query_changed")
        else:
            entries = []
            for path in paths:
                raw = path.read_bytes()
                entries.append({"path": str(path), "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()})
            manifest = {"schema": "caol-log-snapshot-v1", "sources": entries,
                        "filters": filters, "selectors": selectors, "contains": contains}
            snapshot = retain(manifest)["sha256"]
        paths = [Path(entry["path"]) for entry in manifest["sources"]]
    except (OSError, ValueError, KeyError) as error:
        return {"ok": False, "error": str(error)}
    rows = []
    matched = scanned = unparsed = unscoped = 0
    scanned_bytes = 0
    sources = []
    for path, bound in zip(paths, manifest["sources"]):
        try:
            with path.open("rb") as source:
                snapshot_bytes = bound["bytes"]
                if hashlib.sha256(source.read(snapshot_bytes)).hexdigest() != bound["sha256"]:
                    return {"ok": False, "error": "log_snapshot_source_changed", "path": str(path)}
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
            "snapshot": snapshot,
            "snapshot_paging": "Repeat this query with --snapshot HASH and --offset next_offset. Appends are excluded; replaced source prefixes fail hash verification.",
            "diagnostics": "Unparsed/unscoped records are counted, not attributed to a run. Query event=unparsed or event=text to inspect them; raw files are unchanged."}
