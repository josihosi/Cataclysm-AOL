"""Build a disposable playtest world and verify the saved starting conditions."""
from __future__ import annotations

from collections import Counter, defaultdict
from pathlib import Path
import json
import os
import shutil
import tempfile

import startup_harness as h


def _keys(value, allowed, required=()):
    if not isinstance(value, dict):
        raise ValueError(f"expected an object with fields {sorted(allowed)}")
    unknown = value.keys() - set(allowed)
    missing = set(required) - value.keys()
    if unknown or missing:
        raise ValueError(f"unexpected fields {sorted(unknown)}, missing fields {sorted(missing)}")


def _point(value):
    if not isinstance(value, (list, tuple)) or len(value) != 3 or any(type(v) is not int for v in value):
        raise ValueError(f"expected three integer coordinates, got {value!r}")
    return tuple(value)


def _name(value):
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"expected a nonempty name, got {value!r}")
    return value


def _locations(definitions, player):
    if not isinstance(definitions, dict) or "player" in definitions:
        raise ValueError("locations must be an object; player is the saved avatar location")
    found = {"player": tuple(player)}

    def resolve(value, pending=()):
        if isinstance(value, str):
            if value in found:
                return found[value]
            if value not in definitions or value in pending:
                raise ValueError(f"unresolved or circular location: {' -> '.join((*pending, value))}")
            found[value] = resolve(definitions[value], (*pending, value))
            return found[value]
        if isinstance(value, dict):
            _keys(value, {"from", "offset_omt"}, {"from", "offset_omt"})
            base = resolve(_name(value["from"]), pending)
            return tuple(a + b for a, b in zip(base, _point(value["offset_omt"])))
        return _point(value)

    for name in definitions:
        resolve(_name(name))
    return found, resolve


def _fixture(source):
    """Resolve inherited fixture bytes and the transforms normal installation would run."""
    if (source / "manifest.json").is_file():
        manifest = h.load_fixture_manifest(source)
        profile, name = source.parent.name, source.name
        if source == (h.profile_fixture_root(profile) / name).resolve():
            binding = h.resolve_fixture_payload(name, profile)
            save, transforms = binding["save_src"], binding["save_transforms"]
        else:
            save = source / "save"
            transforms = h.normalize_fixture_save_transforms(manifest.get("save_transforms", []),
                                                             manifest_path=source / "manifest.json")
        if manifest.get("world"):
            return save / _component(manifest["world"]), transforms
    else:
        from world_inspect import resolve_world
        return resolve_world(source), []
    worlds = [p for p in save.iterdir() if (p / "worldoptions.json").is_file()]
    if len(worlds) != 1:
        raise ValueError(f"select a single world below {save}")
    return worlds[0], transforms


def _component(name):
    if (not isinstance(name, str) or not name or name in {".", ".."} or
            "/" in name or "\\" in name or ":" in name or Path(name).name != name):
        raise ValueError(f"expected a single filename, got {name!r}")
    return name


def _hash(world):
    digest, error = h.sha256_tree(world)
    if error:
        raise ValueError(error)
    return digest


def _check_closed(world):
    if world.parent.name == "save":
        lease = world.parent.parent / ".harness-owner.lock"
        if lease.is_file():
            h._validate_closed_saved_world_owner(json.loads(lease.read_bytes()))


def _special_definition(special):
    path = h.repo_root() / "data/json/overmap/overmap_special/specials.json"
    definition = next((d for d in json.loads(path.read_bytes()) if d.get("id") == special), None)
    # These fixed home layouts have a real data-defined floor plan and a native
    # site owner. Other special kinds can carry mutable joins or mission state.
    if special not in set(h.BANDIT_HOME_SPECIAL_IDS) | {"cannibal_camp", "Mi-Go Scout Tower", "Mi-Go Encampment"}:
        raise ValueError(f"unsupported fixed home {special!r}; use native overmap placement")
    if not definition or not definition.get("overmaps"):
        raise ValueError(f"no fixed footprint for {special!r}")
    if any(not isinstance(tile.get("overmap"), str) for tile in definition["overmaps"]):
        raise ValueError(f"{special!r} has conditional tiles; use native overmap placement")
    locations_file = h.repo_root() / "data/json/overmap/special_locations.json"
    eligible = {row["id"]: row.get("terrains", []) for row in json.loads(locations_file.read_bytes())}
    layout = []
    for tile in definition["overmaps"]:
        types = tile.get("locations", definition.get("locations", []))
        allowed = {terrain for name in types for terrain in eligible.get(name, [])}
        layout.append((_point(tile["point"]), _name(tile["overmap"]), allowed))
    return layout


