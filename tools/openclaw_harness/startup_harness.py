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
from datetime import datetime
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Pattern, Sequence, Tuple

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
    if isinstance( text, str ) and text:
        return text
    lines = screen_text_report.get( "lines", [] )
    if isinstance( lines, list ) and lines:
        return "\n".join( str( line ) for line in lines )
    json_path_value = str( screen_text_report.get( "json_path", "" ) ).strip()
    if json_path_value:
        try:
            payload = json.loads( Path( json_path_value ).read_text( encoding="utf-8" ) )
        except Exception:
            payload = {}
        if isinstance( payload, dict ):
            payload_text = payload.get( "text", "" )
            if isinstance( payload_text, str ) and payload_text:
                return payload_text
            payload_lines = payload.get( "lines", [] )
            if isinstance( payload_lines, list ):
                return "\n".join( str( line ) for line in payload_lines )
    return ""


def artifact_delta_matches_all_patterns( artifact_report: Optional[Dict[str, Any]] ) -> Dict[str, Any]:
    if not isinstance( artifact_report, dict ):
        return {"matched": False, "patterns": [], "missing_patterns": []}
    patterns = [str( pattern ) for pattern in artifact_report.get( "patterns", [] ) if str( pattern )]
    matches_by_pattern = artifact_report.get( "matches_by_pattern", [] )
    matched_patterns: List[str] = []
    missing_patterns: List[str] = []
    if isinstance( matches_by_pattern, list ):
        for entry in matches_by_pattern:
            if not isinstance( entry, dict ):
                continue
            pattern = str( entry.get( "pattern", "" ) )
            lines = entry.get( "lines", [] )
            if isinstance( lines, list ) and lines:
                matched_patterns.append( pattern )
            elif pattern:
                missing_patterns.append( pattern )
    if patterns:
        known_patterns = set( matched_patterns ) | set( missing_patterns )
        missing_patterns.extend( pattern for pattern in patterns if pattern not in known_patterns )
    return {
        "matched": bool( patterns ) and not missing_patterns,
        "patterns": patterns,
        "matched_patterns": matched_patterns,
        "missing_patterns": missing_patterns,
        "artifact_path": str( artifact_report.get( "path", "" ) ),
        "source_log": str( artifact_report.get( "source_log", "" ) ),
    }


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
    artifact_after_wait: Optional[Dict[str, Any]] = None,
    allow_artifact_elapsed_without_menu_ocr: bool = False,
) -> Dict[str, Any]:
    expected_seconds = wait_duration_seconds( expected_duration )
    duration_patterns = wait_menu_expected_duration_patterns( expected_duration )
    menu_body = screen_text_body( menu_text ).lower()
    menu_expected_matches = [pattern for pattern in duration_patterns if pattern.lower() in menu_body]
    menu_has_wait_prompt = "wait" in menu_body and bool( menu_body.strip() )
    before_clock = extract_clock_or_turn_evidence( before_text )
    after_clock = extract_clock_or_turn_evidence( after_text )
    artifact_elapsed = artifact_delta_matches_all_patterns( artifact_after_wait )
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
    elif artifact_elapsed["matched"] and expected_seconds is not None:
        elapsed.update( {
            "status": "artifact_delta_after_bounded_wait",
            "note": "all configured wait artifact patterns appeared after choosing the expected bounded wait duration",
        } )

    wait_status = str( wait_classification.get( "status", "" ) )
    effective_wait_status = wait_status
    if wait_status == "unknown_after_wait" and artifact_elapsed["matched"]:
        effective_wait_status = "completed_by_artifact_delta"
    menu_ocr_deferred_to_artifact_delta = bool(
        allow_artifact_elapsed_without_menu_ocr and artifact_elapsed["matched"] and expected_seconds is not None
    )
    issues: List[str] = []
    if not menu_has_wait_prompt and not menu_ocr_deferred_to_artifact_delta:
        issues.append( "wait_menu_ocr_missing_prompt" )
    if expected_seconds is not None and not menu_expected_matches and not menu_ocr_deferred_to_artifact_delta:
        issues.append( "wait_menu_ocr_missing_expected_duration" )
    if elapsed["status"] == "not_parsed":
        issues.append( "before_after_clock_or_turn_not_parsed" )
    if effective_wait_status == "interrupted_or_prompt_visible":
        verdict = "blocked_wait_interrupted_or_prompt_visible"
    elif effective_wait_status not in {"completed", "completed_by_artifact_delta"}:
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
        "menu_ocr_deferred_to_artifact_delta": menu_ocr_deferred_to_artifact_delta,
        "before_clock_or_turn": before_clock,
        "after_clock_or_turn": after_clock,
        "artifact_elapsed_evidence": artifact_elapsed,
        "elapsed": elapsed,
        "finish_or_interrupt_status": effective_wait_status,
        "issues": issues,
        "verdict": verdict,
        "review_rule": (
            "Artifact matches alone do not make this wait step green. Green normally requires readable wait-menu OCR, "
            "either a parsed before/after clock or turn delta or all configured post-wait cadence artifacts, "
            "and either a finish signal, classified interruption, or completed-by-artifact delta. A scenario may explicitly "
            "defer noisy wait-menu OCR to bounded choice-key plus matched post-wait cadence artifacts."
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


def audit_log_contains(
    run_dir: Path,
    label: str,
    *,
    artifact_log: Optional[Path],
    artifact_baseline: int,
    patterns: List[str],
    required_line_patterns: Optional[List[List[str]]] = None,
    required_any_line_patterns: Optional[List[List[str]]] = None,
    filter_debug_noise: bool = False,
) -> Dict[str, Any]:
    line_pattern_groups = required_line_patterns or []
    any_line_pattern_groups = required_any_line_patterns or []
    any_group_label = " OR ".join(" && ".join(group) for group in any_line_pattern_groups)
    required_items = patterns + [" && ".join(group) for group in line_pattern_groups]
    if any_line_pattern_groups:
        required_items.append(any_group_label)
    if artifact_log is None:
        return {
            "status": "failed",
            "reason": "artifact_log_not_configured",
            "required_items": required_items,
            "missing_required_items": required_items,
            "source_log": "",
        }
    text = read_log_delta(artifact_log, artifact_baseline, filter_debug_noise=filter_debug_noise)
    out_path = run_dir / f"{label}.log_delta.txt"
    if text:
        out_path.write_text(text, encoding="utf-8")
    log_lines = text.splitlines()
    matches_by_pattern: List[Dict[str, Any]] = []
    missing: List[str] = []
    for pattern in patterns:
        lines = [line for line in log_lines if pattern in line]
        matches_by_pattern.append({"pattern": pattern, "lines": lines})
        if not lines:
            missing.append(pattern)
    for group in line_pattern_groups:
        group_label = " && ".join(group)
        lines = [line for line in log_lines if all(pattern in line for pattern in group)]
        matches_by_pattern.append({"pattern": group_label, "line_patterns": group, "lines": lines})
        if not lines:
            missing.append(group_label)
    if any_line_pattern_groups:
        any_lines: List[str] = []
        any_matches: List[Dict[str, Any]] = []
        for group in any_line_pattern_groups:
            group_label = " && ".join(group)
            lines = [line for line in log_lines if all(pattern in line for pattern in group)]
            any_matches.append({"pattern": group_label, "line_patterns": group, "lines": lines})
            any_lines.extend(lines)
        matches_by_pattern.append({
            "pattern": any_group_label,
            "any_line_patterns": any_matches,
            "lines": any_lines,
        })
        if not any_lines:
            missing.append(any_group_label)
    return {
        "status": "required_state_present" if required_items and not missing else ("required_state_missing" if required_items else "scanned"),
        "source_log": str(artifact_log),
        "artifact_path": str(out_path) if text else "",
        "start_size": artifact_baseline,
        "end_size": artifact_log.stat().st_size if artifact_log.exists() else artifact_baseline,
        "required_items": required_items,
        "observed_items": [entry["pattern"] for entry in matches_by_pattern if entry.get("lines")],
        "missing_required_items": missing,
        "matches_by_pattern": matches_by_pattern,
        "matched_lines": [line for entry in matches_by_pattern for line in entry.get("lines", [])],
        "line_count": len(log_lines) if text else 0,
    }



def player_message_log_path(world_dir: Path, player_save: str = "") -> Path:
    selected_player_save = str(player_save or "").strip()
    if selected_player_save:
        if selected_player_save.endswith(".sav.zzip"):
            return world_dir / (selected_player_save[: -len(".sav.zzip")] + ".log")
        return world_dir / (Path(selected_player_save).stem + ".log")
    logs = sorted(world_dir.glob("*.log"))
    if len(logs) == 1:
        return logs[0]
    saves = sorted(world_dir.glob("*.sav.zzip"))
    if len(saves) == 1:
        return world_dir / (saves[0].name[: -len(".sav.zzip")] + ".log")
    raise RuntimeError(f"Expected one player message log in {world_dir}, found {[path.name for path in logs]}")


def audit_player_message_log_contains(
    world_dir: Path,
    run_dir: Path,
    label: str,
    *,
    player_save: str = "",
    changed_since: Optional[Dict[str, Any]] = None,
    patterns: List[str],
    required_line_patterns: Optional[List[List[str]]] = None,
    required_any_line_patterns: Optional[List[List[str]]] = None,
) -> Dict[str, Any]:
    line_pattern_groups = required_line_patterns or []
    any_line_pattern_groups = required_any_line_patterns or []
    any_group_label = " OR ".join(" && ".join(group) for group in any_line_pattern_groups)
    required_items = patterns + [" && ".join(group) for group in line_pattern_groups]
    if any_line_pattern_groups:
        required_items.append(any_group_label)
    log_path = player_message_log_path(world_dir, player_save=player_save)
    baseline = 0
    changed_label = ""
    if changed_since:
        changed_label = str(changed_since.get("label", "") or "")
        try:
            baseline = int(changed_since.get("log_size", 0) or 0)
        except (TypeError, ValueError):
            baseline = 0
    if not log_path.exists():
        return {
            "status": "required_state_missing" if required_items else "baseline_recorded",
            "reason": "player_message_log_not_found",
            "world": world_dir.name,
            "world_dir": str(world_dir),
            "player_save": player_save,
            "source_log": str(log_path),
            "log_size": 0,
            "required_items": required_items,
            "missing_required_items": required_items,
            "changed_since_label": changed_label,
            "changed_since_log_size": baseline,
        }
    raw_bytes = log_path.read_bytes()
    log_size = len(raw_bytes)
    if baseline > log_size:
        delta_bytes = b""
    else:
        delta_bytes = raw_bytes[max(0, baseline):]
    delta = delta_bytes.decode("utf-8", errors="replace")
    log_lines = delta.splitlines()
    matches_by_pattern: List[Dict[str, Any]] = []
    missing: List[str] = []
    for pattern in patterns:
        lines = [line for line in log_lines if pattern in line]
        matches_by_pattern.append({"pattern": pattern, "lines": lines})
        if not lines:
            missing.append(pattern)
    for group in line_pattern_groups:
        group_label = " && ".join(group)
        lines = [line for line in log_lines if all(pattern in line for pattern in group)]
        matches_by_pattern.append({"pattern": group_label, "line_patterns": group, "lines": lines})
        if not lines:
            missing.append(group_label)
    if any_line_pattern_groups:
        any_lines: List[str] = []
        any_matches: List[Dict[str, Any]] = []
        for group in any_line_pattern_groups:
            group_label = " && ".join(group)
            lines = [line for line in log_lines if all(pattern in line for pattern in group)]
            any_matches.append({"pattern": group_label, "line_patterns": group, "lines": lines})
            any_lines.extend(lines)
        matches_by_pattern.append({
            "pattern": any_group_label,
            "any_line_patterns": any_matches,
            "lines": any_lines,
        })
        if not any_lines:
            missing.append(any_group_label)
    matched_lines = [line for entry in matches_by_pattern for line in entry.get("lines", [])]
    matched_path = run_dir / f"{label}.matched_player_messages.txt"
    if matched_lines:
        matched_path.write_text("\n".join(matched_lines).rstrip() + "\n", encoding="utf-8")
    return {
        "status": "required_state_present" if required_items and not missing else ("required_state_missing" if required_items else "baseline_recorded"),
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": player_save,
        "source_log": str(log_path),
        "artifact_path": str(matched_path) if matched_lines else "",
        "log_size": log_size,
        "changed_since_label": changed_label,
        "changed_since_log_size": baseline,
        "required_items": required_items,
        "observed_items": [entry["pattern"] for entry in matches_by_pattern if entry.get("lines")],
        "missing_required_items": missing,
        "matches_by_pattern": matches_by_pattern,
        "matched_lines": matched_lines,
        "line_count": len(log_lines),
        "capture_policy": "matched decisive player-message lines only; no broad message-log dump",
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
    classification_text = after_text
    report["wait_classification"] = classify_wait_screen_text(
        classification_text, complete_patterns, interrupt_patterns
    )

    interrupt_response_key = str(step.get("interrupt_response_key", "") or "").strip()
    max_interrupt_responses = int(step.get("max_interrupt_responses", 0) or 0)
    if interrupt_response_key and max_interrupt_responses > 0:
        responses: List[Dict[str, Any]] = []
        for response_index in range(max_interrupt_responses):
            if report["wait_classification"].get("status") != "interrupted_or_prompt_visible":
                break
            response_label = f"{label}.interrupt_response_{response_index + 1}"
            response_wait_seconds = float(step.get("interrupt_response_wait_seconds", completion_wait_seconds) or 0.0)
            peekaboo_press_sequence(pid, [interrupt_response_key], delay_ms=delay_ms)
            if response_wait_seconds > 0:
                time.sleep(response_wait_seconds)
            response_capture = capture_screenshot(pid, run_dir, response_label)
            response_text = capture_screen_text_artifact(
                run_dir, response_label, response_capture, tail_lines=tail_lines
            )
            response_artifact_after = capture_wait_artifact_delta(
                artifact_log,
                run_dir,
                label,
                f"after_interrupt_response_{response_index + 1}",
                wait_start_size,
                state_patterns,
                filter_debug_noise=filter_debug_noise,
            )
            response_classification = classify_wait_screen_text(
                response_text, complete_patterns, interrupt_patterns
            )
            responses.append({
                "response_key": interrupt_response_key,
                "response_index": response_index + 1,
                "response_label": response_label,
                "response_wait_seconds": response_wait_seconds,
                "screen_after_response": response_capture.get("screen_summary", {}),
                "screen_after_response_text": response_text,
                "artifact_after_response": response_artifact_after,
                "wait_classification_after_response": response_classification,
            })
            classification_text = response_text
            report["artifact_after_wait"] = response_artifact_after
            report["wait_classification"] = response_classification
        report["interrupt_responses"] = responses

    report["wait_step_ledger"] = classify_wait_step_ledger(
        label=label,
        choice_key=choice_key,
        expected_duration=expected_duration,
        before_text=before_text,
        menu_text=menu_text,
        after_text=classification_text,
        wait_classification=report["wait_classification"],
        artifact_after_wait=report["artifact_after_wait"],
        allow_artifact_elapsed_without_menu_ocr=bool(
            step.get("allow_artifact_elapsed_without_menu_ocr", False)
        ),
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
    count_selection_mode: str = "type_before_select",
) -> str:
    selector = query_or_slot.strip()
    if not selector:
        raise SystemExit("Drop-item selector cannot be empty")
    if count <= 0:
        raise SystemExit("Drop-item count must be > 0")

    peekaboo_press_sequence(pid, ["d"], delay_ms=delay_ms)
    time.sleep(menu_settle_seconds)

    selection_mode = "slot" if looks_like_inventory_slot(selector) else "filter"
    selection_key = selector if selection_mode == "slot" else "l"
    if selection_mode == "filter":
        apply_uilist_filter(
            pid,
            selector,
            delay_ms=delay_ms,
            type_delay_ms=type_delay_ms,
            settle_seconds=prompt_settle_seconds,
        )

    count_selection_mode = count_selection_mode.strip() or "type_before_select"
    if count_selection_mode not in {"type_before_select", "mark_with_count"}:
        raise SystemExit(f"Unsupported drop-item count_selection_mode: {count_selection_mode}")

    if count > 1 and count_selection_mode == "mark_with_count":
        # MARK_WITH_COUNT applies to the selector's selected entries, not merely the
        # highlighted filtered row.  Prime the filtered/slot row first, then replace
        # that selection with the exact requested count.
        peekaboo_press_sequence(pid, [selection_key], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
        peekaboo_press_sequence(pid, ["!"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
        fill_numeric_prompt(
            pid,
            count,
            delay_ms=delay_ms,
            type_delay_ms=type_delay_ms,
            settle_seconds=prompt_settle_seconds,
        )
    else:
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



def iter_map_triples(value: Any) -> Iterable[Tuple[int, int, Any]]:
    if not isinstance(value, list):
        return
    for entry in value:
        if isinstance(entry, list) and len(entry) >= 3:
            try:
                yield int(entry[0]), int(entry[1]), entry[2]
            except (TypeError, ValueError):
                continue
    for index in range(0, len(value) - 2, 3):
        try:
            yield int(value[index]), int(value[index + 1]), value[index + 2]
        except (TypeError, ValueError):
            continue


def summarize_map_field(payload: Any) -> Dict[str, Any]:
    if isinstance(payload, list) and payload:
        return {
            "field_id": payload[0],
            "intensity": payload[1] if len(payload) > 1 else None,
            "age_turns": payload[2] if len(payload) > 2 else None,
        }
    return {"raw": payload}


def map_item_typeid(item: Any) -> str:
    if isinstance(item, dict):
        return str(item.get("typeid", ""))
    if isinstance(item, list) and len(item) == 2 and isinstance(item[0], dict):
        return str(item[0].get("typeid", ""))
    return str(item)


def summarize_map_items(payload: Any) -> List[str]:
    if not isinstance(payload, list):
        return [map_item_typeid(payload)] if payload else []
    if len(payload) == 2 and isinstance(payload[0], dict):
        typeid = map_item_typeid(payload)
        return [typeid] if typeid else []
    return [typeid for typeid in (map_item_typeid(item) for item in payload) if typeid]


def decode_submap_rle(raw: Any, *, context: str) -> List[Any]:
    """Decode submap terrain/radiation RLE into 144 row-major values."""
    if not isinstance(raw, list):
        return []
    flat: List[Any] = []
    for entry in raw:
        if isinstance(entry, list) and len(entry) >= 2:
            try:
                count = int(entry[1])
            except (TypeError, ValueError):
                raise RuntimeError(f"Submap RLE count is invalid in {context}: {entry!r}")
            if count <= 0:
                raise RuntimeError(f"Submap RLE count must be positive in {context}: {entry!r}")
            flat.extend([entry[0]] * count)
        else:
            flat.append(entry)
    return flat


def submap_rle_value_at(raw: Any, local: Tuple[int, int, int], *, context: str) -> Any:
    flat = decode_submap_rle(raw, context=context)
    index = int(local[1]) * 12 + int(local[0])
    if index < 0 or index >= len(flat):
        return None
    return flat[index]


def submap_radiation_value_at(raw: Any, local: Tuple[int, int, int], *, context: str) -> Any:
    """Decode saved submap radiation RLE, which is flat strength/count pairs."""
    if not isinstance(raw, list):
        return None
    target_index = int(local[1]) * 12 + int(local[0])
    if target_index < 0:
        return None
    cell_index = 0
    iterator = iter(raw)
    for strength in iterator:
        try:
            count_raw = next(iterator)
        except StopIteration as exc:
            raise RuntimeError(f"Submap radiation RLE has dangling strength in {context}: {raw!r}") from exc
        try:
            count = int(count_raw)
        except (TypeError, ValueError):
            raise RuntimeError(f"Submap radiation RLE count is invalid in {context}: {count_raw!r}")
        if count <= 0:
            raise RuntimeError(f"Submap radiation RLE count must be positive in {context}: {count_raw!r}")
        if cell_index <= target_index < cell_index + count:
            return strength
        cell_index += count
    return None


def parse_map_tile_offsets(raw_offsets: Sequence[Any]) -> List[Tuple[int, int, int]]:
    if not raw_offsets:
        return []
    if len(raw_offsets) % 3 != 0:
        raise ValueError("offset values must be repeated triples: dx dy dz")
    offsets: List[Tuple[int, int, int]] = []
    for index in range(0, len(raw_offsets), 3):
        offsets.append((int(raw_offsets[index]), int(raw_offsets[index + 1]), int(raw_offsets[index + 2])))
    return offsets


def audit_map_tiles_near_player(
    world_dir: Path,
    *,
    player_save: str = "",
    radius: int = 2,
    offsets: Optional[List[Tuple[int, int, int]]] = None,
    required_terrain: Optional[List[str]] = None,
    required_fields: Optional[List[str]] = None,
    required_items: Optional[List[str]] = None,
    required_furniture: Optional[List[str]] = None,
    required_traps: Optional[List[str]] = None,
    required_radiation: Optional[List[int]] = None,
) -> Dict[str, Any]:
    """Read-only saved-map audit for terrain/fields/items/furniture/traps/radiation near the saved player."""
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    player_abs_omt, player_location = load_player_abs_omt(world_dir, selected_player_save)
    maps_dir = world_dir / "maps"
    explicit_offsets = offsets is not None
    if offsets is None:
        scan_radius = max(0, int(radius))
        offsets = [(dx, dy, 0) for dy in range(-scan_radius, scan_radius + 1) for dx in range(-scan_radius, scan_radius + 1)]

    extracted_packs: set[str] = set()
    preexisting_packs: set[str] = {path.name for path in maps_dir.iterdir() if path.is_dir()} if maps_dir.exists() else set()
    tile_reports: List[Dict[str, Any]] = []

    try:
        for dx, dy, dz in offsets:
            target = [int(player_location[0]) + dx, int(player_location[1]) + dy, int(player_location[2]) + dz]
            abs_sm = (target[0] // 12, target[1] // 12, target[2])
            abs_omt = (target[0] // 24, target[1] // 24, target[2])
            local = (target[0] - abs_sm[0] * 12, target[1] - abs_sm[1] * 12, target[2])
            pack_stem = f"{abs_omt[0] // 32}.{abs_omt[1] // 32}.{abs_omt[2]}"
            pack_dir = maps_dir / pack_stem
            if not pack_dir.exists():
                pack_zzip = maps_dir / f"{pack_stem}.zzip"
                if not pack_zzip.exists():
                    continue
                run_zzip(pack_zzip)
                extracted_packs.add(pack_stem)
            map_path = pack_dir / f"{abs_omt[0]}.{abs_omt[1]}.{abs_omt[2]}.map"
            if not map_path.exists():
                continue
            map_payload = json.loads(map_path.read_text(encoding="utf-8"))
            if not isinstance(map_payload, list):
                continue
            submap = next(
                (
                    sub
                    for sub in map_payload
                    if isinstance(sub, dict) and sub.get("coordinates") == [abs_sm[0], abs_sm[1], abs_sm[2]]
                ),
                None,
            )
            if not isinstance(submap, dict):
                continue
            terrain_raw = submap_rle_value_at(
                submap.get("terrain"),
                local,
                context=f"{map_path}:{abs_sm}:{local}:terrain",
            )
            terrain = str(terrain_raw) if terrain_raw is not None else ""
            radiation_raw = submap_radiation_value_at(
                submap.get("radiation"),
                local,
                context=f"{map_path}:{abs_sm}:{local}:radiation",
            )
            radiation: Optional[int]
            try:
                radiation = int(radiation_raw) if radiation_raw is not None else None
            except (TypeError, ValueError):
                radiation = None
            fields = [summarize_map_field(payload) for x, y, payload in iter_map_triples(submap.get("fields")) if x == local[0] and y == local[1]]
            items: List[str] = []
            for x, y, payload in iter_map_triples(submap.get("items")):
                if x == local[0] and y == local[1]:
                    items.extend(summarize_map_items(payload))
            furniture = [str(payload) for x, y, payload in iter_map_triples(submap.get("furniture")) if x == local[0] and y == local[1]]
            traps = [str(payload) for x, y, payload in iter_map_triples(submap.get("traps")) if x == local[0] and y == local[1]]
            if explicit_offsets or terrain or fields or items or furniture or traps or radiation not in (None, 0):
                tile_reports.append({
                    "offset_ms": [dx, dy, dz],
                    "target_location_ms": target,
                    "target_abs_omt": list(abs_omt),
                    "target_abs_sm": list(abs_sm),
                    "local_ms": [local[0], local[1], local[2]],
                    "terrain": terrain,
                    "fields": fields,
                    "items": items,
                    "furniture": furniture,
                    "traps": traps,
                    "radiation": radiation,
                })
    finally:
        for stem in sorted(extracted_packs):
            if stem not in preexisting_packs:
                pack_dir = maps_dir / stem
                if pack_dir.exists():
                    shutil.rmtree(pack_dir)

    required_terrain = [terrain for terrain in (required_terrain or []) if terrain]
    required_fields = [field for field in (required_fields or []) if field]
    required_items = [item for item in (required_items or []) if item]
    required_furniture = [furn for furn in (required_furniture or []) if furn]
    required_traps = [trap for trap in (required_traps or []) if trap]
    required_radiation = [int(rad) for rad in (required_radiation or [])]
    observed_terrain = [str(tile.get("terrain")) for tile in tile_reports if str(tile.get("terrain", ""))]
    observed_field_ids = [str(field.get("field_id")) for tile in tile_reports for field in tile.get("fields", []) if isinstance(field, dict)]
    observed_items = [str(item) for tile in tile_reports for item in tile.get("items", [])]
    observed_furniture = [str(furn) for tile in tile_reports for furn in tile.get("furniture", [])]
    observed_traps = [str(trap) for tile in tile_reports for trap in tile.get("traps", [])]
    observed_radiation = [int(rad) for rad in (tile.get("radiation") for tile in tile_reports) if rad is not None]
    missing_required_terrain = [terrain for terrain in required_terrain if terrain not in observed_terrain]
    missing_required_fields = [field for field in required_fields if field not in observed_field_ids]
    missing_required_items = [item for item in required_items if item not in observed_items]
    missing_required_furniture = [furn for furn in required_furniture if furn not in observed_furniture]
    missing_required_traps = [trap for trap in required_traps if trap not in observed_traps]
    missing_required_radiation = [rad for rad in required_radiation if rad not in observed_radiation]
    required_count = (
        len(required_terrain)
        + len(required_fields)
        + len(required_items)
        + len(required_furniture)
        + len(required_traps)
        + len(required_radiation)
    )
    missing_count = (
        len(missing_required_terrain)
        + len(missing_required_fields)
        + len(missing_required_items)
        + len(missing_required_furniture)
        + len(missing_required_traps)
        + len(missing_required_radiation)
    )
    if required_count and missing_count == 0:
        status = "required_state_present"
    elif required_count:
        status = "required_state_missing"
    else:
        status = "scanned"
    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_location_ms": player_location,
        "player_abs_omt": list(player_abs_omt),
        "scanned_offsets": len(offsets),
        "required_terrain": required_terrain,
        "required_fields": required_fields,
        "required_items": required_items,
        "required_furniture": required_furniture,
        "required_traps": required_traps,
        "required_radiation": required_radiation,
        "observed_terrain": observed_terrain,
        "observed_field_ids": observed_field_ids,
        "observed_items": observed_items,
        "observed_furniture": observed_furniture,
        "observed_traps": observed_traps,
        "observed_radiation": observed_radiation,
        "missing_required_terrain": missing_required_terrain,
        "missing_required_fields": missing_required_fields,
        "missing_required_items": missing_required_items,
        "missing_required_furniture": missing_required_furniture,
        "missing_required_traps": missing_required_traps,
        "missing_required_radiation": missing_required_radiation,
        "tiles": tile_reports,
        "status": status,
    }


def audit_player_save_mtime(
    world_dir: Path,
    *,
    player_save: str = "",
    changed_since: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Read-only saved-player file mtime audit for post-action save/writeback gates."""
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]
    save_path = world_dir / selected_player_save
    if not save_path.exists():
        raise FileNotFoundError(f"Player save not found: {save_path}")

    stat = save_path.stat()
    save_mtime_ns = int(stat.st_mtime_ns)
    save_mtime = float(stat.st_mtime)
    required_change = changed_since is not None
    baseline_mtime_ns: Optional[int] = None
    baseline_label = ""
    if isinstance(changed_since, dict):
        baseline_label = str(changed_since.get("label", "") or "")
        try:
            baseline_mtime_ns = int(changed_since.get("save_mtime_ns"))
        except (TypeError, ValueError):
            baseline_mtime_ns = None

    if required_change and baseline_mtime_ns is not None and save_mtime_ns > baseline_mtime_ns:
        status = "required_state_present"
    elif required_change:
        status = "required_state_missing"
    else:
        status = "baseline_recorded"

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "save_path": str(save_path),
        "save_mtime": save_mtime,
        "save_mtime_ns": save_mtime_ns,
        "save_mtime_iso": datetime.fromtimestamp(save_mtime).isoformat(timespec="microseconds"),
        "required_change_since_label": baseline_label,
        "required_change_since_mtime_ns": baseline_mtime_ns,
        "mtime_changed_since_required_label": (
            save_mtime_ns > baseline_mtime_ns if baseline_mtime_ns is not None else False
        ),
        "missing_required_items": ["player_save_mtime_changed"] if status == "required_state_missing" else [],
        "status": status,
    }


def load_item_definitions_for(typeids: Iterable[str]) -> Dict[str, Dict[str, Any]]:
    wanted = {str(typeid).strip() for typeid in typeids if str(typeid).strip()}
    found: Dict[str, Dict[str, Any]] = {}
    if not wanted:
        return found
    for path in (repo_root() / "data" / "json").rglob("*.json"):
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        entries = payload if isinstance(payload, list) else [payload]
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            item_id = str(entry.get("id", "")).strip()
            if item_id in wanted and item_id not in found:
                found[item_id] = entry
        if len(found) == len(wanted):
            break
    return found


def item_display_name_from_definition(entry: Dict[str, Any]) -> str:
    name = entry.get("name")
    if isinstance(name, dict):
        return str(name.get("str") or name.get("str_sp") or name.get("ctxt") or "").strip()
    return str(name or "").strip()


def item_use_action_summary(entry: Dict[str, Any]) -> List[Dict[str, Any]]:
    raw_use_action = entry.get("use_action")
    actions = raw_use_action if isinstance(raw_use_action, list) else [raw_use_action]
    summary: List[Dict[str, Any]] = []
    for action in actions:
        if isinstance(action, str):
            summary.append({"type": action})
        elif isinstance(action, dict):
            row: Dict[str, Any] = {"type": str(action.get("type", "")).strip()}
            if action.get("furn_type"):
                row["furn_type"] = str(action.get("furn_type"))
            if action.get("terrain"):
                row["terrain"] = str(action.get("terrain"))
            summary.append(row)
    return [row for row in summary if row.get("type")]


def iter_saved_item_entries(entry: Any, path: str) -> Iterable[Tuple[str, Dict[str, Any]]]:
    """Yield a saved item and recursive container-pocket contents with stable JSON-ish paths."""
    if not isinstance(entry, dict):
        return
    yield path, entry
    contents_obj = entry.get("contents")
    if not isinstance(contents_obj, dict):
        return
    pockets = contents_obj.get("contents", [])
    if not isinstance(pockets, list):
        return
    for pocket_index, pocket in enumerate(pockets):
        if not isinstance(pocket, dict):
            continue
        pocket_contents = pocket.get("contents", [])
        if not isinstance(pocket_contents, list):
            continue
        for item_index, child in enumerate(pocket_contents):
            child_path = f"{path}/contents/contents[{pocket_index}]/contents[{item_index}]"
            yield from iter_saved_item_entries(child, child_path)


def saved_player_worn_items(player: Dict[str, Any]) -> List[Dict[str, Any]]:
    worn = player.get("worn", {})
    if isinstance(worn, dict):
        worn_items = worn.get("worn", [])
    else:
        worn_items = worn
    return worn_items if isinstance(worn_items, list) else []


def saved_player_item_rows(
    player: Dict[str, Any]
) -> Tuple[List[Tuple[str, Dict[str, Any]]], List[Tuple[str, Dict[str, Any]]]]:
    """Return (live-accessible worn/held rows, legacy top-level inv rows).

    The modern inventory selector walks wielded gear plus worn items and their
    contained items.  It does not expose old top-level ``player.inv`` rows by
    itself, so keep that save bucket separate to avoid treating an inaccessible
    fixture injection as live Use-item proof.
    """
    accessible_rows: List[Tuple[str, Dict[str, Any]]] = []
    weapon = player.get("weapon")
    if isinstance(weapon, dict):
        accessible_rows.extend(iter_saved_item_entries(weapon, "player.weapon"))
    for worn_index, worn_item in enumerate(saved_player_worn_items(player)):
        accessible_rows.extend(
            iter_saved_item_entries(worn_item, f"player.worn.worn[{worn_index}]")
        )

    legacy_rows: List[Tuple[str, Dict[str, Any]]] = []
    inventory = player.get("inv", [])
    if not isinstance(inventory, list):
        raise RuntimeError("Extracted player save has non-list inventory")
    for inv_index, inv_item in enumerate(inventory):
        legacy_rows.extend(iter_saved_item_entries(inv_item, f"player.inv[{inv_index}]"))
    return accessible_rows, legacy_rows


def summarize_saved_item_rows(
    rows: Iterable[Tuple[str, Dict[str, Any]]]
) -> Tuple[Dict[str, int], Dict[str, List[Dict[str, Any]]]]:
    counts: Dict[str, int] = {}
    item_rows: Dict[str, List[Dict[str, Any]]] = {}
    for path, entry in rows:
        typeid = str(entry.get("typeid", "")).strip()
        if not typeid:
            continue
        counts[typeid] = counts.get(typeid, 0) + 1
        item_rows.setdefault(typeid, []).append({
            "typeid": typeid,
            "source_path": path,
            "custom_invlet": str(entry.get("invlet", "") or ""),
            "charges": entry.get("charges"),
            "count_by_charges": entry.get("count_by_charges"),
            "flags": sorted(
                str(flag) for flag in entry.get("item_tags", []) if str(flag).strip()
            ) if isinstance(entry.get("item_tags"), list) else [],
        })
    return counts, item_rows


def saved_item_nested_charge_sum(entry: Any, *, ammo_type: str = "") -> int:
    """Sum charges carried by nested ammo/items inside a saved item payload."""
    total = 0
    if not isinstance(entry, dict):
        return total
    typeid = str(entry.get("typeid", "")).strip()
    if (not ammo_type or typeid == ammo_type) and "charges" in entry:
        try:
            total += int(entry.get("charges") or 0)
        except (TypeError, ValueError):
            pass
    contents_obj = entry.get("contents")
    if not isinstance(contents_obj, dict):
        return total
    pockets = contents_obj.get("contents", [])
    if not isinstance(pockets, list):
        return total
    for pocket in pockets:
        if not isinstance(pocket, dict):
            continue
        pocket_contents = pocket.get("contents", [])
        if not isinstance(pocket_contents, list):
            continue
        for child in pocket_contents:
            total += saved_item_nested_charge_sum(child, ammo_type=ammo_type)
    return total


def audit_saved_player_items(
    world_dir: Path,
    *,
    player_save: str = "",
    required_items: Optional[List[str]] = None,
    required_weapon: str = "",
    required_weapon_ammo_type: str = "",
    required_weapon_ammo_min: int = 0,
) -> Dict[str, Any]:
    """Read-only saved-player inventory/wield audit for exact item type ids plus deploy metadata."""
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    player_save_path = world_dir / selected_player_save
    if not player_save_path.exists():
        raise FileNotFoundError(f"Player save not found: {player_save_path}")
    if player_save_path.suffix != ".zzip":
        raise RuntimeError(f"Player inventory audit expects .zzip save path: {player_save_path}")

    extracted_save = player_save_path.with_suffix("")
    run_zzip(player_save_path)
    if not extracted_save.exists():
        raise RuntimeError(f"Player inventory audit did not extract save: {extracted_save}")

    try:
        payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    finally:
        if extracted_save.exists():
            extracted_save.unlink()
    if not isinstance(payload, dict):
        raise RuntimeError(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise RuntimeError(f"Extracted player save is missing player object: {extracted_save}")
    accessible_rows, legacy_rows = saved_player_item_rows(player)
    counts, item_rows = summarize_saved_item_rows(accessible_rows)
    legacy_counts, legacy_item_rows = summarize_saved_item_rows(legacy_rows)
    weapon = player.get("weapon")
    observed_weapon = str(weapon.get("typeid", "")) if isinstance(weapon, dict) else ""
    required_weapon_ammo_type = str(required_weapon_ammo_type or "").strip()
    required_weapon_ammo_min = max(0, int(required_weapon_ammo_min or 0))
    observed_weapon_ammo_remaining = (
        saved_item_nested_charge_sum(weapon, ammo_type=required_weapon_ammo_type)
        if isinstance(weapon, dict)
        else 0
    )

    required = [item for item in (required_items or []) if item]
    required_weapon = str(required_weapon or "").strip()
    interesting_typeids = sorted(set(required) if required else set(counts))
    definitions = load_item_definitions_for(interesting_typeids)
    item_details: Dict[str, Dict[str, Any]] = {}
    for typeid in interesting_typeids:
        definition = definitions.get(typeid, {})
        item_details[typeid] = {
            "count": counts.get(typeid, 0),
            "display_name": item_display_name_from_definition(definition),
            "use_actions": item_use_action_summary(definition),
            "saved_rows": item_rows.get(typeid, []),
            "legacy_top_level_inv_rows": legacy_item_rows.get(typeid, []),
        }

    missing_required_items = [item for item in required if counts.get(item, 0) < 1]
    missing_required_weapon = (
        [required_weapon]
        if required_weapon and observed_weapon != required_weapon
        else []
    )
    missing_required_weapon_ammo = (
        [f"{required_weapon_ammo_type or 'nested_ammo'}>={required_weapon_ammo_min}"]
        if required_weapon_ammo_min and observed_weapon_ammo_remaining < required_weapon_ammo_min
        else []
    )
    required_count = len(required) + (1 if required_weapon else 0) + (1 if required_weapon_ammo_min else 0)
    missing_count = len(missing_required_items) + len(missing_required_weapon) + len(missing_required_weapon_ammo)
    if required_count and not missing_count:
        status = "required_state_present"
    elif required_count:
        status = "required_state_missing"
    else:
        status = "scanned"

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "required_items": required,
        "required_weapon": required_weapon,
        "required_weapon_ammo_type": required_weapon_ammo_type,
        "required_weapon_ammo_min": required_weapon_ammo_min,
        "observed_weapon": observed_weapon,
        "observed_weapon_ammo_remaining": observed_weapon_ammo_remaining,
        "observed_items": sorted(counts),
        "inventory_counts": dict(sorted(counts.items())),
        "live_accessible_counts": dict(sorted(counts.items())),
        "legacy_top_level_inv_counts": dict(sorted(legacy_counts.items())),
        "item_details": item_details,
        "missing_required_items": missing_required_items,
        "missing_required_weapon": missing_required_weapon,
        "missing_required_weapon_ammo": missing_required_weapon_ammo,
        "status": status,
    }


def audit_saved_active_monsters(
    world_dir: Path,
    *,
    player_save: str = "",
    required_monsters: Optional[List[Dict[str, Any]]] = None,
    required_typeids: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Read-only saved-player audit for active monster type/location state.

    Debug-spawned monsters are saved in the player save's ``active_monsters``
    array, not in the submap spawn-point list. Keep this as a same-save
    metadata gate so a completed debug menu macro cannot be credited unless
    the saved game contains the exact monster state the scenario claimed.
    """
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    player_save_path = world_dir / selected_player_save
    if not player_save_path.exists():
        raise FileNotFoundError(f"Player save not found: {player_save_path}")
    if player_save_path.suffix != ".zzip":
        raise RuntimeError(f"Active monster audit expects .zzip save path: {player_save_path}")

    extracted_save = player_save_path.with_suffix("")
    run_zzip(player_save_path)
    if not extracted_save.exists():
        raise RuntimeError(f"Active monster audit did not extract save: {extracted_save}")

    try:
        payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    finally:
        if extracted_save.exists():
            extracted_save.unlink()
    if not isinstance(payload, dict):
        raise RuntimeError(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise RuntimeError(f"Extracted player save is missing player object: {extracted_save}")
    player_location_raw = player.get("location", [])
    if not isinstance(player_location_raw, list) or len(player_location_raw) < 3:
        raise RuntimeError(f"Extracted player save is missing player location: {extracted_save}")
    player_location = [int(player_location_raw[0]), int(player_location_raw[1]), int(player_location_raw[2])]

    active_raw = payload.get("active_monsters", [])
    if not isinstance(active_raw, list):
        raise RuntimeError(f"Extracted player save has non-list active_monsters: {extracted_save}")

    monsters: List[Dict[str, Any]] = []
    counts: Dict[str, int] = {}
    for index, monster in enumerate(active_raw):
        if not isinstance(monster, dict):
            continue
        typeid = str(monster.get("typeid", "")).strip()
        location_raw = monster.get("location", [])
        location: List[int] = []
        offset: List[int] = []
        if isinstance(location_raw, list) and len(location_raw) >= 3:
            location = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
            offset = [
                location[0] - player_location[0],
                location[1] - player_location[1],
                location[2] - player_location[2],
            ]
        if typeid:
            counts[typeid] = counts.get(typeid, 0) + 1
        monsters.append({
            "index": index,
            "typeid": typeid,
            "location_ms": location,
            "offset_ms": offset,
            "friendly": monster.get("friendly"),
            "hallucination": monster.get("hallucination"),
            "dead": monster.get("dead"),
            "hp": monster.get("hp"),
            "faction": monster.get("faction"),
            "unique_name": monster.get("unique_name", ""),
            "nickname": monster.get("nickname", ""),
        })

    required: List[Dict[str, Any]] = []
    for typeid in required_typeids or []:
        normalized = str(typeid).strip()
        if normalized:
            required.append({"typeid": normalized})
    for raw in required_monsters or []:
        if not isinstance(raw, dict):
            continue
        typeid = str(raw.get("typeid", raw.get("monster", ""))).strip()
        if not typeid:
            continue
        requirement: Dict[str, Any] = {"typeid": typeid}
        offset_raw = raw.get("offset_ms")
        if isinstance(offset_raw, list) and len(offset_raw) >= 3:
            requirement["offset_ms"] = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
        required.append(requirement)

    def matches_requirement(monster: Dict[str, Any], requirement: Dict[str, Any]) -> bool:
        if monster.get("typeid") != requirement.get("typeid"):
            return False
        if "offset_ms" in requirement and monster.get("offset_ms") != requirement.get("offset_ms"):
            return False
        return True

    missing_required_monsters = [
        requirement
        for requirement in required
        if not any(matches_requirement(monster, requirement) for monster in monsters)
    ]
    if required and not missing_required_monsters:
        status = "required_state_present"
    elif required:
        status = "required_state_missing"
    else:
        status = "scanned"

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_location_ms": player_location,
        "required_monsters": required,
        "observed_monster_typeids": sorted(counts),
        "active_monster_counts": dict(sorted(counts.items())),
        "active_monsters": monsters,
        "missing_required_monsters": missing_required_monsters,
        "status": status,
    }


def load_saved_player_payload(world_dir: Path, player_save: str = "") -> Tuple[str, Path, Dict[str, Any], os.stat_result]:
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    player_save_path = world_dir / selected_player_save
    if not player_save_path.exists():
        raise FileNotFoundError(f"Player save not found: {player_save_path}")
    if player_save_path.suffix != ".zzip":
        raise RuntimeError(f"Player payload audit expects .zzip save path: {player_save_path}")

    stat = player_save_path.stat()
    extracted_save = player_save_path.with_suffix("")
    run_zzip(player_save_path)
    if not extracted_save.exists():
        raise RuntimeError(f"Player payload audit did not extract save: {extracted_save}")
    try:
        payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    finally:
        if extracted_save.exists():
            extracted_save.unlink()
    if not isinstance(payload, dict):
        raise RuntimeError(f"Extracted player save is not a JSON object: {extracted_save}")
    return selected_player_save, player_save_path, payload, stat


def audit_saved_game_turn(
    world_dir: Path,
    *,
    player_save: str = "",
    record_baseline: bool = False,
    baseline_metadata: Optional[Dict[str, Any]] = None,
    required_min_delta_turns: Optional[int] = None,
    required_time_of_day_seconds: Optional[int] = None,
    required_time_tolerance_seconds: int = 0,
) -> Dict[str, Any]:
    """Read-only saved-player turn audit for proving real local turn advancement."""
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")

    selected_player_save, player_save_path, payload, stat = load_saved_player_payload(world_dir, player_save)
    turn = int(payload.get("turn", 0) or 0)
    time_of_day_seconds = turn % ( 24 * 60 * 60 )
    time_of_day_text = f"{time_of_day_seconds // 3600:02d}:{( time_of_day_seconds % 3600 ) // 60:02d}:{time_of_day_seconds % 60:02d}"
    player = payload.get("player")
    if not isinstance(player, dict):
        raise RuntimeError(f"Extracted player save is missing player object: {player_save_path}")
    location_raw = player.get("location", [])
    player_location: List[int] = []
    player_abs_omt: List[int] = []
    if isinstance(location_raw, list) and len(location_raw) >= 3:
        player_location = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
        player_abs_omt = [player_location[0] // 24, player_location[1] // 24, player_location[2]]

    baseline_turn: Optional[int] = None
    observed_delta_turns: Optional[int] = None
    missing_required_min_delta_turns = False
    normalized_required_time_of_day: Optional[int] = None
    observed_time_of_day_delta_seconds: Optional[int] = None
    missing_required_time_of_day = False
    if baseline_metadata is not None:
        baseline_turn = int(baseline_metadata.get("turn", 0) or 0)
        observed_delta_turns = turn - baseline_turn
        if required_min_delta_turns is not None:
            missing_required_min_delta_turns = observed_delta_turns < int(required_min_delta_turns)

    if required_time_of_day_seconds is not None:
        normalized_required_time_of_day = int(required_time_of_day_seconds) % ( 24 * 60 * 60 )
        forward_delta = ( time_of_day_seconds - normalized_required_time_of_day ) % ( 24 * 60 * 60 )
        backward_delta = ( normalized_required_time_of_day - time_of_day_seconds ) % ( 24 * 60 * 60 )
        observed_time_of_day_delta_seconds = min(forward_delta, backward_delta)
        missing_required_time_of_day = observed_time_of_day_delta_seconds > max(0, int(required_time_tolerance_seconds))

    if record_baseline:
        status = "baseline_recorded"
    elif baseline_metadata is not None and required_min_delta_turns is not None and not missing_required_min_delta_turns:
        status = "required_state_present"
    elif baseline_metadata is not None and required_min_delta_turns is not None:
        status = "required_state_missing"
    elif required_time_of_day_seconds is not None and not missing_required_time_of_day:
        status = "required_state_present"
    elif required_time_of_day_seconds is not None:
        status = "required_state_missing"
    else:
        status = "scanned"

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_save_path": str(player_save_path),
        "player_save_mtime_ns": int(stat.st_mtime_ns),
        "player_save_mtime_iso": datetime.fromtimestamp(float(stat.st_mtime)).isoformat(timespec="microseconds"),
        "turn": turn,
        "time_of_day_seconds": time_of_day_seconds,
        "time_of_day_text": time_of_day_text,
        "required_time_of_day_seconds": normalized_required_time_of_day,
        "required_time_tolerance_seconds": int(required_time_tolerance_seconds),
        "observed_time_of_day_delta_seconds": observed_time_of_day_delta_seconds,
        "missing_required_time_of_day": missing_required_time_of_day,
        "baseline_turn": baseline_turn,
        "observed_delta_turns": observed_delta_turns,
        "required_min_delta_turns": required_min_delta_turns,
        "missing_required_min_delta_turns": missing_required_min_delta_turns,
        "player_location_ms": player_location,
        "player_abs_omt": player_abs_omt,
        "status": status,
    }


def audit_saved_overmap_npcs(
    world_dir: Path,
    *,
    player_save: str = "",
    required_npcs: Optional[List[Dict[str, Any]]] = None,
    scan_all_overmaps: bool = False,
    record_baseline: bool = False,
    baseline_metadata: Optional[Dict[str, Any]] = None,
    required_observed_npc_count_delta: Optional[int] = None,
    required_new_npcs: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """Read-only saved-overmap audit for NPC/follower target state.

    Debug-spawned follower NPCs are persisted in overmap ``npcs`` records,
    not in the player save's ``unique_npcs`` bucket. Keep this audit as a
    same-save metadata gate after a proven save/writeback so a debug NPC
    primitive cannot be credited from menu navigation alone.
    """
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    player_abs_omt, player_location = load_player_abs_omt(world_dir, selected_player_save)
    overmaps_dir = world_dir / "overmaps"
    if not overmaps_dir.exists():
        raise FileNotFoundError(f"Overmaps dir not found: {overmaps_dir}")

    if scan_all_overmaps:
        overmap_paths = sorted(overmaps_dir.glob("o.*.*.zzip"))
    else:
        player_overmap_x = int(player_abs_omt[0]) // OMAPX
        player_overmap_y = int(player_abs_omt[1]) // OMAPY
        candidate = overmaps_dir / f"o.{player_overmap_x}.{player_overmap_y}.zzip"
        overmap_paths = [candidate] if candidate.exists() else []
    if not overmap_paths:
        raise RuntimeError(f"No overmap .zzip files selected for NPC audit in {overmaps_dir}")

    npcs: List[Dict[str, Any]] = []
    scanned_overmaps: List[str] = []
    for overmap_path in overmap_paths:
        plain_path: Optional[Path] = None
        created_plain = False
        try:
            plain_path, _version_line, payload = extract_overmap_payload(overmap_path)
            created_plain = bool(payload.get("_created_plain"))
            scanned_overmaps.append(str(overmap_path.relative_to(world_dir)))
            raw_npcs = payload.get("npcs", [])
            if not isinstance(raw_npcs, list):
                raise RuntimeError(f"Overmap has non-list npcs array: {overmap_path}")
            for index, npc_payload in enumerate(raw_npcs):
                if not isinstance(npc_payload, dict):
                    continue
                location_raw = npc_payload.get("location", [])
                location: List[int] = []
                offset: List[int] = []
                if isinstance(location_raw, list) and len(location_raw) >= 3:
                    location = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
                    offset = [
                        location[0] - player_location[0],
                        location[1] - player_location[1],
                        location[2] - player_location[2],
                    ]
                npcs.append({
                    "overmap": str(overmap_path.relative_to(world_dir)),
                    "index": index,
                    "id": npc_payload.get("id"),
                    "name": str(npc_payload.get("name", "") or ""),
                    "location_ms": location,
                    "offset_ms": offset,
                    "attitude": npc_payload.get("attitude"),
                    "mission": npc_payload.get("mission"),
                    "my_fac": npc_payload.get("my_fac"),
                    "previous_attitude": npc_payload.get("previous_attitude"),
                    "comp_mission_id": npc_payload.get("comp_mission_id"),
                    "rules": npc_payload.get("rules") if isinstance(npc_payload.get("rules"), dict) else {},
                    "op_of_u": npc_payload.get("op_of_u") if isinstance(npc_payload.get("op_of_u"), dict) else {},
                    "chatbin_missions": (
                        npc_payload.get("chatbin", {}).get("missions", [])
                        if isinstance(npc_payload.get("chatbin"), dict)
                        else []
                    ),
                })
        finally:
            if plain_path is not None and created_plain:
                cleanup_extracted_overmap(plain_path, keep=False)

    def normalize_requirement(raw: Dict[str, Any]) -> Dict[str, Any]:
        requirement: Dict[str, Any] = {}
        for key in ("name", "name_contains", "my_fac"):
            value = str(raw.get(key, "") or "").strip()
            if value:
                requirement[key] = value
        for key in ("attitude", "mission"):
            if raw.get(key) is not None and str(raw.get(key)).strip() != "":
                requirement[key] = int(raw.get(key))
        rules_raw = raw.get("rules", raw.get("required_rules", {}))
        if isinstance(rules_raw, dict):
            rules: Dict[str, Any] = {}
            for rule_key, expected in rules_raw.items():
                rule_name = str(rule_key or "").strip()
                if not rule_name:
                    continue
                if isinstance(expected, bool):
                    rules[rule_name] = expected
                elif isinstance(expected, (int, float)):
                    rules[rule_name] = expected
                elif expected is not None:
                    text = str(expected).strip().lower()
                    if text in {"true", "yes", "1", "on"}:
                        rules[rule_name] = True
                    elif text in {"false", "no", "0", "off"}:
                        rules[rule_name] = False
                    else:
                        rules[rule_name] = str(expected)
            if rules:
                requirement["rules"] = rules
        offset_raw = raw.get("offset_ms")
        if isinstance(offset_raw, list) and len(offset_raw) >= 3:
            requirement["offset_ms"] = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
        max_abs_offset_raw = raw.get("max_abs_offset_ms")
        if isinstance(max_abs_offset_raw, list) and len(max_abs_offset_raw) >= 3:
            requirement["max_abs_offset_ms"] = [
                int(max_abs_offset_raw[0]),
                int(max_abs_offset_raw[1]),
                int(max_abs_offset_raw[2]),
            ]
        return requirement

    required: List[Dict[str, Any]] = []
    for raw in required_npcs or []:
        if not isinstance(raw, dict):
            continue
        requirement = normalize_requirement(raw)
        if requirement:
            required.append(requirement)

    required_new: List[Dict[str, Any]] = []
    for raw in required_new_npcs or []:
        if not isinstance(raw, dict):
            continue
        requirement = normalize_requirement(raw)
        if requirement:
            required_new.append(requirement)

    def matches_requirement(npc_row: Dict[str, Any], requirement: Dict[str, Any]) -> bool:
        if "name" in requirement and npc_row.get("name") != requirement.get("name"):
            return False
        if "name_contains" in requirement and requirement["name_contains"] not in str(npc_row.get("name", "")):
            return False
        if "my_fac" in requirement and npc_row.get("my_fac") != requirement.get("my_fac"):
            return False
        if "attitude" in requirement and npc_row.get("attitude") != requirement.get("attitude"):
            return False
        if "mission" in requirement and npc_row.get("mission") != requirement.get("mission"):
            return False
        if "rules" in requirement:
            observed_rules = npc_row.get("rules")
            if not isinstance(observed_rules, dict):
                return False
            for rule_name, expected_value in requirement["rules"].items():
                if observed_rules.get(rule_name) != expected_value:
                    return False
        if "offset_ms" in requirement and npc_row.get("offset_ms") != requirement.get("offset_ms"):
            return False
        if "max_abs_offset_ms" in requirement:
            offset = npc_row.get("offset_ms")
            if not isinstance(offset, list) or len(offset) < 3:
                return False
            max_abs = requirement["max_abs_offset_ms"]
            if any(abs(int(offset[i])) > int(max_abs[i]) for i in range(3)):
                return False
        return True

    missing_required_npcs = [
        requirement
        for requirement in required
        if not any(matches_requirement(npc_row, requirement) for npc_row in npcs)
    ]

    baseline_observed_npc_count: Optional[int] = None
    observed_npc_count_delta: Optional[int] = None
    new_npcs: List[Dict[str, Any]] = []
    missing_required_new_npcs: List[Dict[str, Any]] = []
    missing_required_npc_count_delta = False
    if baseline_metadata is not None:
        baseline_observed_npc_count = int(baseline_metadata.get("observed_npc_count", 0) or 0)
        observed_npc_count_delta = len(npcs) - baseline_observed_npc_count
        baseline_ids = {
            str(npc.get("id"))
            for npc in baseline_metadata.get("observed_npcs", [])
            if isinstance(npc, dict) and npc.get("id") is not None
        }
        baseline_fallback = {
            json.dumps({
                "name": npc.get("name", ""),
                "location_ms": npc.get("location_ms", []),
                "my_fac": npc.get("my_fac"),
                "attitude": npc.get("attitude"),
            }, sort_keys=True)
            for npc in baseline_metadata.get("observed_npcs", [])
            if isinstance(npc, dict) and npc.get("id") is None
        }
        for npc in npcs:
            npc_id = npc.get("id")
            if npc_id is not None:
                if str(npc_id) not in baseline_ids:
                    new_npcs.append(npc)
                continue
            fallback = json.dumps({
                "name": npc.get("name", ""),
                "location_ms": npc.get("location_ms", []),
                "my_fac": npc.get("my_fac"),
                "attitude": npc.get("attitude"),
            }, sort_keys=True)
            if fallback not in baseline_fallback:
                new_npcs.append(npc)
        if required_observed_npc_count_delta is not None:
            missing_required_npc_count_delta = observed_npc_count_delta != int(required_observed_npc_count_delta)
        missing_required_new_npcs = [
            requirement
            for requirement in required_new
            if not any(matches_requirement(npc_row, requirement) for npc_row in new_npcs)
        ]

    if record_baseline:
        status = "baseline_recorded"
    elif (
        (not required or not missing_required_npcs)
        and (required_observed_npc_count_delta is None or not missing_required_npc_count_delta)
        and (not required_new or not missing_required_new_npcs)
        and (required or required_observed_npc_count_delta is not None or required_new)
    ):
        status = "required_state_present"
    elif required or required_observed_npc_count_delta is not None or required_new:
        status = "required_state_missing"
    else:
        status = "scanned"

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_location_ms": player_location,
        "player_abs_omt": list(player_abs_omt),
        "scan_all_overmaps": scan_all_overmaps,
        "scanned_overmaps": scanned_overmaps,
        "required_npcs": required,
        "observed_npc_count": len(npcs),
        "baseline_observed_npc_count": baseline_observed_npc_count,
        "observed_npc_count_delta": observed_npc_count_delta,
        "required_observed_npc_count_delta": required_observed_npc_count_delta,
        "missing_required_npc_count_delta": missing_required_npc_count_delta,
        "observed_npcs": npcs,
        "new_npcs": new_npcs,
        "required_new_npcs": required_new,
        "missing_required_new_npcs": missing_required_new_npcs,
        "missing_required_npcs": missing_required_npcs,
        "status": status,
    }


def audit_saved_weather_state(
    world_dir: Path,
    *,
    required_weather_id: str = "",
    required_temperature_f: Optional[float] = None,
    temperature_tolerance_f: float = 0.75,
) -> Dict[str, Any]:
    """Read-only saved-world audit for current weather/temperature metadata.

    Weather is saved in ``dimension_data.gsav`` rather than the player save.
    Keep this as an explicit same-save target-state gate so debug weather or
    temperature macros are not credited from keystroke completion alone.
    """
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise FileNotFoundError(f"World dimension data not found: {dimension_path}")

    dimension_text = dimension_path.read_text(encoding="utf-8")
    version_line, sep, payload_text = dimension_text.partition("\n")
    if not sep:
        raise RuntimeError(f"Dimension data missing version header newline: {dimension_path}")
    payload = json.loads(payload_text)
    if not isinstance(payload, dict):
        raise RuntimeError(f"Dimension data is not a JSON object: {dimension_path}")
    weather = payload.get("weather")
    if not isinstance(weather, dict):
        raise RuntimeError(f"Dimension data lacks weather object: {dimension_path}")

    observed_weather_id = str(weather.get("weather_id", "")).strip()
    observed_temperature_raw = weather.get("temperature")
    observed_forced_temperature_raw = weather.get("forced_temperature")
    observed_temperature_f: Optional[float]
    observed_forced_temperature_f: Optional[float]
    try:
        observed_temperature_f = float(observed_temperature_raw)
    except (TypeError, ValueError):
        observed_temperature_f = None
    try:
        observed_forced_temperature_f = float(observed_forced_temperature_raw)
    except (TypeError, ValueError):
        observed_forced_temperature_f = None
    observed_effective_temperature_f = (
        observed_forced_temperature_f
        if observed_forced_temperature_f is not None
        else observed_temperature_f
    )

    required_weather = str(required_weather_id or "").strip()
    required_temperature: Optional[float] = None
    if required_temperature_f is not None:
        required_temperature = float(required_temperature_f)
    tolerance = max(0.0, float(temperature_tolerance_f))

    missing_required_weather: List[str] = []
    if required_weather and observed_weather_id != required_weather:
        missing_required_weather.append(f"weather_id={required_weather}")
    if required_temperature is not None:
        if observed_effective_temperature_f is None:
            missing_required_weather.append(f"effective_temperature_f={required_temperature:g}")
        elif abs(observed_effective_temperature_f - required_temperature) > tolerance:
            missing_required_weather.append(f"effective_temperature_f={required_temperature:g}")

    required_count = int(bool(required_weather)) + int(required_temperature is not None)
    if required_count and not missing_required_weather:
        status = "required_state_present"
    elif required_count:
        status = "required_state_missing"
    else:
        status = "scanned"

    stat = dimension_path.stat()
    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "dimension_path": str(dimension_path),
        "dimension_mtime_ns": int(stat.st_mtime_ns),
        "dimension_mtime_iso": datetime.fromtimestamp(float(stat.st_mtime)).isoformat(timespec="microseconds"),
        "version_line": version_line,
        "required_weather_id": required_weather,
        "required_temperature_f": required_temperature,
        "temperature_tolerance_f": tolerance,
        "observed_weather_id": observed_weather_id,
        "observed_temperature_f": observed_temperature_f,
        "observed_forced_temperature_f": observed_forced_temperature_f,
        "observed_effective_temperature_f": observed_effective_temperature_f,
        "observed_weather": {
            "weather_id": observed_weather_id,
            "next_weather": weather.get("next_weather"),
            "temperature": observed_temperature_raw,
            "forced_temperature": observed_forced_temperature_raw,
            "winddirection": weather.get("winddirection"),
            "windspeed": weather.get("windspeed"),
            "lightning": weather.get("lightning"),
        },
        "missing_required_weather": missing_required_weather,
        "status": status,
    }


def summarize_bandit_live_world_site(site: Dict[str, Any]) -> Dict[str, Any]:
    intelligence_map = site.get("intelligence_map")
    leads = []
    if isinstance(intelligence_map, dict):
        raw_leads = intelligence_map.get("leads", [])
        if isinstance(raw_leads, list):
            for lead in raw_leads:
                if not isinstance(lead, dict):
                    continue
                leads.append({
                    "lead_id": lead.get("lead_id", ""),
                    "kind": lead.get("kind", ""),
                    "status": lead.get("status", ""),
                    "target_id": lead.get("target_id", ""),
                    "omt": lead.get("omt", []),
                    "radius_omt": lead.get("radius_omt", 0),
                    "source_key": lead.get("source_key", ""),
                    "source_summary": lead.get("source_summary", ""),
                    "first_seen_minutes": lead.get("first_seen_minutes", -1),
                    "last_seen_minutes": lead.get("last_seen_minutes", -1),
                    "last_checked_minutes": lead.get("last_checked_minutes", -1),
                    "last_scouted_minutes": lead.get("last_scouted_minutes", -1),
                    "bounty": lead.get("bounty", 0),
                    "threat": lead.get("threat", 0),
                    "confidence": lead.get("confidence", 0),
                    "threat_confirmed": lead.get("threat_confirmed", False),
                    "target_alert": lead.get("target_alert", False),
                    "scout_seen": lead.get("scout_seen", False),
                    "generated_by_this_camp_routine": lead.get("generated_by_this_camp_routine", False),
                    "prior_bandit_losses": lead.get("prior_bandit_losses", 0),
                    "prior_defender_losses": lead.get("prior_defender_losses", 0),
                    "times_checked_empty": lead.get("times_checked_empty", 0),
                    "times_harvested": lead.get("times_harvested", 0),
                    "last_outcome": lead.get("last_outcome", ""),
                })
    active_member_ids = site.get("active_member_ids", [])
    if not isinstance(active_member_ids, list):
        active_member_ids = []
    retired_empty_site = bool(site.get("retired_empty_site", False))
    spawn_tiles = site.get("spawn_tiles", [])
    if not isinstance(spawn_tiles, list):
        spawn_tiles = []
    spawn_tile_headcount = 0
    for spawn_tile in spawn_tiles:
        if not isinstance(spawn_tile, dict):
            continue
        spawn_tile_headcount += max(0, int(spawn_tile.get("headcount", 0) or 0))
    members = site.get("members", [])
    if not isinstance(members, list):
        members = []
    ready_at_home = 0
    wounded_or_unready = 0
    active_outside_ids = {str(value) for value in active_member_ids}
    member_state_counts: Dict[str, int] = {}
    for member in members:
        if not isinstance(member, dict):
            continue
        state = str(member.get("state", ""))
        member_state_counts[state] = member_state_counts.get(state, 0) + 1
        is_wounded_or_unready = bool(member.get("wounded_or_unready", False))
        if state == "at_home" and not is_wounded_or_unready:
            ready_at_home += 1
        if state in {"at_home", "outbound", "local_contact"} and is_wounded_or_unready:
            wounded_or_unready += 1
        if state in {"outbound", "local_contact"}:
            active_outside_ids.add(str(member.get("npc_id", "")))
    active_outside_ids.discard("")
    home_side_signals = ready_at_home + wounded_or_unready + max(0, int(site.get("headcount", 0) or 0)) + spawn_tile_headcount
    empty_retirement_blockers: List[str] = []
    if retired_empty_site:
        empty_retirement_blockers.append("already_retired")
    if str(site.get("site_kind", "")) in {"", "none"}:
        empty_retirement_blockers.append("no_hostile_site_kind")
    if home_side_signals != 0:
        empty_retirement_blockers.append("home_side_present")
    if active_outside_ids:
        empty_retirement_blockers.append("active_outside_present")
    known_recent_marks = site.get("known_recent_marks", [])
    if not isinstance(known_recent_marks, list):
        known_recent_marks = []
    return {
        "site_id": site.get("site_id", ""),
        "source_kind": site.get("source_kind", ""),
        "site_kind": site.get("site_kind", ""),
        "hostile_profile": site.get("hostile_profile", site.get("profile", "")),
        "anchor": site.get("anchor", []),
        "headcount": site.get("headcount", 0),
        "spawn_tile_headcount": spawn_tile_headcount,
        "home_side_signal_count": home_side_signals,
        "retired_empty_site": retired_empty_site,
        "retirement_summary": site.get("retirement_summary", ""),
        "empty_site_retirement_eligible": not retired_empty_site and not empty_retirement_blockers,
        "empty_site_retirement_blockers": empty_retirement_blockers,
        "member_count": len(members),
        "ready_at_home_count": ready_at_home,
        "wounded_or_unready_count": wounded_or_unready,
        "active_outside_count": len(active_outside_ids),
        "member_state_counts": member_state_counts,
        "active_group_id": site.get("active_group_id", ""),
        "active_target_id": site.get("active_target_id", ""),
        "active_target_omt": site.get("active_target_omt", []),
        "active_job_type": site.get("active_job_type", ""),
        "active_sortie_started_minutes": site.get("active_sortie_started_minutes", -1),
        "active_sortie_local_contact_minutes": site.get("active_sortie_local_contact_minutes", -1),
        "active_member_ids": active_member_ids,
        "active_member_count": len(active_member_ids),
        "remembered_target_or_mark": site.get("remembered_target_or_mark", ""),
        "remembered_pressure": site.get("remembered_pressure", ""),
        "known_recent_marks": known_recent_marks,
        "known_recent_mark_count": len(known_recent_marks),
        "lead_count": len(leads),
        "leads": leads,
    }


def audit_saved_bandit_live_world_state(
    world_dir: Path,
    *,
    required_profile: str = "",
    required_site_id_contains: str = "",
    required_active_group_id_exact: Optional[str] = None,
    required_active_group_id_contains: str = "",
    required_active_target_id_exact: Optional[str] = None,
    required_active_target_id_prefix: str = "",
    required_active_target_id_contains: str = "",
    required_active_job_type: str = "",
    required_active_sortie_started_minutes: Optional[int] = None,
    required_active_sortie_local_contact_minutes: Optional[int] = None,
    required_member_count: Optional[int] = None,
    required_ready_at_home_count: Optional[int] = None,
    required_wounded_or_unready_count: Optional[int] = None,
    required_active_outside_count: Optional[int] = None,
    required_min_active_member_ids: Optional[int] = None,
    required_active_members_found: bool = False,
    required_active_member_max_abs_offset_ms: Optional[List[int]] = None,
    player_save: str = "",
    required_remembered_target_or_mark_prefix: str = "",
    required_min_leads: Optional[int] = None,
    required_lead_source_contains: str = "",
    required_lead_kind: str = "",
    required_lead_target_id: str = "",
    required_lead_target_id_prefix: str = "",
    required_lead_bounty: Optional[int] = None,
    required_lead_threat: Optional[int] = None,
    required_lead_times_harvested: Optional[int] = None,
    required_lead_last_checked_minutes_min: Optional[int] = None,
    required_lead_status: str = "",
    required_lead_last_outcome: str = "",
    required_lead_confidence: Optional[int] = None,
    required_known_recent_mark_contains: str = "",
    required_home_side_signal_count: Optional[int] = None,
    required_retired_empty_site: Optional[bool] = None,
    required_retirement_summary_contains: str = "",
    required_empty_retirement_blocker_contains: str = "",
) -> Dict[str, Any]:
    """Read-only saved dimension-data audit for persisted bandit_live_world state."""
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise FileNotFoundError(f"World dimension data not found: {dimension_path}")

    dimension_text = dimension_path.read_text(encoding="utf-8")
    version_line, sep, payload_text = dimension_text.partition("\n")
    if not sep:
        raise RuntimeError(f"Dimension data missing version header newline: {dimension_path}")
    payload = json.loads(payload_text)
    if not isinstance(payload, dict):
        raise RuntimeError(f"Dimension data is not a JSON object: {dimension_path}")
    overmapbuffer = payload.get("overmapbuffer")
    if not isinstance(overmapbuffer, dict):
        raise RuntimeError(f"Dimension data lacks overmapbuffer object: {dimension_path}")
    bandit_world = overmapbuffer.get("bandit_live_world")
    if not isinstance(bandit_world, dict):
        raise RuntimeError(f"Dimension data lacks overmapbuffer.bandit_live_world object: {dimension_path}")
    raw_sites = bandit_world.get("sites", [])
    if not isinstance(raw_sites, list):
        raise RuntimeError(f"bandit_live_world.sites is not a list: {dimension_path}")

    observed_sites = [summarize_bandit_live_world_site(site) for site in raw_sites if isinstance(site, dict)]

    selected_player_save = str(player_save or "").strip()
    player_location_ms: List[int] = []
    player_abs_omt: List[int] = []
    active_member_scan_error = ""
    active_member_npcs_by_id: Dict[str, Dict[str, Any]] = {}
    should_scan_active_members = bool(required_active_members_found or required_active_member_max_abs_offset_ms is not None)
    if should_scan_active_members:
        try:
            selected_player_save, _player_save_path, player_payload, _player_stat = load_saved_player_payload(world_dir, selected_player_save)
            player = player_payload.get("player")
            if not isinstance(player, dict):
                raise RuntimeError("Extracted player save is missing player object")
            location_raw = player.get("location", [])
            if not isinstance(location_raw, list) or len(location_raw) < 3:
                raise RuntimeError("Extracted player save is missing player location")
            player_location_ms = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
            player_abs_omt = [player_location_ms[0] // 24, player_location_ms[1] // 24, player_location_ms[2]]
            overmaps_dir = world_dir / "overmaps"
            if not overmaps_dir.exists():
                raise RuntimeError(f"Overmaps dir not found: {overmaps_dir}")
            for overmap_path in sorted(overmaps_dir.glob("o.*.*.zzip")):
                plain_path: Optional[Path] = None
                created_plain = False
                try:
                    plain_path, _version_line, overmap_payload = extract_overmap_payload(overmap_path)
                    created_plain = bool(overmap_payload.get("_created_plain"))
                    raw_npcs = overmap_payload.get("npcs", [])
                    if not isinstance(raw_npcs, list):
                        continue
                    for index, npc_payload in enumerate(raw_npcs):
                        if not isinstance(npc_payload, dict):
                            continue
                        npc_id = npc_payload.get("id")
                        if npc_id is None:
                            continue
                        location_raw = npc_payload.get("location", [])
                        location_ms: List[int] = []
                        offset_ms: List[int] = []
                        if isinstance(location_raw, list) and len(location_raw) >= 3:
                            location_ms = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
                            offset_ms = [
                                location_ms[0] - player_location_ms[0],
                                location_ms[1] - player_location_ms[1],
                                location_ms[2] - player_location_ms[2],
                            ]
                        active_member_npcs_by_id[str(npc_id)] = {
                            "id": npc_id,
                            "overmap": str(overmap_path.relative_to(world_dir)),
                            "index": index,
                            "name": str(npc_payload.get("name", "") or ""),
                            "location_ms": location_ms,
                            "offset_ms": offset_ms,
                            "attitude": npc_payload.get("attitude"),
                            "mission": npc_payload.get("mission"),
                            "my_fac": npc_payload.get("my_fac"),
                            "dead": npc_payload.get("dead"),
                        }
                finally:
                    if plain_path is not None and created_plain:
                        cleanup_extracted_overmap(plain_path, keep=False)
        except Exception as exc:
            active_member_scan_error = str(exc)
            if required_active_members_found or required_active_member_max_abs_offset_ms is not None:
                raise

    normalized_max_abs_offset: Optional[List[int]] = None
    if required_active_member_max_abs_offset_ms is not None:
        if len(required_active_member_max_abs_offset_ms) < 3:
            raise RuntimeError("required_active_member_max_abs_offset_ms needs [x,y,z]")
        normalized_max_abs_offset = [
            int(required_active_member_max_abs_offset_ms[0]),
            int(required_active_member_max_abs_offset_ms[1]),
            int(required_active_member_max_abs_offset_ms[2]),
        ]

    for site in observed_sites:
        lookups: List[Dict[str, Any]] = []
        active_ids = site.get("active_member_ids", [])
        if not isinstance(active_ids, list):
            active_ids = []
        for member_id in active_ids:
            found = active_member_npcs_by_id.get(str(member_id))
            if found is None:
                lookups.append({"id": member_id, "found_in_saved_overmap": False})
                continue
            row = dict(found)
            row["found_in_saved_overmap"] = True
            if normalized_max_abs_offset is not None:
                offset = row.get("offset_ms", [])
                row["within_required_max_abs_offset_ms"] = (
                    isinstance(offset, list)
                    and len(offset) >= 3
                    and all(abs(int(offset[i])) <= normalized_max_abs_offset[i] for i in range(3))
                )
            lookups.append(row)
        site["active_member_saved_lookup"] = lookups
        site["active_members_all_found_in_saved_overmap"] = bool(active_ids) and all(
            bool(row.get("found_in_saved_overmap")) for row in lookups
        )
        if normalized_max_abs_offset is not None:
            site["active_members_within_required_max_abs_offset_ms"] = bool(active_ids) and all(
                bool(row.get("within_required_max_abs_offset_ms")) for row in lookups
            )
        site["position_exposure_footing"] = {
            "player_location_ms": player_location_ms,
            "player_abs_omt": player_abs_omt,
            "active_member_offsets_ms": [row.get("offset_ms", []) for row in lookups if row.get("found_in_saved_overmap")],
            "named_footing": "saved active members are cross-referenced against overmap NPC records near the loaded player position before visible-turn sight-avoid proof",
        }

    required_profile = str(required_profile or "").strip()
    required_site_id_contains = str(required_site_id_contains or "").strip()
    normalized_active_group_id_exact = None if required_active_group_id_exact is None else str(required_active_group_id_exact)
    required_active_group_id_contains = str(required_active_group_id_contains or "").strip()
    normalized_active_target_id_exact = None if required_active_target_id_exact is None else str(required_active_target_id_exact)
    required_active_target_id_prefix = str(required_active_target_id_prefix or "").strip()
    required_active_target_id_contains = str(required_active_target_id_contains or "").strip()
    required_active_job_type = str(required_active_job_type or "").strip()
    required_remembered_target_or_mark_prefix = str(required_remembered_target_or_mark_prefix or "").strip()
    required_lead_source_contains = str(required_lead_source_contains or "").strip()
    required_lead_kind = str(required_lead_kind or "").strip()
    required_lead_target_id = str(required_lead_target_id or "").strip()
    required_lead_target_id_prefix = str(required_lead_target_id_prefix or "").strip()
    required_lead_status = str(required_lead_status or "").strip()
    required_lead_last_outcome = str(required_lead_last_outcome or "").strip()
    required_known_recent_mark_contains = str(required_known_recent_mark_contains or "").strip()
    required_retirement_summary_contains = str(required_retirement_summary_contains or "").strip()
    required_empty_retirement_blocker_contains = str(required_empty_retirement_blocker_contains or "").strip()

    def site_matches(site: Dict[str, Any]) -> bool:
        if required_profile and site.get("hostile_profile") != required_profile:
            return False
        if required_site_id_contains and required_site_id_contains not in str(site.get("site_id", "")):
            return False
        if normalized_active_group_id_exact is not None and str(site.get("active_group_id", "")) != normalized_active_group_id_exact:
            return False
        if required_active_group_id_contains and required_active_group_id_contains not in str(site.get("active_group_id", "")):
            return False
        if normalized_active_target_id_exact is not None and str(site.get("active_target_id", "")) != normalized_active_target_id_exact:
            return False
        if required_active_target_id_prefix and not str(site.get("active_target_id", "")).startswith(required_active_target_id_prefix):
            return False
        if required_active_target_id_contains and required_active_target_id_contains not in str(site.get("active_target_id", "")):
            return False
        if required_active_job_type and site.get("active_job_type") != required_active_job_type:
            return False
        if required_active_sortie_started_minutes is not None and int(site.get("active_sortie_started_minutes", -1) or -1) != required_active_sortie_started_minutes:
            return False
        if required_active_sortie_local_contact_minutes is not None and int(site.get("active_sortie_local_contact_minutes", -1) or -1) != required_active_sortie_local_contact_minutes:
            return False
        if required_member_count is not None and int(site.get("member_count", 0) or 0) != required_member_count:
            return False
        if required_ready_at_home_count is not None and int(site.get("ready_at_home_count", 0) or 0) != required_ready_at_home_count:
            return False
        if required_wounded_or_unready_count is not None and int(site.get("wounded_or_unready_count", 0) or 0) != required_wounded_or_unready_count:
            return False
        if required_active_outside_count is not None and int(site.get("active_outside_count", 0) or 0) != required_active_outside_count:
            return False
        if required_home_side_signal_count is not None and int(site.get("home_side_signal_count", 0) or 0) != required_home_side_signal_count:
            return False
        if required_retired_empty_site is not None and bool(site.get("retired_empty_site", False)) != required_retired_empty_site:
            return False
        if required_retirement_summary_contains and required_retirement_summary_contains not in str(site.get("retirement_summary", "")):
            return False
        if required_empty_retirement_blocker_contains:
            blockers = site.get("empty_site_retirement_blockers", [])
            if not isinstance(blockers, list) or required_empty_retirement_blocker_contains not in [str(blocker) for blocker in blockers]:
                return False
        if required_min_active_member_ids is not None and int(site.get("active_member_count", 0) or 0) < required_min_active_member_ids:
            return False
        if required_active_members_found and not bool(site.get("active_members_all_found_in_saved_overmap")):
            return False
        if normalized_max_abs_offset is not None and not bool(site.get("active_members_within_required_max_abs_offset_ms")):
            return False
        if required_remembered_target_or_mark_prefix and not str(site.get("remembered_target_or_mark", "")).startswith(required_remembered_target_or_mark_prefix):
            return False
        if required_min_leads is not None and int(site.get("lead_count", 0) or 0) < required_min_leads:
            return False
        lead_requirements_present = any([
            required_lead_source_contains,
            required_lead_kind,
            required_lead_target_id,
            required_lead_target_id_prefix,
            required_lead_bounty is not None,
            required_lead_threat is not None,
            required_lead_times_harvested is not None,
            required_lead_last_checked_minutes_min is not None,
            required_lead_status,
            required_lead_last_outcome,
            required_lead_confidence is not None,
        ])
        if lead_requirements_present:
            leads = site.get("leads", [])

            def lead_matches(lead: Dict[str, Any]) -> bool:
                if required_lead_source_contains and not (
                    required_lead_source_contains in str(lead.get("source_key", ""))
                    or required_lead_source_contains in str(lead.get("source_summary", ""))
                    or required_lead_source_contains in str(lead.get("lead_id", ""))
                ):
                    return False
                if required_lead_kind and str(lead.get("kind", "")) != required_lead_kind:
                    return False
                if required_lead_target_id and str(lead.get("target_id", "")) != required_lead_target_id:
                    return False
                if required_lead_target_id_prefix and not str(lead.get("target_id", "")).startswith(required_lead_target_id_prefix):
                    return False
                if required_lead_bounty is not None and int(lead.get("bounty", 0) or 0) != required_lead_bounty:
                    return False
                if required_lead_threat is not None and int(lead.get("threat", 0) or 0) != required_lead_threat:
                    return False
                if required_lead_times_harvested is not None and int(lead.get("times_harvested", 0) or 0) != required_lead_times_harvested:
                    return False
                if required_lead_last_checked_minutes_min is not None and int(lead.get("last_checked_minutes", -1) or -1) < required_lead_last_checked_minutes_min:
                    return False
                if required_lead_status and str(lead.get("status", "")) != required_lead_status:
                    return False
                if required_lead_last_outcome and str(lead.get("last_outcome", "")) != required_lead_last_outcome:
                    return False
                if required_lead_confidence is not None and int(lead.get("confidence", 0) or 0) != required_lead_confidence:
                    return False
                return True

            if not isinstance(leads, list) or not any(
                lead_matches(lead) for lead in leads if isinstance(lead, dict)
            ):
                return False
        if required_known_recent_mark_contains:
            marks = site.get("known_recent_marks", [])
            if not isinstance(marks, list) or not any(
                required_known_recent_mark_contains in str(mark) for mark in marks
            ):
                return False
        return True

    required_fields = {
        "required_profile": required_profile,
        "required_site_id_contains": required_site_id_contains,
        "required_active_group_id_exact": normalized_active_group_id_exact,
        "required_active_group_id_contains": required_active_group_id_contains,
        "required_active_target_id_exact": normalized_active_target_id_exact,
        "required_active_target_id_prefix": required_active_target_id_prefix,
        "required_active_target_id_contains": required_active_target_id_contains,
        "required_active_job_type": required_active_job_type,
        "required_active_sortie_started_minutes": required_active_sortie_started_minutes,
        "required_active_sortie_local_contact_minutes": required_active_sortie_local_contact_minutes,
        "required_member_count": required_member_count,
        "required_ready_at_home_count": required_ready_at_home_count,
        "required_wounded_or_unready_count": required_wounded_or_unready_count,
        "required_active_outside_count": required_active_outside_count,
        "required_min_active_member_ids": required_min_active_member_ids,
        "required_active_members_found": required_active_members_found,
        "required_active_member_max_abs_offset_ms": normalized_max_abs_offset,
        "required_remembered_target_or_mark_prefix": required_remembered_target_or_mark_prefix,
        "required_min_leads": required_min_leads,
        "required_lead_source_contains": required_lead_source_contains,
        "required_lead_kind": required_lead_kind,
        "required_lead_target_id": required_lead_target_id,
        "required_lead_target_id_prefix": required_lead_target_id_prefix,
        "required_lead_bounty": required_lead_bounty,
        "required_lead_threat": required_lead_threat,
        "required_lead_times_harvested": required_lead_times_harvested,
        "required_lead_last_checked_minutes_min": required_lead_last_checked_minutes_min,
        "required_lead_status": required_lead_status,
        "required_lead_last_outcome": required_lead_last_outcome,
        "required_lead_confidence": required_lead_confidence,
        "required_known_recent_mark_contains": required_known_recent_mark_contains,
        "required_home_side_signal_count": required_home_side_signal_count,
        "required_retired_empty_site": required_retired_empty_site,
        "required_retirement_summary_contains": required_retirement_summary_contains,
        "required_empty_retirement_blocker_contains": required_empty_retirement_blocker_contains,
    }
    has_requirement = any(value not in (None, "", []) for value in required_fields.values())
    matching_sites = [site for site in observed_sites if site_matches(site)]
    missing_required_fields = [] if matching_sites or not has_requirement else [
        key for key, value in required_fields.items() if value not in (None, "", [])
    ]
    if has_requirement and matching_sites:
        status = "required_state_present"
    elif has_requirement:
        status = "required_state_missing"
    else:
        status = "scanned"

    stat = dimension_path.stat()
    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "dimension_path": str(dimension_path),
        "dimension_mtime_ns": int(stat.st_mtime_ns),
        "dimension_mtime_iso": datetime.fromtimestamp(float(stat.st_mtime)).isoformat(timespec="microseconds"),
        "version_line": version_line,
        "required_fields": required_fields,
        "player_save": selected_player_save,
        "player_location_ms": player_location_ms,
        "player_abs_omt": player_abs_omt,
        "active_member_scan_error": active_member_scan_error,
        "observed_site_count": len(observed_sites),
        "observed_matching_site_count": len(matching_sites),
        "matching_sites": matching_sites,
        "observed_sites": observed_sites,
        "missing_required_fields": missing_required_fields,
        "status": status,
    }



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
    initial_settle_seconds: float = 5.0,
    exit_menu: bool = True,
) -> None:
    if group_radius < 0:
        raise SystemExit("Monster group radius cannot be negative")
    if initial_settle_seconds > 0:
        time.sleep(initial_settle_seconds)
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


def debug_map_editor_place_furniture(
    pid: int,
    *,
    furniture_query: str,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    query = str(furniture_query or "").strip()
    if not query:
        raise SystemExit("Map-editor furniture placement needs furniture_query")
    run_debug_menu_shortcut_path(
        pid,
        ["m", "M"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    if target_keys:
        peekaboo_press_sequence(pid, target_keys, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["r"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    apply_uilist_filter(
        pid,
        query,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if exit_editor:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


def debug_map_editor_place_field(
    pid: int,
    *,
    field_query: str,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    query = str(field_query or "").strip()
    if not query:
        raise SystemExit("Map-editor field placement needs field_query")
    run_debug_menu_shortcut_path(
        pid,
        ["m", "M"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    if target_keys:
        peekaboo_press_sequence(pid, target_keys, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["e"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    apply_uilist_filter(
        pid,
        query,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    # Field selection opens an intensity menu; accept the default highlighted
    # intensity before applying the brush to the target tile.
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if exit_editor:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


def debug_map_editor_select_feature_and_apply(
    pid: int,
    *,
    selector_key: str,
    query: str,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    selected_query = str(query or "").strip()
    if not selected_query:
        raise SystemExit("Map-editor feature placement needs query")
    run_debug_menu_shortcut_path(
        pid,
        ["m", "M"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    if target_keys:
        peekaboo_press_sequence(pid, target_keys, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, [selector_key], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    apply_uilist_filter(
        pid,
        selected_query,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if exit_editor:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


def debug_map_editor_place_terrain(
    pid: int,
    *,
    terrain_query: str,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    debug_map_editor_select_feature_and_apply(
        pid,
        selector_key="t",
        query=terrain_query,
        target_keys=target_keys,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        menu_settle_seconds=menu_settle_seconds,
        prompt_settle_seconds=prompt_settle_seconds,
        exit_editor=exit_editor,
    )


def debug_map_editor_place_trap(
    pid: int,
    *,
    trap_query: str,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    debug_map_editor_select_feature_and_apply(
        pid,
        selector_key="w",
        query=trap_query,
        target_keys=target_keys,
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        menu_settle_seconds=menu_settle_seconds,
        prompt_settle_seconds=prompt_settle_seconds,
        exit_editor=exit_editor,
    )


def debug_map_editor_place_radiation(
    pid: int,
    *,
    radiation: int,
    target_keys: Optional[List[str]] = None,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.45,
    prompt_settle_seconds: float = 0.25,
    exit_editor: bool = True,
) -> None:
    run_debug_menu_shortcut_path(
        pid,
        ["m", "M"],
        delay_ms=delay_ms,
        menu_settle_seconds=menu_settle_seconds,
    )
    if target_keys:
        peekaboo_press_sequence(pid, target_keys, delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    peekaboo_press_sequence(pid, ["q"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    fill_numeric_prompt(
        pid,
        int(radiation),
        delay_ms=delay_ms,
        type_delay_ms=type_delay_ms,
        settle_seconds=prompt_settle_seconds,
    )
    peekaboo_press_sequence(pid, ["enter"], delay_ms=delay_ms)
    time.sleep(prompt_settle_seconds)
    if exit_editor:
        peekaboo_press_sequence(pid, ["esc"], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)


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


def apply_screen_text_expectation_abort_guard(
    report: Dict[str, Any],
    step: Dict[str, Any],
    text_expectation: Dict[str, Any],
) -> bool:
    if not bool(step.get("abort_on_screen_text_expectation_failure", False)):
        return False
    if str(text_expectation.get("status", "")).strip() != "missing":
        return False
    report["abort"] = {
        "status": "aborted_by_screen_text_expectation_guard",
        "verdict": str(step.get("abort_verdict", "red_step_expected_screen_text_missing")),
        "reason": str(step.get("abort_reason", "required screen text was missing")),
        "patterns": text_expectation.get("patterns", []),
        "missing": text_expectation.get("missing", []),
        "matches": text_expectation.get("matches", []),
        "source": str(text_expectation.get("source", "")),
    }
    return True


def screen_summary_path(screen_summary: Dict[str, Any]) -> str:
    return str(screen_summary.get("png_path", "") or screen_summary.get("path", ""))



def metadata_checkpoint_verdict(metadata: Dict[str, Any]) -> Tuple[str, List[str]]:
    if not metadata:
        return "yellow_step_no_immediate_screen_or_metadata", ["no_screen_or_metadata_artifact"]
    status = str(metadata.get("status", "")).strip()
    if status in {"required_state_present", "required_fields_present"}:
        return "green_step_metadata_required_state_present", []
    if status in {
        "required_state_missing",
        "required_fields_missing",
        "required_items_missing",
        "required_furniture_missing",
        "required_terrain_missing",
        "required_traps_missing",
        "required_radiation_missing",
    }:
        issues = []
        if metadata.get("missing_required_terrain"):
            issues.append("missing_required_terrain")
        if metadata.get("missing_required_fields"):
            issues.append("missing_required_fields")
        if metadata.get("missing_required_items"):
            issues.append("missing_required_items")
        if metadata.get("missing_required_weapon"):
            issues.append("missing_required_weapon")
        if metadata.get("missing_required_weapon_ammo"):
            issues.append("missing_required_weapon_ammo")
        if metadata.get("missing_required_furniture"):
            issues.append("missing_required_furniture")
        if metadata.get("missing_required_traps"):
            issues.append("missing_required_traps")
        if metadata.get("missing_required_radiation"):
            issues.append("missing_required_radiation")
        if metadata.get("missing_required_monsters"):
            issues.append("missing_required_monsters")
        if metadata.get("missing_required_hordes"):
            issues.append("missing_required_hordes")
        if metadata.get("missing_required_npcs"):
            issues.append("missing_required_npcs")
        if metadata.get("missing_required_new_npcs"):
            issues.append("missing_required_new_npcs")
        if metadata.get("missing_required_npc_count_delta"):
            issues.append("missing_required_npc_count_delta")
        if metadata.get("missing_required_weather"):
            issues.append("missing_required_weather")
        if metadata.get("missing_required_points_ms"):
            issues.append("missing_required_points_ms")
        if metadata.get("missing_required_min_delta_turns"):
            issues.append("missing_required_min_delta_turns")
        if metadata.get("missing_required_time_of_day"):
            issues.append("missing_required_time_of_day")
        missing_fields = metadata.get("missing_required_fields")
        if isinstance(missing_fields, list):
            for missing_field in missing_fields:
                if str(missing_field) not in issues:
                    issues.append(str(missing_field))
        return "red_step_metadata_required_state_missing", issues or [status]
    if status == "failed":
        return "blocked_step_metadata_probe_failed", ["metadata_probe_failed"]
    if status == "baseline_recorded":
        return "green_step_metadata_baseline_recorded", []
    if status == "scanned":
        return "yellow_step_metadata_scanned_without_required_state", ["metadata_no_required_state"]
    return "yellow_step_metadata_inconclusive", ["metadata_status_inconclusive"]


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


def direct_report_checkpoint_verdict(report: Dict[str, Any]) -> Tuple[str, List[str], str]:
    """Return the direct screen/metadata checkpoint verdict and artifact for one step report."""
    metadata_summary = report.get("metadata") if isinstance(report.get("metadata"), dict) else {}
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

    if metadata_summary:
        verdict, issues = metadata_checkpoint_verdict(metadata_summary)
        artifact = str(metadata_summary.get("artifact_path", "")) or "probe.report.json:steps[].metadata"
        return verdict, issues, artifact

    verdict, issues = screen_checkpoint_verdict(
        screen_summary=screen_summary,
        expected_visible_fact=str(report.get("expected_visible_fact", "")).strip(),
        text_expectation=text_expectation,
        ocr_requested=ocr_requested,
    )
    return verdict, issues, screen_summary_path(screen_summary)


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
    if kind == "audit_log_contains":
        return "scan harness debug log delta for required UI trace patterns"
    if kind == "audit_player_message_log_contains":
        return "scan saved player message log for narrow decisive in-game message lines"
    if kind.startswith("debug_"):
        return f"debug helper primitive {kind}"
    return kind or "scenario step"


def build_probe_step_ledger(step_reports: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    ledger: List[Dict[str, Any]] = []
    reports_by_label = {str(report.get("label", "")).strip(): report for report in step_reports}
    for report in step_reports:
        label = str(report.get("label", "")).strip()
        kind = str(report.get("kind", "")).strip().lower()
        expected_visible_fact = str(report.get("expected_visible_fact", "")).strip()
        screen_summary: Dict[str, Any] = {}
        metadata_summary: Dict[str, Any] = {}
        text_expectation: Optional[Dict[str, Any]] = None
        ocr_requested = False

        if isinstance(report.get("metadata"), dict):
            metadata_summary = report["metadata"]
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
        deferred_evidence_artifact = ""
        if isinstance(report.get("abort"), dict) and report["abort"].get("guard") == "metadata":
            verdict, issues = metadata_checkpoint_verdict(metadata_summary)
            if not verdict.startswith(("red", "blocked")):
                verdict = "red_step_aborted_by_metadata_guard"
            issues = issues + ["metadata_abort_guard"]
        elif isinstance(report.get("abort"), dict):
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
        elif metadata_summary:
            verdict, issues = metadata_checkpoint_verdict(metadata_summary)
        elif str(report.get("proof_deferred_to_label", "")).strip():
            deferred_label = str(report.get("proof_deferred_to_label", "")).strip()
            target_report = reports_by_label.get(deferred_label)
            if target_report:
                target_verdict, target_issues, deferred_evidence_artifact = direct_report_checkpoint_verdict(target_report)
                if target_verdict.startswith("green"):
                    verdict = "green_step_proof_deferred_to_guard"
                    issues = []
                elif target_verdict.startswith(("red", "blocked")):
                    verdict = "red_step_deferred_guard_failed"
                    issues = ["deferred_guard_failed"] + target_issues
                else:
                    verdict = "yellow_step_deferred_guard_incomplete"
                    issues = ["deferred_guard_incomplete"] + target_issues
            else:
                verdict = "yellow_step_deferred_guard_missing"
                issues = ["deferred_guard_missing"]
        else:
            verdict, issues = screen_checkpoint_verdict(
                screen_summary=screen_summary,
                expected_visible_fact=expected_visible_fact,
                text_expectation=text_expectation,
                ocr_requested=ocr_requested,
            )

        evidence_artifact = screen_summary_path(screen_summary)
        if not evidence_artifact and metadata_summary:
            evidence_artifact = str(metadata_summary.get("artifact_path", "")) or "probe.report.json:steps[].metadata"
        if not evidence_artifact and deferred_evidence_artifact:
            evidence_artifact = deferred_evidence_artifact
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
            "metadata_expectation": {
                "required_terrain": metadata_summary.get("required_terrain", []),
                "required_fields": metadata_summary.get("required_fields", []),
                "required_items": metadata_summary.get("required_items", []),
                "required_furniture": metadata_summary.get("required_furniture", []),
                "required_traps": metadata_summary.get("required_traps", []),
                "required_radiation": metadata_summary.get("required_radiation", []),
                "required_monsters": metadata_summary.get("required_monsters", []),
                "required_npcs": metadata_summary.get("required_npcs", []),
                "required_new_npcs": metadata_summary.get("required_new_npcs", []),
                "required_observed_npc_count_delta": metadata_summary.get("required_observed_npc_count_delta"),
                "observed_npc_count_delta": metadata_summary.get("observed_npc_count_delta"),
                "missing_required_terrain": metadata_summary.get("missing_required_terrain", []),
                "missing_required_fields": metadata_summary.get("missing_required_fields", []),
                "missing_required_items": metadata_summary.get("missing_required_items", []),
                "missing_required_furniture": metadata_summary.get("missing_required_furniture", []),
                "missing_required_traps": metadata_summary.get("missing_required_traps", []),
                "missing_required_radiation": metadata_summary.get("missing_required_radiation", []),
                "missing_required_monsters": metadata_summary.get("missing_required_monsters", []),
                "missing_required_npcs": metadata_summary.get("missing_required_npcs", []),
                "missing_required_new_npcs": metadata_summary.get("missing_required_new_npcs", []),
                "missing_required_npc_count_delta": metadata_summary.get("missing_required_npc_count_delta", False),
                "missing_required_weather": metadata_summary.get("missing_required_weather", []),
                "required_weapon": metadata_summary.get("required_weapon", ""),
                "observed_weapon": metadata_summary.get("observed_weapon", ""),
                "required_weapon_ammo_type": metadata_summary.get("required_weapon_ammo_type", ""),
                "required_weapon_ammo_min": metadata_summary.get("required_weapon_ammo_min", 0),
                "observed_weapon_ammo_remaining": metadata_summary.get("observed_weapon_ammo_remaining", 0),
                "missing_required_weapon": metadata_summary.get("missing_required_weapon", []),
                "missing_required_weapon_ammo": metadata_summary.get("missing_required_weapon_ammo", []),
                "required_weather_id": metadata_summary.get("required_weather_id", ""),
                "required_temperature_f": metadata_summary.get("required_temperature_f"),
                "required_bandit_fields": metadata_summary.get("required_fields", {}),
                "required_min_delta_turns": metadata_summary.get("required_min_delta_turns"),
                "observed_delta_turns": metadata_summary.get("observed_delta_turns"),
                "missing_required_min_delta_turns": metadata_summary.get("missing_required_min_delta_turns", False),
                "active_member_scan_error": metadata_summary.get("active_member_scan_error", ""),
            } if metadata_summary else {},
            "proof_deferred_to_label": str(report.get("proof_deferred_to_label", "")),
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
    env = os.environ.copy()
    env["OPENCLAW_HARNESS_UI_TRACE"] = "1"
    return subprocess.Popen(cmd, cwd=str(repo_root()), stdout=stdout_log, stderr=stderr_log, text=True, env=env)


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
            storage = str(raw.get("storage", "legacy_top_level_inv")).strip().lower()
            supported_storage = {"legacy_top_level_inv", "live_accessible_worn_pocket", "live_accessible_wielded"}
            if storage not in supported_storage:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] player_items storage must be one of "
                    f"{sorted(supported_storage)} in {manifest_path}"
                )
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
            replace_existing_weapon = bool(raw.get("replace_existing_weapon", False))
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "storage": storage,
                "items": items,
                "replace_existing_weapon": replace_existing_weapon,
            })
            continue

        if kind == "player_condition":
            normalized: Dict[str, Any] = {"kind": kind, "player_save": player_save}
            if "hp_percent" in raw:
                try:
                    hp_percent = int(raw.get("hp_percent"))
                except (TypeError, ValueError):
                    raise SystemExit(f"Fixture save_transforms[{index}] hp_percent must be integer in {manifest_path}")
                if hp_percent <= 0 or hp_percent > 100:
                    raise SystemExit(f"Fixture save_transforms[{index}] hp_percent must be 1..100 in {manifest_path}")
                normalized["hp_percent"] = hp_percent
            if "stamina" in raw:
                try:
                    stamina = int(raw.get("stamina"))
                except (TypeError, ValueError):
                    raise SystemExit(f"Fixture save_transforms[{index}] stamina must be integer in {manifest_path}")
                if stamina < 0:
                    raise SystemExit(f"Fixture save_transforms[{index}] stamina must be >= 0 in {manifest_path}")
                normalized["stamina"] = stamina
            effects = normalize_string_list(raw.get("effects", []))
            if effects:
                normalized["effects"] = effects
                normalized["effect_body_part"] = str(raw.get("effect_body_part", "torso") or "torso").strip()
                normalized["effect_duration"] = int(raw.get("effect_duration", 600) or 600)
                normalized["effect_intensity"] = int(raw.get("effect_intensity", 1) or 1)
            if not any(key in normalized for key in ("hp_percent", "stamina", "effects")):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] player_condition needs hp_percent, stamina, or effects in {manifest_path}"
                )
            transforms.append(normalized)
            continue

        if kind == "player_location_offset_ms":
            offset_raw = raw.get("offset_ms", [])
            if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs offset_ms=[x,y,z] in {manifest_path}"
                )
            try:
                offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] offset_ms values must be integers in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "offset_ms": offset_ms,
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
            source_special_id = str(raw.get("source_special_id", special_id)).strip() or special_id
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
                "source_special_id": source_special_id,
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

        if kind == "map_furniture_near_player":
            furniture_raw = raw.get("furniture", raw.get("furnitures", []))
            if not isinstance(furniture_raw, list) or not furniture_raw:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs non-empty furniture list in {manifest_path}"
                )
            furniture: List[Dict[str, Any]] = []
            for furn_index, furn_raw in enumerate(furniture_raw, start=1):
                if not isinstance(furn_raw, dict):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].furniture[{furn_index}] must be an object in {manifest_path}"
                    )
                furn_id = str(furn_raw.get("id", furn_raw.get("furn_id", ""))).strip()
                if not furn_id:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].furniture[{furn_index}] needs id/furn_id in {manifest_path}"
                    )
                offset_raw = furn_raw.get("offset_ms", [0, 0, 0])
                if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].furniture[{furn_index}] needs offset_ms=[x,y,z] in {manifest_path}"
                    )
                try:
                    offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].furniture[{furn_index}] has non-integer offset in {manifest_path}"
                    )
                furniture.append({"id": furn_id, "offset_ms": offset_ms})
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "furniture": furniture,
            })
            continue

        if kind == "map_items_near_player":
            items_raw = raw.get("items", [])
            if not isinstance(items_raw, list) or not items_raw:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] needs non-empty items list in {manifest_path}"
                )
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
                offset_raw = item_raw.get("offset_ms", [0, 0, 0])
                if not isinstance(offset_raw, list) or len(offset_raw) != 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] needs offset_ms=[x,y,z] in {manifest_path}"
                    )
                try:
                    offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
                    count = int(item_raw.get("count", 1) or 1)
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] has non-integer offset/count in {manifest_path}"
                    )
                if count <= 0:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}].items[{item_index}] count must be > 0 in {manifest_path}"
                    )
                item = {"typeid": typeid, "offset_ms": offset_ms, "count": count}
                if "charges" in item_raw:
                    item["charges"] = int(item_raw.get("charges"))
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

        if kind == "source_firewood_zone_near_player":
            def normalize_zone_offset(key: str, default: List[int]) -> List[int]:
                value = raw.get(key, default)
                if not isinstance(value, list) or len(value) != 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] {key} needs [x,y,z] in {manifest_path}"
                    )
                try:
                    return [int(value[0]), int(value[1]), int(value[2])]
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] {key} values must be integers in {manifest_path}"
                    )

            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "name": str(raw.get("name", "OpenClaw fuel source") or "OpenClaw fuel source").strip(),
                "zone_type": str(raw.get("zone_type", "SOURCE_FIREWOOD") or "SOURCE_FIREWOOD").strip(),
                "faction": str(raw.get("faction", "your_followers") or "your_followers").strip(),
                "start_offset_ms": normalize_zone_offset("start_offset_ms", [1, -1, 0]),
                "end_offset_ms": normalize_zone_offset("end_offset_ms", [3, 1, 0]),
                "write_temp": bool(raw.get("write_temp", True)),
            })
            continue

        if kind == "horde_entity_near_player":
            def normalize_horde_offset(key: str, default: List[int]) -> List[int]:
                value = raw.get(key, default)
                if not isinstance(value, list) or len(value) != 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] {key} needs [x,y,z] in {manifest_path}"
                    )
                try:
                    return [int(value[0]), int(value[1]), int(value[2])]
                except (TypeError, ValueError):
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] {key} values must be integers in {manifest_path}"
                    )

            try:
                tracking_intensity = int(raw.get("tracking_intensity", 0) or 0)
                last_processed = int(raw.get("last_processed", 0) or 0)
                moves = int(raw.get("moves", 0) or 0)
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] horde_entity_near_player needs integer "
                    f"tracking_intensity/last_processed/moves in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "monster_id": str(raw.get("monster_id", "mon_zombie") or "mon_zombie").strip(),
                "offset_ms": normalize_horde_offset("offset_ms", [0, -240, 0]),
                "destination_offset_ms": normalize_horde_offset("destination_offset_ms", [0, -240, 0]),
                "tracking_intensity": tracking_intensity,
                "last_processed": last_processed,
                "moves": moves,
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

        if kind == "bandit_camp_map_lead":
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "site_id": str(raw.get("site_id", "") or "").strip(),
                "lead_id": str(raw.get("lead_id", "") or "").strip(),
                "kind_value": str(raw.get("lead_kind", raw.get("kind_value", "basecamp_activity")) or "basecamp_activity").strip(),
                "status": str(raw.get("status", "scout_confirmed") or "scout_confirmed").strip(),
                "target_id": str(raw.get("target_id", "") or "").strip(),
                "target_omt": raw.get("target_omt"),
                "radius_omt": int(raw.get("radius_omt", 2) or 2),
                "source_key": str(raw.get("source_key", "fixture_scout_return") or "fixture_scout_return").strip(),
                "source_summary": str(raw.get("source_summary", "fixture-backed vanished-signal remembered scout lead") or "").strip(),
                "first_seen_minutes": int(raw.get("first_seen_minutes", 0) or 0),
                "last_seen_minutes": int(raw.get("last_seen_minutes", 0) or 0),
                "last_checked_minutes": int(raw.get("last_checked_minutes", 0) or 0),
                "last_scouted_minutes": int(raw.get("last_scouted_minutes", 0) or 0),
                "bounty": int(raw.get("bounty", 8) or 8),
                "threat": int(1 if raw.get("threat") is None or str(raw.get("threat")).strip() == "" else raw.get("threat")),
                "confidence": int(raw.get("confidence", 3) or 3),
                "threat_confirmed": bool(raw.get("threat_confirmed", True)),
                "target_alert": bool(raw.get("target_alert", False)),
                "scout_seen": bool(raw.get("scout_seen", False)),
                "generated_by_this_camp_routine": bool(raw.get("generated_by_this_camp_routine", True)),
                "prior_bandit_losses": int(raw.get("prior_bandit_losses", 0) or 0),
                "prior_defender_losses": int(raw.get("prior_defender_losses", 0) or 0),
                "times_checked_empty": int(raw.get("times_checked_empty", 0) or 0),
                "times_harvested": int(raw.get("times_harvested", 0) or 0),
                "last_outcome": str(raw.get("last_outcome", "still_valid") or "still_valid").strip(),
                "clear_active_pressure": bool(raw.get("clear_active_pressure", True)),
                "mark_cleared_active_members_unready": bool(raw.get("mark_cleared_active_members_unready", True)),
            })
            continue

        if kind == "bandit_clone_site":
            source_site_id = str(raw.get("source_site_id", raw.get("site_id", "")) or "").strip()
            new_site_id = str(raw.get("new_site_id", "") or "").strip()
            if not source_site_id or not new_site_id:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] bandit_clone_site needs source_site_id and new_site_id in {manifest_path}"
                )
            raw_new_anchor = raw.get("new_anchor", raw.get("anchor"))
            new_anchor: Optional[List[int]] = None
            if raw_new_anchor is not None:
                if not isinstance(raw_new_anchor, list) or len(raw_new_anchor) < 3:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] bandit_clone_site new_anchor needs [x,y,z] in {manifest_path}"
                    )
                new_anchor = [int(raw_new_anchor[0]), int(raw_new_anchor[1]), int(raw_new_anchor[2])]
            raw_new_footprint = raw.get("new_footprint", raw.get("footprint"))
            new_footprint: Optional[List[List[int]]] = None
            if raw_new_footprint is not None:
                if not isinstance(raw_new_footprint, list) or not raw_new_footprint:
                    raise SystemExit(
                        f"Fixture save_transforms[{index}] bandit_clone_site new_footprint needs non-empty [[x,y,z], ...] in {manifest_path}"
                    )
                new_footprint = []
                for point_index, point in enumerate(raw_new_footprint):
                    if not isinstance(point, list) or len(point) < 3:
                        raise SystemExit(
                            f"Fixture save_transforms[{index}] bandit_clone_site new_footprint[{point_index}] needs [x,y,z] in {manifest_path}"
                        )
                    new_footprint.append([int(point[0]), int(point[1]), int(point[2])])
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "source_site_id": source_site_id,
                "new_site_id": new_site_id,
                "new_source_id": str(raw.get("new_source_id", raw.get("source_id", "")) or "").strip(),
                "new_site_kind": str(raw.get("new_site_kind", raw.get("site_kind", "")) or "").strip(),
                "new_anchor": new_anchor,
                "new_footprint": new_footprint,
                "clear_intelligence_map": bool(raw.get("clear_intelligence_map", True)),
                "clear_remembered_pressure": bool(raw.get("clear_remembered_pressure", True)),
            })
            continue

        if kind == "bandit_site_roster_shape":
            try:
                living_member_count = int(raw.get("living_member_count"))
                wounded_or_unready_count = int(raw.get("wounded_or_unready_count", 0) or 0)
                active_outside_member_count = int(raw.get("active_outside_member_count", 0) or 0)
                member_start_index = int(raw.get("member_start_index", 0) or 0)
            except (TypeError, ValueError):
                raise SystemExit(
                    f"Fixture save_transforms[{index}] bandit_site_roster_shape needs integer "
                    f"living_member_count/wounded_or_unready_count/active_outside_member_count/member_start_index in {manifest_path}"
                )
            if living_member_count < 0 or wounded_or_unready_count < 0 or active_outside_member_count < 0 or member_start_index < 0:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] bandit_site_roster_shape counts/member_start_index must be >= 0 in {manifest_path}"
                )
            if wounded_or_unready_count + active_outside_member_count > living_member_count:
                raise SystemExit(
                    f"Fixture save_transforms[{index}] bandit_site_roster_shape wounded+active exceeds living count in {manifest_path}"
                )
            transforms.append({
                "kind": kind,
                "player_save": player_save,
                "site_id": str(raw.get("site_id", "") or "").strip(),
                "living_member_count": living_member_count,
                "member_start_index": member_start_index,
                "wounded_or_unready_count": wounded_or_unready_count,
                "active_outside_member_count": active_outside_member_count,
                "active_job_type": str(raw.get("active_job_type", "stalk") or "stalk").strip(),
                "active_target_id": str(raw.get("active_target_id", "") or "").strip(),
                "headcount_override": raw.get("headcount_override"),
                "clear_spawn_tile_headcount": bool(raw.get("clear_spawn_tile_headcount", False)),
            })
            continue

        raise SystemExit(
            f"Unsupported fixture save_transforms[{index}].kind '{kind}' in {manifest_path}; "
            "supported kinds: player_mutations, player_items, player_condition, player_location_offset_ms, player_near_overmap_special, "
            "seed_overmap_special_near_player, map_fields_near_player, map_furniture_near_player, "
            "map_items_near_player, source_firewood_zone_near_player, horde_entity_near_player, game_turn, "
            "bandit_active_sortie_clock, bandit_camp_map_lead, bandit_clone_site, bandit_site_roster_shape"
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

    storage = str(transform.get("storage", "legacy_top_level_inv")).strip().lower()
    replace_existing_weapon = bool(transform.get("replace_existing_weapon", False))
    replaced_weapon_typeid = ""
    target_container: Optional[List[Any]] = None
    target_path = "player.inv"
    if storage == "legacy_top_level_inv":
        target_container = inventory
    elif storage == "live_accessible_wielded":
        existing_weapon = player.get("weapon")
        if isinstance(existing_weapon, dict) and str(existing_weapon.get("typeid", "")).strip():
            if not replace_existing_weapon:
                raise SystemExit(
                    "Fixture player-items transform requested live_accessible_wielded, "
                    f"but player already has weapon {existing_weapon.get('typeid')!r} in {extracted_save}"
                )
            replaced_weapon_typeid = str(existing_weapon.get("typeid", ""))
        requested_count = sum(int(item.get("count", 1) or 1) for item in transform.get("items", []))
        if requested_count != 1:
            raise SystemExit(
                "Fixture player-items transform requested live_accessible_wielded, "
                f"but wielded storage accepts exactly one item (got {requested_count}) in {extracted_save}"
            )
        target_container = []
        target_path = "player.weapon"
    elif storage == "live_accessible_worn_pocket":
        for worn_index, worn_item in enumerate(saved_player_worn_items(player)):
            if not isinstance(worn_item, dict):
                continue
            contents_obj = worn_item.get("contents")
            if not isinstance(contents_obj, dict):
                continue
            pockets = contents_obj.get("contents", [])
            if not isinstance(pockets, list):
                continue
            for pocket_index, pocket in enumerate(pockets):
                if not isinstance(pocket, dict):
                    continue
                try:
                    pocket_type = int(pocket.get("pocket_type", -1))
                except (TypeError, ValueError):
                    continue
                pocket_contents = pocket.setdefault("contents", [])
                if pocket_type == 0 and isinstance(pocket_contents, list):
                    target_container = pocket_contents
                    target_path = (
                        f"player.worn.worn[{worn_index}]/contents/contents[{pocket_index}]/contents"
                    )
                    break
            if target_container is not None:
                break
        if target_container is None:
            raise SystemExit(
                "Fixture player-items transform requested live_accessible_worn_pocket, "
                f"but found no writable worn container pocket in {extracted_save}"
            )
    else:
        raise SystemExit(f"Unsupported player-items storage target: {storage}")

    bday = int(payload.get("turn", 0) or 0)
    added_items: List[Dict[str, Any]] = []
    inserted_paths: List[str] = []
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
            assert target_container is not None
            if storage == "live_accessible_wielded":
                player["weapon"] = entry
                target_container.append(entry)
                inserted_paths.append(target_path)
            else:
                inserted_index = len(target_container)
                target_container.append(entry)
                inserted_paths.append(f"{target_path}[{inserted_index}]")
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
        "storage": storage,
        "target_path": target_path,
        "added_items": added_items,
        "inserted_paths": inserted_paths,
        "replace_existing_weapon": replace_existing_weapon,
        "replaced_weapon_typeid": replaced_weapon_typeid,
    }


