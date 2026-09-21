"""Task-independent change display; native evidence and grants stay with the client."""
from collections import Counter
import json
import re
import shlex

from cockpit_evidence import action_catalog, compact, decode, gameplay_fact


def world_look(snapshot):
    """Present an existing World snapshot, using only its advertised controls."""
    current = snapshot.get("current", {})
    facts = current.get("facts", {})
    actions = [action for action in current.get("actions", []) if action.get("enabled", True)]
    lines = ["YOU"]
    clean = lambda value: re.sub(r"</?color[^>]*>", "", str(value))
    avatar = facts.get("avatar", {})
    status = facts.get("avatar_status", {})
    lines.append(" · ".join(clean(value) for value in
                          (avatar.get("name"), facts.get("world_mode")) if value))
    position = avatar.get("absolute_ms")
    if position is not None:
        lines.append("Position: " + ", ".join(str(value) for value in position))
    parts = status.get("health", {}).get("body_parts", {})
    if parts:
        health = {(part.get("current"), part.get("maximum")) for part in parts.values()}
        if len(health) == 1:
            hp, maximum = next(iter(health))
            lines.append(f"All {len(parts)} body parts: {hp}/{maximum} HP")
        else:
            lines.append("HP: " + " · ".join(
                f"{part.get('name', key)} {part.get('current')}/{part.get('maximum')}"
                for key, part in parts.items()))
    stamina = status.get("stamina", {})
    if "current" in stamina and "maximum" in stamina:
        lines.append(f"Stamina: {stamina['current']}/{stamina['maximum']}")
    weapon = status.get("weapon")
    if isinstance(weapon, dict):
        lines.append("Weapon: " + " ".join(clean(weapon[key]) for key in ("name", "mode", "ammo")
                                            if weapon.get(key)))
    needs = status.get("needs", {})
    conditions = [clean(needs[key].get("text", "")) for key in
                  ("hunger", "thirst", "sleepiness", "pain", "weariness") if needs.get(key, {}).get("text")]
    if conditions:
        lines.append(" · ".join(conditions))
    effects = facts.get("avatar_effects", {}).get("entries", {})
    names = list(dict.fromkeys(clean(part["name"]) for effect, parts_by_body in effects.items()
                              if effect != "wet" for part in parts_by_body.values() if part.get("name")))
    if names:
        lines.append("Effects: " + " · ".join(names))

    lines.extend(["", "SURROUNDINGS"])
    tiles = facts.get("visible_local")
    if isinstance(tiles, list):
        groups = {}
        for tile in tiles:
            dx, dy = tile.get("dx"), tile.get("dy")
            if not isinstance(dx, int) or not isinstance(dy, int):
                continue
            direction = ("N" if dy < 0 else "S" if dy > 0 else "") + ("W" if dx < 0 else "E" if dx > 0 else "")
            description = clean(tile.get("terrain", tile.get("visibility", "unknown")))
            if tile.get("furniture"):
                description += "; " + clean(tile["furniture"])
            if tile.get("fields"):
                description += "; " + ", ".join(clean(field) for field in tile["fields"])
            groups.setdefault(description, []).append(direction or "here")
        for description, directions in groups.items():
            lines.append(description + ": " + ", ".join(directions))
    entities = facts.get("visible_entities")
    if isinstance(entities, list):
        if not entities:
            lines.append("No creatures in the current visible-entity observation.")
        for entity in entities:
            offsets = []
            for key, positive, negative in (("dx", "east", "west"), ("dy", "south", "north")):
                value = entity.get(key)
                if isinstance(value, (int, float)) and value:
                    offsets.append(f"{abs(value):g} {positive if value > 0 else negative}")
            identity = entity.get("identity", {}).get("id", "")
            lines.append(" · ".join(filter(None, (clean(entity.get("name", entity.get("kind", "Creature"))),
                         entity.get("attitude", ""), ", ".join(offsets) or "here", identity))))
    if not isinstance(tiles, list) and not isinstance(entities, list):
        lines.append("Surroundings unavailable in this observation.")

    remaining = list(actions)
    def take(predicate):
        selected = [action for action in remaining if predicate(action)]
        remaining[:] = [action for action in remaining if not predicate(action)]
        return selected

    movement = take(lambda action: action.get("id", "").startswith("world.move.") and not action.get("stable_id"))
    if movement:
        lines.extend(["", "MOVE — one step", "play act world.move.<direction>",
                      " / ".join(action["id"].removeprefix("world.move.") for action in movement),
                      "Occupied tiles may trigger interaction or attack.",
                      "", "MOVE — multiple steps (when allowed by the scenario)",
                      'play move --east 3 --south -2 --bound-maximum 5 --bound-basis path_progress --bound-source "3 east, 2 north"',
                      "3 east, then 2 north. Negative east = west; negative south = north.",
                      "Stops on interruptions by default; reports partial progress."])
    groups = {
        "Combat": {"fire", "reload", "toggle_safemode"},
        "Items": {"inventory", "pickup", "drop"},
        "Observe": {"look", "overmap", "messages"},
        "Interact": {"chat", "zone_manager"},
        "Vertical": {"level_up", "level_down"},
        "Save": {"quicksave", "save_quit"},
        "Debug": {"debug_menu"},
    }
    lines.extend(["", "ACTIONS — play act world.<action>"])
    for label, names in groups.items():
        selected = take(lambda action: action.get("id", "").removeprefix("world.") in names
                        and action.get("id", "").startswith("world.") and not action.get("stable_id"))
        if selected:
            lines.append(label + ": " + " / ".join(action["id"].removeprefix("world.") for action in selected))
    people = take(lambda action: action.get("id") in
                  {"world.inspect_npc", "world.inspect_camp_npc", "world.basecamp_missions"})
    if people:
        lines.extend(["", "PEOPLE", "Copy the target listed under the exact command; target formats differ."])
        for action_id in dict.fromkeys(action["id"] for action in people):
            offered = [action for action in people if action["id"] == action_id]
            lines.append(f"play act {action_id} --target <target>")
            lines.extend("  " + action.get("stable_id", "") + " — " + clean(action.get("label", action_id))
                         for action in offered)
    waiting = take(lambda action: action.get("id") == "world.wait" and not action.get("stable_id"))
    if waiting:
        lines.extend(["", "WAIT — when allowed by the scenario", "play wait <duration> [ignore|safe|stop]",
                      "1m / 5m / 30m / 1h / 2h / 3h / 6h — native availability applies; hours require a watch.",
                      "ignore: danger and interruptions (default); safe: classified harmless interruptions; stop: stop at interruptions.",
                      "Native wait menu: play act world.wait"])
    if remaining:
        lines.extend(["", "OTHER ADVERTISED ACTIONS"])
        for action in remaining:
            command = "play act " + shlex.quote(action["id"])
            if action.get("stable_id"):
                command += " --target " + shlex.quote(action["stable_id"])
            lines.append(clean(action.get("label", action["id"])) + " → " + command)
    lines.extend(["", "SESSION", "play look — observe · play collect — retrieve pending result · play quit — end playtest"])
    return "\n".join(line for line in lines if line is not None)


