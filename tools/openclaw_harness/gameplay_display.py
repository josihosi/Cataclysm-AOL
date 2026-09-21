"""Task-independent change display; native evidence and grants stay with the client."""
from collections import Counter
import json

from cockpit_evidence import action_catalog, compact, decode, gameplay_fact


def player_output(result):
    """Present decisions, keeping transport receipts in the retained request."""
    def clean(value):
        if isinstance(value, list):
            return [clean(item) for item in value]
        if not isinstance(value, dict):
            return value
        if value.get("omitted") is False and "preview" in value:
            return clean(value["preview"])
        result = {}
        for key, item in value.items():
            if key in {"receipt", "native_receipt", "accepted_receipt"}:
                if isinstance(item, dict):
                    result.update({field: item[field] for field in
                                   ("accepted", "rejection_reason", "outcome") if field in item})
            elif key not in {"authority", "source_selector", "actions_selector",
                             "identity_fields", "scope_note", "schema", "run_id", "binding_id",
                             "surface_id", "frame_id", "observation_id", "request_identity",
                             "evidence_handles", "expected_postcondition", "evidence_effect"} \
                    and not key.endswith("sha256"):
                result[key] = clean(item)
        return result

    view = result.get("response", result)
    output = {key: result[key] for key in ("ok", "state", "error", "reason") if key in result}
    if isinstance(view, dict):
        output.update({key: clean(value) for key, value in view.items()
                       if key not in {"authority", "receipt", "turn_assessment",
                                      "startup_diagnostics", "request_result", "retrieval"}})
    assessment = result.get("turn_assessment") or {}
    alerts = {key: assessment[key] for key in ("alarms", "recoveries") if assessment.get(key)}
    if alerts:
        output["performance"] = alerts
    for key in ("next", "request_id", "witness_fields"):
        if key in result:
            output[key] = result[key]
    return output


def bounded_player_output(result):
    """Use the existing output budget without printing its integrity hashes."""
    from evidence_display import bounded

    shown = bounded(player_output(result))
    shown.pop("presentation", None)
    # Omitted fields retain their exact byte counts. The request ID below
    # retrieves the original response through request-result/inspect.
    shown = player_output(shown)
    for key in ("ok", "state", "request_id", "next"):
        if key in result:
            shown[key] = result[key]
    return shown


def observation_path(response, path=""):
    for key in ("observation", "terminal_observation", "result"):
        value = response.get(key)
        if isinstance(value, dict):
            child = path + "." + key if path else key
            if isinstance(value.get("surface"), dict):
                return child
            found = observation_path(value, child)
            if found:
                return found
    return ""


def observation(response):
    for key in ("observation", "terminal_observation", "result"):
        value = response.get(key)
        if isinstance(value, dict):
            if isinstance(value.get("surface"), dict):
                return value
            found = observation(value)
            if found:
                return found
    return None


_ENTITY_ID_FIELDS = ("stable_id", "id", "uid", "handle", "character_id", "fixture_actor_id")


def _entity_identity(value):
    """Return a native stable identity without manufacturing one from order."""
    if not isinstance(value, dict):
        return None
    for field in _ENTITY_ID_FIELDS:
        candidate = value.get(field)
        if candidate not in (None, ""):
            return (field, str(candidate))
    return None


def _entity_delta(current, previous, selector):
    """Put changed nearby entities ahead of unchanged preview occupants.

    The complete current list remains available at ``selector``.  This small
    view deliberately compares only producer-provided stable identities: list
    position is not an identity and is never used to claim that an NPC changed.
    """
    if not isinstance(current, list) or not isinstance(previous, list):
        return gameplay_fact(current, selector)
    before = {_entity_identity(item): item for item in previous if _entity_identity(item) is not None}
    after = {_entity_identity(item): item for item in current if _entity_identity(item) is not None}
    changed = []
    for index, item in enumerate(current):
        identity = _entity_identity(item)
        if identity is None:
            # An unkeyed row cannot be compared safely.  Keep it available in
            # the exact source rather than pretending the list index is stable.
            continue
        if before.get(identity) != item:
            changed.append({"identity": {identity[0]: identity[1]}, "source_index": index,
                            "value": compact(item, f"{selector}.{index}")})
    removed = [{field: value} for field, value in before if (field, value) not in after]
    return {"selector": selector, "current_count": len(current),
            "identity_fields": list(_ENTITY_ID_FIELDS), "changed": changed,
            "removed": removed,
            "note": "Changed stable identities are shown before unchanged nearby entities; page the exact selector for the complete current list."}


