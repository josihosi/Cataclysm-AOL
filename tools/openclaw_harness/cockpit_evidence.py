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


def resolve_surface_selector(value: Any, selector: str) -> dict[str, Any]:
    """Resolve the opt-in surface path within this response, never another frame."""
    if selector != "surface" and not selector.startswith("surface."):
        return {"ok": True, "resolved_selector": selector}
    candidates = []
    for prefix in ("", "observation", "result", "terminal_observation",
                   "observation.terminal_observation", "result.terminal_observation"):
        node = value
        try:
            for part in prefix.split(".") if prefix else ():
                node = node[part]
        except (KeyError, TypeError):
            continue
        if isinstance(node, dict) and isinstance(node.get("surface"), dict):
            candidates.append((prefix + "." if prefix else "") + selector)
    if len(candidates) != 1:
        return {"ok": False, "reason": "surface_ambiguous" if candidates else "surface_unavailable",
                "candidate_selectors": candidates}
    return {"ok": True, "resolved_selector": candidates[0]}


def select(value: Any, selector: str) -> Any:
    """Dot fields and numeric array indices; decode native JSON-string facts on demand."""
    resolved = resolve_surface_selector(value, selector)
    if not resolved["ok"]:
        raise KeyError(resolved["reason"])
    for part in resolved["resolved_selector"].split("."):
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
    resolved = resolve_surface_selector(value, selector)
    if not resolved["ok"]:
        return {"selector": selector, "available": False,
                "reason": resolved["reason"], "candidate_selectors": resolved["candidate_selectors"]}
    provenance = {"resolved_selector": resolved["resolved_selector"]} if selector == "surface" or selector.startswith("surface.") else {}
    try:
        return {"selector": selector, **provenance, "available": True, "value": select(value, resolved["resolved_selector"])}
    except (KeyError, IndexError, TypeError, ValueError):
        return {"selector": selector, **provenance, "available": False, "reason": "field_unavailable"}


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


def recursive_view(value: Any, selector: str, view: str, *, offset: int = 0,
                   limit: int = 20, contains: str | None = None,
                   source_role: str = "observation") -> dict[str, Any]:
    """Bounded rows from one selected serialized tree, including worn pockets.

    Recognition uses actual record keys; labels and unavailable subtrees are
    never interpreted as items or inventory absence. Pointers address decoded
    native JSON-string facts as well as ordinary JSON objects.
    """
    if view not in {"items", "actors"} or offset < 0 or limit <= 0:
        return {"ok": False, "error": "invalid_recursive_view_arguments"}
    root = select_optional(value, selector) if selector else {"available": True, "value": decode(value)}
    if not root["available"]:
        return {"ok": True, "available": False, "reason": root["reason"], "selector": selector,
                **{key: root[key] for key in ("resolved_selector", "candidate_selectors") if key in root}}
    rows = []
    fields = ("uid", "type_id", "charges", "owner") if view == "items" else (
        "id", "name", "location", "attitude", "dead", "mission", "sleep", "narcosis",
        "fleeing", "current_target")
    aliases = {"uid": ("uid", "item_uid", "_item_uid"), "type_id": ("typeid", "type_id"),
               "id": ("npc_id", "actor_id", "id"), "location": ("location", "pos", "absolute_ms")}

    def values(record):
        result = {}
        for field in fields:
            key = next((key for key in aliases.get(field, (field,)) if key in record), None)
            result[field] = {"available": key is not None}
            if key is not None:
                result[field].update({"value": record[key], "source_field": key})
        return result

    def visit(record, pointer, actor, containers):
        record = decode(record)
        if isinstance(record, dict):
            is_item = (isinstance(record.get("typeid"), str) or
                       (isinstance(record.get("type_id"), str) and
                        any(key in record for key in ("uid", "item_uid", "_item_uid"))))
            is_actor = not is_item and ("npc_id" in record or
                       (("actor_id" in record or "id" in record) and
                        any(key in record for key in ("inv", "worn", "attitude", "dead", "mission"))))
            if is_actor:
                actor = {"json_pointer": pointer, **{key: record[key] for key in
                         ("npc_id", "actor_id", "id", "name") if key in record}}
            if (view == "items" and is_item) or (view == "actors" and is_actor):
                row = {"json_pointer": pointer, "fields": values(record)}
                if "json_pointer" in record:
                    row["retained_source_json_pointer"] = record["json_pointer"]
                if view == "items":
                    row.update({"actor": actor, "containers": containers})
                if not contains or contains in json.dumps(row["fields"], ensure_ascii=False):
                    rows.append(row)
            if is_item:
                containers = [*containers, {"json_pointer": pointer,
                              **{key: record[key] for key in ("typeid", "type_id", "uid", "item_uid", "_item_uid") if key in record}}]
            for key, child in record.items():
                escaped = str(key).replace("~", "~0").replace("/", "~1")
                visit(child, pointer + "/" + escaped, actor, containers)
        elif isinstance(record, (list, ArchiveSequence)):
            for index, child in enumerate(record):
                visit(child, pointer + "/" + str(index), actor, containers)

    visit(root["value"], "", None, [])
    end = min(offset + limit, len(rows))
    return {"ok": True, "schema": "caol-recursive-view-v1", "available": True,
            "selector": selector, **{key: root[key] for key in ("resolved_selector",) if key in root},
            "view": view, "source_role": source_role,
            "source_role_provenance": "caller_label_not_inferred_from_path_or_contents",
            "identity": observation_binding(value), "matched_rows": len(rows), "offset": offset,
            "rows": rows[offset:end], "next_offset": end if end < len(rows) else None,
            "coverage": "Selected tree only; missing fields/subtrees are unavailable, not absent. "
                        "Item recognition requires typeid/type_id, not a group/container label. "
                        "Pointers are relative to selector, with JSON-string facts decoded."}


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
    if decoded == [] or decoded == {}:
        return decoded
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


