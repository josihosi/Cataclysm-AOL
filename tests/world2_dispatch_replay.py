#!/usr/bin/env python3
"""Replay saved World 2 dispatch and expedition updates with trace-free timing."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/openclaw_harness"))
from startup_harness import product_source_binding, sha256_file, sha256_tree


WORLD = ROOT / ".userdata/scratch-b77aa9650b904375/save/TestSetup00"
WORLD_HASH = "28e29932372d7af9cccb32d9087ca4131d95a47dc9f887f8647c5f5442052e6b"
CHARACTER = "#VGVzdDAw.sav.zzip"
CHARACTER_HASH = "f3e41e8d252ee999b4c7b93be5ea8d1e6ead4d63f7c645a92820b5e0ec17d21f"
SEED = "124"
RESULT_PREFIX = "WORLD2_DISPATCH_REPLAY "
IDENTITY = ("loaded_turn", "initial_after", "avatar", "activity", "local_actors", "home_bodies",
            "dispatch", "movement_profile")


def verify_world(path: Path) -> None:
    if sha256_tree(path) != (WORLD_HASH, "") or sha256_file(path / CHARACTER) != (CHARACTER_HASH, ""):
        raise RuntimeError(f"World 2 byte binding mismatch at {path}")


def run_replay(mode: str, directory: Path, executable: Path, binary_hash: str,
            evidence: Path, run_name: str | None = None) -> dict:
    name = run_name or mode
    profile = directory / name
    profile.mkdir()
    (profile / ".world2-dispatch-disposable").touch(exist_ok=False)
    world = profile / "save/TestSetup00"
    shutil.copytree(WORLD, world)
    verify_world(world)
    if sha256_file(executable) != (binary_hash, ""):
        raise RuntimeError("Frozen test executable changed")

    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("OPENCLAW_HARNESS_") and not key.startswith("CAOL_HOME_")}
    environment.update(CAOL_WORLD2_DISPATCH_USERDIR=str(profile), CAOL_WORLD2_DISPATCH_MODE=mode)
    stdout_path = evidence / f"world2-{name}.stdout.log"
    stderr_path = evidence / f"world2-{name}.stderr.log"
    with stdout_path.open("x") as stdout, stderr_path.open("x") as stderr:
        with subprocess.Popen([str(executable), "[world2_dispatch_replay]", "--rng-seed", SEED,
                               "--user-dir", str(profile), "--drop-world",
                               "--set-debug-level-mask", "8"],
                              cwd=ROOT, env=environment, stdout=stdout, stderr=stderr) as process:
            print(f"{name}: native test PID {process.pid}", flush=True)
            try:
                subprocess.run(["ps", "-p", str(process.pid), "-o", "pid,ppid,lstart,comm"],
                               check=False)
                status = process.wait(timeout=900)
            except BaseException:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                raise
    if status != 0:
        raise RuntimeError(f"{mode} failed, exit {status}; see {stdout_path} and {stderr_path}")
    if sha256_file(executable) != (binary_hash, ""):
        raise RuntimeError("Frozen test executable changed during execution")
    rows = [json.loads(line.removeprefix(RESULT_PREFIX)) for line in stdout_path.read_text().splitlines()
            if line.startswith(RESULT_PREFIX)]
    if len(rows) != 1 or rows[0].get("mode") != mode:
        raise RuntimeError(f"Missing native result for {mode}: {stdout_path}")
    shutil.rmtree(profile)
    return rows[0]


def require_same_work(reference: dict, candidate: dict) -> None:
    for field in IDENTITY:
        if reference[field] != candidate[field]:
            raise RuntimeError(f"Matched work diverged at {field}: see the native replay records")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--executable", type=Path, default=ROOT / "tests/cata_test")
    args = parser.parse_args()
    evidence = args.evidence.resolve(strict=True)
    verify_world(WORLD)
    source_before = product_source_binding()
    if not source_before.get("ok"):
        raise RuntimeError("Could not bind current dirty product source")
    try:
        with tempfile.TemporaryDirectory(prefix="world2-dispatch-", dir=ROOT / ".userdata") as scratch:
            directory = Path(scratch)
            executable = directory / "cata_test"
            shutil.copy2(args.executable, executable)
            executable.chmod(0o500)
            digest, error = sha256_file(executable)
            if error or sha256_file(args.executable) != (digest, ""):
                raise RuntimeError(f"Unable to freeze a stable test binary: {error}")
            qualified = run_replay("qualify", directory, executable, digest, evidence)
            with (evidence / "world2-dispatch-qualification.json").open("x") as out:
                json.dump(qualified, out, indent=2)
                out.write("\n")
            measured = {}
            for name, mode in (("turn", "turn"), ("turn_repeat", "turn"), ("body", "body")):
                row = run_replay(mode, directory, executable, digest, evidence, name)
                require_same_work(qualified, row)
                measured[name] = row
            if product_source_binding().get("sha256") != source_before["sha256"]:
                raise RuntimeError("Product source changed during the experiment")
            receipt = {"world_tree_sha256": WORLD_HASH, "character_sha256": CHARACTER_HASH,
                       "test_executable_sha256": digest, "product_source_sha256": source_before["sha256"],
                       "rng_seed": int(SEED),
                       "qualification": "world2-dispatch-qualification.json", "measured": measured}
            with (evidence / "world2-dispatch-result.json").open("x") as out:
                json.dump(receipt, out, indent=2)
                out.write("\n")
    finally:
        verify_world(WORLD)


if __name__ == "__main__":
    main()
