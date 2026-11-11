#!/usr/bin/env python3
"""Run the connected zone identity and Zone Manager controls in CI."""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile


SELECTIONS = (("[zones][identity]", 11), ("[semantic_surface][zone_cursor]", 6))
TIMEOUT_SECONDS = 600
PASS = re.compile(r"^All tests passed \(([1-9][0-9]*) assertions? in ([1-9][0-9]*) test cases?\)$", re.MULTILINE)


def run_controls(binary: Path) -> int:
    binary = binary.resolve()
    if not binary.is_file():
        print(f"Test executable not found: {binary}", file=sys.stderr)
        return 1
    failed = False
    for selection, minimum_cases in SELECTIONS:
        with tempfile.TemporaryDirectory(prefix="caol-zone-ci-") as user_dir:
            try:
                result = subprocess.run(
                    [str(binary), selection, "--rng-seed", "67435", "--use-colour", "no",
                     "--user-dir", user_dir],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, errors="replace", check=False, timeout=TIMEOUT_SECONDS,
                )
            except subprocess.TimeoutExpired:
                print(f"Zone control {selection} timed out after {TIMEOUT_SECONDS} seconds",
                      file=sys.stderr, flush=True)
                failed = True
                continue
        print(f"Zone control {selection}: exit {result.returncode}", flush=True)
        print(result.stdout, end="", flush=True)
        match = PASS.search(result.stdout)
        if result.returncode != 0 or not match or int(match.group(2)) < minimum_cases:
            print(f"Zone control {selection} failed or did not execute at least "
                  f"{minimum_cases} cases", file=sys.stderr)
            failed = True
    return int(failed)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="Path to the CI-built Catch executable")
    sys.exit(run_controls(parser.parse_args().binary))
