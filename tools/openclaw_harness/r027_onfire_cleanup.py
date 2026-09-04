#!/usr/bin/env python3
"""Validate the zero-credit R-027 onfire-cleanup snapshots."""

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


def canonical_sha256(value: Any) -> str:
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def effect_map(snapshot: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {effect["id"]: effect for effect in snapshot["avatar"]["effects"]}


def verify(before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    before_effects = effect_map(before)
    after_effects = effect_map(after)
    protected_before = canonical_sha256(before["bandit_live_world"])
    protected_after = canonical_sha256(after["bandit_live_world"])
    invariant_keys = (
        ("avatar", "abs_ms"),
        ("avatar", "abs_omt"),
        ("avatar", "fire_intensity"),
        ("avatar", "body_parts"),
        ("nearby_entities",),
        ("damaging_fields",),
        ("saved_source_south_of_avatar",),
        ("fixed_saved_source",),
    )
    unchanged = all(
        (before[key[0]][key[1]] if len(key) == 2 else before[key[0]]) ==
        (after[key[0]][key[1]] if len(key) == 2 else after[key[0]])
        for key in invariant_keys
    )
    removed_only_onfire = (
        "onfire" in before_effects and "onfire" not in after_effects and
        {key: value for key, value in before_effects.items() if key != "onfire"} == after_effects
    )
    accepted = unchanged and protected_before == protected_after and removed_only_onfire
    return {
        "schema": "caol-r027-onfire-cleanup-verdict-v1",
        "accepted": accepted,
        "onfire_before": before_effects.get("onfire"),
        "onfire_after": after_effects.get("onfire"),
        "protected_camp_sha256_before": protected_before,
        "protected_camp_sha256_after": protected_after,
        "invariants_unchanged": unchanged,
        "only_onfire_removed": removed_only_onfire,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    before = json.loads(args.before.read_text(encoding="utf-8"))
    after = json.loads(args.after.read_text(encoding="utf-8"))
    verdict = verify(before, after)
    args.output.write_text(json.dumps(verdict, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0 if verdict["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
