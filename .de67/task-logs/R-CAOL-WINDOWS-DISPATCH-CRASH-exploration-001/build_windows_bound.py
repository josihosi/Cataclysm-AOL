"""Build the patched Windows working source into a separate native executable."""

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import types


TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[1]
ORIGINAL = ROOT / "build/windows-camp-20260924/cataclysm-tiles.exe"
EXPECTED_ORIGINAL_EXE = "4346cfefe14faab798f607130b2e4bfe233261c31bdcfdca7c904befb963f365"
EXPECTED_SOURCE = {
    "src/do_turn.cpp": "39b2c4b295e1d7324571b295c327c170efdc11bf15f15d455d54450abe2c2848",
    "src/bandit_live_world.h": "2dcfd2a916aef43a0c94a19fea3a15f9d8bd25fd76f711fff4d197e76351f306",
    "tests/bandit_live_world_test.cpp": "37d8463185d8b762841daabd817d4d09048d8e0b601990f3b6be25153331b76c",
}
OLD_OBJECT = "build/windows-camp-20260924/objwin/tiles/do_turn.o"
NEW_OBJECT = "build/windows-dispatch-crash-bound-20260924/do_turn.o"
NEW_EXE = "build/windows-dispatch-crash-bound-20260924/cataclysm-tiles.exe"
TOOLCHAIN = Path("C:/Users/josef/dev/msys64")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(path, expected):
    actual = digest(path)
    if actual != expected:
        raise RuntimeError(f"{path}: actual {actual}, expected {expected}")


def run(args, log_name):
    args[0] = str(TOOLCHAIN / "ucrt64/bin/g++.exe")
    with (TASK / log_name).open("w", encoding="utf-8") as log:
        log.write("ARGS: " + json.dumps(args) + "\n")
        log.flush()
        result = subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        log.write(f"EXIT: {result.returncode}\n")
    if result.returncode:
        raise RuntimeError(f"{log_name}: exit {result.returncode}")
    return args


def main():
    if ROOT.name != "Cataclysm-AOL":
        raise RuntimeError(f"unexpected workspace {ROOT}")
    os.environ["PATH"] = os.pathsep.join(
        [str(TOOLCHAIN / "ucrt64/bin"), str(TOOLCHAIN / "usr/bin"), os.environ["PATH"]]
    )
    verify(ORIGINAL, EXPECTED_ORIGINAL_EXE)
    for relative, expected in EXPECTED_SOURCE.items():
        verify(ROOT / relative, expected)

    compile_args = shlex.split((TASK / "original-do-turn-compile.txt").read_text().strip())
    compile_args[compile_args.index("objwin/tiles/do_turn.o")] = NEW_OBJECT
    run(compile_args, "bound-compile.log")

    link_args = shlex.split((TASK / "original-build-link.log").read_text().splitlines()[0])
    link_args[link_args.index(OLD_OBJECT)] = NEW_OBJECT
    link_args[link_args.index("build/windows-camp-20260924/cataclysm-tiles.exe")] = NEW_EXE
    run(link_args, "bound-link.log")

    verify(ORIGINAL, EXPECTED_ORIGINAL_EXE)
    for relative, expected in EXPECTED_SOURCE.items():
        verify(ROOT / relative, expected)

    # The Windows harness uses the POSIX resource module only conditionally.
    sys.modules.setdefault("resource", types.ModuleType("resource"))
    sys.path.insert(0, str(ROOT / "tools/openclaw_harness"))
    import startup_harness

    executable = (ROOT / NEW_EXE).resolve()
    source = startup_harness.product_source_binding()
    if not source.get("ok"):
        raise RuntimeError(f"product source binding failed: {source}")
    version = subprocess.run(
        [str(executable), "--version"], cwd=ROOT, capture_output=True, text=True
    )
    (TASK / "bound-version.log").write_text(
        version.stdout + version.stderr + f"\nEXIT: {version.returncode}\n", encoding="utf-8"
    )
    if version.returncode:
        raise RuntimeError(f"version command exit {version.returncode}")
    executable_sha = digest(executable)
    receipt = {
        "schema": startup_harness.PRODUCT_BUILD_RECEIPT_SCHEMA,
        "captured_head": startup_harness.current_head_short(),
        "executable_path": str(executable),
        "executable_sha256": executable_sha,
        "product_source_sha256": source["sha256"],
        "built_at": datetime.now(timezone.utc).isoformat(),
        "command": link_args,
        "version_command": [str(executable), "--version"],
        "logs": {
            "compile": str(TASK / "bound-compile.log"),
            "link": str(TASK / "bound-link.log"),
            "version": str(TASK / "bound-version.log"),
            "original_compile_command": str(TASK / "original-do-turn-compile.txt"),
            "original_link_command": str(TASK / "original-build-link.log"),
        },
        "build_configuration": {
            "renderer": "windows-ucrt64-sdl3-tiles",
            "replacement_object": NEW_OBJECT,
            "original_object": OLD_OBJECT,
            "original_executable_sha256": EXPECTED_ORIGINAL_EXE,
            "source_sha256": EXPECTED_SOURCE,
        },
        "log_dir": str(TASK),
    }
    serialized = json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    receipt_path = startup_harness.product_build_receipt_archive_path(
        executable, executable_sha, source["sha256"]
    )
    receipt_path.parent.mkdir(parents=True, exist_ok=True)
    with receipt_path.open("x", encoding="utf-8") as stream:
        stream.write(serialized)
    readiness = startup_harness.executable_source_readiness(executable)
    result = {
        "fixed_executable": str(executable),
        "fixed_executable_sha256": executable_sha,
        "original_executable_sha256": EXPECTED_ORIGINAL_EXE,
        "product_source_sha256": source["sha256"],
        "product_build_receipt": str(receipt_path),
        "readiness": readiness,
    }
    (TASK / "bound-build-manifest.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
