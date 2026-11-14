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
    "robbery visit fixture uses a valid clock after a retained world",
    "bandit_structural_outing_planner_selects_forest_and_town_jobs",
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