_TRACE_EVENTS = frozenset({"raid_actor_action", "raid_site_search", "raid_trace_scope",
                           "raid_trace_repeat", "raid_trace_truncated", "raid_actor_damage",
                           "raid_actor_death", "raid_actor_sleep", "raid_site_route_probe",
                           "npc_attack_callback", "bandit_shakedown_contact", "bandit_shakedown_fight_reply"})
_ATTITUDE_LABELS = ("ignore", "talk", "legacy_1", "follow", "legacy_2", "lead",
                    "wait", "legacy_6", "mug", "wait_for_leave", "kill", "flee",
                    "legacy_3", "heal", "legacy_4", "legacy_5", "activity",
                    "flee_temp", "recover_goods")
_MISSION_LABELS = ("none", "legacy_1", "shelter", "shopkeep", "legacy_2",
                   "legacy_3", "guard_ally", "guard", "guard_patrol", "activity",
                   "travelling", "camp_resident")


def _trace_repeat_scope(record: dict[str, Any]) -> tuple[str, int, str] | None:
    key = str(record.get("key", ""))
    if key.startswith("scenario@"):
        group, separator, member = key[len("scenario@"):].rpartition(":")
        if separator and group and member:
            return group, -1, member
        return None
    # The recorder's key is operation_id + '#' + generation + ':' + actor/search.
    # The operation ID itself can contain '#', so split only at the final one.
    operation, separator, suffix = str(record.get("key", "")).rpartition("#")
    generation, colon, member = suffix.partition(":")
    if separator and colon and generation.isdecimal() and member:
        return operation, int(generation), member
    return None


def _trace_base_key(record: dict[str, Any]) -> str | None:
    """Reconstruct the recorder key carried by a repeat's preceding base row."""
    kind = record.get("event")
    group = record.get("trace_group_id")
    if kind == "raid_trace_scope" and isinstance(group, str) and group:
        return f"scenario@{group}:scope"
    operation, generation = record.get("operation_id"), record.get("generation")
    if isinstance(operation, str) and operation and isinstance(generation, int):
        if kind == "raid_site_search":
            return f"{operation}#{generation}:search"
        if kind in {"raid_actor_action", "raid_site_route_probe"} and isinstance(record.get("npc_id"), int):
            return f"{operation}#{generation}:{record['npc_id']}"
    if kind in {"raid_actor_action", "raid_site_route_probe"} and isinstance(group, str) and group and isinstance(record.get("npc_id"), int):
        return f"scenario@{group}:{record['npc_id']}"
    return None


