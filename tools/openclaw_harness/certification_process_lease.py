"""Exclusive, identity-checked process leases for certification rounds.

The registry keeps immutable round manifests and append-only lease facts.  This
module derives the current lease state inside an IMMEDIATE SQLite transaction
and is the sole owner allowed to signal a certification process.
"""

from __future__ import annotations

import hashlib
import json
import os
from dataclasses import dataclass
from pathlib import Path
import signal
import subprocess
from typing import Any, Dict, Mapping, Optional, Protocol

from identity_binding import RoundManifestError, _validate_round_manifest, canonical_digest
from scenario_registry_store import ScenarioRegistryStoreError, register_certification_round


class CertificationLeaseError(RuntimeError):
    """A certification process lease could not be acquired or used safely."""


class CertificationLeaseConflict(CertificationLeaseError):
    """A live lease already owns the requested world or process."""


@dataclass(frozen=True)
class ProcessSnapshot:
    pid: int
    alive: bool
    executable_path: str = ""
    birth_identity: str = ""
    command: str = ""


class ProcessInspector(Protocol):
    def inspect(self, pid: int) -> ProcessSnapshot:
        """Return one current process observation without sending a signal."""

    def signal(self, pid: int, sig: int) -> None:
        """Signal the exact process that was just inspected."""


class SystemProcessInspector:
    """POSIX inspector using a start identity, executable path, and command line."""

    def inspect(self, pid: int) -> ProcessSnapshot:
        if pid <= 0:
            return ProcessSnapshot(pid=pid, alive=False)
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return ProcessSnapshot(pid=pid, alive=False)
        except PermissionError:
            pass

        proc_root = Path("/proc") / str(pid)
        if proc_root.is_dir():
            try:
                stat_fields = (proc_root / "stat").read_text(encoding="utf-8").split()
                birth = "linux-start:" + stat_fields[21]
                executable = str((proc_root / "exe").resolve())
                raw_command = (proc_root / "cmdline").read_bytes()
                command = raw_command.replace(b"\0", b" ").decode("utf-8", errors="replace").strip()
                return ProcessSnapshot(pid=pid, alive=True, executable_path=executable,
                                       birth_identity=birth, command=command)
            except (IndexError, OSError):
                return ProcessSnapshot(pid=pid, alive=True)

        try:
            result = subprocess.run(
                ["ps", "-p", str(pid), "-o", "lstart=", "-o", "command="],
                capture_output=True, text=True, check=False, timeout=2.0,
            )
        except (OSError, subprocess.TimeoutExpired):
            return ProcessSnapshot(pid=pid, alive=True)
        line = result.stdout.strip()
        if not line:
            return ProcessSnapshot(pid=pid, alive=False)
        # lstart is the fixed-width ctime format.  command begins immediately
        # after its 24 characters; its first token is the executable path.
        birth = "posix-lstart:" + line[:24].strip()
        command = line[24:].strip()
        executable = command.split()[0] if command else ""
        return ProcessSnapshot(pid=pid, alive=True, executable_path=executable,
                               birth_identity=birth, command=command)

    def signal(self, pid: int, sig: int) -> None:
        os.kill(pid, sig)


def _json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with Path(path).resolve().open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _canonical_path(path: str) -> str:
    if not path:
        return ""
    try:
        return str(Path(path).resolve())
    except OSError:
        return str(Path(path))


def _process_identity(snapshot: ProcessSnapshot, executable_sha256: str) -> str:
    return canonical_digest({
        "pid": snapshot.pid,
        "birth_identity": snapshot.birth_identity,
        "executable_path": _canonical_path(snapshot.executable_path),
        "executable_sha256": executable_sha256,
    }, domain="caol-certification-process-lease:v1")


def world_identity_for_manifest(manifest: Mapping[str, Any]) -> str:
    """Return the sealed world/save identity; no caller-supplied world aliases it."""
    _validate_manifest(manifest)
    world = manifest["binding"]["authoritative_components"]["world_save"]
    return canonical_digest(world, domain="caol-certification-world-lease:v1")


