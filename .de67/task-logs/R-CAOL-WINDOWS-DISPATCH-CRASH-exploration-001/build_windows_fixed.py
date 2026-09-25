"""Build a separate native Windows crash-fix executable from retained build commands."""

import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[1]
SOURCE = TASK / "source"
ORIGINAL = ROOT / "build" / "windows-camp-20260924" / "cataclysm-tiles.exe"
EXPECTED_ORIGINAL_EXE = "4346cfefe14faab798f607130b2e4bfe233261c31bdcfdca7c904befb963f365"
EXPECTED_OLD_SOURCE = {
    "do_turn.cpp": "28e0a92d59e6c93469a588d9643684d1b3e12a56d9a244a9610fa91c1ca0934b",
    "bandit_live_world.h": "25a97f9b74efa0944cb73ca6a7848e232f9e0b0052855c1816bbdda10d5263c3",
}
EXPECTED_NEW_SOURCE = {
    "do_turn.cpp": "6ba9977b0da0903ea52f5f5ccf53fd0a2d5df1de2a8c3944e22930c620a94940",
    "bandit_live_world.patch-reference.h": "2dcfd2a916aef43a0c94a19fea3a15f9d8bd25fd76f711fff4d197e76351f306",
}
OLD_OBJECT = "build/windows-camp-20260924/objwin/tiles/do_turn.o"
NEW_OBJECT = "build/windows-dispatch-crash-20260924/do_turn.o"
NEW_EXE = "build/windows-dispatch-crash-20260924/cataclysm-tiles.exe"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_hash(path, expected):
    actual = digest(path)
    if actual != expected:
        raise RuntimeError(f"unexpected hash for {path}: {actual}, expected {expected}")
    return actual


def run_command(args, log_name):
    env = dict(os.environ)
    tools = Path("C:/Users/josef/dev/msys64")
    env["PATH"] = os.pathsep.join(
        [str(tools / "ucrt64" / "bin"), str(tools / "usr" / "bin"), env["PATH"]]
    )
    args[0] = str(tools / "ucrt64" / "bin" / "g++.exe")
    with (TASK / log_name).open("w", encoding="utf-8") as log:
        log.write("ARGS: " + json.dumps(args) + "\n")
        log.flush()
        proc = subprocess.run(args, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        log.write(f"EXIT: {proc.returncode}\n")
    if proc.returncode:
        raise RuntimeError(f"{log_name} failed with exit {proc.returncode}")


def main():
    if ROOT.name != "Cataclysm-AOL":
        raise RuntimeError(f"unexpected workspace: {ROOT}")
    require_hash(ORIGINAL, EXPECTED_ORIGINAL_EXE)
    for name, expected in EXPECTED_OLD_SOURCE.items():
        require_hash(ROOT / "src" / name, expected)
    for name, expected in EXPECTED_NEW_SOURCE.items():
        require_hash(SOURCE / name, expected)

    compile_line = (TASK / "original-do-turn-compile.txt").read_text().strip()
    compile_args = shlex.split(compile_line, posix=True)
    compile_args[compile_args.index("objwin/tiles/do_turn.o")] = NEW_OBJECT
    compile_args[compile_args.index("src/do_turn.cpp")] = (
        "build/windows-dispatch-crash-20260924/source/do_turn.cpp"
    )
    compile_args.insert(1, "-Ibuild/windows-dispatch-crash-20260924/source")
    run_command(compile_args, "fixed-compile.log")

    link_lines = (TASK / "original-build-link.log").read_text().splitlines()
    link_args = shlex.split(link_lines[0], posix=True)
    link_args[link_args.index(OLD_OBJECT)] = NEW_OBJECT
    link_args[link_args.index("build/windows-camp-20260924/cataclysm-tiles.exe")] = NEW_EXE
    run_command(link_args, "fixed-link.log")
    require_hash(ORIGINAL, EXPECTED_ORIGINAL_EXE)
    record = {
        "original_executable": str(ORIGINAL),
        "original_executable_sha256": EXPECTED_ORIGINAL_EXE,
        "fixed_executable": str(ROOT / NEW_EXE),
        "fixed_executable_sha256": digest(ROOT / NEW_EXE),
        "source_sha256": EXPECTED_NEW_SOURCE,
        "mac_do_turn_source_sha256": "39b2c4b295e1d7324571b295c327c170efdc11bf15f15d455d54450abe2c2848",
        "source_difference_from_mac": "one forward declaration for the isolated Windows object build; original Windows header is used by the preexisting include graph",
        "original_source_sha256": EXPECTED_OLD_SOURCE,
        "original_link_command": str(TASK / "original-build-link.log"),
        "original_compile_command": str(TASK / "original-do-turn-compile.txt"),
        "replaced_object": OLD_OBJECT,
        "replacement_object": NEW_OBJECT,
        "original_executable_preserved": True,
    }
    (TASK / "fixed-build-manifest.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"BUILD FAILED: {exc}", file=sys.stderr)
        raise
