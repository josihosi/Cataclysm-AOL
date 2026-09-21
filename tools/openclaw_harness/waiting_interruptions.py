"""Choose native waiting continuations without interpreting translated prose."""


def continuation(surface, mode):
    if mode == "stop":
        return None
    actions = [a for a in surface.get("actions", []) if a.get("enabled", True)]
    facts = surface.get("facts", {})
    kind = surface.get("kind")
    title = facts.get("title")

    def only(candidates):
        return candidates[0] if len(candidates) == 1 else None

    # A notification with a single dismiss action has no gameplay choice.
    if kind in {"prompt", "modal"} and len(actions) == 1 and actions[0].get("id") in {
            "prompt.cancel", "prompt.acknowledge", "modal.acknowledge"}:
        return actions[0]
    if mode != "ignore":
        return None
    if kind == "activity_distraction":
        return only([a for a in actions if a.get("id") == "activity.ignore"])
    if kind != "prompt":
        return None
    if title == "CANCEL_ACTIVITY_OR_IGNORE_QUERY":
        return only([a for a in actions if a.get("id") == "prompt.choose" and a.get("label") == "IGNORE"])
    if title == "YESNO":
        # Only the activity's stop query may inherit an active wait's policy.
        if "Activity in progress" not in surface.get("breadcrumbs", []):
            return None
        return only([a for a in actions if a.get("id") == "prompt.choose" and a.get("label") == "NO"])
    if title == "YES_QUERY" and {a.get("label") for a in actions} == {"YES0", "YES1", "YES2"}:
        # portal_storm_query gives all three choices the same continuation.
        return only([a for a in actions if a.get("label") == "YES0"])
    return None