def _validate_manifest(manifest: Mapping[str, Any]) -> None:
    try:
        _validate_round_manifest(manifest)
    except (RoundManifestError, TypeError, ValueError) as exc:
        raise CertificationLeaseError(f"sealed certification manifest is invalid: {exc}") from exc


def load_certification_manifest(path: str) -> Mapping[str, Any]:
    """Load an explicit, already-sealed round manifest from disk."""
    manifest_path = Path(path).expanduser().resolve()
    try:
        value = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CertificationLeaseError(f"certification round manifest is unreadable: {manifest_path}: {exc}") from exc
    _validate_manifest(value)
    return value


def _lease_rows(connection: Any) -> list[Any]:
    return list(connection.execute(
        "SELECT lease_event_id,round_id,lease_id,event_sequence,event_kind,process_identity,world_identity,details_json "
        "FROM certification_round_lease_history ORDER BY lease_event_id"
    ))


def _current_leases(connection: Any) -> list[Dict[str, Any]]:
    current: Dict[tuple[str, str], Dict[str, Any]] = {}
    for row in _lease_rows(connection):
        try:
            details = json.loads(str(row["details_json"]))
        except json.JSONDecodeError as exc:
            raise CertificationLeaseError("certification lease history contains invalid JSON") from exc
        if not isinstance(details, dict):
            raise CertificationLeaseError("certification lease history details are not an object")
        item = dict(details)
        item.update({
            "round_id": str(row["round_id"]), "lease_id": str(row["lease_id"]),
            "event_sequence": int(row["event_sequence"]), "event_kind": str(row["event_kind"]),
            "process_identity": str(row["process_identity"]), "world_identity": str(row["world_identity"]),
        })
        current[(item["round_id"], item["lease_id"])] = item
    return list(current.values())


def _append(connection: Any, *, record: Mapping[str, Any], event_kind: str,
            state: str, extra: Optional[Mapping[str, Any]] = None) -> Dict[str, Any]:
    round_id = str(record["round_id"])
    lease_id = str(record["lease_id"])
    prior = connection.execute(
        "SELECT COALESCE(MAX(event_sequence), 0) FROM certification_round_lease_history "
        "WHERE round_id = ? AND lease_id = ?", (round_id, lease_id),
    ).fetchone()[0]
    details = dict(record)
    details["state"] = state
    if extra:
        details.update(extra)
    process_identity = str(record["process_identity"])
    connection.execute(
        "INSERT INTO certification_round_lease_history( round_id,lease_id,event_sequence,event_kind,process_identity,world_identity,details_json ) "
        "VALUES(?,?,?,?,?,?,?)",
        (round_id, lease_id, int(prior) + 1, event_kind, process_identity,
         str(record["world_identity"]), _json(details)),
    )
    return details | {"event_sequence": int(prior) + 1, "event_kind": event_kind}


def _record_for_snapshot(*, manifest: Mapping[str, Any], lease_id: str,
                         executable_path: str, executable_sha256: str,
                         world_identity: str, snapshot: ProcessSnapshot,
                         state: str, cleanup_token: str = "") -> Dict[str, Any]:
    executable = _canonical_path(executable_path)
    if not lease_id.strip() or not executable or len(executable_sha256) != 64:
        raise CertificationLeaseError("lease ID, executable path, and executable SHA-256 are required")
    return {
        "schema": 1,
        "round_id": str(manifest["round_id"]),
        "lease_id": lease_id,
        "world_identity": world_identity,
        "executable_path": executable,
        "executable_sha256": executable_sha256.lower(),
        "pid": snapshot.pid,
        "process_birth_identity": snapshot.birth_identity,
        "command": snapshot.command,
        "process_identity": _process_identity(snapshot, executable_sha256.lower()),
        "state": state,
        "cleanup_token": cleanup_token,
    }


def _matches_live_owner(record: Mapping[str, Any], inspector: ProcessInspector) -> bool:
    if record.get("state") not in {"active", "termination_requested"}:
        return False
    snapshot = inspector.inspect(int(record.get("pid", 0) or 0))
    if not snapshot.alive:
        return False
    if snapshot.birth_identity != record.get("process_birth_identity"):
        return False
    if _canonical_path(snapshot.executable_path) != record.get("executable_path"):
        return False
    try:
        if _sha256_file(snapshot.executable_path) != record.get("executable_sha256"):
            return False
    except OSError:
        return False
    command = snapshot.command
    executable = str(record.get("executable_path", ""))
    return bool(command and (executable in command or Path(executable).name in command))