def apply_player_condition_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    """Apply a small harness-only player condition restage to a copied fixture save."""
    player_save_name = str(transform.get("player_save", "")).strip()
    player_save = world_dir / player_save_name
    if not player_save.exists():
        raise SystemExit(f"Fixture player-condition transform target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture player-condition transform expects .zzip save path: {player_save}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture player-condition transform did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")

    hp_percent_raw = transform.get("hp_percent")
    updated_body_parts: List[str] = []
    if hp_percent_raw is not None:
        hp_percent = max(1, min(100, int(hp_percent_raw)))
        body = player.get("body")
        if not isinstance(body, dict):
            raise SystemExit(f"Player body is not a dict in {extracted_save}")
        for part_id, part in body.items():
            if not isinstance(part, dict):
                continue
            hp_max = int(part.get("hp_max", 0) or 0)
            if hp_max <= 0:
                continue
            part["hp_cur"] = max(1, ( hp_max * hp_percent ) // 100)
            updated_body_parts.append(str(part_id))

    updated_stamina: Optional[int] = None
    if "stamina" in transform:
        updated_stamina = max(0, int(transform.get("stamina", 0) or 0))
        player["stamina"] = updated_stamina

    added_effects: List[str] = []
    effects = player.setdefault("effects", {})
    if not isinstance(effects, dict):
        raise SystemExit(f"Player effects is not a dict in {extracted_save}")
    turn = int(payload.get("turn", 0) or 0)
    for effect_id in normalize_string_list(transform.get("effects", [])):
        bp = str(transform.get("effect_body_part", "torso") or "torso").strip()
        duration = int(transform.get("effect_duration", 600) or 600)
        intensity = int(transform.get("effect_intensity", 1) or 1)
        effect_bucket = effects.setdefault(effect_id, {})
        if not isinstance(effect_bucket, dict):
            raise SystemExit(f"Player effect bucket is not a dict for {effect_id} in {extracted_save}")
        effect_bucket[bp] = {
            "eff_type": effect_id,
            "duration": duration,
            "bp": bp,
            "permanent": False,
            "intensity": intensity,
            "start_turn": turn,
            "source": {"character_id": None, "faction_id": None},
        }
        added_effects.append(f"{effect_id}:{bp}")

    extracted_save.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    run_zzip(extracted_save)
    if extracted_save.exists():
        extracted_save.unlink()

    return {
        "kind": "player_condition",
        "world": world_dir.name,
        "player_save": player_save_name,
        "hp_percent": hp_percent_raw,
        "updated_body_parts": updated_body_parts,
        "stamina": updated_stamina,
        "added_effects": added_effects,
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
    source_special_id = str(transform.get("source_special_id", special_id)).strip() or special_id
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

    placements = load_bandit_special_placements(repo_root(), world_dir, {source_special_id})
    matching = [placement for placement in placements if placement.special_id == source_special_id]
    if requested_site_index > len(matching):
        raise SystemExit(
            f"Fixture seed-overmap-special-near-player requested source {source_special_id} site_index={requested_site_index} "
            f"for target {special_id}, but only found {len(matching)} matching placements in {world_dir}"
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
        "source_special_id": source_special_id,
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



def apply_player_location_offset_ms_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    """Move the saved player by a small map-square offset and update load anchors.

    This is setup footing only. It is intended for narrow live probes that need
    the player on an already-auditable adjacent z-level/tile without pretending
    the move itself proves any downstream product behavior.
    """
    player_save = world_dir / str(transform.get("player_save", "")).strip()
    if not player_save.exists():
        raise SystemExit(f"Fixture player-location-offset target not found: {player_save}")
    if player_save.suffix != ".zzip":
        raise SystemExit(f"Fixture player-location-offset expects .zzip save path: {player_save}")
    raw_offset = transform.get("offset_ms", [])
    if not isinstance(raw_offset, list) or len(raw_offset) != 3:
        raise SystemExit(f"Fixture player-location-offset needs offset_ms=[x,y,z]: {transform}")
    try:
        offset_ms = [int(raw_offset[0]), int(raw_offset[1]), int(raw_offset[2])]
    except (TypeError, ValueError):
        raise SystemExit(f"Fixture player-location-offset offset_ms values must be integers: {transform}")

    extracted_save = player_save.with_suffix("")
    run_zzip(player_save)
    if not extracted_save.exists():
        raise SystemExit(f"Fixture player-location-offset did not extract save: {extracted_save}")

    payload = json.loads(extracted_save.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise SystemExit(f"Extracted player save is not a JSON object: {extracted_save}")
    player = payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit(f"Extracted player save is missing player object: {extracted_save}")

    old_location_raw = player.get("location", [])
    if not isinstance(old_location_raw, list) or len(old_location_raw) < 3:
        raise SystemExit(f"Extracted player save is missing usable player.location: {extracted_save}")
    old_location = [int(old_location_raw[0]), int(old_location_raw[1]), int(old_location_raw[2])]
    target_location = [old_location[i] + offset_ms[i] for i in range(3)]

    old_overmap_x = int(payload.get("om_x", 0))
    old_overmap_y = int(payload.get("om_y", 0))
    old_levx = int(payload.get("levx", 0))
    old_levy = int(payload.get("levy", 0))
    old_levz = int(payload.get("levz", 0))
    updated_load_anchor = player_load_anchor_from_location(
        old_location,
        target_location,
        old_overmap_x=old_overmap_x,
        old_overmap_y=old_overmap_y,
        old_levx=old_levx,
        old_levy=old_levy,
        old_levz=old_levz,
    )
    player["location"] = target_location
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
        "kind": "player_location_offset_ms",
        "world": world_dir.name,
        "player_save": str(transform.get("player_save", "")),
        "offset_ms": offset_ms,
        "previous_location": old_location,
        "target_location": target_location,
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


def saved_item_payload(typeid: str, *, bday: int = 0, charges: Optional[int] = None, contents: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "typeid": typeid,
        "bday": int(bday),
        "last_temp_check": 0,
        "template_traits": [],
    }
    if charges is not None:
        payload["charges"] = int(charges)
    if isinstance(contents, dict):
        payload["contents"] = contents
    return payload


def apply_map_items_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    _player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    maps_dir = world_dir / "maps"
    if not maps_dir.exists():
        raise SystemExit(f"Fixture map-item transform maps dir not found: {maps_dir}")

    placed_items: List[Dict[str, Any]] = []
    extracted_packs: List[str] = []
    modified_map_paths: Dict[str, Path] = {}
    for item in transform.get("items", []):
        typeid = str(item.get("typeid", "")).strip()
        offset_ms_raw = item.get("offset_ms", [0, 0, 0])
        offset_ms = [int(offset_ms_raw[0]), int(offset_ms_raw[1]), int(offset_ms_raw[2])]
        count = int(item.get("count", 1) or 1)
        if count <= 0:
            raise SystemExit(f"Fixture map-item transform count must be positive: {item}")
        charges = item.get("charges")
        contents = item.get("contents") if isinstance(item.get("contents"), dict) else None

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
                raise SystemExit(f"Fixture map-item transform target map pack not found: {pack_zzip}")
            run_zzip(pack_zzip)
            extracted_packs.append(pack_stem)
        if not pack_dir.exists() or not pack_dir.is_dir():
            raise SystemExit(f"Fixture map-item transform did not extract map pack: {pack_dir}")

        map_path = pack_dir / f"{target_abs_omt[0]}.{target_abs_omt[1]}.{target_abs_omt[2]}.map"
        if not map_path.exists():
            raise SystemExit(f"Fixture map-item transform target map file not found: {map_path}")
        map_payload = json.loads(map_path.read_text(encoding="utf-8"))
        if not isinstance(map_payload, list):
            raise SystemExit(f"Fixture map-item transform map payload is not a list: {map_path}")
        target_submap: Optional[Dict[str, Any]] = None
        target_coords = [target_abs_sm[0], target_abs_sm[1], target_abs_sm[2]]
        for submap in map_payload:
            if isinstance(submap, dict) and submap.get("coordinates") == target_coords:
                target_submap = submap
                break
        if target_submap is None:
            raise SystemExit(f"Fixture map-item transform submap {target_coords} not found in {map_path}")
        items = target_submap.setdefault("items", [])
        if not isinstance(items, list):
            raise SystemExit(f"Fixture map-item transform items is not a list in {map_path}")
        item_payloads = [
            saved_item_payload(
                typeid,
                bday=0,
                charges=int(charges) if charges is not None else None,
                contents=contents,
            )
            for _ in range(count)
        ]
        items.extend([local_ms[0], local_ms[1], item_payloads])
        map_path.write_text(json.dumps(map_payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        modified_map_paths[str(map_path)] = map_path

        placed_items.append({
            "typeid": typeid,
            "count": count,
            "charges": int(charges) if charges is not None else None,
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
        "kind": "map_items_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "player_location": player_location,
        "placed_items": placed_items,
        "extracted_map_packs": sorted(set(extracted_packs)),
        "recompressed_maps": recompressed_maps,
        "removed_extracted_map_dirs": removed_extracted_dirs,
        "plain_map_dirs_left_for_game_load": False,
    }


def player_zone_file_stem(player_save_name: str) -> str:
    if player_save_name.endswith(".sav.zzip"):
        return player_save_name[:-len(".sav.zzip")]
    if player_save_name.endswith(".sav"):
        return player_save_name[:-len(".sav")]
    return Path(player_save_name).stem


def zone_contains_abs_point(zone: Dict[str, Any], point: Sequence[int]) -> bool:
    start = zone.get("start", [])
    end = zone.get("end", [])
    if not isinstance(start, list) or not isinstance(end, list) or len(start) < 3 or len(end) < 3:
        return False
    try:
        mins = [min(int(start[i]), int(end[i])) for i in range(3)]
        maxs = [max(int(start[i]), int(end[i])) for i in range(3)]
        target = [int(point[i]) for i in range(3)]
    except (TypeError, ValueError, IndexError):
        return False
    return all(mins[i] <= target[i] <= maxs[i] for i in range(3))


def map_submap_for_ms_location(world_dir: Path, target_location: Sequence[int]) -> Tuple[Path, Path, str, Path, List[Any], Dict[str, Any], Tuple[int, int, int], Tuple[int, int, int], Tuple[int, int, int], bool]:
    maps_dir = world_dir / "maps"
    if not maps_dir.exists():
        raise SystemExit(f"Fixture map transform maps dir not found: {maps_dir}")
    target_abs_sm = (int(target_location[0]) // 12, int(target_location[1]) // 12, int(target_location[2]))
    target_abs_omt = (int(target_location[0]) // 24, int(target_location[1]) // 24, int(target_location[2]))
    local_ms = (
        int(target_location[0]) - target_abs_sm[0] * 12,
        int(target_location[1]) - target_abs_sm[1] * 12,
        int(target_location[2]),
    )
    pack_stem = f"{target_abs_omt[0] // 32}.{target_abs_omt[1] // 32}.{target_abs_omt[2]}"
    pack_dir = maps_dir / pack_stem
    pack_zzip = maps_dir / f"{pack_stem}.zzip"
    extracted = False
    if not pack_dir.exists():
        if not pack_zzip.exists():
            raise SystemExit(f"Fixture map transform target map pack not found: {pack_zzip}")
        run_zzip(pack_zzip)
        extracted = True
    if not pack_dir.exists() or not pack_dir.is_dir():
        raise SystemExit(f"Fixture map transform did not extract map pack: {pack_dir}")
    map_path = pack_dir / f"{target_abs_omt[0]}.{target_abs_omt[1]}.{target_abs_omt[2]}.map"
    if not map_path.exists():
        raise SystemExit(f"Fixture map transform target map file not found: {map_path}")
    map_payload = json.loads(map_path.read_text(encoding="utf-8"))
    if not isinstance(map_payload, list):
        raise SystemExit(f"Fixture map transform map payload is not a list: {map_path}")
    target_coords = [target_abs_sm[0], target_abs_sm[1], target_abs_sm[2]]
    target_submap = next(
        (submap for submap in map_payload if isinstance(submap, dict) and submap.get("coordinates") == target_coords),
        None,
    )
    if target_submap is None:
        raise SystemExit(f"Fixture map transform submap {target_coords} not found in {map_path}")
    return maps_dir, pack_dir, pack_stem, map_path, map_payload, target_submap, target_abs_omt, target_abs_sm, local_ms, extracted


def apply_map_furniture_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    _player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    placed_furniture: List[Dict[str, Any]] = []
    extracted_packs: set[str] = set()
    modified_map_paths: Dict[str, Tuple[Path, List[Any]]] = {}

    for furniture in transform.get("furniture", []):
        furn_id = str(furniture.get("id", "")).strip()
        offset_raw = furniture.get("offset_ms", [0, 0, 0])
        offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
        target_location = [
            int(player_location[0]) + offset_ms[0],
            int(player_location[1]) + offset_ms[1],
            int(player_location[2]) + offset_ms[2],
        ]
        maps_dir, pack_dir, pack_stem, map_path, map_payload, target_submap, target_abs_omt, target_abs_sm, local_ms, extracted = map_submap_for_ms_location(world_dir, target_location)
        if extracted:
            extracted_packs.add(pack_stem)
        triples = target_submap.setdefault("furniture", [])
        if not isinstance(triples, list):
            raise SystemExit(f"Fixture furniture transform furniture is not a list in {map_path}")
        kept: List[Any] = []
        for x, y, payload in iter_map_triples(triples):
            if x == local_ms[0] and y == local_ms[1]:
                continue
            kept.append([x, y, payload])
        kept.append([local_ms[0], local_ms[1], furn_id])
        target_submap["furniture"] = kept
        modified_map_paths[str(map_path)] = (map_path, map_payload)
        placed_furniture.append({
            "id": furn_id,
            "offset_ms": offset_ms,
            "target_location_ms": target_location,
            "target_abs_omt": list(target_abs_omt),
            "target_abs_sm": list(target_abs_sm),
            "local_ms": [local_ms[0], local_ms[1], local_ms[2]],
            "map_pack": pack_stem,
            "map_file": map_path.name,
        })

    recompressed_maps: List[str] = []
    for map_path, map_payload in sorted(modified_map_paths.values(), key=lambda pair: str(pair[0])):
        map_path.write_text(json.dumps(map_payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        run_zzip(map_path)
        recompressed_maps.append(str(map_path.relative_to(world_dir)))

    removed_extracted_dirs: List[str] = []
    maps_dir = world_dir / "maps"
    for pack_stem in sorted(extracted_packs):
        pack_dir = maps_dir / pack_stem
        if pack_dir.exists():
            shutil.rmtree(pack_dir)
            removed_extracted_dirs.append(str(pack_dir.relative_to(world_dir)))

    return {
        "kind": "map_furniture_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "player_location": player_location,
        "placed_furniture": placed_furniture,
        "extracted_map_packs": sorted(extracted_packs),
        "recompressed_maps": recompressed_maps,
        "removed_extracted_map_dirs": removed_extracted_dirs,
        "plain_map_dirs_left_for_game_load": False,
    }


def apply_source_firewood_zone_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    _player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    start_offset = [int(value) for value in transform.get("start_offset_ms", [1, -1, 0])]
    end_offset = [int(value) for value in transform.get("end_offset_ms", [3, 1, 0])]
    start = [int(player_location[i]) + start_offset[i] for i in range(3)]
    end = [int(player_location[i]) + end_offset[i] for i in range(3)]
    zone = {
        "name": str(transform.get("name", "OpenClaw fuel source") or "OpenClaw fuel source"),
        "type": str(transform.get("zone_type", "SOURCE_FIREWOOD") or "SOURCE_FIREWOOD"),
        "faction": str(transform.get("faction", "your_followers") or "your_followers"),
        "invert": False,
        "enabled": True,
        "temporarily_disabled": False,
        "is_vehicle": False,
        "is_personal": False,
        "cached_shift": [0, 0, 0],
        "start": [min(start[i], end[i]) for i in range(3)],
        "end": [max(start[i], end[i]) for i in range(3)],
        "is_displayed": False,
    }

    stem = player_zone_file_stem(player_save_name)
    zone_paths = [world_dir / f"{stem}.zones.json"]
    if bool(transform.get("write_temp", True)):
        zone_paths.append(world_dir / f"{stem}.zoneszmgr-temp.json")

    updated_paths: List[str] = []
    for zone_path in zone_paths:
        existing: List[Any] = []
        if zone_path.exists():
            loaded = json.loads(zone_path.read_text(encoding="utf-8"))
            if not isinstance(loaded, list):
                raise SystemExit(f"Fixture source-firewood zone transform expected zone list: {zone_path}")
            existing = loaded
        existing = [
            entry for entry in existing
            if not (
                isinstance(entry, dict)
                and entry.get("name") == zone["name"]
                and entry.get("type") == zone["type"]
            )
        ]
        existing.append(dict(zone))
        zone_path.write_text(json.dumps(existing, indent=2, ensure_ascii=False), encoding="utf-8")
        updated_paths.append(str(zone_path.relative_to(world_dir)))

    return {
        "kind": "source_firewood_zone_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "player_location": player_location,
        "zone": zone,
        "updated_zone_files": updated_paths,
    }


def audit_saved_zones_near_player(
    world_dir: Path,
    *,
    player_save: str = "",
    required_zone_type: str = "",
    required_name_contains: str = "",
    required_faction: str = "",
    required_offsets: Optional[List[Tuple[int, int, int]]] = None,
) -> Dict[str, Any]:
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save = player_save.strip()
    if not selected_player_save:
        saves = sorted(path.name for path in world_dir.glob("*.sav.zzip"))
        if len(saves) != 1:
            raise RuntimeError(f"Expected one *.sav.zzip in {world_dir}, found {saves}")
        selected_player_save = saves[0]

    _player_abs_omt, player_location = load_player_abs_omt(world_dir, selected_player_save)
    stem = player_zone_file_stem(selected_player_save)
    candidate_paths = [
        world_dir / f"{stem}.zones.json",
        world_dir / f"{stem}.zoneszmgr-temp.json",
        world_dir / "zones.json",
        world_dir / "zoneszmgr-temp.json",
    ]
    zone_files: List[Dict[str, Any]] = []
    matching_zones: List[Dict[str, Any]] = []
    required_points = []
    for offset in required_offsets or []:
        required_points.append([
            int(player_location[0]) + int(offset[0]),
            int(player_location[1]) + int(offset[1]),
            int(player_location[2]) + int(offset[2]),
        ])

    for zone_path in candidate_paths:
        if not zone_path.exists():
            continue
        loaded = json.loads(zone_path.read_text(encoding="utf-8"))
        if not isinstance(loaded, list):
            continue
        file_matches: List[Dict[str, Any]] = []
        for index, zone in enumerate(loaded):
            if not isinstance(zone, dict):
                continue
            if required_zone_type and str(zone.get("type", "")) != required_zone_type:
                continue
            if required_faction and str(zone.get("faction", "")) != required_faction:
                continue
            if required_name_contains and required_name_contains not in str(zone.get("name", "")):
                continue
            contained_points = [point for point in required_points if zone_contains_abs_point(zone, point)]
            zone_summary = {
                "file": str(zone_path.relative_to(world_dir)),
                "index": index,
                "name": zone.get("name", ""),
                "type": zone.get("type", ""),
                "faction": zone.get("faction", ""),
                "start": zone.get("start"),
                "end": zone.get("end"),
                "contained_required_points": contained_points,
            }
            if not required_points or contained_points:
                file_matches.append(zone_summary)
                matching_zones.append(zone_summary)
        zone_files.append({
            "file": str(zone_path.relative_to(world_dir)),
            "zone_count": len(loaded),
            "matching_count": len(file_matches),
        })

    missing_points = [
        point for point in required_points
        if not any(point in zone.get("contained_required_points", []) for zone in matching_zones)
    ]
    required_present = bool(matching_zones) and not missing_points
    status = "required_state_present" if required_present else "required_state_missing"
    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_location_ms": player_location,
        "required_zone_type": required_zone_type,
        "required_name_contains": required_name_contains,
        "required_faction": required_faction,
        "required_offsets": [list(offset) for offset in required_offsets or []],
        "required_points_ms": required_points,
        "missing_required_points_ms": missing_points,
        "zone_files": zone_files,
        "matching_zones": matching_zones,
        "status": status,
    }


def horde_overmap_path_for_abs_ms(world_dir: Path, abs_ms: List[int]) -> Path:
    if len(abs_ms) < 3:
        raise SystemExit(f"Horde abs_ms point needs [x,y,z]: {abs_ms!r}")
    abs_omt = (int(abs_ms[0]) // 24, int(abs_ms[1]) // 24, int(abs_ms[2]))
    overmap_x, overmap_y, _local = overmap_file_coords_from_abs_omt(abs_omt)
    return world_dir / "overmaps" / f"o.{overmap_x}.{overmap_y}.zzip"


def iter_horde_map_entries(raw_horde_map: Any) -> List[Dict[str, Any]]:
    if raw_horde_map in (None, ""):
        return []
    if not isinstance(raw_horde_map, list):
        raise SystemExit("Overmap horde_map is not a list")
    if len(raw_horde_map) % 6 != 0:
        raise SystemExit(f"Overmap horde_map length {len(raw_horde_map)} is not divisible by 6")

    entries: List[Dict[str, Any]] = []
    for base in range(0, len(raw_horde_map), 6):
        location_raw = raw_horde_map[base]
        monster_raw = raw_horde_map[base + 1]
        destination_raw = raw_horde_map[base + 2]
        if not isinstance(location_raw, list) or len(location_raw) < 3:
            continue
        if not isinstance(destination_raw, list) or len(destination_raw) < 3:
            destination: List[int] = []
        else:
            destination = [int(destination_raw[0]), int(destination_raw[1]), int(destination_raw[2])]
        if isinstance(monster_raw, str):
            monster_id = monster_raw
        elif isinstance(monster_raw, dict):
            monster_id = str(
                monster_raw.get(
                    "typeid",
                    monster_raw.get("type", monster_raw.get("id", "")),
                )
            ).strip()
        else:
            monster_id = ""
        location = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]
        entries.append({
            "index": base // 6,
            "base_index": base,
            "location_ms": location,
            "monster_id": monster_id,
            "monster_payload_kind": "type_id" if isinstance(monster_raw, str) else type(monster_raw).__name__,
            "destination_ms": destination,
            "tracking_intensity": int(raw_horde_map[base + 3] or 0),
            "last_processed": raw_horde_map[base + 4],
            "moves": int(raw_horde_map[base + 5] or 0),
        })
    return entries


def apply_horde_entity_near_player_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    player_save_name = str(transform.get("player_save", "")).strip()
    monster_id = str(transform.get("monster_id", "mon_zombie") or "mon_zombie").strip()
    if not monster_id:
        raise SystemExit(f"Fixture horde transform needs monster_id: {transform}")
    _player_abs_omt, player_location = load_player_abs_omt(world_dir, player_save_name)
    offset_ms = [int(value) for value in transform.get("offset_ms", [0, -240, 0])]
    destination_offset_ms = [int(value) for value in transform.get("destination_offset_ms", offset_ms)]
    location_ms = [int(player_location[i]) + offset_ms[i] for i in range(3)]
    destination_ms = [int(player_location[i]) + destination_offset_ms[i] for i in range(3)]
    overmap_path = horde_overmap_path_for_abs_ms(world_dir, location_ms)
    if not overmap_path.exists():
        raise SystemExit(f"Target overmap file not found for horde transform: {overmap_path}")

    plain_path, version_line, payload = extract_overmap_payload(overmap_path)
    try:
        raw_horde_map = payload.get("horde_map")
        if raw_horde_map is None:
            raw_horde_map = []
            payload["horde_map"] = raw_horde_map
        if not isinstance(raw_horde_map, list):
            raise SystemExit(f"Overmap horde_map is not a list in {plain_path}")
        existing_entries = iter_horde_map_entries(raw_horde_map)
        removed_entries = [entry for entry in existing_entries if entry.get("location_ms") == location_ms]
        if removed_entries:
            kept: List[Any] = []
            for base in range(0, len(raw_horde_map), 6):
                loc = raw_horde_map[base]
                if isinstance(loc, list) and len(loc) >= 3 and [int(loc[0]), int(loc[1]), int(loc[2])] == location_ms:
                    continue
                kept.extend(raw_horde_map[base:base + 6])
            raw_horde_map[:] = kept
        raw_horde_map.extend([
            location_ms,
            monster_id,
            destination_ms,
            int(transform.get("tracking_intensity", 0) or 0),
            int(transform.get("last_processed", 0) or 0),
            int(transform.get("moves", 0) or 0),
        ])
        write_overmap_payload(plain_path, version_line, payload)
    except Exception:
        cleanup_extracted_overmap(plain_path, keep=not bool(payload.get("_created_plain", False)))
        raise

    return {
        "kind": "horde_entity_near_player",
        "world": world_dir.name,
        "player_save": player_save_name,
        "player_location_ms": player_location,
        "monster_id": monster_id,
        "offset_ms": offset_ms,
        "location_ms": location_ms,
        "destination_offset_ms": destination_offset_ms,
        "destination_ms": destination_ms,
        "tracking_intensity": int(transform.get("tracking_intensity", 0) or 0),
        "last_processed": int(transform.get("last_processed", 0) or 0),
        "moves": int(transform.get("moves", 0) or 0),
        "overmap": str(overmap_path.relative_to(world_dir)),
        "removed_existing_at_location": len(removed_entries),
    }


def audit_saved_hordes_near_player(
    world_dir: Path,
    *,
    player_save: str = "",
    required_hordes: Optional[List[Dict[str, Any]]] = None,
    radius_ms: Optional[int] = None,
) -> Dict[str, Any]:
    if not world_dir.exists():
        raise FileNotFoundError(f"World dir not found: {world_dir}")
    selected_player_save, _player_save_path, payload, _stat = load_saved_player_payload(world_dir, player_save)
    player = payload.get("player")
    if not isinstance(player, dict):
        raise RuntimeError("Extracted player save is missing player object")
    location_raw = player.get("location", [])
    if not isinstance(location_raw, list) or len(location_raw) < 3:
        raise RuntimeError("Extracted player save is missing player location")
    player_location = [int(location_raw[0]), int(location_raw[1]), int(location_raw[2])]

    normalized_required: List[Dict[str, Any]] = []
    overmap_paths: Dict[str, Path] = {}
    player_overmap = horde_overmap_path_for_abs_ms(world_dir, player_location)
    if player_overmap.exists():
        overmap_paths[str(player_overmap)] = player_overmap
    for raw in required_hordes or []:
        if not isinstance(raw, dict):
            continue
        monster_id = str(raw.get("monster_id", raw.get("typeid", raw.get("monster", ""))) or "").strip()
        requirement: Dict[str, Any] = {}
        if monster_id:
            requirement["monster_id"] = monster_id
        offset_raw = raw.get("offset_ms")
        if isinstance(offset_raw, list) and len(offset_raw) >= 3:
            offset_ms = [int(offset_raw[0]), int(offset_raw[1]), int(offset_raw[2])]
            location_ms = [player_location[i] + offset_ms[i] for i in range(3)]
            requirement["offset_ms"] = offset_ms
            requirement["location_ms"] = location_ms
            overmap_path = horde_overmap_path_for_abs_ms(world_dir, location_ms)
            if overmap_path.exists():
                overmap_paths[str(overmap_path)] = overmap_path
        destination_offset_raw = raw.get("destination_offset_ms")
        if isinstance(destination_offset_raw, list) and len(destination_offset_raw) >= 3:
            requirement["destination_offset_ms"] = [
                int(destination_offset_raw[0]),
                int(destination_offset_raw[1]),
                int(destination_offset_raw[2]),
            ]
            requirement["destination_ms"] = [
                player_location[i] + requirement["destination_offset_ms"][i] for i in range(3)
            ]
        if "tracking_intensity" in raw:
            requirement["tracking_intensity"] = int(raw.get("tracking_intensity") or 0)
        if "min_tracking_intensity" in raw:
            requirement["min_tracking_intensity"] = int(raw.get("min_tracking_intensity") or 0)
        if "moves" in raw:
            requirement["moves"] = int(raw.get("moves") or 0)
        if "min_moves" in raw:
            requirement["min_moves"] = int(raw.get("min_moves") or 0)
        if "min_last_processed" in raw:
            requirement["min_last_processed"] = int(raw.get("min_last_processed") or 0)
        if requirement:
            normalized_required.append(requirement)

    observed: List[Dict[str, Any]] = []
    scanned_overmaps: List[Dict[str, Any]] = []
    for overmap_path in sorted(overmap_paths.values(), key=lambda p: str(p)):
        plain_path, _version_line, overmap_payload = extract_overmap_payload(overmap_path)
        try:
            entries = iter_horde_map_entries(overmap_payload.get("horde_map", []))
        finally:
            cleanup_extracted_overmap(plain_path, keep=not bool(overmap_payload.get("_created_plain", False)))
        nearby_count = 0
        for entry in entries:
            location_ms = entry.get("location_ms", [])
            if not isinstance(location_ms, list) or len(location_ms) < 3:
                continue
            offset_ms = [location_ms[i] - player_location[i] for i in range(3)]
            distance_xy = max(abs(offset_ms[0]), abs(offset_ms[1]))
            if radius_ms is not None and distance_xy > int(radius_ms):
                continue
            summary = dict(entry)
            summary["offset_ms"] = offset_ms
            summary["distance_chebyshev_ms"] = distance_xy
            summary["overmap"] = str(overmap_path.relative_to(world_dir))
            observed.append(summary)
            nearby_count += 1
        scanned_overmaps.append({
            "overmap": str(overmap_path.relative_to(world_dir)),
            "horde_count": len(entries),
            "observed_nearby_count": nearby_count,
        })

    def matches_requirement(entry: Dict[str, Any], requirement: Dict[str, Any]) -> bool:
        if "monster_id" in requirement and entry.get("monster_id") != requirement.get("monster_id"):
            return False
        if "location_ms" in requirement and entry.get("location_ms") != requirement.get("location_ms"):
            return False
        if "destination_ms" in requirement and entry.get("destination_ms") != requirement.get("destination_ms"):
            return False
        if "tracking_intensity" in requirement and entry.get("tracking_intensity") != requirement.get("tracking_intensity"):
            return False
        if "min_tracking_intensity" in requirement and int(entry.get("tracking_intensity") or 0) < int(requirement.get("min_tracking_intensity") or 0):
            return False
        if "moves" in requirement and entry.get("moves") != requirement.get("moves"):
            return False
        if "min_moves" in requirement and int(entry.get("moves") or 0) < int(requirement.get("min_moves") or 0):
            return False
        if "min_last_processed" in requirement and int(entry.get("last_processed") or 0) < int(requirement.get("min_last_processed") or 0):
            return False
        return True

    missing_required_hordes = [
        requirement for requirement in normalized_required
        if not any(matches_requirement(entry, requirement) for entry in observed)
    ]
    if normalized_required and not missing_required_hordes:
        status = "required_state_present"
    elif normalized_required:
        status = "required_state_missing"
    else:
        status = "scanned"
    counts: Dict[str, int] = {}
    for entry in observed:
        monster_id = str(entry.get("monster_id", ""))
        if monster_id:
            counts[monster_id] = counts.get(monster_id, 0) + 1

    return {
        "world": world_dir.name,
        "world_dir": str(world_dir),
        "player_save": selected_player_save,
        "player_location_ms": player_location,
        "radius_ms": radius_ms,
        "required_hordes": normalized_required,
        "scanned_overmaps": scanned_overmaps,
        "observed_horde_counts": dict(sorted(counts.items())),
        "observed_hordes": observed,
        "missing_required_hordes": missing_required_hordes,
        "missing_required_monsters": missing_required_hordes,
        "status": status,
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



def apply_bandit_camp_map_lead_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise SystemExit(f"Fixture bandit camp-map lead transform target not found: {dimension_path}")

    selected_player_save, _player_path, player_payload, _player_stat = load_saved_player_payload(
        world_dir, str(transform.get("player_save", "") or "").strip()
    )
    player = player_payload.get("player")
    if not isinstance(player, dict):
        raise SystemExit("Fixture bandit camp-map lead transform could not read player object")
    player_location = player.get("location", [])
    if not isinstance(player_location, list) or len(player_location) < 3:
        raise SystemExit("Fixture bandit camp-map lead transform could not read player location")
    player_omt = [int(player_location[0]) // 24, int(player_location[1]) // 24, int(player_location[2])]

    raw_target_omt = transform.get("target_omt")
    if isinstance(raw_target_omt, list) and len(raw_target_omt) >= 3:
        target_omt = [int(raw_target_omt[0]), int(raw_target_omt[1]), int(raw_target_omt[2])]
    else:
        target_omt = player_omt
    target_id = str(transform.get("target_id", "") or "").strip()
    if not target_id:
        target_id = f"player@{target_omt[0]},{target_omt[1]},{target_omt[2]}"

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

    requested_site_id = str(transform.get("site_id", "") or "").strip()
    selected_site: Optional[Dict[str, Any]] = None
    for site in sites:
        if not isinstance(site, dict):
            continue
        site_id = str(site.get("site_id", ""))
        if requested_site_id and site_id != requested_site_id:
            continue
        selected_site = site
        break
    if selected_site is None:
        for site in sites:
            if isinstance(site, dict) and str(site.get("site_kind", "")) == "bandit_camp":
                selected_site = site
                break
    if selected_site is None:
        raise SystemExit(
            "Fixture bandit camp-map lead transform found no owned bandit site"
            + (f" matching {requested_site_id}" if requested_site_id else "")
        )

    site_id = str(selected_site.get("site_id", ""))
    old_active_member_ids = list(selected_site.get("active_member_ids", [])) if isinstance(
        selected_site.get("active_member_ids", []), list
    ) else []
    if bool(transform.get("clear_active_pressure", True)):
        selected_site["active_group_id"] = ""
        selected_site["active_target_id"] = ""
        selected_site["active_target_omt"] = [0, 0, 0]
        selected_site["active_job_type"] = ""
        selected_site["active_member_ids"] = []
        selected_site["active_sortie_started_minutes"] = -1
        selected_site["active_sortie_local_contact_minutes"] = -1
        mark_unready = bool(transform.get("mark_cleared_active_members_unready", True))
        old_active_id_set = {str(value) for value in old_active_member_ids}
        for member in selected_site.get("members", []):
            if not isinstance(member, dict):
                continue
            if member.get("state") in {"outbound", "local_contact"}:
                member["state"] = "at_home"
            if mark_unready and str(member.get("npc_id", "")) in old_active_id_set:
                member["wounded_or_unready"] = True

    intelligence_map = selected_site.get("intelligence_map")
    if not isinstance(intelligence_map, dict):
        intelligence_map = {"schema_version": 1, "leads": []}
        selected_site["intelligence_map"] = intelligence_map
    leads = intelligence_map.get("leads")
    if not isinstance(leads, list):
        leads = []
        intelligence_map["leads"] = leads

    lead_kind = str(transform.get("kind_value", "basecamp_activity") or "basecamp_activity").strip()
    lead_id = str(transform.get("lead_id", "") or "").strip()
    if not lead_id:
        lead_id = f"{site_id}#lead:{lead_kind}:{target_id}@{target_omt[0]},{target_omt[1]},{target_omt[2]}"
    lead = {
        "lead_id": lead_id,
        "kind": lead_kind,
        "status": str(transform.get("status", "scout_confirmed") or "scout_confirmed").strip(),
        "target_id": target_id,
        "omt": target_omt,
        "radius_omt": int(transform.get("radius_omt", 2) or 2),
        "source_key": str(transform.get("source_key", "fixture_scout_return") or "fixture_scout_return").strip(),
        "source_summary": str(transform.get("source_summary", "") or "").strip(),
        "first_seen_minutes": int(transform.get("first_seen_minutes", 0) or 0),
        "last_seen_minutes": int(transform.get("last_seen_minutes", 0) or 0),
        "last_checked_minutes": int(transform.get("last_checked_minutes", 0) or 0),
        "last_scouted_minutes": int(transform.get("last_scouted_minutes", 0) or 0),
        "bounty": int(transform.get("bounty", 8) or 8),
        "threat": int(1 if transform.get("threat") is None or str(transform.get("threat")).strip() == "" else transform.get("threat")),
        "confidence": int(transform.get("confidence", 3) or 3),
        "threat_confirmed": bool(transform.get("threat_confirmed", True)),
        "target_alert": bool(transform.get("target_alert", False)),
        "scout_seen": bool(transform.get("scout_seen", False)),
        "generated_by_this_camp_routine": bool(transform.get("generated_by_this_camp_routine", True)),
        "prior_bandit_losses": int(transform.get("prior_bandit_losses", 0) or 0),
        "prior_defender_losses": int(transform.get("prior_defender_losses", 0) or 0),
        "times_checked_empty": int(transform.get("times_checked_empty", 0) or 0),
        "times_harvested": int(transform.get("times_harvested", 0) or 0),
        "last_outcome": str(transform.get("last_outcome", "still_valid") or "still_valid").strip(),
    }
    leads[:] = [existing for existing in leads if not (
        isinstance(existing, dict) and str(existing.get("lead_id", "")) == lead_id
    )]
    leads.append(lead)
    selected_site["remembered_target_or_mark"] = target_id
    selected_site["remembered_threat_estimate"] = int(1 if transform.get("threat") is None or str(transform.get("threat")).strip() == "" else transform.get("threat"))
    selected_site["remembered_bounty_estimate"] = int(transform.get("bounty", 8) or 8)
    selected_site.setdefault("known_recent_marks", [])

    dimension_path.write_text(
        version_line + "\n" + json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    return {
        "kind": "bandit_camp_map_lead",
        "world": world_dir.name,
        "player_save": selected_player_save,
        "site_id": site_id,
        "old_active_member_ids": old_active_member_ids,
        "target_id": target_id,
        "target_omt": target_omt,
        "lead_id": lead_id,
        "lead": lead,
        "clear_active_pressure": bool(transform.get("clear_active_pressure", True)),
    }


def apply_bandit_clone_site_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise SystemExit(f"Fixture bandit clone-site transform target not found: {dimension_path}")

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

    source_site_id = str(transform.get("source_site_id", "") or "").strip()
    new_site_id = str(transform.get("new_site_id", "") or "").strip()
    source_site: Optional[Dict[str, Any]] = None
    for site in sites:
        if isinstance(site, dict) and str(site.get("site_id", "")) == source_site_id:
            source_site = site
            break
    if source_site is None:
        raise SystemExit(f"Fixture bandit clone-site transform found no source site matching {source_site_id}")
    if any(isinstance(site, dict) and str(site.get("site_id", "")) == new_site_id for site in sites):
        raise SystemExit(f"Fixture bandit clone-site transform target site already exists: {new_site_id}")

    cloned_site = json.loads(json.dumps(source_site))
    cloned_site["site_id"] = new_site_id
    new_source_id = str(transform.get("new_source_id", "") or "").strip()
    if new_source_id:
        cloned_site["source_id"] = new_source_id
    new_site_kind = str(transform.get("new_site_kind", "") or "").strip()
    if new_site_kind:
        cloned_site["site_kind"] = new_site_kind
    new_anchor = transform.get("new_anchor")
    if isinstance(new_anchor, list) and len(new_anchor) >= 3:
        cloned_site["anchor"] = [int(new_anchor[0]), int(new_anchor[1]), int(new_anchor[2])]
    new_footprint = transform.get("new_footprint")
    if isinstance(new_footprint, list) and new_footprint:
        cloned_site["footprint"] = [
            [int(point[0]), int(point[1]), int(point[2])]
            for point in new_footprint
            if isinstance(point, list) and len(point) >= 3
        ]
    cloned_site["active_group_id"] = ""
    cloned_site["active_member_ids"] = []
    cloned_site["active_target_id"] = ""
    cloned_site["active_target_omt"] = [0, 0, 0]
    cloned_site["active_job_type"] = ""
    cloned_site["active_sortie_started_minutes"] = -1
    cloned_site["active_sortie_local_contact_minutes"] = -1
    cloned_site["retired_empty_site"] = False
    cloned_site["retirement_summary"] = ""
    if bool(transform.get("clear_intelligence_map", True)):
        cloned_site["intelligence_map"] = {"leads": []}
    if bool(transform.get("clear_remembered_pressure", True)):
        cloned_site["remembered_target_or_mark"] = ""
        cloned_site["remembered_pressure"] = ""
        cloned_site["known_recent_marks"] = []

    sites.append(cloned_site)
    dimension_path.write_text(
        version_line + "\n" + json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    return {
        "kind": "bandit_clone_site",
        "world": world_dir.name,
        "source_site_id": source_site_id,
        "new_site_id": new_site_id,
        "new_source_id": str(cloned_site.get("source_id", "")),
        "new_site_kind": str(cloned_site.get("site_kind", "")),
        "new_anchor": cloned_site.get("anchor", []),
        "new_footprint": cloned_site.get("footprint", []),
        "clear_intelligence_map": bool(transform.get("clear_intelligence_map", True)),
        "clear_remembered_pressure": bool(transform.get("clear_remembered_pressure", True)),
    }


def apply_bandit_site_roster_shape_transform(world_dir: Path, transform: Dict[str, Any]) -> Dict[str, Any]:
    dimension_path = world_dir / "dimension_data.gsav"
    if not dimension_path.exists():
        raise SystemExit(f"Fixture bandit roster-shape transform target not found: {dimension_path}")

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

    requested_site_id = str(transform.get("site_id", "") or "").strip()
    selected_site: Optional[Dict[str, Any]] = None
    for site in sites:
        if not isinstance(site, dict):
            continue
        site_id = str(site.get("site_id", ""))
        if requested_site_id and site_id != requested_site_id:
            continue
        selected_site = site
        break
    if selected_site is None:
        raise SystemExit(
            "Fixture bandit roster-shape transform found no owned site"
            + (f" matching {requested_site_id}" if requested_site_id else "")
        )

    members = selected_site.get("members", [])
    if not isinstance(members, list):
        raise SystemExit("Fixture bandit roster-shape transform target site has no members list")
    living_member_count = int(transform.get("living_member_count", 0) or 0)
    member_start_index = int(transform.get("member_start_index", 0) or 0)
    wounded_or_unready_count = int(transform.get("wounded_or_unready_count", 0) or 0)
    active_outside_member_count = int(transform.get("active_outside_member_count", 0) or 0)
    member_end_index = member_start_index + living_member_count
    if member_end_index > len(members):
        raise SystemExit(
            f"Fixture bandit roster-shape requested members {member_start_index}:{member_end_index} but only {len(members)} are available"
        )

    shaped_members = [dict(member) for member in members[member_start_index:member_end_index] if isinstance(member, dict)]
    for member in shaped_members:
        member["state"] = "at_home"
        member["wounded_or_unready"] = False
        member["last_writeback_summary"] = "fixture roster shape: at-home ready member"

    active_member_ids: List[Any] = []
    for member in shaped_members[:active_outside_member_count]:
        member["state"] = "outbound"
        member["last_writeback_summary"] = "fixture roster shape: unresolved outside pressure"
        active_member_ids.append(member.get("npc_id"))

    wounded_start = active_outside_member_count
    wounded_end = wounded_start + wounded_or_unready_count
    for member in shaped_members[wounded_start:wounded_end]:
        member["wounded_or_unready"] = True
        member["last_writeback_summary"] = "fixture roster shape: wounded/unready member"

    selected_site["members"] = shaped_members
    headcount_override = transform.get("headcount_override")
    if headcount_override is None or str(headcount_override).strip() == "":
        selected_site["headcount"] = living_member_count
    else:
        selected_site["headcount"] = max(0, int(headcount_override))
    if bool(transform.get("clear_spawn_tile_headcount", False)):
        spawn_tiles = selected_site.get("spawn_tiles", [])
        if isinstance(spawn_tiles, list):
            for spawn_tile in spawn_tiles:
                if isinstance(spawn_tile, dict):
                    spawn_tile["headcount"] = 0
    selected_site["active_member_ids"] = active_member_ids
    if active_member_ids:
        site_id = str(selected_site.get("site_id", ""))
        selected_site["active_group_id"] = site_id + "#fixture_active_pressure"
        selected_site["active_job_type"] = str(transform.get("active_job_type", "stalk") or "stalk").strip()
        selected_site["active_target_id"] = str(transform.get("active_target_id", "") or "").strip()
        selected_site["active_sortie_started_minutes"] = 0
        selected_site["active_sortie_local_contact_minutes"] = 0
    else:
        selected_site["active_group_id"] = ""
        selected_site["active_target_id"] = ""
        selected_site["active_target_omt"] = [0, 0, 0]
        selected_site["active_job_type"] = ""
        selected_site["active_sortie_started_minutes"] = -1
        selected_site["active_sortie_local_contact_minutes"] = -1

    dimension_path.write_text(
        version_line + "\n" + json.dumps(payload, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    return {
        "kind": "bandit_site_roster_shape",
        "world": world_dir.name,
        "site_id": str(selected_site.get("site_id", "")),
        "living_member_count": living_member_count,
        "member_start_index": member_start_index,
        "headcount": int(selected_site.get("headcount", 0) or 0),
        "clear_spawn_tile_headcount": bool(transform.get("clear_spawn_tile_headcount", False)),
        "ready_at_home_count": max(0, living_member_count - active_outside_member_count - wounded_or_unready_count),
        "wounded_or_unready_count": wounded_or_unready_count,
        "active_outside_member_count": active_outside_member_count,
        "active_member_ids": active_member_ids,
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
        if kind == "player_condition":
            reports.append(apply_player_condition_transform(world_dir, transform))
            continue
        if kind == "player_location_offset_ms":
            reports.append(apply_player_location_offset_ms_transform(world_dir, transform))
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
        if kind == "map_furniture_near_player":
            reports.append(apply_map_furniture_near_player_transform(world_dir, transform))
            continue
        if kind == "map_items_near_player":
            reports.append(apply_map_items_near_player_transform(world_dir, transform))
            continue
        if kind == "source_firewood_zone_near_player":
            reports.append(apply_source_firewood_zone_near_player_transform(world_dir, transform))
            continue
        if kind == "horde_entity_near_player":
            reports.append(apply_horde_entity_near_player_transform(world_dir, transform))
            continue
        if kind == "game_turn":
            reports.append(apply_game_turn_transform(world_dir, transform))
            continue
        if kind == "bandit_camp_map_lead":
            reports.append(apply_bandit_camp_map_lead_transform(world_dir, transform))
            continue
        if kind == "bandit_clone_site":
            reports.append(apply_bandit_clone_site_transform(world_dir, transform))
            continue
        if kind == "bandit_site_roster_shape":
            reports.append(apply_bandit_site_roster_shape_transform(world_dir, transform))
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
    profile: str,
    world: str,
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
        artifact_log_start_size = artifact_log.stat().st_size if artifact_log is not None and artifact_log.exists() else artifact_baseline
        report: Dict[str, Any] = {
            "index": index,
            "kind": kind,
            "label": label,
            "settle_seconds": settle_seconds,
            "artifact_log_start_size": artifact_log_start_size,
        }
        expected_visible_fact = str(step.get("expected_visible_fact", "")).strip()
        if expected_visible_fact:
            report["expected_visible_fact"] = expected_visible_fact
        for optional_key in (
            "preconditions",
            "expected_immediate_state",
            "failure_rule",
            "next_step_gate",
            "proof_deferred_to_label",
        ):
            optional_value = str(step.get(optional_key, "") or "").strip()
            if optional_value:
                report[optional_key] = optional_value
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
                report["artifact_log_end_size"] = artifact_log.stat().st_size if artifact_log is not None and artifact_log.exists() else artifact_log_start_size
                reports.append(report)
                return reports
        elif kind == "wait":
            seconds = float(step.get("seconds", 0.0) or 0.0)
            if seconds <= 0:
                raise SystemExit(f"Scenario step '{label}' needs seconds > 0")
            time.sleep(seconds)
            report["seconds"] = seconds
        elif kind == "audit_log_contains":
            patterns = normalize_screen_text_patterns(
                step.get("required_patterns", step.get("patterns", step.get("required_items", [])))
            )
            raw_line_groups = step.get("required_line_patterns", []) or []
            line_groups = [normalize_screen_text_patterns(group) for group in raw_line_groups]
            raw_any_line_groups = step.get("required_any_line_patterns", []) or []
            any_line_groups = [normalize_screen_text_patterns(group) for group in raw_any_line_groups]
            if not patterns and not line_groups and not any_line_groups:
                raise SystemExit(f"Scenario step '{label}' needs required_patterns/patterns/required_line_patterns/required_any_line_patterns")
            audit_baseline = artifact_baseline
            since_label = str(step.get("since_label", step.get("since_step", "")) or "").strip()
            if since_label:
                for previous_report in reversed(reports):
                    if str(previous_report.get("label", "")) == since_label:
                        try:
                            audit_baseline = int(previous_report.get("artifact_log_start_size", artifact_baseline))
                        except (TypeError, ValueError):
                            audit_baseline = artifact_baseline
                        break
                else:
                    raise SystemExit(f"Scenario step '{label}' since_label not found: {since_label}")
            metadata = audit_log_contains(
                run_dir,
                label,
                artifact_log=artifact_log,
                artifact_baseline=audit_baseline,
                patterns=patterns,
                required_line_patterns=line_groups,
                required_any_line_patterns=any_line_groups,
                filter_debug_noise=filter_debug_noise,
            )
            if since_label:
                metadata["since_label"] = since_label
                metadata["since_label_start_size"] = audit_baseline
            metadata["artifact_path"] = str(run_dir / f"{label}.metadata.json")
            write_json(run_dir / f"{label}.metadata.json", metadata)
            report["metadata"] = metadata
            if metadata.get("status") == "required_state_missing" and bool(step.get("abort_on_metadata_failure", False)):
                report["abort"] = {
                    "guard": "metadata",
                    "status": "aborted_by_metadata_guard",
                    "verdict": str(step.get("abort_verdict", "red_step_required_log_pattern_missing")),
                    "reason": str(step.get("abort_reason", "required log pattern was missing")),
                    "missing_required_items": metadata.get("missing_required_items", []),
                }
                reports.append(report)
                return reports
        elif kind == "audit_player_message_log_contains":
            patterns = normalize_screen_text_patterns(
                step.get("required_patterns", step.get("patterns", step.get("required_items", [])))
            )
            raw_line_groups = step.get("required_line_patterns", []) or []
            line_groups = [normalize_screen_text_patterns(group) for group in raw_line_groups]
            raw_any_line_groups = step.get("required_any_line_patterns", []) or []
            any_line_groups = [normalize_screen_text_patterns(group) for group in raw_any_line_groups]
            player_save = str(step.get("player_save", "") or "").strip()
            since_label = str(step.get("since_label", step.get("since_step", "")) or "").strip()
            changed_since = None
            if since_label:
                for previous_report in reversed(reports):
                    if str(previous_report.get("label", "")) == since_label:
                        previous_metadata = previous_report.get("metadata")
                        if isinstance(previous_metadata, dict):
                            changed_since = dict(previous_metadata)
                            changed_since.setdefault("label", since_label)
                        break
                if changed_since is None:
                    raise SystemExit(f"Scenario step '{label}' since_label not found or had no metadata: {since_label}")
            elif not patterns and not line_groups and not any_line_groups:
                changed_since = {}
            if not patterns and not line_groups and not any_line_groups and since_label:
                raise SystemExit(f"Scenario step '{label}' with since_label needs required_patterns/patterns/required_line_patterns/required_any_line_patterns")
            world_dir = save_dir_for_profile(profile) / world
            metadata = audit_player_message_log_contains(
                world_dir,
                run_dir,
                label,
                player_save=player_save,
                changed_since=changed_since,
                patterns=patterns,
                required_line_patterns=line_groups,
                required_any_line_patterns=any_line_groups,
            )
            metadata_json = run_dir / f"{label}.metadata.json"
            metadata["metadata_path"] = str(metadata_json)
            if not metadata.get("artifact_path"):
                metadata["artifact_path"] = str(metadata_json)
            write_json(metadata_json, metadata)
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "scan saved player message log for narrow decisive in-game message lines",
                "expected_immediate_state": "player message log contains the required action-path line(s), captured narrowly without dumping broad logs",
                "failure_rule": "missing decisive player-message line leaves the normal action bridge unproven",
            })
            if metadata.get("status") == "required_state_missing" and bool(step.get("abort_on_metadata_failure", False)):
                report["abort"] = {
                    "guard": "metadata",
                    "status": "aborted_by_metadata_guard",
                    "verdict": str(step.get("abort_verdict", "red_step_required_player_message_missing")),
                    "reason": str(step.get("abort_reason", "required player message line was missing")),
                    "missing_required_items": metadata.get("missing_required_items", []),
                }
                reports.append(report)
                return reports
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
            initial_settle_seconds = float(step.get("initial_settle_seconds", 5.0) or 0.0)
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
                initial_settle_seconds=initial_settle_seconds,
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
                "initial_settle_seconds": initial_settle_seconds,
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
        elif kind == "debug_map_editor_place_furniture":
            furniture_query = str(
                step.get("furniture_query")
                or step.get("furniture")
                or step.get("query")
                or ""
            ).strip()
            if not furniture_query:
                raise SystemExit(f"Scenario step '{label}' needs furniture_query/furniture")
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.45) or 0.45)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_map_editor_place_furniture(
                pid,
                furniture_query=furniture_query,
                target_keys=target_keys,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "furniture_query": furniture_query,
                "target_keys": target_keys,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "M"],
                "selection_path": target_keys + ["r", "/", furniture_query, "enter", "enter"],
                "spawn_target": "map_editor_target_tile",
            })
        elif kind == "debug_map_editor_place_field":
            field_query = str(
                step.get("field_query")
                or step.get("field")
                or step.get("query")
                or ""
            ).strip()
            if not field_query:
                raise SystemExit(f"Scenario step '{label}' needs field_query/field")
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.45) or 0.45)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_map_editor_place_field(
                pid,
                field_query=field_query,
                target_keys=target_keys,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "field_query": field_query,
                "target_keys": target_keys,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "M"],
                "selection_path": target_keys + ["e", "/", field_query, "enter", "enter", "enter"],
                "spawn_target": "map_editor_target_tile",
            })
        elif kind == "debug_map_editor_place_terrain":
            terrain_query = str(
                step.get("terrain_query")
                or step.get("terrain")
                or step.get("query")
                or ""
            ).strip()
            if not terrain_query:
                raise SystemExit(f"Scenario step '{label}' needs terrain_query/terrain")
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.45) or 0.45)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_map_editor_place_terrain(
                pid,
                terrain_query=terrain_query,
                target_keys=target_keys,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "terrain_query": terrain_query,
                "target_keys": target_keys,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "M"],
                "selection_path": target_keys + ["t", "/", terrain_query, "enter", "enter"],
                "spawn_target": "map_editor_target_tile",
            })
        elif kind == "debug_map_editor_place_trap":
            trap_query = str(
                step.get("trap_query")
                or step.get("trap")
                or step.get("query")
                or ""
            ).strip()
            if not trap_query:
                raise SystemExit(f"Scenario step '{label}' needs trap_query/trap")
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.45) or 0.45)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_map_editor_place_trap(
                pid,
                trap_query=trap_query,
                target_keys=target_keys,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "trap_query": trap_query,
                "target_keys": target_keys,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "M"],
                "selection_path": target_keys + ["w", "/", trap_query, "enter", "enter"],
                "spawn_target": "map_editor_target_tile",
            })
        elif kind == "debug_map_editor_place_radiation":
            raw_radiation = step.get("radiation", step.get("radiation_value", step.get("value")))
            if raw_radiation is None or str(raw_radiation).strip() == "":
                raise SystemExit(f"Scenario step '{label}' needs radiation/radiation_value")
            radiation = int(raw_radiation)
            raw_target_keys = step.get("target_keys", [])
            if isinstance(raw_target_keys, str):
                target_keys = [raw_target_keys] if raw_target_keys.strip() else []
            elif isinstance(raw_target_keys, list):
                target_keys = [str(key) for key in raw_target_keys if str(key).strip()]
            else:
                target_keys = []
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.45) or 0.45)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_map_editor_place_radiation(
                pid,
                radiation=radiation,
                target_keys=target_keys,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "radiation": radiation,
                "target_keys": target_keys,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "debug_menu_path": ["}", "m", "M"],
                "selection_path": target_keys + ["q", str(radiation), "enter", "enter"],
                "spawn_target": "map_editor_target_tile",
            })
        elif kind == "audit_saved_bandit_live_world_state":
            def optional_step_int(*keys: str) -> Optional[int]:
                for key in keys:
                    raw_value = step.get(key)
                    if raw_value is None or str(raw_value).strip() == "":
                        continue
                    return int(raw_value)
                return None

            required_member_count = optional_step_int("required_member_count")
            required_ready_at_home_count = optional_step_int("required_ready_at_home_count")
            required_wounded_or_unready_count = optional_step_int("required_wounded_or_unready_count")
            required_active_outside_count = optional_step_int("required_active_outside_count")
            required_active_sortie_started_minutes = optional_step_int("required_active_sortie_started_minutes")
            required_active_sortie_local_contact_minutes = optional_step_int("required_active_sortie_local_contact_minutes")
            required_home_side_signal_count = optional_step_int("required_home_side_signal_count")
            required_lead_bounty = optional_step_int("required_lead_bounty")
            required_lead_threat = optional_step_int("required_lead_threat")
            required_lead_times_harvested = optional_step_int("required_lead_times_harvested")
            required_lead_last_checked_minutes_min = optional_step_int("required_lead_last_checked_minutes_min")
            raw_required_retired_empty_site = step.get("required_retired_empty_site")
            required_retired_empty_site: Optional[bool]
            if raw_required_retired_empty_site is None or str(raw_required_retired_empty_site).strip() == "":
                required_retired_empty_site = None
            elif isinstance(raw_required_retired_empty_site, bool):
                required_retired_empty_site = raw_required_retired_empty_site
            else:
                required_retired_empty_site = str(raw_required_retired_empty_site).strip().lower() in {"1", "true", "yes", "y"}
            raw_required_min_active_member_ids = step.get("required_min_active_member_ids", step.get("required_active_member_count_min"))
            required_min_active_member_ids: Optional[int]
            if raw_required_min_active_member_ids is None or str(raw_required_min_active_member_ids).strip() == "":
                required_min_active_member_ids = None
            else:
                required_min_active_member_ids = int(raw_required_min_active_member_ids)
            raw_required_min_leads = step.get("required_min_leads", step.get("required_lead_count_min"))
            required_min_leads: Optional[int]
            if raw_required_min_leads is None or str(raw_required_min_leads).strip() == "":
                required_min_leads = None
            else:
                required_min_leads = int(raw_required_min_leads)
            raw_required_lead_confidence = step.get("required_lead_confidence")
            required_lead_confidence: Optional[int]
            if raw_required_lead_confidence is None or str(raw_required_lead_confidence).strip() == "":
                required_lead_confidence = None
            else:
                required_lead_confidence = int(raw_required_lead_confidence)
            raw_required_max_offset = step.get("required_active_member_max_abs_offset_ms")
            required_max_offset: Optional[List[int]] = None
            if isinstance(raw_required_max_offset, list) and len(raw_required_max_offset) >= 3:
                required_max_offset = [
                    int(raw_required_max_offset[0]),
                    int(raw_required_max_offset[1]),
                    int(raw_required_max_offset[2]),
                ]
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_bandit_live_world_state(
                    world_dir,
                    required_profile=str(step.get("required_profile", "") or "").strip(),
                    required_site_id_contains=str(step.get("required_site_id_contains", "") or "").strip(),
                    required_active_group_id_exact=step.get("required_active_group_id_exact"),
                    required_active_group_id_contains=str(step.get("required_active_group_id_contains", "") or "").strip(),
                    required_active_target_id_exact=step.get("required_active_target_id_exact"),
                    required_active_target_id_prefix=str(step.get("required_active_target_id_prefix", "") or "").strip(),
                    required_active_target_id_contains=str(step.get("required_active_target_id_contains", "") or "").strip(),
                    required_active_job_type=str(step.get("required_active_job_type", "") or "").strip(),
                    required_active_sortie_started_minutes=required_active_sortie_started_minutes,
                    required_active_sortie_local_contact_minutes=required_active_sortie_local_contact_minutes,
                    required_member_count=required_member_count,
                    required_ready_at_home_count=required_ready_at_home_count,
                    required_wounded_or_unready_count=required_wounded_or_unready_count,
                    required_active_outside_count=required_active_outside_count,
                    required_home_side_signal_count=required_home_side_signal_count,
                    required_retired_empty_site=required_retired_empty_site,
                    required_retirement_summary_contains=str(
                        step.get("required_retirement_summary_contains", "") or ""
                    ).strip(),
                    required_empty_retirement_blocker_contains=str(
                        step.get("required_empty_retirement_blocker_contains", "") or ""
                    ).strip(),
                    required_min_active_member_ids=required_min_active_member_ids,
                    required_active_members_found=bool(step.get("required_active_members_found", False)),
                    required_active_member_max_abs_offset_ms=required_max_offset,
                    player_save=str(step.get("player_save", "") or "").strip(),
                    required_remembered_target_or_mark_prefix=str(
                        step.get("required_remembered_target_or_mark_prefix", "") or ""
                    ).strip(),
                    required_min_leads=required_min_leads,
                    required_lead_source_contains=str(step.get("required_lead_source_contains", "") or "").strip(),
                    required_lead_kind=str(step.get("required_lead_kind", "") or "").strip(),
                    required_lead_target_id=str(step.get("required_lead_target_id", "") or "").strip(),
                    required_lead_target_id_prefix=str(step.get("required_lead_target_id_prefix", "") or "").strip(),
                    required_lead_bounty=required_lead_bounty,
                    required_lead_threat=required_lead_threat,
                    required_lead_times_harvested=required_lead_times_harvested,
                    required_lead_last_checked_minutes_min=required_lead_last_checked_minutes_min,
                    required_lead_status=str(step.get("required_lead_status", "") or "").strip(),
                    required_lead_last_outcome=str(step.get("required_lead_last_outcome", "") or "").strip(),
                    required_lead_confidence=required_lead_confidence,
                    required_known_recent_mark_contains=str(
                        step.get("required_known_recent_mark_contains", "") or ""
                    ).strip(),
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_profile": str(step.get("required_profile", "") or "").strip(),
                    "required_site_id_contains": str(step.get("required_site_id_contains", "") or "").strip(),
                    "required_active_group_id_exact": step.get("required_active_group_id_exact"),
                    "required_active_group_id_contains": str(step.get("required_active_group_id_contains", "") or "").strip(),
                    "required_active_target_id_exact": step.get("required_active_target_id_exact"),
                    "required_active_target_id_prefix": str(step.get("required_active_target_id_prefix", "") or "").strip(),
                    "required_active_target_id_contains": str(step.get("required_active_target_id_contains", "") or "").strip(),
                    "required_active_job_type": str(step.get("required_active_job_type", "") or "").strip(),
                    "required_active_sortie_started_minutes": required_active_sortie_started_minutes,
                    "required_active_sortie_local_contact_minutes": required_active_sortie_local_contact_minutes,
                    "required_member_count": required_member_count,
                    "required_ready_at_home_count": required_ready_at_home_count,
                    "required_wounded_or_unready_count": required_wounded_or_unready_count,
                    "required_active_outside_count": required_active_outside_count,
                    "required_home_side_signal_count": required_home_side_signal_count,
                    "required_retired_empty_site": required_retired_empty_site,
                    "required_retirement_summary_contains": str(
                        step.get("required_retirement_summary_contains", "") or ""
                    ).strip(),
                    "required_empty_retirement_blocker_contains": str(
                        step.get("required_empty_retirement_blocker_contains", "") or ""
                    ).strip(),
                    "required_min_active_member_ids": required_min_active_member_ids,
                    "required_active_members_found": bool(step.get("required_active_members_found", False)),
                    "required_active_member_max_abs_offset_ms": required_max_offset,
                    "player_save": str(step.get("player_save", "") or "").strip(),
                    "required_min_leads": required_min_leads,
                    "required_lead_kind": str(step.get("required_lead_kind", "") or "").strip(),
                    "required_lead_target_id": str(step.get("required_lead_target_id", "") or "").strip(),
                    "required_lead_target_id_prefix": str(step.get("required_lead_target_id_prefix", "") or "").strip(),
                    "required_lead_bounty": required_lead_bounty,
                    "required_lead_threat": required_lead_threat,
                    "required_lead_times_harvested": required_lead_times_harvested,
                    "required_lead_last_checked_minutes_min": required_lead_last_checked_minutes_min,
                    "required_lead_status": str(step.get("required_lead_status", "") or "").strip(),
                    "required_lead_last_outcome": str(step.get("required_lead_last_outcome", "") or "").strip(),
                    "required_lead_confidence": required_lead_confidence,
                    "required_known_recent_mark_contains": str(
                        step.get("required_known_recent_mark_contains", "") or ""
                    ).strip(),
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "metadata": metadata,
                "action_description": "read saved dimension-data bandit_live_world metadata and require active site/profile/target/signal-state persistence",
                "expected_immediate_state": "saved bandit_live_world metadata contains the required active group/profile/target/signal state before persistence proof may continue",
                "failure_rule": "missing required saved bandit_live_world metadata or unreadable dimension data makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": str(
                            step.get(
                                "abort_verdict",
                                "blocked_untrusted_saved_bandit_live_world_state",
                            )
                        ),
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved bandit_live_world metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_game_turn":
            player_save = str(step.get("player_save", "") or "").strip()
            record_baseline = bool(step.get("record_baseline", False))
            baseline_label = str(step.get("baseline_label", "") or "").strip()
            baseline_metadata = None
            if baseline_label:
                for prior_report in reports:
                    if str(prior_report.get("label", "") or "").strip() == baseline_label:
                        if isinstance(prior_report.get("metadata"), dict):
                            baseline_metadata = prior_report["metadata"]
                        break
                if baseline_metadata is None:
                    raise SystemExit(f"Scenario step '{label}' baseline_label not found or has no metadata: {baseline_label}")
            raw_min_delta = step.get("required_min_delta_turns")
            required_min_delta = int(raw_min_delta) if raw_min_delta is not None and str(raw_min_delta).strip() != "" else None
            raw_required_time_of_day = step.get("required_time_of_day_seconds")
            required_time_of_day = int(raw_required_time_of_day) if raw_required_time_of_day is not None and str(raw_required_time_of_day).strip() != "" else None
            required_time_tolerance = int(step.get("required_time_tolerance_seconds", 0) or 0)
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_game_turn(
                    world_dir,
                    player_save=player_save,
                    record_baseline=record_baseline,
                    baseline_metadata=baseline_metadata,
                    required_min_delta_turns=required_min_delta,
                    required_time_of_day_seconds=required_time_of_day,
                    required_time_tolerance_seconds=required_time_tolerance,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "player_save": player_save,
                    "baseline_label": baseline_label,
                    "required_min_delta_turns": required_min_delta,
                    "required_time_of_day_seconds": required_time_of_day,
                    "required_time_tolerance_seconds": required_time_tolerance,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "read saved player turn counter and require a post-action turn delta from the recorded baseline",
                "expected_immediate_state": "saved player turn counter baseline/delta proves the actual game turn path advanced, not just that turn keys were pressed",
                "failure_rule": "missing saved turn counter, missing baseline, or insufficient turn delta makes this local-turn proof red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": str(step.get("abort_verdict", "blocked_untrusted_saved_game_turn_delta")),
                        "reason": str(step.get("abort_reason", "required saved game-turn delta was missing or too small")),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_weather_state":
            required_weather_id = str(step.get("required_weather_id", step.get("weather_id", "")) or "").strip()
            raw_required_temperature = step.get("required_temperature_f", step.get("temperature_f"))
            required_temperature_f: Optional[float]
            if raw_required_temperature is None or str(raw_required_temperature).strip() == "":
                required_temperature_f = None
            else:
                try:
                    required_temperature_f = float(raw_required_temperature)
                except (TypeError, ValueError):
                    raise SystemExit(f"Scenario step '{label}' required_temperature_f/temperature_f must be numeric")
            temperature_tolerance_f = float(step.get("temperature_tolerance_f", 0.75) or 0.75)
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_weather_state(
                    world_dir,
                    required_weather_id=required_weather_id,
                    required_temperature_f=required_temperature_f,
                    temperature_tolerance_f=temperature_tolerance_f,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_weather_id": required_weather_id,
                    "required_temperature_f": required_temperature_f,
                    "temperature_tolerance_f": temperature_tolerance_f,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "metadata": metadata,
                "action_description": "read saved dimension weather metadata and require exact weather/temperature target state",
                "expected_immediate_state": "saved dimension weather metadata contains the required weather/temperature state before weather primitive proof may continue",
                "failure_rule": "missing required saved weather/temperature metadata or unreadable dimension data makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": str(
                            step.get(
                                "abort_verdict",
                                "blocked_untrusted_saved_weather_target_state",
                            )
                        ),
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved weather/temperature metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
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
            count_selection_mode = str(step.get("count_selection_mode", "type_before_select") or "type_before_select").strip()
            selection_mode = drop_item(
                pid,
                query_or_slot=query_or_slot,
                count=count,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
                count_selection_mode=count_selection_mode,
            )
            report.update({
                "query_or_slot": query_or_slot,
                "count": count,
                "delay_ms": delay_ms,
                "type_delay_ms": type_delay_ms,
                "menu_settle_seconds": menu_settle_seconds,
                "prompt_settle_seconds": prompt_settle_seconds,
                "count_selection_mode": count_selection_mode,
                "inventory_action": "drop_item",
                "selection_mode": selection_mode,
            })
        elif kind == "audit_player_save_mtime":
            player_save = str(step.get("player_save", "") or "").strip()
            changed_since_label = str(step.get("changed_since_label", "") or "").strip()
            changed_since: Optional[Dict[str, Any]] = None
            if changed_since_label:
                for previous_report in reversed(reports):
                    if str(previous_report.get("label", "")) == changed_since_label:
                        previous_metadata = previous_report.get("metadata")
                        if isinstance(previous_metadata, dict):
                            changed_since = dict(previous_metadata)
                            changed_since.setdefault("label", changed_since_label)
                        break
                if changed_since is None:
                    changed_since = {"label": changed_since_label}
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_player_save_mtime(
                    world_dir,
                    player_save=player_save,
                    changed_since=changed_since,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "player_save": player_save,
                    "required_change_since_label": changed_since_label,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "changed_since_label": changed_since_label,
                "metadata": metadata,
                "action_description": "read saved-player file mtime and optionally require same-run save/writeback progress",
                "expected_immediate_state": (
                    "saved-player file mtime changed after the requested baseline before saved-map feature proof may continue"
                    if changed_since_label
                    else "saved-player file mtime baseline is recorded for a later same-run save/writeback gate"
                ),
                "failure_rule": (
                    "missing player-save mtime progress or unreadable save metadata makes this step red/blocked"
                    if changed_since_label
                    else "missing or unreadable player-save mtime baseline makes the later writeback gate untrusted"
                ),
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": metadata_verdict,
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required player-save mtime progress was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_player_items":
            player_save = str(step.get("player_save", "") or "").strip()
            required_items = [str(item).strip() for item in step.get("required_items", step.get("required_item_ids", [])) if str(item).strip()]
            required_weapon = str(step.get("required_weapon", step.get("required_weapon_typeid", "")) or "").strip()
            required_weapon_ammo_type = str(step.get("required_weapon_ammo_type", "") or "").strip()
            required_weapon_ammo_min = int(step.get("required_weapon_ammo_min", 0) or 0)
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_player_items(
                    world_dir,
                    player_save=player_save,
                    required_items=required_items,
                    required_weapon=required_weapon,
                    required_weapon_ammo_type=required_weapon_ammo_type,
                    required_weapon_ammo_min=required_weapon_ammo_min,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_items": required_items,
                    "required_weapon": required_weapon,
                    "required_weapon_ammo_type": required_weapon_ammo_type,
                    "required_weapon_ammo_min": required_weapon_ammo_min,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "read saved player inventory/wield state and require exact item type metadata",
                "expected_immediate_state": "saved player inventory/wield state contains the required metadata before feature proof may continue",
                "failure_rule": "missing required saved-player item/wield metadata or unreadable save metadata makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": metadata_verdict,
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved-player metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_active_monsters":
            player_save = str(step.get("player_save", "") or "").strip()
            required_typeids = [
                str(monster).strip()
                for monster in step.get("required_typeids", step.get("required_monster_typeids", []))
                if str(monster).strip()
            ]
            required_monsters_raw = step.get("required_monsters", [])
            if not isinstance(required_monsters_raw, list):
                raise SystemExit(f"Scenario step '{label}' required_monsters must be a list")
            required_monsters = [
                monster for monster in required_monsters_raw if isinstance(monster, dict)
            ]
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_active_monsters(
                    world_dir,
                    player_save=player_save,
                    required_monsters=required_monsters,
                    required_typeids=required_typeids,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_monsters": required_monsters,
                    "required_typeids": required_typeids,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "read saved player active_monsters and require exact monster type/location metadata",
                "expected_immediate_state": "saved player active_monsters contains the required debug-spawned monster metadata before spawn proof may continue",
                "failure_rule": "missing required saved active-monster metadata or unreadable save metadata makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": metadata_verdict,
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved active-monster metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_hordes_near_player":
            player_save = str(step.get("player_save", "") or "").strip()
            required_hordes_raw = step.get("required_hordes", step.get("required_monsters", []))
            if not isinstance(required_hordes_raw, list):
                raise SystemExit(f"Scenario step '{label}' required_hordes must be a list")
            required_hordes = [horde for horde in required_hordes_raw if isinstance(horde, dict)]
            radius_raw = step.get("radius_ms")
            radius_ms = int(radius_raw) if radius_raw is not None and str(radius_raw).strip() != "" else None
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_hordes_near_player(
                    world_dir,
                    player_save=player_save,
                    required_hordes=required_hordes,
                    radius_ms=radius_ms,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_hordes": required_hordes,
                    "radius_ms": radius_ms,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "read saved overmap horde_map entries near the player and require exact horde type/location metadata",
                "expected_immediate_state": "saved overmap horde_map contains the required staged horde metadata before roof/fire horde proof may continue",
                "failure_rule": "missing required saved horde metadata or unreadable overmap metadata makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": str(
                            step.get(
                                "abort_verdict",
                                "blocked_untrusted_saved_horde_target_state",
                            )
                        ),
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved horde metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_overmap_npcs":
            player_save = str(step.get("player_save", "") or "").strip()
            required_npcs_raw = step.get("required_npcs", [])
            if not isinstance(required_npcs_raw, list):
                raise SystemExit(f"Scenario step '{label}' required_npcs must be a list")
            required_npcs = [npc for npc in required_npcs_raw if isinstance(npc, dict)]
            required_new_npcs_raw = step.get("required_new_npcs", [])
            if not isinstance(required_new_npcs_raw, list):
                raise SystemExit(f"Scenario step '{label}' required_new_npcs must be a list")
            required_new_npcs = [npc for npc in required_new_npcs_raw if isinstance(npc, dict)]
            record_baseline = bool(step.get("record_baseline", False))
            baseline_label = str(step.get("baseline_label", "") or "").strip()
            baseline_metadata = None
            if baseline_label:
                for prior_report in reports:
                    if str(prior_report.get("label", "") or "").strip() == baseline_label:
                        if isinstance(prior_report.get("metadata"), dict):
                            baseline_metadata = prior_report["metadata"]
                        break
                if baseline_metadata is None:
                    raise SystemExit(f"Scenario step '{label}' baseline_label not found or has no metadata: {baseline_label}")
            required_delta_raw = step.get("required_observed_npc_count_delta")
            required_delta = int(required_delta_raw) if required_delta_raw is not None else None
            scan_all_overmaps = bool(step.get("scan_all_overmaps", False))
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_overmap_npcs(
                    world_dir,
                    player_save=player_save,
                    required_npcs=required_npcs,
                    scan_all_overmaps=scan_all_overmaps,
                    record_baseline=record_baseline,
                    baseline_metadata=baseline_metadata,
                    required_observed_npc_count_delta=required_delta,
                    required_new_npcs=required_new_npcs,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_npcs": required_npcs,
                    "required_new_npcs": required_new_npcs,
                    "baseline_label": baseline_label,
                    "required_observed_npc_count_delta": required_delta,
                    "scan_all_overmaps": scan_all_overmaps,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "metadata": metadata,
                "action_description": "read saved overmap npcs and require exact NPC/follower metadata",
                "expected_immediate_state": "saved overmap npcs contains the required debug-spawned NPC/follower metadata and any required baseline delta before spawn proof may continue",
                "failure_rule": "missing required saved overmap NPC metadata, missing baseline delta, or unreadable save metadata makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": str(
                            step.get(
                                "abort_verdict",
                                "blocked_untrusted_saved_overmap_npc_target_state",
                            )
                        ),
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved overmap NPC/follower metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_map_tile_near_player":
            raw_offsets = step.get("offsets", step.get("offset", []))
            offsets: Optional[List[Tuple[int, int, int]]]
            if isinstance(raw_offsets, list) and raw_offsets and all(isinstance(entry, list) for entry in raw_offsets):
                offsets = [
                    (int(entry[0]), int(entry[1]), int(entry[2]))
                    for entry in raw_offsets
                    if isinstance(entry, list) and len(entry) >= 3
                ]
            elif isinstance(raw_offsets, list):
                offsets = parse_map_tile_offsets(raw_offsets) if raw_offsets else None
            else:
                raise SystemExit(f"Scenario step '{label}' offsets must be a list")
            radius = int(step.get("radius", 2) or 2)
            player_save = str(step.get("player_save", "") or "").strip()
            required_terrain = [str(terrain).strip() for terrain in step.get("required_terrain", step.get("required_terrain_ids", [])) if str(terrain).strip()]
            required_fields = [str(field).strip() for field in step.get("required_fields", step.get("required_field_ids", [])) if str(field).strip()]
            required_items = [str(item).strip() for item in step.get("required_items", step.get("required_item_ids", [])) if str(item).strip()]
            required_furniture = [str(furn).strip() for furn in step.get("required_furniture", step.get("required_furniture_ids", [])) if str(furn).strip()]
            required_traps = [str(trap).strip() for trap in step.get("required_traps", step.get("required_trap_ids", [])) if str(trap).strip()]
            required_radiation = [int(rad) for rad in step.get("required_radiation", step.get("required_radiation_values", []))]
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_map_tiles_near_player(
                    world_dir,
                    player_save=player_save,
                    radius=radius,
                    offsets=offsets,
                    required_terrain=required_terrain,
                    required_fields=required_fields,
                    required_items=required_items,
                    required_furniture=required_furniture,
                    required_traps=required_traps,
                    required_radiation=required_radiation,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_terrain": required_terrain,
                    "required_fields": required_fields,
                    "required_items": required_items,
                    "required_furniture": required_furniture,
                    "required_traps": required_traps,
                    "required_radiation": required_radiation,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "offsets": [list(offset) for offset in offsets] if offsets is not None else [],
                "radius": radius,
                "metadata": metadata,
                "action_description": "read saved map tiles near player and require exact terrain/field/item/furniture/trap/radiation metadata",
                "expected_immediate_state": "saved map contains the required target-tile metadata before feature proof may continue",
                "failure_rule": "missing required saved-map terrain/field/item/furniture/trap/radiation or unreadable save metadata makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": metadata_verdict,
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved-map metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
        elif kind == "audit_saved_zones_near_player":
            raw_offsets = step.get("offsets", step.get("offset", []))
            offsets: Optional[List[Tuple[int, int, int]]]
            if isinstance(raw_offsets, list) and raw_offsets and all(isinstance(entry, list) for entry in raw_offsets):
                offsets = [
                    (int(entry[0]), int(entry[1]), int(entry[2]))
                    for entry in raw_offsets
                    if isinstance(entry, list) and len(entry) >= 3
                ]
            elif isinstance(raw_offsets, list):
                offsets = parse_map_tile_offsets(raw_offsets) if raw_offsets else None
            else:
                raise SystemExit(f"Scenario step '{label}' offsets must be a list")
            player_save = str(step.get("player_save", "") or "").strip()
            required_zone_type = str(step.get("required_zone_type", step.get("zone_type", "")) or "").strip()
            required_name_contains = str(step.get("required_name_contains", step.get("zone_name_contains", "")) or "").strip()
            required_faction = str(step.get("required_faction", "") or "").strip()
            world_dir = save_dir_for_profile(profile) / world
            metadata_artifact = run_dir / f"{label}.metadata.json"
            try:
                metadata = audit_saved_zones_near_player(
                    world_dir,
                    player_save=player_save,
                    required_zone_type=required_zone_type,
                    required_name_contains=required_name_contains,
                    required_faction=required_faction,
                    required_offsets=offsets,
                )
            except (Exception, SystemExit) as exc:
                metadata = {
                    "status": "failed",
                    "error": str(exc),
                    "world": world,
                    "world_dir": str(world_dir),
                    "required_zone_type": required_zone_type,
                    "required_name_contains": required_name_contains,
                    "required_faction": required_faction,
                }
            metadata["artifact_path"] = str(metadata_artifact)
            metadata_artifact.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
            report.update({
                "world_dir": str(world_dir),
                "player_save": player_save,
                "offsets": [list(offset) for offset in offsets] if offsets is not None else [],
                "metadata": metadata,
                "action_description": "read saved zone metadata near player and require exact zone type/faction/coverage",
                "expected_immediate_state": "saved zone metadata contains the required zone covering the target offset(s) before feature proof may continue",
                "failure_rule": "missing required saved-zone metadata or unreadable zone files makes this step red/blocked",
            })
            if bool(step.get("abort_on_metadata_failure", False)):
                metadata_verdict, metadata_issues = metadata_checkpoint_verdict(metadata)
                if not metadata_verdict.startswith("green"):
                    report["abort"] = {
                        "guard": "metadata",
                        "status": "aborted_by_metadata_guard",
                        "verdict": metadata_verdict,
                        "reason": str(
                            step.get(
                                "abort_reason",
                                "required saved-zone metadata was missing or unreadable",
                            )
                        ),
                        "issues": metadata_issues,
                    }
                    reports.append(report)
                    return reports
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
                    if apply_screen_text_expectation_abort_guard(
                        report,
                        step,
                        report["screen_text_expectation"],
                    ):
                        reports.append(report)
                        return reports
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
                    if apply_screen_text_expectation_abort_guard(
                        report,
                        step,
                        report["screen_after_text_expectation"],
                    ):
                        reports.append(report)
                        return reports
                if apply_screen_text_abort_guard(report, step, screen_text_report):
                    reports.append(report)
                    return reports
        report["artifact_log_end_size"] = artifact_log.stat().st_size if artifact_log is not None and artifact_log.exists() else artifact_log_start_size
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
    matched_artifacts = bool(matches_by_pattern) and all(entry.get("lines") for entry in matches_by_pattern)
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
        profile=profile,
        world=world,
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

    step_artifact_matches: List[Dict[str, Any]] = []
    for step_report in step_reports:
        if str(step_report.get("kind", "")).strip().lower() not in {"audit_log_contains", "audit_player_message_log_contains"}:
            continue
        metadata = step_report.get("metadata")
        if not isinstance(metadata, dict):
            continue
        for entry in metadata.get("matches_by_pattern", []):
            if isinstance(entry, dict) and entry.get("lines"):
                step_artifact_matches.append({
                    "pattern": f"{step_report.get('label', '')}: {entry.get('pattern', '')}",
                    "lines": entry.get("lines", []),
                    "source": str(metadata.get("source_log", "")),
                    "artifact_path": str(metadata.get("artifact_path", "")),
                    "capture_policy": str(metadata.get("capture_policy", "")),
                })
    effective_artifact_patterns = list(artifact_patterns)
    effective_matches_by_pattern = list(matches_by_pattern)
    if not effective_artifact_patterns and step_artifact_matches:
        effective_artifact_patterns = [str(entry.get("pattern", "")) for entry in step_artifact_matches]
        effective_matches_by_pattern = step_artifact_matches

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
    if not all_matched_lines and effective_matches_by_pattern is step_artifact_matches:
        all_matched_lines = [line for entry in effective_matches_by_pattern for line in entry.get("lines", [])]
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
    elif matches_by_pattern and all(entry["lines"] for entry in matches_by_pattern):
        verdict = "artifacts_matched"
    elif any(entry["lines"] for entry in matches_by_pattern):
        verdict = "inconclusive_partial_artifact_match"
    elif not artifact_text.strip():
        verdict = "inconclusive_no_new_artifacts"

    startup_classification = start_result.get("proof_classification") if isinstance(start_result.get("proof_classification"), dict) else {}
    proof_classification = probe_proof_classification(
        verdict=verdict,
        startup_classification=startup_classification,
        step_reports=step_reports,
        artifact_patterns=effective_artifact_patterns,
        matches_by_pattern=effective_matches_by_pattern,
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
            "match_patterns": effective_artifact_patterns,
            "matches_by_pattern": effective_matches_by_pattern,
            "step_artifact_matches": step_artifact_matches,
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
