#!/usr/bin/env python3
"""Control one terminal-independent DE67 supervisor session on macOS."""
from __future__ import annotations
import argparse, fcntl, hashlib, json, os, shlex, shutil, signal, subprocess, sys, uuid
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path

from deadline_harness import DeadlineHarness
from policy_kernel import compile_policy, guard_policy_candidate, load_contracts

class ServiceError(RuntimeError): pass

@dataclass(frozen=True)
class ServiceSpec:
    label: str
    workspace: Path
    log_directory: Path
    tmux: str
    shell_command: str

def service_identity(workspace_value: str | Path) -> ServiceSpec:
    workspace = Path(workspace_value).expanduser().resolve()
    digest = hashlib.sha256(str(workspace).encode()).hexdigest()[:16]
    return ServiceSpec(f"de67-{digest}", workspace,
                       workspace / ".de67/state/supervisor-service", "", "")

def _workspace_config(workspace: Path) -> tuple[Path, str]:
    path = workspace / ".de67/state/workspace.json"
    try: payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ServiceError(f"Unreadable DE67 workspace configuration: {error}") from error
    clock = payload.get("clock")
    if payload.get("workspace") != str(workspace) or not isinstance(clock, dict):
        raise ServiceError("DE67 workspace configuration does not match the requested workspace")
    state_value, lineage = clock.get("state"), clock.get("lineage")
    if not isinstance(state_value, str) or not isinstance(lineage, str) or not lineage:
        raise ServiceError("DE67 workspace configuration lacks its clock state or lineage")
    state = Path(state_value).expanduser().resolve()
    if not state.is_file(): raise ServiceError(f"DE67 deadline state is missing: {state}")
    return state, lineage

def _executable(variable: str, fallback: str) -> str:
    requested = os.environ.get(variable, fallback).strip()
    if not requested: raise ServiceError(f"{variable} must not be empty")
    resolved = shutil.which(requested)
    if resolved is None: raise ServiceError(f"Executable was not found: {requested}")
    return str(Path(resolved).resolve())

def service_spec(workspace_value: str | Path) -> ServiceSpec:
    if sys.platform != "darwin": raise ServiceError("Durable DE67 launch currently requires macOS")
    identity = service_identity(workspace_value)
    state, lineage = _workspace_config(identity.workspace)
    codex, tmux = _executable("DE67_CODEX", "codex"), _executable("DE67_TMUX", "tmux")
    scripts, python = Path(__file__).resolve().parent, str(Path(sys.executable).resolve())
    # Owner policy: unattended Phase 3 must not inherit a sandbox restriction
    # that prevents its coordinators, workers, or mutation reviewer from editing.
    arguments = ["/usr/bin/env", f"DE67_CODEX={codex}",
                 "DE67_COORDINATOR_SANDBOX=danger-full-access",
                 f"DE67_SUPERVISOR_START_TOKEN={uuid.uuid4().hex}",
                 f"PATH={os.environ.get('PATH', '/usr/bin:/bin:/usr/sbin:/sbin')}"]
    codex_state = os.environ.get("DE67_CODEX_STATE", "").strip()
    if codex_state: arguments.append(f"DE67_CODEX_STATE={Path(codex_state).expanduser().resolve()}")
    arguments += [python, str(scripts / "coordinator_supervisor.py"),
                  "--state", str(state), "--lineage", lineage,
                  "--workspace", str(identity.workspace), "--run-root",
                  str(identity.workspace / ".de67/state/coordinator-runs"),
                  "--coordinator-model", "gpt-5.6-sol",
                  "--coordinator-reasoning-effort", "low", "--runner",
                  python, str(scripts / "codex_runner.py")]
    stdout, stderr = identity.log_directory / "stdout.log", identity.log_directory / "stderr.log"
    command = f"exec {shlex.join(arguments)} >> {shlex.quote(str(stdout))} 2>> {shlex.quote(str(stderr))}"
    return ServiceSpec(identity.label, identity.workspace, identity.log_directory, tmux, command)

def _tmux(spec: ServiceSpec, *args: str) -> subprocess.CompletedProcess[str]:
    socket = _socket_path(spec)
    socket.parent.mkdir(parents=True, exist_ok=True)
    return subprocess.run([spec.tmux or _executable("DE67_TMUX", "tmux"),
                           "-S", str(socket), *args],
                          text=True, capture_output=True, check=False)

def _socket_path(spec: ServiceSpec) -> Path:
    return Path.home() / ".codex/state/de67-tmux" / f"{spec.label}.sock"

def _legacy_details(spec: ServiceSpec) -> tuple[str, str, Path]:
    digest = spec.label.removeprefix("de67-")
    label = f"local.de67.supervisor.{digest}"
    return label, f"gui/{os.getuid()}/{label}", Path.home() / "Library/LaunchAgents" / f"{label}.plist"