def _generated_tiles(world, points):
    packs = defaultdict(set)
    for x, y, z in points:
        packs[f"{x // 32}.{y // 32}.{z}"].add(f"{x}.{y}.{z}.map")
    generated = []
    for stem, names in packs.items():
        folder = world / "maps" / stem
        packed = folder.with_name(stem + ".zzip")
        extracted = not folder.exists() and packed.exists()
        if extracted:
            h.run_zzip(packed)
        try:
            generated.extend(name for name in names if (folder / name).is_file())
        finally:
            if extracted:
                shutil.rmtree(folder)
    return generated


def _data_directories(world, core):
    data = h.repo_root() / "data"
    yield data / "json" / core
    active = json.loads((world / "mods.json").read_bytes()) if (world / "mods.json").is_file() else []
    mods = {}
    for info in (data / "mods").glob("*/modinfo.json"):
        for definition in json.loads(info.read_bytes()):
            if definition.get("type") == "MOD_INFO":
                mods[definition.get("id")] = info.parent / definition.get("path", "")
    for mod in active:
        if mod in mods and mods[mod].resolve() != (data / "json").resolve():
            yield mods[mod]


def _defined_terrain(world):
    """Expand terrain types into the saved IDs produced by oter_type_t::finalize."""
    definitions = defaultdict(list)
    concrete = set()
    for directory in _data_directories(world, "overmap/overmap_terrain"):
        for path in sorted(directory.rglob("*.json")):
            raw = path.read_bytes()
            if b'"overmap_terrain"' not in raw:
                continue
            for row in json.loads(raw):
                if not isinstance(row, dict) or row.get("type") != "overmap_terrain":
                    continue
                names = row.get("id", row.get("abstract", []))
                for name in names if isinstance(names, list) else [names]:
                    definitions[name].append(row)
                    if "id" in row:
                        concrete.add(name)

    def flags_for(name, index=-1, visiting=frozenset()):
        rows = definitions.get(name, [])
        if not rows:
            return None
        i = len(rows) - 1 if index == -1 else index
        if (name, i) in visiting or i < 0:
            return None
        row = rows[i]
        parent = row.get("copy-from")
        if "flags" in row:
            flags = set(row["flags"])
        elif parent == name and i == 0:
            return None
        elif parent:
            flags = flags_for(parent, i - 1 if parent == name else -1, visiting | {(name, i)})
            if flags is None:
                return None
        else:
            flags = set()
        return (flags | set(row.get("extend", {}).get("flags", []))) - set(row.get("delete", {}).get("flags", []))

    ids = {}
    lines = ("isolated", "end_north", "end_south", "end_east", "end_west",
             "ne", "es", "sw", "wn", "ns", "ew", "nes", "esw", "nsw", "new", "nesw")
    for name in concrete:
        flags = flags_for(name)
        if flags is None:
            continue
        if "NO_ROTATE" in flags:
            ids[name] = name
        else:
            directions = lines if "LINEAR" in flags else ("north", "east", "south", "west")
            ids.update((f"{name}_{direction}", name) for direction in directions)
    return ids


