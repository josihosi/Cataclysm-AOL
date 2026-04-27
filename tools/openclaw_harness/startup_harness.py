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
import signal
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Pattern, Tuple

from bandit_live_world_audit import load_special_placements as load_bandit_special_placements


IGNORABLE_DEBUG_LOG_PATTERNS: List[Pattern[str]] = [
    re.compile(
        r"src/mod_tracker\.h:\d+: Tried check if 'vector_null' had a duplicate, but type 'attack_vector' does not track object sources"
    ),
    re.compile( r"\d{2}:\d{2}:\d{2}\.\d{3} WARNING : .* pack load time:: .*" ),
]

RUNTIME_RELEVANT_PATHS: Tuple[str, ...] = (
    "src",
    "data",
    "lang",
    "gfx",
    "lua",
    "mods",
    "build-data",
    "cmake",
    "CMakeLists.txt",
    "Makefile",
    "make.sh",
)

PATROL_CACHE_RE = re.compile(
    r"camp patrol: cache camp=(?P<camp>.+?) shift=(?P<shift>\w+) workers=(?P<workers>\d+) "
    r"roster=(?P<roster>\d+) active=(?P<active>\d+) reserve=(?P<reserve>\d+) "
    r"clusters=(?P<clusters>.+)$"
)
PATROL_RUNTIME_RE = re.compile(
    r"camp patrol: runtime worker=(?P<worker>.+?) behavior=(?P<behavior>\w+) "
    r"pos=(?P<pos>\([^)]*\)) target=(?P<target>\([^)]*\)) route=(?P<route>\[[^\]]*\])"
)
PATROL_COORD_RE = re.compile(r"\((-?\d+),(-?\d+),(-?\d+)\)")

CLEANUP_ACCEPTED_STATUSES = {
    "already_exited",
    "terminated",
    "terminated_during_kill_escalation",
    "killed",
}

PLAYER_MUTATION_STATE_TEMPLATE: Dict[str, Any] = {
    "corrupted": 0,
    "key": 32,
    "charge": 0,
    "powered": False,
    "show_sprite": True,
}

OMAPX = 180
OMAPY = OMAPX
OVERMAP_DEPTH = 10


@dataclass
class WorldInfo:
    name: str
    path: Path
    has_save: bool
    save_files: List[str]
    save_markers: List[str]


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


def current_head_short() -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo_root()), "rev-parse", "--short=10", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.stdout.strip()


def runtime_relevant_changes_since(captured_head: str) -> Tuple[List[str], str]:
    captured = str(captured_head or "").strip()
    if not captured:
        return [], "captured head is empty"

    commit_check = subprocess.run(
        ["git", "-C", str(repo_root()), "cat-file", "-e", f"{captured}^{{commit}}"],
        capture_output=True,
        text=True,
        check=False,
    )
    if commit_check.returncode != 0:
        detail = (commit_check.stderr or commit_check.stdout).strip()
        return [], detail or f"captured head {captured} is not resolvable"

    proc = subprocess.run(
        [
            "git",
            "-C",
            str(repo_root()),
            "diff",
            "--name-only",
            f"{captured}..HEAD",
            "--",
            *RUNTIME_RELEVANT_PATHS,
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        return [], detail or f"git diff failed for {captured}..HEAD"

    changes = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
    return changes, ""


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


def profile_snapshots_root() -> Path:
    return repo_root() / "tools" / "openclaw_harness" / "fixtures" / "profiles"


def scenarios_root() -> Path:
    return repo_root() / "tools" / "openclaw_harness" / "scenarios"


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
            "post_lastworld_wait_seconds": 0.0,
            "post_lastworld_continue_keys": [],
        },
    }

    merged = dict(defaults)
    merged_startup = dict(defaults["startup"])
    candidate_paths = [profile_config_path("master")]
    if profile != "master":
        candidate_paths.append(profile_config_path(profile))

    for path in candidate_paths:
        if not path.exists():
            continue
        loaded = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(loaded, dict):
            raise SystemExit(f"Invalid profile config: {path}")
        merged_startup.update(loaded.get("startup", {}))
        merged.update(loaded)

    merged["profile_name"] = profile
    merged["startup"] = merged_startup
    return merged


def world_save_marker_paths(world_dir: Path) -> List[Path]:
    markers: List[Path] = list(world_dir.glob("*.sav*"))
    for character_dir in sorted(world_dir.glob("*.mm1"), key=lambda p: p.name.lower()):
        if not character_dir.is_dir():
            continue
        payloads = sorted(character_dir.glob("*.zzip"), key=lambda p: p.name.lower())
        markers.extend(payloads if payloads else [character_dir])
    return sorted(markers, key=lambda p: str(p.relative_to(world_dir)).lower())


def world_save_marker_names(world_dir: Path) -> List[str]:
    marker_names = []
    for marker in world_save_marker_paths(world_dir):
        try:
            marker_names.append(str(marker.relative_to(world_dir)))
        except ValueError:
            marker_names.append(marker.name)
    return marker_names


def list_worlds(profile: str) -> List[WorldInfo]:
    save_dir = save_dir_for_profile(profile)
    if not save_dir.exists():
        return []
    worlds: List[WorldInfo] = []
    for path in sorted([p for p in save_dir.iterdir() if p.is_dir()], key=lambda p: p.name.lower()):
        save_files = sorted([p.name for p in path.glob("*.sav*")])
        save_markers = world_save_marker_names(path)
        worlds.append(
            WorldInfo(
                name=path.name,
                path=path,
                has_save=bool(save_markers),
                save_files=save_files,
                save_markers=save_markers,
            )
        )
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


def load_game_options(profile: str) -> Dict[str, str]:
    path = config_dir_for_profile(profile) / "options.json"
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    if not isinstance(data, list):
        return {}

    options: Dict[str, str] = {}
    for entry in data:
        if not isinstance(entry, dict):
            continue
        name = str(entry.get("name", "")).strip()
        if not name:
            continue
        options[name] = str(entry.get("value", "")).strip()
    return options


def resolve_configured_python_command(raw_value: str) -> Tuple[List[str], str]:
    raw = str(raw_value).strip()
    if not raw:
        return [], ""

    expanded = Path(raw).expanduser()
    candidates: List[Path] = []
    if expanded.is_dir():
        candidates = [
            expanded / "bin" / "python3",
            expanded / "bin" / "python",
            expanded / "Scripts" / "python.exe",
            expanded / "Scripts" / "python",
        ]
    elif expanded.exists():
        candidates = [expanded]

    for candidate in candidates:
        if candidate.exists():
            return [str(candidate)], str(candidate)

    return [raw], raw


def resolve_game_runtime_python(options: Dict[str, str]) -> Tuple[List[str], str, str]:
    configured_value = options.get("LLM_INTENT_PYTHON", "")
    configured_cmd, configured_label = resolve_configured_python_command(configured_value)
    if configured_cmd:
        return configured_cmd, configured_label, "configured_option"

    backend = options.get("LLM_INTENT_BACKEND", "").strip().lower()
    if backend == "api":
        fallback_cmd, fallback_label = resolve_configured_python_command("/Users/josefhorvath/ollama/api_env311")
        if fallback_cmd:
            return fallback_cmd, fallback_label, "api_fallback_venv"

    if backend in {"api", "ollama"}:
        platform_default = "python" if os.name == "nt" else "/usr/bin/python3"
        platform_cmd, platform_label = resolve_configured_python_command(platform_default)
        if platform_cmd:
            return platform_cmd, platform_label, f"{backend}_platform_default"
        return [platform_default], platform_default, f"{backend}_platform_default"

    return configured_cmd, configured_label, "configured_option"


def probe_runtime_blockers(profile: str, artifact_source: str) -> List[Dict[str, str]]:
    normalized_source = str(artifact_source).strip().lower()
    if normalized_source not in {"llm", "llm_intent", "llm_intent.log"}:
        return []

    options = load_game_options(profile)
    blockers: List[Dict[str, str]] = []

    if options.get("LLM_INTENT_ENABLE", "").lower() != "true":
        blockers.append({
            "code": "llm_intent_disabled",
            "message": "LLM intent is disabled in profile options.",
            "option": "LLM_INTENT_ENABLE",
        })
    if options.get("DEBUG_LLM_INTENT_LOG", "").lower() != "true":
        blockers.append({
            "code": "llm_intent_log_disabled",
            "message": "LLM intent log capture is disabled in profile options.",
            "option": "DEBUG_LLM_INTENT_LOG",
        })

    backend = options.get("LLM_INTENT_BACKEND", "").strip().lower()
    python_cmd, python_label, python_source = resolve_game_runtime_python(options)
    if not python_cmd:
        blockers.append({
            "code": "llm_python_missing",
            "message": "LLM runner Python path is empty and no runtime fallback resolved.",
            "option": "LLM_INTENT_PYTHON",
        })
    elif backend == "api":
        import_check = subprocess.run(
            python_cmd + ["-c", "import any_llm"],
            capture_output=True,
            text=True,
            check=False,
        )
        if import_check.returncode != 0:
            detail_text = (import_check.stderr or import_check.stdout).strip()
            detail = detail_text.splitlines()[0] if detail_text else "import any_llm failed"
            blockers.append({
                "code": "llm_python_missing_any_llm",
                "message": f"Resolved LLM runner Python cannot import any_llm ({detail}).",
                "option": "LLM_INTENT_PYTHON",
                "python": python_label,
                "python_source": python_source,
            })

    return blockers


def probe_runtime_warnings(profile: str, artifact_source: str) -> List[Dict[str, str]]:
    normalized_source = str(artifact_source).strip().lower()
    if normalized_source not in {"llm", "llm_intent", "llm_intent.log"}:
        return []

    options = load_game_options(profile)
    warnings: List[Dict[str, str]] = []
    backend = options.get("LLM_INTENT_BACKEND", "").strip().lower()
    python_cmd, python_label, python_source = resolve_game_runtime_python(options)

    if backend == "api" and options.get("LLM_INTENT_PYTHON", "").strip() == "" and python_cmd:
        warnings.append({
            "code": "llm_python_option_empty_using_runtime_fallback",
            "message": f"LLM_INTENT_PYTHON is empty; the runtime will fall back to {python_label} ({python_source}).",
            "option": "LLM_INTENT_PYTHON",
            "python": python_label,
            "python_source": python_source,
        })

    if backend == "api":
        api_key_env = options.get("LLM_INTENT_API_KEY_ENV", "").strip()
        if not api_key_env:
            warnings.append({
                "code": "llm_api_key_env_missing",
                "message": "API backend is selected, but no API key env-var name is configured; prompt logging may still work, but responses will fail.",
                "option": "LLM_INTENT_API_KEY_ENV",
            })
        elif not os.environ.get(api_key_env):
            warnings.append({
                "code": "llm_api_key_env_unset",
                "message": f"API backend is selected, but env var '{api_key_env}' is not set for the harness process; prompt logging may still work, but responses will fail.",
                "option": "LLM_INTENT_API_KEY_ENV",
                "env_var": api_key_env,
            })

    return warnings


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
        "save_markers": list(world.save_markers),
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


def peekaboo_focus_pid(pid: int) -> Dict[str, Any]:
    cmd = ["peekaboo", "window", "focus", "--pid", str(pid)]
    proc = subprocess.run(cmd, check=False, capture_output=True, text=True)
    return {
        "command": cmd,
        "returncode": proc.returncode,
        "ok": proc.returncode == 0,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


def run_peekaboo_interaction(
    pid: int,
    cmd: List[str],
    *,
    retry_delay_seconds: float = 0.15,
) -> None:
    peekaboo_focus_pid(pid)
    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError:
        peekaboo_focus_pid(pid)
        if retry_delay_seconds > 0:
            time.sleep(retry_delay_seconds)
        subprocess.run(cmd, check=True, capture_output=True, text=True)


PEEKABOO_KEY_ALIASES = {
    "esc": "escape",
    "backspace": "delete",
    "enter": "return",
}


def normalize_peekaboo_key(raw_key: Any) -> str:
    key = str(raw_key).strip()
    if not key:
        raise SystemExit("Peekaboo key cannot be empty")
    return PEEKABOO_KEY_ALIASES.get(key.lower(), key)


def is_printable_text_key(key: str) -> bool:
    return len(key) == 1 and key.isprintable() and key not in {"\n", "\r", "\t"}


def peekaboo_press_sequence(pid: int, keys: List[str], delay_ms: int = 200) -> None:
    if not keys:
        return
    text_buffer: List[str] = []

    def flush_text_buffer() -> None:
        if not text_buffer:
            return
        peekaboo_type_text(pid, "".join(text_buffer), delay_ms=max(10, min(delay_ms, 200)))
        text_buffer.clear()

    for raw_key in keys:
        key = normalize_peekaboo_key(raw_key)
        if is_printable_text_key(key):
            text_buffer.append(key)
            continue
        flush_text_buffer()
        cmd = ["peekaboo", "press", key, "--pid", str(pid), "--delay", str(delay_ms)]
        run_peekaboo_interaction(pid, cmd)
    flush_text_buffer()


def peekaboo_type_text(pid: int, text: str, delay_ms: int = 20) -> None:
    if not text:
        return
    max_chunk_len = 20
    for start in range(0, len(text), max_chunk_len):
        chunk = text[start:start + max_chunk_len]
        cmd = ["peekaboo", "type", chunk, "--pid", str(pid), "--delay", str(delay_ms), "--profile", "linear"]
        run_peekaboo_interaction(pid, cmd)


def advance_turns(pid: int, count: int) -> Dict[str, Any]:
    timing: Dict[str, Any] = {
        "count": max(count, 0),
        "batch_size": 120,
        "batches": [],
        "total_duration_seconds": 0.0,
        "avg_turn_ms": 0.0,
        "max_batch_duration_seconds": 0.0,
        "max_batch_turn_ms": 0.0,
    }
    if count <= 0:
        return timing

    batch_size = timing["batch_size"]
    for start in range(0, count, batch_size):
        batch_count = min(batch_size, count - start)
        started_at = time.perf_counter()
        peekaboo_press_sequence(pid, ["."] * batch_count)
        duration = time.perf_counter() - started_at
        batch = {
            "start_turn": start + 1,
            "count": batch_count,
            "duration_seconds": round(duration, 6),
            "per_turn_ms": round((duration / batch_count) * 1000.0, 3),
        }
        timing["batches"].append(batch)
        timing["total_duration_seconds"] += duration
        timing["max_batch_duration_seconds"] = max(timing["max_batch_duration_seconds"], duration)
        timing["max_batch_turn_ms"] = max(timing["max_batch_turn_ms"], batch["per_turn_ms"])
        if start + batch_size < count:
            time.sleep(0.05)

    timing["total_duration_seconds"] = round(timing["total_duration_seconds"], 6)
    timing["avg_turn_ms"] = round((timing["total_duration_seconds"] / count) * 1000.0, 3)
    timing["max_batch_duration_seconds"] = round(timing["max_batch_duration_seconds"], 6)
    return timing


LONG_WAIT_MENU_CHOICES = {
    "1": "20s",
    "2": "1m",
    "3": "5m",
    "4": "30m",
    "5": "1h",
    "6": "2h",
    "7": "3h",
    "8": "6h",
    "d": "daylight",
    "n": "noon",
    "k": "night",
    "m": "midnight",
    "W": "weather",
}


def classify_wait_screen_text(
    screen_text_report: Dict[str, Any],
    complete_patterns: List[str],
    interrupt_patterns: List[str],
) -> Dict[str, Any]:
    complete_matches = find_screen_text_matches(screen_text_report, complete_patterns)
    interrupt_matches = find_screen_text_matches(screen_text_report, interrupt_patterns)
    if interrupt_matches:
        status = "interrupted_or_prompt_visible"
    elif complete_matches:
        status = "completed"
    else:
        status = "unknown_after_wait"
    return {
        "status": status,
        "complete_patterns": complete_patterns,
        "interrupt_patterns": interrupt_patterns,
        "complete_matches": complete_matches,
        "interrupt_matches": interrupt_matches,
    }




def screen_text_body( screen_text_report: Dict[str, Any] ) -> str:
    text = screen_text_report.get( "text", "" )
    if isinstance( text, str ):
        return text
    lines = screen_text_report.get( "lines", [] )
    if isinstance( lines, list ):
        return "\n".join( str( line ) for line in lines )
    return ""


def wait_duration_seconds( duration: str ) -> Optional[int]:
    value = duration.strip().lower()
    if not value:
        return None
    match = re.fullmatch( r"(\d+)\s*s(?:ec(?:ond)?s?)?", value )
    if match:
        return int( match.group( 1 ) )
    match = re.fullmatch( r"(\d+)\s*m(?:in(?:ute)?s?)?", value )
    if match:
        return int( match.group( 1 ) ) * 60
    match = re.fullmatch( r"(\d+)\s*h(?:our)?s?", value )
    if match:
        return int( match.group( 1 ) ) * 60 * 60
    return None


def wait_menu_expected_duration_patterns( expected_duration: str ) -> List[str]:
    seconds = wait_duration_seconds( expected_duration )
    if seconds is None:
        return [expected_duration] if expected_duration else []
    if seconds < 60:
        amount = seconds
        unit = "second" if amount == 1 else "seconds"
    elif seconds < 60 * 60:
        amount = seconds // 60
        unit = "minute" if amount == 1 else "minutes"
    else:
        amount = seconds // ( 60 * 60 )
        unit = "hour" if amount == 1 else "hours"
    return [
        expected_duration,
        f"{amount} {unit}",
        f"{amount}{unit[0]}",
    ]


def extract_clock_or_turn_evidence( screen_text_report: Dict[str, Any] ) -> Dict[str, Any]:
    text = screen_text_body( screen_text_report )
    clock_matches: List[Dict[str, Any]] = []
    for match in re.finditer( r"\b([0-2]?\d):([0-5]\d)(?::([0-5]\d))?\b", text ):
        hour = int( match.group( 1 ) )
        minute = int( match.group( 2 ) )
        second = int( match.group( 3 ) or 0 )
        if hour <= 23:
            clock_matches.append( {
                "text": match.group( 0 ),
                "seconds_since_midnight": hour * 3600 + minute * 60 + second,
            } )
    turn_matches: List[Dict[str, Any]] = []
    for match in re.finditer( r"\bturn\s*[:#]?\s*(\d{1,12})\b", text, flags=re.IGNORECASE ):
        turn_matches.append( {"text": match.group( 0 ), "turn": int( match.group( 1 ) )} )
    return {
        "clock_matches": clock_matches[:5],
        "turn_matches": turn_matches[:5],
        "source": str( screen_text_report.get( "source_png", "" ) or screen_text_report.get( "image_path", "" ) ),
    }


def classify_wait_step_ledger(
    *,
    label: str,
    choice_key: str,
    expected_duration: str,
    before_text: Dict[str, Any],
    menu_text: Dict[str, Any],
    after_text: Dict[str, Any],
    wait_classification: Dict[str, Any],
) -> Dict[str, Any]:
    expected_seconds = wait_duration_seconds( expected_duration )
    duration_patterns = wait_menu_expected_duration_patterns( expected_duration )
    menu_body = screen_text_body( menu_text ).lower()
    menu_expected_matches = [pattern for pattern in duration_patterns if pattern.lower() in menu_body]
    menu_has_wait_prompt = "wait" in menu_body and bool( menu_body.strip() )
    before_clock = extract_clock_or_turn_evidence( before_text )
    after_clock = extract_clock_or_turn_evidence( after_text )
    elapsed: Dict[str, Any] = {
        "status": "not_parsed",
        "expected_seconds": expected_seconds,
    }
    if before_clock["turn_matches"] and after_clock["turn_matches"]:
        delta = after_clock["turn_matches"][0]["turn"] - before_clock["turn_matches"][0]["turn"]
        elapsed.update( {"status": "turn_delta_parsed", "delta_turns": delta} )
    elif before_clock["clock_matches"] and after_clock["clock_matches"]:
        before_seconds = before_clock["clock_matches"][0]["seconds_since_midnight"]
        after_seconds = after_clock["clock_matches"][0]["seconds_since_midnight"]
        delta = after_seconds - before_seconds
        if delta < 0:
            delta += 24 * 60 * 60
        elapsed.update( {"status": "clock_delta_parsed", "delta_seconds": delta} )

    wait_status = str( wait_classification.get( "status", "" ) )
    issues: List[str] = []
    if not menu_has_wait_prompt:
        issues.append( "wait_menu_ocr_missing_prompt" )
    if expected_seconds is not None and not menu_expected_matches:
        issues.append( "wait_menu_ocr_missing_expected_duration" )
    if elapsed["status"] == "not_parsed":
        issues.append( "before_after_clock_or_turn_not_parsed" )
    if wait_status == "interrupted_or_prompt_visible":
        verdict = "blocked_wait_interrupted_or_prompt_visible"
    elif wait_status != "completed":
        verdict = "yellow_wait_finish_or_interrupt_not_classified"
        issues.append( "missing_finish_or_interruption_signal" )
    elif issues:
        verdict = "yellow_wait_elapsed_or_menu_not_fully_proven"
    else:
        verdict = "green_wait_step_proven"

    return {
        "label": label,
        "choice_key": choice_key,
        "expected_duration": expected_duration,
        "expected_seconds": expected_seconds,
        "wait_menu_artifact": f"{label}.wait_menu.png / {label}.wait_menu.screen_text.json",
        "menu_has_wait_prompt": menu_has_wait_prompt,
        "menu_expected_duration_patterns": duration_patterns,
        "menu_expected_matches": menu_expected_matches,
        "before_clock_or_turn": before_clock,
        "after_clock_or_turn": after_clock,
        "elapsed": elapsed,
        "finish_or_interrupt_status": wait_status,
        "issues": issues,
        "verdict": verdict,
        "review_rule": (
            "Artifact matches do not make this wait step green. Green requires the wait menu, "
            "a parsed before/after clock or turn delta, and either a finish signal or classified interruption."
        ),
    }


def summarize_wait_step_ledgers( step_reports: List[Dict[str, Any]] ) -> Dict[str, Any]:
    ledgers = [step.get( "wait_step_ledger" ) for step in step_reports if isinstance( step.get( "wait_step_ledger" ), dict )]
    verdicts = [str( ledger.get( "verdict", "" ) ) for ledger in ledgers]
    blocked = [ledger for ledger in ledgers if str( ledger.get( "verdict", "" ) ).startswith( "blocked" )]
    yellow = [ledger for ledger in ledgers if str( ledger.get( "verdict", "" ) ).startswith( "yellow" )]
    green = [ledger for ledger in ledgers if str( ledger.get( "verdict", "" ) ).startswith( "green" )]
    if blocked:
        status = "blocked_wait_step"
    elif yellow:
        status = "yellow_wait_step_unverified"
    elif ledgers and len( green ) == len( ledgers ):
        status = "green_wait_steps_proven"
    else:
        status = "no_wait_steps"
    return {
        "status": status,
        "count": len( ledgers ),
        "green_count": len( green ),
        "yellow_count": len( yellow ),
        "blocked_count": len( blocked ),
        "verdicts": verdicts,
        "non_green_steps": [
            {
                "label": str( ledger.get( "label", "" ) ),
                "verdict": str( ledger.get( "verdict", "" ) ),
                "issues": ledger.get( "issues", [] ),
            }
            for ledger in ledgers if not str( ledger.get( "verdict", "" ) ).startswith( "green" )
        ],
        "review_rule": "Overall artifact verdict must not be green when wait ledgers are yellow or blocked.",
    }

def capture_wait_artifact_delta(
    artifact_log: Optional[Path],
    run_dir: Path,
    label: str,
    suffix: str,
    start_size: int,
    patterns: List[str],
    *,
    filter_debug_noise: bool,
) -> Dict[str, Any]:
    if artifact_log is None:
        return {
            "status": "not_configured",
            "source_log": "",
            "start_size": start_size,
            "end_size": start_size,
            "line_count": 0,
            "patterns": patterns,
            "matches_by_pattern": [],
            "matched_lines": [],
            "path": "",
        }
    end_size = artifact_log.stat().st_size if artifact_log.exists() else start_size
    text = read_log_delta(artifact_log, start_size, filter_debug_noise=filter_debug_noise)
    out_path = run_dir / f"{label}.{suffix}.artifacts.log"
    if text:
        out_path.write_text(text, encoding="utf-8")
    matches_by_pattern: List[Dict[str, Any]] = []
    matched_lines: List[str] = []
    for pattern in patterns:
        lines = [line for line in text.splitlines() if pattern in line]
        matches_by_pattern.append({"pattern": pattern, "lines": lines})
        matched_lines.extend(lines)
    return {
        "status": "captured" if text else "no_new_artifacts",
        "source_log": str(artifact_log),
        "start_size": start_size,
        "end_size": end_size,
        "line_count": len(text.splitlines()) if text else 0,
        "patterns": patterns,
        "matches_by_pattern": matches_by_pattern,
        "matched_lines": matched_lines,
        "path": str(out_path) if text else "",
    }


def execute_long_wait_action(
    pid: int,
    run_dir: Path,
    label: str,
    step: Dict[str, Any],
    *,
    artifact_log: Optional[Path] = None,
    artifact_baseline: int = 0,
    filter_debug_noise: bool = False,
    artifact_patterns: Optional[List[str]] = None,
) -> Dict[str, Any]:
    wait_key = str(step.get("wait_key", "|") or "|")
    choice_key = str(step.get("choice_key", step.get("choice", "3")) or "3")
    expected_duration = str(
        step.get("expected_duration") or LONG_WAIT_MENU_CHOICES.get(choice_key, "unknown")
    )
    delay_ms = int(step.get("delay_ms", 200) or 200)
    pre_menu_choice_key = str(step.get("pre_menu_choice_key", "") or "").strip()
    menu_settle_seconds = float(step.get("menu_settle_seconds", 0.8) or 0.8)
    pre_menu_settle_seconds = float(step.get("pre_menu_settle_seconds", 0.5) or 0.5)
    after_choice_settle_seconds = float(step.get("after_choice_settle_seconds", 0.5) or 0.5)
    completion_wait_seconds = float(step.get("completion_wait_seconds", 8.0) or 8.0)
    tail_lines = int(step.get("extract_text_tail_lines", step.get("extract_text_after_capture_tail_lines", 32)) or 32)
    complete_patterns = normalize_screen_text_patterns(
        step.get("wait_complete_text_contains", ["You finish waiting."])
    )
    state_patterns = normalize_string_list(step.get("artifact_state_patterns", artifact_patterns or []))
    interrupt_patterns = normalize_screen_text_patterns(
        step.get("wait_interrupt_text_contains", [
            "Stop moving?",
            "Spotted",
            "hostile spotted",
            "Safe mode is triggered",
            "Ignore",
            "ignore",
            "What do you want to do",
            "Set an alarm",
            "Wait a while",
            "Continue",
            "continue",
            "interrupted",
            "You stop waiting",
        ])
    )
    report: Dict[str, Any] = {
        "wait_key": wait_key,
        "choice_key": choice_key,
        "pre_menu_choice_key": pre_menu_choice_key,
        "expected_duration": expected_duration,
        "delay_ms": delay_ms,
        "menu_settle_seconds": menu_settle_seconds,
        "pre_menu_settle_seconds": pre_menu_settle_seconds,
        "after_choice_settle_seconds": after_choice_settle_seconds,
        "completion_wait_seconds": completion_wait_seconds,
        "proof_rule": (
            "captures before/initial-menu/duration-menu/after screens plus before/after artifact deltas; "
            "does not type through prompts; interruptions remain evidence and must not be classified green by default"
        ),
        "artifact_state_patterns": state_patterns,
        "elapsed_time_evidence": {
            "duration_choice_key": choice_key,
            "expected_duration": expected_duration,
            "before_clock_source": f"{label}.before.png / {label}.before.screen_text.json",
            "after_clock_source": f"{label}.after.png / {label}.after.screen_text.json",
            "completion_signal": "wait_complete_text_contains match after choosing the bounded duration",
            "parse_note": "HUD clock/turn evidence is archived in screenshots/OCR; exact OCR parsing may be noisy and is not normalized here.",
        },
    }

    wait_start_size = artifact_log.stat().st_size if artifact_log is not None and artifact_log.exists() else artifact_baseline
    report["artifact_before_wait"] = capture_wait_artifact_delta(
        artifact_log,
        run_dir,
        label,
        "before_wait",
        artifact_baseline,
        state_patterns,
        filter_debug_noise=filter_debug_noise,
    )

    before_capture = capture_screenshot(pid, run_dir, f"{label}.before")
    report["screen_before"] = before_capture.get("screen_summary", {})
    before_text = capture_screen_text_artifact(run_dir, f"{label}.before", before_capture, tail_lines=tail_lines)
    report["screen_before_text"] = before_text

    peekaboo_press_sequence(pid, [wait_key], delay_ms=delay_ms)
    if menu_settle_seconds > 0:
        time.sleep(menu_settle_seconds)
    initial_menu_capture = capture_screenshot(pid, run_dir, f"{label}.initial_wait_menu")
    report["screen_initial_wait_menu"] = initial_menu_capture.get("screen_summary", {})
    initial_menu_text = capture_screen_text_artifact(
        run_dir, f"{label}.initial_wait_menu", initial_menu_capture, tail_lines=tail_lines
    )
    report["screen_initial_wait_menu_text"] = initial_menu_text

    if pre_menu_choice_key:
        peekaboo_press_sequence(pid, [pre_menu_choice_key], delay_ms=delay_ms)
        if pre_menu_settle_seconds > 0:
            time.sleep(pre_menu_settle_seconds)

    menu_capture = capture_screenshot(pid, run_dir, f"{label}.wait_menu")
    report["screen_wait_menu"] = menu_capture.get("screen_summary", {})
    menu_text = capture_screen_text_artifact(run_dir, f"{label}.wait_menu", menu_capture, tail_lines=tail_lines)
    report["screen_wait_menu_text"] = menu_text

    peekaboo_press_sequence(pid, [choice_key], delay_ms=delay_ms)
    if after_choice_settle_seconds > 0:
        time.sleep(after_choice_settle_seconds)
    if completion_wait_seconds > 0:
        time.sleep(completion_wait_seconds)
    after_capture = capture_screenshot(pid, run_dir, f"{label}.after")
    report["screen_after"] = after_capture.get("screen_summary", {})
    after_text = capture_screen_text_artifact(run_dir, f"{label}.after", after_capture, tail_lines=tail_lines)
    report["screen_after_text"] = after_text
    report["artifact_after_wait"] = capture_wait_artifact_delta(
        artifact_log,
        run_dir,
        label,
        "after_wait",
        wait_start_size,
        state_patterns,
        filter_debug_noise=filter_debug_noise,
    )
    report["wait_classification"] = classify_wait_screen_text(
        after_text, complete_patterns, interrupt_patterns
    )
    report["wait_step_ledger"] = classify_wait_step_ledger(
        label=label,
        choice_key=choice_key,
        expected_duration=expected_duration,
        before_text=before_text,
        menu_text=menu_text,
        after_text=after_text,
        wait_classification=report["wait_classification"],
    )
    if bool(step.get("abort_on_interrupt", False)) and \
            report["wait_classification"].get("status") == "interrupted_or_prompt_visible":
        report["status"] = "aborted_by_wait_interruption"
        report["verdict"] = str(step.get("abort_verdict", "inconclusive_wait_interrupted"))
        report["abort_reason"] = str(
            step.get("abort_reason", "wait action was interrupted or showed a prompt")
        )
    return report


def looks_like_inventory_slot(selector: str) -> bool:
    selector = selector.strip()
    return len(selector) == 1 and selector.isprintable() and not selector.isspace()


def drop_item(
    pid: int,
    *,
    query_or_slot: str,
    count: int = 1,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.35,
    prompt_settle_seconds: float = 0.25,
    exit_menu: bool = True,
) -> str:
    selector = query_or_slot.strip()
    if not selector:
        raise SystemExit("Drop-item selector cannot be empty")
    if count <= 0:
        raise SystemExit("Drop-item count must be > 0")

    peekaboo_press_sequence(pid, ["d"], delay_ms=delay_ms)
    time.sleep(menu_settle_seconds)

    selection_mode = "slot" if looks_like_inventory_slot(selector) else "filter"
    selection_key = selector if selection_mode == "slot" else "a"
    if selection_mode == "filter":
        apply_uilist_filter(
            pid,
            selector,
            delay_ms=delay_ms,
            type_delay_ms=type_delay_ms,
            settle_seconds=prompt_settle_seconds,
        )

    if count > 1:
        peekaboo_type_text(pid, str(count), delay_ms=type_delay_ms)
        time.sleep(prompt_settle_seconds)

    peekaboo_press_sequence(pid, [selection_key], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)

    if exit_menu:
        peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)

    return selection_mode


