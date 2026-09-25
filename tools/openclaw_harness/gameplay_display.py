"""Task-independent change display; native evidence and grants stay with the client."""
from collections import Counter
import json
import re
import shlex

from cockpit_evidence import action_catalog, compact, decode, gameplay_fact


def world_look(snapshot, operation_availability=None, *, show_controls=True, controls_only=False):
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
        lines.extend(["", "WAIT", "Native wait menu: play act world.wait"])
    if waiting and operations.get("game.wait"):
        lines.extend(["play wait <duration> [ignore|safe|stop]",
                      "1m / 5m / 30m / 1h / 2h / 3h / 6h — native availability applies; hours require a watch.",
                      "ignore: danger and interruptions (default); safe: classified harmless interruptions; stop: stop at interruptions."])
    if remaining:
        lines.extend(["", "OTHER ADVERTISED ACTIONS"])
        for action in remaining:
            command = "play act " + shlex.quote(action["id"])
            if action.get("stable_id"):
                command += " --target " + shlex.quote(action["stable_id"])
            lines.append(clean(action.get("label", action["id"])) + " → " + command)
    lines.extend(["", "SESSION", "play look — observe · play collect — retrieve pending result · play quit — end playtest"])
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


def _plain_controls(actions, heading="ALLOWED HERE ONLY"):
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
        if action_id == "inventory.commit":
            label = ("Confirm marked items: " if "inventory.toggle" in ids else "Confirm highlighted item: ") + label
        if not action.get("enabled", True):
            lines.append(label + " (unavailable)")
            continue
        command = "play act " + shlex.quote(action_id)
        if action_id in {"menu.filter", "inventory.filter"}:
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
                    lines.append(label + (" → " + command if action.get("enabled", True) else " (unavailable)"))
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
    if isinstance(result.get("next"), dict):
        footer += "\nMore rows: --offset " + str(result["next"]["offset"])
    unavailable = len(result.get("unavailable_sources", []))
    needs_retrieval = bool(result.get("next") or unavailable)
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
            needs_retrieval = True
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
        text = f"{index}. " + facts(detail)
        raw = text.encode("utf-8")
        if len(raw) > per_row:
            needs_retrieval = True
            shown = raw[:max(0, per_row - 70)].decode("utf-8", errors="ignore")
            text = shown + f"\n[{len(text) - len(shown)} rendered characters omitted.]"
        entries.append(text)
    body = "\n".join(entries)
    if len(body.encode("utf-8")) > available:
        needs_retrieval = True
        shown = body.encode("utf-8")[:max(0, available - 70)].decode("utf-8", errors="ignore")
        body = shown + f"\n[{len(body) - len(shown)} rendered characters omitted.]"
    return "\n".join([header, body, *([footer] if needs_retrieval else [])]).rstrip()


def plain_player_output(result, *, snapshot=None, full_look=True, startup_error=None,
                        operation_availability=None, inspection_request_id=None):
    """Render the existing player projection; never replace game facts with advice."""
    import shlex
    import re

    lines = []
    status = result.get("status", {})
    if not isinstance(status, dict):
        status = {}
    if status.get("state") == "starting":
        return "Loading → play look"
    if status.get("state") in {"process_dead", "bridge_failed", "reentry_failed", "terminalization_failed"}:
        reason = startup_error or status.get("error") or status.get("reason") or "game process exited"
        return "Playtest stopped: " + str(reason).replace("pre_descriptor_no_progress", "startup failed before game controls became available")

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
                      "ignore: continue through danger (default); safe: harmless interruptions only; stop: any interruption.",
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
                  "Full technical examples → play --diagnostics controls"]
        return "\n".join(lines)
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
        summary.append(f"Read a field → play inspect {selector}.INDEX.value.PATH{request_option}\n"
                       "INDEX and PATH are listed below. Copy raw values into checks; quoted strings stay quoted.")
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
                    summary.append(f"  INDEX {index}; same PATH/FIELD as {field_sets[signature]}.")
                else:
                    field_sets[signature] = entry["citation_id"]
                    summary.append(f"  INDEX {index}; PATH " + (relative_path + ".FIELD" if relative_path else "FIELD") +
                                   "; FIELD: " + ", ".join(fields))
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
    if fresh:
        facts = snapshot["current"].get("facts", {})
        if owner == "world":
            if full_look:
                lines.append(world_look(snapshot, operation_availability,
                                        show_controls=not result.get("world_controls_unchanged", False)))
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
                if key in {"actions", "actions_changed"} and isinstance(item, list) and all(
                        isinstance(action, dict) and "id" in action and "enabled" in action for action in item):
                    if (operation_availability or {}).get("game.act") is not False:
                        lines.append(_plain_controls(item))
                    continue
                if key in {"current_input", "facts_changed", "outcome", "response", "new_events", "value", "slice"}:
                    write(item)
                else:
                    write(item, (label + "." if label else "") + key.removeprefix("diagnostic_").replace("_", " "))
        elif isinstance(value, list):
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
                lines.append(_plain_controls(snapshot["current"].get("actions", []),
                                             "Allowed actions replaced — use only these:"))
            elif changed:
                lines.append(_plain_controls(changed, "Changed controls (others unchanged):"))
            if not changed and not removed:
                lines.append("Allowed actions unchanged.")
        else:
            lines.append(_plain_controls(snapshot["current"].get("actions", [])))
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
    for key in ("game_minutes", "game_turn", "game_time", "turn"):
        if key in observed and (reset or previous.get(key) != observed[key]):
            view[key] = {"before": previous.get(key), "after": observed[key]}
    snapshot = {**identity, "facts": facts, "actions": actions, "breadcrumbs": breadcrumbs}
    state = {**previous, "run_id": observed.get("run_id"), "owner": owner,
             "current": snapshot, **{k: observed[k] for k in ("game_minutes", "game_turn", "game_time", "turn") if k in observed}}
    if owner == "world":
        state["world"] = snapshot
    return view, state
