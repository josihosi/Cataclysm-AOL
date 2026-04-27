#!/usr/bin/env python3
"""Small save audit for map fields/items/furniture near the saved player.

This is intentionally narrow for harness proof packets: it prints only tiles in a
small radius (or exact offsets) that have fields/items/furniture, plus player
coordinates. It is read-only apart from temporary zzip extraction cleanup.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Sequence, Tuple, List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
import startup_harness as harness  # noqa: E402


def parse_offsets(raw_offsets: Sequence[str]) -> Optional[List[Tuple[int, int, int]]]:
    if not raw_offsets:
        return None
    return harness.parse_map_tile_offsets(raw_offsets)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("world_dir", type=Path)
    parser.add_argument("--player-save", default="", help="Player .sav.zzip filename; defaults to the only save in the world")
    parser.add_argument("--radius", type=int, default=2)
    parser.add_argument("--offset", nargs="*", default=[], help="Optional repeated dx dy dz triples; overrides radius scan")
    parser.add_argument("--field-id", action="append", default=[], help="Optional field ids to require in summary status")
    parser.add_argument("--item-id", action="append", default=[], help="Optional item ids/typeids to require in summary status")
    parser.add_argument("--furniture-id", action="append", default=[], help="Optional furniture ids to require in summary status")
    args = parser.parse_args()

    result = harness.audit_map_tiles_near_player(
        args.world_dir,
        player_save=args.player_save.strip(),
        radius=args.radius,
        offsets=parse_offsets(args.offset),
        required_fields=[field for field in args.field_id if field],
        required_items=[item for item in args.item_id if item],
        required_furniture=[furn for furn in args.furniture_id if furn],
    )
    legacy_status = result.get("status")
    if legacy_status == "required_state_present" and result.get("required_fields") and not result.get("required_items") and not result.get("required_furniture"):
        result["status"] = "required_fields_present"
    elif legacy_status == "required_state_missing" and result.get("required_fields") and not result.get("required_items") and not result.get("required_furniture"):
        result["status"] = "required_fields_missing"
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if legacy_status != "required_state_missing" else 2


if __name__ == "__main__":
    raise SystemExit(main())
