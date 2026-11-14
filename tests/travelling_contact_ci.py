#!/usr/bin/env python3
"""Run connected travelling controls and repaired fixture regressions."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile


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
    "native casualty commits refresh the survivor and retire only the dead claim",
    "hostile_camp_shared_cleanup_records_off_route_scout_death_before_erasure",
    "robbery visit fixture uses a valid clock after a retained world",
    "bandit_structural_outing_planner_selects_forest_and_town_jobs",
    "actual watch motor retains only its unconsumed pair order",
    "alarm fixture isolates persisted route threats and restores their custody",
    "actual dispatched faction party receives attack alert and retains native flight",
    "authenticated scouts disengage homeward without dismissing a current alert",
    "assessment return transfers preserve the deadline clock and real physical progress",
    "bandit_road_opportunity_is_physically_scouted_without_fabricating_bounty",
    "bandit_structural_outing_arrival_is_once_only_after_reload",
    "bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives",
    "R8 saved abstract return leaves its intermediate waypoint on the next motor cadence",
    "active structural scouts separate coarse hostile optics from real danger",
)
PASS = re.compile(r"^All tests passed \(([1-9][0-9]*) assertions? in 1 test case\)$", re.MULTILINE)


def run_controls(binary: Path) -> int:
    binary = binary.resolve()
    if not binary.is_file():
        print(f"Test executable not found: {binary}", file=sys.stderr)
        return 1
    failed = False
    for selection in SELECTIONS:
        with tempfile.TemporaryDirectory(prefix="caol-travelling-ci-") as user_dir:
            try:
                result = subprocess.run(
                    [str(binary), selection, "--rng-seed", "33491075", "--use-colour", "no",
                     "--user-dir", user_dir], stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, errors="replace", check=False, timeout=600,
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
    sys.exit(run_controls(parser.parse_args().binary))
