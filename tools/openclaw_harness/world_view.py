"""Compact terminal rendering of a saved world inspection."""
from __future__ import annotations

from collections import Counter

from world_inspect import HOME_SYMBOLS


def _glyph(tile):
    if tile is None:
        return "?"
    if tile.startswith(("river", "lake", "water", "ocean")):
        return "~"
    if tile.startswith(("bridge", "road", "highway", "street", "railroad", "forest_trail")):
        return "="
    if tile.startswith(("forest", "orchard")):
        return "T"
    if tile.startswith(("field", "grass", "open_air", "empty_rock", "crater")):
        return "."
    if tile.startswith(("house", "cabin", "shop", "s_", "office", "apartments", "church")):
        return "#"
    return "+"


def _map(report):
    x0, y0, x1, y1 = report["bounds"]
    symbols = {}
    z = report["z"]
    for row in report["sites"]:
        for x, y, pz in row["footprint"]:
            if pz == z:
                symbols[x, y] = HOME_SYMBOLS.get(row["faction"], "H")
    for actor in report["npcs"] + report["predators"]:
        x, y, pz = actor["omt"]
        if pz == z:
            symbols.setdefault((x, y), "n" if "name" in actor else "p")
    for tile in report["horde_tiles"]:
        x, y, pz = tile["omt"]
        if pz == z:
            symbols.setdefault((x, y), "h")
    ax, ay, az = report["player_omt"]
    symbols[ax, ay] = "@" if az == z else "^"
    local = report["local_map_omt"]
    # Use brackets outside the first/last cell, so terrain inside the bubble stays visible.
    def row_prefix(y, x):
        if local and local[1] <= y <= local[3]:
            if x == local[0]:
                return "["
            if x == local[2] + 1:
                return "]"
        return " " if local and x in (local[0], local[2] + 1) else ""

    lines = [f"Map · x {x0} to {x1} west to east · z {z} · north at top"]
    for y, row in enumerate(report["terrain_rows"], y0):
        line = "".join(row_prefix(y, x) + symbols.get((x, y), _glyph(tile))
                       for x, tile in enumerate(row, x0))
        lines.append(f"{y:>5}  {line}{row_prefix(y, x1 + 1)}")
    lines.append("@ avatar · ^ avatar on another level · B bandit · C cannibal · M Mi-go · n NPC · p predator · h overmap monster")
    lines.append(". open · T woods · ~ water · = road/trail/rail · # building · + other · ? unsaved · [ ] local map")
    return lines


def render(report):
    avatar = report["player_omt"]
    lines = [f"{report['world']}   |   {report['radius_omt']} OMT radius   |   z={report['z']}",
             f"Player {tuple(avatar)} · view center {tuple(report['center_omt'])} · "
             f"{report['saved_tiles']:,} saved tiles, {report['unknown_tiles']:,} unknown"]
    if report["center_omt"] != avatar[:2]:
        lines.append(f"AI range stays centered on the player at {tuple(avatar[:2])}, radius 40 OMT")
    if report["local_map_omt"]:
        x0, y0, x1, y1 = report["local_map_omt"]
        lines.append(f"Local map intersects OMT x {x0}..{x1}, y {y0}..{y1}")
    if report["terrain_counts"]:
        common = list(report["terrain_counts"].items())[:6]
        lines.append("Terrain    " + ", ".join(f"{name} × {count}" for name, count in common))
    for row in report["sites"]:
        known = row["registered"]
        if known is None:
            state = "population unavailable"
        elif not known:
            state = "placed footprint, no population record"
        else:
            state = f"{len(row['members'])} member records"
            if row.get("living_total") is not None:
                state += f", saved living total {row['living_total']}"
            if row.get("defensive_reserve") is not None:
                state += f", reserve {row['defensive_reserve']}"
        if row.get("outing"):
            state += f", outing {row['outing'].get('phase')} ({row['outing']['saved_member_range']} AI range)"
        if row.get("operation"):
            operation = row["operation"]
            phase = "resolved" if operation.get("resolved") else "open"
            state += f", {phase} tower operation with {len(operation['members'])} member records"
        if not row["anchor_in_ai_range"]:
            state += ", home anchor outside player AI range"
        distance = row["edge_distance_omt"]
        edge = f"{distance} OMT to edge" if distance is not None else "footprint unknown"
        lines.append(f"{row['kind']} {tuple(row['anchor'])} · {edge} · {state}")
    if not report["sites"]:
        lines.append("No home identified in the inspected area.")
    counts = Counter(a["kind"] for a in report["predators"])
    populations = [f"{len(report['npcs'])} saved NPCs"]
    populations += [f"{n} {k}" for k, n in counts.most_common()]
    populations += [f"{sum(report['horde_monster_counts'].values())} horde-map monsters"]
    lines.append("Actors     " + ", ".join(populations))
    for check in report.get("requirements", []):
        lines.append(f"Requirement {check['status']}    {check['detail']}")
    if "terrain_rows" in report:
        lines.extend(_map(report))
    return "\n".join(lines)