def _legacy_loaded(spec: ServiceSpec) -> bool:
    _label, target, _plist = _legacy_details(spec)
    result = subprocess.run(["/bin/launchctl", "print", target], text=True,
                            capture_output=True, check=False)
    if result.returncode == 0: return True
    if result.returncode == 113: return False
    raise ServiceError(result.stderr.strip() or "legacy launchd service query failed")

def _remove_legacy(spec: ServiceSpec) -> None:
    _label, target, plist = _legacy_details(spec)
    if _legacy_loaded(spec):
        result = subprocess.run(["/bin/launchctl", "bootout", target], text=True,
                                capture_output=True, check=False)
        if result.returncode: raise ServiceError(result.stderr.strip() or "legacy launchd bootout failed")
    plist.unlink(missing_ok=True)

def _absent(result: subprocess.CompletedProcess[str]) -> bool:
    error = result.stderr.lower()
    return result.returncode == 1 and (
        "can't find session" in error
        or "no server running" in error
        or "server exited unexpectedly" in error
        or ("error connecting to" in error and "no such file or directory" in error)
    )

def _normalize_explicit_start(
    state: Path,
    lineage: str,
    *,
    now: float | None = None,
) -> dict[str, object]:
    with DeadlineHarness(state) as harness:
        return harness.normalize_external_supervisor_start(lineage, now=now)

def _sync_installed_policy(workspace: Path) -> None:
    """Deploy one internally consistent installed policy set before restart."""
    source = Path(__file__).resolve().parents[1] / "assets/environment"
    candidate = json.loads((source / "phase3-policy.json").read_text(encoding="utf-8"))
    contracts = load_contracts(source / "phase3-contracts.json")
    compiled = compile_policy(guard_policy_candidate(candidate, contracts))
    packaged = (source / "phase3-policy.d67").read_bytes()
    if compiled != packaged:
        raise ServiceError("Installed Phase-3 policy source, contracts, and bytecode disagree")
    destination = workspace / ".de67"
    destination.mkdir(parents=True, exist_ok=True)
    for name in ("phase3-policy.json", "phase3-contracts.json", "phase3-policy.d67"):
        target = destination / name
        temporary = target.with_name(f".{target.name}.{uuid.uuid4().hex}.tmp")
        try:
            shutil.copy2(source / name, temporary)
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)

@contextmanager
def _lock(spec: ServiceSpec):
    spec.log_directory.mkdir(parents=True, exist_ok=True)
    with (spec.log_directory / "control.lock").open("a+") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        try: yield
        finally: fcntl.flock(lock.fileno(), fcntl.LOCK_UN)

def _running(spec: ServiceSpec) -> bool:
    result = _tmux(spec, "has-session", "-t", f"={spec.label}")
    if result.returncode == 0: return True
    if _absent(result): return False
    raise ServiceError(result.stderr.strip() or "tmux session query failed")

def start_service(workspace: str | Path) -> ServiceSpec:
    spec = service_spec(workspace)
    with _lock(spec):
        if _legacy_loaded(spec):
            raise ServiceError("A legacy DE67 supervisor is still loaded; run stop, then start")
        _remove_legacy(spec)
        if _running(spec): raise ServiceError(f"DE67 supervisor is already running: {spec.label}")
        gate = f"{spec.label}-start-{uuid.uuid4().hex}"
        gated_command = (
            shlex.join([spec.tmux, "-S", str(_socket_path(spec)), "wait-for", gate])
            + " && " + spec.shell_command
        )
        result = _tmux(spec, "new-session", "-d", "-s", spec.label,
                       "-c", str(spec.workspace), gated_command)
        if result.returncode: raise ServiceError(result.stderr.strip() or "tmux new-session failed")
        if not _running(spec):
            raise ServiceError(f"DE67 supervisor exited during startup; inspect {spec.log_directory / 'stderr.log'}")
        try:
            state, lineage = _workspace_config(spec.workspace)
            _sync_installed_policy(spec.workspace)
            _normalize_explicit_start(state, lineage)
        except Exception as error:
            _tmux(spec, "kill-session", "-t", f"={spec.label}")
            raise ServiceError(f"DE67 restart normalization failed: {error}") from error
        released = _tmux(spec, "wait-for", "-S", gate)
        if released.returncode:
            _tmux(spec, "kill-session", "-t", f"={spec.label}")
            raise ServiceError(released.stderr.strip() or "tmux start gate failed")
        if not _running(spec):
            raise ServiceError(f"DE67 supervisor exited during startup; inspect {spec.log_directory / 'stderr.log'}")
    return spec

def _control_spec(workspace: str | Path) -> ServiceSpec:
    identity = service_identity(workspace)
    return ServiceSpec(identity.label, identity.workspace, identity.log_directory,
                       _executable("DE67_TMUX", "tmux"), "")