def run_debug_menu_shortcut_path(
    pid: int,
    shortcut_keys: List[str],
    *,
    delay_ms: int = 200,
    menu_settle_seconds: float = 0.35,
) -> None:
    if not shortcut_keys:
        raise SystemExit("Debug menu shortcut path needs at least one key")
    peekaboo_press_sequence(pid, ["}"], delay_ms=delay_ms)
    time.sleep(menu_settle_seconds)
    for key in shortcut_keys:
        peekaboo_press_sequence(pid, [key], delay_ms=delay_ms)
        time.sleep(menu_settle_seconds)


def apply_uilist_filter(
    pid: int,
    query: str,
    *,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    settle_seconds: float = 0.25,
) -> None:
    query = query.strip()
    if not query:
        raise SystemExit("UI list filter query cannot be empty")
    peekaboo_press_sequence(pid, ["/"], delay_ms=delay_ms)
    time.sleep(settle_seconds)
    peekaboo_type_text(pid, query, delay_ms=type_delay_ms)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(settle_seconds)


def fill_numeric_prompt(
    pid: int,
    value: int,
    *,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    clear_presses: int = 8,
    settle_seconds: float = 0.15,
) -> None:
    if value < 0:
        raise SystemExit("Numeric prompt value cannot be negative")
    peekaboo_press_sequence(pid, ["backspace"] * max(1, clear_presses), delay_ms=max(50, min(delay_ms, 150)))
    time.sleep(settle_seconds)
    peekaboo_type_text(pid, str(value), delay_ms=type_delay_ms)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(settle_seconds)


def debug_selection_key_moves_highlight(selection_key: str) -> bool:
    normalized = str(selection_key or "").strip().lower()
    return normalized in {
        "up",
        "down",
        "left",
        "right",
        "pageup",
        "pagedown",
        "home",
        "end",
    }


