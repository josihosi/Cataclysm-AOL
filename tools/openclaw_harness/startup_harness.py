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


def current_head_short() -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo_root()), "rev-parse", "--short=10", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.stdout.strip()


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

    python_value = options.get("LLM_INTENT_PYTHON", "")
    python_cmd, python_label = resolve_configured_python_command(python_value)
    if not python_cmd:
        blockers.append({
            "code": "llm_python_missing",
            "message": "LLM runner Python path is empty; API/Ollama probes cannot launch the runner.",
            "option": "LLM_INTENT_PYTHON",
        })

    backend = options.get("LLM_INTENT_BACKEND", "").strip().lower()
    if backend == "api":
        api_key_env = options.get("LLM_INTENT_API_KEY_ENV", "").strip()
        if not api_key_env:
            blockers.append({
                "code": "llm_api_key_env_missing",
                "message": "API backend is selected, but no API key env-var name is configured.",
                "option": "LLM_INTENT_API_KEY_ENV",
            })
        elif not os.environ.get(api_key_env):
            blockers.append({
                "code": "llm_api_key_env_unset",
                "message": f"API backend is selected, but env var '{api_key_env}' is not set for the harness process.",
                "option": "LLM_INTENT_API_KEY_ENV",
                "env_var": api_key_env,
            })

        if python_cmd:
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
                    "message": f"Configured LLM runner Python cannot import any_llm ({detail}).",
                    "option": "LLM_INTENT_PYTHON",
                    "python": python_label,
                })

    return blockers


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
    cmd = ["peekaboo", "type", text, "--pid", str(pid), "--delay", str(delay_ms), "--profile", "linear"]
    run_peekaboo_interaction(pid, cmd)


def advance_turns(pid: int, count: int) -> None:
    if count <= 0:
        return
    peekaboo_press_sequence(pid, ["tab"] * count)


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
    if selection_mode == "slot":
        peekaboo_press_sequence(pid, [selector], delay_ms=delay_ms)
        time.sleep(prompt_settle_seconds)
    else:
        apply_uilist_filter(
            pid,
            selector,
            delay_ms=delay_ms,
            type_delay_ms=type_delay_ms,
            settle_seconds=prompt_settle_seconds,
        )
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


def debug_spawn_item(
    pid: int,
    *,
    item_query: str,
    count: int = 1,
    delay_ms: int = 200,
    type_delay_ms: int = 20,
    menu_settle_seconds: float = 0.35,
    prompt_settle_seconds: float = 0.25,
    exit_menu: bool = True,
) -> None:
    if count <= 0:
        raise SystemExit("Item spawn count must be > 0")
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


def resolve_artifact_source(profile: str, source: str) -> Tuple[Path, bool, str]:
    normalized = source.strip().lower() if source else "debug.log"
    if normalized in {"debug", "debug.log"}:
        return config_dir_for_profile(profile) / "debug.log", True, "debug.log"
    if normalized in {"llm", "llm_intent", "llm_intent.log"}:
        return config_dir_for_profile(profile) / "llm_intent.log", False, "llm_intent.log"
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