def _terminate_session_process_groups(spec: ServiceSpec) -> tuple[int, ...]:
    result = _tmux(spec, "list-panes", "-t", f"={spec.label}", "-F", "#{pane_pid}")
    if result.returncode:
        raise ServiceError(result.stderr.strip() or "tmux could not enumerate supervisor panes")
    own_group = os.getpgrp()
    groups: list[int] = []
    for value in result.stdout.splitlines():
        try:
            group = int(value.strip())
        except ValueError as error:
            raise ServiceError(f"tmux returned an invalid supervisor process id: {value!r}") from error
        if group <= 0 or group == own_group:
            raise ServiceError(f"Refusing to signal unsafe supervisor process group: {group}")
        groups.append(group)
        try:
            os.killpg(group, signal.SIGTERM)
        except ProcessLookupError:
            pass
        except PermissionError as error:
            raise ServiceError(f"Cannot terminate supervisor process group: {group}") from error
    return tuple(groups)

def _kill_surviving_process_groups(groups: tuple[int, ...]) -> None:
    for group in groups:
        try:
            os.killpg(group, 0)
        except ProcessLookupError:
            continue
        except PermissionError:
            # The exact tmux-owned group is gone. A now-inaccessible group with
            # the same numeric id is not ours to signal.
            continue
        try:
            os.killpg(group, signal.SIGKILL)
        except ProcessLookupError:
            continue
        except PermissionError as error:
            raise ServiceError(f"Cannot kill supervisor process group: {group}") from error

def _capture_coordinator_runtime(workspace: Path):
    """Retain ancestry before tmux termination reparents server descendants."""
    address = workspace / ".de67/state/coordinator-input.json"
    try:
        binding = json.loads(address.read_text(encoding="utf-8"))
        socket = Path(binding["socket"])
        root = Path(os.environ.get("CODEX_HOME") or Path.home() / ".codex") / "state/de67-input"
        if (binding.get("workspace") != str(workspace)
                or socket.parent.resolve() != root.resolve()
                or len(socket.stem) != 16
                or any(char not in "0123456789abcdef" for char in socket.stem)
                or socket.suffix != ".sock"):
            return None
        runner_pid = int(binding["runner_pid"])
    except (OSError, ValueError, KeyError, TypeError):
        return None
    from codex_app_server_runner import process_snapshot
    return runner_pid, socket, process_snapshot()


def _cleanup_coordinator_runtime(workspace: Path, captured) -> None:
    if captured is None:
        return
    from types import SimpleNamespace
    from codex_app_server_runner import stop_owned_runtime
    runner_pid, socket, rows = captured
    # Match the unique server socket, never trust a potentially recycled PID.
    process = SimpleNamespace(pid=runner_pid, poll=lambda: 0)
    stop_owned_runtime(process, workspace, workspace, {},
                       captured_rows=rows, owned_socket=socket)


def stop_service(workspace: str | Path) -> ServiceSpec:
    identity = service_identity(workspace)
    with _lock(identity):
        _remove_legacy(identity)
        spec = _control_spec(workspace)
        captured = _capture_coordinator_runtime(spec.workspace)
        if _running(spec):
            groups = _terminate_session_process_groups(spec)
            result = _tmux(spec, "kill-session", "-t", f"={spec.label}")
            _kill_surviving_process_groups(groups)
            if result.returncode and not _absent(result):
                raise ServiceError(result.stderr.strip() or "tmux kill-session failed")
        _cleanup_coordinator_runtime(spec.workspace, captured)
    return spec

def status_service(workspace: str | Path) -> tuple[ServiceSpec, str]:
    identity = service_identity(workspace)
    if _legacy_loaded(identity): return identity, "legacy-launchagent-loaded"
    spec = _control_spec(workspace)
    if not _running(spec): return spec, "stopped"
    result = _tmux(spec, "display-message", "-p", "-t", f"={spec.label}",
                   "pid=#{pane_pid} created=#{session_created} windows=#{session_windows}")
    if result.returncode: raise ServiceError(result.stderr.strip() or "tmux status failed")
    return spec, result.stdout.strip()

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("start", "status", "stop")); parser.add_argument("--workspace", required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "start": spec, status = start_service(args.workspace), "started"
        elif args.command == "stop": spec, status = stop_service(args.workspace), "stopped"
        else:
            spec, status = status_service(args.workspace)
            print(json.dumps({"label": spec.label, "service": status}, sort_keys=True)); return 0
        print(json.dumps({"label": spec.label, "status": status, "logs": str(spec.log_directory)}, sort_keys=True)); return 0
    except ServiceError as error:
        print(f"de67 supervisor service: {error}", file=sys.stderr); return 1

if __name__ == "__main__": raise SystemExit(main())