def _plain_text(value):
    return re.sub(r"</?color[^>]*>", "", str(value))


def _plain_controls(actions):
    """Print every advertised choice once, sharing command syntax across targets."""
    lines = ["ALLOWED ACTIONS IN THIS VIEW", "Only these actions are available until this view changes."]
    targeted = {}
    ids = {action["id"] for action in actions}
    if {"inventory.toggle", "inventory.commit"} <= ids:
        lines.append("toggle: mark/unmark an item; commit: perform the labelled action on marked items. select confirms; it does not mark items.")
    elif "inventory.toggle" in ids:
        toggle = next(action for action in actions if action["id"] == "inventory.toggle")
        lines.append("toggle: " + _plain_text(toggle.get("label", "Toggle")))
    elif "inventory.select" in ids:
        lines.append("select: confirm an item; details: inspect it.")
    if "menu.select" in ids and "menu.choose" in ids:
        lines.append("select: highlight; choose: activate the option.")
    for action in actions:
        action_id = action["id"]
        target = action.get("stable_id", "")
        if target:
            namespace, _, verb = action_id.rpartition(".")
            targeted.setdefault(namespace, {}).setdefault(target, []).append((verb, action))
            continue
        label = _plain_text(action.get("label", action_id))
        if not action.get("enabled", True):
            lines.append(label + " (unavailable)")
            continue
        command = "play act " + shlex.quote(action_id)
        if action_id in {"menu.filter", "inventory.filter"}:
            command += " --param text=TEXT"
        elif action_id == "activity.pause":
            command = "play stop"
        elif action_id in {"npc_inspection.item_details", "camp_npc_inspection.item_details"}:
            command += " --param item_uid=ID"
        lines.append(label + " → " + command)
    for namespace, targets in targeted.items():
        if namespace == "prompt":
            for target, choices in targets.items():
                for verb, action in choices:
                    label = _plain_text(action.get("label", verb))
                    command = "play " + label.lower() if label.lower() in {"yes", "no", "ignore"} else (
                        f"play act prompt.{verb} --target " + shlex.quote(target))
                    lines.append(label + (" → " + command if action.get("enabled", True) else " (unavailable)"))
            continue
        prefix = "uilist-entry:" if namespace == "menu" and all(
            target.startswith("uilist-entry:") for target in targets) else ""
        lines.append(f"play act {namespace}.<action> --target {prefix}<target> (copy the target exactly)")
        verb_sets = {tuple(verb for verb, action in choices if action.get("enabled", True))
                     for choices in targets.values()}
        shared = next(iter(verb_sets)) if len(verb_sets) == 1 else ()
        if shared:
            lines.append("Actions for every target below: " + "/".join(shared))
        for target, choices in targets.items():
            named = next((action for verb, action in choices if verb in {"select", "choose"}), choices[0][1])
            label = _plain_text(named.get("label", named["id"]))
            available = [verb for verb, action in choices if action.get("enabled", True)]
            disabled = [verb + ": " + _plain_text(action.get("label", verb))
                        for verb, action in choices if not action.get("enabled", True)]
            lines.append(f"  {shlex.quote(target.removeprefix(prefix))} — {label}" + (" — " + "/".join(available) if available and not shared else ""))
            if disabled:
                lines.append("    Unavailable: " + "; ".join(disabled))
    return "\n".join(lines)