def _recover_or_reject_conflicts(connection: Any, *, candidate: Mapping[str, Any],
                                 inspector: ProcessInspector) -> None:
    for current in _current_leases(connection):
        if current.get("state") not in {"active", "termination_requested", "quarantined"}:
            continue
        same_lease = (current["round_id"], current["lease_id"]) == (
            candidate["round_id"], candidate["lease_id"])
        same_world = current.get("world_identity") == candidate.get("world_identity")
        same_process = (
            int(current.get("pid", 0) or 0) == int(candidate.get("pid", 0) or 0)
            and current.get("process_birth_identity") == candidate.get("process_birth_identity")
        )
        if same_lease or not (same_world or same_process):
            continue
        if current.get("state") == "quarantined":
            raise CertificationLeaseConflict(
                f"quarantined certification lease {current['lease_id']} in round {current['round_id']} owns "
                + ("the requested world" if same_world else "the requested process")
            )
        if _matches_live_owner(current, inspector):
            raise CertificationLeaseConflict(
                f"live certification lease {current['lease_id']} in round {current['round_id']} owns "
                + ("the requested world" if same_world else "the requested process")
            )
        _append(connection, record=current, event_kind="stale_recovered", state="stale_recovered",
                extra={"recovery_reason": "stored_owner_not_live_or_identity_mismatch"})


def quarantine_released_live_lease(connection: Any, *, manifest: Mapping[str, Any],
                                   lease_id: str, reason: str) -> Dict[str, Any]:
    """Fence a historically released lease whose process must never be reused.

    A released row normally has no live owner.  If later external observation
    proves its PID still exists but its executable bytes have drifted, this
    registry cannot authenticate or signal that process safely.  Preserve the
    original immutable receipt and append a no-signal quarantine instead.
    """
    _validate_manifest(manifest)
    if not reason.strip():
        raise CertificationLeaseError("quarantine reason is required")
    try:
        connection.execute("BEGIN IMMEDIATE")
        current = next((item for item in _current_leases(connection)
                        if item["round_id"] == manifest["round_id"] and item["lease_id"] == lease_id), None)
        if current is None:
            connection.execute("COMMIT")
            return {"status": "rejected_lease_unknown"}
        if current.get("state") == "quarantined":
            connection.execute("COMMIT")
            return {"status": "already_quarantined", "lease": current, "idempotent": True}
        if current.get("state") != "released":
            connection.execute("COMMIT")
            return {"status": "rejected_lease_not_released"}
        quarantined = _append(
            connection, record=current, event_kind="quarantined_released_live_owner",
            state="quarantined", extra={"quarantine_reason": reason},
        )
        connection.execute("COMMIT")
        return {"status": "quarantined", "lease": quarantined}
    except BaseException:
        if connection.in_transaction:
            connection.execute("ROLLBACK")
        raise