def debug_spawn_item(
    pid: int,
    *,
    item_query: str,
    count: int = 1,
    selection_key: str = "enter",
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.35,
    prompt_settle_seconds: float = 0.25,
    exit_menu: bool = True,
) -> None:
    if count <= 0:
        raise SystemExit("Item spawn count must be > 0")
    chosen_selection_key = str(selection_key or "enter").strip() or "enter"
    run_debug_menu_shortcut_path(
        pid,
        ["s", "w"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    apply_uilist_filter(
        pid,
        item_query,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    peekaboo_press_sequence(pid, [chosen_selection_key], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)

    if debug_selection_key_moves_highlight( chosen_selection_key ):
        peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)

    if count == 1:
        peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    else:
        fill_numeric_prompt(
            pid,
            count,
            delay_ms=delay_ms,
            type_delay_ms=type_delay_ms,
        )
    if exit_menu:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


def debug_spawn_monster(
    pid: int,
    *,
    monster_query: str,
    target_keys: Optional[List[str]] = None,
    group_radius: int = 0,
    friendly: bool = False,
    hallucination: bool = False,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.35,
    prompt_settle_seconds: float = 0.25,
    exit_menu: bool = True,
) -> None:
    if group_radius < 0:
        raise SystemExit("Monster group radius cannot be negative")
    run_debug_menu_shortcut_path(
        pid,
        ["s", "m"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    apply_uilist_filter(
        pid,
        monster_query,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    if friendly:
        peekaboo_press_sequence(pid, ["f"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    if hallucination:
        peekaboo_press_sequence(pid, ["h"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    if group_radius > 0:
        peekaboo_press_sequence(pid, ["i"] * group_radius, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if target_keys:
        peekaboo_press_sequence(pid, target_keys, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if exit_menu:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


def debug_spawn_follower_npc(
    pid: int,
    *,
    count: int = 1,
    delay_ms: int = 200,
    menu_settle_seconds: float = 0.35,
) -> None:
    if count <= 0:
        return
    for _ in range(count):
        run_debug_menu_shortcut_path(
            pid,
            ["s", "f"],
            delay_ms=delay_ms,
            menu_settle_seconds=menu_settle_seconds,
        )


def debug_force_temperature(
    pid: int,
    *,
    temperature_f: int,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.35,
    prompt_settle_seconds: float = 0.25,
) -> None:
    run_debug_menu_shortcut_path(
        pid,
        ["m", "T"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    # The Force temperature submenu currently lists Reset first and Set second.
    # Move to Set explicitly before confirming so an existing forced temperature
    # does not accidentally select Reset.
    peekaboo_press_sequence(pid, ["down"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    fill_numeric_prompt(
        pid,
        temperature_f,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )


def assign_nearby_npc_to_camp_dialog(
    pid: int,
    *,
    npc_selector: str,
    interaction_keys: Optional[List[str]] = None,
    lets_talk_key: str = "b",
    assignment_keys: Optional[List[str]] = None,
    exit_dialog_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    stage_settle_seconds: float = 0.8,
) -> None:
    interaction_path = interaction_keys or ["C", "t"]
    lets_talk_path = [lets_talk_key] if lets_talk_key.strip() else []
    assignment_path = assignment_keys or ["d", "n", "a"]
    exit_path = exit_dialog_keys or ["q", "c"]

    for keys in [
        interaction_path,
        [npc_selector],
        lets_talk_path,
        assignment_path,
        exit_path,
    ]:
        if not keys:
            continue
        peekaboo_press_sequence(pid, keys, delay_ms=delay_ms)
        time.sleep(stage_settle_seconds)


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


def read_log_delta(src: Path, start_size: int, *, filter_debug_noise: bool = False) -> str:
    if not src.exists():
        return ""
    size = src.stat().st_size
    if size <= start_size:
        return ""
    with src.open("r", encoding="utf-8", errors="replace") as fh:
        fh.seek(start_size)
        data = fh.read()
    if filter_debug_noise:
        return filter_debug_log_text(data)
    return data


def read_filtered_log_delta(src: Path, start_size: int) -> str:
    return read_log_delta(src, start_size, filter_debug_noise=True)


def copy_filtered_debug_log_delta_if_exists(src: Path, start_size: int, dst: Path) -> None:
    data = read_filtered_log_delta(src, start_size)
    if not data:
        return
    ensure_dir(dst.parent)
    dst.write_text(data, encoding="utf-8")


def parse_patrol_coords(raw: str) -> List[Dict[str, int]]:
    coords: List[Dict[str, int]] = []
    for x, y, z in PATROL_COORD_RE.findall(raw or ""):
        coords.append({"x": int(x), "y": int(y), "z": int(z)})
    return coords


def parse_patrol_clusters(raw: str) -> List[Dict[str, Any]]:
    clusters: List[Dict[str, Any]] = []
    for chunk in [part.strip() for part in str(raw or "").split(";") if part.strip()]:
        match = re.match(r"(?P<index>\d+)=\[(?P<body>.*)\]$", chunk)
        if not match:
            continue
        tiles = parse_patrol_coords(match.group("body"))
        clusters.append({
            "index": int(match.group("index")),
            "tile_count": len(tiles),
            "tiles": tiles,
        })
    return clusters


def summarize_patrol_artifacts(scenario_name: str, matched_lines: List[str]) -> Optional[Dict[str, Any]]:
    cache_summary: Optional[Dict[str, Any]] = None
    runtime_entries: List[Dict[str, Any]] = []

    for line in matched_lines:
        cache_match = PATROL_CACHE_RE.search(line)
        if cache_match and cache_summary is None:
            clusters = parse_patrol_clusters(cache_match.group("clusters"))
            cache_summary = {
                "camp": cache_match.group("camp"),
                "shift": cache_match.group("shift"),
                "workers": int(cache_match.group("workers")),
                "roster": int(cache_match.group("roster")),
                "active": int(cache_match.group("active")),
                "reserve": int(cache_match.group("reserve")),
                "cluster_count": len(clusters),
                "cluster_tile_count": sum(cluster["tile_count"] for cluster in clusters),
                "clusters": clusters,
                "raw_line": line,
            }
            continue

        runtime_match = PATROL_RUNTIME_RE.search(line)
        if runtime_match:
            route = parse_patrol_coords(runtime_match.group("route"))
            positions = parse_patrol_coords(runtime_match.group("pos"))
            targets = parse_patrol_coords(runtime_match.group("target"))
            runtime_entries.append({
                "worker": runtime_match.group("worker"),
                "behavior": runtime_match.group("behavior"),
                "position": positions[0] if positions else {},
                "target": targets[0] if targets else {},
                "route_length": len(route),
                "route": route,
                "raw_line": line,
            })

    if cache_summary is None and not runtime_entries:
        return None

    notes: List[str] = []
    if cache_summary is not None:
        workers = cache_summary["workers"]
        roster = cache_summary["roster"]
        active = cache_summary["active"]
        shift = cache_summary["shift"]
        reserve = cache_summary["reserve"]
        cluster_count = cache_summary["cluster_count"]
        cluster_tile_count = cache_summary["cluster_tile_count"]
        off_shift = max(0, workers - roster)
        singleton_clusters = cluster_count > 0 and all(cluster["tile_count"] == 1 for cluster in cache_summary["clusters"])

        if singleton_clusters:
            notes.append(
                f"This fixture paints {cluster_count} disconnected one-tile patrol posts, so a lone active guard must circulate instead of occupying them all at once."
            )
        elif cluster_count == 1 and cluster_tile_count > 1:
            notes.append(
                f"This fixture paints one connected patrol cluster with {cluster_tile_count} patrol tiles, so multiple active guards can hold distinct squares inside the same cluster."
            )

        if off_shift > 0:
            notes.append(
                f"{workers} patrol-enabled worker(s) exist, but only {roster} are rostered on the {shift} shift, so {off_shift} remain off-shift before reserve is even counted."
            )
        else:
            notes.append(
                f"All {workers} patrol-enabled worker(s) are rostered on the {shift} shift for this packet."
            )

        if reserve > 0:
            notes.append(
                f"{reserve} rostered guard(s) are in reserve for the current shift, which is why they are not expected to occupy a patrol tile unless an active guard drops out."
            )

        if active < cluster_count:
            notes.append(
                f"Only {active} active guard(s) cover {cluster_count} patrol post/group(s), so uncovered posts are expected between visits in this snapshot."
            )

    loop_workers = [entry for entry in runtime_entries if entry.get("behavior") == "loop"]
    if loop_workers:
        unique_loop_names = list(dict.fromkeys(entry["worker"] for entry in loop_workers))
        loop_names = ", ".join(unique_loop_names)
        loop_verb = "walks" if len(unique_loop_names) == 1 else "walk"
        longest_route = max(entry.get("route_length", 0) for entry in loop_workers)
        if longest_route > 0:
            notes.append(
                f"Loop behavior is live here: {loop_names} {loop_verb} a fixed {longest_route}-tile patrol route, so the paired runtime crops should show motion one dwell later."
            )
        else:
            notes.append(
                f"Loop behavior is live here: {loop_names} should move between the paired runtime crops instead of holding a single square."
            )

    hold_workers = [entry for entry in runtime_entries if entry.get("behavior") == "hold"]
    if hold_workers:
        unique_hold_names = list(dict.fromkeys(entry["worker"] for entry in hold_workers))
        hold_names = ", ".join(unique_hold_names)
        hold_verb = "keeps" if len(unique_hold_names) == 1 else "keep"
        notes.append(
            f"Hold behavior is live here: {hold_names} {hold_verb} their assigned patrol squares, so the paired runtime crops should stay visually unchanged apart from UI noise."
        )

    return {
        "kind": "camp_patrol_summary",
        "scenario": scenario_name,
        "cache": cache_summary,
        "runtime_workers": runtime_entries,
        "player_legibility_notes": notes,
    }


def write_patrol_summary(run_dir: Path, patrol_summary: Dict[str, Any]) -> Dict[str, Any]:
    cache_summary = patrol_summary.get("cache") or {}
    runtime_workers = patrol_summary.get("runtime_workers") or []
    lines = [
        f"Patrol packet explainer: {patrol_summary.get('scenario', '')}",
        "",
    ]

    if cache_summary:
        shift = cache_summary.get("shift", "?")
        workers = int(cache_summary.get("workers", 0))
        roster = int(cache_summary.get("roster", 0))
        active = int(cache_summary.get("active", 0))
        reserve = int(cache_summary.get("reserve", 0))
        off_shift = max(0, workers - roster)
        cluster_count = int(cache_summary.get("cluster_count", 0))
        cluster_tile_count = int(cache_summary.get("cluster_tile_count", 0))
        lines.extend([
            f"- shift: {shift}",
            f"- patrol-enabled workers: {workers}",
            f"- this-shift roster: {roster} total = {active} active + {reserve} reserve + {off_shift} off-shift",
            f"- patrol layout: {cluster_count} cluster(s) across {cluster_tile_count} painted patrol tile(s)",
            "",
        ])

    if runtime_workers:
        lines.append("Runtime workers:")
        for entry in runtime_workers:
            target = entry.get("target") or {}
            lines.append(
                f"- {entry.get('worker', '?')}: {entry.get('behavior', '?')} target=({target.get('x', '?')},{target.get('y', '?')},{target.get('z', '?')}) route_len={entry.get('route_length', 0)}"
            )
        lines.append("")

    lines.append("Read this packet as:")
    for note in patrol_summary.get("player_legibility_notes", []):
        lines.append(f"- {note}")

    summary_path = run_dir / "probe.patrol_summary.txt"
    summary_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    return {
        "kind": "camp_patrol_summary",
        "path": str(summary_path),
        "summary": patrol_summary,
    }


def resolve_artifact_source(profile: str, source: str) -> Tuple[Path, bool, str]:
    normalized = source.strip().lower() if source else "debug.log"
    if normalized in {"debug", "debug.log"}:
        return config_dir_for_profile(profile) / "debug.log", True, "debug.log"
    if normalized in {"llm", "llm_intent", "llm_intent.log"}:
        return repo_root() / "config" / "llm_intent.log", False, "llm_intent.log"
    raise SystemExit(f"Unsupported artifact source: {source}")


def capture_debug_delta(profile: str, start_size: int, run_dir: Path, serial: int) -> int:
    debug_log = config_dir_for_profile(profile) / "debug.log"
    if not debug_log.exists():
        return start_size
    size = debug_log.stat().st_size
    data = read_filtered_log_delta(debug_log, start_size)
    if not data:
        return start_size
    out = run_dir / f"debug_delta_{serial:02d}.log"
    out.write_text(data, encoding="utf-8")
    return size


def extract_window_build_info(window_title: str) -> Dict[str, Any]:
    info: Dict[str, Any] = {
        "window_title": window_title,
        "captured_head": "",
        "captured_dirty": False,
    }
    match = re.search(r" - ([0-9a-f]{10})(-dirty)?$", window_title or "")
    if not match:
        return info
    info["captured_head"] = match.group(1)
    info["captured_dirty"] = bool(match.group(2))
    return info


def window_area(window: Dict[str, Any]) -> int:
    bounds = window.get("bounds")
    if not isinstance(bounds, list) or len(bounds) != 2:
        return 0
    top_left, size = bounds
    if not isinstance(top_left, list) or not isinstance(size, list) or len(size) != 2:
        return 0
    try:
        width = max(0, int(size[0]))
        height = max(0, int(size[1]))
    except (TypeError, ValueError):
        return 0
    return width * height


def list_windows_for_pid(pid: int) -> List[Dict[str, Any]]:
    payload = run_json(["peekaboo", "list", "windows", "--json", "--app", f"PID:{pid}"], check=False)
    if not isinstance(payload, dict):
        return []
    data = payload.get("data", {}) if isinstance(payload.get("data"), dict) else {}
    windows = data.get("windows", [])
    if not isinstance(windows, list):
        return []
    return [window for window in windows if isinstance(window, dict)]


def choose_capture_window(pid: int) -> Dict[str, Any]:
    windows = list_windows_for_pid(pid)
    if not windows:
        return {}

    def capture_rank(window: Dict[str, Any]) -> Tuple[int, int, int, int, int]:
        title = str(window.get("title", "")).strip()
        return (
            1 if bool(window.get("isOnScreen", False)) else 0,
            1 if not bool(window.get("isMinimized", False)) else 0,
            1 if title else 0,
            1 if "cataclysm" in title.lower() else 0,
            window_area(window),
        )

    return max(windows, key=capture_rank)


def summarize_peekaboo_image_capture(
    stdout: str,
    png_path: Path,
    json_path: Path,
    capture_target: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    repo_head = current_head_short()
    summary: Dict[str, Any] = {
        "png_path": str(png_path),
        "peekaboo_json": str(json_path),
        "repo_head": repo_head,
        "peekaboo_success": False,
        "window_title": "",
        "window_id": 0,
        "window_index": None,
        "captured_head": "",
        "captured_dirty": False,
        "version_matches_repo_head": None,
        "version_matches_runtime_paths": None,
        "runtime_relevant_paths": list(RUNTIME_RELEVANT_PATHS),
        "runtime_relevant_diff_since_capture": [],
        "runtime_compare_error": "",
        "parse_error": "",
    }
    if not stdout.strip():
        summary["parse_error"] = "peekaboo produced no JSON stdout"
        return summary
    try:
        payload = json.loads(stdout)
    except Exception as exc:
        summary["parse_error"] = str(exc)
        return summary
    if not isinstance(payload, dict):
        summary["parse_error"] = "peekaboo stdout was not a JSON object"
        return summary

    data = payload.get("data", {}) if isinstance(payload.get("data"), dict) else {}
    files = data.get("files", []) if isinstance(data.get("files"), list) else []
    first_file = files[0] if files and isinstance(files[0], dict) else {}
    summary["peekaboo_success"] = bool(payload.get("success"))
    summary["window_title"] = str(
        first_file.get("window_title")
        or (capture_target or {}).get("title", "")
    ).strip()
    try:
        summary["window_id"] = int(first_file.get("window_id") or (capture_target or {}).get("window_id") or 0)
    except (TypeError, ValueError):
        summary["window_id"] = 0
    summary["window_index"] = first_file.get("window_index", (capture_target or {}).get("index"))
    error_info = payload.get("error", {}) if isinstance(payload.get("error"), dict) else {}
    if not summary["peekaboo_success"] and error_info.get("message"):
        summary["parse_error"] = str(error_info.get("message", "")).strip()
    build_info = extract_window_build_info(summary["window_title"])
    summary.update(build_info)
    if summary["captured_head"]:
        captured_head = str(summary["captured_head"])
        summary["version_matches_repo_head"] = captured_head == repo_head
        if summary["version_matches_repo_head"]:
            summary["version_matches_runtime_paths"] = True
        else:
            runtime_changes, runtime_error = runtime_relevant_changes_since(captured_head)
            summary["runtime_relevant_diff_since_capture"] = runtime_changes
            summary["runtime_compare_error"] = runtime_error
            if not runtime_error:
                summary["version_matches_runtime_paths"] = not runtime_changes
    return summary


def normalize_capture_crop(raw_crop: Any) -> Optional[Dict[str, int]]:
    if not isinstance(raw_crop, dict):
        return None
    try:
        height = int(raw_crop.get("height", raw_crop.get("pixels_h", 0)) or 0)
        width = int(raw_crop.get("width", raw_crop.get("pixels_w", 0)) or 0)
        offset_y = int(raw_crop.get("offset_y", raw_crop.get("top", raw_crop.get("y", 0))) or 0)
        offset_x = int(raw_crop.get("offset_x", raw_crop.get("left", raw_crop.get("x", 0))) or 0)
    except (TypeError, ValueError):
        return None
    if height <= 0 or width <= 0 or offset_y < 0 or offset_x < 0:
        return None
    return {
        "height": height,
        "width": width,
        "offset_y": offset_y,
        "offset_x": offset_x,
    }


def crop_png_with_sips(source_path: Path, output_path: Path, crop: Dict[str, int]) -> Dict[str, Any]:
    cmd = [
        "sips",
        "-c",
        str(crop["height"]),
        str(crop["width"]),
        "--cropOffset",
        str(crop["offset_y"]),
        str(crop["offset_x"]),
        str(source_path),
        "--out",
        str(output_path),
    ]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=False,
    )
    return {
        "ok": proc.returncode == 0,
        "command": cmd,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "source_png_path": str(source_path),
        "output_png_path": str(output_path),
        "crop": dict(crop),
    }


def capture_screenshot(
    pid: int,
    run_dir: Path,
    label: str,
    crop: Optional[Dict[str, int]] = None,
) -> Dict[str, Any]:
    png_path = run_dir / f"{label}.png"
    json_path = run_dir / f"{label}.peekaboo.json"
    capture_target = choose_capture_window(pid)
    cmd = ["peekaboo", "image", "--json", "--path", str(png_path)]
    if capture_target.get("window_id"):
        cmd.extend(["--window-id", str(capture_target["window_id"])])
    else:
        cmd.extend(["--pid", str(pid)])
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=False,
    )
    crop_request = normalize_capture_crop(crop)
    crop_report: Optional[Dict[str, Any]] = None
    if proc.returncode == 0 and crop_request:
        full_png_path = run_dir / f"{label}.full.png"
        shutil.copy2(png_path, full_png_path)
        crop_report = crop_png_with_sips(full_png_path, png_path, crop_request)
        if not crop_report.get("ok"):
            shutil.copy2(full_png_path, png_path)
    screen_summary = summarize_peekaboo_image_capture(
        proc.stdout,
        png_path,
        json_path,
        capture_target=capture_target,
    )
    if crop_report:
        screen_summary["crop_requested"] = crop_request
        screen_summary["crop_applied"] = bool(crop_report.get("ok"))
        screen_summary["source_png_path"] = str(crop_report.get("source_png_path", ""))
        if not crop_report.get("ok"):
            screen_summary["crop_error"] = str(crop_report.get("stderr", "")).strip()
    payload = {
        "label": label,
        "command": cmd,
        "capture_target": capture_target,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "crop": crop_report,
        "screen_summary": screen_summary,
    }
    json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return payload


def run_screen_ocr(image_path: Path) -> Dict[str, Any]:
    script_path = repo_root() / "tools" / "openclaw_harness" / "ocr_image.swift"
    if not script_path.exists():
        return {
            "ok": False,
            "error": f"OCR helper script not found: {script_path}",
            "image_path": str(image_path),
        }

    swift_cmd = shutil.which("swift")
    if not swift_cmd:
        return {
            "ok": False,
            "error": "swift executable not found in PATH",
            "image_path": str(image_path),
        }

    proc = subprocess.run(
        [swift_cmd, str(script_path), "--image", str(image_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        return {
            "ok": False,
            "error": (proc.stderr or proc.stdout).strip() or f"swift exited with {proc.returncode}",
            "image_path": str(image_path),
        }

    try:
        payload = json.loads(proc.stdout)
    except Exception as exc:
        return {
            "ok": False,
            "error": f"failed to parse OCR JSON: {exc}",
            "image_path": str(image_path),
            "stdout": proc.stdout,
        }

    if not isinstance(payload, dict):
        return {
            "ok": False,
            "error": "OCR helper did not return a JSON object",
            "image_path": str(image_path),
        }
    return payload


def capture_screen_text_artifact(
    run_dir: Path,
    label: str,
    capture: Dict[str, Any],
    tail_lines: int = 8,
) -> Dict[str, Any]:
    screen_summary = capture.get("screen_summary", {}) if isinstance(capture.get("screen_summary"), dict) else {}
    png_path_value = str(screen_summary.get("png_path", "")).strip()
    if not png_path_value:
        return {
            "kind": "screen_text_capture",
            "label": label,
            "ok": False,
            "error": "capture did not include a png_path",
        }

    png_path = Path(png_path_value)
    ocr_payload = run_screen_ocr(png_path)
    text_json_path = run_dir / f"{label}.screen_text.json"
    text_txt_path = run_dir / f"{label}.screen_text.txt"
    text_json_path.write_text(json.dumps(ocr_payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    raw_lines = ocr_payload.get("lines", []) if isinstance(ocr_payload, dict) else []
    if isinstance(raw_lines, list):
        lines = [str(line).strip() for line in raw_lines if str(line).strip()]
    else:
        lines = []
    tail = lines[-max(1, int(tail_lines)):] if lines else []
    text_txt_path.write_text("\n".join(tail).rstrip() + ("\n" if tail else ""), encoding="utf-8")

    result = {
        "kind": "screen_text_capture",
        "label": label,
        "ok": bool(ocr_payload.get("ok")),
        "source_png": str(png_path),
        "json_path": str(text_json_path),
        "text_path": str(text_txt_path),
        "line_count": len(lines),
        "tail_line_count": len(tail),
        "tail_lines": tail,
    }
    if not ocr_payload.get("ok"):
        result["error"] = str(ocr_payload.get("error", "OCR failed"))
    return result


def normalize_screen_text_patterns(raw_value: Any) -> List[str]:
    if isinstance(raw_value, str):
        return [raw_value.strip()] if raw_value.strip() else []
    if not isinstance(raw_value, list):
        return []
    return [str(value).strip() for value in raw_value if str(value).strip()]


def find_screen_text_matches(screen_text_report: Dict[str, Any], patterns: List[str]) -> List[Dict[str, str]]:
    if not patterns:
        return []
    json_path_value = str(screen_text_report.get("json_path", "")).strip()
    if not json_path_value:
        return []
    try:
        payload = json.loads(Path(json_path_value).read_text(encoding="utf-8"))
    except Exception:
        return []

    raw_lines = payload.get("lines", []) if isinstance(payload, dict) else []
    lines = [str(line).strip() for line in raw_lines if str(line).strip()] if isinstance(raw_lines, list) else []
    raw_text = str(payload.get("text", "")).strip() if isinstance(payload, dict) else ""
    haystacks = lines + ([raw_text] if raw_text else [])
    matches: List[Dict[str, str]] = []
    for pattern in patterns:
        needle = pattern.lower()
        for line in haystacks:
            if needle in line.lower():
                matches.append({"pattern": pattern, "line": line})
                break
    return matches


def apply_screen_text_abort_guard(
    report: Dict[str, Any],
    step: Dict[str, Any],
    screen_text_report: Dict[str, Any],
) -> bool:
    patterns = normalize_screen_text_patterns(step.get("abort_if_text_contains", []))
    matches = find_screen_text_matches(screen_text_report, patterns)
    if not matches:
        return False
    report["abort"] = {
        "status": "aborted_by_screen_text_guard",
        "verdict": str(step.get("abort_verdict", "inconclusive_screen_text_guard")),
        "reason": str(step.get("abort_reason", "screen text matched abort guard")),
        "patterns": patterns,
        "matches": matches,
    }
    return True


def evaluate_screen_text_expectation(
    screen_text_report: Dict[str, Any],
    patterns: List[str],
) -> Dict[str, Any]:
    matches = find_screen_text_matches(screen_text_report, patterns)
    matched_patterns = {str(match.get("pattern", "")) for match in matches}
    missing = [pattern for pattern in patterns if pattern not in matched_patterns]
    return {
        "status": "matched" if patterns and not missing else ("not_requested" if not patterns else "missing"),
        "patterns": patterns,
        "matches": matches,
        "missing": missing,
        "source": str(screen_text_report.get("json_path", "") or screen_text_report.get("source_png", "")),
    }


def screen_summary_path(screen_summary: Dict[str, Any]) -> str:
    return str(screen_summary.get("png_path", "") or screen_summary.get("path", ""))


def screen_checkpoint_verdict(
    *,
    screen_summary: Dict[str, Any],
    expected_visible_fact: str,
    text_expectation: Optional[Dict[str, Any]] = None,
    ocr_requested: bool = False,
) -> Tuple[str, List[str]]:
    issues: List[str] = []
    if not screen_summary:
        return "yellow_step_no_immediate_screen_or_metadata", ["no_screen_or_metadata_artifact"]
    if not screen_summary.get("peekaboo_success"):
        return "red_step_screen_capture_failed", ["screen_capture_failed"]
    if screen_summary.get("version_matches_runtime_paths") is False:
        issues.append("runtime_version_mismatch")

    if not expected_visible_fact:
        issues.append("missing_expected_visible_fact")
    if ocr_requested and text_expectation is None:
        issues.append("ocr_without_expected_text_guard")
    if text_expectation is not None:
        if text_expectation.get("status") == "missing":
            return "red_step_expected_screen_text_missing", issues + ["expected_screen_text_missing"]
        if text_expectation.get("status") == "matched":
            if issues:
                return "yellow_step_screen_text_matched_with_caveats", issues
            return "green_step_screen_text_guarded", issues

    if issues:
        return "yellow_step_screen_checkpoint_caveated", issues
    return "green_step_screen_checkpoint_named", issues


def auto_step_action_description(report: Dict[str, Any]) -> str:
    kind = str(report.get("kind", "")).strip()
    if kind == "press":
        return "peekaboo press sequence " + json.dumps(report.get("keys", []), ensure_ascii=False)
    if kind == "type":
        return "peekaboo type text" + (" and submit" if report.get("submit") else "")
    if kind == "capture":
        return "capture screenshot / optional OCR"
    if kind in {"long_wait", "wait_action"}:
        return "open bounded wait menu and select duration"
    if kind == "advance_turns":
        return f"send one-turn wait key {int(report.get('count', 0) or 0)} time(s)"
    if kind == "wait":
        return f"sleep wall-clock {report.get('seconds', '')} second(s)"
    if kind.startswith("debug_"):
        return f"debug helper primitive {kind}"
    return kind or "scenario step"


def build_probe_step_ledger(step_reports: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    ledger: List[Dict[str, Any]] = []
    for report in step_reports:
        label = str(report.get("label", "")).strip()
        kind = str(report.get("kind", "")).strip().lower()
        expected_visible_fact = str(report.get("expected_visible_fact", "")).strip()
        screen_summary: Dict[str, Any] = {}
        text_expectation: Optional[Dict[str, Any]] = None
        ocr_requested = False

        if isinstance(report.get("screen_after"), dict):
            screen_summary = report["screen_after"]
            if isinstance(report.get("screen_after_text_expectation"), dict):
                text_expectation = report["screen_after_text_expectation"]
            ocr_requested = isinstance(report.get("screen_after_text"), dict)
        elif isinstance(report.get("screen"), dict):
            screen_summary = report["screen"]
            if isinstance(report.get("screen_text_expectation"), dict):
                text_expectation = report["screen_text_expectation"]
            ocr_requested = isinstance(report.get("screen_text"), dict)

        issues: List[str] = []
        if isinstance(report.get("abort"), dict):
            verdict = "red_step_aborted_by_screen_text_guard"
            issues.append("screen_text_abort_guard")
        elif kind in {"long_wait", "wait_action"} and isinstance(report.get("wait_step_ledger"), dict):
            wait_verdict = str(report["wait_step_ledger"].get("verdict", ""))
            if wait_verdict.startswith("green"):
                verdict = "green_wait_step_proven"
            elif wait_verdict.startswith("blocked"):
                verdict = "blocked_wait_step_unproven"
            else:
                verdict = "yellow_wait_step_unproven"
            issues.extend(str(issue) for issue in report["wait_step_ledger"].get("issues", []))
        else:
            verdict, issues = screen_checkpoint_verdict(
                screen_summary=screen_summary,
                expected_visible_fact=expected_visible_fact,
                text_expectation=text_expectation,
                ocr_requested=ocr_requested,
            )

        evidence_artifact = screen_summary_path(screen_summary)
        if not evidence_artifact and kind in {"long_wait", "wait_action"}:
            evidence_artifact = str(report.get("wait_step_ledger", {}).get("wait_menu_artifact", ""))
        if not evidence_artifact:
            evidence_artifact = "probe.report.json:steps"

        ledger.append({
            "index": report.get("index"),
            "primitive_step": label,
            "kind": kind,
            "preconditions": str(report.get("preconditions", "previous scenario gate allowed this step; profile/world/fixture from probe contract")),
            "action": str(report.get("action_description", "") or auto_step_action_description(report)),
            "expected_immediate_state": str(report.get("expected_immediate_state", "") or expected_visible_fact or "step-local state must be observed before this can support feature proof"),
            "evidence_artifact": evidence_artifact,
            "expected_visible_fact": expected_visible_fact,
            "screen_text_expectation": text_expectation or {},
            "failure_rule": str(report.get("failure_rule", "missing/wrong screen, failed OCR guard, stale runtime, or absent state metadata makes this non-green")),
            "next_step_gate": str(report.get("next_step_gate", "next step may run mechanically, but feature closure requires this ledger row to be green")),
            "issues": issues,
            "verdict": verdict,
        })
    return ledger


def summarize_probe_step_ledger(step_ledger: List[Dict[str, Any]]) -> Dict[str, Any]:
    verdicts = [str(row.get("verdict", "")) for row in step_ledger]
    red_or_blocked = [row for row in step_ledger if str(row.get("verdict", "")).startswith(("red", "blocked"))]
    yellow = [row for row in step_ledger if str(row.get("verdict", "")).startswith("yellow")]
    green = [row for row in step_ledger if str(row.get("verdict", "")).startswith("green")]
    if red_or_blocked:
        status = "red_step_local_proof_failed"
    elif yellow:
        status = "yellow_step_local_proof_incomplete"
    elif step_ledger and len(green) == len(step_ledger):
        status = "green_step_local_proof"
    else:
        status = "no_feature_steps"
    return {
        "status": status,
        "count": len(step_ledger),
        "green_count": len(green),
        "yellow_count": len(yellow),
        "red_or_blocked_count": len(red_or_blocked),
        "verdicts": verdicts,
        "non_green_steps": [
            {
                "label": str(row.get("primitive_step", "")),
                "verdict": str(row.get("verdict", "")),
                "issues": row.get("issues", []),
            }
            for row in step_ledger if not str(row.get("verdict", "")).startswith("green")
        ],
        "review_rule": "Artifact matches are not feature proof unless every scenario step has green step-local proof.",
    }


def render_blink_compare(
    run_dir: Path,
    label: str,
    frame_names: List[str],
    *,
    scale: int = 1,
    frame_delay_ms: int = 450,
) -> Dict[str, Any]:
    frame_paths = [(run_dir / str(name)).resolve() for name in frame_names]
    missing_inputs = [str(path) for path in frame_paths if not path.exists()]
    ffmpeg_path = shutil.which("ffmpeg")
    output_path = run_dir / f"{label}.gif"
    concat_path = run_dir / f"{label}.ffconcat"
    report: Dict[str, Any] = {
        "kind": "blink_compare",
        "label": label,
        "inputs": [str(path) for path in frame_paths],
        "output_path": str(output_path),
        "scale": max(1, int(scale)),
        "frame_delay_ms": max(50, int(frame_delay_ms)),
        "ffconcat_path": str(concat_path),
    }
    if missing_inputs:
        report.update({
            "ok": False,
            "reason": "missing_inputs",
            "missing_inputs": missing_inputs,
        })
        return report
    if not ffmpeg_path:
        report.update({
            "ok": False,
            "reason": "missing_ffmpeg",
        })
        return report

    frame_delay_seconds = report["frame_delay_ms"] / 1000.0
    concat_lines = ["ffconcat version 1.0"]
    for frame_path in frame_paths:
        concat_lines.append(f"file '{frame_path}'")
        concat_lines.append(f"duration {frame_delay_seconds:.3f}")
    concat_lines.append(f"file '{frame_paths[-1]}'")
    concat_path.write_text("\n".join(concat_lines) + "\n", encoding="utf-8")

    scale_filter = "null"
    if report["scale"] > 1:
        scale_filter = f"scale=iw*{report['scale']}:ih*{report['scale']}:flags=neighbor"
    filter_complex = (
        f"[0:v]{scale_filter},split[s0][s1];"
        "[s0]palettegen=stats_mode=diff[p];"
        "[s1][p]paletteuse"
    )
    cmd = [
        ffmpeg_path,
        "-y",
        "-f",
        "concat",
        "-safe",
        "0",
        "-i",
        str(concat_path),
        "-vsync",
        "vfr",
        "-filter_complex",
        filter_complex,
        "-loop",
        "0",
        str(output_path),
    ]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=False,
    )
    report.update({
        "ok": proc.returncode == 0,
        "command": cmd,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    })
    if proc.returncode != 0 and output_path.exists():
        output_path.unlink()
    return report


def render_derived_screens(run_dir: Path, specs: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    reports: List[Dict[str, Any]] = []
    for index, spec in enumerate(specs, start=1):
        if not isinstance(spec, dict):
            reports.append({
                "ok": False,
                "kind": "invalid",
                "label": f"derived_screen_{index:02d}",
                "reason": "spec_not_object",
            })
            continue
        kind = str(spec.get("kind", "")).strip().lower()
        label = str(spec.get("label", f"derived_screen_{index:02d}")).strip() or f"derived_screen_{index:02d}"
        if kind == "blink_compare":
            raw_frames = spec.get("frames", [])
            frame_names = [str(frame).strip() for frame in raw_frames if str(frame).strip()] if isinstance(raw_frames, list) else []
            reports.append(
                render_blink_compare(
                    run_dir,
                    label,
                    frame_names,
                    scale=int(spec.get("scale", 1) or 1),
                    frame_delay_ms=int(spec.get("frame_delay_ms", 450) or 450),
                )
            )
            continue
        reports.append({
            "ok": False,
            "kind": kind or "unknown",
            "label": label,
            "reason": "unsupported_kind",
        })
    return reports


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


def pid_command(pid: int) -> str:
    proc = subprocess.run(["ps", "-p", str(pid), "-o", "command="], capture_output=True, text=True, check=False)
    return proc.stdout.strip()


def wait_for_pid_exit(pid: int, timeout_seconds: float) -> bool:
    deadline = time.monotonic() + max(timeout_seconds, 0.0)
    while time.monotonic() < deadline:
        if not pid_command(pid):
            return True
        time.sleep(0.1)
    return not pid_command(pid)


def cleanup_game_process(pid: int, *, grace_seconds: float = 2.0) -> Dict[str, Any]:
    info: Dict[str, Any] = {
        "pid": pid,
        "grace_seconds": grace_seconds,
    }
    if pid <= 0:
        info["status"] = "invalid_pid"
        return info

    command = pid_command(pid)
    info["command"] = command
    if not command:
        info["status"] = "already_exited"
        return info
    if not re.search(r"cataclysm-(tiles|tlg-tiles)", command):
        info["status"] = "skipped_non_cataclysm_process"
        return info

    try:
        os.kill(pid, signal.SIGTERM)
        info["signal"] = "SIGTERM"
    except ProcessLookupError:
        info["status"] = "already_exited"
        return info
    except PermissionError:
        info["status"] = "permission_denied"
        return info

    if wait_for_pid_exit(pid, grace_seconds):
        info["status"] = "terminated"
        return info

    try:
        os.kill(pid, signal.SIGKILL)
        info["signal"] = "SIGKILL"
    except ProcessLookupError:
        info["status"] = "terminated_during_kill_escalation"
        return info
    except PermissionError:
        info["status"] = "permission_denied_on_kill"
        return info

    info["status"] = "killed" if wait_for_pid_exit(pid, 1.0) else "still_running_after_kill"
    return info


def load_fixture_manifest(path: Path) -> Dict[str, Any]:
    manifest_path = path / "manifest.json"
    if not manifest_path.exists():
        return {}
    return json.loads(manifest_path.read_text(encoding="utf-8"))


def normalize_fixture_save_transforms(raw_value: Any, *, manifest_path: Path) -> List[Dict[str, Any]]:
    if raw_value in (None, ""):
        return []
    if not isinstance(raw_value, list):
        raise SystemExit(f"Fixture save_transforms must be a list: {manifest_path}")

    transforms: List[Dict[str, Any]] = []
    for index, raw in enumerate(raw_value, start=1):
        if not isinstance(raw, dict):
            raise SystemExit(f"Fixture save_transforms[{index}] must be an object: {manifest_path}")
        kind = str(raw.get("kind", "")).strip().lower()
        player_save = str(raw.get("player_save", "")).strip()

        if kind == "bandit_active_sortie_clock":
            site_id = str(raw.get("site_id", "")).strip()
            active_job_type = str(raw.get("active_job_type", "scout")).strip() or "scout"
            try:
                started_minutes = int(raw.get("active_sortie_started_minutes", 0))
                local_contact_minutes = int(raw.get("active_sortie_local_contact_minutes", started_minutes))
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs integer active sortie minutes in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "site_id": site_id,
                "active_job_type": active_job_type,
                "active_sortie_started_minutes": started_minutes,
                "active_sortie_local_contact_minutes": local_contact_minutes,
            })
            continue

        if not player_save:
            raise SystemExit(f"Fixture save_transforms[{index}] needs player_save in {manifest_path}")

        if kind == "player_mutations":
            mutations = normalize_string_list(raw.get("mutations", []))
            if not mutations:
                raise SystemExit(f"Fixture save_transforms[{index}] needs mutations in {manifest_path}")
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "mutations": mutations,
            })
            continue

        if kind == "player_items":
            items_raw = raw.get("items", [])
            if not isinstance(items_raw, list) or not items_raw:
                raise SystemExit(f"Fixture save_transforms[{index}] needs non-empty items in {manifest_path}")
            items: List[Dict[str, Any]] = []
            for item_index, item_raw in enumerate(items_raw, start=1):
                if not isinstance(item_raw, dict):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] must be an object in {manifest_path}"
                    )
                typeid = str(item_raw.get("typeid", "")).strip()
                if not typeid:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] needs typeid in {manifest_path}"
                    )
                try:
                    count = int(item_raw.get("count", 1) or 1)
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] count must be integer in {manifest_path}"
                    )
                if count <= 0:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] count must be > 0 in {manifest_path}"
                    )
                item = {"typeid": typeid, "count": count}
                if "charges" in item_raw:
                    try:
                        item["charges"] = int(item_raw.get("charges"))
                    except (TypeError, ValueError):
                        raise SystemExit(
                            f"Fixture save_transforms[{index}].items[{item_index}] charges must be integer in {manifest_path}"
                        )
                if "contents" in item_raw:
                    contents = item_raw.get("contents")
                    if not isinstance(contents, dict):
                        raise SystemExit(
                            f"Fixture save_transforms[{index}].items[{item_index}] contents must be object in {manifest_path}"
                        )
                    item["contents"] = contents
                items.append(item)
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "items": items,
            })
            continue

        if kind == "player_near_overmap_special":
            special_id = str(raw.get("special_id", "")).strip()
            if not special_id:
                raise SystemExit(f"Fixture save_transforms[{index}] needs special_id in {manifest_path}")
            try:
                site_index = int(raw.get("site_index", 1) or 1)
            except (TypeError, ValueError):
                raise SystemExit(f"Fixture save_transforms[{index}] needs integer site_index in {manifest_path}")
            offset_raw = raw.get("offset_omt", [])
            if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs offset_omt=[x,y,z] in {manifest_path}"
                )
            try:
                offset_omt = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] offset_omt values must be integers in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "special_id": special_id,
                "site_index": site_index,
                "offset_omt": offset_omt,
            })
            continue

        if kind == "seed_overmap_special_near_player":
            special_id = str(raw.get("special_id", "")).strip()
            if not special_id:
                raise SystemExit(f"Fixture save_transforms[{index}] needs special_id in {manifest_path}")
            try:
                site_index = int(raw.get("site_index", 1) or 1)
            except (TypeError, ValueError):
                raise SystemExit(f"Fixture save_transforms[{index}] needs integer site_index in {manifest_path}")
            if site_index <= 0:
                raise SystemExit(f"Fixture save_transforms[{index}] needs site_index >= 1 in {manifest_path}")
            offset_raw = raw.get("offset_omt", [])
            if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs offset_omt=[x,y,z] in {manifest_path}"
                )
            try:
                offset_omt = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] offset_omt values must be integers in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "special_id": special_id,
                "site_index": site_index,
                "offset_omt": offset_omt,
            })
            continue

        if kind == "map_fields_near_player":
            fields_raw = raw.get("fields", [])
            if not isinstance(fields_raw, list) or not fields_raw:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs non-empty fields list in {manifest_path}"
                )
            fields: List[Dict[str, Any]] = []
            for field_index, field_raw in enumerate(fields_raw, start=1):
                if not isinstance(field_raw, dict):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].fields[{field_index}] must be an object in {manifest_path}"
                    )
                field_id = str(field_raw.get("field_id", "")).strip()
                if not field_id:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].fields[{field_index}] needs field_id in {manifest_path}"
                    )
                offset_raw = field_raw.get("offset_ms", [0, 0, 0])
                if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].fields[{field_index}] needs offset_ms=[x,y,z] in {manifest_path}"
                    )
                try:
                    offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
                    intensity = int(field_raw.get("intensity", 1))
                    age_turns = int(field_raw.get("age_turns", 0))
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].fields[{field_index}] has non-integer offset/intensity/age in {manifest_path}"
                    )
                fields.append({
                    "field_id": field_id,
                    "offset_ms": offset_ms,
                    "intensity": intensity,
                    "age_turns": age_turns,
                })
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "fields": fields,
            })
            continue

        if kind == "game_turn":
            try:
                turn = int(raw.get("turn"))
            except (TypeError, ValueError):
                raise SystemExit(f"Fixture save_transforms[{index}] needs integer turn in {manifest_path}")
            if turn < 0:
                raise SystemExit(f"Fixture save_transforms[{index}] needs turn >= 0 in {manifest_path}")
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "turn": turn,
            })
            continue

        raise SystemExit(
            f"Unsupported fixture save_transforms[{index}].kind '{kind}' in {manifest_path}; "
            "supported kinds: player_mutations, player_items, player_near_overmap_special, "
            "seed_overmap_special_near_player, map_fields_near_player, game_turn, "
            "bandit_active_sortie_clock"
        )
    return transforms