def _edit_geometry(world, terrain, specials, resolve):
    edits = defaultdict(dict)
    groups = defaultdict(list)
    receipt = []
    game_terrain = None
    special_locations = {}

    def put(point, new, special):
        ox, oy, local = h.overmap_file_coords_from_abs_omt(point)
        h.overmap_layer_index(point[2])
        if local in edits[ox, oy]:
            raise ValueError(f"overlapping recipe edits at {point}")
        edits[ox, oy][local] = (point, new, special)

    for row in terrain:
        _keys(row, {"at", "id", "to"}, {"at", "id"})
        start = resolve(row["at"])
        end = resolve(row.get("to", row["at"]))
        delta = [b - a for a, b in zip(start, end)]
        distance = max(map(abs, delta))
        if start[2] != end[2] or any(n not in (0, distance) for n in map(abs, delta)):
            raise ValueError("terrain paths must be cardinal or diagonal on a single z level")
        if distance > h.OMAPX:
            raise ValueError(f"terrain path exceeds one overmap width ({h.OMAPX} OMT)")
        for i in range(distance + 1):
            point = tuple(a + (1 if step > 0 else -1 if step < 0 else 0) * i for a, step in zip(start, delta))
            put(point, _name(row["id"]), None)
        receipt.append({"terrain": row["id"], "from_omt": list(start), "to_omt": list(end), "tiles": distance + 1})

    for row in specials:
        _keys(row, {"at", "id"}, {"at", "id"})
        anchor = resolve(row["at"])
        special = _name(row["id"])
        ox, oy, origin = h.overmap_file_coords_from_abs_omt(anchor)
        footprint = []
        for offset, identifier, allowed in _special_definition(special):
            pt = tuple(a + b for a, b in zip(anchor, offset))
            px, py, local = h.overmap_file_coords_from_abs_omt(pt)
            if (px, py) != (ox, oy):
                raise ValueError("special footprint crosses an overmap seam; choose a nearby anchor")
            footprint.append(local)
            put(pt, identifier, special)
            special_locations[pt] = allowed
        groups[ox, oy].append((special, origin, footprint))
        receipt.append({"special": special, "anchor_omt": list(anchor), "footprint_omt":
                        [list(edits[ox, oy][p][0]) for p in footprint]})

    for (ox, oy), changes in edits.items():
        archived = world / "overmaps" / f"o.{ox}.{oy}.zzip"
        if not archived.exists():
            raise ValueError(f"unknown map at {next(iter(changes.values()))[0]}: overmap {ox},{oy} is unsaved")
        plain, version, payload = h.extract_overmap_payload(archived)
        layers = payload["layers"]
        planes = {}
        occupied = {tuple(point["p"]): entry["special"]
                    for entry in payload["overmap_special_placements"]
                    for placement in entry["placements"] for point in placement["points"]}
        known = {entry[0] for layer in layers for entry in layer}
        dirty = []
        existing_groups = {(entry["special"], tuple(site.get("origin") or site["points"][0]["p"]),
                            frozenset(tuple(p["p"]) for p in site["points"]))
                           for entry in payload["overmap_special_placements"] for site in entry["placements"]
                           if site["points"]}
        reused = {point for special, origin, footprint in groups[ox, oy]
                  if (special, origin, frozenset(footprint)) in existing_groups for point in footprint}
        for local, (absolute, identifier, special) in changes.items():
            if local in occupied and (local not in reused or occupied[local] != special):
                raise ValueError(f"{absolute} already belongs to special {occupied[local]}")
            if special is None and identifier not in known:
                if game_terrain is None:
                    game_terrain = _defined_terrain(world)
                if identifier not in game_terrain:
                    raise ValueError(f"unknown terrain {identifier!r} in the save and active game data")
            z = h.overmap_layer_index(local[2])
            if z not in planes:
                planes[z] = h.decode_overmap_layer(layers[z], context=str(archived))
            idx = h.overmap_flat_index(local)
            previous = planes[z][idx]
            if local in reused:
                if previous != identifier:
                    raise ValueError(f"existing {special} footprint terrain disagrees at {absolute}: {previous}")
                continue
            if special:
                if game_terrain is None:
                    game_terrain = _defined_terrain(world)
                terrain_type = game_terrain.get(previous, previous)
                empty_layer = (absolute[2] > 0 and previous == "open_air") or (
                    absolute[2] < 0 and previous in {"empty_rock", "solid_earth", "deep_rock"})
                if not empty_layer and terrain_type not in special_locations[absolute]:
                    raise ValueError(f"unsuitable terrain {previous!r} for {special} at {absolute}")
            if special or previous != identifier:
                dirty.append(absolute)
            planes[z][idx] = identifier
        existing = _generated_tiles(world, dirty)
        if existing:
            raise ValueError(f"existing generated local terrain at {sorted(existing)[0]}; choose an ungenerated tile")
        if not dirty:
            h.cleanup_extracted_overmap(plain, keep=not payload.get("_created_plain", False))
            continue
        for z, plane in planes.items():
            layers[z] = h.encode_overmap_layer(plane)
        for special, origin, footprint in groups[ox, oy]:
            if (special, origin, frozenset(footprint)) in existing_groups:
                continue
            h.upsert_special_placement(payload, special_id=special, placement_origin=origin, placement_points=footprint)
            # Native mapgen lazily rolls parameters once for the whole special.
            # Each OMT must refer to the same initially empty argument slot,
            # otherwise mapgendata cannot initialize the special on first visit.
            storage = payload.setdefault("mapgen_arg_storage", [])
            index = payload.setdefault("mapgen_arg_index", [])
            if any(tuple(p) in footprint for p, _ in index):
                raise ValueError(f"mapgen parameters already occupy the target of {special} at {origin}")
            slot = len(storage)
            storage.append(None)
            index.extend([[list(p), slot] for p in footprint])
        h.write_overmap_payload(plain, version, payload)

        check_path, _, check = h.extract_overmap_payload(archived)
        try:
            actual_layers = {z: h.decode_overmap_layer(check["layers"][z], context=str(archived)) for z in planes}
            for local, (absolute, identifier, _) in changes.items():
                if actual_layers[h.overmap_layer_index(local[2])][h.overmap_flat_index(local)] != identifier:
                    raise ValueError(f"terrain readback mismatch at {absolute}")
            for special, origin, footprint in groups[ox, oy]:
                if not any(entry["special"] == special and any(tuple(site["origin"]) == origin and
                           {tuple(p["p"]) for p in site["points"]} == set(footprint) for site in entry["placements"])
                           for entry in check["overmap_special_placements"]):
                    raise ValueError(f"missing saved {special} footprint at {origin}")
        finally:
            h.cleanup_extracted_overmap(check_path, keep=False)
    return receipt