def _trace_matches(record: dict[str, Any], decision: dict[str, Any],
                   repeat_groups: dict[str, str]) -> bool:
    kind = record.get("event")
    if kind not in _TRACE_EVENTS:
        return False
    if kind == "raid_trace_truncated":
        # The recorder cap is run-wide and has no operation or turn identity.
        return True
    group_id = decision.get("group_id")
    if kind == "raid_trace_scope":
        return bool(group_id and record.get("trace_group_id") == group_id)
    if kind == "raid_trace_repeat":
        scope = _trace_repeat_scope(record)
        if scope is None:
            return False
        if group_id:
            if scope[1] == -1:
                if scope[0] != group_id:
                    return False
            elif repeat_groups.get(str(record.get("key"))) != group_id:
                return False
        elif scope[1] == -1 or scope[0] != decision["operation_id"]:
            return False
        if scope[2] != "search" and decision["actor_ids"] and (
                not scope[2].isdecimal() or int(scope[2]) not in decision["actor_ids"]):
            return False
        first, last = record.get("first_turn"), record.get("last_turn")
    else:
        if group_id and record.get("trace_group_id") != group_id:
            return False
        if not group_id and record.get("operation_id") != decision["operation_id"]:
            return False
        # A group decision can affect every selected actor, even if a different
        # member triggered it. Keep it beside the individual decisions.
        if decision["actor_ids"]:
            endpoints = ({record.get("npc_id")} if kind in {
                "raid_actor_action", "raid_actor_sleep", "raid_site_route_probe"}
                         else {record.get("selected_source_npc_id"), record.get("selected_victim_npc_id")}
                         if kind in {"raid_actor_damage", "npc_attack_callback"}
                         else {record.get("selected_killer_npc_id"), record.get("selected_victim_npc_id")}
                         if kind == "raid_actor_death"
                         else {record.get("speaker_id"), record.get("receiver_id")}
                         if kind == "bandit_shakedown_contact"
                         else {record.get("receiver_id")} if kind == "bandit_shakedown_fight_reply"
                         else set(decision["actor_ids"]))
            if not endpoints.intersection(decision["actor_ids"]):
                return False
        first = last = record.get("game_turn")
    if not isinstance(first, int) or not isinstance(last, int):
        return False
    return ((decision["from_turn"] is None or last >= decision["from_turn"]) and
            (decision["to_turn"] is None or first <= decision["to_turn"]))


