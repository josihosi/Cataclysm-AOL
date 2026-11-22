"""Inspect saved overmap geometry and populations without loading a game."""
from __future__ import annotations

import json
import shutil
import tempfile
from collections import Counter
from pathlib import Path

from bandit_live_world_audit import RELEVANT_SPECIAL_IDS

# The native submap is 12 map squares, and an OMT spans two submaps.
SQUARES_PER_OMT = 24
LOCAL_WIDTH_SM = 11
AI_RADIUS = 40
HOME_SYMBOLS = {"bandit": "B", "cannibal": "C", "mi-go": "M"}
MIGO_SPECIALS = {"Mi-Go Scout Tower", "Mi-Go Encampment"}


def resolve_world(source: Path) -> Path:
    """Accept a closed world, a fixture, or the artifact from world prepare."""
    source = Path(source)
    if not source.is_dir():
        raise ValueError(f"World or fixture directory missing: {source}")
    def materialized(path):
        return (path / "worldoptions.json").is_file() or (path / "overmaps").is_dir()

    if materialized(source):
        return source.resolve()
    saves = source if source.name == "save" else source / "save"
    if saves.is_dir():
        worlds = sorted(path for path in saves.iterdir() if path.is_dir() and materialized(path))
        if len(worlds) == 1:
            return worlds[0].resolve()
        if len(worlds) > 1:
            raise ValueError(f"Select one world under {saves}")
    raise ValueError(f"No materialized save in {source}; select a closed world or installed fixture")


def _point(value):
    return isinstance(value, (list, tuple)) and len(value) == 3 and all(type(v) is int for v in value)