def reserve_certification_lease(connection: Any, *, manifest: Mapping[str, Any], lease_id: str,
                                executable_path: str, executable_sha256: str,
                                inspector: ProcessInspector, cleanup_token: str = "") -> Dict[str, Any]:
    """Atomically reserve the sealed world before startup; it never matches processes globally."""
    _validate_manifest(manifest)
    register_certification_round(connection, manifest)
    world_identity = world_identity_for_manifest(manifest)
    reserved = _record_for_snapshot(
        manifest=manifest, lease_id=lease_id, executable_path=executable_path,
        executable_sha256=executable_sha256, world_identity=world_identity,
        snapshot=ProcessSnapshot(pid=0, alive=False, birth_identity="not-started"), state="reserved",
        cleanup_token=cleanup_token,
    )
    try:
        connection.execute("BEGIN IMMEDIATE")
        current = next((item for item in _current_leases(connection)
                        if item["round_id"] == reserved["round_id"] and item["lease_id"] == lease_id), None)
        if current is not None:
            if current.get("state") == "active":
                # A normal relaunch reaches this owner after the bound process
                # has exited.  Transition that exact same-round receipt back
                # to reserved atomically; never infer it from a matching path.
                snapshot = inspector.inspect(int(current.get("pid", 0) or 0))
                if not snapshot.alive:
                    transferable = _append(
                        connection, record=current,
                        event_kind="exited_reserved_for_transfer", state="reserved",
                        extra={"previous_pid": current.get("pid", 0),
                               "previous_process_birth_identity": current.get("process_birth_identity", "")},
                    )
                    connection.execute("COMMIT")
                    return transferable | {"idempotent": False, "relaunch_transfer": True}
                if not _matches_live_owner(current, inspector):
                    raise CertificationLeaseError(
                        "active lease PID was reused or replaced; it is not transferable"
                    )
                raise CertificationLeaseConflict(
                    "live same-round lease must exit before a relaunch reservation"
                )
            expected = {key: reserved[key] for key in (
                "round_id", "lease_id", "world_identity", "executable_path", "executable_sha256", "pid",
                "process_birth_identity", "command", "process_identity", "state", "cleanup_token")}
            observed = {key: current.get(key) for key in expected}
            if observed == expected:
                connection.execute("COMMIT")
                return current | {"idempotent": True}
            raise CertificationLeaseError("lease ID already has different immutable reservation facts")
        _recover_or_reject_conflicts(connection, candidate=reserved, inspector=inspector)
        appended = _append(connection, record=reserved, event_kind="reserved", state="reserved")
        connection.execute("COMMIT")
        return appended | {"idempotent": False}
    except BaseException:
        if connection.in_transaction:
            connection.execute("ROLLBACK")
        raise


def transfer_certification_lease(connection: Any, *, manifest: Mapping[str, Any], lease_id: str,
                                 pid: int, executable_path: str, executable_sha256: str,
                                 inspector: ProcessInspector, cleanup_token: str = "") -> Dict[str, Any]:
    """Attach a reserved/exited same-round lease to a new PID by explicit transition only."""
    _validate_manifest(manifest)
    snapshot = inspector.inspect(pid)
    if not snapshot.alive:
        raise CertificationLeaseError("new lease process is not live")
    candidate = _record_for_snapshot(
        manifest=manifest, lease_id=lease_id, executable_path=executable_path,
        executable_sha256=executable_sha256, world_identity=world_identity_for_manifest(manifest),
        snapshot=snapshot, state="active", cleanup_token=cleanup_token,
    )
    if not _matches_live_owner(candidate, inspector):
        raise CertificationLeaseError("new lease process does not match executable, birth identity, and command")
    try:
        connection.execute("BEGIN IMMEDIATE")
        current = next((item for item in _current_leases(connection)
                        if item["round_id"] == candidate["round_id"] and item["lease_id"] == lease_id), None)
        if current is None:
            raise CertificationLeaseError("lease must be reserved before a process can be attached")
        if current.get("state") == "active":
            if all(current.get(key) == candidate.get(key) for key in (
                    "world_identity", "executable_path", "executable_sha256", "pid",
                    "process_birth_identity", "command", "process_identity", "cleanup_token")):
                connection.execute("COMMIT")
                return current | {"idempotent": True}
            if _matches_live_owner(current, inspector):
                raise CertificationLeaseError("live process replacement requires its old owner to exit first")
        elif current.get("state") not in {"reserved", "stale_recovered"}:
            raise CertificationLeaseError("released lease cannot be relaunched")
        elif current.get("cleanup_token", "") != candidate.get("cleanup_token", ""):
            raise CertificationLeaseError("lease cleanup token does not match its reservation")
        _recover_or_reject_conflicts(connection, candidate=candidate, inspector=inspector)
        appended = _append(connection, record=candidate, event_kind="transferred", state="active",
                           extra={"previous_pid": current.get("pid", 0),
                                  "previous_process_birth_identity": current.get("process_birth_identity", "")})
        connection.execute("COMMIT")
        return appended | {"idempotent": False}
    except BaseException:
        if connection.in_transaction:
            connection.execute("ROLLBACK")
        raise