def _tile_samples(world, player_save, offsets):
    if not offsets:
        return {}
    # The common auditor decodes the native item and field arrays. An unsaved
    # submap yields no row, distinct from a saved tile containing no fire.
    report = h.audit_map_tiles_near_player(world, player_save=player_save, offsets=sorted(offsets))
    return {tuple(tile["offset_ms"]): tile for tile in report["tiles"]}


def _item_counts(samples):
    counts = Counter()
    for offset, tile in samples.items():
        for item in tile["item_states"]:
            counts[offset, item["typeid"]] += item.get("count", 1)
    return counts


def _validate_item_ids(world, requested):
    missing = set(requested)
    if not missing:
        return
    for directory in _data_directories(world, "items"):
        for definition in directory.rglob("*.json"):
            raw = definition.read_bytes()
            if any(json.dumps(item, ensure_ascii=False).encode("utf-8") in raw for item in missing):
                for row in json.loads(raw):
                    if isinstance(row, dict) and row.get("type") == "ITEM" and isinstance(row.get("id"), str):
                        missing.discard(row["id"])
                if not missing:
                    return
    raise ValueError(f"unknown item IDs in active game data: {', '.join(sorted(missing))}")


def _items(world, rows, player_save):
    items = []
    additions = Counter()
    for row in rows:
        _keys(row, {"id", "count", "charges", "offset_ms"}, {"id"})
        item = _name(row["id"])
        offset = _point(row.get("offset_ms", [0, 0, 0]))
        count = row.get("count", 1)
        if type(count) is not int or count < 1:
            raise ValueError("item count must be a positive integer")
        entry = {"typeid": item, "count": count, "offset_ms": list(offset)}
        if "charges" in row:
            if type(row["charges"]) is not int or row["charges"] < 0:
                raise ValueError("item charges must be a nonnegative integer")
            entry["charges"] = row["charges"]
        items.append(entry)
        additions[offset, item] += count
    if not items:
        return []
    _validate_item_ids(world, [entry["typeid"] for entry in items])
    offsets = {offset for offset, _ in additions}
    before = _item_counts(_tile_samples(world, player_save, offsets))
    h.apply_map_items_near_player_transform(world, {"player_save": player_save, "items": items})
    after = _item_counts(_tile_samples(world, player_save, offsets))
    for key, added in additions.items():
        if after[key] != before[key] + added:
            raise ValueError(f"item readback mismatch for {key[1]} at {key[0]}")
    return [{"id": item, "offset_ms": list(offset), "added": added, "total_on_tile": after[offset, item]}
            for (offset, item), added in sorted(additions.items())]


