"""World preparation and playtest commands shared by the startup harness."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def _source(value):
    if re.fullmatch(r"[A-Za-z0-9_.-]+:[A-Za-z0-9_.-]+", value):
        from startup_harness import profile_fixture_root
        profile, fixture = value.split(":")
        if profile in {".", ".."} or fixture in {".", ".."}:
            raise ValueError("Choose a fixture profile and name")
        return profile_fixture_root(profile) / fixture
    return Path(value)


def _recipe(args):
    recipe = json.loads(args.recipe.read_text(encoding="utf-8-sig")) if args.recipe else {}
    if not isinstance(recipe, dict):
        raise ValueError("Expected a JSON object in the preparation recipe")
    for index, (kind, dx, dy, dz) in enumerate(args.home):
        name = f"home_{index + 1}"
        locations = recipe.setdefault("locations", {})
        if name in locations:
            raise ValueError(f"Choose a different recipe location name than {name} when using --home")
        locations[name] = {"from": "player", "offset_omt": [int(dx), int(dy), int(dz)]}
        recipe.setdefault("specials", []).append({"id": kind, "at": name})
    for item, count in args.give:
        recipe.setdefault("items", []).append({"id": item, "count": int(count)})
    return recipe


def add_commands(commands):
    world = commands.add_parser("world", help="Inspect, prepare, launch and remember playtest worlds.")
    actions = world.add_subparsers(dest="world_command", required=True)
    inspect = actions.add_parser("inspect", help="Read saved terrain, home footprints and populations.")
    inspect.add_argument("source", type=_source, help="Closed world, materialized fixture, or prepared directory.")
    inspect.add_argument("--radius", type=int, choices=(20, 40), default=40, help="Radius in OMT (default 40).")
    inspect.add_argument("--zoom", dest="radius", action="store_const", const=20, help="Zoom to 20 OMT.")
    inspect.add_argument("--center", nargs=2, type=int, metavar=("X", "Y"), help="Absolute OMT center (default player).")
    inspect.add_argument("--z", type=int, default=0, help="Saved terrain layer (default ground).")
    inspect.add_argument("--player-save", help="Character filename for a world with more than one player.")
    inspect.add_argument("--check", type=Path, metavar="PREPARATION_JSON", help="Recheck the saved recipe conditions at their original positions.")
    inspect.add_argument("--map", action="store_true", help="Show all tiles and the local map boundary.")
    inspect.add_argument("--json", action="store_true", help="Machine-readable facts; --map adds exact terrain IDs.")
    prepare = actions.add_parser("prepare", help="Make a disposable world from a source and JSON recipe.")
    prepare.add_argument("source", type=_source, help="World directory or profile:fixture.")
    prepare.add_argument("recipe", type=Path, nargs="?", help="Optional JSON recipe file.")
    prepare.add_argument("destination", type=Path, help="A new directory for this preparation.")
    prepare.add_argument("--player-save")
    prepare.add_argument("--home", nargs=4, action="append", default=[], metavar=("SPECIAL", "DX", "DY", "DZ"),
                         help="Place a data-defined home at this offset from the player in OMT. Repeatable.")
    prepare.add_argument("--give", nargs=2, action="append", default=[], metavar=("ITEM", "COUNT"),
                         help="Put items at the player's feet. Repeatable.")
    scratch = actions.add_parser("scratch", help="Launch an ordinary disposable playtest with a compatible build.")
    scratch.add_argument("world", type=_source)
    scratch.add_argument("--player-save", help="Select the character in a raw multi-character world.")
    scratch.add_argument("--question", default="", help="What the player should test.")
    scratch.add_argument("--family", default="", help="Group the result under an existing gameplay question.")
    scratch.add_argument("--registry", type=Path, help="Scenario registry SQLite file.")
    build = scratch.add_mutually_exclusive_group()
    build.add_argument("--executable", type=Path)
    build.add_argument("--build-receipt", type=Path)
    scratch.add_argument("--dry-run", action="store_true", help="Check prerequisites without launching a session.")
    import world_examples
    world_examples.add_commands(actions)


def run(args):
    try:
        if args.world_command == "inspect":
            from world_inspect import inspect_world
            from world_view import render

            preparation = json.loads(args.check.read_text(encoding="utf-8-sig")) if args.check else {}
            if args.check and (not isinstance(preparation, dict) or preparation.get("schema") != "caol-world-preparation-v1"):
                raise ValueError("Expected a preparation.json from world prepare")
            player = args.player_save or preparation.get("player_save")
            result = inspect_world(args.source, player_save=player, radius=args.radius,
                                   center=args.center, z=args.z, include_map=args.map)
            if args.check:
                from world_prepare import verify_requirements
                result.update(verify_requirements(args.source, preparation, player_save=player))
            print(json.dumps(result, ensure_ascii=True) if args.json else render(result))
            return 2 if result.get("requirements_met") is False else 0
        if args.world_command == "prepare":
            from world_prepare import prepare_world

            recipe = _recipe(args)
            result = prepare_world(args.source, recipe, args.destination, player_save=args.player_save)
        elif args.world_command == "scratch":
            from world_scratch import launch_scratch

            result = launch_scratch(args.world, executable=args.executable, build_receipt=args.build_receipt,
                                    dry_run=args.dry_run, question=args.question, family=args.family, registry=args.registry,
                                    player_save=args.player_save)
        else:
            import world_examples
            return world_examples.run_command(args)
        print(json.dumps(result, indent=2, default=str, ensure_ascii=True))
        return 1 if result.get("status") in {"blocked", "failed", "error"} else 0
    except (ValueError, OSError, RuntimeError) as error:
        print(f"World {args.world_command}: {error}", file=sys.stderr)
        return 1