def _message_delta(current, previous, selector):
    """Expose every new message event instead of grouping repeated text."""
    if not isinstance(current, list) or not isinstance(previous, list):
        return gameplay_fact(current, selector)
    # A multiset preserves repeated identical events.  A new actor, timestamp,
    # or payload is distinct; identical retained fixture history is not
    # re-presented merely because the frame was refreshed.
    previous_counts = Counter(json.dumps(item, sort_keys=True, ensure_ascii=False,
                                         separators=(",", ":")) for item in previous)
    events = []
    for index, item in enumerate(current):
        key = json.dumps(item, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
        if previous_counts[key]:
            previous_counts[key] -= 1
            continue
        events.append({"source_index": index, "value": compact(item, f"{selector}.{index}")})
    append_only = len(current) >= len(previous) and current[:len(previous)] == previous
    return {"selector": selector,
            "cursor": {"before_count": len(previous), "after_count": len(current),
                       "mode": "append" if append_only else "identity_delta"},
            "new_events": events, "new_event_count": len(events),
            "retained_history_count": len(previous),
            "scope_note": "These are events newly present in this retained frame; fixture/history origin is not inferred. Use time, actor and request facts before attribution."}


def display(response, previous=None, refresh=False):
    observed = observation(response)
    if not observed:
        return response, previous
    fact_prefix = observation_path(response) + ".surface.facts."
    surface = observed["surface"]
    owner = surface.get("kind")
    facts = {k: decode(v) for k, v in surface.get("facts", {}).items()}
    actions = surface.get("actions", [])
    identity = {k: observed.get(k) for k in ("run_id", "surface_id")}
    previous = previous or {}
    reset = refresh or previous.get("run_id") != observed.get("run_id")
    old = {} if reset else previous.get("world" if owner == "world" else "current", {})
    transition = reset or previous.get("owner") != owner
    old_facts = old.get("facts", {})
    removed = []
    def changes(new, old, path=""):
        result = {}
        for key, value in new.items():
            child_path = path + "." + key if path else key
            selector = fact_prefix + child_path
            if key not in old:
                if key in {"visible_entities", "visible_zones"}:
                    result[key] = _entity_delta(value, [], selector)
                elif key == "messages":
                    result[key] = _message_delta(value, [], selector)
                else:
                    result[key] = gameplay_fact(value, selector)
            elif value != old[key]:
                if isinstance(value, dict) and isinstance(old[key], dict):
                    result[key] = changes(value, old[key], child_path)
                elif key in {"visible_entities", "visible_zones"}:
                    result[key] = _entity_delta(value, old[key], selector)
                elif key == "messages":
                    result[key] = _message_delta(value, old[key], selector)
                else:
                    result[key] = gameplay_fact(value, selector)
        removed.extend(path + "." + key if path else key for key in old if key not in new)
        return result
    changed = changes(facts, old_facts)
    source_selector = observation_path(response)
    actions_selector = source_selector + ".surface.actions"
    current = {"owner": owner, "source_selector": source_selector,
               "actions_selector": actions_selector,
               "view": "full" if transition else "delta"}
    breadcrumbs = surface.get("breadcrumbs", observed.get("breadcrumbs", []))
    if transition or old.get("breadcrumbs") != breadcrumbs:
        current["breadcrumbs"] = breadcrumbs
    def action_key(action):
        return (action.get("id"), action.get("stable_id", ""))
    def action_view(action):
        return {k: v for k, v in action.items()
                if not (k == "stable_id" and not v) and not (k == "label" and v == action.get("id"))}
    if transition:
        catalog = action_catalog(actions, actions_selector)
        current["actions"] = catalog if catalog is not None else [action_view(a) for a in actions]
        current["navigation"] = [action_view(a) for a in actions if
            str(a.get("id", "")).rsplit(".", 1)[-1] in {"close", "cancel", "back", "done"}]
    elif old.get("actions") != actions:
        old_actions = {action_key(a): a for a in old.get("actions", [])}
        new_actions = {action_key(a): a for a in actions}
        current["actions_changed"] = [action_view(a) for key, a in new_actions.items()
                                      if old_actions.get(key) != a]
        current["actions_removed"] = [{"id": key[0], "stable_id": key[1]} for key in old_actions if key not in new_actions]
    # Prompt and selection changes are independent of game-time advancement.
    if owner != "world":
        current["facts_changed"] = changed
        current["facts_removed"] = removed
    outcome = {k: v for k, v in response.items() if k not in {
        "observation", "result", "receipt", "operation_availability"}}
    receipt = response.get("receipt")
    if isinstance(receipt, dict):
        native = receipt.get("native_receipt", receipt)
        outcome["native_receipt"] = {k: native[k] for k in
            ("request_id", "action_id", "accepted", "rejection_reason", "outcome") if k in native}
    result = response.get("result")
    if isinstance(result, dict) and "terminal_observation" in result:
        outcome["chain"] = {k: v for k, v in result.items() if k not in {
            "terminal_observation", "native_receipts", "handled_interruptions", "observations", "transcript"}}
    view = {"current_input": current, "outcome": outcome,
            "authority": {"run_id": observed.get("run_id"),
                          "observation_id": observed.get("observation_id")}}
    if owner == "world":
        view["facts_changed"] = changed
        view["facts_removed"] = removed
    for key in ("game_minutes", "game_turn", "game_time", "turn"):
        if key in observed and (reset or previous.get(key) != observed[key]):
            view[key] = {"before": previous.get(key), "after": observed[key]}
    snapshot = {**identity, "facts": facts, "actions": actions, "breadcrumbs": breadcrumbs}
    state = {**previous, "run_id": observed.get("run_id"), "owner": owner,
             "current": snapshot, **{k: observed[k] for k in ("game_minutes", "game_turn", "game_time", "turn") if k in observed}}
    if owner == "world":
        state["world"] = snapshot
    return view, state
