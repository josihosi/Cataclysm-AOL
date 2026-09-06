"""Task-independent change display; native evidence and grants stay with the client."""
from cockpit_evidence import decode, gameplay_fact


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
            if key not in old:
                result[key] = gameplay_fact(value, fact_prefix + child_path)
            elif value != old[key]:
                if isinstance(value, dict) and isinstance(old[key], dict):
                    result[key] = changes(value, old[key], child_path)
                else:
                    result[key] = gameplay_fact(value, fact_prefix + child_path)
        removed.extend(path + "." + key if path else key for key in old if key not in new)
        return result
    changed = changes(facts, old_facts)
    current = {"owner": owner}
    breadcrumbs = surface.get("breadcrumbs", observed.get("breadcrumbs", []))
    if transition or old.get("breadcrumbs") != breadcrumbs:
        current["breadcrumbs"] = breadcrumbs
    def action_key(action):
        return (action.get("id"), action.get("stable_id", ""))
    def action_view(action):
        return {k: v for k, v in action.items()
                if not (k == "stable_id" and not v) and not (k == "label" and v == action.get("id"))}
    if transition:
        current["actions"] = [action_view(a) for a in actions]
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
            "terminal_observation", "native_receipts", "observations", "transcript"}}
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