def _trace_project(record: dict[str, Any]) -> dict[str, Any]:
    kind = record["event"]
    if kind == "raid_actor_action":
        attitude, mission = record.get("attitude"), record.get("mission")
        return {"kind": "actor", "turn": record.get("game_turn"),
                "actor_id": record.get("npc_id"), "action": record.get("action"),
                "actor_role": record.get("actor_role"),
                "decision_origin": ("execute_action call site; not the reason for the choice"
                                    if record.get("reason") == "execute_action" else record.get("reason")),
                "attitude": (_ATTITUDE_LABELS[attitude] if isinstance(attitude, int) and
                             0 <= attitude < len(_ATTITUDE_LABELS) else f"unknown({attitude})"),
                "mission": (_MISSION_LABELS[mission] if isinstance(mission, int) and
                            0 <= mission < len(_MISSION_LABELS) else f"unknown({mission})"),
                "target": record.get("target"), "target_visible": record.get("target_visible"),
                "avatar_visible": record.get("avatar_visible"),
                "position_abs": record.get("position_abs"), "goto_abs": record.get("goto_abs"),
                "path_next_local": record.get("path_next_local"),
                "path_next_passable": record.get("path_next_passable"),
                "path_length": record.get("path_length"), "bravery": record.get("bravery"),
                "panic": record.get("panic"), "flee": record.get("flee"),
                "fire_danger": record.get("fire_danger"),
                "danger": record.get("danger"), "sound_alerts": record.get("sound_alerts"),
                "assigned_camp": record.get("assigned_camp"),
                "has_job": record.get("has_job"), "patrol_priority": record.get("patrol_priority"),
                "patrol_order": record.get("patrol_order"),
                "guard_pos_abs": record.get("guard_pos_abs"),
                "outing_member": record.get("outing_member"),
                "outing_id": record.get("outing_id"), "outing_kind": record.get("outing_kind"),
                "outing_phase": record.get("outing_phase"), "outing_owner": record.get("outing_owner"),
                "outing_waypoint_index": record.get("outing_waypoint_index"),
                "operation_phase": record.get("operation_phase"),
                "operation_owner": record.get("operation_owner"),
                "route_waypoint_index": record.get("route_waypoint_index"),
                "actor_route_waypoint": record.get("actor_route_waypoint"),
                "site_search_waypoint": record.get("site_search_waypoint")}
    if kind == "bandit_shakedown_contact":
        fields = ("operation_id", "generation", "site_id", "speaker_id", "receiver_id",
                  "receiver_is_avatar", "audible_shout", "unseen_receiver")
        return {"kind": "shakedown_contact", "turn": record.get("game_turn"),
                **{field: record.get(field) for field in fields}}
    if kind == "bandit_shakedown_fight_reply":
        fields = ("operation_id", "generation", "site_id", "receiver_id", "receiver_is_avatar",
                  "position_abs", "attempted", "emitted", "native_volume")
        return {"kind": "fight_reply", "turn": record.get("game_turn"),
                **{field: record.get(field) for field in fields}}
    if kind == "npc_attack_callback":
        fields = ("edge", "source", "victim", "selected_source_npc_id",
                  "selected_victim_npc_id", "attacker_player_ally", "attitude_before",
                  "attitude_after", "hit_by_player_before", "hit_by_player_after",
                  "shakedown_released", "active_covert_scout", "anger_branch",
                  "alarm_invoked", "alarm_raised", "victim_operation", "source_operation",
                  "victim_operation_after")
        return {"kind": "attack_callback", "turn": record.get("game_turn"),
                **{field: record.get(field) for field in fields}}
    if kind == "raid_actor_damage":
        return {"kind": "damage", "turn": record.get("game_turn"),
                "source": record.get("source"), "victim": record.get("victim"),
                "selected_source_npc_id": record.get("selected_source_npc_id"),
                "selected_victim_npc_id": record.get("selected_victim_npc_id"),
                "body_part": record.get("body_part"), "damage_kind": record.get("damage_kind"),
                "hp_before": record.get("hp_before"), "hp_after": record.get("hp_after"),
                "applied_damage": record.get("applied_damage"),
                "operation_phase": record.get("operation_phase"),
                "operation_owner": record.get("operation_owner"),
                "route_waypoint_index": record.get("route_waypoint_index")}
    if kind == "raid_actor_death":
        return {"kind": "death", "turn": record.get("game_turn"),
                "killer": record.get("killer"), "victim": record.get("victim"),
                "selected_killer_npc_id": record.get("selected_killer_npc_id"),
                "selected_victim_npc_id": record.get("selected_victim_npc_id"),
                "confirmed": record.get("confirmed"),
                "operation_phase": record.get("operation_phase"),
                "operation_owner": record.get("operation_owner"),
                "route_waypoint_index": record.get("route_waypoint_index")}
    if kind == "raid_actor_sleep":
        return {"kind": "sleep", "turn": record.get("game_turn"),
                "actor_id": record.get("npc_id"), "actor": record.get("actor"),
                "edge": record.get("edge"), "available_reason": record.get("available_reason"),
                "cause": record.get("cause"), "narcosis": record.get("narcosis"),
                "sleep_effect": record.get("sleep_effect"),
                "sleepiness": record.get("sleepiness"),
                "operation_phase": record.get("operation_phase"),
                "operation_owner": record.get("operation_owner"),
                "route_waypoint_index": record.get("route_waypoint_index")}
    if kind == "raid_site_search":
        return {"kind": "search", "turn": record.get("game_turn"), "reason": record.get("reason"),
                "trigger_actor_id": record.get("trigger_actor_id"),
                "seen_target": record.get("seen_target"), "waypoint": record.get("waypoint"),
                "waypoint_attempted": record.get("waypoint_attempted"),
                "recipients": record.get("recipients")}
    if kind == "raid_site_route_probe":
        return {"kind": "route_probe", "turn": record.get("game_turn"),
                "trace_group_id": record.get("trace_group_id"),
                "operation_id": record.get("operation_id"),
                "generation": record.get("generation"), "site_id": record.get("site_id"),
                "actor_id": record.get("npc_id"), "member_index": record.get("member_index"),
                "candidate": record.get("candidate"), "purpose": record.get("purpose"),
                "engaged": record.get("engaged"), "position_abs": record.get("position_abs"),
                "goal_abs": record.get("goal_abs"), "goal_local": record.get("goal_local"),
                "already_at_goal": record.get("already_at_goal"),
                "goal_inbounds": record.get("goal_inbounds"),
                "goal_passable": record.get("goal_passable"),
                "route_evaluated": record.get("route_evaluated"),
                "route_found": record.get("route_found"), "route_length": record.get("route_length"),
                "diagnosis": record.get("diagnosis"),
                "avoid_rejected_checks": record.get("avoid_rejected_checks"),
                "occupied_rejected_checks": record.get("occupied_rejected_checks"),
                "other_avoid_rejected_checks": record.get("other_avoid_rejected_checks"),
                "avoid_samples_truncated": record.get("avoid_samples_truncated"),
                "relaxed_route_evaluated": record.get("relaxed_route_evaluated"),
                "relaxed_route_found": record.get("relaxed_route_found"),
                "relaxed_route_length": record.get("relaxed_route_length"),
                "relaxed_route_basis": record.get("relaxed_route_basis"),
                "avoid_samples": record.get("avoid_samples")}
    if kind == "raid_trace_repeat":
        scope = _trace_repeat_scope(record)
        return {"kind": "repeat", "base_turn": record.get("base_turn"),
                "first_turn": record.get("first_turn"),
                "last_turn": record.get("last_turn"), "count": record.get("count"),
                "of_event": record.get("of_event"), "generation": scope[1] if scope else None,
                "member": scope[2] if scope else None}
    if kind == "raid_trace_scope":
        return {"kind": "scope", "turn": record.get("game_turn"),
                "group_id": record.get("trace_group_id"),
                "selected_npc_ids": record.get("selected_npc_ids"),
                "capture_from_turn": record.get("capture_from_turn"),
                "capture_to_turn": record.get("capture_to_turn"),
                "selection": record.get("selection")}
    return {"kind": "capture_truncated", "row_budget": record.get("row_budget"),
            "decisions": record.get("decisions"), "written_rows": record.get("written_rows"),
            "scope": record.get("scope"), "unpreserved_rows": record.get("unpreserved_rows")}