def _expanded_changes(changed, current):
    """Recover displayed changed values from the same snapshot, without adding unchanged facts."""
    if isinstance(changed, dict) and isinstance(current, dict):
        if changed.get("omitted") is True:
            return current
        return {key: (current[key] if key in {"health", "stamina", "weapon"}
                      else _expanded_changes(value, current[key])) if key in current else value
                for key, value in changed.items()}
    return current


def plain_player_output(result, *, snapshot=None, full_look=True):
    """Render the existing player projection; never replace game facts with advice."""
    import shlex
    import re

    lines = []

    view = player_output(result)
    source = result.get("response", result).get("current_input", {}).get("source_selector", "")
    fact_selector = source + ".surface.facts." if source else ""
    current = view.get("current_input", {})
    owner = current.get("owner")
    fresh = (snapshot and result.get("state") in {"collected", "rejected"}
             and owner and owner == snapshot.get("owner"))
    if fresh:
        facts = snapshot["current"].get("facts", {})
        if owner == "world":
            if full_look:
                lines.append(world_look(snapshot))
            else:
                lines.append("World.")
            # Keep messages/outcomes on actions, without reprinting world diagnostics/maps.
            changed = view.get("facts_changed", {})
            selected = {key: _expanded_changes(changed[key], facts[key]) for key in
                        ("avatar", "avatar_status", "avatar_effects", "visible_entities", "visible_local",
                         "last_save_result", "last_debug_intervention")
                        if key in changed and key in facts} if not full_look else {}
            if "visible_entities" in selected:
                selected["visible_entities"] = facts["visible_entities"]
            if "messages" in changed and not full_look:
                selected["messages"] = changed["messages"]
            view = {**view, "facts_changed": selected}
        else:
            title = facts.get("title") or owner.replace("_", " ").upper()
            if facts.get("actor_name"):
                title += " — " + facts["actor_name"]
            if title != "YESNO" and not (title == "Menu" and facts.get("text")):
                lines.append(_plain_text(title))
            view = {**view, "facts_changed": facts}
        view = {key: value for key, value in view.items() if key not in {"current_input", "facts_removed"}}
    terminal = view.get("result", {})
    if isinstance(terminal, dict) and terminal.get("state") == "finished":
        cleanup = terminal.get("cleanup", {}).get("status", "unknown")
        lines.append("Playtest ended." if cleanup in {"terminated", "already_exited", "killed",
                     "terminated_during_kill_escalation"} else "Playtest ended. Cleanup: " + cleanup)
        view = {key: item for key, item in view.items() if key not in {"result", "next"}}

    def write(value, label=""):
        value = decode(value)
        if label == "selected items" and isinstance(value, dict):
            marking = fresh and any(action["id"] == "inventory.toggle"
                                    for action in snapshot["current"].get("actions", []))
            lines.append(("Marked: " if marking else "Selected: ") + (", ".join(
                f"{key} ×{item.get('count', 1)}" + (" charges" if item.get("unit") == "charges" else "")
                for key, item in value.items()) or "none"))
            return
        if value is None or value == [] or value == {}:
            return
        if isinstance(value, dict):
            if label.endswith("health"):
                parts = value.get("body_parts", value)
                if parts and all(isinstance(part, dict) and ("hp" in part or "current" in part) for part in parts.values()):
                    hp = {(p.get("hp", p.get("current")), p.get("hp_max", p.get("maximum"))) for p in parts.values()}
                    if len(hp) == 1:
                        current_hp, maximum = next(iter(hp))
                        lines.append(f"All {len(parts)} body parts: {current_hp}/{maximum} HP")
                    else:
                        lines.append("HP: " + " · ".join(f"{p.get('name', k)} {p.get('hp', p.get('current'))}/{p.get('hp_max', p.get('maximum'))}" for k, p in parts.items()))
                    return
            if label.endswith("highlighted item"):
                if value.get("name"):
                    lines.append("Highlighted: " + _plain_text(value["name"]) + " [" + str(value.get("id", "")) + "]")
                return
            if label.endswith("needs"):
                texts = [_plain_text(part["text"]) for part in value.values() if isinstance(part, dict) and part.get("text")]
                if texts:
                    lines.append(" · ".join(texts))
                return
            if label.endswith("weapon"):
                lines.append("Weapon: " + " ".join(_plain_text(value[k]) for k in ("name", "mode", "ammo") if value.get(k)))
                return
            if label.endswith("stamina") and "current" in value:
                lines.append("Stamina: " + str(value["current"]) + ("/" + str(value["maximum"]) if "maximum" in value else ""))
                return
            if label == "items" and all(isinstance(p, dict) and "name" in p for p in value.values()):
                lines.append("Items: " + "; ".join(f"{_plain_text(p.get('name', k))} [{k}, {p.get('slot', '')}]" +
                             (f" ×{p['charges']}" if p.get("charges") else "") for k, p in value.items()))
                return
            if label in {"camp patrol", "orders", "llm intent"}:
                summary = []
                for key, item in value.items():
                    if key in {"provenance", "schema", "shift_cache_state", "shift_cache_available",
                               "runtime_available"} or key.endswith("coordinate_system") or item in (None, "", [], {}):
                        continue
                    if isinstance(item, list):
                        item = ", ".join(str(part) for part in item)
                    if isinstance(item, bool):
                        item = "yes" if item else "no"
                    summary.append(key.replace("_", " ") + ": " + _plain_text(item))
                lines.append(label.capitalize() + " — " + "; ".join(summary))
                return
            if label.endswith("avatar effects"):
                names = list(dict.fromkeys(_plain_text(part["name"]) for effect, parts in value.get("entries", {}).items()
                                          if effect != "wet" for part in parts.values() if part.get("name")))
                if names:
                    lines.append("Effects: " + " · ".join(names))
                return
            if value.get("omitted") is True:
                size = value.get("json_bytes", value.get("evidence", {}).get("json_bytes"))
                preview = value.get("preview", value.get("named_effects"))
                if preview:
                    write(preview, label)
                detail = str(size) + " bytes" if size is not None else "details"
                command = " → play inspect " + value["selector"] if value.get("selector") else ""
                lines.append(label + ": " + detail + (" total; partial view" if preview else " omitted") + command)
                return
            if "text" in value and "time" in value:
                lines.append(str(value["time"]) + ": " + re.sub(r"</?color[^>]*>", "", str(value["text"])))
                return
            if "owner" in value and "facts_changed" in value:
                write(value["facts_changed"])
                value = {key: item for key, item in value.items() if key != "facts_changed"}
            if "id" in value and ("enabled" in value or label.startswith("actions")):
                command = "play act " + shlex.quote(str(value["id"]))
                if value.get("stable_id"):
                    command += " --target " + shlex.quote(str(value["stable_id"]))
                answer = str(value.get("label", "")).strip().lower()
                if value["id"] == "prompt.choose" and answer in {"yes", "no", "ignore"}:
                    command = "play " + answer
                elif value["id"] == "activity.pause":
                    command = "play stop"
                lines.append(str(value.get("label", value["id"])) +
                             (" → " + command if value.get("enabled", True) else " (unavailable)"))
                return
            for key, item in value.items():
                if fresh and fact_selector and owner in {"npc_inspection", "camp_npc_inspection"} and key == "diagnostic_items" and isinstance(item, dict):
                    shown = {uid for uid, gear in item.items() if gear.get("slot") == "wielded"}
                    while True:
                        children = {uid for uid, gear in item.items() if gear.get("parent_uid") in shown}
                        if children <= shown:
                            break
                        shown.update(children)
                    write({uid: gear for uid, gear in item.items() if uid in shown}, "items")
                    other = {uid: gear for uid, gear in item.items() if uid not in shown}
                    if other:
                        count = len(json.dumps(other, ensure_ascii=False))
                        lines.append(f"Other gear: {len(other)} items, {count} characters omitted → play inspect {fact_selector}{key}")
                    continue
                if fresh and fact_selector and owner in {"npc_inspection", "camp_npc_inspection"} and key in {
                        "diagnostic_rules", "diagnostic_llm_intent"}:
                    count = len(item) if isinstance(item, str) else len(json.dumps(item, ensure_ascii=False))
                    name = "Rules" if key == "diagnostic_rules" else "AI intent"
                    lines.append(f"{name}: {count} characters omitted → play inspect {fact_selector}{key}")
                    continue
                if fresh and fact_selector and key == "item_info_text" and isinstance(item, str):
                    kept, omitted = [], 0
                    for row in _plain_text(item).splitlines(keepends=True):
                        text = row.strip()
                        if ":" in text or text.startswith("*") or re.match(r"^\d", text) or text.startswith((
                                "Disassembly ", "Can be stored ", "Length: ")):
                            kept.append(text)
                        else:
                            omitted += len(row)
                    lines.extend(kept)
                    if omitted:
                        lines.append(f"Additional item text: {omitted} characters omitted → play inspect {fact_selector}{key}")
                    continue
                if view.get("error") and ((key == "ok" and item is False)
                                          or (key == "state" and item == "rejected")):
                    continue
                if key == "game_turn":
                    continue
                if key == "game_minutes" and isinstance(item, dict) and "after" in item:
                    if item["after"] is not None:
                        lines.append("Game time: " + str(item["after"]) + " minutes.")
                    continue
                if key == "state" and item == "process_exited":
                    if "Game exited." not in lines:
                        lines.append("Game exited.")
                    continue
                if key == "error":
                    hint = {
                        "action_not_advertised": "This action is not available in the current menu.",
                        "stable_id_not_advertised": "Use the target listed for this exact action; targets differ between actions.",
                        "stale_observation": "The game has moved on from that observation.",
                    }.get(str(item))
                    if str(item).startswith("look_required"):
                        hint = "A fresh observation is required before another action."
                    if hint:
                        lines.append("Error: " + hint)
                        continue
                    if item == "native_action_rejected" and view.get("rejection_reason"):
                        continue
                if key == "rejection_reason":
                    if not item:
                        continue
                    lines.append("Rejected: " + str(item).replace("_", " ") + ".")
                    if item == "stale_frame" and "Next: play look" not in lines:
                        lines.append("Next: play look")
                    continue
                if key in {"view", "request_id", "navigation", "breadcrumbs", "source_index",
                           "current_input_actions", "current_input_owner",
                           "outcome_unknown_on_reconnect", "interruption_policy", "observed_turn",
                           "selector", "cursor", "retained_history_count", "new_event_count",
                           "note", "detail", "facts_removed", "actions_removed", "selection_source",
                           "provenance", "schema", "calendar_turn", "item_details_parameters", "item_info_text_source",
                           "pid", "alive", "exit_observed_at", "evidence_ref", "native_receipt_matched",
                           "failure", "session_state", "unused_authority", "selected_stable_id",
                           "identity", "gameplay_credit", "unit", "color", "speaker_id", "speaker_name",
                           "artifact_reference_envelope", "released_continuation", "surface_request",
                           "activity_generation", "native_action", "native_owner", "run_id", "binding_id",
                           "surface_id", "frame_id", "observation_id", "authority", "evidence_handles"} or key.endswith("sha256") or (key == "ok" and item is True):
                    continue
                if key == "activity_type":
                    lines.append("Activity: " + str(item).removeprefix("ACT_").lower().replace("_", " "))
                    continue
                if key == "chain" and isinstance(item, dict):
                    start, end = item.get("start_game_minutes"), item.get("terminal_game_minutes")
                    if isinstance(start, (int, float)) and isinstance(end, (int, float)):
                        lines.append(f"Waited {end - start:g} game minutes. " +
                                     str(item.get("stop_reason", "")).replace("_", " ") + ".")
                    elif "partial_progress" in item and "offset_ms" not in item:
                        progress = item["partial_progress"]
                        if isinstance(progress, (int, float)):
                            lines.append(f"Waited {progress:g} game minutes.")
                    if "offset_ms" in item:
                        if "partial_progress" in item and "planned_steps" in item:
                            lines.append(f"Moved {item['partial_progress']}/{item['planned_steps']} steps.")
                        if item.get("terminal_absolute_ms") is not None:
                            lines.append("Position: " + ", ".join(str(x) for x in item["terminal_absolute_ms"]))
                    item = {field: content for field, content in item.items() if field not in {
                        "start_game_minutes", "target_game_minutes", "terminal_game_minutes",
                        "stop_reason", "derived_bound", "native_action_count", "model_round_trips",
                        "tool_round_trips", "safety_frame_count", "danger_handling", "unused_authority",
                        "session_state", "action_id", "step_index", "guarded_handling_count",
                        "partial_progress", "planned_steps", "offset_ms", "origin_absolute_ms",
                        "target_absolute_ms", "terminal_absolute_ms"}
                        and not (field == "target_overshoot_game_minutes" and content == 0)}
                if key == "owner":
                    if item not in {"world", "process_exited"}:
                        lines.append(str(item).replace("_", " ").title())
                    continue
                if key == "accepted" and item is True:
                    continue
                if key == "operation" and isinstance(item, dict):
                    progress = item.get("completed_progress_game_minutes")
                    if progress is not None:
                        lines.append("Elapsed: " + str(progress) + " game minutes.")
                    item = {field: content for field, content in item.items() if field not in {
                        "kind", "start_game_minutes", "requested_duration_game_minutes",
                        "requested_target_game_minutes", "completed_progress_game_minutes"}}
                if key == "state" and item in ("collected", "accepted"):
                    continue
                if key == "performance" and item == {"status": "retained_by_public_collect"}:
                    continue
                if key == "next" and item == "act, look, inspect, or journal":
                    continue
                if key == "next" and item in {"look", "collect"}:
                    lines.append("Next: play " + item)
                    continue
                if key == "title" and item in ("YESNO", ""):
                    continue
                if key == "filter_required" and item in (False, "false"):
                    continue
                if fresh and key == "title":
                    continue
                if fresh and owner != "world" and key.endswith("_available"):
                    continue  # The current controls carry availability and native reasons.
                if fresh and owner in {"npc_inspection", "camp_npc_inspection"} and key in {"actor_name", "diagnostic_item_count"}:
                    continue
                if label == "visible" and key in {"worn", "wielded_item_uid"}:
                    continue
                if key in {"actions", "actions_changed"} and isinstance(item, list):
                    lines.append(_plain_controls(item))
                    continue
                if key in {"current_input", "facts_changed", "outcome", "response", "new_events", "value", "slice"}:
                    write(item)
                else:
                    write(item, (label + "." if label else "") + key.removeprefix("diagnostic_").replace("_", " "))
        elif isinstance(value, list):
            if label.endswith("visible local"):
                groups = {}
                for tile in value:
                    dx, dy = tile.get("dx", 0), tile.get("dy", 0)
                    direction = ("N" if dy < 0 else "S" if dy > 0 else "") + ("W" if dx < 0 else "E" if dx > 0 else "")
                    terrain = "; ".join(_plain_text(x) for x in (tile.get("terrain", tile.get("visibility", "unknown")), tile.get("furniture")) if x)
                    if tile.get("fields"):
                        terrain += "; " + ", ".join(_plain_text(x) for x in tile["fields"])
                    groups.setdefault(terrain, []).append(direction or "here")
                lines.extend(terrain + ": " + ", ".join(directions) for terrain, directions in groups.items())
                return
            if label.endswith("rules") and all(isinstance(item, dict) and "label" in item for item in value):
                lines.append("Rules: " + "; ".join(re.sub(r"^(?:He|She|They) will ", "", _plain_text(item["label"])).rstrip(".") for item in value))
                return
            if label.endswith("visible entities"):
                for entity in value:
                    lines.append(f"{_plain_text(entity.get('name', entity.get('kind', 'Creature')))} · {entity.get('attitude', '')} · "
                                 f"offset {entity.get('dx', 0)}, {entity.get('dy', 0)} · {entity.get('identity', {}).get('id', '')}")
                if not value:
                    lines.append("No visible creatures.")
                return
            if all(isinstance(item, (str, int, float, bool)) for item in value):
                lines.append(label + ": " + ", ".join(str(item) for item in value))
                return
            for item in value:
                write(item, label)
        else:
            if value == "":
                return
            text = str(value) if not isinstance(value, bool) else ("yes" if value else "no")
            text = re.sub(r"</?color[^>]*>", "", text)
            if label == "text":
                text = re.sub(r"</?color[^>]*>", "", text)
                text = text.removeprefix("Confirm: ").replace(" (Case Sensitive)", "")
                label = ""
            lines.append((label + ": " if label else "") + text)

    write(view)
    error = str(view.get("error", ""))
    if (error in {"action_not_advertised", "stable_id_not_advertised", "stale_observation"}
            or error.startswith("look_required")) and "Next: play look" not in lines:
        lines.append("Next: play look")
    if fresh and owner != "world":
        if (not full_look and current.get("view") == "delta"
                and not current.get("actions_changed") and not current.get("actions_removed")):
            lines.append("Allowed actions unchanged.")
        else:
            lines.append(_plain_controls(snapshot["current"].get("actions", [])))
    return "\n".join(line for line in lines if line) or "No changes."


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
    # Rejections can carry their cause in the outer receipt rather than response.
    # Keep that native cause, never infer it from the failed action or UI owner.
    for source in (result, view):
        if isinstance(source, dict):
            receipt = source.get("receipt", {})
            if isinstance(receipt, dict):
                receipt = clean({"receipt": receipt.get("native_receipt") or receipt})
                if receipt.get("rejection_reason"):
                    output["rejection_reason"] = receipt["rejection_reason"]
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
