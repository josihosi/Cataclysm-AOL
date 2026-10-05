"""Task-independent change display; native evidence and grants stay with the client."""
from collections import Counter
import json
import re
import shlex

from cockpit_evidence import action_catalog, compact, decode, gameplay_fact


def local_text_map(facts, *, game_turn=None):
    """Draw only the current avatar-visible native cells around the player."""
    avatar = facts.get("avatar", {})
    position = avatar.get("absolute_ms") if isinstance(avatar, dict) else None
    if not isinstance(position, list) or len(position) != 3 or any(
            not isinstance(value, int) or isinstance(value, bool) for value in position):
        return "LOCAL MAP unavailable: current player position is unknown."
    minimap = facts.get("minimap")
    native_cells = minimap.get("cells") if isinstance(minimap, dict) else None
    native_radius = minimap.get("radius") if isinstance(minimap, dict) else None
    if not isinstance(native_cells, list) or not isinstance(native_radius, int) or native_radius < 1:
        native_cells, native_radius = [], 1
    local = facts.get("visible_local")
    if not native_cells and not isinstance(local, list):
        return "LOCAL MAP unavailable: no current local cells were supplied."
    radius = min(9, native_radius)
    cells = {}
    for source in (native_cells, local if isinstance(local, list) else []):
        for cell in source:
            if not isinstance(cell, dict):
                continue
            dx, dy = cell.get("dx"), cell.get("dy")
            if any(not isinstance(value, int) or isinstance(value, bool) for value in (dx, dy)):
                continue
            if abs(dx) <= radius and abs(dy) <= radius and cell.get("visibility") == "clear":
                cells[(dx, dy)] = {**cells.get((dx, dy), {}), **cell}

    def glyph(cell):
        if cell is None:
            return "?"
        terrain = re.sub(r"</?color[^>]*>", "", str(cell.get("terrain", ""))).casefold()
        furniture = str(cell.get("furniture", ""))
        fields = cell.get("fields") if isinstance(cell.get("fields"), list) else []
        fire, smoke = "fd_fire" in fields, "fd_smoke" in fields
        if furniture == "f_brazier":
            return "F" if fire else "S" if smoke else "B"
        if fire:
            return "*"
        if smoke:
            return "s"
        passable = cell.get("passable")
        if "wall" in terrain:
            return "#"
        if "door" in terrain:
            return "D" if passable is False else "d" if passable is True else "O"
        if "window" in terrain or "curtain" in terrain:
            return "W" if passable is False else "w" if passable is True else "C"
        if passable is False:
            return "X"
        if passable is True:
            return "."
        return ":"  # Seen terrain with no native passability value.

    symbols = {(dx, dy): glyph(cells.get((dx, dy))) for dy in range(-radius, radius + 1)
               for dx in range(-radius, radius + 1)}
    creatures = []
    entities = facts.get("visible_entities")
    for entity in entities if isinstance(entities, list) else []:
        if not isinstance(entity, dict):
            continue
        dx, dy, absolute = entity.get("dx"), entity.get("dy"), entity.get("absolute_ms")
        if any(not isinstance(value, int) or isinstance(value, bool) for value in (dx, dy)) or \
                not isinstance(absolute, list) or len(absolute) != 3 or \
                absolute != [position[0] + dx, position[1] + dy, position[2]] or \
                abs(dx) > radius or abs(dy) > radius:
            continue
        creatures.append((dx, dy, entity))
    creature_lines = []
    for index, (dx, dy, entity) in enumerate(creatures):
        if index >= 26:
            symbols[(dx, dy)] = "+"
            continue
        letter = chr(ord("a") + index)
        if (dx, dy) != (0, 0):
            symbols[(dx, dy)] = letter
        offset = " ".join(part for part in (f"E{dx}" if dx > 0 else f"W{-dx}" if dx < 0 else "",
                                                 f"S{dy}" if dy > 0 else f"N{-dy}" if dy < 0 else "") if part) or "here"
        name = re.sub(r"</?color[^>]*>", "", str(entity.get("name") or entity.get("kind") or "creature"))
        attitude = str(entity.get("attitude") or "unknown")
        creature_lines.append(f"{letter} {name} ({attitude}) {offset}")
    symbols[(0, 0)] = "@"
    title = f"LOCAL MAP z={position[2]} · @ [{position[0]},{position[1]},{position[2]}] · N up, E right · radius {radius}"
    if game_turn is not None:
        title += f" · turn {game_turn}"
    lines = [title]
    for dy in range(-radius, radius + 1):
        row = "".join(symbols[(dx, dy)] for dx in range(-radius, radius + 1))
        lines.append(f"{'N' + str(-dy) if dy < 0 else 'S' + str(dy) if dy > 0 else '0'} {row}")
    used = set(symbols.values())
    meanings = {"@": "player", "?": "unobserved", "#": "wall", "D": "closed door",
                "d": "passable door/frame", "O": "door; passability unreported",
                "W": "blocked window/curtain", "w": "passable window/curtain",
                "C": "window/curtain; passability unreported",
                "X": "other blocked tile", ".": "passable tile", ":": "seen; passability unreported",
                "B": "brazier", "S": "brazier with smoke", "F": "burning brazier",
                "*": "other fire", "s": "smoke", "+": "additional visible creature"}
    lines.append("Legend: " + " · ".join(f"{key} {meaning}" for key, meaning in meanings.items() if key in used))
    if creature_lines:
        lines.append("Visible creatures: " + "; ".join(creature_lines))
    if len(creatures) > 26:
        lines.append(f"+ {len(creatures) - 26} more visible creatures; inspect visible_entities for exact positions.")
    lines.append("Current avatar-visible tiles only; off-screen NPCs require saved metadata.")
    return "\n".join(lines)


