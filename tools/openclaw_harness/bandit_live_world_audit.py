#!/usr/bin/env python3
"""Freeze the current live-world bandit ownership surface into one readable report.

This stays deliberately narrow:
- read bandit-style overmap-special and map-extra families from repo data
- inspect one save's overmap placement data for live bandit special placements
- print one ownership-grain suggestion so later saveable-owner work stops guessing
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


RELEVANT_SPECIAL_IDS = {
    "bandit_cabin",
    "bandit_camp",
    "bandit_drug_lab",
    "bandit_work_camp",
    "bandit_garage",
    "o_container_yard_inland_bandit",
}
RELEVANT_EXTRA_IDS = {"mx_bandits_block"}


@dataclass
class SpecialDefinition:
    special_id: str
    all_points: list[tuple[int, int, int]]
    all_overmaps: list[str]
    ground_points: list[tuple[int, int, int]]
    ground_overmaps: list[str]


@dataclass
class SpecialPlacement:
    overmap_file: str
    special_id: str
    site_index: int
    points: list[tuple[int, int, int]]


@dataclass
class MapExtraDefinition:
    extra_id: str
    generator_id: str
    min_max_zlevel: list[int]
    autonote: bool
    flags: list[str]


def repo_root_from_script(script_path: Path) -> Path:
    return script_path.resolve().parents[2]


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def is_relevant_special(entry: dict[str, Any]) -> bool:
    special_id = str(entry.get("id", ""))
    if special_id in RELEVANT_SPECIAL_IDS:
        return True
    overmaps = entry.get("overmaps", [])
    if isinstance(overmaps, list):
        for overmap_entry in overmaps:
            if not isinstance(overmap_entry, dict):
                continue
            overmap_id = str(overmap_entry.get("overmap", ""))
            if "bandit" in overmap_id:
                return True
    return False


def is_relevant_extra(entry: dict[str, Any]) -> bool:
    extra_id = str(entry.get("id", ""))
    generator = entry.get("generator", {})
    generator_id = str(generator.get("generator_id", "")) if isinstance(generator, dict) else ""
    return extra_id in RELEVANT_EXTRA_IDS or "bandit" in extra_id or "bandit" in generator_id


def load_special_definitions(repo_root: Path) -> dict[str, SpecialDefinition]:
    specials_path = repo_root / "data/json/overmap/overmap_special/specials.json"
    raw = load_json(specials_path)
    result: dict[str, SpecialDefinition] = {}
    for entry in raw:
        if not isinstance(entry, dict) or entry.get("type") != "overmap_special":
            continue
        if not is_relevant_special(entry):
            continue
        special_id = str(entry["id"])
        all_points: list[tuple[int, int, int]] = []
        all_overmaps: list[str] = []
        ground_points: list[tuple[int, int, int]] = []
        ground_overmaps: list[str] = []
        for overmap_entry in entry.get("overmaps", []):
            if not isinstance(overmap_entry, dict):
                continue
            point_raw = overmap_entry.get("point", [0, 0, 0])
            if not isinstance(point_raw, list) or len(point_raw) != 3:
                continue
            point = (int(point_raw[0]), int(point_raw[1]), int(point_raw[2]))
            overmap_id = str(overmap_entry.get("overmap", ""))
            all_points.append(point)
            if overmap_id:
                all_overmaps.append(overmap_id)
            if point[2] <= 0:
                ground_points.append(point)
                if overmap_id:
                    ground_overmaps.append(overmap_id)
        result[special_id] = SpecialDefinition(
            special_id=special_id,
            all_points=all_points,
            all_overmaps=all_overmaps,
            ground_points=ground_points,
            ground_overmaps=ground_overmaps,
        )
    return result


def load_map_extra_definitions(repo_root: Path) -> dict[str, MapExtraDefinition]:
    extras_path = repo_root / "data/json/overmap/map_extras.json"
    raw = load_json(extras_path)
    result: dict[str, MapExtraDefinition] = {}
    for entry in raw:
        if not isinstance(entry, dict) or entry.get("type") != "map_extra":
            continue
        if not is_relevant_extra(entry):
            continue
        generator = entry.get("generator", {})
        generator_id = str(generator.get("generator_id", "")) if isinstance(generator, dict) else ""
        min_max_zlevel_raw = entry.get("min_max_zlevel", [0, 0])
        min_max_zlevel = [int(value) for value in min_max_zlevel_raw[:2]] if isinstance(min_max_zlevel_raw, list) else [0, 0]
        flags_raw = entry.get("flags", [])
        flags = [str(flag) for flag in flags_raw] if isinstance(flags_raw, list) else []
        extra_id = str(entry.get("id", ""))
        result[extra_id] = MapExtraDefinition(
            extra_id=extra_id,
            generator_id=generator_id,
            min_max_zlevel=min_max_zlevel,
            autonote=bool(entry.get("autonote", False)),
            flags=flags,
        )
    return result


def extract_json_array_after_key(text: str, key: str) -> list[Any]:
    key_index = text.find(key)
    if key_index < 0:
        return []
    start = text.find("[", key_index)
    if start < 0:
        return []
    depth = 0
    in_string = False
    escape = False
    for index in range(start, len(text)):
        char = text[index]
        if in_string:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            continue
        if char == "[":
            depth += 1
            continue
        if char == "]":
            depth -= 1
            if depth == 0:
                return json.loads(text[start:index + 1])
    raise ValueError(f"Unclosed JSON array for key {key!r}")


def load_text_from_overmap_file(repo_root: Path, path: Path) -> str:
    if path.suffix != ".zzip":
        return path.read_text(encoding="utf-8")

    zzip_binary = repo_root / "zzip"
    if not zzip_binary.exists():
        raise FileNotFoundError(f"Needed zzip helper at {zzip_binary}")

    candidate_plain_paths = [path.with_suffix("")]
    if path.parent.name == "overmaps":
        candidate_plain_paths.append(path.parent.parent / path.stem)

    plain_path = next((candidate for candidate in candidate_plain_paths if candidate.exists()), None)
    created_plain = False
    if plain_path is None:
        subprocess.run(
            [str(zzip_binary), str(path)],
            cwd=path.parent,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        plain_path = next((candidate for candidate in candidate_plain_paths if candidate.exists()), None)
        created_plain = True

    if plain_path is None:
        raise FileNotFoundError(f"zzip did not create a plain overmap for {path}")

    try:
        return plain_path.read_text(encoding="utf-8")
    finally:
        if created_plain and plain_path.exists():
            plain_path.unlink()


def iter_overmap_sources(save_dir: Path) -> list[Path]:
    overmaps_dir = save_dir / "overmaps"
    if not overmaps_dir.is_dir():
        return []
    paths = [path for path in overmaps_dir.iterdir() if path.is_file() and (path.suffix == ".zzip" or path.name.startswith("o."))]
    return sorted(paths)


def load_special_placements(repo_root: Path, save_dir: Path, relevant_special_ids: set[str]) -> list[SpecialPlacement]:
    placements: list[SpecialPlacement] = []
    seen_plain_names: set[str] = set()
    for path in iter_overmap_sources(save_dir):
        if path.suffix != ".zzip":
            seen_plain_names.add(path.name)
            if f"{path.name}.zzip" in {candidate.name for candidate in iter_overmap_sources(save_dir)}:
                continue
        text = load_text_from_overmap_file(repo_root, path)
        raw_placements = extract_json_array_after_key(text, '"overmap_special_placements":')
        for entry in raw_placements:
            if not isinstance(entry, dict):
                continue
            special_id = str(entry.get("special", ""))
            if special_id not in relevant_special_ids:
                continue
            site_groups = entry.get("placements", [])
            if not isinstance(site_groups, list):
                continue
            for site_index, site_group in enumerate(site_groups, start=1):
                if not isinstance(site_group, dict):
                    continue
                points_raw = site_group.get("points", [])
                points: list[tuple[int, int, int]] = []
                if isinstance(points_raw, list):
                    for point_entry in points_raw:
                        if not isinstance(point_entry, dict):
                            continue
                        point_raw = point_entry.get("p", [])
                        if not isinstance(point_raw, list) or len(point_raw) != 3:
                            continue
                        points.append((int(point_raw[0]), int(point_raw[1]), int(point_raw[2])))
                placements.append(
                    SpecialPlacement(
                        overmap_file=path.name,
                        special_id=special_id,
                        site_index=site_index,
                        points=points,
                    )
                )
    placements.sort(key=lambda item: (item.special_id, item.overmap_file, item.site_index))
    return placements


def bounding_box(points: list[tuple[int, int, int]]) -> str:
    if not points:
        return "<none>"
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    zs = [point[2] for point in points]
    return f"x[{min(xs)}..{max(xs)}] y[{min(ys)}..{max(ys)}] z[{min(zs)}..{max(zs)}]"


def ground_points(points: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    return [point for point in points if point[2] <= 0]


def format_special_section(specials: dict[str, SpecialDefinition]) -> str:
    lines = ["Data-defined bandit overmap-special families:"]
    for special_id in sorted(specials):
        definition = specials[special_id]
        lines.append(
            f"- {special_id}: {len(definition.ground_points)} ground omt / {len(definition.all_points)} total omt"
        )
        if definition.ground_overmaps:
            lines.append(f"  ground terrains: {', '.join(definition.ground_overmaps)}")
    return "\n".join(lines)


def format_extra_section(extras: dict[str, MapExtraDefinition]) -> str:
    lines = ["Data-defined bandit map-extra families:"]
    for extra_id in sorted(extras):
        definition = extras[extra_id]
        lines.append(
            f"- {extra_id}: generator={definition.generator_id}, z={definition.min_max_zlevel[0]}..{definition.min_max_zlevel[1]}, autonote={str(definition.autonote).lower()}"
        )
    return "\n".join(lines)


def format_live_section(placements: list[SpecialPlacement]) -> str:
    lines = ["Live save overmap-special placements:"]
    if not placements:
        lines.append("- none found in inspected overmaps")
        return "\n".join(lines)
    for placement in placements:
        live_ground_points = ground_points(placement.points)
        lines.append(
            f"- {placement.special_id} site {placement.site_index} in {placement.overmap_file}: "
            f"{len(live_ground_points)} ground omt / {len(placement.points)} total omt, {bounding_box(placement.points)}"
        )
        if live_ground_points:
            lines.append(
                "  ground points: "
                + ", ".join(f"({x},{y},{z})" for x, y, z in live_ground_points)
            )
    return "\n".join(lines)


def ownership_grain_lines(specials: dict[str, SpecialDefinition], extras: dict[str, MapExtraDefinition]) -> str:
    lines = [
        "Suggested owner grain for the next saveable-owner slice:",
        "- one owner record per live bandit spawn family origin, not one loose blob for the whole world",
        "- overmap-special families use one owner per special placement; the owner key can be `special_id + anchor_omt`, with ground omt children as spawn tiles",
        "- map-extra families use one owner per map-extra origin omt; the same owner shape can still carry one or more child spawn tiles after local generation",
        "- roofs stay metadata on the owner, not separate owners; later headcount/writeback should live on ground omt children",
        f"- current repo families to unify under one owner schema: {', '.join(sorted(set(specials) | set(extras)))}",
    ]
    return "\n".join(lines)


def build_report(repo_root: Path, save_dir: Path) -> str:
    specials = load_special_definitions(repo_root)
    extras = load_map_extra_definitions(repo_root)
    placements = load_special_placements(repo_root, save_dir, set(specials))
    sections = [
        f"Bandit live-world ownership audit for save: {save_dir}",
        "",
        format_special_section(specials),
        "",
        format_extra_section(extras),
        "",
        format_live_section(placements),
        "",
        ownership_grain_lines(specials, extras),
    ]
    return "\n".join(sections)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save_dir", type=Path, help="Path to a world save dir, e.g. save/McWilliams")
    parser.add_argument("--repo-root", type=Path, default=None, help="Repo root (defaults from script location)")
    args = parser.parse_args(argv)

    repo_root = args.repo_root.resolve() if args.repo_root else repo_root_from_script(Path(__file__))
    save_dir = args.save_dir.resolve()

    if not save_dir.is_dir():
        raise SystemExit(f"Save dir not found: {save_dir}")

    print(build_report(repo_root, save_dir))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
