#!/usr/bin/env python3
"""Verify C-AOL release assets contain the manual handoff payload."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path
from zipfile import ZipFile


EXPECTED_MANUAL_SCENARIOS = {
    "manual.cannibal_night_pack_mcw",
    "manual.intact_camp_shakedown_mcw",
    "manual.mixed_hostile_siege_mcw",
    "manual.writhing_stalker_hit_fade_mcw",
    "manual.zombie_rider_open_field_mcw",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"release asset manual handoff check failed: {message}")


def safe_extract_zip(archive: Path, destination: Path) -> None:
    with ZipFile(archive) as zf:
        for member in zf.infolist():
            target = destination / member.filename
            require(destination in target.resolve().parents or target.resolve() == destination, f"unsafe zip path: {member.filename}")
        zf.extractall(destination)


def safe_extract_tar(archive: Path, destination: Path) -> None:
    with tarfile.open(archive, "r:gz") as tf:
        for member in tf.getmembers():
            target = destination / member.name
            require(destination in target.resolve().parents or target.resolve() == destination, f"unsafe tar path: {member.name}")
        tf.extractall(destination)


def mount_dmg(archive: Path) -> str:
    proc = subprocess.run(
        ["hdiutil", "attach", "-readonly", "-nobrowse", "-plist", str(archive)],
        capture_output=True,
        text=True,
        check=False,
    )
    require(proc.returncode == 0, f"hdiutil attach failed:\n{proc.stdout}\n{proc.stderr}")
    mount_point = ""
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("<string>/Volumes/"):
            mount_point = line.replace("<string>", "").replace("</string>", "")
    require(bool(mount_point), "hdiutil attach did not report a mount point")
    return mount_point


def detach_dmg(mount_point: str) -> None:
    subprocess.run(["hdiutil", "detach", mount_point], check=False, capture_output=True, text=True)


def copy_macos_app_from_dmg(mount_root: Path, destination: Path) -> Path:
    apps = sorted(mount_root.glob("*.app"))
    require(apps, f"no .app bundle found in mounted DMG at {mount_root}")
    require(len(apps) == 1, f"expected one .app bundle in mounted DMG, found: {[app.name for app in apps]}")
    writable_app = destination / apps[0].name
    shutil.copytree(apps[0], writable_app, symlinks=True)
    return writable_app


def find_harness_script(root: Path) -> Path:
    matches = sorted(root.rglob("tools/openclaw_harness/startup_harness.py"))
    require(matches, f"startup_harness.py not found under {root}")
    return matches[0]


def require_payload(root: Path, platform: str) -> None:
    harness = find_harness_script(root)
    game_root = harness.parents[2]
    helper_name = "zzip.exe" if platform == "windows" else "zzip"
    helper = game_root / helper_name
    require(helper.is_file(), f"missing platform zzip helper at {helper}")
    if platform != "windows":
        require(os.access(helper, os.X_OK), f"platform zzip helper is not executable: {helper}")
    require((game_root / "data").is_dir(), f"missing data directory under {game_root}")
    require((game_root / "gfx").is_dir(), f"missing gfx directory under {game_root}")
    require((harness.parent / "requirements.txt").is_file(), "missing tools/openclaw_harness/requirements.txt")

    proc = subprocess.run(
        [sys.executable, str(harness), "list-scenarios"],
        cwd=str(game_root),
        capture_output=True,
        text=True,
        check=False,
    )
    require(proc.returncode == 0, f"list-scenarios failed:\n{proc.stdout}\n{proc.stderr}")
    payload = json.loads(proc.stdout)
    names = {
        str(item.get("name", ""))
        for item in payload.get("scenarios", [])
        if str(item.get("name", "")).startswith("manual.")
    }
    require(names == EXPECTED_MANUAL_SCENARIOS, f"manual scenario set mismatch: expected {sorted(EXPECTED_MANUAL_SCENARIOS)}, got {sorted(names)}")

    for scenario in sorted(EXPECTED_MANUAL_SCENARIOS):
        proc = subprocess.run(
            [sys.executable, str(harness), "handoff", scenario, "--launch-only", "--dry-run", "--compact-stdout"],
            cwd=str(game_root),
            capture_output=True,
            text=True,
            check=False,
        )
        require(proc.returncode == 0, f"launch-only dry-run failed for {scenario}:\n{proc.stdout}\n{proc.stderr}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset", type=Path)
    parser.add_argument("--platform", choices=["windows", "macos", "linux"], required=True)
    args = parser.parse_args()

    asset = args.asset.resolve()
    require(asset.is_file(), f"asset not found: {asset}")
    mount_point = ""
    with tempfile.TemporaryDirectory(prefix="caol-release-shape-") as tmp:
        tmp_path = Path(tmp).resolve()
        if args.platform == "windows":
            safe_extract_zip(asset, tmp_path)
            root = tmp_path
        elif args.platform == "linux":
            safe_extract_tar(asset, tmp_path)
            root = tmp_path
        else:
            require(sys.platform == "darwin", "macOS DMG verification must run on macOS")
            mount_point = mount_dmg(asset)
            root = copy_macos_app_from_dmg(Path(mount_point), tmp_path)
        try:
            require_payload(root, args.platform)
        finally:
            if mount_point:
                detach_dmg(mount_point)

    print(json.dumps({
        "ok": True,
        "asset": str(asset),
        "platform": args.platform,
        "manual_scenarios": sorted(EXPECTED_MANUAL_SCENARIOS),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
