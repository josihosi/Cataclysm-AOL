#!/usr/bin/env python3
"""Phase-0 OpenClaw startup harness for Cataclysm-AOL.

Goals for this first slice:
- branch-aware profile/userdir resolution
- world/save detection
- fixture-save capture/install helpers
- launch the game on macOS with the correct userdir
- autoload an existing save if possible via --world
- otherwise drive the minimal New Game -> Play Now! default path
- copy debug log deltas and capture failure artifacts

This harness currently targets the local macOS flow and uses Peekaboo for
window focus, key presses, and screenshots when not in --dry-run mode.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Pattern, Tuple


IGNORABLE_DEBUG_LOG_PATTERNS: List[Pattern[str]] = [
    re.compile(
        r"src/mod_tracker\.h:\d+: Tried check if 'vector_null' had a duplicate, but type 'attack_vector' does not track object sources"
    ),
    re.compile( r"\d{2}:\d{2}:\d{2}\.\d{3} WARNING : .* pack load time:: .*" ),
]


@dataclass
class WorldInfo:
    name: str
    path: Path
    has_save: bool
    save_files: List[str]


@dataclass
class StartupPlan:
    profile: str
    userdir: str
    executable: str
    strategy: str
    reason: str
    target_world: str
    existing_worlds: List[Dict[str, Any]]
    fixture: str
    run_dir: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def run_json(cmd: List[str], check: bool = True) -> Dict[str, Any]:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stdout}\n{proc.stderr}")
    if not proc.stdout.strip():
        return {}
    return json.loads(proc.stdout)


def sanitize_profile_name(name: str) -> str:
    name = name.strip()
    if not name:
        return "default"
    name = name.replace("/", "_").replace("\\", "_")
    name = re.sub(r"[^A-Za-z0-9_.-]+", "_", name)
    return name or "default"


def current_branch() -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo_root()), "branch", "--show-current"],
        capture_output=True,
        text=True,
        check=False,
    )
    branch = proc.stdout.strip()
    return branch or "detached"


def resolve_profile_name(explicit: str) -> str:
    return sanitize_profile_name(explicit or current_branch())


def userdir_for_profile(profile: str) -> Path:
    return repo_root() / ".userdata" / profile


def save_dir_for_profile(profile: str) -> Path:
    return userdir_for_profile(profile) / "save"


def config_dir_for_profile(profile: str) -> Path:
    return userdir_for_profile(profile) / "config"


def fixtures_root() -> Path:
    return repo_root() / "tools" / "openclaw_harness" / "fixtures" / "saves"


def profile_config_path(profile: str) -> Path:
    return repo_root() / "tools" / "openclaw_harness" / "profiles" / f"{profile}.json"


def load_profile_config(profile: str) -> Dict[str, Any]:
    defaults: Dict[str, Any] = {
        "profile_name": profile,
        "startup": {
            "play_now_default_sequence": ["n", "d"],
            "debug_popup_ignore_text": "i",
            "initial_wait_seconds": 2.5,
            "post_input_wait_seconds": 1.0,
            "poll_seconds": 2.0,
            "timeout_seconds": 90.0,
            "max_popup_dismissals": 6,
        },
    }
    path = profile_config_path(profile)
    if not path.exists():
        return defaults
    loaded = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(loaded, dict):
        raise SystemExit(f"Invalid profile config: {path}")
    merged = dict(defaults)
    merged_startup = dict(defaults["startup"])
    merged_startup.update(loaded.get("startup", {}))
    merged.update(loaded)
    merged["startup"] = merged_startup
    return merged


def list_worlds(profile: str) -> List[WorldInfo]:
    save_dir = save_dir_for_profile(profile)
    if not save_dir.exists():
        return []
    worlds: List[WorldInfo] = []
    for path in sorted([p for p in save_dir.iterdir() if p.is_dir()], key=lambda p: p.name.lower()):
        save_files = sorted([p.name for p in path.glob("*.sav*")])
        worlds.append(WorldInfo(name=path.name, path=path, has_save=bool(save_files), save_files=save_files))
    return worlds


def read_lastworld(profile: str) -> Dict[str, str]:
    path = config_dir_for_profile(profile) / "lastworld.json"
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    if not isinstance(data, dict):
        return {}
    return {
        "world_name": str(data.get("world_name", "")).strip(),
        "character_name": str(data.get("character_name", "")).strip(),
    }


def pick_target_world(profile: str, explicit_world: str) -> Tuple[str, str, List[WorldInfo]]:
    worlds = list_worlds(profile)
    if explicit_world:
        for world in worlds:
            if world.name == explicit_world:
                return explicit_world, "explicit world requested", worlds
        return "", f"explicit world '{explicit_world}' not found", worlds

    lastworld = read_lastworld(profile)
    last_name = lastworld.get("world_name", "")
    if last_name:
        for world in worlds:
            if world.name == last_name and world.has_save:
                return world.name, "reusing last loaded save world", worlds

    for world in worlds:
        if world.has_save:
            return world.name, "using first world that already has a save", worlds

    return "", "no existing save world found; need Play Now bootstrap", worlds


def choose_strategy(profile: str, explicit_world: str) -> Tuple[str, str, str, List[WorldInfo]]:
    target_world, reason, worlds = pick_target_world(profile, explicit_world)
    if target_world:
        return "autoload_world", reason, target_world, worlds
    return "play_now_default", reason, "", worlds


def detect_executable() -> Path:
    root = repo_root()
    for name in ("cataclysm-tlg-tiles", "cataclysm-tiles"):
        path = root / name
        if path.exists() and os.access(path, os.X_OK):
            return path
    raise SystemExit("Could not find a runnable game executable (cataclysm-tlg-tiles or cataclysm-tiles).")


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def create_run_dir(profile: str) -> Path:
    run_dir = userdir_for_profile(profile) / "harness_runs" / time.strftime("%Y%m%d_%H%M%S")
    ensure_dir(run_dir)
    return run_dir


def build_plan(profile: str, explicit_world: str, fixture: str) -> StartupPlan:
    strategy, reason, target_world, worlds = choose_strategy(profile, explicit_world)
    run_dir = create_run_dir(profile)
    plan = StartupPlan(
        profile=profile,
        userdir=str(userdir_for_profile(profile)),
        executable=str(detect_executable()),
        strategy=strategy,
        reason=reason,
        target_world=target_world,
        existing_worlds=[asdict_world(world) for world in worlds],
        fixture=fixture,
        run_dir=str(run_dir),
    )
    return plan


def asdict_world(world: WorldInfo) -> Dict[str, Any]:
    return {
        "name": world.name,
        "path": str(world.path),
        "has_save": world.has_save,
        "save_files": list(world.save_files),
    }


def write_json(path: Path, data: Dict[str, Any]) -> None:
    ensure_dir(path.parent)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def require_peekaboo_permissions() -> None:
    data = run_json(["peekaboo", "permissions", "--json"])
    perms = data.get("data", {}).get("permissions", [])
    missing = [perm for perm in perms if perm.get("isRequired") and not perm.get("isGranted")]
    if missing:
        details = ", ".join(f"{perm.get('name')} ({perm.get('grantInstructions')})" for perm in missing)
        raise SystemExit(f"Peekaboo permissions missing: {details}")


def peekaboo_focus_pid(pid: int) -> None:
    subprocess.run(["peekaboo", "window", "focus", "--pid", str(pid)], check=False, capture_output=True, text=True)


def peekaboo_press_sequence(pid: int, keys: List[str], delay_ms: int = 200) -> None:
    if not keys:
        return
    cmd = ["peekaboo", "press", *keys, "--pid", str(pid), "--delay", str(delay_ms)]
    subprocess.run(cmd, check=True, capture_output=True, text=True)


def peekaboo_type_text(pid: int, text: str, delay_ms: int = 20) -> None:
    if not text:
        return
    cmd = ["peekaboo", "type", text, "--pid", str(pid), "--delay", str(delay_ms), "--profile", "linear"]
    subprocess.run(cmd, check=True, capture_output=True, text=True)


def copy_file_if_exists(src: Path, dst: Path) -> None:
    if src.exists():
        ensure_dir(dst.parent)
        shutil.copy2(src, dst)


def filter_debug_log_text(text: str) -> str:
    filtered_lines = []
    for line in text.splitlines(keepends=True):
        if any(pattern.search(line) for pattern in IGNORABLE_DEBUG_LOG_PATTERNS):
            continue
        filtered_lines.append(line)
    return "".join(filtered_lines)


def copy_filtered_debug_log_delta_if_exists(src: Path, start_size: int, dst: Path) -> None:
    if not src.exists():
        return
    size = src.stat().st_size
    if size <= start_size:
        return
    ensure_dir(dst.parent)
    with src.open("r", encoding="utf-8", errors="replace") as fh:
        fh.seek(start_size)
        data = fh.read()
    dst.write_text(filter_debug_log_text(data), encoding="utf-8")


def capture_debug_delta(profile: str, start_size: int, run_dir: Path, serial: int) -> int:
    debug_log = config_dir_for_profile(profile) / "debug.log"
    if not debug_log.exists():
        return start_size
    size = debug_log.stat().st_size
    if size <= start_size:
        return start_size
    with debug_log.open("r", encoding="utf-8", errors="replace") as fh:
        fh.seek(start_size)
        data = fh.read()
    out = run_dir / f"debug_delta_{serial:02d}.log"
    out.write_text(filter_debug_log_text(data), encoding="utf-8")
    return size


def capture_screenshot(pid: int, run_dir: Path, label: str) -> None:
    png_path = run_dir / f"{label}.png"
    json_path = run_dir / f"{label}.peekaboo.json"
    proc = subprocess.run(
        ["peekaboo", "see", "--pid", str(pid), "--path", str(png_path), "--json"],
        capture_output=True,
        text=True,
        check=False,
    )
    payload = {
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }
    json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def kill_existing_game_processes() -> List[int]:
    proc = subprocess.run(["pgrep", "-f", "cataclysm-(tiles|tlg-tiles)"], capture_output=True, text=True, check=False)
    pids: List[int] = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            pid = int(line)
        except ValueError:
            continue
        if pid == os.getpid():
            continue
        pids.append(pid)
    for pid in pids:
        subprocess.run(["kill", str(pid)], check=False, capture_output=True, text=True)
    if pids:
        time.sleep(1.0)
        for pid in pids:
            subprocess.run(["kill", "-9", str(pid)], check=False, capture_output=True, text=True)
    return pids


def launch_game(profile: str, target_world: str, run_dir: Path) -> subprocess.Popen[str]:
    exe = detect_executable()
    cmd = [str(exe), "--userdir", f".userdata/{profile}/"]
    if target_world:
        cmd.extend(["--world", target_world])
    stdout_log = (run_dir / "game.stdout.log").open("w", encoding="utf-8")
    stderr_log = (run_dir / "game.stderr.log").open("w", encoding="utf-8")
    return subprocess.Popen(cmd, cwd=str(repo_root()), stdout=stdout_log, stderr=stderr_log, text=True)


def load_fixture_manifest(path: Path) -> Dict[str, Any]:
    manifest_path = path / "manifest.json"
    if not manifest_path.exists():
        return {}
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def profile_fixture_root(profile: str) -> Path:
    return fixtures_root() / profile


def list_fixtures(profile: str) -> List[Dict[str, Any]]:
    root = profile_fixture_root(profile)
    if not root.exists():
        return []
    fixtures = []
    for path in sorted([p for p in root.iterdir() if p.is_dir()], key=lambda p: p.name.lower()):
        manifest = load_fixture_manifest(path)
        fixtures.append({
            "fixture": path.name,
            "path": str(path),
            "manifest": manifest,
        })
    return fixtures


def choose_world_for_fixture(profile: str, explicit_world: str) -> WorldInfo:
    worlds = list_worlds(profile)
    if explicit_world:
        for world in worlds:
            if world.name == explicit_world:
                return world
        raise SystemExit(f"World not found for fixture capture: {explicit_world}")
    last = read_lastworld(profile).get("world_name", "")
    for world in worlds:
        if world.name == last:
            return world
    if worlds:
        return worlds[0]
    raise SystemExit(f"No worlds found under {save_dir_for_profile(profile)}")


def capture_fixture(profile: str, fixture_name: str, explicit_world: str, overwrite: bool) -> Dict[str, Any]:
    fixture_dir = profile_fixture_root(profile) / fixture_name
    if fixture_dir.exists() and not overwrite:
        raise SystemExit(f"Fixture already exists: {fixture_dir} (use --overwrite)")
    if fixture_dir.exists():
        shutil.rmtree(fixture_dir)
    ensure_dir(fixture_dir)
    world = choose_world_for_fixture(profile, explicit_world)
    dest_save = fixture_dir / "save" / world.name
    ensure_dir(dest_save.parent)
    shutil.copytree(world.path, dest_save, dirs_exist_ok=False)
    manifest = {
        "profile": profile,
        "world": world.name,
        "captured_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "source_path": str(world.path),
    }
    write_json(fixture_dir / "manifest.json", manifest)
    return {
        "fixture": fixture_name,
        "fixture_dir": str(fixture_dir),
        "world": world.name,
    }


def install_fixture(profile: str, fixture_name: str, replace: bool) -> Dict[str, Any]:
    fixture_dir = profile_fixture_root(profile) / fixture_name
    save_src = fixture_dir / "save"
    if not save_src.exists():
        raise SystemExit(f"Fixture save payload not found: {save_src}")
    save_dst = save_dir_for_profile(profile)
    ensure_dir(save_dst)
    installed_worlds = []
    for world_dir in sorted([p for p in save_src.iterdir() if p.is_dir()], key=lambda p: p.name.lower()):
        dst = save_dst / world_dir.name
        if dst.exists() and not replace:
            raise SystemExit(f"Destination world already exists: {dst} (use --replace)")
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(world_dir, dst)
        installed_worlds.append(world_dir.name)
    return {
        "fixture": fixture_name,
        "profile": profile,
        "installed_worlds": installed_worlds,
        "destination": str(save_dst),
    }


def success_from_lastworld(profile: str, baseline_mtime: float, expected_world: str) -> Tuple[bool, Dict[str, str]]:
    path = config_dir_for_profile(profile) / "lastworld.json"
    if not path.exists():
        return False, {}
    stat = path.stat()
    if stat.st_mtime <= baseline_mtime:
        return False, {}
    data = read_lastworld(profile)
    if not data.get("world_name") or not data.get("character_name"):
        return False, data
    if expected_world and data.get("world_name") != expected_world:
        return False, data
    return True, data


def run_startup(args: argparse.Namespace) -> int:
    profile = resolve_profile_name(args.profile)
    if args.fixture:
        install_fixture(profile, args.fixture, replace=args.replace_existing_worlds)
    config = load_profile_config(profile)
    plan = build_plan(profile, args.world, args.fixture)
    run_dir = Path(plan.run_dir)
    write_json(run_dir / "plan.json", asdict(plan))

    if args.dry_run:
        print(json.dumps(asdict(plan), indent=2, ensure_ascii=False))
        return 0

    require_peekaboo_permissions()
    killed_pids = kill_existing_game_processes()
    ensure_dir(config_dir_for_profile(profile))
    debug_log = config_dir_for_profile(profile) / "debug.log"
    lastworld = config_dir_for_profile(profile) / "lastworld.json"
    debug_size = debug_log.stat().st_size if debug_log.exists() else 0
    baseline_mtime = lastworld.stat().st_mtime if lastworld.exists() else 0.0
    copy_file_if_exists(lastworld, run_dir / "lastworld.before.json")
    proc = launch_game(profile, plan.target_world, run_dir)
    write_json(run_dir / "process.json", {
        "pid": proc.pid,
        "command": [plan.executable, "--userdir", f".userdata/{profile}/"] + (["--world", plan.target_world] if plan.target_world else []),
        "killed_previous_pids": killed_pids,
    })

    startup_cfg = config["startup"]
    time.sleep(float(startup_cfg["initial_wait_seconds"]))
    peekaboo_focus_pid(proc.pid)
    if plan.strategy == "play_now_default":
        peekaboo_press_sequence(proc.pid, list(startup_cfg["play_now_default_sequence"]))
        time.sleep(float(startup_cfg["post_input_wait_seconds"]))

    poll_seconds = float(startup_cfg["poll_seconds"])
    timeout_seconds = float(startup_cfg["timeout_seconds"])
    debug_popup_ignore_text = str(startup_cfg["debug_popup_ignore_text"])
    max_popup_dismissals = int(startup_cfg["max_popup_dismissals"])
    dismissals = 0
    deadline = time.monotonic() + timeout_seconds
    expected_world = plan.target_world

    while time.monotonic() < deadline:
        code = proc.poll()
        if code is not None:
            capture_screenshot(proc.pid, run_dir, "failure_process_exit")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            print(json.dumps({
                "ok": False,
                "reason": "process_exited",
                "returncode": code,
                "run_dir": str(run_dir),
            }, indent=2, ensure_ascii=False))
            return 1

        ok, data = success_from_lastworld(profile, baseline_mtime, expected_world)
        if ok:
            capture_screenshot(proc.pid, run_dir, "success")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            print(json.dumps({
                "ok": True,
                "ok_with_debug_popups": dismissals > 0,
                "debug_popups_recorded": dismissals,
                "strategy": plan.strategy,
                "lastworld": data,
                "run_dir": str(run_dir),
            }, indent=2, ensure_ascii=False))
            return 0

        new_debug_size = capture_debug_delta(profile, debug_size, run_dir, dismissals + 1)
        if new_debug_size > debug_size and dismissals < max_popup_dismissals:
            debug_size = new_debug_size
            capture_screenshot(proc.pid, run_dir, f"debug_popup_{dismissals + 1:02d}")
            peekaboo_focus_pid(proc.pid)
            peekaboo_type_text(proc.pid, debug_popup_ignore_text)
            dismissals += 1
        time.sleep(poll_seconds)

    capture_screenshot(proc.pid, run_dir, "failure_timeout")
    copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
    copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
    print(json.dumps({
        "ok": False,
        "reason": "startup_timeout",
        "strategy": plan.strategy,
        "run_dir": str(run_dir),
    }, indent=2, ensure_ascii=False))
    return 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Phase-0 startup harness for Cataclysm-AOL.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    plan_p = subparsers.add_parser("plan", help="Print the branch-aware startup plan.")
    plan_p.add_argument("--profile", default="", help="Profile name (defaults to sanitized current branch).")
    plan_p.add_argument("--world", default="", help="Explicit target world name.")
    plan_p.add_argument("--fixture", default="", help="Fixture name only for plan metadata.")

    start_p = subparsers.add_parser("start", help="Launch and try to reach gameplay.")
    start_p.add_argument("--profile", default="", help="Profile name (defaults to sanitized current branch).")
    start_p.add_argument("--world", default="", help="Explicit target world name.")
    start_p.add_argument("--fixture", default="", help="Install this fixture before startup.")
    start_p.add_argument("--replace-existing-worlds", action="store_true", help="Allow fixture install to replace existing worlds.")
    start_p.add_argument("--dry-run", action="store_true", help="Resolve plan only; do not launch or press keys.")

    list_p = subparsers.add_parser("list-fixtures", help="List harness-owned save fixtures for a profile.")
    list_p.add_argument("--profile", default="", help="Profile name (defaults to sanitized current branch).")

    capture_p = subparsers.add_parser("capture-fixture", help="Capture a world save into harness-owned fixture storage.")
    capture_p.add_argument("fixture", help="Fixture name to create/update.")
    capture_p.add_argument("--profile", default="", help="Profile name (defaults to sanitized current branch).")
    capture_p.add_argument("--world", default="", help="Explicit world name to capture.")
    capture_p.add_argument("--overwrite", action="store_true", help="Overwrite an existing fixture.")

    install_p = subparsers.add_parser("install-fixture", help="Install a captured fixture into the target profile save dir.")
    install_p.add_argument("fixture", help="Fixture name to install.")
    install_p.add_argument("--profile", default="", help="Profile name (defaults to sanitized current branch).")
    install_p.add_argument("--replace", action="store_true", help="Replace existing worlds when installing.")

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "plan":
        plan = build_plan(resolve_profile_name(args.profile), args.world, args.fixture)
        print(json.dumps(asdict(plan), indent=2, ensure_ascii=False))
        return 0
    if args.command == "start":
        return run_startup(args)
    if args.command == "list-fixtures":
        profile = resolve_profile_name(args.profile)
        print(json.dumps({"profile": profile, "fixtures": list_fixtures(profile)}, indent=2, ensure_ascii=False))
        return 0
    if args.command == "capture-fixture":
        profile = resolve_profile_name(args.profile)
        result = capture_fixture(profile, args.fixture, args.world, overwrite=args.overwrite)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    if args.command == "install-fixture":
        profile = resolve_profile_name(args.profile)
        result = install_fixture(profile, args.fixture, replace=args.replace)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    parser.error(f"Unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
