#!/usr/bin/env python3
"""Bind a DE-67 workspace clock and install one guarded upstream push."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import re
import shlex
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from contextlib import closing
from pathlib import Path
from typing import Any, Sequence


MANAGED_HOOK_MARKER = "# managed by DE-67 workspace_setup.py v1"
CONFIG_RELATIVE_PATH = Path(".de67/state/workspace.json")
PUSH_STATUS_RELATIVE_PATH = Path(".de67/state/checkpoint-push.json")
DEADLINE_STATE_RELATIVE_PATH = Path(".de67/state/deadlines.sqlite3")
STATE_IGNORE_SENTINEL = Path(".de67/state/.workspace-setup-ignore-probe.tmp")
PHASE3_ENVIRONMENT_ROOT = (
    Path(__file__).resolve().parents[1] / "de-67-3" / "assets" / "environment"
)
PHASE3_WORKSPACE_FILES = (
    "phase3-policy.d67",
    "phase3-policy.json",
    "phase3-contracts.json",
    "test-and-task-guidelines.md",
    "work-ledger.md",
    "mutation-suggestions.md",
)


class SetupError(RuntimeError):
    """Raised when workspace setup or a guarded push is unsafe."""


def _run_git(
    workspace: Path,
    arguments: Sequence[str],
    *,
    check: bool = True,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(workspace), *arguments],
        text=True,
        capture_output=True,
        env=environment,
        check=False,
    )
    if check and result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise SetupError(detail or f"git {' '.join(arguments)} failed")
    return result


def _git_text(workspace: Path, arguments: Sequence[str]) -> str:
    return _run_git(workspace, arguments).stdout.strip()


def _workspace(path: str | Path) -> Path:
    requested = Path(path).expanduser().resolve()
    if not requested.is_dir():
        raise SetupError(f"Workspace does not exist: {requested}")
    root = Path(_git_text(requested, ["rev-parse", "--show-toplevel"])).resolve()
    if root != requested:
        raise SetupError(f"Use the exact Git worktree root: {root}")
    return root


def _current_ref(workspace: Path) -> str:
    result = _run_git(workspace, ["symbolic-ref", "-q", "HEAD"], check=False)
    reference = result.stdout.strip()
    if result.returncode != 0 or not reference.startswith("refs/heads/"):
        raise SetupError("Checkpoint pushes require an attached local branch")
    return reference


def _remote_push_url(workspace: Path, remote: str) -> str:
    result = _run_git(
        workspace, ["remote", "get-url", "--push", "--all", remote], check=False
    )
    urls = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if result.returncode != 0 or len(urls) != 1:
        raise SetupError(f"Remote {remote} must have exactly one push URL")
    mirror = _run_git(
        workspace, ["config", "--bool", f"remote.{remote}.mirror"], check=False
    )
    if mirror.returncode == 0 and mirror.stdout.strip().lower() == "true":
        raise SetupError(f"Remote {remote} must not be a mirror")
    return urls[0]


def _upstream(workspace: Path) -> str:
    result = _run_git(
        workspace,
        ["rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"],
        check=False,
    )
    value = result.stdout.strip()
    if result.returncode != 0 or not value:
        raise SetupError("The source branch must have a configured upstream")
    return value


def _atomic_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def _load_config(workspace: Path) -> dict[str, Any] | None:
    path = workspace / CONFIG_RELATIVE_PATH
    if not path.is_file():
        return None
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SetupError(f"Invalid workspace configuration: {error}") from error
    if not isinstance(payload, dict) or payload.get("version") != 1:
        raise SetupError("Unsupported workspace configuration")
    return payload


def _guidance_source(workspace: Path, source: str | Path) -> Path:
    path = Path(source).expanduser()
    if not path.is_absolute():
        path = workspace / path
    path = path.resolve()
    if not path.is_file():
        raise SetupError(f"Guidance source is not a readable file: {path}")
    try:
        path.read_bytes()
    except OSError as error:
        raise SetupError(f"Cannot read guidance source {path}: {error}") from error
    return path


def _valid_effective_guidance(value: Any) -> dict[str, Any] | None:
    """Accept only an unchanged explicit audit record; never infer prose equivalence."""

    if not isinstance(value, dict) or value.get("effective") is not True:
        return None
    try:
        source = Path(str(value["source"]))
        digest = str(value["sha256"])
        if source.is_file() and hashlib.sha256(source.read_bytes()).hexdigest() == digest:
            return value
    except (KeyError, OSError):
        pass
    return None


def _reconcile_guidance(
    workspace: Path, source: str | Path | None, existing: Any = None
) -> dict[str, Any]:
    """Record an explicit audited source or install the portable project fallback.

    A source flag is an audit assertion that its rules are effective.  The helper
    records its identity; it does not infer equivalent prose or edit a global file.
    """

    if source is not None:
        path = _guidance_source(workspace, source)
        return {
            "source": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "effective": True,
            "fallback": False,
        }
    retained = _valid_effective_guidance(existing)
    if retained is not None:
        return retained
    return {
        # Runtime assembly owns this one fallback. Setup does not copy it into
        # project AGENTS.md because that would duplicate every injected role context.
        "source": None,
        "sha256": None,
        "effective": False,
        "fallback": True,
    }


def _worker_capabilities(
    requested: Sequence[Sequence[str]], *, required: bool
) -> list[dict[str, str]]:
    if not requested:
        if required:
            raise SetupError("Record successfully probed Luna and Sol capabilities")
        return []
    result: list[dict[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for pair in requested:
        if len(pair) != 2:
            raise SetupError("Worker capabilities use MODEL REASONING_EFFORT")
        model, effort = (str(value).strip() for value in pair)
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._/-]*", model) or not re.fullmatch(
            r"[A-Za-z0-9][A-Za-z0-9._-]*", effort
        ):
            raise SetupError("Worker capabilities use MODEL REASONING_EFFORT")
        efforts = {
            "gpt-6-luna": {"low", "medium", "high", "xhigh", "max"},
            "gpt-6-sol": {"low", "medium", "high", "xhigh", "max", "ultra"},
            "gpt-6-astra": {"low", "medium", "high", "xhigh", "max", "ultra"},
        }
        if model not in efforts or effort not in efforts[model]:
            raise SetupError("Use an available GPT-6 Luna, Sol or Astra model/effort pair")
        key = (model, effort)
        if key in seen:
            raise SetupError(f"Duplicate worker capability: {model}/{effort}")
        seen.add(key)
        result.append({"model": model, "reasoning_effort": effort})
    if required:
        models = {item["model"] for item in result}
        required_models = {"gpt-6-luna", "gpt-6-sol"}
        if not required_models.issubset(models):
            raise SetupError("Record successfully probed Luna and Sol capabilities")
        if len({item["reasoning_effort"] for item in result}) < 2:
            raise SetupError("Prove more than one reasoning effort across the worker roster")
    return result


def _configured_worker_capabilities(config: dict[str, Any]) -> list[dict[str, str]]:
    raw = config.get("worker_capabilities", [])
    if not isinstance(raw, list):
        raise SetupError("Existing worker capability roster is invalid")
    pairs: list[tuple[str, str]] = []
    for item in raw:
        if not isinstance(item, dict):
            raise SetupError("Existing worker capability roster is invalid")
        pairs.append(
            (
                str(item.get("model", "")),
                str(item.get("reasoning_effort", "")),
            )
        )
    try:
        return _worker_capabilities(pairs, required=False)
    except SetupError as error:
        raise SetupError("Existing worker capability roster is invalid") from error


def _require_ignored_state(workspace: Path) -> None:
    result = _run_git(
        workspace,
        ["check-ignore", "-q", "--", STATE_IGNORE_SENTINEL.as_posix()],
        check=False,
    )
    if result.returncode != 0:
        raise SetupError("Ignore the whole .de67/state/ directory before setup")


def _hook_path(workspace: Path) -> Path:
    common_raw = _git_text(workspace, ["rev-parse", "--git-common-dir"])
    common = Path(common_raw)
    if not common.is_absolute():
        common = workspace / common
    expected = Path(os.path.abspath(common / "hooks/post-commit"))
    raw = _git_text(workspace, ["rev-parse", "--git-path", "hooks/post-commit"])
    candidate = Path(raw)
    if not candidate.is_absolute():
        candidate = workspace / candidate
    candidate = Path(os.path.abspath(candidate))
    if os.path.normcase(str(candidate)) != os.path.normcase(str(expected)):
        raise SetupError(
            "Custom core.hooksPath is outside this repository's common Git directory"
        )
    return expected


def _shell_path(path: Path) -> str:
    return shlex.quote(path.as_posix())


def _same_file_content(left: Path, right: Path) -> bool:
    if not left.is_file() or not right.is_file():
        return False
    left_digest = hashlib.sha256(left.read_bytes()).digest()
    right_digest = hashlib.sha256(right.read_bytes()).digest()
    return left_digest == right_digest


def _equivalent_managed_hook(existing: str, interpreter: Path, script: Path) -> bool:
    lines = existing.splitlines()
    if len(lines) != 3 or lines[:2] != ["#!/bin/sh", MANAGED_HOOK_MARKER]:
        return False
    try:
        command = shlex.split(lines[2], posix=True)
    except ValueError:
        return False
    if len(command) != 8 or command[2:] != [
        "push", "--workspace", "$PWD", "--hook", "||", ":"
    ]:
        return False
    existing_interpreter = Path(command[0])
    existing_script = Path(command[1])
    try:
        same_interpreter = existing_interpreter.samefile(interpreter)
    except OSError:
        same_interpreter = False
    return same_interpreter and _same_file_content(existing_script, script)


def _install_hook(workspace: Path) -> Path:
    hook = _hook_path(workspace)
    script = Path(__file__).resolve()
    hook_text = (
        "#!/bin/sh\n"
        f"{MANAGED_HOOK_MARKER}\n"
        f"{_shell_path(Path(sys.executable))} {_shell_path(script)} "
        'push --workspace "$PWD" --hook || :\n'
    )
    if hook.exists():
        existing = hook.read_text(encoding="utf-8", errors="replace")
        if existing != hook_text:
            if _equivalent_managed_hook(existing, Path(sys.executable), script):
                return hook
            kind = (
                "modified managed hook"
                if MANAGED_HOOK_MARKER in existing
                else "unmanaged hook"
            )
            raise SetupError(f"Refusing to overwrite {kind}: {hook}")
    hook.parent.mkdir(parents=True, exist_ok=True)
    with hook.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(hook_text)
    if os.name != "nt":
        hook.chmod(hook.stat().st_mode | 0o111)
    return hook


def _targets(
    workspace: Path, requested: Sequence[Sequence[str]]
) -> tuple[list[dict[str, str]], str]:
    if len(requested) != 1:
        raise SetupError(
            "Exactly one --target REMOTE BRANCH is allowed for automatic pushes; "
            "checkpoint pushes are explicit one-shot operations"
        )
    upstream = _upstream(workspace)
    result: list[dict[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for pair in requested:
        remote, branch = pair
        remote = remote.strip()
        branch = branch.strip()
        if (
            not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._/-]*", remote)
            or not branch
            or branch.startswith("refs/")
        ):
            raise SetupError("Targets use a remote name and short branch name")
        if _run_git(
            workspace,
            ["check-ref-format", f"refs/heads/{branch}"],
            check=False,
        ).returncode != 0:
            raise SetupError(f"Invalid target branch: {branch}")
        key = (remote, branch)
        if key in seen:
            raise SetupError(f"Duplicate checkpoint target: {remote}/{branch}")
        seen.add(key)
        result.append(
            {
                "remote": remote,
                "remote_url": _remote_push_url(workspace, remote),
                "target_ref": f"refs/heads/{branch}",
            }
        )
    first = requested[0]
    if upstream != f"{first[0]}/{first[1]}":
        raise SetupError(
            f"First checkpoint target must match branch upstream {upstream}"
        )
    return result, upstream


def _configured_targets(config: dict[str, Any]) -> list[dict[str, str]]:
    raw_targets = config.get("targets")
    if not isinstance(raw_targets, list) or not raw_targets:
        raise SetupError("Existing workspace configuration has no checkpoint targets")
    result: list[dict[str, str]] = []
    seen: set[tuple[str, str]] = set()
    for raw in raw_targets:
        if not isinstance(raw, dict):
            raise SetupError("Existing workspace checkpoint target is invalid")
        target = {
            "remote": str(raw.get("remote", "")).strip(),
            "remote_url": str(raw.get("remote_url", "")).strip(),
            "target_ref": str(raw.get("target_ref", "")).strip(),
        }
        if (
            not target["remote"]
            or not target["remote_url"]
            or not target["target_ref"].startswith("refs/heads/")
        ):
            raise SetupError("Existing workspace checkpoint target is invalid")
        key = (target["remote"], target["target_ref"])
        if key in seen:
            raise SetupError("Existing workspace checkpoint targets contain a duplicate")
        seen.add(key)
        result.append(target)
    return result


def _merge_existing_binding(
    workspace: Path,
    existing_config: dict[str, Any] | None,
    source_ref: str,
    upstream: str,
    requested_targets: list[dict[str, str]],
    state_path: Path,
) -> tuple[list[dict[str, str]], str | None]:
    if existing_config is None:
        return requested_targets, None
    if Path(str(existing_config.get("workspace", ""))).resolve() != workspace:
        raise SetupError("Existing workspace configuration belongs to another worktree")
    if existing_config.get("source_ref") != source_ref:
        raise SetupError("Existing checkpoint source branch cannot be changed by setup")
    if existing_config.get("upstream") != upstream:
        raise SetupError("Existing checkpoint upstream cannot be changed by setup")

    existing_targets = _configured_targets(existing_config)
    requested_by_key = {
        (target["remote"], target["target_ref"]): target
        for target in requested_targets
    }
    merged = list(existing_targets)
    existing_keys: set[tuple[str, str]] = set()
    for existing in existing_targets:
        key = (existing["remote"], existing["target_ref"])
        existing_keys.add(key)
        requested = requested_by_key.get(key)
        if requested is None:
            raise SetupError("Existing checkpoint targets cannot be removed by setup")
        if requested["remote_url"] != existing["remote_url"]:
            raise SetupError(
                f"Remote {existing['remote']} URL cannot be changed by setup"
            )
    merged.extend(
        target
        for target in requested_targets
        if (target["remote"], target["target_ref"]) not in existing_keys
    )

    raw_clock = existing_config.get("clock")
    if raw_clock is None:
        return merged, None
    if not isinstance(raw_clock, dict):
        raise SetupError("Existing workspace clock configuration is invalid")
    configured_lineage = str(raw_clock.get("lineage", "")).strip()
    configured_state = Path(str(raw_clock.get("state", ""))).resolve()
    if not configured_lineage or configured_state != state_path.resolve():
        raise SetupError("Existing workspace clock configuration is invalid")
    return merged, configured_lineage


def _default_lineage(
    workspace: Path, source_ref: str, primary_remote_url: str
) -> str:
    name = re.split(r"[:/\\]", primary_remote_url.rstrip("/"))[-1].removesuffix(
        ".git"
    )
    name = name or workspace.name
    return f"{name}:{source_ref.removeprefix('refs/heads/')}"


def _existing_lineage(state_path: Path) -> str | None:
    if not state_path.is_file():
        return None
    import sqlite3

    try:
        connection = sqlite3.connect(state_path)
        try:
            table = connection.execute(
                "SELECT 1 FROM sqlite_master "
                "WHERE type = 'table' AND name = 'lineage_binding'"
            ).fetchone()
            if table is None:
                return None
            row = connection.execute(
                "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
            ).fetchone()
        finally:
            connection.close()
    except sqlite3.Error as error:
        raise SetupError(f"Cannot read existing deadline lineage: {error}") from error
    return None if row is None else str(row[0])


def _deadline_harness_class() -> type[Any]:
    path = Path(__file__).resolve().parents[1] / "de-67-3/scripts/deadline_harness.py"
    specification = importlib.util.spec_from_file_location("de67_deadline_harness", path)
    if specification is None or specification.loader is None:
        raise SetupError(f"Cannot load deadline harness: {path}")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module.DeadlineHarness


def _resolve_specification(workspace: Path) -> Any:
    script_root = str(Path(__file__).resolve().parents[1] / "de-67-3/scripts")
    if script_root not in sys.path:
        sys.path.insert(0, script_root)
    from specification import SpecificationError, resolve

    try:
        return resolve(workspace / ".de67")
    except SpecificationError as error:
        raise SetupError(str(error)) from error


def _require_frozen_fs(workspace: Path) -> None:
    specification = _resolve_specification(workspace)
    if re.search(
        r"(?mi)^\s*(?:-\s*)?Status:\s*`?(?:Frozen|Refrozen)\b", specification.text
    ) is None:
        raise SetupError(f"{specification.label} must record Frozen or Refrozen status")


def _prepare_phase3_environment(workspace: Path) -> dict[str, list[str]]:
    """Copy missing Phase-3 policy/state inputs without replacing workspace files."""

    environment = workspace / ".de67"
    environment.mkdir(parents=True, exist_ok=True)
    copied: list[str] = []
    preserved: list[str] = []
    for name in PHASE3_WORKSPACE_FILES:
        source = PHASE3_ENVIRONMENT_ROOT / name
        if not source.is_file():
            raise SetupError(f"Missing packaged Phase-3 environment file: {source}")
        destination = environment / name
        if destination.exists():
            if not destination.is_file():
                raise SetupError(f"Phase-3 environment path is not a file: {destination}")
            preserved.append(name)
            continue
        try:
            with destination.open("xb") as handle:
                handle.write(source.read_bytes())
        except FileExistsError:
            preserved.append(name)
            continue
        except OSError as error:
            raise SetupError(f"Cannot prepare Phase-3 environment file {name}: {error}") from error
        copied.append(name)
    return {"copied": copied, "preserved": preserved}


def _validate_delivery_projection(workspace: Path, state_path: Path) -> None:
    """Exercise the delivery projection on a snapshot, never on authoring state."""
    if not state_path.is_file():
        return
    harness_class = _deadline_harness_class()
    try:
        specification = _resolve_specification(workspace)
        with tempfile.TemporaryDirectory(prefix="de67-refreeze-") as directory:
            environment = Path(directory) / ".de67"
            copied_state = environment / "state" / "deadlines.sqlite3"
            copied_state.parent.mkdir(parents=True)
            names = (specification.path.name, "work-ledger.md")
            for name in names:
                shutil.copy2(workspace / ".de67" / name, environment / name)
            with closing(sqlite3.connect(state_path.as_uri() + "?mode=ro", uri=True)) as source:
                with closing(sqlite3.connect(copied_state)) as destination:
                    source.backup(destination)
            with harness_class(copied_state) as harness:
                if harness.connection.execute("SELECT 1 FROM claim_acceptances LIMIT 1").fetchone():
                    harness.synchronize_delivery_statuses()
    except Exception as error:
        raise SetupError(f"FS cannot project durable acceptance: {error}") from error


def configure(
    workspace_path: str | Path,
    requested_targets: Sequence[Sequence[str]],
    *,
    bind_clock: bool,
    lineage: str | None = None,
    worker_capabilities: Sequence[Sequence[str]] = (),
    guidance_source: str | Path | None = None,
) -> dict[str, Any]:
    workspace = _workspace(workspace_path)
    _require_ignored_state(workspace)
    source_ref = _current_ref(workspace)
    targets, upstream = _targets(workspace, requested_targets)
    state_path = workspace / DEADLINE_STATE_RELATIVE_PATH
    existing_config = _load_config(workspace)
    targets, configured_lineage = _merge_existing_binding(
        workspace,
        existing_config,
        source_ref,
        upstream,
        targets,
        state_path,
    )
    if bind_clock:
        configured_workers = _worker_capabilities(
            worker_capabilities, required=True
        )
    else:
        if worker_capabilities:
            raise SetupError("Worker capabilities are recorded only while binding the clock")
        configured_workers = (
            []
            if existing_config is None
            else _configured_worker_capabilities(existing_config)
        )
    selected_lineage: str | None = None
    if bind_clock:
        _require_frozen_fs(workspace)
        phase3_environment = _prepare_phase3_environment(workspace)
        guidance = _reconcile_guidance(
            workspace, guidance_source, (existing_config or {}).get("guidance")
        )
        _validate_delivery_projection(workspace, state_path)
        requested_lineage = None if lineage is None else lineage.strip()
        if lineage is not None and not requested_lineage:
            raise SetupError("Lineage must not be empty")
        bound_lineage = _existing_lineage(state_path)
        known_lineages = {
            value
            for value in (configured_lineage, bound_lineage, requested_lineage)
            if value is not None
        }
        if len(known_lineages) > 1:
            raise SetupError("Existing clock lineage cannot be changed by setup")
        selected_lineage = next(iter(known_lineages), None) or _default_lineage(
            workspace, source_ref, targets[0]["remote_url"]
        )
        harness_class = _deadline_harness_class()
        try:
            with harness_class(state_path) as harness:
                harness.coordinator_restart_status(selected_lineage)
        except Exception as error:
            raise SetupError(f"Cannot bind deadline clock: {error}") from error
    else:
        if guidance_source is not None:
            raise SetupError("Guidance reconciliation is available only during Phase-2 setup")
        selected_lineage = configured_lineage
        phase3_environment = {"copied": [], "preserved": []}
        guidance = (existing_config or {}).get("guidance")
    hook = _install_hook(workspace)
    config = dict(existing_config or {})
    config.update({
        "version": 1,
        "workspace": str(workspace),
        "source_ref": source_ref,
        "upstream": upstream,
        "targets": targets,
        "worker_capabilities": configured_workers,
        "clock": (
            None
            if selected_lineage is None
            else {"lineage": selected_lineage, "state": str(state_path)}
        ),
    })
    if guidance is not None:
        config["guidance"] = guidance
    _atomic_json(workspace / CONFIG_RELATIVE_PATH, config)
    pushed = push_checkpoints(workspace)
    return {
        "workspace": str(workspace),
        "hook": str(hook),
        "clock": config["clock"],
        "phase3_environment": phase3_environment,
        "guidance": guidance,
        "push": pushed,
    }


def _concise_error(result: subprocess.CompletedProcess[str]) -> str:
    lines = [
        line.strip()
        for line in (result.stderr or result.stdout).splitlines()
        if line.strip()
    ]
    return lines[-1][:500] if lines else f"git push exited {result.returncode}"


def push_checkpoints(workspace_path: str | Path) -> dict[str, Any]:
    workspace = _workspace(workspace_path)
    head: str | None = None
    source_ref: str | None = None
    outcomes: list[dict[str, Any]] = []
    try:
        config = _load_config(workspace)
        if config is None:
            return {"configured": False, "ok": True}
        head = _git_text(workspace, ["rev-parse", "HEAD"])
        if Path(str(config.get("workspace", ""))).resolve() != workspace:
            raise SetupError("Workspace configuration belongs to another worktree")
        source_ref = _current_ref(workspace)
        if source_ref != config.get("source_ref"):
            raise SetupError(
                f"Configured source is {config.get('source_ref')}; "
                f"current source is {source_ref}"
            )
        if _upstream(workspace) != config.get("upstream"):
            raise SetupError("Branch upstream changed after checkpoint setup")
        environment = os.environ.copy()
        environment.update(
            {"GIT_TERMINAL_PROMPT": "0", "GCM_INTERACTIVE": "Never"}
        )
        raw_targets = config.get("targets")
        if not isinstance(raw_targets, list) or not raw_targets:
            raise SetupError("Workspace configuration has no checkpoint targets")
        for target in raw_targets:
            if not isinstance(target, dict):
                raise SetupError("Workspace checkpoint target is invalid")
            remote = str(target.get("remote", ""))
            target_ref = str(target.get("target_ref", ""))
            expected_url = str(target.get("remote_url", ""))
            try:
                if _remote_push_url(workspace, remote) != expected_url:
                    raise SetupError(f"Remote {remote} push URL changed after setup")
                result = _run_git(
                    workspace,
                    [
                        "push",
                        "--porcelain",
                        "--no-force",
                        "--no-follow-tags",
                        "--recurse-submodules=no",
                        "--",
                        remote,
                        f"{head}:{target_ref}",
                    ],
                    check=False,
                    environment=environment,
                )
                if result.returncode != 0:
                    outcomes.append(
                        {
                            "remote": remote,
                            "target_ref": target_ref,
                            "ok": False,
                            "error": _concise_error(result),
                        }
                    )
                else:
                    outcomes.append(
                        {"remote": remote, "target_ref": target_ref, "ok": True}
                    )
            except SetupError as error:
                outcomes.append(
                    {
                        "remote": remote,
                        "target_ref": target_ref,
                        "ok": False,
                        "error": str(error),
                    }
                )
        status = {
            "head": head,
            "source_ref": source_ref,
            "recorded_at": time.time(),
            "ok": bool(outcomes) and all(item["ok"] for item in outcomes),
            "targets": outcomes,
        }
        _atomic_json(workspace / PUSH_STATUS_RELATIVE_PATH, status)
        return {"configured": True, **status}
    except (SetupError, OSError, subprocess.SubprocessError) as error:
        status = {
            "head": head,
            "source_ref": source_ref,
            "recorded_at": time.time(),
            "ok": False,
            "error": str(error),
            "targets": outcomes,
        }
        try:
            _atomic_json(workspace / PUSH_STATUS_RELATIVE_PATH, status)
        except OSError:
            pass
        raise


def disable(workspace_path: str | Path) -> dict[str, Any]:
    workspace = _workspace(workspace_path)
    config_path = workspace / CONFIG_RELATIVE_PATH
    removed = config_path.exists()
    if removed:
        config_path.unlink()
    return {"workspace": str(workspace), "disabled": removed}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name, help_text in (
        ("setup", "Bind the clock and configure checkpoint pushes"),
        ("configure-push", "Configure checkpoint pushes without a project clock"),
    ):
        command = commands.add_parser(name, help=help_text)
        command.add_argument("--workspace", required=True)
        command.add_argument(
            "--target", nargs=2, action="append", metavar=("REMOTE", "BRANCH"), required=True
        )
        if name == "setup":
            command.add_argument("--lineage")
            command.add_argument(
                "--guidance-source",
                help="Audited effective guidance source; setup records it without editing it",
            )
            command.add_argument(
                "--worker-capability",
                nargs=2,
                action="append",
                metavar=("MODEL", "REASONING_EFFORT"),
                required=True,
                help="Record one model/effort pair that a fresh native spawn probe passed",
            )
    push = commands.add_parser("push", help="Push the exact configured committed HEAD")
    push.add_argument("--workspace", required=True)
    push.add_argument("--hook", action="store_true", help=argparse.SUPPRESS)
    off = commands.add_parser("disable", help="Disable pushes for one worktree")
    off.add_argument("--workspace", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.command == "setup":
            result = configure(
                arguments.workspace,
                arguments.target,
                bind_clock=True,
                lineage=arguments.lineage,
                worker_capabilities=arguments.worker_capability,
                guidance_source=arguments.guidance_source,
            )
        elif arguments.command == "configure-push":
            result = configure(
                arguments.workspace, arguments.target, bind_clock=False
            )
        elif arguments.command == "disable":
            result = disable(arguments.workspace)
        else:
            result = push_checkpoints(arguments.workspace)
            if result.get("configured") and not result.get("ok"):
                failed = next(
                    (
                        str(item.get("error", "")).strip()
                        for item in result.get("targets", [])
                        if not item.get("ok")
                    ),
                    "",
                )
                raise SetupError(failed or "one or more checkpoint pushes failed")
        if not getattr(arguments, "hook", False):
            print(json.dumps(result, sort_keys=True))
        if arguments.command in {"setup", "configure-push"}:
            return 0 if result["push"]["ok"] else 1
        return 0
    except (SetupError, OSError, subprocess.SubprocessError) as error:
        print(f"DE-67 workspace setup: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