def _omt(position):
    return [position[0] // SQUARES_PER_OMT, position[1] // SQUARES_PER_OMT, position[2]]


def _distance(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def _read_json(path):
    with path.open(encoding="utf-8-sig") as stream:
        first = stream.readline()
        if not first.startswith("#"):
            stream.seek(0)
        data = json.load(stream)
    if not isinstance(data, dict):
        raise ValueError(f"Expected a saved object in {path.name}")
    return data


def _read_save(world, relative, stage):
    """zzip extracts beside the input. Only run it inside our private metadata copy."""
    from startup_harness import run_zzip

    path = world / relative
    if path.suffix != ".zzip":
        return _read_json(path)
    staged = stage / relative
    staged.parent.mkdir(exist_ok=True, parents=True)
    shutil.copyfile(path, staged)
    run_zzip(staged)
    plain = stage / staged.stem if staged.parent.name == "overmaps" else staged.with_suffix("")
    return _read_json(plain)


def _player_file(world, requested):
    if requested:
        if Path(requested).name != requested or not requested.endswith((".sav", ".sav.zzip")):
            raise ValueError("Select a local .sav or .sav.zzip player filename")
        if not (world / requested).is_file():
            raise ValueError(f"Player save missing: {requested}")
        return requested
    choices = {p.name.removesuffix(".zzip") for pattern in ("*.sav", "*.sav.zzip")
               for p in world.glob(pattern)}
    if len(choices) != 1:
        raise ValueError(f"Choose --player-save from {sorted(choices)}")
    name = choices.pop()
    return name + ".zzip" if (world / (name + ".zzip")).is_file() else name


def _local_bounds(player, width):
    if not all(type(player.get(k)) is int for k in ("levx", "levy", "om_x", "om_y")):
        return None
    x = player["om_x"] * width * 2 + player["levx"]
    y = player["om_y"] * width * 2 + player["levy"]
    return [x // 2, y // 2, (x + LOCAL_WIDTH_SM - 1) // 2, (y + LOCAL_WIDTH_SM - 1) // 2]


def _plane(payload, z, name):
    from startup_harness import OMAPX, OMAPY, overmap_layer_index

    layers = payload.get("layers")
    index = overmap_layer_index(z)
    if not isinstance(layers, list) or index >= len(layers):
        raise ValueError(f"Missing terrain layer z={z} in {name}")
    cells = []
    size = OMAPX * OMAPY
    for pair in layers[index]:
        if (not isinstance(pair, list) or len(pair) != 2 or not isinstance(pair[0], str)
                or type(pair[1]) is not int or not 0 < pair[1] <= size - len(cells)):
            raise ValueError(f"Invalid saved terrain run in {name}, z={z}")
        terrain, count = pair
        cells.extend([terrain] * count)
    if len(cells) != size:
        raise ValueError(f"Incomplete terrain layer in {name}, z={z}")
    return cells


def _faction(kind, profile=None):
    if kind == "cannibal_camp" or profile == "cannibal_camp":
        return "cannibal"
    if kind in MIGO_SPECIALS:
        return "mi-go"
    if kind in RELEVANT_SPECIAL_IDS:
        return "bandit"
    return "other"


def _placement_sites(payload, ox, oy, width):
    from startup_harness import anchor_from_special_points

    for special in payload.get("overmap_special_placements", []):
        kind = special["special"]
        faction = _faction(kind)
        if faction == "other":
            continue
        for group in special["placements"]:
            points = []
            for entry in group["points"]:
                pos = entry["p"]
                if not _point(pos):
                    raise ValueError(f"Invalid {kind} footprint")
                points.append([ox * width + pos[0], oy * width + pos[1], pos[2]])
            ground = [pt for pt in points if pt[2] == 0]
            anchor = list(anchor_from_special_points(ground or points))
            if kind in MIGO_SPECIALS:
                # Mi-go encampments bind their population to the camp1 ground tile.
                prefix = "tower" if kind == "Mi-Go Scout Tower" else "camp"
                ident = f"mi-go-{prefix}:({anchor[0]},{anchor[1]},0)"
            else:
                ident = f"overmap_special:{kind}@{anchor[0]},{anchor[1]},{anchor[2]}"
            yield dict(site_id=ident, faction=faction, kind=kind, anchor=anchor, footprint=points,
                       placement=True, registered=None)


def _region_facts(payload, npcs, horde_tiles, center, radius):
    for npc in payload.get("npcs", []):
        pos = npc.get("location")
        if _point(pos) and isinstance(npc.get("id"), (int, str)):
            npcs[str(npc["id"])] = dict(id=npc["id"], name=npc.get("name"), omt=_omt(pos),
                                    dead=npc.get("dead"), faction=npc.get("my_fac"))
    # Native horde_map serializes alternating absolute map-square keys and bodies.
    horde = payload.get("horde_map", [])
    for body in horde[1::2]:
        if isinstance(body, dict) and _point(body.get("location")):
            pos = _omt(body["location"])
            if _distance(pos, center) <= radius:
                horde_tiles.setdefault(tuple(pos), Counter())[body.get("typeid", "unknown")] += 1


def _merge_populations(sites, buffer, npcs, avatar):
    human_records = buffer.get("bandit_live_world", {}).get("sites")
    migo_records = buffer.get("migo_populations", {}).get("sites")
    for row in sites.values():
        present = migo_records if row["faction"] == "mi-go" else human_records
        row["registered"] = False if isinstance(present, list) else None
    for site in human_records or []:
        kind = site.get("site_kind")
        anchor = site.get("anchor")
        if not _point(anchor):
            continue
        ident = site["site_id"]
        row = sites.setdefault(ident, dict(site_id=ident, kind=kind, anchor=anchor,
                                           footprint=site.get("footprint", []), placement=None))
        row.update(faction=_faction(kind, site.get("hostile_profile")), registered=True,
                   living_total=site.get("living_total"), members=[])
        for member in site.get("members", []):
            ident = member.get("npc_id")
            npc = npcs.get(str(ident)) or {}
            row["members"].append(dict(id=ident, state=member.get("state"), omt=npc.get("omt")))
        outing = site.get("active_outing") or {}
        if outing.get("activity_id"):
            party = {key: outing.get(key) for key in ("activity_id", "phase", "member_ids")}
            ids = party["member_ids"] or []
            bodies = [npcs.get(str(member_id)) for member_id in ids]
            survivors = [body for body in bodies if body and body["dead"] is False]
            if any(_distance(body["omt"], avatar) <= AI_RADIUS for body in survivors):
                eligibility = "inside"
            elif bodies and all(body and type(body["dead"]) is bool for body in bodies):
                eligibility = "outside" if survivors else "no_survivors"
            else:
                eligibility = "unknown"
            party["saved_member_range"] = eligibility
            party["member_positions"] = [{"id": ident, "omt": body["omt"] if body else None}
                                         for ident, body in zip(ids, bodies)]
            row["outing"] = party
    for site in migo_records or []:
        anchor = site.get("anchor")
        if not _point(anchor):
            continue
        ident = site["id"]
        # Multi-tile encampments can rotate. Their population owner supplies the real anchor.
        matches = [key for key, row in sites.items() if row["faction"] == "mi-go"
                   and anchor in row["footprint"]]
        if ident not in sites and len(matches) == 1:
            sites[ident] = sites.pop(matches[0])
        row = sites.setdefault(ident, dict(kind="mi-go", faction="mi-go", footprint=[], placement=None))
        row.update(site_id=ident, anchor=anchor, registered=True, seed_committed=site.get("seed_committed"),
                   defensive_reserve=site.get("defensive_reserve"), members=[])
        for member in site.get("members", []):
            position = member.get("current_position")
            row["members"].append(dict(id=member.get("identity", {}).get("member"), hp=member.get("hp"),
                                       omt=_omt(position) if _point(position) else None))


def _tower_operations(sites, buffer):
    for director in buffer.get("migo_towers", {}).get("sites", []):
        row = sites.get(director.get("site_id"))
        operation = director.get("operation")
        if not row or not isinstance(operation, dict):
            continue
        row["operation"] = {key: operation.get(key) for key in
                            ("generation", "resolved", "party_at_departure", "communicated_losses")}
        row["operation"]["members"] = [
            {"id": member.get("member_id"), "settled": member.get("settled"), "returned": member.get("returned"),
             "last_observed_omt": _omt(member["last_position"]) if _point(member.get("last_position")) else None}
            for member in operation.get("members", [])
        ]


def inspect_world(source: Path, *, player_save=None, radius=AI_RADIUS, center=None, z=None, include_map=False):
    from startup_harness import OMAPX, OMAPY, OVERMAP_DEPTH

    if radius not in (20, 40):
        raise ValueError("Choose a 40-OMT view or a 20-OMT zoom")
    world = resolve_world(source)
    filename = _player_file(world, player_save)
    z = 0 if z is None else z
    if type(z) is not int or not -OVERMAP_DEPTH <= z <= 10:
        raise ValueError("Saved z-level must be between -10 and 10")
    sites, npcs, planes = {}, {}, {}
    horde_tiles = {}
    # Verify a single saved snapshot across player, regions and population metadata.
    # Include absent paths so a concurrently installed overmap cannot become a silent gap.
    inputs = {}

    def identity(path):
        try:
            stat = path.stat()
        except FileNotFoundError:
            return None
        return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns

    def present(relative):
        path = world / relative
        inputs[path] = identity(path)
        return inputs[path] is not None

    dim = world / "dimension_data.gsav"
    present(filename)
    has_dimension = present("dimension_data.gsav")
    with tempfile.TemporaryDirectory(prefix="caol-world-inspect-") as temp:
        stage = Path(temp)
        (stage / "worldoptions.json").write_text("[]", encoding="utf-8")
        if present("overmaps.dict"):
            shutil.copyfile(world / "overmaps.dict", stage / "overmaps.dict")
        saved = _read_save(world, filename, stage)
        pos = saved.get("player", {}).get("location")
        if not _point(pos):
            raise ValueError(f"Invalid absolute player location in {filename}")
        avatar = _omt(pos)
        if center is None:
            center = avatar[:2]
        if len(center) != 2 or any(type(v) is not int for v in center):
            raise ValueError("Map center requires absolute X and Y in OMT")
        x0, y0 = center[0] - radius, center[1] - radius
        x1, y1 = center[0] + radius, center[1] + radius
        for oy in range(y0 // OMAPY, y1 // OMAPY + 1):
            for ox in range(x0 // OMAPX, x1 // OMAPX + 1):
                name = f"o.{ox}.{oy}"
                candidates = (Path("overmaps") / (name + ".zzip"), Path("overmaps") / name, Path(name))
                relative = next((path for path in candidates if present(path)), None)
                if relative is None:
                    continue
                payload = _read_save(world, relative, stage)
                planes[ox, oy] = _plane(payload, z, name)
                for row in _placement_sites(payload, ox, oy, OMAPX):
                    sites[row["site_id"]] = row
                _region_facts(payload, npcs, horde_tiles, center, radius)
    buffer = _read_json(dim).get("overmapbuffer", {}) if has_dimension else {}
    if any(identity(path) != before for path, before in inputs.items()):
        raise ValueError("Saved metadata changed during inspection; inspect a settled save")
    _merge_populations(sites, buffer, npcs, avatar)
    _tower_operations(sites, buffer)
    homes = []
    for row in sites.values():
        distances = [_distance(p, center) for p in row["footprint"] if _point(p)]
        row["edge_distance_omt"] = min(distances, default=None)
        row["anchor_distance_omt"] = _distance(row["anchor"], center)
        row["anchor_in_ai_range"] = _distance(row["anchor"], avatar) <= AI_RADIUS
        near_member = any(member.get("omt") and _distance(member["omt"], center) <= radius
                          for member in row.get("members", []))
        party_positions = row.get("outing", {}).get("member_positions", [])
        near_party = any(p["omt"] and _distance(p["omt"], center) <= radius for p in party_positions)
        if min(distances + [row["anchor_distance_omt"]]) <= radius or near_member or near_party:
            homes.append(row)
    terrain = Counter()
    rows = []
    for y in range(y0, y1 + 1):
        oy, ly = divmod(y, OMAPY)
        line = []
        for x in range(x0, x1 + 1):
            ox, lx = divmod(x, OMAPX)
            plane = planes.get((ox, oy))
            tile = plane[ly * OMAPX + lx] if plane is not None else None
            terrain[tile] += 1
            if include_map:
                line.append(tile)
        if include_map:
            rows.append(line)
    predators = [dict(id=a.get("actor_id"), kind=a.get("type"), omt=_omt(a["position"]))
                 for a in buffer.get("predator_activity", {}).get("actors", [])
                 if _point(a.get("position")) and _distance(_omt(a["position"]), center) <= radius]
    horde_counts = Counter()
    for counts in horde_tiles.values():
        horde_counts.update(counts)
    unknown = terrain.pop(None, 0)
    result = dict(world=world.name, player_save=filename, player_omt=avatar, center_omt=list(center),
                  radius_omt=radius, z=z, bounds=[x0, y0, x1, y1], local_map_omt=_local_bounds(saved, OMAPX),
                  saved_tiles=sum(terrain.values()), unknown_tiles=unknown, terrain_counts=dict(terrain.most_common()),
                  sites=sorted(homes, key=lambda row: (row["anchor_distance_omt"], row["site_id"])),
                  npcs=[n for n in npcs.values() if _distance(n["omt"], center) <= radius],
                  predators=predators, horde_monster_counts=dict(horde_counts),
                  horde_tiles=[{"omt": list(point), "types": dict(counts)}
                               for point, counts in sorted(horde_tiles.items())],
                  populations_available={"human": isinstance(buffer.get("bandit_live_world", {}).get("sites"), list),
                                         "mi-go": isinstance(buffer.get("migo_populations", {}).get("sites"), list)})
    if include_map:
        result["terrain_rows"] = rows
    return result