def query(paths: list[Path], filters: dict[str, Any], selectors: list[str],
          offset: int, limit: int, contains: str | None = None, snapshot: str | None = None,
          decision: dict[str, Any] | None = None) -> dict[str, Any]:
    """Filter parsed records before projection; pages never cut a JSON record in half."""
    from evidence_display import retain, recover
    try:
        if decision is not None:
            if (not isinstance(filters.get("run_id"), str) or not filters["run_id"] or
                    bool(decision.get("operation_id")) == bool(decision.get("group_id")) or
                    any(not isinstance(actor, int) for actor in decision["actor_ids"]) or
                    (decision["from_turn"] is not None and decision["to_turn"] is not None and
                     decision["from_turn"] > decision["to_turn"])):
                raise ValueError("invalid_decision_scope")
        if snapshot:
            manifest = recover(snapshot)
            if manifest.get("schema") != "caol-log-snapshot-v1":
                raise ValueError("invalid_log_snapshot")
            if (manifest["filters"] != filters or manifest["selectors"] != selectors or
                    manifest["contains"] != contains or manifest.get("decision") != decision):
                raise ValueError("snapshot_query_changed")
        else:
            entries = []
            for path in paths:
                raw = path.read_bytes()
                entries.append({"path": str(path), "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()})
            manifest = {"schema": "caol-log-snapshot-v1", "sources": entries,
                        "filters": filters, "selectors": selectors, "contains": contains}
            if decision is not None:
                manifest["decision"] = decision
            snapshot = retain(manifest)["sha256"]
        paths = [Path(entry["path"]) for entry in manifest["sources"]]
    except (OSError, ValueError, KeyError) as error:
        return {"ok": False, "error": str(error)}
    rows = []
    matched = scanned = unparsed = unscoped = 0
    scanned_bytes = 0
    sources = []
    trace_counts: dict[str, int] = {}
    trace_actors: dict[str, str] = {}
    trace_last_turn: dict[str, int] = {}
    trace_cap: list[dict[str, Any]] = []
    invalid_repeat_keys = 0
    orphan_repeat_bases = 0
    repeat_groups: dict[str, str] = {}
    trace_bases: dict[tuple[str, str], dict[str, Any]] = {}
    selected_npc_ids: list[int] = []
    selected_capture_window: dict[str, int | None] = {}
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
                    if decision is not None and record.get("run_id") == filters["run_id"] and record.get("event") in _TRACE_EVENTS:
                        kind = record["event"]
                        trace_counts[kind] = trace_counts.get(kind, 0) + 1
                        base_key = _trace_base_key(record)
                        if base_key is not None:
                            trace_bases[(base_key, kind)] = {
                                "path": str(path), "offset": position, "length": len(raw),
                                "sha256": hashlib.sha256(raw).hexdigest()}
                        if (kind == "raid_trace_scope" and decision.get("group_id") == record.get("trace_group_id") and
                                isinstance(record.get("selected_npc_ids"), list)):
                            selected_npc_ids = [actor for actor in record["selected_npc_ids"] if isinstance(actor, int)]
                            selected_capture_window = {"from_turn": record.get("capture_from_turn"),
                                                       "to_turn": record.get("capture_to_turn")}
                        if kind == "raid_actor_action" and isinstance(record.get("trace_group_id"), str) and record.get("operation_id"):
                            repeat_groups[f"{record['operation_id']}#{record.get('generation')}:{record.get('npc_id')}"] = record["trace_group_id"]
                        if kind == "raid_trace_truncated":
                            trace_cap.append({key: record.get(key) for key in
                                              ("row_budget", "decisions", "written_rows",
                                               "scope", "unpreserved_rows")})
                        elif kind == "raid_trace_repeat" and _trace_repeat_scope(record) is None:
                            invalid_repeat_keys += 1
                        if (kind == "raid_actor_action" and
                                record.get("trace_group_id" if decision.get("group_id") else "operation_id") ==
                                (decision.get("group_id") or decision.get("operation_id")) and
                                isinstance(record.get("npc_id"), int)):
                            trace_actors[str(record["npc_id"])] = str(record.get("npc_name") or "name unavailable")
                        trace_turn = record.get("last_turn", record.get("game_turn"))
                        if isinstance(trace_turn, int):
                            trace_last_turn[kind] = max(trace_last_turn.get(kind, trace_turn), trace_turn)
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
                    if decision is not None and not _trace_matches(record, decision, repeat_groups):
                        continue
                    if contains is not None and contains.casefold() not in raw.decode("utf-8", errors="replace").casefold():
                        continue
                    repeat_base = None
                    if decision is not None and record.get("event") == "raid_trace_repeat":
                        repeat_base = trace_bases.get((str(record.get("key")), str(record.get("of_event"))))
                        orphan_repeat_bases += repeat_base is None
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
                        projected = _trace_project(record) if decision is not None else compact(record)
                        if decision is not None and projected.get("kind") == "repeat":
                            projected["base_missing"] = repeat_base is None
                            projected["base_artifact"] = repeat_base
                    rows.append({"artifact": handle, "record": projected})
        except OSError as error:
            return {"ok": False, "error": str(error), "sources": sources}
    result = {"ok": True, "sources_on_page": sources, "source_count": len(paths),
            "source_bytes": scanned_bytes, "filters": filters, "contains": contains, "scanned": scanned,
            "matched": matched, "unparsed_records": unparsed, "records_without_run_id": unscoped,
            "rows": rows, "page": {"offset": offset, "limit": limit,
            "omitted_matches": matched - len(rows),
            "next_offset": offset + limit if offset + limit < matched else None},
            "retrieval": "record-artifact --path PATH --offset OFFSET --length LENGTH --sha256 SHA256 [--select FIELD]",
            "snapshot": snapshot,
            "snapshot_paging": "Repeat this query with --snapshot HASH and --offset next_offset. Appends are excluded; replaced source prefixes fail hash verification.",
            "diagnostics": "Unparsed/unscoped records are counted, not attributed to a run. Query event=unparsed or event=text to inspect them; raw files are unchanged."}
    if decision is not None:
        result["decision_trace"] = {"run_id": filters["run_id"], **decision,
                                    "actors": trace_actors, "captured_event_counts": trace_counts,
                                    "selected_npc_ids": selected_npc_ids,
                                    "selected_capture_window": selected_capture_window,
                                    "last_recorded_turn_by_event": trace_last_turn,
                                    "invalid_repeat_keys": invalid_repeat_keys,
                                    "orphan_repeat_bases": orphan_repeat_bases,
                                    "capture_truncation": trace_cap,
                                    "native_cap": "The opt-in recorder defaults to 1024 rows with space reserved for combat and sleep edges. Recorder and log-window truncation rows state any observed gap.",
                                    "tail": "Final repeats may be unflushed; absence of later rows does not establish an actor stopped acting.",
                                    "group_scope": "Search rows are included for the operation even when a different actor triggered them.",
                                    "display_limit": {"offset": offset, "limit": limit,
                                                      "matched": matched, "shown": len(rows)}}
    return result