def release_certification_lease(connection: Any, *, manifest: Mapping[str, Any], lease_id: str,
                                pid: int, world_identity: str, executable_path: str,
                                executable_sha256: str, process_birth_identity: str,
                                inspector: ProcessInspector, cleanup_token: str = "") -> Dict[str, Any]:
    """Verify all sealed ownership facts immediately before signalling the process."""
    _validate_manifest(manifest)
    if world_identity != world_identity_for_manifest(manifest):
        return {"status": "rejected_world_identity"}
    try:
        connection.execute("BEGIN IMMEDIATE")
        current = next((item for item in _current_leases(connection)
                        if item["round_id"] == manifest["round_id"] and item["lease_id"] == lease_id), None)
        if current is None:
            connection.execute("COMMIT")
            return {"status": "rejected_lease_unknown"}
        if current.get("cleanup_token") and cleanup_token != current.get("cleanup_token"):
            _append(connection, record=current, event_kind="release_rejected", state=str(current.get("state", "")),
                    extra={"release_rejection": "cleanup_token_mismatch"})
            connection.execute("COMMIT")
            return {"status": "rejected_cleanup_token"}
        expected = {
            "pid": pid, "world_identity": world_identity,
            "executable_path": _canonical_path(executable_path),
            "executable_sha256": executable_sha256.lower(),
            "process_birth_identity": process_birth_identity,
        }
        if any(current.get(key) != value for key, value in expected.items()):
            _append(connection, record=current, event_kind="release_rejected", state=str(current.get("state", "")),
                    extra={"release_rejection": "stored_lease_identity_mismatch"})
            connection.execute("COMMIT")
            return {"status": "rejected_stored_identity"}
        if current.get("state") == "released":
            connection.execute("COMMIT")
            return {"status": "already_released", "lease": current, "idempotent": True}
        if current.get("state") not in {"active", "termination_requested"}:
            _append(connection, record=current, event_kind="release_rejected", state=str(current.get("state", "")),
                    extra={"release_rejection": "lease_not_active"})
            connection.execute("COMMIT")
            return {"status": "rejected_stored_identity"}
        snapshot = inspector.inspect(pid)
        if not snapshot.alive:
            released = _append(connection, record=current, event_kind="exited", state="released",
                               extra={"signal": "none", "cleanup": "already_exited"})
            connection.execute("COMMIT")
            return {"status": "already_exited", "lease": released}
        if not _matches_live_owner(current, inspector):
            _append(connection, record=current, event_kind="release_rejected", state="active",
                    extra={"release_rejection": "current_process_identity_mismatch"})
            connection.execute("COMMIT")
            return {"status": "rejected_current_process_identity"}
        if current.get("state") == "termination_requested":
            connection.execute("COMMIT")
            return {"status": "exit_unobserved", "lease": current}
        inspector.signal(pid, signal.SIGTERM)
        requested = _append(connection, record=current, event_kind="termination_requested",
                            state="termination_requested", extra={"signal": "SIGTERM"})
        exited = inspector.inspect(pid)
        if not exited.alive:
            released = _append(connection, record=requested, event_kind="exited", state="released",
                               extra={"signal": "SIGTERM", "cleanup": "observed_exit"})
            connection.execute("COMMIT")
            return {"status": "terminated", "lease": released}
        connection.execute("COMMIT")
        return {"status": "exit_unobserved", "lease": requested}
    except BaseException:
        if connection.in_transaction:
            connection.execute("ROLLBACK")
        raise


def release_certification_lease_handle(connection: Any, *, manifest: Mapping[str, Any],
                                       lease: Mapping[str, Any], inspector: ProcessInspector) -> Dict[str, Any]:
    """Release a process only from the exact persisted lease receipt."""
    return release_certification_lease(
        connection, manifest=manifest, lease_id=str(lease.get("lease_id", "")),
        pid=int(lease.get("pid", 0) or 0), world_identity=str(lease.get("world_identity", "")),
        executable_path=str(lease.get("executable_path", "")),
        executable_sha256=str(lease.get("executable_sha256", "")),
        process_birth_identity=str(lease.get("process_birth_identity", "")), inspector=inspector,
        cleanup_token=str(lease.get("cleanup_token", "")),
    )