def _staffing(world, special, anchor):
    path = world / "dimension_data.gsav"
    if not path.exists():
        return "unknown", "No saved population ledger"
    raw = path.read_bytes()
    ledger = json.loads(raw.split(b"\n", 1)[1] if raw.startswith(b"#") else raw).get("overmapbuffer", {})
    if special in {"Mi-Go Scout Tower", "Mi-Go Encampment"}:
        sites = ledger.get("migo_populations", {}).get("sites")
        if not isinstance(sites, list):
            return "unknown", "No saved Mi-go population ledger"
        for site in sites:
            if site.get("anchor") == list(anchor) and site.get("tower") == (special == "Mi-Go Scout Tower"):
                members = site.get("members", [])
                # migo_population.h: available duty (0) and a living slot, local
                # body or abstract owner. Dispatched/returning members are away.
                count = sum(isinstance(m, dict) and isinstance(m.get("hp"), int) and m["hp"] > 0
                            and m.get("duty") == 0 and m.get("owner") in (0, 1, 2)
                            for m in members)
                if site.get("seed_committed") and count:
                    return "met", f"{count} available living committed Mi-go members"
                return "unmet", "No available living committed Mi-go members"
    else:
        sites = ledger.get("bandit_live_world", {}).get("sites")
        if not isinstance(sites, list):
            return "unknown", "No saved human population ledger"
        for site in sites:
            if site.get("anchor") == list(anchor) and special in (site.get("site_kind"), site.get("hostile_profile")):
                members = [m for m in site.get("members", []) if m.get("state") == "at_home"
                           and type(m.get("npc_id")) is int and m["npc_id"] > 0]
                if members:
                    return "met", f"{len(members)} identified members recorded at home"
                if site.get("living_total", 0) > 0 and not site.get("members"):
                    return "unknown", f"Living total {site['living_total']} without individual member records"
                return "unmet", "No identified members recorded at home"
    return "unmet", "No registered population at this anchor; use native setup and save to verify staffing"


def _requirements(world, player_save, rows, resolve, *, origin_shift=(0, 0, 0)):
    answers = []
    samples = {}
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("each requirement must be an object")
        if "staffed" in row:
            _keys(row, {"staffed", "at"}, {"staffed", "at"})
            status, detail = _staffing(world, _name(row["staffed"]), resolve(row["at"]))
        else:
            kind = "item" if "item" in row else "field"
            state = "active" if kind == "item" else "present"
            _keys(row, {kind, state, "offset_ms"}, {kind, state})
            identifier = _name(row[kind])
            expected = row[state]
            if type(expected) is not bool:
                raise ValueError(f"{state} must be true or false")
            offset = tuple(a + b for a, b in zip(_point(row.get("offset_ms", [0, 0, 0])), origin_shift))
            if offset not in samples:
                samples[offset] = _tile_samples(world, player_save, {offset}).get(offset)
            tile = samples[offset]
            if tile is None:
                status, detail = "unknown", "Local map tile is unsaved"
            elif kind == "field":
                seen = any(f.get("field_id") == identifier for f in tile["fields"])
                if not seen and any("raw" in f for f in tile["fields"]):
                    status, detail = "unknown", "Unrecognized saved field data"
                else:
                    status, detail = ("met" if seen == expected else "unmet"), f"{identifier} present={seen}"
            else:
                states = [i["active"] for i in tile["item_states"] if i["typeid"] == identifier]
                # For an off source, require a matching item and every matching copy off.
                matches = bool(states) and any(states) == expected
                status, detail = ("met" if matches else "unmet"), f"{len(states)} matching items, {sum(states)} active"
        answers.append({"requirement": row, "status": status, "detail": detail})
    return answers


