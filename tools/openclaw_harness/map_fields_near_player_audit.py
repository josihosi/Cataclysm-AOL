#!/usr/bin/env python3
"""Small save audit for map fields/items/furniture near the saved player.

This is intentionally narrow for harness proof packets: it prints only tiles in a
small radius (or exact offsets) that have fields/items/furniture, plus player
coordinates. It is read-only apart from temporary zzip extraction cleanup.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_harness as harness  # noqa: E402


def iter_triples(value: Any) -> Iterable[Tuple[int, int, Any]]:
    if not isinstance(value, list):
        return
    for entry in value:
        if isinstance(entry, list) and len(entry) >= 3:
            try:
                yield int(entry[0]), int(entry[1]), entry[2]
            except (TypeError, ValueError):
                continue
    for index in range(0, len(value) - 2, 3):
        try:
            yield int(value[index]), int(value[index + 1]), value[index + 2]
        except (TypeError, ValueError):
            continue


def summarize_field(payload: Any) -> Dict[str, Any]:
    if isinstance(payload, list) and payload:
        return {
            "field_id": payload[0],
            "intensity": payload[1] if len(payload) > 1 else None,
            "age_turns": payload[2] if len(payload) > 2 else None,
        }
    return {"raw": payload}


def item_typeid(item: Any) -> str:
    if isinstance(item, dict):
        return str(item.get("typeid", ""))
    return str(item)


def summarize_items(payload: Any) -> List[str]:
    if not isinstance(payload, list):
        return [item_typeid(payload)] if payload else []
    return [typeid for typeid in (item_typeid(item) for item in payload) if typeid]


def parse_offsets(raw_offsets: Sequence[str]) -> Optional[List[Tuple[int, int, int]]]:
    if not raw_offsets:
        return None
    if len(raw_offsets) % 3 != 0:
        raise SystemExit("--offset values must be repeated triples: dx dy dz")
    offsets: List[Tuple[int, int, int]] = []
    for index in range(0, len(raw_offsets), 3):
        offsets.append((int(raw_offsets[index]), int(raw_offsets[index + 1]), int(raw_offsets[index + 2])))
    return offsets


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("world_dir", type=Path)
    parser.add_argument("--player-save", default="", help="Player .sav.zzip filename; defaults to the only save in the world")
    parser.add_argument("--radius", type=int, default=2)
    parser.add_argument("--offset", nargs="*", default=[], help="Optional repeated dx dy dz triples; overrides radius scan")
    parser.add_argument("--field-id", action="append", default=[], help="Optional field ids to require in summary status")
    args = parser.parse_args()

    world_dir = args.world_dir
    if not world_dir.exists():
        raise SystemExit(f"World dir not found: {world_dir}")
    player_save = args.player_save.strip()
    if not player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise SystemExit(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        player_save = saves[0]

    player_abs_omt, player_location = harness.load_player_abs_omt(world_dir, player_save)
    maps_dir = world_dir / "maps"
    offsets = parse_offsets(args.offset)
    if offsets is None:
        radius = max(0, int(args.radius))
        offsets = [(dx, dy, 0) for dy in range(-radius, radius + 1) for dx in range(-radius, radius + 1)]

    extracted_packs: set[str] = set()
    preexisting_packs: set[str] = {path.name for path in maps_dir.iterdir() if path.is_dir()} if maps_dir.exists() else set()
    tile_reports: List[Dict[str, Any]] = []

    try:
        for dx, dy, dz in offsets:
            target = [int(player_location[0]) + dx, int(player_location[1]) + dy, int(player_location[2]) + dz]
            abs_sm = (target[0] // 12, target[1] // 12, target[2])
            abs_omt = (target[0] // 24, target[1] // 24, target[2])
            local = (target[0] - abs_sm[0] * 12, target[1] - abs_sm[1] * 12, target[2])
            pack_stem = f"{abs_omt[0] // 32}.{abs_omt[1] // 32}.{abs_omt[2]}"
            pack_dir = maps_dir / pack_stem
            if not pack_dir.exists():
                pack_zzip = maps_dir / f"{pack_stem}.zzip"
                if not pack_zzip.exists():
                    continue
                harness.run_zzip(pack_zzip)
                extracted_packs.add(pack_stem)
            map_path = pack_dir / f"{abs_omt[0]}.{abs_omt[1]}.{abs_omt[2]}.map"
            if not map_path.exists():
                continue
            map_payload = json.loads(map_path.read_text(encoding="utf-8"))
            if not isinstance(map_payload, list):
                continue
            submap = next(
                (
                    sub
                    for sub in map_payload
                    if isinstance(sub, dict) and sub.get("coordinates") == [abs_sm[0], abs_sm[1], abs_sm[2]]
                ),
                None,
            )
            if not isinstance(submap, dict):
                continue
            fields = [summarize_field(payload) for x, y, payload in iter_triples(submap.get("fields")) if x == local[0] and y == local[1]]
            items: List[str] = []
            for x, y, payload in iter_triples(submap.get("items")):
                if x == local[0] and y == local[1]:
                    items.extend(summarize_items(payload))
            furniture = [payload for x, y, payload in iter_triples(submap.get("furniture")) if x == local[0] and y == local[1]]
            if fields or items or furniture:
                tile_reports.append({
                    "offset_ms": [dx, dy, dz],
                    "target_location_ms": target,
                    "target_abs_omt": list(abs_omt),
                    "target_abs_sm": list(abs_sm),
                    "local_ms": [local[0], local[1], local[2]],
                    "fields": fields,
                    "items": items,
                    "furniture": furniture,
                })
    finally:
        for stem in sorted(extracted_packs):
            if stem not in preexisting_packs:
                pack_dir = maps_dir / stem
                if pack_dir.exists():
                    shutil.rmtree(pack_dir)

    required_fields = [field for field in args.field_id if field]
    observed_field_ids = [str(field.get("field_id")) for tile in tile_reports for field in tile.get("fields", []) if isinstance(field, dict)]
    missing_required_fields = [field for field in required_fields if field not in observed_field_ids]
    result = {
        "world": world_dir.name,
        "player_save": player_save,
        "player_location_ms": player_location,
        "player_abs_omt": list(player_abs_omt),
        "scanned_offsets": len(offsets),
        "required_fields": required_fields,
        "observed_field_ids": observed_field_ids,
        "missing_required_fields": missing_required_fields,
        "tiles": tile_reports,
        "status": "required_fields_present" if required_fields and not missing_required_fields else ("required_fields_missing" if required_fields else "scanned"),
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if not missing_required_fields else 2


if __name__ == "__main__":
    raise SystemExit(main())