def zzip_binary() -> Path:
    path = repo_root() / "zzip"
    if not path.exists():
        raise SystemExit(f"Required repo zzip helper not found: {path}")
    return path


def run_zzip(path: Path) -> None:
    proc = subprocess.run(
        [str(zzip_binary()), str(path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise SystemExit(
            f"zzip failed for {path}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )


def apply_player_mutations_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save = world_dir / str(transform.get("player_save", "")).strip()
    if not player_save.exists():
        raise SystemExit(f"Fixture player-mutation transform target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture player-mutation transform expects .zzip save path: {player_save}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture player-mutation transform did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")

    traits_raw = player.get("traits", [])
    if isinstance(traits_raw, list):
        traits = [str(value) for value in traits_raw]
    else:
        raise SystemExit(f"Player traits is not a list in {extracted_save}")

    mutations_raw = player.get("mutations", {})
    if isinstance(mutations_raw, dict):
        mutations = dict(mutations_raw)
    elif isinstance(mutations_raw, list) and not mutations_raw:
        mutations = {}
    else:
        raise SystemExit(f"Player mutations is not a supported container in {extracted_save}")

    cached_raw = player.get("cached_mutations", {})
    if isinstance(cached_raw, dict):
        cached_mutations = dict(cached_raw)
    elif isinstance(cached_raw, list) and not cached_raw:
        cached_mutations = {}
    else:
        raise SystemExit(f"Player cached_mutations is not a supported container in {extracted_save}")

    requested_mutations = [str(value) for value in transform.get("mutations", []) if str(value).strip()]
    added_traits: List[str] = []
    for mutation_id in requested_mutations:
        if mutation_id not in traits:
            traits.append(mutation_id)
            added_traits.append(mutation_id)
        mutations.setdefault(mutation_id, dict(PLAYER_MUTATION_STATE_TEMPLATE))
        cached_mutations.setdefault(mutation_id, dict(PLAYER_MUTATION_STATE_TEMPLATE))

    player["traits"] = traits
    player["mutations"] = mutations
    player["cached_mutations"] = cached_mutations
    extracted_save.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
    run_zzip(extracted_save)
    if extracted_save.exists():
        extracted_save.unlink()

    return {
        "kind": "player_mutations",
        "world": world_dir.name,
        "player_save": str(transform.get("player_save", "")),
        "mutations": requested_mutations,
        "added_traits": added_traits,
    }


def apply_player_items_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    player_save = world_dir / player_save_name
    if not player_save.exists():
        raise SystemExit(f"Fixture player-items transform target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture player-items transform expects .zzip save path: {player_save}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture player-items transform did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")
    inventory = player.setdefault("inv", [])
    if not isinstance(inventory, list):
        raise SystemExit(f"Player inventory is not a list in {extracted_save}")

    bday = int(payload.get("turn", 0) or 0)
    added_items: List[Dict[str, Any]] = []
    for item in transform.get("items", []):
        typeid = str(item.get("typeid", "")).strip()
        count = int(item.get("count", 1) or 1)
        charges = item.get("charges")
        contents = item.get("contents")
        for _ in range(count):
            entry: Dict[str, Any] = {
                "typeid": typeid,
                "bday": bday,
                "last_temp_check": 0,
                "template_traits": [],
            }
            if charges is not None:
                entry["charges"] = int(charges)
            if isinstance(contents, dict):
                entry["contents"] = contents
            inventory.append(entry)
        added: Dict[str, Any] = {"typeid": typeid, "count": count}
        if charges is not None:
            added["charges"] = int(charges)
        added_items.append(added)

    extracted_save.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    run_zzip(extracted_save)
    if extracted_save.exists():
        extracted_save.unlink()

    return {
        "kind": "player_items",
        "world": world_dir.name,
        "player_save": player_save_name,
        "added_items": added_items,
    }


def anchor_from_special_points(points: List[Tuple[int, int, int]]) -> Tuple[int, int, int]:
    if not points:
        raise SystemExit("Special placement has no usable points")
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    zs = [point[2] for point in points]
    return min(xs), min(ys), min(zs)



def player_location_from_omt(target_omt: Tuple[int, int, int]) -> List[int]:
    return [target_omt[0] * 24 + 12, target_omt[1] * 24 + 12, target_omt[2]]



def player_load_anchor_from_location(
    old_player_location: List[int], target_player_location: List[int], *, old_overmap_x: int,
    old_overmap_y: int, old_levx: int, old_levy: int, old_levz: int,
) -> Dict[str, int]:
    if len(old_player_location) < 3 or len(target_player_location) < 3:
        raise SystemExit("Player load-anchor transform needs both old and target player locations")
    old_player_abs_sm_x = old_player_location[0] // 12
    old_player_abs_sm_y = old_player_location[1] // 12
    old_player_abs_sm_z = int(old_player_location[2])
    target_player_abs_sm_x = target_player_location[0] // 12
    target_player_abs_sm_y = target_player_location[1] // 12
    target_player_abs_sm_z = int(target_player_location[2])
    overmap_width_sm = OMAPX * 2
    old_abs_sm_x = old_overmap_x * overmap_width_sm + old_levx
    old_abs_sm_y = old_overmap_y * overmap_width_sm + old_levy
    offset_sm_x = old_player_abs_sm_x - old_abs_sm_x
    offset_sm_y = old_player_abs_sm_y - old_abs_sm_y
    offset_sm_z = old_player_abs_sm_z - old_levz

    target_abs_sm_x = target_player_abs_sm_x - offset_sm_x
    target_abs_sm_y = target_player_abs_sm_y - offset_sm_y
    target_abs_sm_z = target_player_abs_sm_z - offset_sm_z
    target_overmap_x, target_levx = divmod(target_abs_sm_x, overmap_width_sm)
    target_overmap_y, target_levy = divmod(target_abs_sm_y, overmap_width_sm)
    return {
        "om_x": target_overmap_x,
        "om_y": target_overmap_y,
        "levx": target_levx,
        "levy": target_levy,
        "levz": target_abs_sm_z,
        "offset_sm_x": offset_sm_x,
        "offset_sm_y": offset_sm_y,
        "offset_sm_z": offset_sm_z,
    }



def overmap_file_coords_from_abs_omt(abs_omt: Tuple[int, int, int]) -> Tuple[int, int, Tuple[int, int, int]]:
    overmap_x, local_x = divmod(abs_omt[0], OMAPX)
    overmap_y, local_y = divmod(abs_omt[1], OMAPY)
    return overmap_x, overmap_y, (local_x, local_y, abs_omt[2])



def overmap_plain_path(overmap_path: Path) -> Path:
    if overmap_path.suffix == ".zzip":
        return overmap_path.parent.parent / overmap_path.stem
    return overmap_path



def extract_overmap_payload(overmap_path: Path) -> Tuple[Path, str, Dict[str, Any]]:
    plain_path = overmap_plain_path(overmap_path)
    created_plain = False
    if overmap_path.suffix == ".zzip" and not plain_path.exists():
        run_zzip(overmap_path)
        created_plain = True
    if not plain_path.exists():
        raise SystemExit(f"Overmap payload not found: {plain_path}")

    text = plain_path.read_text(encoding="utf-8")
    version_line, sep, payload_text = text.partition("\n")
    if not sep:
        raise SystemExit(f"Overmap payload missing version header newline: {plain_path}")
    payload = json.loads(payload_text)
    if not isinstance(payload, dict):
        raise SystemExit(f"Overmap payload is not a JSON object: {plain_path}")
    payload["_created_plain"] = created_plain
    return plain_path, version_line, payload



def cleanup_extracted_overmap(plain_path: Path, *, keep: bool) -> None:
    if not keep and plain_path.exists():
        plain_path.unlink()



def write_overmap_payload(plain_path: Path, version_line: str, payload: Dict[str, Any]) -> None:
    payload_to_write = dict(payload)
    payload_to_write.pop("_created_plain", None)
    plain_path.write_text(
        version_line + "\n" + json.dumps(payload_to_write, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    run_zzip(plain_path)
    if plain_path.exists():
        plain_path.unlink()



def decode_overmap_layer(raw_layer: Any, *, context: str) -> List[str]:
    if not isinstance(raw_layer, list):
        raise SystemExit(f"Overmap layer is not a list in {context}")
    flat: List[str] = []
    for entry in raw_layer:
        if not isinstance(entry, list) or len(entry) < 2:
            raise SystemExit(f"Overmap RLE entry is invalid in {context}: {entry!r}")
        terrain = str(entry[0])
        try:
            count = int(entry[1])
        except (TypeError, ValueError):
            raise SystemExit(f"Overmap RLE count is invalid in {context}: {entry!r}")
        if count <= 0:
            raise SystemExit(f"Overmap RLE count must be positive in {context}: {entry!r}")
        flat.extend([terrain] * count)
    expected = OMAPX * OMAPY
    if len(flat) != expected:
        raise SystemExit(f"Overmap layer decoded to {len(flat)} cells, expected {expected} in {context}")
    return flat



def encode_overmap_layer(flat: List[str]) -> List[List[Any]]:
    if not flat:
        return []
    encoded: List[List[Any]] = []
    current = flat[0]
    count = 1
    for terrain in flat[1:]:
        if terrain == current:
            count += 1
            continue
        encoded.append([current, count])
        current = terrain
        count = 1
    encoded.append([current, count])
    return encoded



def overmap_layer_index(z_level: int) -> int:
    layer_index = z_level + OVERMAP_DEPTH
    if layer_index < 0:
        raise SystemExit(f"Unsupported overmap z-level for transform: {z_level}")
    return layer_index



def overmap_flat_index(point: Tuple[int, int, int]) -> int:
    x, y, _ = point
    if x < 0 or x >= OMAPX or y < 0 or y >= OMAPY:
        raise SystemExit(f"Overmap point out of bounds for transform: {point}")
    return y * OMAPX + x



def load_player_abs_omt(world_dir: Path, player_save_name: str) -> Tuple[Tuple[int, int, int], List[int]]:
    player_save = world_dir / player_save_name
    if not player_save.exists():
        raise SystemExit(f"Fixture player target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture player target expects .zzip save path: {player_save}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture player target did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if extracted_save.exists():
        extracted_save.unlink()
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")
    location = player.get("location", [])
    if not isinstance(location, list) or len(location) < 3:
        raise SystemExit(f"Extracted player save is missing location array: {extracted_save}")
    try:
        abs_omt = (int(location[0]) // 24, int(location[1]) // 24, int(location[2]))
        location_copy = [int(location[0]), int(location[1]), int(location[2])]
    except (TypeError, ValueError):
        raise SystemExit(f"Extracted player save has invalid location array: {location!r}")
    return abs_omt, location_copy



def upsert_special_placement(
    payload: Dict[str, Any], *, special_id: str,
    placement_origin: Tuple[int, int, int],
    placement_points: List[Tuple[int, int, int]],
) -> None:
    placements = payload.get("overmap_special_placements")
    if not isinstance(placements, list):
        raise SystemExit("Overmap payload missing overmap_special_placements array")

    target_entry: Optional[Dict[str, Any]] = None
    for entry in placements:
        if isinstance(entry, dict) and str(entry.get("special", "")) == special_id:
            target_entry = entry
            break

    if target_entry is None:
        target_entry = {"special": special_id, "placements": []}
        placements.append(target_entry)

    grouped = target_entry.get("placements")
    if not isinstance(grouped, list):
        raise SystemExit(f"Special placement list is not valid for {special_id}")

    placement_points_json = [{"p": [point[0], point[1], point[2]]} for point in placement_points]
    grouped[:] = [
        entry for entry in grouped
        if not (
            isinstance(entry, dict)
            and tuple(entry.get("origin", [])) == placement_origin
        )
    ]
    grouped.append({
        "origin": [placement_origin[0], placement_origin[1], placement_origin[2]],
        "points": placement_points_json,
    })



def apply_seed_overmap_special_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    special_id = str(transform.get("special_id", "")).strip()
    try:
        requested_site_index = int(transform.get("site_index", 1) or 1)
    except (TypeError, ValueError):
        raise SystemExit(f"Fixture seed-overmap-special-near-player needs integer site_index: {transform}")
    if requested_site_index <= 0:
        raise SystemExit(f"Fixture seed-overmap-special-near-player needs site_index >= 1: {transform}")
    raw_offset = transform.get("offset_omt", [])
    if not isinstance(raw_offset, list) or len(raw_offset) != 3:
        raise SystemExit(f"Fixture seed-overmap-special-near-player needs offset_omt=[x,y,z]: {transform}")
    try:
        offset_omt = [int(raw_offset[0]), int(raw_offset[1]), int(raw_offset[2])]
    except (TypeError, ValueError):
        raise SystemExit(f"Fixture seed-overmap-special-near-player offset_omt must be integers: {transform}")

    player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    target_abs_omt = (
        player_abs_omt[0] + offset_omt[0],
        player_abs_omt[1] + offset_omt[1],
        player_abs_omt[2] + offset_omt[2],
    )
    target_overmap_x, target_overmap_y, target_anchor_local = overmap_file_coords_from_abs_omt(target_abs_omt)
    target_overmap_path = world_dir / "overmaps" / f"o.{target_overmap_x}.{target_overmap_y}.zzip"
    if not target_overmap_path.exists():
        raise SystemExit(f"Target overmap file not found for nearby seeded special: {target_overmap_path}")

    placements = load_bandit_special_placements(repo_root(), world_dir, {special_id})
    matching = [placement for placement in placements if placement.special_id == special_id]
    if requested_site_index > len(matching):
        raise SystemExit(
            f"Fixture seed-overmap-special-near-player requested {special_id} site_index={requested_site_index}, "
            f"but only found {len(matching)} matching placements in {world_dir}"
        )
    source_placement = matching[requested_site_index - 1]
    source_points = list(source_placement.points)
    if not source_points:
        raise SystemExit(f"Source placement has no points for {special_id} site_index={requested_site_index}")
    source_ground_points = [point for point in source_points if point[2] <= 0] or source_points
    source_anchor = anchor_from_special_points(source_ground_points)
    translated_points = [
        (
            target_anchor_local[0] + point[0] - source_anchor[0],
            target_anchor_local[1] + point[1] - source_anchor[1],
            target_anchor_local[2] + point[2] - source_anchor[2],
        )
        for point in source_points
    ]

    target_local_overmaps = {
        (translated_point[0] // OMAPX, translated_point[1] // OMAPY)
        for translated_point in translated_points
    }
    if len(target_local_overmaps) != 1 or next(iter(target_local_overmaps)) != (0, 0):
        raise SystemExit(
            f"Fixture seed-overmap-special-near-player would cross overmap boundaries at target {target_abs_omt}; "
            "pick a nearer offset for this bounded helper"
        )

    source_overmap_path = world_dir / "overmaps" / source_placement.overmap_file
    if not source_overmap_path.exists():
        raise SystemExit(f"Source overmap file not found for {special_id}: {source_overmap_path}")

    source_plain_path, _, source_payload = extract_overmap_payload(source_overmap_path)
    keep_source_plain = not bool(source_payload.get("_created_plain", False))
    try:
        source_layers = source_payload.get("layers")
        if not isinstance(source_layers, list):
            raise SystemExit(f"Source overmap payload missing layers array: {source_overmap_path}")
        source_layer_cache: Dict[int, List[str]] = {}
        source_terrain_by_point: Dict[Tuple[int, int, int], str] = {}
        for point in source_points:
            layer_index = overmap_layer_index(point[2])
            if layer_index >= len(source_layers):
                raise SystemExit(f"Source overmap layer index out of range for point {point} in {source_overmap_path}")
            layer = source_layer_cache.get(layer_index)
            if layer is None:
                layer = decode_overmap_layer(source_layers[layer_index], context=f"{source_overmap_path} z={point[2]}")
                source_layer_cache[layer_index] = layer
            source_terrain_by_point[point] = layer[overmap_flat_index(point)]
    finally:
        cleanup_extracted_overmap(source_plain_path, keep=keep_source_plain)

    target_plain_path, target_version_line, target_payload = extract_overmap_payload(target_overmap_path)
    keep_target_plain = not bool(target_payload.get("_created_plain", False))
    target_layers = target_payload.get("layers")
    if not isinstance(target_layers, list):
        cleanup_extracted_overmap(target_plain_path, keep=keep_target_plain)
        raise SystemExit(f"Target overmap payload missing layers array: {target_overmap_path}")
    target_layer_cache: Dict[int, List[str]] = {}
    target_previous_terrain: Dict[Tuple[int, int, int], str] = {}
    try:
        for source_point, target_point in zip(source_points, translated_points):
            layer_index = overmap_layer_index(target_point[2])
            if layer_index >= len(target_layers):
                raise SystemExit(f"Target overmap layer index out of range for point {target_point} in {target_overmap_path}")
            layer = target_layer_cache.get(layer_index)
            if layer is None:
                layer = decode_overmap_layer(target_layers[layer_index], context=f"{target_overmap_path} z={target_point[2]}")
                target_layer_cache[layer_index] = layer
            flat_index = overmap_flat_index(target_point)
            target_previous_terrain[target_point] = layer[flat_index]
            layer[flat_index] = source_terrain_by_point[source_point]

        for layer_index, layer in target_layer_cache.items():
            target_layers[layer_index] = encode_overmap_layer(layer)
        upsert_special_placement(
            target_payload,
            special_id=special_id,
            placement_origin=target_anchor_local,
            placement_points=translated_points,
        )
        write_overmap_payload(target_plain_path, target_version_line, target_payload)
    except Exception:
        cleanup_extracted_overmap(target_plain_path, keep=keep_target_plain)
        raise

    return {
        "kind": "seed_overmap_special_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "special_id": special_id,
        "site_index": requested_site_index,
        "source_overmap_file": source_placement.overmap_file,
        "source_anchor_omt": list(source_anchor),
        "player_abs_omt": list(player_abs_omt),
        "player_location": player_location,
        "offset_omt": offset_omt,
        "target_abs_omt": list(target_abs_omt),
        "target_overmap_file": target_overmap_path.name,
        "target_anchor_omt": list(target_anchor_local),
        "copied_points": [list(point) for point in translated_points],
        "previous_target_terrain": {
            f"{point[0]},{point[1]},{point[2]}": terrain
            for point, terrain in sorted(target_previous_terrain.items())
        },
        "approx_range_omt": max(abs(offset_omt[0]), abs(offset_omt[1]), abs(offset_omt[2])),
    }



def apply_player_near_overmap_special_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save = world_dir / str(transform.get("player_save", "")).strip()
    if not player_save.exists():
        raise SystemExit(f"Fixture player-near-overmap-special target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(
            f"Fixture player-near-overmap-special expects .zzip save path: {player_save}"
        )

    special_id = str(transform.get("special_id", "")).strip()
    try:
        requested_site_index = int(transform.get("site_index", 1) or 1)
    except (TypeError, ValueError):
        raise SystemExit(f"Fixture player-near-overmap-special needs integer site_index: {transform}")
    if requested_site_index <= 0:
        raise SystemExit(f"Fixture player-near-overmap-special needs site_index >= 1: {transform}")
    raw_offset = transform.get("offset_omt", [])
    if not isinstance(raw_offset, list) or len(raw_offset) != 3:
        raise SystemExit(f"Fixture player-near-overmap-special needs offset_omt=[x,y,z]: {transform}")
    try:
        offset_omt = [int(raw_offset[0]), int(raw_offset[1]), int(raw_offset[2])]
    except (TypeError, ValueError):
        raise SystemExit(f"Fixture player-near-overmap-special offset_omt must be integers: {transform}")

    placements = load_bandit_special_placements(repo_root(), world_dir, {special_id})
    matching = [placement for placement in placements if placement.special_id == special_id]
    if requested_site_index > len(matching):
        raise SystemExit(
            f"Fixture player-near-overmap-special requested {special_id} site_index={requested_site_index}, "
            f"but only found {len(matching)} matching placements in {world_dir}"
        )
    placement = matching[requested_site_index - 1]
    candidate_points = [point for point in placement.points if point[2] <= 0] or list(placement.points)
    anchor_omt = anchor_from_special_points(candidate_points)
    target_omt = (
        anchor_omt[0] + offset_omt[0],
        anchor_omt[1] + offset_omt[1],
        anchor_omt[2] + offset_omt[2],
    )
    player_location = player_location_from_omt(target_omt)

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(
            f"Fixture player-near-overmap-special did not extract save: {extracted_save}"
        )

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")

    old_location_raw = player.get("location", [])
    old_location = list(old_location_raw) if isinstance(old_location_raw, list) else []
    old_overmap_x = int(payload.get("om_x", 0))
    old_overmap_y = int(payload.get("om_y", 0))
    old_levx = int(payload.get("levx", 0))
    old_levy = int(payload.get("levy", 0))
    old_levz = int(payload.get("levz", 0))
    updated_load_anchor = player_load_anchor_from_location(
        old_location,
        player_location,
        old_overmap_x=old_overmap_x,
        old_overmap_y=old_overmap_y,
        old_levx=old_levx,
        old_levy=old_levy,
        old_levz=old_levz,
    )
    player["location"] = player_location
    payload["om_x"] = updated_load_anchor["om_x"]
    payload["om_y"] = updated_load_anchor["om_y"]
    payload["levx"] = updated_load_anchor["levx"]
    payload["levy"] = updated_load_anchor["levy"]
    payload["levz"] = updated_load_anchor["levz"]
    extracted_save.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
    run_zzip(extracted_save)
    if extracted_save.exists():
        extracted_save.unlink()

    return {
        "kind": "player_near_overmap_special",
        "world": world_dir.name,
        "player_save": str(transform.get("player_save", "")),
        "special_id": special_id,
        "site_index": requested_site_index,
        "overmap_file": placement.overmap_file,
        "special_ground_points_omt": [list(point) for point in candidate_points],
        "special_anchor_omt": list(anchor_omt),
        "offset_omt": offset_omt,
        "target_omt": list(target_omt),
        "target_location": player_location,
        "previous_location": old_location,
        "previous_load_anchor": {
            "om_x": old_overmap_x,
            "om_y": old_overmap_y,
            "levx": old_levx,
            "levy": old_levy,
            "levz": old_levz,
        },
        "target_load_anchor": {
            "om_x": updated_load_anchor["om_x"],
            "om_y": updated_load_anchor["om_y"],
            "levx": updated_load_anchor["levx"],
            "levy": updated_load_anchor["levy"],
            "levz": updated_load_anchor["levz"],
        },
        "preserved_player_offset_sm": [
            updated_load_anchor["offset_sm_x"],
            updated_load_anchor["offset_sm_y"],
            updated_load_anchor["offset_sm_z"],
        ],
        "approx_range_omt": max(abs(offset_omt[0]), abs(offset_omt[1]), abs(offset_omt[2])),
    }



def apply_map_fields_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    maps_dir = world_dir / "maps"
    if not maps_dir.exists():
        raise SystemExit(f"Fixture map-field transform maps dir not found: {maps_dir}")

    placed_fields: List[Dict[str, Any]] = []
    extracted_packs: List[str] = []
    modified_map_paths: Dict[str, Path] = {}
    for field in transform.get("fields", []):
        field_id = str(field.get("field_id", "")).strip()
        offset_ms_raw = field.get("offset_ms", [0, 0, 0])
        offset_ms = [int(offset_ms_raw[0]), int(offset_ms_raw[1]), int(offset_ms_raw[2])]
        intensity = int(field.get("intensity", 1))
        age_turns = int(field.get("age_turns", 0))
        if intensity <= 0:
            raise SystemExit(f"Fixture map-field transform intensity must be positive: {field}")

        target_location = [
            int(player_location[0]) + offset_ms[0],
            int(player_location[1]) + offset_ms[1],
            int(player_location[2]) + offset_ms[2],
        ]
        target_abs_sm = (target_location[0] // 12, target_location[1] // 12, target_location[2])
        target_abs_omt = (target_location[0] // 24, target_location[1] // 24, target_location[2])
        local_ms = (
            target_location[0] - target_abs_sm[0] * 12,
            target_location[1] - target_abs_sm[1] * 12,
            target_location[2],
        )
        pack_stem = f"{target_abs_omt[0] // 32}.{target_abs_omt[1] // 32}.{target_abs_omt[2]}"
        pack_dir = maps_dir / pack_stem
        pack_zzip = maps_dir / f"{pack_stem}.zzip"
        if not pack_dir.exists():
            if not pack_zzip.exists():
                raise SystemExit(f"Fixture map-field transform target map pack not found: {pack_zzip}")
            run_zzip(pack_zzip)
            extracted_packs.append(pack_stem)
        if not pack_dir.exists() or not pack_dir.is_dir():
            raise SystemExit(f"Fixture map-field transform did not extract map pack: {pack_dir}")

        map_path = pack_dir / f"{target_abs_omt[0]}.{target_abs_omt[1]}.{target_abs_omt[2]}.map"
        if not map_path.exists():
            raise SystemExit(f"Fixture map-field transform target map file not found: {map_path}")
        map_payload = json.loads(map_path.read_text(encoding="utf-8"))
        if not isinstance(map_payload, list):
            raise SystemExit(f"Fixture map-field transform map payload is not a list: {map_path}")
        target_submap: Optional[Dict[str, Any]] = None
        target_coords = [target_abs_sm[0], target_abs_sm[1], target_abs_sm[2]]
        for submap in map_payload:
            if isinstance(submap, dict) and submap.get("coordinates") == target_coords:
                target_submap = submap
                break
        if target_submap is None:
            raise SystemExit(
                f"Fixture map-field transform submap {target_coords} not found in {map_path}"
            )
        fields = target_submap.setdefault("fields", [])
        if not isinstance(fields, list):
            raise SystemExit(f"Fixture map-field transform fields is not a list in {map_path}")
        field_payload = [
            field_id,
            intensity,
            age_turns,
            {"character_id": None, "faction_id": None},
        ]
        fields.extend([local_ms[0], local_ms[1], field_payload])
        map_path.write_text(json.dumps(map_payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        modified_map_paths[str(map_path)] = map_path

        placed_fields.append({
            "field_id": field_id,
            "intensity": intensity,
            "age_turns": age_turns,
            "offset_ms": offset_ms,
            "target_location_ms": target_location,
            "target_abs_omt": list(target_abs_omt),
            "target_abs_sm": list(target_abs_sm),
            "local_ms": [local_ms[0], local_ms[1], local_ms[2]],
            "map_pack": pack_stem,
            "map_file": map_path.name,
        })

    recompressed_maps: List[str] = []
    for map_path in sorted(modified_map_paths.values(), key=lambda path: str(path)):
        run_zzip(map_path)
        recompressed_maps.append(str(map_path.relative_to(world_dir)))

    removed_extracted_dirs: List[str] = []
    for pack_stem in sorted(set(extracted_packs)):
        pack_dir = maps_dir / pack_stem
        if pack_dir.exists():
            shutil.rmtree(pack_dir)
            removed_extracted_dirs.append(str(pack_dir.relative_to(world_dir)))

    return {
        "kind": "map_fields_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "player_abs_omt": list(player_abs_omt),
        "player_location": player_location,
        "placed_fields": placed_fields,
        "extracted_map_packs": sorted(set(extracted_packs)),
        "recompressed_maps": recompressed_maps,
        "removed_extracted_map_dirs": removed_extracted_dirs,
        "plain_map_dirs_left_for_game_load": False,
    }


def apply_game_turn_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    player_save = world_dir / player_save_name
    if not player_save.exists():
        raise SystemExit(f"Fixture game-turn transform target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture game-turn transform expects .zzip save path: {player_save}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture game-turn transform did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")

    old_turn = int(payload.get("turn", 0))
    new_turn = int(transform.get("turn", old_turn))
    payload["turn"] = new_turn
    extracted_save.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    run_zzip(extracted_save)
    if extracted_save.exists():
        extracted_save.unlink()

    return {
        "kind": "game_turn",
        "world": world_dir.name,
        "player_save": player_save_name,
        "old_turn": old_turn,
        "new_turn": new_turn,
    }


def apply_bandit_active_sortie_clock_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise SystemExit(f"Fixture bandit sortie transform target not found: {dimension_path}")

    dimension_text = dimension_path.read_text(encoding="utf-8")
    version_line, sep, payload_text = dimension_text.partition("\n")
    if not sep:
        raise SystemExit(f"Fixture dimension data missing version header newline: {dimension_path}")
    payload = json.loads(payload_text)
    if not isinstance(payload, dict):
        raise SystemExit(f"Fixture dimension data is not a JSON object: {dimension_path}")
    overmapbuffer = payload.get("overmapbuffer", {})
    if not isinstance(overmapbuffer, dict):
        raise SystemExit(f"Fixture dimension data lacks overmapbuffer object: {dimension_path}")
    live_world = overmapbuffer.get("bandit_live_world", {})
    if not isinstance(live_world, dict):
        raise SystemExit(f"Fixture dimension data lacks bandit_live_world object: {dimension_path}")
    sites = live_world.get("sites", [])
    if not isinstance(sites, list):
        raise SystemExit(f"Fixture bandit_live_world.sites is not a list: {dimension_path}")

    requested_site_id = str(transform.get("site_id", "")).strip()
    active_job_type = str(transform.get("active_job_type", "scout")).strip() or "scout"
    started_minutes = int(transform.get("active_sortie_started_minutes", 0))
    local_contact_minutes = int(transform.get("active_sortie_local_contact_minutes", started_minutes))
    touched_sites: List[str] = []
    for site in sites:
        if not isinstance(site, dict):
            continue
        site_id = str(site.get("site_id", ""))
        if requested_site_id and site_id != requested_site_id:
            continue
        if not site.get("active_group_id") or not site.get("active_member_ids"):
            continue
        site["active_job_type"] = active_job_type
        site["active_sortie_started_minutes"] = started_minutes
        site["active_sortie_local_contact_minutes"] = local_contact_minutes
        touched_sites.append(site_id)

    if not touched_sites:
        raise SystemExit(
            "Fixture bandit sortie transform found no active owned site"
            + (f" matching {requested_site_id}" if requested_site_id else "")
        )

    dimension_path.write_text(
        version_line + "\n" + json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    return {
        "kind": "bandit_active_sortie_clock",
        "world": world_dir.name,
        "site_id": requested_site_id,
        "active_job_type": active_job_type,
        "active_sortie_started_minutes": started_minutes,
        "active_sortie_local_contact_minutes": local_contact_minutes,
        "touched_sites": touched_sites,
    }


def apply_fixture_save_transforms(world_dir: Path, transforms: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    reports: List[Dict[str, Any]] = []
    for transform in transforms:
        kind = str(transform.get("kind", "")).strip().lower()
        if kind == "bandit_active_sortie_clock":
            reports.append(apply_bandit_active_sortie_clock_transform(world_dir, transform))
            continue
        if kind == "player_mutations":
            reports.append(apply_player_mutations_transform(world_dir, transform))
            continue
        if kind == "player_items":
            reports.append(apply_player_items_transform(world_dir, transform))
            continue
        if kind == "player_near_overmap_special":
            reports.append(apply_player_near_overmap_special_transform(world_dir, transform))
            continue
        if kind == "seed_overmap_special_near_player":
            reports.append(apply_seed_overmap_special_near_player_transform(world_dir, transform))
            continue
        if kind == "map_fields_near_player":
            reports.append(apply_map_fields_near_player_transform(world_dir, transform))
            continue
        if kind == "game_turn":
            reports.append(apply_game_turn_transform(world_dir, transform))
            continue
        raise SystemExit(f"Unsupported fixture save transform kind: {kind}")
    return reports


def profile_fixture_root(profile: str) -> Path:
    return fixtures_root() / profile


def profile_snapshot_root(profile: str) -> Path:
    return profile_snapshots_root() / profile


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


def scenario_path(name: str) -> Path:
    return scenarios_root() / f"{name}.json"


def load_scenario(name: str) -> Dict[str, Any]:
    path = scenario_path(name)
    if not path.exists():
        raise SystemExit(f"Scenario not found: {path}")
    loaded = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(loaded, dict):
        raise SystemExit(f"Invalid scenario config: {path}")
    loaded.setdefault("name", name)
    loaded["path"] = str(path)
    return loaded


def normalize_string_list(raw_value: Any) -> List[str]:
    if isinstance(raw_value, str):
        return [raw_value.strip()] if raw_value.strip() else []
    if not isinstance(raw_value, list):
        return []
    return [str(value).strip() for value in raw_value if str(value).strip()]


def scenario_blocker_info(scenario: Dict[str, Any]) -> Dict[str, Any]:
    status = str(scenario.get("status", "")).strip().lower()
    blocked_reason = str(scenario.get("blocked_reason", "")).strip()
    required_helpers = normalize_string_list(scenario.get("required_helpers", []))
    is_blocked = status == "blocked" or bool(blocked_reason)
    return {
        "status": "blocked" if is_blocked else "active",
        "blocked_reason": blocked_reason,
        "required_helpers": required_helpers,
    }


def list_scenarios() -> List[Dict[str, Any]]:
    root = scenarios_root()
    if not root.exists():
        return []
    scenarios = []
    for path in sorted(root.glob("*.json"), key=lambda p: p.name.lower()):
        loaded = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(loaded, dict):
            continue
        blocker_info = scenario_blocker_info(loaded)
        scenarios.append({
            "name": str(loaded.get("name", path.stem)),
            "path": str(path),
            "description": str(loaded.get("description", "")).strip(),
            "artifact_source": str(loaded.get("artifact_source", "debug.log")).strip() or "debug.log",
            "step_count": len(loaded.get("steps", [])) if isinstance(loaded.get("steps"), list) else 0,
            "status": blocker_info["status"],
            "blocked_reason": blocker_info["blocked_reason"],
            "required_helpers": blocker_info["required_helpers"],
        })
    return scenarios


def normalize_scenario_steps(raw_steps: Any, advance_count: int, settle_seconds: float) -> List[Dict[str, Any]]:
    if isinstance(raw_steps, list) and raw_steps:
        steps: List[Dict[str, Any]] = []
        for index, raw in enumerate(raw_steps, start=1):
            if not isinstance(raw, dict):
                raise SystemExit(f"Scenario step #{index} is not an object")
            step = dict(raw)
            step.setdefault("label", f"step_{index:02d}_{str(step.get('kind', 'step')).strip() or 'step'}")
            steps.append(step)
        return steps
    if advance_count > 0:
        return [{
            "kind": "advance_turns",
            "count": advance_count,
            "settle_seconds": settle_seconds,
            "label": "advance_turns",
        }]
    return []


def execute_probe_steps(
    pid: int,
    run_dir: Path,
    steps: List[Dict[str, Any]],
    *,
    artifact_log: Optional[Path] = None,
    artifact_baseline: int = 0,
    filter_debug_noise: bool = False,
    artifact_patterns: Optional[List[str]] = None,
) -> List[Dict[str, Any]]:
    reports: List[Dict[str, Any]] = []
    for index, step in enumerate(steps, start=1):
        kind = str(step.get("kind", "")).strip().lower()
        label = str(step.get("label", f"step_{index:02d}")).strip() or f"step_{index:02d}"
        settle_seconds = float(step.get("settle_seconds", 0.0) or 0.0)
        report: Dict[str, Any] = {
            "index": index,
            "kind": kind,
            "label": label,
            "settle_seconds": settle_seconds,
        }
        expected_visible_fact = str(step.get("expected_visible_fact", "")).strip()
        if expected_visible_fact:
            report["expected_visible_fact"] = expected_visible_fact
        if kind == "press":
            raw_keys = step.get("keys", [])
            if isinstance(raw_keys, str):
                keys = [raw_keys] if raw_keys.strip() else []
            elif isinstance(raw_keys, list):
                keys = [str(key) for key in raw_keys if str(key).strip()]
            else:
                keys = []
            if not keys:
                raise SystemExit(f"Scenario step '{label}' has no keys")
            delay_ms = int(step.get("delay_ms", 200) or 200)
            peekaboo_press_sequence(pid, keys, delay_ms=delay_ms)
            report.update({"keys": keys, "delay_ms": delay_ms})
        elif kind == "type":
            text = str(step.get("text", ""))
            if not text:
                raise SystemExit(f"Scenario step '{label}' has no text")
            delay_ms = int(step.get("delay_ms", 20) or 20)
            peekaboo_type_text(pid, text, delay_ms=delay_ms)
            submit = bool(step.get("submit", False))
            submit_settle_seconds = float(step.get("submit_settle_seconds", 0.2) or 0.0)
            report.update({"text": text, "delay_ms": delay_ms, "submit": submit})
            if submit:
                if submit_settle_seconds > 0:
                    time.sleep(submit_settle_seconds)
                submit_key = str(step.get("submit_key", "enter") or "enter")
                submit_delay_ms = int(step.get("submit_delay_ms", 200) or 200)
                peekaboo_press_sequence(pid, [submit_key], delay_ms=submit_delay_ms)
                report.update({
                    "submit_key": submit_key,
                    "submit_delay_ms": submit_delay_ms,
                    "submit_settle_seconds": submit_settle_seconds,
                })
        elif kind == "advance_turns":
            count = int(step.get("count", 0) or 0)
            if count <= 0:
                raise SystemExit(f"Scenario step '{label}' needs count > 0")
            report["count"] = count
            report["timing"] = advance_turns(pid, count)
        elif kind in {"long_wait", "wait_action"}:
            report.update(execute_long_wait_action(
                pid,
                run_dir,
                label,
                step,
                artifact_log=artifact_log,
                artifact_baseline=artifact_baseline,
                filter_debug_noise=filter_debug_noise,
                artifact_patterns=artifact_patterns,
            ))
            if report.get("status") == "aborted_by_wait_interruption":
                reports.append(report)
                return reports
        elif kind == "wait":
            seconds = float(step.get("seconds", 0.0) or 0.0)
            if seconds <= 0:
                raise SystemExit(f"Scenario step '{label}' needs seconds > 0")
            time.sleep(seconds)
            report["seconds"] = seconds
        elif kind == "debug_spawn_item":
            item_query = str(
                step.get("item_query")
                or step.get("item")
                or step.get("query")
                or ""
            ).strip()
            if not item_query:
                raise SystemExit(f"Scenario step '{label}' needs item_query/item")
            count = int(step.get("count", 1) or 1)
            selection_key = str(step.get("selection_key", "enter") or "enter").strip() or "enter"
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_spawn_item(
                pid,
                item_query=item_query,
                count=count,
                selection_key=selection_key,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "item_query": item_query,
                "count": count,
                "selection_key": selection_key,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "s", "w"],
                "spawn_target": "inventory",
            })
        elif kind == "debug_spawn_monster":
            monster_query = str(
                step.get("monster_query")
                or step.get("monster")
                or step.get("query")
                or ""
            ).strip()
            if not monster_query:
                raise SystemExit(f"Scenario step '{label}' needs monster_query/monster")
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            group_radius = int(step.get("group_radius", step.get("radius", 0)) or 0)
            friendly = bool(step.get("friendly", False))
            hallucination = bool(step.get("hallucination", False))
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_spawn_monster(
                pid,
                monster_query=monster_query,
                target_keys=target_keys,
                group_radius=group_radius,
                friendly=friendly,
                hallucination=hallucination,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "monster_query": monster_query,
                "target_keys": target_keys,
                "group_radius": group_radius,
                "friendly": friendly,
                "hallucination": hallucination,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "s", "m"],
                "spawn_target": "look_around_confirm",
            })
        elif kind == "debug_spawn_follower_npc":
            count = int(step.get("count", 1) or 1)
            if count <= 0:
                raise SystemExit(f"Scenario step '{label}' needs count > 0")
            delay_ms = int(step.get("delay_ms", 200) or 200)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            debug_spawn_follower_npc(
                pid,
                count=count,
                delay_ms=delay_ms,
                menu_settle_seconds=menu_settle_seconds,
            )
            report.update({
                "count": count,
                "delay_ms": delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "debug_menu_path": ["}", "s", "f"],
                "spawn_type": "random_follower_npc",
            })
        elif kind == "debug_force_temperature":
            try:
                temperature_f = int(step.get("temperature_f"))
            except (TypeError, ValueError):
                raise SystemExit(f"Scenario step '{label}' needs integer temperature_f")
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_force_temperature(
                pid,
                temperature_f=temperature_f,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "temperature_f": temperature_f,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "T"],
                "selection_path": ["down", "enter"],
            })
        elif kind == "assign_nearby_npc_to_camp_dialog":
            npc_selector = str(step.get("npc_selector") or step.get("selector") or "").strip()
            if not npc_selector:
                raise SystemExit(f"Scenario step '{label}' needs npc_selector/selector")
            raw_interaction_keys = step.get("interaction_keys", ["C", "t"])
            if isinstance(raw_interaction_keys, str):
                interaction_keys = [raw_interaction_keys] if raw_interaction_keys.strip() else []
            elif isinstance(raw_interaction_keys, list):
                interaction_keys = [str(key) for key in raw_interaction_keys if str(key).strip()]
            else:
                interaction_keys = []
            if not interaction_keys:
                raise SystemExit(f"Scenario step '{label}' needs interaction_keys")
            raw_lets_talk_key = step.get("lets_talk_key", "b")
            lets_talk_key = "" if raw_lets_talk_key is None else str(raw_lets_talk_key).strip()
            raw_assignment_keys = step.get("assignment_keys", ["d", "n", "a"])
            if isinstance(raw_assignment_keys, str):
                assignment_keys = [raw_assignment_keys] if raw_assignment_keys.strip() else []
            elif isinstance(raw_assignment_keys, list):
                assignment_keys = [str(key) for key in raw_assignment_keys if str(key).strip()]
            else:
                assignment_keys = []
            if not assignment_keys:
                raise SystemExit(f"Scenario step '{label}' needs assignment_keys")
            raw_exit_dialog_keys = step.get("exit_dialog_keys", ["q", "c"])
            if isinstance(raw_exit_dialog_keys, str):
                exit_dialog_keys = [raw_exit_dialog_keys] if raw_exit_dialog_keys.strip() else []
            elif isinstance(raw_exit_dialog_keys, list):
                exit_dialog_keys = [str(key) for key in raw_exit_dialog_keys if str(key).strip()]
            else:
                exit_dialog_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            stage_settle_seconds = float(step.get("stage_settle_seconds", 0.8) or 0.8)
            assign_nearby_npc_to_camp_dialog(
                pid,
                npc_selector=npc_selector,
                interaction_keys=interaction_keys,
                lets_talk_key=lets_talk_key,
                assignment_keys=assignment_keys,
                exit_dialog_keys=exit_dialog_keys,
                delay_ms=delay_ms,
                stage_settle_seconds=stage_settle_seconds,
            )
            report.update({
                "npc_selector": npc_selector,
                "interaction_keys": interaction_keys,
                "lets_talk_key": lets_talk_key,
                "assignment_keys": assignment_keys,
                "exit_dialog_keys": exit_dialog_keys,
                "delay_ms": delay_ms,
                "stage_settle_seconds": stage_settle_seconds,
                "dialog_path": interaction_keys + [npc_selector, lets_talk_key] + assignment_keys + exit_dialog_keys,
            })
        elif kind == "drop_item":
            query_or_slot = str(
                step.get("query_or_slot")
                or step.get("query")
                or step.get("slot")
                or ""
            ).strip()
            if not query_or_slot:
                raise SystemExit(f"Scenario step '{label}' needs query_or_slot/query/slot")
            count = int(step.get("count", 1) or 1)
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            selection_mode = drop_item(
                pid,
                query_or_slot=query_or_slot,
                count=count,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "query_or_slot": query_or_slot,
                "count": count,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "inventory_action": "drop_item",
                "selection_mode": selection_mode,
            })
        elif kind == "capture":
            capture_crop = normalize_capture_crop(
                step.get("capture_crop", step.get("crop"))
            )
            capture = capture_screenshot(pid, run_dir, label, crop=capture_crop)
            report["screen"] = capture.get("screen_summary", {})
            if expected_visible_fact:
                report["screen"]["expected_visible_fact"] = expected_visible_fact
            expected_screen_text_patterns = normalize_screen_text_patterns(
                step.get("expected_screen_text_contains", [])
            )
            if bool(step.get("extract_text", False)) or normalize_screen_text_patterns(
                step.get("abort_if_text_contains", [])
            ) or expected_screen_text_patterns:
                screen_text_report = capture_screen_text_artifact(
                    run_dir,
                    label,
                    capture,
                    tail_lines=int(step.get("extract_text_tail_lines", 8) or 8),
                )
                report["screen_text"] = screen_text_report
                if expected_screen_text_patterns:
                    report["screen_text_expectation"] = evaluate_screen_text_expectation(
                        screen_text_report,
                        expected_screen_text_patterns,
                    )
                if apply_screen_text_abort_guard(report, step, screen_text_report):
                    reports.append(report)
                    return reports
        else:
            raise SystemExit(f"Unsupported scenario step kind: {kind or '<empty>'}")
        if settle_seconds > 0:
            time.sleep(settle_seconds)
        if kind != "capture" and bool(step.get("capture_after", False)):
            capture_after_crop = normalize_capture_crop(
                step.get("capture_after_crop", step.get("capture_crop", step.get("crop")))
            )
            capture = capture_screenshot(pid, run_dir, f"{label}.after", crop=capture_after_crop)
            report["screen_after"] = capture.get("screen_summary", {})
            if expected_visible_fact:
                report["screen_after"]["expected_visible_fact"] = expected_visible_fact
            expected_screen_text_patterns = normalize_screen_text_patterns(
                step.get("expected_screen_text_after_contains", step.get("expected_screen_text_contains", []))
            )
            if bool(step.get("extract_text_after_capture", False)) or normalize_screen_text_patterns(
                step.get("abort_if_text_contains", [])
            ) or expected_screen_text_patterns:
                screen_text_report = capture_screen_text_artifact(
                    run_dir,
                    f"{label}.after",
                    capture,
                    tail_lines=int(step.get("extract_text_tail_lines", step.get("extract_text_after_capture_tail_lines", 8)) or 8),
                )
                report["screen_after_text"] = screen_text_report
                if expected_screen_text_patterns:
                    report["screen_after_text_expectation"] = evaluate_screen_text_expectation(
                        screen_text_report,
                        expected_screen_text_patterns,
                    )
                if apply_screen_text_abort_guard(report, step, screen_text_report):
                    reports.append(report)
                    return reports
        reports.append(report)
    return reports


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


def resolve_fixture_payload(
    fixture_name: str,
    fixture_profile: str,
    seen: Optional[List[Tuple[str, str]]] = None,
) -> Dict[str, Any]:
    source_profile = resolve_profile_name(fixture_profile)
    fixture_dir = profile_fixture_root(source_profile) / fixture_name
    if not fixture_dir.exists():
        raise SystemExit(f"Fixture not found: {fixture_dir}")
    manifest = load_fixture_manifest(fixture_dir)
    save_transforms = normalize_fixture_save_transforms(
        manifest.get("save_transforms", []),
        manifest_path=fixture_dir / "manifest.json",
    )
    save_src = fixture_dir / "save"
    chain = list(seen or [])
    chain.append((source_profile, fixture_name))
    if save_src.exists():
        return {
            "fixture": fixture_name,
            "fixture_profile": source_profile,
            "fixture_dir": fixture_dir,
            "save_src": save_src,
            "manifest": manifest,
            "source_chain": chain,
            "save_transforms": save_transforms,
        }

    source_fixture = str(manifest.get("source_fixture", "") or "").strip()
    if not source_fixture:
        raise SystemExit(f"Fixture save payload not found: {save_src}")
    nested_profile = str(manifest.get("source_profile", "") or source_profile).strip() or source_profile
    if (nested_profile, source_fixture) in chain:
        raise SystemExit(
            "Fixture source cycle detected: "
            + " -> ".join(f"{profile}:{name}" for profile, name in chain + [(nested_profile, source_fixture)])
        )
    resolved = resolve_fixture_payload(source_fixture, nested_profile, seen=chain)
    return {
        **resolved,
        "save_transforms": list(resolved.get("save_transforms", [])) + save_transforms,
    }


def install_fixture(profile: str, fixture_name: str, replace: bool, fixture_profile: str = "") -> Dict[str, Any]:
    source_profile = resolve_profile_name(fixture_profile or profile)
    resolved = resolve_fixture_payload(fixture_name, source_profile)
    save_src = resolved["save_src"]
    save_transforms = list(resolved.get("save_transforms", []))
    save_dst = save_dir_for_profile(profile)
    ensure_dir(save_dst)
    installed_worlds = []
    applied_save_transforms: List[Dict[str, Any]] = []
    for world_dir in sorted([p for p in save_src.iterdir() if p.is_dir()], key=lambda p: p.name.lower()):
        dst = save_dst / world_dir.name
        if dst.exists() and not replace:
            raise SystemExit(f"Destination world already exists: {dst} (use --replace)")
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(world_dir, dst)
        installed_worlds.append(world_dir.name)
        if save_transforms:
            applied_save_transforms.extend(apply_fixture_save_transforms(dst, save_transforms))
    return {
        "fixture": fixture_name,
        "profile": profile,
        "fixture_profile": source_profile,
        "resolved_fixture": resolved["fixture"],
        "resolved_fixture_profile": resolved["fixture_profile"],
        "source_chain": [f"{entry_profile}:{entry_name}" for entry_profile, entry_name in resolved["source_chain"]],
        "installed_worlds": installed_worlds,
        "applied_save_transforms": applied_save_transforms,
        "destination": str(save_dst),
        "manifest": resolved["manifest"],
    }


def resolve_profile_snapshot_payload(
    snapshot_name: str,
    snapshot_profile: str,
    seen: Optional[List[Tuple[str, str]]] = None,
) -> Dict[str, Any]:
    source_profile = resolve_profile_name(snapshot_profile)
    snapshot_dir = profile_snapshot_root(source_profile) / snapshot_name
    if not snapshot_dir.exists():
        raise SystemExit(f"Profile snapshot not found: {snapshot_dir}")
    manifest = load_fixture_manifest(snapshot_dir)
    copyable_entries = [
        entry for entry in sorted(snapshot_dir.iterdir(), key=lambda p: p.name.lower())
        if entry.name not in {"manifest.json", "save", "harness_runs"}
    ]
    chain = list(seen or [])
    chain.append((source_profile, snapshot_name))
    if copyable_entries:
        return {
            "snapshot": snapshot_name,
            "snapshot_profile": source_profile,
            "snapshot_dir": snapshot_dir,
            "copyable_entries": copyable_entries,
            "manifest": manifest,
            "source_chain": chain,
        }

    source_snapshot = str(manifest.get("source_snapshot", "") or "").strip()
    if not source_snapshot:
        raise SystemExit(f"Profile snapshot has no copyable payload: {snapshot_dir}")
    nested_profile = str(manifest.get("source_profile", "") or source_profile).strip() or source_profile
    if (nested_profile, source_snapshot) in chain:
        raise SystemExit(
            "Profile snapshot source cycle detected: "
            + " -> ".join(f"{profile}:{name}" for profile, name in chain + [(nested_profile, source_snapshot)])
        )
    return resolve_profile_snapshot_payload(source_snapshot, nested_profile, seen=chain)


def install_profile_snapshot(profile: str, snapshot_name: str, snapshot_profile: str = "") -> Dict[str, Any]:
    source_profile = resolve_profile_name(snapshot_profile or profile)
    resolved = resolve_profile_snapshot_payload(snapshot_name, source_profile)
    snapshot_dir = resolved["snapshot_dir"]

    userdir = userdir_for_profile(profile)
    ensure_dir(userdir)
    copied_entries = []
    skipped_entries = ["manifest.json", "save", "harness_runs"]
    for entry in resolved["copyable_entries"]:
        dst = userdir / entry.name
        if dst.exists() or dst.is_symlink():
            if dst.is_dir() and not dst.is_symlink():
                shutil.rmtree(dst)
            else:
                dst.unlink()
        if entry.is_dir():
            shutil.copytree(entry, dst)
        else:
            shutil.copy2(entry, dst)
        copied_entries.append(entry.name)

    return {
        "profile": profile,
        "snapshot": snapshot_name,
        "snapshot_profile": source_profile,
        "resolved_snapshot": resolved["snapshot"],
        "resolved_snapshot_profile": resolved["snapshot_profile"],
        "source_path": str(snapshot_dir),
        "destination": str(userdir),
        "copied_entries": copied_entries,
        "skipped_entries": skipped_entries,
        "source_chain": [f"{entry_profile}:{entry_name}" for entry_profile, entry_name in resolved["source_chain"]],
        "manifest": resolved["manifest"],
    }


def startup_ledger_row(
    *,
    step: str,
    preconditions: str,
    action: str,
    expected_state: str,
    evidence_artifact: str,
    failure_rule: str,
    next_step_gate: str,
    verdict: str,
    metadata: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    row: Dict[str, Any] = {
        "primitive_or_step": step,
        "preconditions": preconditions,
        "action": action,
        "expected_immediate_state": expected_state,
        "evidence_artifact": evidence_artifact,
        "failure_rule": failure_rule,
        "next_step_gate": next_step_gate,
        "verdict": verdict,
    }
    if metadata:
        row["metadata"] = metadata
    return row


def startup_fixture_install_verdict(fixture_name: str, install_result: Dict[str, Any]) -> str:
    if not fixture_name:
        return "green_not_requested"
    installed_worlds = install_result.get("installed_worlds", [])
    if isinstance(installed_worlds, list) and installed_worlds:
        return "green_fixture_installed"
    return "red_fixture_not_installed"


def startup_profile_snapshot_verdict(snapshot_name: str, snapshot_result: Dict[str, Any]) -> str:
    if not snapshot_name:
        return "green_not_requested"
    copied_entries = snapshot_result.get("copied_entries", [])
    if isinstance(copied_entries, list) and copied_entries:
        return "green_profile_snapshot_installed"
    return "red_profile_snapshot_not_installed"


def startup_screen_capture_verdict(screen_summary: Dict[str, Any]) -> str:
    if not screen_summary.get("peekaboo_success"):
        return "red_screen_capture_failed"
    if screen_summary.get("version_matches_runtime_paths") is False:
        return "yellow_runtime_version_mismatch"
    return "green_screen_captured"


def build_startup_step_ledger(
    *,
    plan: StartupPlan,
    profile_snapshot_name: str,
    profile_snapshot_result: Dict[str, Any],
    fixture_name: str,
    fixture_install_result: Dict[str, Any],
    process_path: Path,
    focus_result: Dict[str, Any],
    readiness: Dict[str, Any],
    readiness_ok: bool,
    screen_summary: Dict[str, Any],
    screen_label: str,
    run_dir: Path,
    failure_reason: str = "",
) -> List[Dict[str, Any]]:
    plan_path = run_dir / "plan.json"
    ledger = [
        startup_ledger_row(
            step="dry plan / runtime resolution",
            preconditions="repo checkout and requested profile/world/fixture are known",
            action="build_plan(profile, world, fixture)",
            expected_state="profile, userdir, executable, strategy, target world, existing worlds, and run dir are resolved without launching",
            evidence_artifact=str(plan_path),
            failure_rule="missing executable/profile/run_dir or contradictory target world is red; valid metadata is plan-only proof",
            next_step_gate="launch may run only after plan metadata resolves",
            verdict="green_plan_resolved" if plan.executable and plan.run_dir else "red_plan_unresolved",
            metadata={
                "profile": plan.profile,
                "target_world": plan.target_world,
                "strategy": plan.strategy,
                "executable": plan.executable,
                "fixture": plan.fixture,
            },
        ),
        startup_ledger_row(
            step="profile snapshot install metadata",
            preconditions="optional disposable profile snapshot requested before launch",
            action="install_profile_snapshot(...) if --profile-snapshot is present",
            expected_state="requested snapshot entries are copied into the harness profile userdir, or explicitly not requested",
            evidence_artifact=str(run_dir / "startup.result.json"),
            failure_rule="requested snapshot with no copied entries is red; not requested is not feature evidence",
            next_step_gate="fixture install/launch may continue when snapshot is installed or explicitly not requested",
            verdict=startup_profile_snapshot_verdict(profile_snapshot_name, profile_snapshot_result),
            metadata={
                "profile_snapshot": profile_snapshot_name,
                "result": profile_snapshot_result,
            },
        ),
        startup_ledger_row(
            step="fixture install metadata",
            preconditions="optional disposable save fixture requested before launch",
            action="install_fixture(...) if --fixture is present",
            expected_state="requested fixture world(s) copied into target profile save dir, including manifest/save-transform metadata when present",
            evidence_artifact=str(run_dir / "startup.result.json"),
            failure_rule="requested fixture with no installed worlds is red; manifest metadata remains fixture/setup proof only",
            next_step_gate="launch may continue when fixture is installed or explicitly not requested",
            verdict=startup_fixture_install_verdict(fixture_name, fixture_install_result),
            metadata={
                "fixture": fixture_name,
                "result": fixture_install_result,
            },
        ),
        startup_ledger_row(
            step="launch process",
            preconditions="plan resolved and previous game processes have been killed",
            action="launch_game(profile, target_world, run_dir)",
            expected_state="cataclysm process starts with the requested userdir/world arguments",
            evidence_artifact=str(process_path),
            failure_rule="missing pid or early process exit is red",
            next_step_gate="focus/load polling may continue only with a live pid",
            verdict="green_process_started" if readiness_ok or not failure_reason == "process_exited" else "red_process_exited",
            metadata={"pid": readiness.get("pid", 0)},
        ),
        startup_ledger_row(
            step="window focus",
            preconditions="cataclysm process is running",
            action="peekaboo window focus --pid <pid>",
            expected_state="the game window is selected before any title/load keystrokes or screenshots",
            evidence_artifact=str(run_dir / "startup.result.json"),
            failure_rule="non-zero focus returncode is yellow for startup and red for any keystroke-dependent feature proof",
            next_step_gate="startup polling may continue, but feature proof needs a green focus row before key delivery is trusted",
            verdict="green_focus_returned" if focus_result.get("ok") else "yellow_focus_unproven",
            metadata=focus_result,
        ),
        startup_ledger_row(
            step="load/readiness gate",
            preconditions="game process is running after focus and optional default-flow keystrokes",
            action="poll lastworld.json and world save markers until expected world readiness changes",
            expected_state="lastworld metadata or a save marker changes for the requested world",
            evidence_artifact=str(run_dir / "lastworld.after.json"),
            failure_rule="timeout, wrong world, missing character/save metadata, or process exit is red",
            next_step_gate="success screenshot may be captured only after readiness metadata changes",
            verdict="green_load_ready" if readiness_ok else f"red_{failure_reason or 'load_not_ready'}",
            metadata=readiness,
        ),
        startup_ledger_row(
            step="success/failure screenshot capture",
            preconditions="readiness gate reached or failure state needs artifact capture",
            action=f"peekaboo image capture label={screen_label}",
            expected_state="named PNG/Peekaboo JSON records the actual window selected for the startup result",
            evidence_artifact=str(run_dir / f"{screen_label}.png"),
            failure_rule="capture failure is red; stale runtime metadata is yellow; screenshot alone remains startup/load proof only",
            next_step_gate="startup command may report success, but no feature step may close from this ledger alone",
            verdict=startup_screen_capture_verdict(screen_summary),
            metadata=screen_summary,
        ),
    ]
    return ledger


def startup_proof_classification(*, ok: bool, screen_summary: Dict[str, Any], failure_reason: str = "") -> Dict[str, Any]:
    if not ok:
        return {
            "evidence_class": "startup/load",
            "status": "red",
            "verdict": failure_reason or "startup_failed",
            "feature_proof": False,
            "startup_clean_for_feature_steps": False,
            "feature_gate": failure_reason or "startup_failed",
            "rule": "A failed startup cannot support feature proof.",
        }
    status = "green"
    verdict = "startup_load_only"
    clean_for_feature_steps = True
    feature_gate = "startup_clean"
    if not screen_summary.get("peekaboo_success"):
        status = "red"
        verdict = "startup_load_only_screen_capture_failed"
        clean_for_feature_steps = False
        feature_gate = "screen_capture_failed"
    elif screen_summary.get("version_matches_runtime_paths") is False:
        status = "yellow"
        verdict = "startup_load_only_runtime_version_mismatch"
        clean_for_feature_steps = False
        feature_gate = "runtime_version_mismatch"
    return {
        "evidence_class": "startup/load",
        "status": status,
        "verdict": verdict,
        "feature_proof": False,
        "startup_clean_for_feature_steps": clean_for_feature_steps,
        "feature_gate": feature_gate,
        "rule": "Load/readiness plus screenshot prove only startup/load; feature proof still needs later step-local ledgers and a clean startup gate.",
    }


def probe_proof_classification(
    *,
    verdict: str,
    startup_classification: Dict[str, Any],
    step_reports: List[Dict[str, Any]],
    artifact_patterns: List[str],
    matches_by_pattern: List[Dict[str, Any]],
    wait_step_summary: Optional[Dict[str, Any]] = None,
    abort_report: Optional[Dict[str, Any]] = None,
    step_ledger_summary: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    step_count = len(step_reports)
    matched_artifacts = any(entry.get("lines") for entry in matches_by_pattern)
    startup_status = str(startup_classification.get("status", "unclassified"))
    startup_verdict = str(startup_classification.get("verdict", "unclassified"))
    startup_feature_gate = str(startup_classification.get("feature_gate", startup_verdict))
    startup_clean_for_feature_steps = bool(startup_classification.get("startup_clean_for_feature_steps", False))
    startup_feature_proof = bool(startup_classification.get("feature_proof", False))
    wait_status = str((wait_step_summary or {}).get("status", "")).strip()
    step_ledger_status = str((step_ledger_summary or {}).get("status", "")).strip()
    has_wait_blocker = wait_status in {"blocked_wait_step", "yellow_wait_step_unverified"}
    has_step_ledger_blocker = step_ledger_status in {
        "red_step_local_proof_failed",
        "yellow_step_local_proof_incomplete",
    }
    if abort_report is not None:
        status = "red" if str(verdict).startswith("blocked") else "yellow"
        feature_proof = False
    elif has_wait_blocker:
        status = "red" if wait_status.startswith("blocked") else "yellow"
        feature_proof = False
    elif has_step_ledger_blocker:
        status = "red" if step_ledger_status.startswith("red") else "yellow"
        feature_proof = False
        if verdict == "artifacts_matched":
            verdict = step_ledger_status
    elif step_count <= 0:
        status = "yellow"
        feature_proof = False
        verdict = "startup_load_only_no_feature_steps"
    elif verdict == "artifacts_matched" and step_count > 0 and matched_artifacts and not startup_clean_for_feature_steps:
        status = "yellow" if startup_status != "red" else "red"
        feature_proof = False
        verdict = "startup_gate_not_clean_artifact_match_inconclusive"
    elif verdict == "artifacts_matched" and step_count > 0 and matched_artifacts:
        if step_ledger_status == "green_step_local_proof":
            status = "green"
            feature_proof = True
        else:
            status = "yellow"
            feature_proof = False
            verdict = "step_local_proof_missing_artifact_match_inconclusive"
    elif verdict.startswith("blocked") or verdict.startswith("inconclusive"):
        status = "red" if verdict.startswith("blocked") else "yellow"
        feature_proof = False
    else:
        status = "yellow"
        feature_proof = False
    return {
        "evidence_class": "feature-path" if feature_proof else "startup/load-or-inconclusive",
        "status": status,
        "verdict": verdict,
        "feature_proof": feature_proof,
        "startup_feature_proof": startup_feature_proof,
        "startup_clean_for_feature_steps": startup_clean_for_feature_steps,
        "startup_status": startup_status,
        "startup_verdict": startup_verdict,
        "startup_feature_gate": startup_feature_gate,
        "step_count": step_count,
        "artifact_patterns": artifact_patterns,
        "matched_artifact_patterns": [entry.get("pattern", "") for entry in matches_by_pattern if entry.get("lines")],
        "wait_step_status": wait_status,
        "step_ledger_status": step_ledger_status,
        "rule": "Startup/load evidence is never feature proof; feature classification requires a clean startup gate, green step-local ledger rows, matched artifacts, and no blocked/yellow wait ledger.",
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


def latest_world_save_marker(profile: str, expected_world: str = "") -> Dict[str, Any]:
    latest: Dict[str, Any] = {}
    for world in list_worlds(profile):
        if expected_world and world.name != expected_world:
            continue
        for marker in world_save_marker_paths(world.path):
            try:
                stat = marker.stat()
            except OSError:
                continue
            try:
                marker_name = str(marker.relative_to(world.path))
            except ValueError:
                marker_name = marker.name
            if not latest or stat.st_mtime > float(latest.get("mtime", 0.0)):
                latest = {
                    "world_name": world.name,
                    "marker": marker_name,
                    "path": str(marker),
                    "mtime": stat.st_mtime,
                    "reason": "world_save_marker",
                }
    return latest


def success_from_world_save_marker(
    profile: str, baseline_mtime: float, expected_world: str
) -> Tuple[bool, Dict[str, Any]]:
    marker = latest_world_save_marker(profile, expected_world)
    if not marker:
        return False, {}
    if float(marker.get("mtime", 0.0)) <= baseline_mtime:
        return False, marker
    marker["readiness"] = "new_world_save_marker_after_startup"
    return True, marker


def run_startup(args: argparse.Namespace) -> int:
    profile = resolve_profile_name(args.profile)
    config = load_profile_config(profile)
    profile_snapshot = str(getattr(args, "profile_snapshot", "") or "").strip()

    if args.dry_run:
        plan = build_plan(profile, args.world, args.fixture)
        run_dir = Path(plan.run_dir)
        write_json(run_dir / "plan.json", asdict(plan))
        dry_result = asdict(plan)
        dry_result["dry_run_contract"] = {
            "profile_snapshot_install": "not_run_dry_run",
            "fixture_install": "not_run_dry_run",
            "launch": "not_run_dry_run",
            "feature_proof": False,
            "evidence_class": "plan-only",
        }
        print(json.dumps(dry_result, indent=2, ensure_ascii=False))
        return 0

    profile_snapshot_result: Dict[str, Any] = {}
    if profile_snapshot:
        profile_snapshot_result = install_profile_snapshot(
            profile,
            profile_snapshot,
            snapshot_profile=getattr(args, "profile_snapshot_profile", ""),
        )
    fixture_install_result: Dict[str, Any] = {}
    if args.fixture:
        fixture_install_result = install_fixture(
            profile,
            args.fixture,
            replace=args.replace_existing_worlds,
            fixture_profile=getattr(args, "fixture_profile", ""),
        )
    plan = build_plan(profile, args.world, args.fixture)
    run_dir = Path(plan.run_dir)
    write_json(run_dir / "plan.json", asdict(plan))

    require_peekaboo_permissions()
    killed_pids = kill_existing_game_processes()
    ensure_dir(config_dir_for_profile(profile))
    debug_log = config_dir_for_profile(profile) / "debug.log"
    lastworld = config_dir_for_profile(profile) / "lastworld.json"
    debug_size = debug_log.stat().st_size if debug_log.exists() else 0
    baseline_mtime = lastworld.stat().st_mtime if lastworld.exists() else 0.0
    baseline_save_marker = latest_world_save_marker(profile, plan.target_world)
    baseline_save_marker_mtime = float(baseline_save_marker.get("mtime", 0.0) or 0.0)
    copy_file_if_exists(lastworld, run_dir / "lastworld.before.json")
    proc = launch_game(profile, plan.target_world, run_dir)
    write_json(run_dir / "process.json", {
        "pid": proc.pid,
        "command": [plan.executable, "--userdir", f".userdata/{profile}/"] + (["--world", plan.target_world] if plan.target_world else []),
        "killed_previous_pids": killed_pids,
    })

    startup_cfg = config["startup"]
    time.sleep(float(startup_cfg["initial_wait_seconds"]))
    focus_result = peekaboo_focus_pid(proc.pid)
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
            screen = capture_screenshot(proc.pid, run_dir, "failure_process_exit")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            screen_summary = screen.get("screen_summary", {})
            readiness = {"kind": "process_exit", "pid": proc.pid, "returncode": code}
            startup_step_ledger = build_startup_step_ledger(
                plan=plan,
                profile_snapshot_name=profile_snapshot,
                profile_snapshot_result=profile_snapshot_result,
                fixture_name=args.fixture,
                fixture_install_result=fixture_install_result,
                process_path=run_dir / "process.json",
                focus_result=focus_result,
                readiness=readiness,
                readiness_ok=False,
                screen_summary=screen_summary,
                screen_label="failure_process_exit",
                run_dir=run_dir,
                failure_reason="process_exited",
            )
            proof_classification = startup_proof_classification(
                ok=False,
                screen_summary=screen_summary,
                failure_reason="process_exited",
            )
            result = {
                "ok": False,
                "reason": "process_exited",
                "returncode": code,
                "pid": proc.pid,
                "profile": profile,
                "profile_snapshot": profile_snapshot_result,
                "fixture_install": fixture_install_result,
                "run_dir": str(run_dir),
                "screen": screen_summary,
                "startup_step_ledger": startup_step_ledger,
                "proof_classification": proof_classification,
                "evidence_class": proof_classification["evidence_class"],
                "feature_proof": proof_classification["feature_proof"],
            }
            write_json(run_dir / "startup.step_ledger.json", {"startup_step_ledger": startup_step_ledger})
            write_json(run_dir / "startup.result.json", result)
            print(json.dumps(result, indent=2, ensure_ascii=False))
            return 1

        ok, data = success_from_lastworld(profile, baseline_mtime, expected_world)
        readiness_kind = "lastworld"
        if not ok:
            ok, data = success_from_world_save_marker(profile, baseline_save_marker_mtime, expected_world)
            readiness_kind = "world_save_marker"
        if ok:
            post_lastworld_wait = float(startup_cfg.get("post_lastworld_wait_seconds", 0.0) or 0.0)
            if post_lastworld_wait > 0:
                time.sleep(post_lastworld_wait)
                peekaboo_focus_pid(proc.pid)
            post_lastworld_continue_keys = startup_cfg.get("post_lastworld_continue_keys", [])
            if isinstance(post_lastworld_continue_keys, list) and post_lastworld_continue_keys:
                peekaboo_press_sequence(proc.pid, [str(key) for key in post_lastworld_continue_keys])
                time.sleep(float(startup_cfg["post_input_wait_seconds"]))
            screen = capture_screenshot(proc.pid, run_dir, "success")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            screen_summary = screen.get("screen_summary", {})
            readiness = {
                "kind": readiness_kind,
                "reason": data.get("readiness", "lastworld_updated") if isinstance(data, dict) else "lastworld_updated",
                "marker": data if readiness_kind == "world_save_marker" else {},
                "baseline_save_marker": baseline_save_marker,
                "pid": proc.pid,
            }
            startup_step_ledger = build_startup_step_ledger(
                plan=plan,
                profile_snapshot_name=profile_snapshot,
                profile_snapshot_result=profile_snapshot_result,
                fixture_name=args.fixture,
                fixture_install_result=fixture_install_result,
                process_path=run_dir / "process.json",
                focus_result=focus_result,
                readiness=readiness,
                readiness_ok=True,
                screen_summary=screen_summary,
                screen_label="success",
                run_dir=run_dir,
            )
            proof_classification = startup_proof_classification(ok=True, screen_summary=screen_summary)
            result = {
                "ok": True,
                "pid": proc.pid,
                "profile": profile,
                "ok_with_debug_popups": dismissals > 0,
                "debug_popups_recorded": dismissals,
                "strategy": plan.strategy,
                "profile_snapshot": profile_snapshot_result,
                "fixture_install": fixture_install_result,
                "post_lastworld_wait_seconds": post_lastworld_wait,
                "lastworld": data if readiness_kind == "lastworld" else {},
                "readiness": readiness,
                "run_dir": str(run_dir),
                "screen": screen_summary,
                "startup_step_ledger": startup_step_ledger,
                "proof_classification": proof_classification,
                "evidence_class": proof_classification["evidence_class"],
                "feature_proof": proof_classification["feature_proof"],
            }
            write_json(run_dir / "startup.step_ledger.json", {"startup_step_ledger": startup_step_ledger})
            write_json(run_dir / "startup.result.json", result)
            print(json.dumps(result, indent=2, ensure_ascii=False))
            return 0

        new_debug_size = capture_debug_delta(profile, debug_size, run_dir, dismissals + 1)
        if new_debug_size > debug_size and dismissals < max_popup_dismissals:
            debug_size = new_debug_size
            capture_screenshot(proc.pid, run_dir, f"debug_popup_{dismissals + 1:02d}")
            peekaboo_focus_pid(proc.pid)
            peekaboo_type_text(proc.pid, debug_popup_ignore_text)
            dismissals += 1
        time.sleep(poll_seconds)

    screen = capture_screenshot(proc.pid, run_dir, "failure_timeout")
    copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
    copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
    screen_summary = screen.get("screen_summary", {})
    readiness = {
        "kind": "timeout",
        "baseline_save_marker": baseline_save_marker,
        "latest_save_marker": latest_world_save_marker(profile, expected_world),
        "pid": proc.pid,
    }
    startup_step_ledger = build_startup_step_ledger(
        plan=plan,
        profile_snapshot_name=profile_snapshot,
        profile_snapshot_result=profile_snapshot_result,
        fixture_name=args.fixture,
        fixture_install_result=fixture_install_result,
        process_path=run_dir / "process.json",
        focus_result=focus_result,
        readiness=readiness,
        readiness_ok=False,
        screen_summary=screen_summary,
        screen_label="failure_timeout",
        run_dir=run_dir,
        failure_reason="startup_timeout",
    )
    proof_classification = startup_proof_classification(
        ok=False,
        screen_summary=screen_summary,
        failure_reason="startup_timeout",
    )
    result = {
        "ok": False,
        "reason": "startup_timeout",
        "pid": proc.pid,
        "profile": profile,
        "strategy": plan.strategy,
        "profile_snapshot": profile_snapshot_result,
        "fixture_install": fixture_install_result,
        "readiness": readiness,
        "run_dir": str(run_dir),
        "screen": screen_summary,
        "startup_step_ledger": startup_step_ledger,
        "proof_classification": proof_classification,
        "evidence_class": proof_classification["evidence_class"],
        "feature_proof": proof_classification["feature_proof"],
    }
    write_json(run_dir / "startup.step_ledger.json", {"startup_step_ledger": startup_step_ledger})
    write_json(run_dir / "startup.result.json", result)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 1


def run_json_command(cmd: List[str]) -> Tuple[int, Dict[str, Any], str, str]:
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    stdout = proc.stdout.strip()
    if not stdout:
        return proc.returncode, {}, proc.stdout, proc.stderr
    try:
        payload = json.loads(stdout)
    except Exception:
        payload = {
            "ok": False,
            "reason": "invalid_json_stdout",
            "stdout": proc.stdout,
            "stderr": proc.stderr,
            "returncode": proc.returncode,
        }
    return proc.returncode, payload, proc.stdout, proc.stderr


def snapshot_world_state(profile: str, world_name: str, run_dir: Path, label: str = "saved_world") -> Dict[str, Any]:
    world_name = str(world_name or "").strip()
    if not world_name:
        raise SystemExit("World snapshot requested without a world name")
    source_world = save_dir_for_profile(profile) / world_name
    if not source_world.exists():
        raise SystemExit(f"World snapshot source not found: {source_world}")
    snapshot_root = run_dir / label
    if snapshot_root.exists():
        shutil.rmtree(snapshot_root)
    ensure_dir(snapshot_root.parent)
    destination = snapshot_root / world_name
    shutil.copytree(source_world, destination)
    snapshot = {
        "label": label,
        "profile": profile,
        "world": world_name,
        "source": str(source_world),
        "destination": str(destination),
    }
    write_json(run_dir / f"{label}.json", snapshot)
    return snapshot


def finalize_probe_report(
    run_dir: Optional[Path],
    report: Dict[str, Any],
    *,
    cleanup_pid: int = 0,
    report_filename: str = "probe.report.json",
) -> None:
    if cleanup_pid > 0 and "cleanup" not in report:
        report["cleanup"] = cleanup_game_process(cleanup_pid)
    if run_dir is not None:
        write_json(run_dir / report_filename, report)
    print(json.dumps(report, indent=2, ensure_ascii=False))


def normalize_repeatability_expectations(raw_value: Any) -> List[Dict[str, Any]]:
    expectations: List[Dict[str, Any]] = []
    if not isinstance(raw_value, list):
        return expectations
    for index, raw in enumerate(raw_value, start=1):
        if not isinstance(raw, dict):
            raise SystemExit(f"Repeatability expectation #{index} is not an object")
        label = str(raw.get("label", "")).strip()
        if not label:
            raise SystemExit(f"Repeatability expectation #{index} is missing label")
        contains_raw = raw.get("contains", [])
        if isinstance(contains_raw, str):
            contains = [contains_raw.strip()] if contains_raw.strip() else []
        elif isinstance(contains_raw, list):
            contains = [str(value).strip() for value in contains_raw if str(value).strip()]
        else:
            contains = []
        expectations.append({
            "label": label,
            "contains": contains,
        })
    return expectations


def collect_companion_tail_lines(report: Dict[str, Any]) -> Dict[str, List[str]]:
    lookup: Dict[str, List[str]] = {}
    raw_helpers = report.get("companion_helpers", [])
    if not isinstance(raw_helpers, list):
        return lookup
    for helper in raw_helpers:
        if not isinstance(helper, dict):
            continue
        label = str(helper.get("label", "")).strip()
        if not label or label in lookup:
            continue
        raw_lines = helper.get("tail_lines", [])
        if isinstance(raw_lines, list):
            lines = [str(line).strip() for line in raw_lines if str(line).strip()]
        else:
            lines = []
        lookup[label] = lines
    return lookup


def evaluate_repeatability_expectations(
    report: Dict[str, Any],
    expectations: List[Dict[str, Any]],
) -> Tuple[List[Dict[str, Any]], Dict[str, List[str]]]:
    helper_lines = collect_companion_tail_lines(report)
    results: List[Dict[str, Any]] = []
    for expectation in expectations:
        label = str(expectation.get("label", "")).strip()
        contains = [str(value).strip() for value in expectation.get("contains", []) if str(value).strip()]
        lines = helper_lines.get(label, [])
        haystack = "\n".join(lines)
        missing = [needle for needle in contains if needle not in haystack]
        results.append({
            "label": label,
            "expected_contains": contains,
            "matched": not missing,
            "missing": missing,
            "observed_tail_lines": lines,
        })
    return results, helper_lines


def repeatability_run_summary(
    ordinal: int,
    returncode: int,
    report: Dict[str, Any],
    expectations: List[Dict[str, Any]],
) -> Dict[str, Any]:
    startup = report.get("startup", {}) if isinstance(report.get("startup"), dict) else {}
    startup_screen = startup.get("screen", {}) if isinstance(startup.get("screen"), dict) else {}
    cleanup = report.get("cleanup", {}) if isinstance(report.get("cleanup"), dict) else {}
    expectation_results, helper_lines = evaluate_repeatability_expectations(report, expectations)
    cleanup_status = str(cleanup.get("status", "")).strip()
    return {
        "run": ordinal,
        "returncode": returncode,
        "report_ok": bool(report.get("ok")),
        "scenario": str(report.get("scenario", "")).strip(),
        "run_dir": str(startup.get("run_dir", "") or report.get("run_dir", "")).strip(),
        "verdict": str(report.get("verdict", "")).strip(),
        "window_title": str(startup_screen.get("window_title", "")).strip(),
        "version_matches_repo_head": startup_screen.get("version_matches_repo_head"),
        "version_matches_runtime_paths": startup_screen.get("version_matches_runtime_paths"),
        "cleanup": cleanup,
        "cleanup_ok": cleanup_status in CLEANUP_ACCEPTED_STATUSES,
        "expectations": expectation_results,
        "helper_tail_lines": helper_lines,
    }


def render_repeatability_text_report(summary: Dict[str, Any]) -> str:
    lines = [
        f"Scenario: {summary.get('scenario', '')}",
        f"Runs: {summary.get('run_count', 0)}",
        f"Overall verdict: {summary.get('overall_verdict', '')}",
        "",
    ]
    for run in summary.get("runs", []):
        lines.append(
            "Run {run}: verdict={verdict} cleanup={cleanup} runtime_current={runtime_current}".format(
                run=run.get("run", "?"),
                verdict=run.get("verdict", ""),
                cleanup=run.get("cleanup", {}).get("status", ""),
                runtime_current=run.get("version_matches_runtime_paths", None),
            )
        )
        for expectation in run.get("expectations", []):
            status = "ok" if expectation.get("matched") else "missing"
            lines.append(f"  - {expectation.get('label', '')}: {status}")
            for missing in expectation.get("missing", []):
                lines.append(f"      missing: {missing}")
        lines.append("")
    aggregate = summary.get("aggregate_expectations", [])
    if aggregate:
        lines.append("Aggregate expectation status:")
        for item in aggregate:
            status = "stable" if item.get("all_runs_matched") else "mixed"
            lines.append(f"- {item.get('label', '')}: {status} ({item.get('matched_runs', 0)}/{item.get('run_count', 0)} runs)")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def run_repeatability(args: argparse.Namespace) -> int:
    scenario = load_scenario(args.scenario)
    profile = resolve_profile_name(args.profile or str(scenario.get("profile", "")))
    world = args.world or str(scenario.get("world", ""))
    fixture = args.fixture if args.fixture is not None else str(scenario.get("fixture", ""))
    repeat_count = int(args.count if args.count is not None else scenario.get("repeatability_count", 3) or 3)
    if repeat_count <= 0:
        raise SystemExit("Repeatability run count must be > 0")

    expectations = normalize_repeatability_expectations(scenario.get("repeatability_expectations", []))
    base_cmd = [sys.executable, str(Path(__file__).resolve()), "probe", str(scenario.get("name", args.scenario))]
    if args.profile:
        base_cmd.extend(["--profile", args.profile])
    if world:
        base_cmd.extend(["--world", world])
    if args.fixture is not None:
        base_cmd.extend(["--fixture", fixture])
    if args.replace_existing_worlds or bool(scenario.get("replace_existing_worlds", False)):
        base_cmd.append("--replace-existing-worlds")

    summary_run_dir = create_run_dir(profile)
    if args.dry_run:
        payload = {
            "scenario": str(scenario.get("name", args.scenario)),
            "profile": profile,
            "world": world,
            "fixture": fixture,
            "count": repeat_count,
            "expectations": expectations,
            "probe_command": base_cmd,
            "summary_run_dir": str(summary_run_dir),
        }
        print(json.dumps(payload, indent=2, ensure_ascii=False))
        return 0

    run_summaries: List[Dict[str, Any]] = []
    raw_runs: List[Dict[str, Any]] = []
    for ordinal in range(1, repeat_count + 1):
        rc, payload, stdout, stderr = run_json_command(base_cmd)
        raw_runs.append({
            "run": ordinal,
            "returncode": rc,
            "stdout": stdout,
            "stderr": stderr,
            "report": payload,
        })
        run_summaries.append(repeatability_run_summary(ordinal, rc, payload, expectations))

    aggregate_expectations: List[Dict[str, Any]] = []
    for expectation in expectations:
        label = str(expectation.get("label", "")).strip()
        matched_runs = sum(
            1
            for run in run_summaries
            for result in run.get("expectations", [])
            if result.get("label") == label and result.get("matched")
        )
        aggregate_expectations.append({
            "label": label,
            "run_count": repeat_count,
            "matched_runs": matched_runs,
            "all_runs_matched": matched_runs == repeat_count,
        })

    all_runs_ok = all(run.get("returncode") == 0 and run.get("report_ok") for run in run_summaries)
    all_runtime_current = all(run.get("version_matches_runtime_paths") is True for run in run_summaries)
    all_cleanup_ok = all(run.get("cleanup_ok") for run in run_summaries)
    all_expectations_ok = all(item.get("all_runs_matched") for item in aggregate_expectations) if aggregate_expectations else True
    overall_verdict = "stable_repeatability_pass" if all_runs_ok and all_runtime_current and all_cleanup_ok and all_expectations_ok else "mixed_repeatability"

    summary = {
        "ok": True,
        "scenario": str(scenario.get("name", args.scenario)),
        "profile": profile,
        "world": world,
        "fixture": fixture,
        "run_count": repeat_count,
        "summary_run_dir": str(summary_run_dir),
        "probe_command": base_cmd,
        "expectations": expectations,
        "runs": run_summaries,
        "aggregate_expectations": aggregate_expectations,
        "overall_verdict": overall_verdict,
    }
    write_json(summary_run_dir / "repeatability.report.json", summary)
    write_json(summary_run_dir / "repeatability.raw_runs.json", {
        "scenario": str(scenario.get("name", args.scenario)),
        "runs": raw_runs,
    })
    (summary_run_dir / "repeatability.summary.txt").write_text(
        render_repeatability_text_report(summary),
        encoding="utf-8",
    )
    print(json.dumps(summary, indent=2, ensure_ascii=False))
    return 0


def run_probe_mode(args: argparse.Namespace, *, handoff: bool = False) -> int:
    scenario = load_scenario(args.scenario)
    blocker_info = scenario_blocker_info(scenario)
    profile = resolve_profile_name(args.profile or str(scenario.get("profile", "")))
    world = args.world or str(scenario.get("world", ""))
    fixture = args.fixture if args.fixture is not None else str(scenario.get("fixture", ""))
    fixture_profile = str(scenario.get("fixture_profile", "")).strip()
    profile_snapshot = str(scenario.get("profile_snapshot", "")).strip()
    profile_snapshot_profile = str(scenario.get("profile_snapshot_profile", "")).strip()
    replace_existing_worlds = args.replace_existing_worlds or bool(scenario.get("replace_existing_worlds", False))
    advance_count = int(args.advance_turns if args.advance_turns is not None else scenario.get("advance_turns", 0))
    settle_seconds = float(args.settle_seconds if args.settle_seconds is not None else scenario.get("settle_seconds", 1.0))
    raw_patterns = scenario.get("artifact_patterns", [])
    if isinstance(raw_patterns, list):
        artifact_patterns = [str(pattern) for pattern in raw_patterns if str(pattern).strip()]
    else:
        artifact_patterns = []
    if not artifact_patterns:
        artifact_pattern = args.artifact_pattern or str(scenario.get("artifact_pattern", ""))
        artifact_patterns = [artifact_pattern] if artifact_pattern.strip() else []
    elif args.artifact_pattern:
        artifact_patterns = [args.artifact_pattern]
    recommended_test_command = args.test_command or str(scenario.get("recommended_test_command", "")).strip()
    artifact_source = str(scenario.get("artifact_source", "debug.log")).strip() or "debug.log"
    steps = normalize_scenario_steps(scenario.get("steps", []), advance_count, settle_seconds)
    raw_derived_screens = scenario.get("derived_screens", [])
    derived_screens = [entry for entry in raw_derived_screens if isinstance(entry, dict)] if isinstance(raw_derived_screens, list) else []
    capture_world_after = bool(scenario.get("capture_world_after", False))
    mode = "handoff" if handoff else "probe"
    report_filename = "handoff.report.json" if handoff else "probe.report.json"

    if blocker_info["status"] == "blocked":
        run_dir = create_run_dir(profile)
        proof_classification = {
            "evidence_class": "not-run",
            "status": "red",
            "verdict": "blocked_helper_gap",
            "feature_proof": False,
            "rule": "Blocked scenarios do not run startup or feature steps.",
        }
        report = {
            "ok": False,
            "mode": mode,
            "scenario": str(scenario.get("name", args.scenario)),
            "contract": {
                "profile": profile,
                "world": world,
                "profile_snapshot": profile_snapshot,
                "profile_snapshot_profile": profile_snapshot_profile,
                "fixture": fixture,
                "fixture_profile": fixture_profile,
                "replace_existing_worlds": replace_existing_worlds,
                "advance_turns": advance_count,
                "settle_seconds": settle_seconds,
                "artifact_source": artifact_source,
                "artifact_patterns": artifact_patterns,
                "steps": steps,
                "capture_world_after": capture_world_after,
            },
            "startup": {
                "status": "not_run_blocked_scenario",
                "run_dir": str(run_dir),
            },
            "steps": [
                {
                    "index": index,
                    "kind": str(step.get("kind", "")).strip().lower(),
                    "label": str(step.get("label", f"step_{index:02d}")).strip() or f"step_{index:02d}",
                    "status": "not_run_blocked_scenario",
                }
                for index, step in enumerate(steps, start=1)
            ],
            "screen": {
                "status": "not_run",
                "before": {},
                "after": {},
            },
            "tests": {
                "status": "not_run",
                "recommended_command": recommended_test_command,
            },
            "artifacts": {
                "status": "not_run",
                "path": "",
                "source_log": artifact_source,
                "match_patterns": artifact_patterns,
                "matches_by_pattern": [],
                "matched_lines": [],
                "line_count": 0,
            },
            "blocked": blocker_info,
            "proof_classification": proof_classification,
            "evidence_class": proof_classification["evidence_class"],
            "feature_proof": proof_classification["feature_proof"],
            "verdict": "blocked_helper_gap",
        }
        finalize_probe_report(run_dir, report, report_filename=report_filename)
        return 2

    start_cmd = [sys.executable, str(Path(__file__).resolve()), "start", "--profile", profile]
    if world:
        start_cmd.extend(["--world", world])
    if profile_snapshot:
        start_cmd.extend(["--profile-snapshot", profile_snapshot])
    if profile_snapshot_profile:
        start_cmd.extend(["--profile-snapshot-profile", profile_snapshot_profile])
    if fixture:
        start_cmd.extend(["--fixture", fixture])
    if fixture_profile:
        start_cmd.extend(["--fixture-profile", fixture_profile])
    if replace_existing_worlds:
        start_cmd.append("--replace-existing-worlds")
    if args.dry_run:
        start_cmd.append("--dry-run")

    start_rc, start_result, start_stdout, start_stderr = run_json_command(start_cmd)
    if args.dry_run:
        print(json.dumps({
            "mode": mode,
            "scenario": scenario,
            "resolved_contract": {
                "profile": profile,
                "world": world,
                "profile_snapshot": profile_snapshot,
                "profile_snapshot_profile": profile_snapshot_profile,
                "fixture": fixture,
                "fixture_profile": fixture_profile,
                "replace_existing_worlds": replace_existing_worlds,
                "advance_turns": advance_count,
                "settle_seconds": settle_seconds,
                "artifact_source": artifact_source,
                "artifact_patterns": artifact_patterns,
                "recommended_test_command": recommended_test_command,
                "steps": steps,
                "capture_world_after": capture_world_after,
            },
            "startup_plan": start_result,
            "start_command": start_cmd,
        }, indent=2, ensure_ascii=False))
        return 0

    run_dir_value = str(start_result.get("run_dir", "")).strip()
    run_dir = Path(run_dir_value) if run_dir_value else None
    if start_rc != 0 or not start_result.get("ok"):
        proof_classification = start_result.get("proof_classification") if isinstance(start_result.get("proof_classification"), dict) else {
            "evidence_class": "startup/load",
            "status": "red",
            "verdict": "startup_failed",
            "feature_proof": False,
            "rule": "Startup command failed before any feature-path proof could run.",
        }
        report = {
            "ok": False,
            "mode": mode,
            "scenario": str(scenario.get("name", args.scenario)),
            "reason": "startup_failed",
            "startup": start_result,
            "proof_classification": proof_classification,
            "evidence_class": proof_classification.get("evidence_class", "startup/load"),
            "feature_proof": proof_classification.get("feature_proof", False),
            "start_command": start_cmd,
            "start_stdout": start_stdout,
            "start_stderr": start_stderr,
        }
        finalize_probe_report(
            run_dir,
            report,
            cleanup_pid=int(start_result.get("pid", 0)),
            report_filename=report_filename,
        )
        return 1

    pid = int(start_result.get("pid", 0))
    if pid <= 0 or run_dir is None:
        proof_classification = start_result.get("proof_classification") if isinstance(start_result.get("proof_classification"), dict) else {
            "evidence_class": "startup/load",
            "status": "red",
            "verdict": "startup_missing_runtime_details",
            "feature_proof": False,
            "rule": "Startup omitted pid/run_dir, so no feature-path proof can run.",
        }
        report = {
            "ok": False,
            "mode": mode,
            "scenario": str(scenario.get("name", args.scenario)),
            "reason": "startup_missing_runtime_details",
            "startup": start_result,
            "proof_classification": proof_classification,
            "evidence_class": proof_classification.get("evidence_class", "startup/load"),
            "feature_proof": proof_classification.get("feature_proof", False),
        }
        finalize_probe_report(run_dir, report, cleanup_pid=pid, report_filename=report_filename)
        return 1

    artifact_log, filter_debug_noise, resolved_artifact_source = resolve_artifact_source(profile, artifact_source)
    runtime_blockers = probe_runtime_blockers(profile, resolved_artifact_source)
    runtime_warnings = probe_runtime_warnings(profile, resolved_artifact_source)
    if runtime_blockers:
        blocked_screen = capture_screenshot(pid, run_dir, f"{mode}_blocked")
        startup_classification = start_result.get("proof_classification") if isinstance(start_result.get("proof_classification"), dict) else {}
        proof_classification = probe_proof_classification(
            verdict="blocked_runtime_prereqs",
            startup_classification=startup_classification,
            step_reports=[],
            artifact_patterns=artifact_patterns,
            matches_by_pattern=[],
            wait_step_summary={},
            abort_report=None,
        )
        report = {
            "ok": True,
            "mode": mode,
            "scenario": str(scenario.get("name", args.scenario)),
            "contract": {
                "profile": profile,
                "world": world,
                "profile_snapshot": profile_snapshot,
                "profile_snapshot_profile": profile_snapshot_profile,
                "fixture": fixture,
                "fixture_profile": fixture_profile,
                "replace_existing_worlds": replace_existing_worlds,
                "advance_turns": advance_count,
                "settle_seconds": settle_seconds,
                "artifact_source": resolved_artifact_source,
                "artifact_patterns": artifact_patterns,
                "steps": steps,
                "capture_world_after": capture_world_after,
            },
            "startup": start_result,
            "steps": [
                {
                    "index": index,
                    "kind": str(step.get("kind", "")).strip().lower(),
                    "label": str(step.get("label", f"step_{index:02d}")).strip() or f"step_{index:02d}",
                    "status": "skipped_runtime_blocker",
                }
                for index, step in enumerate(steps, start=1)
            ],
            "screen": {
                "status": "captured",
                "before": blocked_screen.get("screen_summary", {}),
                "after": {},
            },
            "tests": {
                "status": "not_run",
                "recommended_command": recommended_test_command,
            },
            "artifacts": {
                "status": "blocked",
                "path": "",
                "source_log": str(artifact_log),
                "match_patterns": artifact_patterns,
                "matches_by_pattern": [],
                "matched_lines": [],
                "line_count": 0,
            },
            "runtime_blockers": runtime_blockers,
            "runtime_warnings": runtime_warnings,
            "proof_classification": proof_classification,
            "evidence_class": proof_classification.get("evidence_class", "startup/load-or-inconclusive"),
            "feature_proof": proof_classification.get("feature_proof", False),
            "verdict": "blocked_runtime_prereqs",
        }
        finalize_probe_report(run_dir, report, cleanup_pid=pid, report_filename=report_filename)
        return 0

    artifact_start = artifact_log.stat().st_size if artifact_log.exists() else 0
    screen_before = capture_screenshot(pid, run_dir, f"{mode}_before")
    step_reports = execute_probe_steps(
        pid,
        run_dir,
        steps,
        artifact_log=artifact_log,
        artifact_baseline=artifact_start,
        filter_debug_noise=filter_debug_noise,
        artifact_patterns=artifact_patterns,
    )
    derived_screen_reports = render_derived_screens(run_dir, derived_screens)
    screen_after = capture_screenshot(pid, run_dir, f"{mode}_after")

    artifact_text = read_log_delta(artifact_log, artifact_start, filter_debug_noise=filter_debug_noise)
    artifact_path = run_dir / f"{mode}.artifacts.log"
    if artifact_text:
        artifact_path.write_text(artifact_text, encoding="utf-8")
    matches_by_pattern = []
    all_matched_lines: List[str] = []
    for pattern in artifact_patterns:
        lines = [line for line in artifact_text.splitlines() if pattern in line]
        matches_by_pattern.append({"pattern": pattern, "lines": lines})
        all_matched_lines.extend(lines)

    companion_helpers: List[Dict[str, Any]] = []
    abort_report: Optional[Dict[str, Any]] = None
    for step_report in step_reports:
        step_abort = step_report.get("abort")
        if abort_report is None and isinstance(step_abort, dict):
            abort_report = step_abort
        for helper_key in ("screen_text", "screen_after_text"):
            helper = step_report.get(helper_key)
            if isinstance(helper, dict):
                companion_helpers.append(helper)
    patrol_summary = summarize_patrol_artifacts(str(scenario.get("name", args.scenario)), all_matched_lines)
    if patrol_summary is not None:
        companion_helpers.append(write_patrol_summary(run_dir, patrol_summary))
    wait_step_summary = summarize_wait_step_ledgers(step_reports)
    step_ledger = build_probe_step_ledger(step_reports)
    step_ledger_summary = summarize_probe_step_ledger(step_ledger)
    write_json(run_dir / f"{mode}.step_ledger.json", {
        "mode": mode,
        "scenario": str(scenario.get("name", args.scenario)),
        "step_ledger_summary": step_ledger_summary,
        "step_ledger": step_ledger,
    })

    verdict = "inconclusive_no_artifact_match"
    screen_summary = start_result.get("screen", {}) if isinstance(start_result.get("screen"), dict) else {}
    version_matches_runtime = screen_summary.get("version_matches_runtime_paths")
    if abort_report is not None:
        verdict = str(abort_report.get("verdict", "inconclusive_screen_text_guard"))
    elif version_matches_runtime is False:
        verdict = "inconclusive_version_mismatch"
    elif wait_step_summary["status"] == "blocked_wait_step":
        verdict = "blocked_wait_step_ledger"
    elif wait_step_summary["status"] == "yellow_wait_step_unverified":
        verdict = "yellow_wait_step_unverified"
    elif step_ledger_summary["status"] == "red_step_local_proof_failed":
        verdict = "blocked_step_local_proof"
    elif step_ledger_summary["status"] == "yellow_step_local_proof_incomplete":
        verdict = "yellow_step_local_proof_incomplete"
    elif any(entry["lines"] for entry in matches_by_pattern):
        verdict = "artifacts_matched"
    elif not artifact_text.strip():
        verdict = "inconclusive_no_new_artifacts"

    startup_classification = start_result.get("proof_classification") if isinstance(start_result.get("proof_classification"), dict) else {}
    proof_classification = probe_proof_classification(
        verdict=verdict,
        startup_classification=startup_classification,
        step_reports=step_reports,
        artifact_patterns=artifact_patterns,
        matches_by_pattern=matches_by_pattern,
        wait_step_summary=wait_step_summary,
        abort_report=abort_report,
        step_ledger_summary=step_ledger_summary,
    )

    report = {
        "ok": True,
        "mode": mode,
        "scenario": str(scenario.get("name", args.scenario)),
        "contract": {
            "profile": profile,
            "world": world,
            "profile_snapshot": profile_snapshot,
            "profile_snapshot_profile": profile_snapshot_profile,
            "fixture": fixture,
            "fixture_profile": fixture_profile,
            "replace_existing_worlds": replace_existing_worlds,
            "advance_turns": advance_count,
            "settle_seconds": settle_seconds,
            "artifact_source": resolved_artifact_source,
            "artifact_patterns": artifact_patterns,
            "steps": steps,
            "derived_screens": derived_screens,
            "capture_world_after": capture_world_after,
        },
        "startup": start_result,
        "steps": step_reports,
        "step_ledger": step_ledger,
        "step_ledger_summary": step_ledger_summary,
        "derived_screens": derived_screen_reports,
        "companion_helpers": companion_helpers,
        "screen": {
            "status": "captured",
            "before": screen_before.get("screen_summary", {}),
            "after": screen_after.get("screen_summary", {}),
        },
        "tests": {
            "status": "not_run",
            "recommended_command": recommended_test_command,
        },
        "artifacts": {
            "status": "captured" if artifact_text else "no_new_artifacts",
            "path": str(artifact_path) if artifact_text else "",
            "source_log": str(artifact_log),
            "match_patterns": artifact_patterns,
            "matches_by_pattern": matches_by_pattern,
            "matched_lines": all_matched_lines,
            "line_count": len(artifact_text.splitlines()) if artifact_text else 0,
        },
        "wait_step_summary": wait_step_summary,
        "runtime_warnings": runtime_warnings,
        "abort": abort_report,
        "proof_classification": proof_classification,
        "evidence_class": proof_classification.get("evidence_class", "startup/load-or-inconclusive"),
        "feature_proof": proof_classification.get("feature_proof", False),
        "verdict": verdict,
    }
    if capture_world_after:
        try:
            report["world_snapshot"] = snapshot_world_state(profile, world, run_dir)
        except Exception as exc:
            report["world_snapshot"] = {
                "status": "failed",
                "error": str(exc),
            }
    if handoff:
        report["cleanup"] = {
            "status": "deferred_handoff",
            "pid": pid,
            "reason": "manual_playtest_handoff",
        }
        report["handoff"] = {
            "pid": pid,
            "run_dir": str(run_dir),
            "report_path": str(run_dir / report_filename),
        }
        (run_dir / "handoff.pid").write_text(f"{pid}\n", encoding="utf-8")
        finalize_probe_report(run_dir, report, report_filename=report_filename)
        return 0

    finalize_probe_report(run_dir, report, cleanup_pid=pid, report_filename=report_filename)
    return 0


def run_probe(args: argparse.Namespace) -> int:
    return run_probe_mode(args, handoff=False)


def run_handoff(args: argparse.Namespace) -> int:
    return run_probe_mode(args, handoff=True)


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
    start_p.add_argument("--profile-snapshot", default="", help="Install this captured profile snapshot before startup.")
    start_p.add_argument("--profile-snapshot-profile", default="", help="Profile snapshot source profile; defaults to the target profile.")
    start_p.add_argument("--fixture", default="", help="Install this fixture before startup.")
    start_p.add_argument("--fixture-profile", default="", help="Fixture source profile; defaults to the target profile.")
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
    install_p.add_argument("--fixture-profile", default="", help="Fixture source profile; defaults to the target profile.")
    install_p.add_argument("--replace", action="store_true", help="Replace existing worlds when installing.")

    scenarios_p = subparsers.add_parser("list-scenarios", help="List packaged live-probe scenario contracts.")
    scenarios_p.add_argument("--profile", default="", help="Ignored today; reserved for future per-profile scenario filtering.")

    probe_p = subparsers.add_parser("probe", help="Run a packaged live-probe scenario contract.")
    probe_p.add_argument("scenario", help="Scenario name, e.g. locker.weather_wait")
    probe_p.add_argument("--profile", default="", help="Override scenario profile.")
    probe_p.add_argument("--world", default="", help="Override scenario world.")
    probe_p.add_argument("--fixture", nargs="?", default=None, help="Override scenario fixture; pass empty string to disable fixture install.")
    probe_p.add_argument("--replace-existing-worlds", action="store_true", help="Allow fixture install to replace existing worlds.")
    probe_p.add_argument("--advance-turns", type=int, default=None, help="Override the deterministic turn-advance count.")
    probe_p.add_argument("--settle-seconds", type=float, default=None, help="Override post-action settle time.")
    probe_p.add_argument("--artifact-pattern", default="", help="Override the artifact substring to match in the debug delta.")
    probe_p.add_argument("--test-command", default="", help="Override the recommended deterministic test command recorded in the report.")
    probe_p.add_argument("--dry-run", action="store_true", help="Resolve the scenario contract and startup plan only.")

    handoff_p = subparsers.add_parser("handoff", help="Run a packaged live setup and leave the game running for manual playtest handoff.")
    handoff_p.add_argument("scenario", help="Scenario name, e.g. bandit.basecamp_playtestkit_restage_mcw")
    handoff_p.add_argument("--profile", default="", help="Override scenario profile.")
    handoff_p.add_argument("--world", default="", help="Override scenario world.")
    handoff_p.add_argument("--fixture", nargs="?", default=None, help="Override scenario fixture; pass empty string to disable fixture install.")
    handoff_p.add_argument("--replace-existing-worlds", action="store_true", help="Allow fixture install to replace existing worlds.")
    handoff_p.add_argument("--advance-turns", type=int, default=None, help="Override the deterministic turn-advance count.")
    handoff_p.add_argument("--settle-seconds", type=float, default=None, help="Override post-action settle time.")
    handoff_p.add_argument("--artifact-pattern", default="", help="Override the artifact substring to match in the debug delta.")
    handoff_p.add_argument("--test-command", default="", help="Override the recommended deterministic test command recorded in the report.")
    handoff_p.add_argument("--dry-run", action="store_true", help="Resolve the scenario contract and startup plan only.")

    repeat_p = subparsers.add_parser("repeatability", help="Run the same packaged probe multiple times and summarize screen-first repeatability evidence.")
    repeat_p.add_argument("scenario", help="Scenario name, e.g. bandit.basecamp_named_spawn_mcw")
    repeat_p.add_argument("--count", type=int, default=None, help="How many probe runs to execute (defaults to scenario repeatability_count or 3).")
    repeat_p.add_argument("--profile", default="", help="Override scenario profile.")
    repeat_p.add_argument("--world", default="", help="Override scenario world.")
    repeat_p.add_argument("--fixture", nargs="?", default=None, help="Override scenario fixture; pass empty string to disable fixture install.")
    repeat_p.add_argument("--replace-existing-worlds", action="store_true", help="Allow fixture install to replace existing worlds.")
    repeat_p.add_argument("--dry-run", action="store_true", help="Resolve the repeatability contract only.")

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
        result = install_fixture(profile, args.fixture, replace=args.replace, fixture_profile=args.fixture_profile)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    if args.command == "list-scenarios":
        print(json.dumps({"scenarios": list_scenarios()}, indent=2, ensure_ascii=False))
        return 0
    if args.command == "probe":
        return run_probe(args)
    if args.command == "handoff":
        return run_handoff(args)
    if args.command == "repeatability":
        return run_repeatability(args)
    parser.error(f"Unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
