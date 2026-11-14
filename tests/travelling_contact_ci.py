#!/usr/bin/env python3
"""Run selected connected travelling, signal and ecology controls."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Optional


SELECTIONS = (
    "ordinary loaded road contact callbacks conserve the NPC action budget",
    "hostile search route work across paid blocked turns and opening",
    "hostile_camp_abstract_threat_observer_is_corridor_bounded_and_detours_only_to_withdraw",
    "hostile_camp_structural_observer_records_owned_evidence_before_returning_report",
    "bandit_live_world_persists_partial_scout_casualties_without_closing_the_partner",
    "hostile_camp_offscreen_overlap_avoids_threats_without_invented_casualties",
    "manageable overlap advances and later dangerous overlap withdraws",
    "corrupt abstract ownership refuses even a weak overlap",
    "legacy overlap bookkeeping does not consume an advancing route step",
    "legacy overlap reentry clears absence and permits danger escalation",
    "hostile_camp_abstract_encounter_episode_is_once_only_local_exclusive_and_reentrant",
    "hostile_camp_abstract_missing_outcomes_round_trip_and_close_at_their_deadlines",
    "retained physical scouts withdraw from genuine danger and report home once",
    "unicode world saves with unicode receipt tree entries",
    "quicksave_reports_confirmed_turn_and_recovers_after_failed_write",
    "native casualty commits refresh the survivor and retire only the dead claim",
    "hostile_camp_shared_cleanup_records_off_route_scout_death_before_erasure",
    "robbery visit fixture uses a valid clock after a retained world",
    "bandit_structural_outing_planner_selects_forest_and_town_jobs",
    "actual watch motor retains only its unconsumed pair order",
    "alarm fixture isolates persisted route threats and restores their custody",
    "actual dispatched faction party receives attack alert and retains native flight",
    "follow close limits target selection only for player allies",
    "authenticated scouts disengage homeward without dismissing a current alert",
    "homeward pair reaches its boundary through ordinary paid movement",
    "homeward boundary crossing awaits a physically incapacitated survivor",
    "assessment return transfers preserve the deadline clock and real physical progress",
    "bandit_road_opportunity_is_physically_scouted_without_fabricating_bounty",
    "bandit_structural_outing_arrival_is_once_only_after_reload",
    "bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives",
    "R8 saved abstract return leaves its intermediate waypoint on the next motor cadence",
    "active structural scouts separate coarse hostile optics from real danger",
    "fire_signal_stock_weather_calibration",
    "fire_signal_range_keeps_fraction_and_combines_weather",
    "fractional fire range pays exact terrain budget",
    "production_smoke_signal_records_staffed_camp_leads_once_and_keeps_failures",
    "production fire signals follow clock and weather without stale samples",
    "firelight escapes through a distant transparent opening",
    "fog permits nearby but not distant zombie fire investigation",
    "production rider light clusters respect walls and source escape",
    "legacy firelight sample survives save reload and co-located lamp",
    "rider fire clusters apply baseline bonus before weather caps",
    "ecology_view_reads_migo_leases_without_mutating_them",
    "ecology overmap facts follow gated canonical migo projection",
    "native ecology no-op actions publish handled successor state",
    "semantic World exposes only registered native vehicle controls",
)
PASS = re.compile(r"^All tests passed \(([1-9][0-9]*) assertions? in 1 test case\)$", re.MULTILINE)
CURSES_ONLY_NOOP = "native ecology no-op actions publish handled successor state"
TILES_MARKER = "drain_planner_epoch_pack_unpack_roundtrips"


def renderer_matches(binary: Path, tiles: bool) -> bool:
    """Check the actual binary's mutually exclusive compiled test markers."""
    markers = f"{CURSES_ONLY_NOOP},{TILES_MARKER}"
    try:
        with tempfile.TemporaryDirectory(prefix="caol-renderer-ci-") as user_dir:
            result = subprocess.run(
                [str(binary), markers, "--list-test-names-only", "--user-dir", user_dir],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                errors="replace", check=False, timeout=30,
            )
    except subprocess.TimeoutExpired:
        print("Renderer capability query timed out", file=sys.stderr)
        return False
    expected = TILES_MARKER if tiles else CURSES_ONLY_NOOP
    observed = {line.strip() for line in result.stdout.splitlines()} & {CURSES_ONLY_NOOP, TILES_MARKER}
    # Catch's list-names exit status is the number of matching test cases.
    if result.returncode != 1 or observed != {expected}:
        print(f"Renderer capability mismatch: expected {expected!r}, observed {sorted(observed)!r}, "
              f"list exit {result.returncode}", file=sys.stderr)
        return False
    return True


def curses_test_environment() -> Optional[dict[str, str]]:
    """Keep a usable TERM or select the installed xterm entry for headless Linux."""
    env = os.environ.copy()
    infocmp = shutil.which("infocmp")
    if not infocmp:
        print("Linux curses tests require infocmp and xterm terminfo", file=sys.stderr)
        return None

    def available(terminal: str) -> bool:
        try:
            result = subprocess.run([infocmp, terminal], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL, check=False, timeout=5, env=env)
            return result.returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            return False

    terminal = env.get("TERM", "")
    if not terminal or terminal == "unknown" or not available(terminal):
        if not available("xterm"):
            print("Linux curses tests require infocmp and xterm terminfo", file=sys.stderr)
            return None
        env["TERM"] = "xterm"
    return env


def run_controls(binary: Path, tiles: bool) -> int:
    binary = binary.resolve()
    if not binary.is_file():
        print(f"Test executable not found: {binary}", file=sys.stderr)
        return 1
    if not renderer_matches(binary, tiles):
        return 1
    if tiles:
        print(f"Tiles build: {CURSES_ONLY_NOOP} is compiled only for curses", flush=True)
    failed = False
    for selection in SELECTIONS:
        if tiles and selection == CURSES_ONLY_NOOP:
            continue
        case_env = None
        if not tiles and sys.platform.startswith("linux") and selection == CURSES_ONLY_NOOP:
            case_env = curses_test_environment()
            if case_env is None:
                print(f"Travelling control terminal setup failed: {selection}", file=sys.stderr)
                failed = True
                continue
        with tempfile.TemporaryDirectory(prefix="caol-travelling-ci-") as user_dir:
            try:
                result = subprocess.run(
                    [str(binary), selection, "--rng-seed", "33491075", "--use-colour", "no",
                     "--user-dir", user_dir], stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors="replace", check=False, timeout=600,
                    env=case_env,
                )
            except subprocess.TimeoutExpired:
                print(f"Travelling control timed out: {selection}", file=sys.stderr, flush=True)
                failed = True
                continue
        print(f"Travelling control {selection}: exit {result.returncode}", flush=True)
        print(result.stdout, end="", flush=True)
        if result.returncode != 0 or not PASS.search(result.stdout):
            print(f"Travelling control failed or did not execute: {selection}", file=sys.stderr)
            failed = True
    return int(failed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="Path to the CI-built Catch executable")
    parser.add_argument("--tiles", required=True, type=int, choices=(0, 1),
                        help="Actual build renderer: 0 for curses, 1 for Tiles")
    args = parser.parse_args()
    sys.exit(run_controls(args.binary, bool(args.tiles)))