def summarize_peekaboo_capture(stdout: str, png_path: Path, json_path: Path) -> Dict[str, Any]:
    repo_head = current_head_short()
    summary: Dict[str, Any] = {
        "png_path": str(png_path),
        "peekaboo_json": str(json_path),
        "repo_head": repo_head,
        "peekaboo_success": False,
        "window_title": "",
        "captured_head": "",
        "captured_dirty": False,
        "version_matches_repo_head": None,
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
    summary["peekaboo_success"] = bool(payload.get("success"))
    summary["window_title"] = str(data.get("window_title", "")).strip()
    build_info = extract_window_build_info(summary["window_title"])
    summary.update(build_info)
    if summary["captured_head"]:
        summary["version_matches_repo_head"] = summary["captured_head"] == repo_head
    return summary


def capture_screenshot(pid: int, run_dir: Path, label: str) -> Dict[str, Any]:
    png_path = run_dir / f"{label}.png"
    json_path = run_dir / f"{label}.peekaboo.json"
    proc = subprocess.run(
        ["peekaboo", "see", "--pid", str(pid), "--path", str(png_path), "--json"],
        capture_output=True,
        text=True,
        check=False,
    )
    payload = {
        "label": label,
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "screen_summary": summarize_peekaboo_capture(proc.stdout, png_path, json_path),
    }
    json_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return payload


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


def execute_probe_steps(pid: int, run_dir: Path, steps: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
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
            report.update({"text": text, "delay_ms": delay_ms, "submit": submit})
            if submit:
                submit_key = str(step.get("submit_key", "enter") or "enter")
                submit_delay_ms = int(step.get("submit_delay_ms", 200) or 200)
                peekaboo_press_sequence(pid, [submit_key], delay_ms=submit_delay_ms)
                report.update({"submit_key": submit_key, "submit_delay_ms": submit_delay_ms})
        elif kind == "advance_turns":
            count = int(step.get("count", 0) or 0)
            if count <= 0:
                raise SystemExit(f"Scenario step '{label}' needs count > 0")
            advance_turns(pid, count)
            report["count"] = count
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
            delay_ms = int(step.get("delay_ms", 200) or 200)
            type_delay_ms = int(step.get("type_delay_ms", 20) or 20)
            menu_settle_seconds = float(step.get("menu_settle_seconds", 0.35) or 0.35)
            prompt_settle_seconds = float(step.get("prompt_settle_seconds", 0.25) or 0.25)
            debug_spawn_item(
                pid,
                item_query=item_query,
                count=count,
                delay_ms=delay_ms,
                type_delay_ms=type_delay_ms,
                menu_settle_seconds=menu_settle_seconds,
                prompt_settle_seconds=prompt_settle_seconds,
            )
            report.update({
                "item_query": item_query,
                "count": count,
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
            capture = capture_screenshot(pid, run_dir, label)
            report["screen"] = capture.get("screen_summary", {})
        else:
            raise SystemExit(f"Unsupported scenario step kind: {kind or '<empty>'}")
        if settle_seconds > 0:
            time.sleep(settle_seconds)
        if kind != "capture" and bool(step.get("capture_after", False)):
            capture = capture_screenshot(pid, run_dir, f"{label}.after")
            report["screen_after"] = capture.get("screen_summary", {})
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


def install_fixture(profile: str, fixture_name: str, replace: bool, fixture_profile: str = "") -> Dict[str, Any]:
    source_profile = resolve_profile_name(fixture_profile or profile)
    fixture_dir = profile_fixture_root(source_profile) / fixture_name
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
        "fixture_profile": source_profile,
        "installed_worlds": installed_worlds,
        "destination": str(save_dst),
    }


def install_profile_snapshot(profile: str, snapshot_name: str, snapshot_profile: str = "") -> Dict[str, Any]:
    source_profile = resolve_profile_name(snapshot_profile or profile)
    snapshot_dir = profile_snapshot_root(source_profile) / snapshot_name
    if not snapshot_dir.exists():
        raise SystemExit(f"Profile snapshot not found: {snapshot_dir}")

    userdir = userdir_for_profile(profile)
    ensure_dir(userdir)
    copied_entries = []
    skipped_entries = []
    for entry in sorted(snapshot_dir.iterdir(), key=lambda p: p.name.lower()):
        if entry.name in {"manifest.json", "save", "harness_runs"}:
            skipped_entries.append(entry.name)
            continue
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

    manifest = load_fixture_manifest(snapshot_dir)
    return {
        "profile": profile,
        "snapshot": snapshot_name,
        "snapshot_profile": source_profile,
        "source_path": str(snapshot_dir),
        "destination": str(userdir),
        "copied_entries": copied_entries,
        "skipped_entries": skipped_entries,
        "manifest": manifest,
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
    profile_snapshot = str(getattr(args, "profile_snapshot", "") or "").strip()
    if profile_snapshot:
        install_profile_snapshot(
            profile,
            profile_snapshot,
            snapshot_profile=getattr(args, "profile_snapshot_profile", ""),
        )
    if args.fixture:
        install_fixture(
            profile,
            args.fixture,
            replace=args.replace_existing_worlds,
            fixture_profile=getattr(args, "fixture_profile", ""),
        )
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
            screen = capture_screenshot(proc.pid, run_dir, "failure_process_exit")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            result = {
                "ok": False,
                "reason": "process_exited",
                "returncode": code,
                "pid": proc.pid,
                "profile": profile,
                "run_dir": str(run_dir),
                "screen": screen.get("screen_summary", {}),
            }
            write_json(run_dir / "startup.result.json", result)
            print(json.dumps(result, indent=2, ensure_ascii=False))
            return 1

        ok, data = success_from_lastworld(profile, baseline_mtime, expected_world)
        if ok:
            post_lastworld_wait = float(startup_cfg.get("post_lastworld_wait_seconds", 0.0) or 0.0)
            if post_lastworld_wait > 0:
                time.sleep(post_lastworld_wait)
                peekaboo_focus_pid(proc.pid)
            screen = capture_screenshot(proc.pid, run_dir, "success")
            copy_filtered_debug_log_delta_if_exists(debug_log, debug_size, run_dir / "debug.final.log")
            copy_file_if_exists(lastworld, run_dir / "lastworld.after.json")
            result = {
                "ok": True,
                "pid": proc.pid,
                "profile": profile,
                "ok_with_debug_popups": dismissals > 0,
                "debug_popups_recorded": dismissals,
                "strategy": plan.strategy,
                "post_lastworld_wait_seconds": post_lastworld_wait,
                "lastworld": data,
                "run_dir": str(run_dir),
                "screen": screen.get("screen_summary", {}),
            }
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
    result = {
        "ok": False,
        "reason": "startup_timeout",
        "pid": proc.pid,
        "profile": profile,
        "strategy": plan.strategy,
        "run_dir": str(run_dir),
        "screen": screen.get("screen_summary", {}),
    }
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


def run_probe(args: argparse.Namespace) -> int:
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
        artifact_patterns = [str(pattern).strip() for pattern in raw_patterns if str(pattern).strip()]
    else:
        artifact_patterns = []
    if not artifact_patterns:
        artifact_pattern = args.artifact_pattern or str(scenario.get("artifact_pattern", "")).strip()
        artifact_patterns = [artifact_pattern] if artifact_pattern else []
    elif args.artifact_pattern:
        artifact_patterns = [args.artifact_pattern]
    recommended_test_command = args.test_command or str(scenario.get("recommended_test_command", "")).strip()
    artifact_source = str(scenario.get("artifact_source", "debug.log")).strip() or "debug.log"
    steps = normalize_scenario_steps(scenario.get("steps", []), advance_count, settle_seconds)

    if blocker_info["status"] == "blocked":
        run_dir = create_run_dir(profile)
        report = {
            "ok": False,
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
            "verdict": "blocked_helper_gap",
        }
        write_json(run_dir / "probe.report.json", report)
        print(json.dumps(report, indent=2, ensure_ascii=False))
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
            },
            "startup_plan": start_result,
            "start_command": start_cmd,
        }, indent=2, ensure_ascii=False))
        return 0

    run_dir_value = str(start_result.get("run_dir", "")).strip()
    run_dir = Path(run_dir_value) if run_dir_value else None
    if start_rc != 0 or not start_result.get("ok"):
        report = {
            "ok": False,
            "scenario": str(scenario.get("name", args.scenario)),
            "reason": "startup_failed",
            "startup": start_result,
            "start_command": start_cmd,
            "start_stdout": start_stdout,
            "start_stderr": start_stderr,
        }
        if run_dir is not None:
            write_json(run_dir / "probe.report.json", report)
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 1

    pid = int(start_result.get("pid", 0))
    if pid <= 0 or run_dir is None:
        report = {
            "ok": False,
            "scenario": str(scenario.get("name", args.scenario)),
            "reason": "startup_missing_runtime_details",
            "startup": start_result,
        }
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 1

    artifact_log, filter_debug_noise, resolved_artifact_source = resolve_artifact_source(profile, artifact_source)
    runtime_blockers = probe_runtime_blockers(profile, resolved_artifact_source)
    if runtime_blockers:
        blocked_screen = capture_screenshot(pid, run_dir, "probe_blocked")
        report = {
            "ok": True,
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
            "verdict": "blocked_runtime_prereqs",
        }
        write_json(run_dir / "probe.report.json", report)
        print(json.dumps(report, indent=2, ensure_ascii=False))
        return 0

    artifact_start = artifact_log.stat().st_size if artifact_log.exists() else 0
    screen_before = capture_screenshot(pid, run_dir, "probe_before")
    step_reports = execute_probe_steps(pid, run_dir, steps)
    screen_after = capture_screenshot(pid, run_dir, "probe_after")

    artifact_text = read_log_delta(artifact_log, artifact_start, filter_debug_noise=filter_debug_noise)
    artifact_path = run_dir / "probe.artifacts.log"
    if artifact_text:
        artifact_path.write_text(artifact_text, encoding="utf-8")
    matches_by_pattern = []
    all_matched_lines: List[str] = []
    for pattern in artifact_patterns:
        lines = [line for line in artifact_text.splitlines() if pattern in line]
        matches_by_pattern.append({"pattern": pattern, "lines": lines})
        all_matched_lines.extend(lines)

    verdict = "inconclusive_no_artifact_match"
    screen_summary = start_result.get("screen", {}) if isinstance(start_result.get("screen"), dict) else {}
    if screen_summary.get("version_matches_repo_head") is False:
        verdict = "inconclusive_version_mismatch"
    elif any(entry["lines"] for entry in matches_by_pattern):
        verdict = "artifacts_matched"
    elif not artifact_text.strip():
        verdict = "inconclusive_no_new_artifacts"

    report = {
        "ok": True,
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
        },
        "startup": start_result,
        "steps": step_reports,
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
        "verdict": verdict,
    }
    write_json(run_dir / "probe.report.json", report)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0


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
    parser.error(f"Unknown command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