def prepare_world(source: Path, recipe: dict, destination: Path, *, player_save: str | None = None) -> dict:
    """Publish a fresh, byte-verified disposable copy; leave all parent saves untouched."""
    _keys(recipe, {"name", "locations", "specials", "terrain", "items", "requires"})
    if "name" in recipe:
        _name(recipe["name"])
    for field in ("terrain", "specials", "items", "requires"):
        if field in recipe and not isinstance(recipe[field], list):
            raise ValueError(f"{field} must be a list")
    source = Path(source).expanduser().resolve(strict=True)
    original, transforms = _fixture(source)
    original = original.resolve(strict=True)
    if not (original / "worldoptions.json").is_file():
        raise ValueError(f"no saved world found at {original}")
    requested = Path(destination).expanduser().absolute()
    if requested.is_symlink() or requested.exists():
        raise FileExistsError(f"choose an unused preparation destination: {requested}")
    dest = requested.resolve()
    if dest == source or source in dest.parents or dest == original or original in dest.parents:
        raise ValueError("destination must be outside the source")
    if any((parent / "worldoptions.json").exists() or (parent / ".harness-owner.lock").exists()
           for parent in dest.parents):
        raise ValueError("destination must be outside existing worlds and owned harness profiles")
    if any(p.is_symlink() or (getattr(p, "is_junction", lambda: False)()) for p in original.rglob("*")):
        raise ValueError("world contains a filesystem link; supply a materialized world copy")
    from world_inspect import _player_file
    player_save = _player_file(original, _component(player_save) if player_save is not None else None)
    _check_closed(original)
    before = _hash(original)
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".prep-", dir=dest.parent) as temp:
        stage = Path(temp) / "prepared"
        world = stage / "save" / original.name
        shutil.copytree(original, world)
        if _hash(world) != before or _hash(original) != before:
            raise ValueError("source changed while copying; select a closed save")
        _check_closed(original)
        h._make_profile_tree_owner_writable(world)
        h.apply_fixture_save_transforms(world, transforms)
        player_omt, player_ms = h.load_player_abs_omt(world, player_save)
        locations, resolve = _locations(recipe.get("locations", {}), player_omt)
        geometry = _edit_geometry(world, recipe.get("terrain", []), recipe.get("specials", []), resolve)
        items = _items(world, recipe.get("items", []), player_save)
        required = _requirements(world, player_save, recipe.get("requires", []), resolve)
        result = {
            "schema": "caol-world-preparation-v1", "source": str(source), "source_sha256": before,
            "world": world.name, "world_dir": str(dest / "save" / world.name), "player_save": player_save,
            "world_sha256": _hash(world), "recipe": recipe, "player_location_ms": player_ms,
            "locations": {k: list(v) for k, v in locations.items()},
            "geometry": geometry, "items": items, "requirements": required,
            "requirements_met": all(r["status"] == "met" for r in required),
        }
        (stage / "preparation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        if os.name == "nt":
            stage.rename(dest)  # Windows never replaces an existing directory with rename.
        else:
            dest.mkdir()
            try:
                os.replace(stage, dest)
            except BaseException:
                dest.rmdir()
                raise
    return result

def verify_requirements(source: Path, preparation: dict, *, player_save: str | None = None) -> dict:
    """Read saved facts at the original recipe coordinates after native setup or play."""
    if preparation.get("schema") != "caol-world-preparation-v1":
        raise ValueError("expected a preparation.json from world prepare")
    original, _ = _fixture(Path(source).expanduser().resolve(strict=True))
    if original.name != preparation.get("world"):
        raise ValueError(f"preparation is for {preparation.get('world')}, selected {original.name}")
    from world_inspect import _player_file
    selected = _player_file(original, _component(player_save or preparation.get("player_save")))
    anchor = _point(preparation.get("player_location_ms"))
    locations, resolve = _locations(preparation["recipe"].get("locations", {}),
                                   (anchor[0] // 24, anchor[1] // 24, anchor[2]))
    if {name: list(point) for name, point in locations.items()} != preparation.get("locations"):
        raise ValueError("preparation location bindings disagree with the saved recipe")
    rows = preparation["recipe"].get("requires", [])
    _check_closed(original)
    # Native archive readers extract into the world. Stage only the relevant
    # payloads, and keep the original map-square anchor even if the avatar moved.
    with tempfile.TemporaryDirectory(prefix="world-recheck-") as temp:
        staged = Path(temp) / original.name
        (staged / "maps").mkdir(parents=True)
        files = {Path("worldoptions.json"), Path(selected)}
        if any("staffed" in row for row in rows):
            files.add(Path("dimension_data.gsav"))
        for row in rows:
            if "item" in row or "field" in row:
                x, y, z = (a + b for a, b in zip(anchor, _point(row.get("offset_ms", [0, 0, 0]))))
                pack = f"{x // 24 // 32}.{y // 24 // 32}.{z}"
                plain = Path("maps") / pack / f"{x // 24}.{y // 24}.{z}.map"
                if (original / plain).exists():
                    files.add(plain)
                else:
                    files.update({Path("maps.dict"), Path("maps") / (pack + ".zzip")})

        def identity(relative):
            try:
                info = (original / relative).stat()
            except FileNotFoundError:
                return None
            return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns

        identities = {relative: identity(relative) for relative in files}
        for relative in files:
            src = original / relative
            if identities[relative] is not None:
                (staged / relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, staged / relative)
        if any(identity(path) != previous for path, previous in identities.items()):
            raise ValueError("saved inputs changed during requirement inspection; select a closed save")
        _, current = h.load_player_abs_omt(staged, selected)
        shift = tuple(a - b for a, b in zip(anchor, current))
        required = _requirements(staged, selected, rows, resolve, origin_shift=shift)
    _check_closed(original)
    return {"requirements": required, "requirements_met": all(r["status"] == "met" for r in required)}