def world_look(snapshot, operation_availability=None, *, show_controls=True, controls_only=False,
               show_local_map=False):
    """Present an existing World snapshot, using only its advertised controls."""
    current = snapshot.get("current", {})
    facts = current.get("facts", {})
    actions = [action for action in current.get("actions", []) if action.get("enabled", True)]
    operations = operation_availability or {}
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
    names = list(dict.fromkeys(re.sub(r"^(Warm|Chilly) \([^)]*\)$", r"\1", clean(part["name"])) for effect, parts_by_body in effects.items()
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
            lines.append(" · ".join(filter(None, (clean(entity.get("name", entity.get("kind", "Creature"))),
                         entity.get("attitude", ""), ", ".join(offsets) or "here"))))
    if not isinstance(tiles, list) and not isinstance(entities, list):
        lines.append("Surroundings unavailable in this observation.")
    if show_local_map:
        lines.extend(["", local_text_map(facts, game_turn=snapshot.get("game_turn"))])
    elif isinstance(facts.get("minimap"), dict):
        lines.append("Local layout → play look --map (optional current observation).")

    if operations.get("game.act") is False:
        controls = "Observation-only phase.\nplay look — observe · play quit — end playtest"
        return controls if controls_only else "\n".join(lines) + "\n\n" + controls
    if not show_controls:
        return "\n".join(lines) + "\nControls unchanged → play controls"
    controls_start = len(lines)
    remaining = list(actions)
    def take(predicate):
        selected = [action for action in remaining if predicate(action)]
        remaining[:] = [action for action in remaining if not predicate(action)]
        return selected

    movement = take(lambda action: action.get("id", "").startswith("world.move.") and not action.get("stable_id"))
    if movement:
        lines.extend(["", "MOVE — one step", "play act world.move.<direction>",
                      " / ".join(action["id"].removeprefix("world.move.") for action in movement),
                      "Occupied tiles may trigger interaction or attack."])
    if movement and operations.get("game.move_relative"):
        lines.extend(["", "MOVE — multiple steps",
                      'play move --east 3 --south -2 --bound-maximum 5 --bound-basis path_progress --bound-source "3 east, 2 north"',
                      "3 east, then 2 north. Negative east = west; negative south = north.",
                      "Stops on interruptions by default; reports partial progress."])
    groups = {
        "Combat": {"fire", "reload", "toggle_safemode", "autoattack"},
        "Items": {"inventory", "pickup", "drop"},
        "Observe": {"look", "overmap", "messages"},
        "Interact": {"chat", "zone_manager", "examine"},
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
    native_inspection = {action.get("id") for action in actions if action.get("enabled", True)} \
                        & {"world.look", "world.examine"}
    if native_inspection:
        explanation = ["play look is read-only"]
        if "world.look" in native_inspection:
            explanation.append("world.look opens the native look cursor")
        if "world.examine" in native_inspection:
            explanation.append("world.examine opens native Examine")
        lines.append("; ".join(explanation) + ".")
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
        lines.extend(["", "WAIT", "Native wait menu: play act world.wait"])
    if waiting and operations.get("game.wait"):
        lines.extend(["play wait <duration> [ignore|safe|stop]",
                      "1m / 5m / 30m / 1h / 2h / 3h / 6h — native availability applies; hours require a watch.",
                      "ignore: continue through danger (default); safe: inspect danger/damage; stop: stop at each interruption."])
    if any(action.get("id") == "world.autoattack" for action in actions):
        lines.append("world.autoattack: attack a hostile in reach; otherwise pass a turn when native safe mode permits.")
    if any(action.get("id") == "world.pause" for action in actions):
        lines.append("world.pause: pass one native turn without initiating an attack.")
    if any(action.get("id") in {"world.pause", "world.autoattack"} for action in actions):
        lines.append("Count repeat: play repeat world.pause --count 10 or play repeat world.autoattack --count 10; collect a pending receipt, then play repeat --resume.")
    if remaining:
        lines.extend(["", "OTHER ADVERTISED ACTIONS"])
        for action in remaining:
            command = "play act " + shlex.quote(action["id"])
            if action.get("stable_id"):
                command += " --target " + shlex.quote(action["stable_id"])
            lines.append(clean(action.get("label", action["id"])) + " → " + command)
    lines.extend(["", "SESSION", "play look — read-only observe · play collect — retrieve pending result · play quit — end playtest"])
    return "\n".join(line for line in (lines[controls_start:] if controls_only else lines) if line is not None).strip()


def _plain_text(value):
    return re.sub(r"</?color[^>]*>", "", str(value))


def _save_status_line(status, checkpoint, current_turn, *, quicksave=False):
    """Describe native persistence without equating the action counter to disk freshness."""
    if not isinstance(status, str) or status in {"unattempted", "unavailable"}:
        return None
    saved_turn = checkpoint.get("confirmed_turn") if isinstance(checkpoint, dict) else None
    if type(saved_turn) is not int:
        saved_turn = None
    if type(current_turn) is not int:
        current_turn = None
    label = "Quicksave" if quicksave else "Last save"
    if status == "saved":
        if saved_turn is None:
            return f"{label}: write complete; saved turn unknown."
        line = f"{label}: saved turn {saved_turn}."
        if current_turn is not None and current_turn > saved_turn:
            line += f" Current turn {current_turn} is unconfirmed."
        return line
    if status == "not_needed":
        line = f"{label}: skipped; no counted action since last save."
        line += (f" Confirmed saved turn {saved_turn};" if saved_turn is not None
                 else " Confirmed saved turn unknown;")
        if current_turn is not None:
            line += f" current turn {current_turn}."
        else:
            line = line.rstrip(";") + "."
        return line + " Current state not confirmed on disk."
    if status.startswith("failed_"):
        line = f"{label}: failed ({status.removeprefix('failed_').replace('_', ' ')})."
        return line + (f" Last confirmed saved turn {saved_turn}." if saved_turn is not None
                       else " Disk state and saved turn uncertain.")
    return f"{label}: {status}; saved turn unknown."


def _plain_trade_rows(rows, *, limit=8):
    """Native group/cell facts only; compact display never estimates item value."""
    lines = ["Letter | UID | Item | selected/available | Unit price | selected value | location/owner"]
    for row in rows[:limit]:
        locations = row.get("locations")
        places = list(dict.fromkeys(str(loc.get("root_source", "?")) + "/" + str(loc.get("owner_faction", "?"))
                                    for loc in locations if isinstance(loc, dict))) if isinstance(locations, list) else []
        source = places[0] if len(places) == 1 else "mixed(" + str(len(places)) + ")" if places else "unavailable"
        line = (f"{row.get('letter') or '-'} | {row.get('group_uid', '?')} | {_plain_text(row.get('name', '?'))} | "
                f"{row.get('selected', '?')}/{row.get('available', '?')} {row.get('unit', '?')} | "
                f"{row.get('unit_price', 'unavailable')} | {row.get('selected_value', 'unavailable')} | {source}")
        if row.get("native_row_available") is False:
            line += " (collapsed; select by current UID)"
        if row.get("enabled") is False:
            line += " - unavailable: " + _plain_text(row.get("denial", ""))
        lines.append(line)
    return "\n".join(lines)


def _trade_page_actions(actions, facts):
    rows = decode(facts.get("trade_rows"))
    if not isinstance(rows, list):
        return actions
    visible = {row.get("group_uid") for row in rows[:8] if isinstance(row, dict)}
    return [action for action in actions if not action.get("stable_id") or action.get("stable_id") in visible]


def _plain_controls(actions, heading="ALLOWED HERE ONLY", *, prompt_title=None):
    """Print every advertised choice once, sharing command syntax across targets."""
    lines = [heading]
    targeted = {}
    ids = {action["id"] for action in actions}
    if {"inventory.toggle", "inventory.commit"} <= ids:
        lines.append("toggle: mark/unmark an item; commit: perform the labelled action on marked items. select confirms; it does not mark items.")
    elif "inventory.toggle" in ids:
        toggle = next(action for action in actions if action["id"] == "inventory.toggle")
        lines.append("toggle: " + _plain_text(toggle.get("label", "Toggle")))
    elif "inventory.select" in ids:
        lines.append("select: confirm the supplied target; details: inspect it.")
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
        if action_id == "inventory.commit" and label != "Close contents":
            label = ("Confirm marked items: " if "inventory.toggle" in ids else "Confirm highlighted item: ") + label
        if not action.get("enabled", True):
            lines.append(label + " (unavailable)")
            continue
        command = "play act " + shlex.quote(action_id)
        if action_id == "trade.toggle_letters":
            command = "play trade LETTERS (toggle these current letters together; no commit)"
        elif action_id in {"menu.filter", "inventory.filter"}:
            command += ' --param "text=TEXT"'
        elif action_id == "prompt.submit":
            command += ' --param "text=TEXT"'
        elif action_id == "activity.pause":
            command = "play stop"
        elif action_id in {"npc_inspection.item_details", "camp_npc_inspection.item_details"}:
            command += " --param item_uid=ID"
        lines.append(label + " → " + command + (" (no --target)" if action_id == "inventory.commit" else ""))
    for namespace, targets in targeted.items():
        if namespace == "prompt":
            for target, choices in targets.items():
                for verb, action in choices:
                    label = _plain_text(action.get("label", verb))
                    command = "play " + label.lower() if label.lower() in {"yes", "no", "ignore"} else (
                        f"play act prompt.{verb} --target " + shlex.quote(target))
                    cue = ""
                    if prompt_title == "CANCEL_ACTIVITY_OR_IGNORE_QUERY" and action.get("enabled", True) and \
                            action.get("id") == "prompt.choose":
                        cue = {"no": " — continue this time",
                               "ignore": " — continue; ignore this distraction type for the current activity and backlog"}.get(
                                   label.casefold(), "")
                    lines.append(label + (" → " + command + cue if action.get("enabled", True)
                                          else " (unavailable)"))
            continue
        if namespace == "target":
            for target, choices in targets.items():
                for verb, action in choices:
                    label = _plain_text(action.get("label", verb))
                    command = f"play act target.{verb} --target " + shlex.quote(target)
                    lines.append(label + (" → " + command if action.get("enabled", True)
                                          else " (unavailable)"))
            continue
        prefix = "uilist-entry:" if namespace == "menu" and all(
            target.startswith("uilist-entry:") for target in targets) else ""
        lines.append(f"play act {namespace}.<action> --target {prefix}<target> (copy the target exactly)")
        verb_lists = [tuple(verb for verb, action in choices if action.get("enabled", True))
                      for choices in targets.values()]
        shared = tuple(verb for verb in verb_lists[0] if all(verb in verbs for verbs in verb_lists))
        if shared:
            lines.append("Actions for every target below: " + "/".join(shared))
        for target, choices in targets.items():
            named = next((action for verb, action in choices if verb in {"select", "choose"}), choices[0][1])
            label = _plain_text(named.get("label", named["id"]))
            available = [verb for verb, action in choices if action.get("enabled", True) and verb not in shared]
            disabled = [verb + ": " + _plain_text(action.get("label", verb))
                        for verb, action in choices if not action.get("enabled", True)]
            lines.append(f"  {shlex.quote(target.removeprefix(prefix))} — {label}" +
                         (" — " + ("also " if shared else "") + "/".join(available) if available else ""))
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


def _plain_evidence_output(result):
    """Give every returned event space; keep bulk data in its existing snapshot."""
    from evidence_display import DEFAULT_BYTES

    rows = result["rows"]
    header = f"{result.get('matched', len(rows))} matches; showing {len(rows)}."
    footer = ("Full rows → python3 tools/openclaw_harness/evidence_display.py --sha256 "
              + result["snapshot"]["sha256"] + " --selector rows --offset 0 --limit 20")
    footer += "\nOne field: replace --selector rows with /rows/INDEX/fields/FIELD (dots in FIELD stay literal); source/raw handle: rows.INDEX.source (INDEX starts at 0)."
    if isinstance(result.get("next"), dict):
        footer += "\nMore rows: --offset " + str(result["next"]["offset"])
    unavailable = len(result.get("unavailable_sources", []))
    if unavailable:
        header += f"\nUnavailable sources: {unavailable}."
        footer += "\nSource errors: replace --selector rows with --selector unavailable_sources."
    links = result.get("links", [])
    if links:
        from collections import Counter
        outcomes = Counter(str(link.get("outcome", "unknown")) for link in links)
        header += "\nRequests: " + ", ".join(f"{count} {outcome}" for outcome, count in sorted(outcomes.items())) + "."
        incomplete = sum(link.get("status") != "complete" for link in links)
        if incomplete:
            header += f" {incomplete} incomplete or contradictory; inspect links for details."
            footer += "\nRequest details: replace --selector rows with --selector links."

    def facts(value, prefix=""):
        if isinstance(value, dict):
            return "\n".join(facts(v, (prefix + "." if prefix else "") + str(k))
                             for k, v in value.items())
        if isinstance(value, list):
            return "\n".join(facts(v, prefix) for v in value)
        return (prefix + ": " if prefix else "") + (
            "unavailable" if value is None else "yes" if value is True else "no" if value is False else str(value))

    available = DEFAULT_BYTES - len((header + "\n" + footer + "\n").encode("utf-8"))
    per_row = max(0, available // max(1, len(rows)) - 1)
    entries = []
    for index, row in enumerate(rows, 1):
        detail = row.get("fields")
        if detail is None:
            detail = {"event": row.get("event"),
                      "actor": row.get("actor_name") or row.get("actor_id"),
                      **(row.get("payload") if isinstance(row.get("payload"), dict)
                         else {"details": row.get("payload")})}
            detail = {key: value for key, value in detail.items() if value is not None}
        row_id = str(row.get("event_id", "unavailable"))
        source = row.get("source", {})
        location = f"{source.get('producer', 'source')}@{source.get('offset', '?')}"
        text = f"{index}. Situation {row_id} ({location})\n" + facts(detail)
        raw = text.encode("utf-8")
        if len(raw) > per_row:
            shown = raw[:max(0, per_row - 70)].decode("utf-8", errors="ignore")
            text = shown + f"\n[{len(text) - len(shown)} rendered characters omitted.]"
        entries.append(text)
    body = "\n".join(entries)
    if len(body.encode("utf-8")) > available:
        shown = body.encode("utf-8")[:max(0, available - 70)].decode("utf-8", errors="ignore")
        body = shown + f"\n[{len(body) - len(shown)} rendered characters omitted.]"
    return "\n".join([header, body, footer]).rstrip()


def _plain_decision_output(result):
    """Show one bounded decision page; exact byte handles remain on every row."""
    from evidence_display import DEFAULT_BYTES

    scope = result["decision_trace"]
    page = result["page"]

    def location(value):
        return ",".join(map(str, value)) if isinstance(value, list) else "?"

    def yes_no(value):
        return "yes" if value is True else "no" if value is False else "?"

    def capture_window(first, last):
        return ("all turns" if first is None and last is None else
                f"{first if first is not None else 'start'}.."
                f"{last if last is not None else 'end'}")

    def row_text(value):
        kind = value.get("kind")
        if kind == "actor":
            target = value.get("target")
            target_text = (f"{target.get('type', '?')}#{target.get('id', '?')}@{location(target.get('position_abs'))}"
                           if isinstance(target, dict) else "none")
            duty = (f" duty=camp:{yes_no(value.get('assigned_camp'))}"
                    f"/job:{yes_no(value.get('has_job'))}"
                    f"/patrol:{value.get('patrol_priority')}"
                    f"/order:{yes_no(value.get('patrol_order'))}"
                    if value.get('assigned_camp') is not None else "")
            outing = (f" outing={value.get('outing_kind')}:{value.get('outing_phase')}"
                      f"/{value.get('outing_owner')}@{value.get('outing_waypoint_index')}"
                      if value.get('outing_member') else "")
            return (f"t{value.get('turn')} #{value.get('actor_id')} {value.get('action')} "
                    f"[{value.get('attitude')}/{value.get('mission')}] "
                    f"role={value.get('actor_role') or '?'} "
                    f"target={target_text} seen={yes_no(value.get('target_visible'))} "
                    f"avatar_seen={yes_no(value.get('avatar_visible'))} "
                    f"pos={location(value.get('position_abs'))} → goal={location(value.get('goto_abs'))} "
                    f"next_local={location(value.get('path_next_local'))} "
                    f"passable={yes_no(value.get('path_next_passable'))} "
                    f"path={value.get('path_length')} panic={value.get('panic')} "
                    f"flee={yes_no(value.get('flee'))} bravery={value.get('bravery')}"
                    f" danger={value.get('danger')} sound={value.get('sound_alerts')}"
                    f" phase={value.get('operation_phase')}/{value.get('operation_owner')}"
                    f" route={value.get('route_waypoint_index')}:{value.get('actor_route_waypoint')}"
                    f" search={value.get('site_search_waypoint')}{duty}{outing}")
        if kind == "shakedown_contact":
            return (f"t{value.get('turn')} SHAKEDOWN CONTACT #{value.get('speaker_id')}"
                    f" → {'avatar' if value.get('receiver_is_avatar') else 'camp NPC'}#{value.get('receiver_id')}"
                    f" audible={yes_no(value.get('audible_shout'))} unseen={yes_no(value.get('unseen_receiver'))}"
                    f" operation={value.get('operation_id')} generation={value.get('generation')}")
        if kind == "fight_reply":
            return (f"t{value.get('turn')} FIGHT REPLY "
                    f"{'avatar' if value.get('receiver_is_avatar') else 'camp NPC'}#{value.get('receiver_id')}"
                    f" pos={location(value.get('position_abs'))} attempted={yes_no(value.get('attempted'))}"
                    f" emitted={yes_no(value.get('emitted'))} native_volume={value.get('native_volume')}"
                    f" operation={value.get('operation_id')} generation={value.get('generation')}")
        if kind == "attack_callback":
            source = value.get("source") or {}
            victim = value.get("victim") or {}
            return (f"t{value.get('turn')} ATTACK CALLBACK {value.get('edge')} "
                    f"{source.get('type', 'unknown')}#{source.get('id')}@{location(source.get('position_abs'))}"
                    f" → {victim.get('type', 'unknown')}#{victim.get('id')}@{location(victim.get('position_abs'))}"
                    f" attitude={value.get('attitude_before')}→{value.get('attitude_after')}"
                    f" release={yes_no(value.get('shakedown_released'))}"
                    f" anger={value.get('anger_branch') or 'unknown'}"
                    f" alarm={yes_no(value.get('alarm_invoked'))}/{yes_no(value.get('alarm_raised'))}")
        if kind == "damage":
            source = value.get("source") or {}
            victim = value.get("victim") or {}
            return (f"t{value.get('turn')} DAMAGE {value.get('damage_kind')} "
                    f"{source.get('type', 'unknown')}#{source.get('id')}@{location(source.get('position_abs'))}"
                    f" → {victim.get('type', 'unknown')}#{victim.get('id')}@{location(victim.get('position_abs'))}"
                    f" {value.get('body_part')} HP {value.get('hp_before')}→{value.get('hp_after')}"
                    f" applied={value.get('applied_damage')}")
        if kind == "death":
            killer = value.get("killer") or {}
            victim = value.get("victim") or {}
            return (f"t{value.get('turn')} CONFIRMED DEATH "
                    f"{victim.get('type', 'unknown')}#{victim.get('id')}@{location(victim.get('position_abs'))}"
                    f" killer={killer.get('type', 'unknown')}#{killer.get('id')}@{location(killer.get('position_abs'))}")
        if kind == "sleep":
            return (f"t{value.get('turn')} #{value.get('actor_id')} {value.get('edge')} "
                    f"pos={location((value.get('actor') or {}).get('position_abs'))} "
                    f"sleepiness={value.get('sleepiness')} narcosis={yes_no(value.get('narcosis'))} "
                    f"cause={value.get('cause') or 'unknown'}")
        if kind == "scope":
            return (f"t{value.get('turn')} SCOPE group={value.get('group_id')} "
                    f"selected_npc_ids={value.get('selected_npc_ids')} "
                    f"capture={capture_window(value.get('capture_from_turn'), value.get('capture_to_turn'))} "
                    f"selection={value.get('selection')}")
        if kind == "search":
            recipients = value.get("recipients") or []
            orders = ",".join(f"#{item.get('id')}→{location(item.get('goto_abs'))}"
                              for item in recipients if isinstance(item, dict)) or "none"
            target = value.get("seen_target")
            seen = (f"{target.get('type', '?')}#{target.get('id', '?')}"
                    if isinstance(target, dict) else "?")
            return (f"t{value.get('turn')} SEARCH {value.get('reason')} "
                    f"trigger=#{value.get('trigger_actor_id') or '?'} seen={seen} "
                    f"waypoint={value.get('waypoint')} attempted={yes_no(value.get('waypoint_attempted'))} "
                    f"orders={orders}")
        if kind == "repeat":
            return (f"t{value.get('first_turn')}..{value.get('last_turn')} "
                    f"REPEAT {value.get('count')} × {value.get('of_event')} "
                    f"generation={value.get('generation')} member={value.get('member')} "
                    f"({'base row missing from snapshot; action/search payload unknown' if value.get('base_missing') else 'base handle in diagnostics'}; "
                    "full span may extend outside requested window)")
        if kind == "capture_truncated":
            return (f"CAPTURE TRUNCATED scope={value.get('scope')} budget={value.get('row_budget')} "
                    f"decisions={value.get('decisions')} written={value.get('written_rows')} "
                    f"unpreserved={value.get('unpreserved_rows')}")
        # --select is an exact raw field projection; show it without guessing
        # the missing decision fields or converting null into a negative fact.
        return ", ".join(f"{key}={json.dumps(item, ensure_ascii=False, separators=(',', ':'))}"
                         for key, item in value.items())

    actors = ", ".join(f"#{actor} {name}" for actor, name in sorted(
        scope["actors"].items(), key=lambda pair: int(pair[0]))
                     if not scope["actor_ids"] or int(actor) in scope["actor_ids"]) or "none recorded"
    window = f"{scope['from_turn'] if scope['from_turn'] is not None else 'start'}..{scope['to_turn'] if scope['to_turn'] is not None else 'end'}"
    counts = scope["captured_event_counts"]
    capture = ", ".join(f"{key}={counts.get(key, 0)}" for key in
                        ("raid_actor_action", "raid_site_search", "raid_trace_scope",
                         "raid_trace_repeat", "raid_trace_truncated"))
    cap = ("TRUNCATED " + str(scope["capture_truncation"])
           if scope["capture_truncation"] else "no truncation marker in snapshot; current recorder default 1024 rows")
    paths = {row["artifact"]["path"] for row in result["rows"]}
    source = (next(iter(paths)) if len(paths) == 1 else
              "no rows on this page" if not paths else "multiple; see each row's diagnostic artifact")
    scope_label = (f"operation={scope['operation_id']}" if scope.get("operation_id") else
                   f"group={scope.get('group_id')}")
    selected = (f" Selected NPC IDs: {scope.get('selected_npc_ids') or 'none recorded'}."
                if scope.get("group_id") else "")
    if scope.get("group_id") and scope.get("selected_capture_window"):
        bounds = scope["selected_capture_window"]
        selected += (" Capture turns: " +
                     capture_window(bounds.get('from_turn'), bounds.get('to_turn')) + ".")
    header = (f"Decision trace run={scope['run_id']} {scope_label} window={window}\n"
              f"Actors observed: {actors}.{selected} "
              f"{'Group search rows include other members gates.' if scope.get('operation_id') else 'Only loaded NPC hook decisions are captured; abstract travel is outside this hook.'}\n"
              f"Recorded in source snapshot: {capture}; {cap}. Final repeat tail unconfirmed. "
              f"Orphan repeat bases: {scope.get('orphan_repeat_bases', 0)}.\n"
              f"Matching {result['matched']}; page offset={page['offset']} limit={page['limit']} "
              f"returned={len(result['rows'])}; next={page['next_offset']}.\n"
              f"Source: {source}\n"
              "Each @offset+length sha256 is an exact raw source handle; use cockpit_file_bridge.py record-artifact. "
              "execute_action names the trace call site, not why an action was chosen.\n")
    footer = (f"Snapshot={result['snapshot']}; page with the same scope plus --snapshot HASH --offset N. "
              "--select FIELD projects raw fields; --diagnostics returns full row handles and coverage.")
    if len((header + footer + "\n").encode("utf-8")) + 100 > DEFAULT_BYTES:
        return (f"Decision view header exceeds {DEFAULT_BYTES} bytes; "
                f"snapshot={result['snapshot']}. Use --diagnostics for exact scope and source handles.")
    allowance = DEFAULT_BYTES - len((header + footer + "\n").encode("utf-8")) - 100
    shown = []
    for index, row in enumerate(result["rows"], page["offset"] + 1):
        artifact = row["artifact"]
        rendered = (f"{index}. {row_text(row['record'])} "
                    f"@{artifact['offset']}+{artifact['length']} sha256={artifact['sha256']}")
        size = len((rendered + "\n").encode("utf-8"))
        if size > allowance:
            break
        shown.append(rendered)
        allowance -= size
    if len(shown) < len(result["rows"]):
        footer = (f"Stdout byte limit displayed {len(shown)} of {len(result['rows'])} page rows. " + footer)
    return header + "\n".join(shown) + "\n" + footer


def _startup_dialog_text(dialog):
    """Present a startup UI observation without upgrading OCR or log clues to proof."""
    if not isinstance(dialog, dict):
        return ""
    state = dialog.get("state")
    pid = dialog.get("pid")
    birth = dialog.get("birth_identity")
    identity = f"PID {pid}" if pid else "game PID unknown"
    if birth:
        identity += f"; born {birth}"
    lines = [f"Startup UI ({identity}; {dialog.get('evidence_class', 'startup_ui_only')}):"]
    if dialog.get("run_id"):
        lines.append("Run: " + str(dialog["run_id"]) + "; bridge binding: " +
                     str(dialog.get("binding_id", "unknown")))
    if state == "confirmed_debug_dialog":
        lines.append("Current debug dialog confirmed by a fresh window capture and matching log text.")
        lines.append("Message: " + str(dialog.get("message", "")))
        source = dialog.get("source_file")
        if source:
            lines.append(f"Log source: {source}:{dialog.get('source_line', '?')}")
    elif state == "debug_dialog_unmatched":
        lines.append("A debug dialog is visible; its exact log message is unconfirmed.")
        if dialog.get("ocr_excerpt"):
            lines.append("Window OCR: " + str(dialog["ocr_excerpt"]))
    elif state == "other_surface":
        lines.append("A different startup screen is visible; no debug recovery decision is established.")
        if dialog.get("ocr_excerpt"):
            lines.append("Window OCR: " + str(dialog["ocr_excerpt"]))
    elif state == "loading_unknown":
        lines.append("No blocking debug dialog confirmed; loading state remains unknown.")
    else:
        lines.append("Current blocking screen unknown: " + str(dialog.get("reason", state or "unavailable")))
        if dialog.get("capture_detail"):
            lines.append("Capture detail: " + str(dialog["capture_detail"]))
    if dialog.get("image_path"):
        if dialog.get("window_title"):
            lines.append("Window: " + str(dialog["window_title"]))
        lines.append("UI capture: " + str(dialog["image_path"]) +
                     (" (SHA-256 " + str(dialog["image_sha256"]) + ")"
                      if dialog.get("image_sha256") else ""))
    if state == "confirmed_debug_dialog" and dialog.get("log_path") is not None:
        lines.append(f"Exact log: {dialog['log_path']} @ byte {dialog.get('log_byte_offset', '?')}")
        other_clues = [clue for clue in dialog.get("log_clues", [])
                       if clue.get("message") != dialog.get("message")]
        if other_clues:
            lines.append("Other profile log clues (separate messages; not the current captured dialog):")
            for clue in other_clues[-3:]:
                lines.append(f"- {clue.get('source_file', '?')}:{clue.get('source_line', '?')} "
                             f"{clue.get('message', '')}")
    elif dialog.get("log_clues"):
        lines.append("Recent profile debug-log clues (not confirmed as the current screen):")
        for clue in dialog["log_clues"][-3:]:
            lines.append(f"- {clue.get('source_file', '?')}:{clue.get('source_line', '?')} "
                         f"{clue.get('message', '')} — {clue.get('log_path', '?')} "
                         f"@ byte {clue.get('log_byte_offset', '?')}")
    if state == "confirmed_debug_dialog":
        lines.append("Decision: Record this exact UI/log/run clue; notify the coordinator asynchronously "
                     "for triage without waiting. Assess its consequence for this playtest.")
        if dialog.get("image_sha256") and dialog.get("session"):
            command = ("play --session " + shlex.quote(str(dialog["session"])) +
                       " debug-ignore --capture " + str(dialog["image_sha256"]))
            lines.append("Recovery for an understood non-destructive debug continuation: " + command)
            lines.append("Add --note only when a claim-specific consequence needs recording; message wording alone needs no new approval.")
            lines.append("This sends one native Ignore, then reobserves. No input sent by play look.")
        else:
            lines.append("Recovery command unavailable until a current capture and session are bound. No input sent.")
        lines.append("Guidance: .agents/skills/caol-harness/references/debug-errors.md")
    elif state == "debug_dialog_unmatched":
        lines.append("Decision: Assess the visible debug text and notify the coordinator asynchronously; "
                     "the exact current log message is unconfirmed. Reobserve or use independently verified "
                     "PID-bound input after identifying the current dialog. No input sent.")
    else:
        lines.append("Decision: Reobserve or report the unknown screen before recovery input. No input sent.")
    lines.append("UI and log diagnostics do not prove save integrity, gameplay, or actor behavior.")
    return "\n".join(lines)


def _startup_debug_recovery_text(recovery):
    before = recovery.get("before", {})
    lines = ["Debug Ignore: " + ("one key reported delivered" if recovery.get("ok") else "no confirmed delivery")]
    if before.get("message"):
        lines.append("Before: " + str(before["message"]))
        lines.append("Source: " + str(before.get("source_file", "?")) + ":" +
                     str(before.get("source_line", "?")) + "; run " + str(before.get("run_id", "?")) +
                     "; PID " + str(before.get("pid", "?")) + "; born " +
                     str(before.get("birth_identity", "?")))
    if recovery.get("reason"):
        lines.append("Input not sent: " + str(recovery["reason"]))
    input_result = recovery.get("input", {}).get("peekaboo", {})
    if input_result.get("error"):
        lines.append("Delivery error: " + str(input_result["error"]) +
                     ("; " + str(input_result["detail"]) if input_result.get("detail") else ""))
    if recovery.get("attempt_path"):
        lines.append("Attempt record: " + str(recovery["attempt_path"]))
    if recovery.get("result_path"):
        lines.append("Delivery/result record: " + str(recovery["result_path"]))
    after = recovery.get("after", {})
    if after.get("state") == "bridge_status_changed":
        lines.append("Bridge state after input: " + str(after.get("status", "unknown")) +
                     "; native World still needs verification.")
    elif after:
        if (recovery.get("ok") and before.get("image_sha256")
                and after.get("image_sha256") == before.get("image_sha256")):
            lines.append("The same modal remains in the immediate reobservation; recheck it before another Ignore.")
        lines.append("Current UI after input:\n" + _startup_dialog_text(after))
    elif before and not recovery.get("ok"):
        lines.append("Current UI at refusal:\n" + _startup_dialog_text(before))
    lines.append("Next: play look; use the new current capture for any next distinct debug warning. "
                 "Key delivery does not prove World or item integrity.")
    return "\n".join(lines)


def plain_player_output(result, *, snapshot=None, full_look=True, startup_error=None,
                        operation_availability=None, inspection_request_id=None,
                        show_local_map=False):
    """Render the existing player projection; never replace game facts with advice."""
    import shlex
    import re

    if result.get("schema") == "caol-startup-debug-recovery-v1":
        return _startup_debug_recovery_text(result)
    if result.get("schema") == "caol-play-count-repeat-v1":
        lines = [f"{result.get('action_id')}: {result.get('completed_count')}/{result.get('requested_count')} actions — {result.get('state')}"]
        if result.get("stop_reason"):
            lines.append("Stop: " + str(result["stop_reason"]))
        lines.append("Unused: " + str(result.get("unused_count")))
        lines.append(f"Native turns: {result.get('first_native_turn')} → {result.get('last_native_turn')}; owner: {result.get('current_owner')}")
        if result.get("receipt_handles"):
            lines.append("Receipts: " + ", ".join(str(item.get("request_id")) for item in result["receipt_handles"]))
        if result.get("outstanding_request_id"):
            lines.append("Pending exact request: " + str(result["outstanding_request_id"]))
        if result.get("next"):
            lines.append("Next: " + str(result["next"]))
        return "\n".join(lines)
    lines = []
    status = result.get("status", {})
    if not isinstance(status, dict):
        status = {}
    if status.get("state") in {"starting", "preparing"}:
        dialog = _startup_dialog_text(result.get("startup_dialog"))
        return "Loading → play look" + ("\n" + dialog if dialog else "")
    if (status.get("state") == "transitioning" and
            status.get("phase") == "awaiting_declared_reentry_descriptor"):
        dialog = _startup_dialog_text(result.get("startup_dialog"))
        return "Reloading saved game → play look" + ("\n" + dialog if dialog else "")
    if status.get("state") in {"process_dead", "bridge_failed", "reentry_failed", "terminalization_failed"}:
        reason = startup_error or status.get("error") or status.get("reason") or "game process exited"
        stopped = "Playtest stopped: " + str(reason).replace("pre_descriptor_no_progress", "startup failed before game controls became available")
        dialog = _startup_dialog_text(result.get("startup_dialog"))
        return stopped + ("\n" + dialog if dialog else "")

    view = player_output(result)
    if "latest" in result and "turn_assessment" in result:
        record = result.get("latest") or {}
        assessment = result.get("turn_assessment") or {}
        resources = record.get("resources") or {}
        metrics = assessment.get("metric") or {}
        for key, title in (("mean_seconds", "Mean"), ("tail_seconds", "Tail"), ("max_seconds", "Maximum")):
            if isinstance(metrics.get(key), (int, float)):
                lines.append(f"{title}: {metrics[key] * 1000:.1f} ms/turn")
        if isinstance(metrics.get("game_turns_per_simulation_second"), (int, float)):
            lines.append(f"Simulation: {metrics['game_turns_per_simulation_second']:.1f} turns/s")
        if metrics.get("sample_count") is not None:
            lines.append(f"Measured turns: {metrics['sample_count']}")
        if record.get("action_latency_seconds") is not None:
            lines.append(f"Action elapsed: {record['action_latency_seconds']:.3g} s (includes non-simulation time)")
        game_time = record.get("game_time") or {}
        if game_time.get("delta_minutes") is not None:
            lines.append(f"Game time advanced: {game_time['delta_minutes']:g} minutes")
        if isinstance(resources.get("interval_cpu_seconds"), (int, float)):
            wall = resources.get("interval_wall_seconds")
            wall = f"{wall:.3g}" if isinstance(wall, (int, float)) else "unknown"
            lines.append(f"CPU: {resources['interval_cpu_seconds']:.3g} s over {wall} s wall time")
        memory = resources.get("resident_memory") or {}
        if isinstance(memory.get("value"), (int, float)):
            lines.append(f"Memory: {memory['value'] / 1048576:.1f} MiB")
        comparison = result.get("comparison") or {}
        if comparison.get("reason") == "no baseline selected":
            comparison = "No baseline selected."
        view = {"assessment": {key: assessment[key] for key in
                ("status", "reason", "waiting", "current_incomplete_turn", "observation_uncertainty")
                if assessment.get(key) not in (None, {}, [], "")},
                "comparison": comparison,
                **{key: view[key] for key in ("performance", "records", "page", "baseline_saved") if key in view}}
        if not record:
            lines.append("No process sample available.")
    controls = view.get("result")
    if isinstance(controls, dict) and "danger_handling" in controls and "availability" in controls:
        availability = controls["availability"]
        if availability.get("game.act") is False:
            return "Observation-only phase.\nplay look — observe · play quit — end playtest"
        lines = ["Use only the current menu's actions → play look",
                 "Pending result → play collect; end playtest → play quit"]
        if availability.get("game.wait"):
            lines += ["Wait → play wait 5m [ignore|safe|stop] (example duration)",
                      "ignore: continue through danger (default); safe: inspect danger/damage; stop: each interruption.",
                      "For other native durations/modes → play act world.wait (when advertised)."]
        elif availability.get("game.wait") is False:
            lines.append("Short wait unavailable in this scenario; use the advertised native wait menu.")
        else:
            lines.append("Short wait permission unknown; play look first.")
        if availability.get("game.move_relative"):
            lines += ['Move → play move --east 3 --south -2 --bound-maximum 5 --bound-basis path_progress --bound-source "3 east, 2 north"',
                      "East then south; negative values mean west/north. Stops at interruptions; reports partial movement."]
        elif availability.get("game.move_relative") is False:
            lines.append("Multi-step move unavailable in this scenario; use advertised single-step actions.")
        else:
            lines.append("Multi-step move permission unknown; play look first.")
        lines += ["After rejection/interruption, read the current state; do not blindly replay input.",
                  "Speech: an emitted LLM response may need play act world.pause to apply (only when advertised).",
                  "look/collect do not advance game time. Use play evidence to check request/response events.",
                  "Exact retained field → play inspect SELECTOR; full transport JSON → play controls --diagnostics when needed."]
        return "\n".join(lines)
    if isinstance(result.get("decision_trace"), dict):
        return _plain_decision_output(result)
    evidence_snapshot = result.get("snapshot") if isinstance(result.get("rows"), list) else None
    if isinstance(evidence_snapshot, dict) and evidence_snapshot.get("sha256"):
        return _plain_evidence_output(result)
    if view.get("state") == "pending":
        lines.append("Pending → play collect")
        view = {key: item for key, item in view.items()
                if key not in {"state", "bridge_state", "next"}}
    journal = view.get("result", {}).get("evidence_journal") if isinstance(view.get("result"), dict) else None
    if isinstance(journal, dict):
        entries = journal.get("entries", [])
        request_option = (" --request-id " + shlex.quote(str(result["request_id"]))
                          if result.get("request_id") else "")
        return (f"Journal ready: {len(entries)} entries.\n"
                f"Read entries → play inspect result.evidence_journal.entries --limit 10{request_option}\n"
                "Finish → play finish --witness FILE\n"
                "FILE is JSON with these existing fields:\n"
                "verdict: proved | contradicted | inconclusive\n"
                "smallest_supported_claim, causal_account: your conclusion and why\n"
                "citations: list of citation_id, meaning, checks\n"
                'checks: object mapping paths to exact values, e.g. {"action_id":"wait.1m"}; not a list\n'
                "Paths start inside entry.value: actions use action_id; observations use value.surface.facts.FIELD. Inspect first.\n"
                'Field inspection prints copyable checks with exact types; "true" differs from true.\n'
                "recommended_disposition: accept | continue | repair | change-strategy\n"
                "contradictions: list all supplied contradictions with citation_id and meaning; [] if none\n"
                "Omit evidence_ceiling. Read cited entries before finishing.")
    selected = view.get("slice")
    selector = str(result.get("selector", ""))
    citation_field = re.search(r"(?:^|\.)entries\.\d+\.value\.(.+)$", selector)
    if result.get("ok") is not False and "slice" in view and citation_field and (selected is None or isinstance(selected, (str, int, float, bool))):
        from evidence_display import DEFAULT_BYTES
        check = "checks: " + json.dumps({citation_field.group(1): selected}, ensure_ascii=False)
        if len(check.encode("utf-8")) <= DEFAULT_BYTES:
            return check
        if isinstance(selected, str):
            preview = selected[:DEFAULT_BYTES // 8]
            request_option = " --request-id " + shlex.quote(inspection_request_id) if inspection_request_id else ""
            return ("Preview (not a complete witness check): " + json.dumps(preview, ensure_ascii=False) +
                    f"\n{len(selected) - len(preview)} characters omitted; total {len(selected)} characters.\n" +
                    f"Full value → play --diagnostics inspect {shlex.quote(selector)}{request_option}")
    single_entry = re.fullmatch(r"(.*entries)\.(\d+)", selector)
    if (isinstance(selected, dict) and "citation_id" in selected
            and single_entry and result.get("ok") is not False):
        selected = [selected]
        selector = single_entry.group(1)
    else:
        single_entry = None
    if isinstance(selected, list) and selected and all(
            isinstance(entry, dict) and "citation_id" in entry for entry in selected):
        offset = result.get("page", {}).get("offset", 0)
        summary = []
        indices = ([int(single_entry.group(2))] if single_entry else
                   result.get("source_indices", range(offset, offset + len(selected))))
        request_option = (" --request-id " + shlex.quote(inspection_request_id)
                          if inspection_request_id else "")
        summary.append(f"Inspect → play inspect SELECTOR{request_option}\n"
                       "Replace INDEX/FIELD in the complete SELECTOR below. Copy exact values into checks.")
        field_sets = {}
        for index, entry in zip(indices, selected):
            value = entry.get("value", {})
            value = value if isinstance(value, dict) else {}
            observation = value.get("value", {})
            observation = observation if isinstance(observation, dict) else {}
            surface = observation.get("surface", value.get("surface", {}))
            kind = str(entry.get("kind", "event")).replace("_", " ")
            detail = value.get("action_id") or surface.get("kind") or value.get("stop_reason") or ""
            fields = list(value)
            field_path = f"{selector}.{index}.value"
            if isinstance(surface.get("facts"), dict):
                field_path += ".value.surface.facts" if "surface" in observation else ".surface.facts"
                fields = list(surface["facts"])
            summary.append(f"{entry['citation_id']}: {kind}" + (f" — {detail}" if detail else ""))
            if value.get("action_id"):
                summary.append("  checks: " + json.dumps({"action_id": value["action_id"]}, ensure_ascii=False))
            elif fields:
                relative_path = field_path.removeprefix(f"{selector}.{index}.value.")
                if relative_path == field_path:
                    relative_path = ""
                signature = (relative_path, tuple(fields))
                if signature in field_sets:
                    summary.append(f"  INDEX {index}; same SELECTOR/FIELD as {field_sets[signature]}.")
                else:
                    field_sets[signature] = entry["citation_id"]
                    field_selector = f"{selector}.INDEX.value." + (relative_path + "." if relative_path else "") + "FIELD"
                    summary.append(f"  INDEX {index}; SELECTOR {field_selector}; FIELD: " + ", ".join(fields))
            else:
                summary.append("  No value fields.")
        page = result.get("page", {})
        if page.get("next_offset") is not None:
            filter_option = (" --contains " + shlex.quote(result["filter"]["contains"])
                             if result.get("filter", {}).get("contains") is not None else "")
            summary.append(f"More → play inspect {selector} --offset {page['next_offset']} --limit {len(selected)}{filter_option}{request_option}")
        return "\n".join(summary)
    if "slice" in view and selector.endswith("diagnostic_items") and isinstance(selected, dict):
        view["slice"] = {"items": selected}
    if "slice" in view and selector.endswith("diagnostic_rules") and isinstance(selected, dict):
        view["slice"] = {"rules": selected}
    if result.get("state") in {"finishing", "finished", "cleanup_failed", "reentered"}:
        state = result["state"]
        if state == "reentered":
            lines.append("Saved game reloaded. Observe → play look")
        elif state == "finishing":
            lines.append(("Reloading saved game." if result.get("bridge_state") == "transitioning"
                          else "Finishing session.") + " Check progress → play collect")
        elif state == "cleanup_failed":
            failure = result.get("reentry_failure") or {}
            lines.append("Session cleanup failed: " + str(result.get("reason") or
                         failure.get("cause") or result.get("bridge_state", "unknown cause")))
        else:
            cleanup = result.get("cleanup") or {}
            disposition = cleanup.get("status", "unknown")
            lines.append("Playtest ended. Cleanup complete." if disposition in {
                "accepted", "terminated", "already_exited", "killed", "terminated_during_kill_escalation"
            } else "Playtest ended. Cleanup: " + str(disposition))
        # Keep actual alarms, without repeating process identities and receipts.
        view = {key: value for key, value in view.items() if key == "performance"}
    source = result.get("response", result).get("current_input", {}).get("source_selector", "")
    fact_selector = source + ".surface.facts." if source else ""
    current = view.get("current_input", {})
    owner = current.get("owner")
    fresh = (snapshot and result.get("state") in {"collected", "rejected"}
             and owner and owner == snapshot.get("owner"))
    prompt_title = (current.get("facts_changed", {}).get("title")
                    if owner == "prompt" and isinstance(current.get("facts_changed"), dict) else None)
    if fresh:
        facts = snapshot["current"].get("facts", {})
        if owner == "prompt":
            prompt_title = facts.get("title")
        if owner == "world":
            if full_look:
                lines.append(world_look(snapshot, operation_availability,
                                        show_controls=not result.get("world_controls_unchanged", False),
                                        show_local_map=show_local_map))
            else:
                lines.append("World.")
            # Keep messages/outcomes on actions, without reprinting world diagnostics/maps.
            changed = view.get("facts_changed", {})
            raw = result.get("response", result)
            native = raw.get("outcome", {}).get("native_receipt", {}) if isinstance(raw, dict) else {}
            quicksave = (result.get("state") == "collected" and isinstance(native, dict)
                         and native.get("action_id") == "world.quicksave"
                         and native.get("accepted") is True)
            if result.get("state") == "collected" and (quicksave or
                    "last_save_result" in changed or "last_save_checkpoint" in changed):
                save_line = _save_status_line(facts.get("last_save_result"),
                                              facts.get("last_save_checkpoint"),
                                              snapshot.get("game_turn"), quicksave=quicksave)
                if save_line:
                    lines.append(save_line)
            selected = {key: _expanded_changes(changed[key], facts[key]) for key in
                        ("avatar", "avatar_status", "avatar_effects", "visible_entities", "visible_local",
                         "last_debug_intervention")
                        if key in changed and key in facts} if not full_look else {}
            if "visible_entities" in selected:
                selected["visible_entities"] = facts["visible_entities"]
            if "messages" in changed and not full_look:
                selected["messages"] = changed["messages"]
            view = {**view, "facts_changed": selected}
        else:
            title = facts.get("title") or owner.replace("_", " ").upper()
            if owner == "prompt" and re.fullmatch(r"##\d+", str(title)):
                title = "Enter text"
            if facts.get("actor_name"):
                title += " — " + facts["actor_name"]
            if title not in {"YESNO", "CANCEL_ACTIVITY_OR_IGNORE_QUERY"} and not (title == "Menu" and facts.get("text")):
                lines.append(_plain_text(title))
            if owner == "target" and str(facts.get("mode", "")) == "0":
                lines.append("Firing can leave targeting open. Cancel returns to World; check ammo and shot messages there.")
            if isinstance(decode(facts.get("trade_rows")), list):
                side = "OUR SIDE" if facts.get("active_party") == "player" else "TRADER SIDE"
                name = facts.get("player_name") if facts.get("active_party") == "player" else facts.get("trader_name")
                lines.append(f"{side}: {name or '?'} | Demand {facts.get('demand_amount', 'unavailable')} | "
                             f"{facts.get('balance_label', 'Balance')} {facts.get('balance_amount', facts.get('balance', 'unavailable'))} | "
                             f"Max credit {facts.get('max_credit', 'unavailable')} | "
                             f"Our offer {facts.get('player_offer_value', 'unavailable')} | "
                             f"Trader offer {facts.get('trader_offer_value', 'unavailable')}")
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
        if isinstance(value, dict) and value.get("kind") in {"waiting_slow", "waiting_recovery"} and \
                all(isinstance(value.get(key), (int, float))
                    for key in ("mean_seconds", "limit_seconds", "sample_count")):
            state = "Performance alarm" if value["kind"] == "waiting_slow" else "Waiting performance recovered"
            lines.append(f"{state}: {value['mean_seconds'] * 1000:.1f} ms/turn "
                         f"({value['sample_count']:g}-turn average; limit {value['limit_seconds'] * 1000:g} ms).")
            if value["kind"] == "waiting_slow":
                lines.append("Tell the coordinator: waiting performance needs attention.")
            return
        if label.endswith("visible entities") and value == []:
            lines.append("No visible creatures.")
            return
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
            if label == "messages" and value.get("initial_history") and fresh:
                history = snapshot["current"].get("facts", {}).get("messages", [])
                if isinstance(history, list) and history and isinstance(history[-1], dict) and "time" in history[-1]:
                    latest_time = history[-1]["time"]
                    earlier = [entry for entry in history if entry.get("time") != latest_time]
                    if earlier:
                        size = len(json.dumps(earlier, ensure_ascii=False))
                        lines.append(f"Earlier messages: {len(earlier)}, {size} characters omitted → play messages --offset 0 --limit {len(history)}")
                    lines.append("Latest retained messages (may predate this action):")
                    write([entry for entry in history if entry.get("time") == latest_time])
                    return
            if label.endswith("rules"):
                for field, rule in value.items():
                    if field in {"title", "schema"}:
                        continue
                    if isinstance(rule, str):
                        rule = re.sub(r"^(?:He|She|They) will ", "", _plain_text(rule))
                    write(rule, "Rules" if isinstance(rule, list) else field.replace("_", " "))
                return
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
                elif value.get("present") is False:
                    lines.append("No highlighted item.")
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
                names = list(dict.fromkeys(re.sub(r"^(Warm|Chilly) \([^)]*\)$", r"\1", _plain_text(part["name"])) for effect, parts in value.get("entries", {}).items()
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
                message = _plain_text(value["text"])
                if re.fullmatch(r"You feel your .+ getting (?:warm|chilly)\.(?: x \d+)?", message):
                    routine_messages.append(message)
                    return
                lines.append(str(value["time"]) + ": " + message)
                return
            if "owner" in value and "facts_changed" in value:
                write(value["facts_changed"])
                value = {key: item for key, item in value.items() if key != "facts_changed"}
            if "id" in value and label.rsplit(".", 1)[-1] in {"actions", "actions changed"}:
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
                    write(item, "items")
                    lines.append(f"Full gear data: {len(json.dumps(item, ensure_ascii=False))} characters → play inspect {fact_selector}{key}")
                    lines.append(f"Item details → play inspect {fact_selector}{key}.UID (copy an item ID; no --contains)")
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
                        if text.startswith(("Origin:", "Can be stored in (worn):")):
                            omitted += len(row)
                        elif ":" in text or text.startswith("*") or re.match(r"^\d", text) or text.startswith((
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
                    if item == "native_wait_interrupted":
                        lines.append("Wait interrupted. Choose from the current prompt.")
                        continue
                    if item in {"raw_move_relative_no_progress", "guarded_move_relative_no_progress"}:
                        lines.append("Movement stopped: the last action did not change position. Check the current surroundings.")
                        continue
                    if item == "keep_watch_recipe_action_not_advertised":
                        lines.append("Waiting paused: choose an action from the current menu.")
                        continue
                    hint = {
                        "slice_filter_requires_array": "This value is not a list; --contains cannot filter it.",
                        "selected_response_slice_is_unavailable": "The requested field is absent from this response.",
                        "action_not_advertised": "This action is not available in the current menu.",
                        "stable_id_not_advertised": "Use the target listed for this exact action; targets differ between actions.",
                        "stale_observation": "The game has moved on from that observation.",
                        "native_surface_receipt_timeout": "Native acknowledgment was not observed. The action outcome is unknown. Observe the current game; do not replay the submitted input.",
                        "proved_no_progress": "No game progress was recorded. Check for an interruption → play look.",
                        "operation_not_authorized_for_live_session": "This shortcut is unavailable in this scenario. Use the controls shown by play look.",
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
                if key in {"view", "request_id", "navigation", "breadcrumbs", "source_index", "world_controls_unchanged", "world_observation", "witness_fields",
                           "current_input_actions", "current_input_owner",
                           "initial_history",
                           "outcome_unknown_on_reconnect", "interruption_policy", "observed_turn",
                           "selector", "cursor", "retained_history_count", "new_event_count",
                           "note", "detail", "facts_removed", "actions_removed", "selection_source",
                           "provenance", "schema", "calendar_turn", "item_details_parameters", "item_info_text_source",
                           "pid", "alive", "exit_observed_at", "evidence_ref", "native_receipt_matched",
                           "failure", "session_state", "unused_authority", "selected_stable_id",
                           "identity", "gameplay_credit", "unit", "color", "speaker_id", "speaker_name",
                           "artifact_reference_envelope", "released_continuation", "surface_request",
                           "activity_generation", "native_action", "native_owner", "run_id", "binding_id",
                           "surface_id", "frame_id", "native_frame_id", "observation_id", "authority", "evidence_handles"} or key.endswith("sha256") or (key == "ok" and item is True):
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
                if key == "owner" and isinstance(item, str):
                    if item not in {"world", "process_exited"}:
                        lines.append(str(item).replace("_", " ").title())
                    continue
                if key == "accepted" and item is True:
                    continue
                if key == "operation" and isinstance(item, dict):
                    if item.get("state") == "completed":
                        continue
                    progress = item.get("completed_progress_game_minutes")
                    if progress is not None:
                        lines.append("Elapsed: " + str(progress) + " game minutes.")
                    item = {field: content for field, content in item.items() if field not in {
                        "kind", "start_game_minutes", "requested_duration_game_minutes",
                        "requested_target_game_minutes", "completed_progress_game_minutes"}
                        and not (field == "state" and content == "awaiting_decision")
                        and not (field == "blocker" and content == "native_authority_required")}
                if key == "outcome" and item in ("no_progress", "blocked"):
                    lines.append("Position unchanged." if item == "no_progress" else "Movement blocked.")
                    continue
                if key == "state" and item in ("collected", "accepted"):
                    continue
                if key == "performance" and item == {"status": "retained_by_public_collect"}:
                    continue
                if key == "next" and item == "act, look, inspect, or journal":
                    continue
                if key == "next" and isinstance(item, str) and item in {"look", "collect"}:
                    hint = "Next: play " + item
                    if hint not in lines:
                        lines.append(hint)
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
                if fresh and snapshot and isinstance(decode(snapshot.get("current", {}).get("facts", {}).get("trade_rows")), list):
                    if key in {"active_party", "active_actor_id", "player_actor_id", "player_name",
                               "trader_actor_id", "trader_name", "demand_amount", "balance", "balance_label",
                               "balance_amount", "max_credit", "player_offer_value", "trader_offer_value",
                               "opening_balance", "trade_values_source", "selection_source"}:
                        continue  # These native values are already in the Trade header; raw facts remain inspectable.
                    if key == "selected_items" and isinstance(decode(item), dict):
                        lines.append(f"{len(decode(item))} selected locations; exact: play inspect surface.facts.selected_items")
                        continue
                if key == "trade_rows" and isinstance(decode(item), list):
                    rows = decode(item)
                    lines.append(_plain_trade_rows(rows))
                    lines.append(f"{len(rows)} filtered groups; more: play inspect --view trade --offset 8 --limit 8; "
                                 "full raw: play inspect surface.facts.trade_rows --diagnostics")
                    continue
                if key in {"actions", "actions_changed"} and isinstance(item, list) and all(
                        isinstance(action, dict) and "id" in action and "enabled" in action for action in item):
                    if (operation_availability or {}).get("game.act") is not False:
                        if fresh and snapshot:
                            item = _trade_page_actions(item, snapshot.get("current", {}).get("facts", {}))
                        lines.append(_plain_controls(item, prompt_title=prompt_title))
                    continue
                if key in {"current_input", "facts_changed", "outcome", "response", "new_events", "value", "slice"}:
                    write(item)
                else:
                    write(item, (label + "." if label else "") + key.removeprefix("diagnostic_").replace("_", " "))
        elif isinstance(value, list):
            if value and all(isinstance(row, dict) and "group_uid" in row for row in value):
                lines.append(_plain_trade_rows(value, limit=len(value)))
                return
            if label.rsplit(".", 1)[-1] in {"zones", "visible zones"} and all(isinstance(row, dict) for row in value):
                lines.append("Zones:")
                for row in value:
                    name = _plain_text(row.get("name", row.get("type", "Zone")))
                    state = "on" if row.get("enabled") is True else "off" if row.get("enabled") is False else "state unknown"
                    lines.append(f"  {row.get('id', '?')} — {name}; {state}" + ("; selected" if row.get("selected") else ""))
                    details = []
                    for key, item in row.items():
                        if key in {"id", "name", "enabled", "selected"}:
                            continue
                        if isinstance(item, list) and all(isinstance(part, (int, float)) for part in item):
                            item = ",".join(str(part) for part in item)
                        if isinstance(item, (dict, list)):
                            write(item, key)
                        else:
                            details.append(key.replace("_", " ") + ": " + _plain_text(item))
                    if details:
                        lines.append("    " + "; ".join(details))
                return
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
            if label.lower().endswith("rules") and all(isinstance(item, dict) and "label" in item for item in value):
                lines.append("Rules: " + "; ".join(re.sub(r"^(?:He|She|They) will ", "", _plain_text(item["label"])).rstrip(".") for item in value))
                return
            if label.endswith("visible entities"):
                for entity in value:
                    lines.append(f"{_plain_text(entity.get('name', entity.get('kind', 'Creature')))} · {entity.get('attitude', '')} · "
                                 f"offset {entity.get('dx', 0)}, {entity.get('dy', 0)}")
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

    routine_messages = []
    write(view)
    if routine_messages:
        lines.append(f"Temperature comfort messages: {len(routine_messages)} grouped ({sum(map(len, routine_messages))} characters).")
    error = str(view.get("error", ""))
    if (error in {"action_not_advertised", "stable_id_not_advertised", "stale_observation"}
            or error.startswith("look_required")) and "Next: play look" not in lines:
        lines.append("Next: play look")
    if fresh and result.get("next") == "look":
        if "Next: play look" not in lines:
            lines.append("Next: play look")
        lines.append("Refresh before choosing an action.")
    elif fresh and owner != "world":
        if (operation_availability or {}).get("game.act") is False:
            lines.append("Observation-only phase.\nplay look — observe · play quit — end playtest")
        elif not full_look and current.get("view") == "delta":
            changed = current.get("actions_changed", [])
            removed = current.get("actions_removed", [])
            if removed:
                lines.append(_plain_controls(_trade_page_actions(snapshot["current"].get("actions", []), snapshot["current"].get("facts", {})),
                                             "Allowed actions replaced — use only these:",
                                             prompt_title=prompt_title))
            elif changed:
                lines.append(_plain_controls(_trade_page_actions(changed, snapshot["current"].get("facts", {})), "Changed controls (others unchanged):",
                                             prompt_title=prompt_title))
            if not changed and not removed:
                lines.append("Allowed actions unchanged.")
        else:
            lines.append(_plain_controls(_trade_page_actions(snapshot["current"].get("actions", []), snapshot["current"].get("facts", {})),
                                         prompt_title=prompt_title))
    if fresh and owner == "activity_wait" and result.get("state") == "collected":
        lines.append("Check progress → play look (collect repeats this recorded result).")
    if any(line.startswith("Movement stopped:") for line in lines):
        lines = [line for line in lines
                 if not re.fullmatch(r"(?:[\w ]+\.)*(?:ok: no|state: rejected)", line)
                 and not re.match(r"(?:[\w ]+\.)*next action: Choose from terminal_observation actions", line)]
        if "Next: play look" not in lines:
            lines.append("Next: play look")
    if "Wait interrupted. Choose from the current prompt." in lines:
        lines = [line for line in lines
                 if not re.fullmatch(r"(?:[\w ]+\.)*(?:ok: no|state: rejected|state: interrupted)", line)
                 and not re.match(r"(?:[\w ]+\.)*(?:next action: Choose from terminal_observation actions|native stop reason: native_authority_required)", line)]
        if any(line.startswith("Waited ") for line in lines):
            lines = [line for line in lines if not line.startswith("Elapsed:")]
    text = "\n".join(line for line in lines if line) or "No changes."
    return text


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


def _message_delta(current, previous, selector, *, initial=False):
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
            "initial_history": initial,
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
    identity = {k: observed.get(k) for k in ("run_id", "surface_id", "observation_id")}
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
                    result[key] = _message_delta(value, [], selector, initial=True)
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
        if str(native.get("action_id", "")).startswith("world.move.") and \
                isinstance(native.get("surface_receipt"), dict):
            outcome["native_receipt"]["accepted"] = native["surface_receipt"].get("accepted")
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
    for key in ("game_minutes", "game_turn", "avatar_moves", "game_time", "turn"):
        if key in observed and (reset or previous.get(key) != observed[key]):
            view[key] = {"before": previous.get(key), "after": observed[key]}
    snapshot = {**identity, "facts": facts, "actions": actions, "breadcrumbs": breadcrumbs}
    state = {**previous, "run_id": observed.get("run_id"), "owner": owner,
             "current": snapshot, **{k: observed[k] for k in ("game_minutes", "game_turn", "avatar_moves", "game_time", "turn") if k in observed}}
    if owner == "world":
        state["world"] = snapshot
    return view, state
