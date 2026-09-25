#!/usr/bin/env python3
"""Named worker conversations, dispatched by the existing coordinator adapter.

The registry owns conversation identity, not task acceptance. The deadline harness
continues to own assignments and receipts. Only the adapter sends worker RPCs.
"""
from __future__ import annotations

import argparse
from contextlib import closing
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sqlite3
import sys
import time
from typing import Any, Mapping
import uuid


class WorkerLibraryError(RuntimeError):
    pass


EXECUTING = {"starting", "submitting", "running", "uncertain"}
BINDING_FIELDS = ("workspace", "run_id", "thread_id", "runner_pid", "server_pid",
                  "socket", "deadline_state", "lineage", "supervisor_id")


def _root(workspace: Path) -> Path:
    return Path(workspace).resolve() / ".de67/state/worker-library"


def _pit_crew_boundary(workspace: Path, assignment: Mapping[str, Any]) -> None:
    """Offer one completed audit append to the optional Pit Crew package.

    The core worker library must remain ordinary and usable when the optional
    package is absent, malformed, disabled, or otherwise unavailable.  This
    deliberately gives Pit Crew no RPC, harness, or coordinator object.
    """
    try:
        config_path = Path(workspace).resolve() / ".de67/state/workspace.json"
        value = json.loads(config_path.read_text(encoding="utf-8")) if config_path.exists() else {}
        config = value.get("jev_pit_crew") if isinstance(value, dict) else None
        if not isinstance(config, dict) or config.get("mode") not in {"shadow", "on"}:
            return
        source = Path(__file__).resolve().parents[2] / "integrations/jev_pit_crew/pit_crew.py"
        if not source.is_file():
            return
        module_name = "_de67_optional_pit_crew_" + hashlib.sha256(str(source.resolve()).encode()).hexdigest()[:16]
        module = sys.modules.get(module_name)
        if module is None:
            spec = importlib.util.spec_from_file_location(module_name, source)
            if spec is None or spec.loader is None:
                return
            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            try:
                spec.loader.exec_module(module)
            except Exception:
                sys.modules.pop(module_name, None)
                return
        module.observe_boundary(Path(workspace), dict(assignment))
    except Exception:
        # An optional advisory must never block an audit event or alter worker
        # dispatch semantics.  Its own durable state is inspectable when enabled.
        return


def _connect(workspace: Path, *, write: bool = False) -> sqlite3.Connection | None:
    path = _root(workspace) / "registry.sqlite3"
    if not write and not path.exists():
        return None
    if write:
        path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(str(path) if write else path.as_uri() + "?mode=ro", uri=not write,
                         timeout=30)
    db.row_factory = sqlite3.Row
    if write:
        db.executescript("""
            CREATE TABLE IF NOT EXISTS identity (workspace TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS workers (
                name TEXT PRIMARY KEY, job TEXT NOT NULL, model TEXT NOT NULL,
                effort TEXT NOT NULL, thread_id TEXT UNIQUE,
                created_at REAL NOT NULL, retired_at REAL);
            CREATE TABLE IF NOT EXISTS assignments (
                id TEXT PRIMARY KEY, name TEXT NOT NULL, task_id TEXT NOT NULL,
                state_path TEXT NOT NULL, lineage TEXT NOT NULL,
                packet TEXT NOT NULL, digest TEXT NOT NULL, assignment_revision TEXT,
                binding TEXT NOT NULL, status TEXT NOT NULL, worker_id TEXT, claim_supervisor_id TEXT,
                turn_id TEXT, request_id TEXT, result_path TEXT, error TEXT,
                created_at REAL NOT NULL, updated_at REAL NOT NULL);
            CREATE TABLE IF NOT EXISTS requests (
                id TEXT PRIMARY KEY, name TEXT NOT NULL, assignment_id TEXT NOT NULL,
                kind TEXT NOT NULL, message TEXT, binding TEXT NOT NULL,
                status TEXT NOT NULL, error TEXT, created_at REAL NOT NULL,
                updated_at REAL NOT NULL);
        """)
        with db:
            db.execute("INSERT INTO identity SELECT ? WHERE NOT EXISTS (SELECT 1 FROM identity)",
                       (str(Path(workspace).resolve()),))
    identity = db.execute("SELECT workspace FROM identity").fetchall()
    if len(identity) != 1 or identity[0][0] != str(Path(workspace).resolve()):
        db.close()
        raise WorkerLibraryError("Worker registry belongs to a different workspace")
    return db


def _name(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", value):
        raise WorkerLibraryError("Worker name must use letters, digits, dots, underscores or hyphens")
    return value


def _text(value: str, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise WorkerLibraryError(label + " must not be empty")
    return value.strip()


def _binding(workspace: Path) -> dict[str, Any]:
    path = Path(workspace).resolve() / ".de67/state/coordinator-input.json"
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise WorkerLibraryError("No current coordinator input binding") from error
    if (value.get("workspace") != str(Path(workspace).resolve())
            or value.get("role") != "coordinator" or value.get("state") != "active"
            or any(value.get(key) in (None, "") for key in BINDING_FIELDS)):
        raise WorkerLibraryError("Coordinator binding is inactive, incomplete or belongs to another workspace")
    if not Path(value["socket"]).exists():
        raise WorkerLibraryError("Coordinator App Server socket is unavailable; use native workers on the CLI transport")
    return value


def _same_binding(left: Mapping[str, Any], right: Mapping[str, Any]) -> bool:
    return all(left.get(key) == right.get(key) for key in BINDING_FIELDS)


def _old_server_present(binding: Mapping[str, Any]) -> bool:
    if not Path(binding["socket"]).exists():
        return False
    try:
        os.kill(int(binding["server_pid"]), 0)
        return True
    except ProcessLookupError:
        return False
    except (PermissionError, ValueError):
        return True


def _authorize(workspace: Path, environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    environment = os.environ if environment is None else environment
    current = _binding(workspace)
    if (environment.get("CODEX_THREAD_ID") != current["thread_id"]
            or environment.get("DE67_COORDINATOR_RUN_ID") != current["run_id"]):
        raise WorkerLibraryError("Only the current Sol conversation may change the worker library")
    return {key: current[key] for key in BINDING_FIELDS}


def _task(state: Path, lineage: str, task_id: str) -> dict[str, Any]:
    try:
        with closing(sqlite3.connect(Path(state).resolve().as_uri() + "?mode=ro", uri=True)) as db:
            db.row_factory = sqlite3.Row
            row = db.execute("""SELECT t.task_id, t.claim_id, t.attempt_terminal_at,
                w.worker_id, w.coordinator_session_id, w.supervisor_id, w.released_at
                FROM tasks t LEFT JOIN worker_claims w
                ON w.lineage_id=t.lineage_id AND w.task_id=t.task_id
                WHERE t.lineage_id=? AND t.task_id=?""", (lineage, task_id)).fetchone()
    except sqlite3.Error as error:
        raise WorkerLibraryError("Task state is unavailable: " + str(error)) from error
    if row is None:
        raise WorkerLibraryError("No exact task in this lineage: " + task_id)
    return dict(row)


def _validate_packet(workspace: Path, state: Path, lineage: str, task_id: str,
                     packet: Path, digest: str) -> str | None:
    workspace, state, packet = Path(workspace).resolve(), Path(state).resolve(), Path(packet).resolve()
    try:
        packet.relative_to(workspace / ".de67/state/worker-dispatch")
    except ValueError as error:
        raise WorkerLibraryError("Use the prepared packet in this workspace's worker-dispatch directory") from error
    if not re.fullmatch(r"[a-f0-9]{64}", digest) or hashlib.sha256(packet.read_bytes()).hexdigest() != digest:
        raise WorkerLibraryError("Worker packet SHA-256 mismatch")
    index = workspace / ".de67/state/work-context.sqlite3"
    try:
        with closing(sqlite3.connect(index.as_uri() + "?mode=ro", uri=True)) as db:
            row = db.execute("""SELECT path,digest,context_json FROM dispatches
                WHERE source=? AND lineage=? AND task=?
                ORDER BY recorded_at DESC,rowid DESC LIMIT 1""",
                (str(state), lineage, task_id)).fetchone()
    except sqlite3.Error as error:
        raise WorkerLibraryError("No recorded prepared dispatch for this exact task") from error
    if row is None or Path(row[0]).resolve() != packet or row[1] != digest:
        raise WorkerLibraryError("Packet is not the current prepared dispatch for this exact task")
    from policy_kernel import current_owner_contract
    try:
        metadata = json.loads(row[2])
        owner_digest = metadata.get("owner_contract_sha256") if isinstance(metadata, dict) else None
    except (TypeError, ValueError):
        owner_digest = None
    if owner_digest != hashlib.sha256(current_owner_contract(workspace).encode("utf-8")).hexdigest():
        raise WorkerLibraryError("Owner instructions changed or were not bound to this packet; prepare its current packet again")
    from context_library import _assignment_revision, selected_context, task_view
    revision = _assignment_revision(workspace, task_id)
    view = task_view(workspace, task_id)
    if view.get("brief") and view.get("assignment_revision") != revision:
        raise WorkerLibraryError("Task assignment changed; prepare its current packet before dispatch")
    selected_context(workspace, task_id)  # Recheck selected source/dependency freshness, without writing.
    return revision


def _worker(db: sqlite3.Connection, name: str) -> dict[str, Any]:
    row = db.execute("SELECT * FROM workers WHERE name=?", (_name(name),)).fetchone()
    if row is None:
        raise WorkerLibraryError("Unknown named worker: " + name)
    return dict(row)


def _latest(db: sqlite3.Connection, name: str) -> dict[str, Any] | None:
    row = db.execute("SELECT * FROM assignments WHERE name=? ORDER BY created_at DESC,rowid DESC LIMIT 1",
                     (name,)).fetchone()
    return dict(row) if row is not None else None


def _idle(assignment: dict[str, Any] | None) -> bool:
    if assignment is None:
        return True
    if assignment["status"] in EXECUTING or assignment["status"] == "pending":
        return False
    task = _task(Path(assignment["state_path"]), assignment["lineage"], assignment["task_id"])
    binding = json.loads(assignment["binding"])
    owns_task = (task["worker_id"] is not None and task["worker_id"] == assignment["worker_id"]
                 and task["released_at"] is None and task["coordinator_session_id"] == binding["thread_id"]
                 and task["supervisor_id"] == (assignment["claim_supervisor_id"] or str(binding["supervisor_id"])))
    # A rejected request remains reusable if another assignment acquired the task.
    return task["attempt_terminal_at"] is not None or (assignment["status"] == "rejected" and not owns_task)


def create(workspace: Path, name: str, job: str, model: str, effort: str, *,
           environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    _authorize(workspace, environment)
    name, job = _name(name), _text(job, "Worker job")
    from policy_kernel import worker_model_choices
    if {"model": model, "reasoning_effort": effort} not in worker_model_choices(Path(workspace)):
        raise WorkerLibraryError("Choose an available ordinary-worker model and effort")
    with closing(_connect(workspace, write=True)) as db, db:
        try:
            db.execute("INSERT INTO workers VALUES (?,?,?,?,NULL,?,NULL)",
                       (name, job, model, effort, time.time()))
        except sqlite3.IntegrityError as error:
            raise WorkerLibraryError("Worker name already exists: " + name) from error
    return describe(workspace, name)


def catalog(workspace: Path, state: Path | None = None, lineage: str | None = None) -> dict[str, Any]:
    """Compact, read-only discovery; never load conversation transcripts."""
    result: dict[str, Any] = {"schema": "de67.worker-library.v1", "workspace": str(Path(workspace).resolve()),
                              "workers": []}
    try:
        binding = _binding(workspace)
        result["runtime"] = {"available": True, "coordinator_session_id": binding["thread_id"],
                             "run_id": binding["run_id"], "transport": "app-server"}
    except WorkerLibraryError as error:
        result["runtime"] = {"available": False, "reason": str(error),
                             "alternative": "Use native workers when the coordinator uses the CLI transport."}
    db = _connect(workspace)
    if db is None:
        return result
    with closing(db):
        for row in db.execute("SELECT * FROM workers ORDER BY name"):
            worker = dict(row)
            assignment = _latest(db, worker["name"])
            worker["status"] = "retired" if worker["retired_at"] is not None else "idle"
            worker["assignment"] = None
            if assignment is not None:
                if ((state is not None and assignment["state_path"] != str(Path(state).resolve()))
                        or (lineage is not None and assignment["lineage"] != lineage)):
                    worker["status"] = "other-lineage" if worker["retired_at"] is None else "retired"
                    result["workers"].append(worker)
                    continue
                worker["assignment"] = {key: assignment[key] for key in
                    ("id", "task_id", "state_path", "lineage", "status", "worker_id", "turn_id",
                     "request_id", "result_path", "error", "updated_at")}
                worker["assignment"]["events_path"] = str(_root(workspace) / "events" / (assignment["id"] + ".jsonl"))
                try:
                    idle = _idle(assignment)
                except WorkerLibraryError as error:
                    idle = False
                    worker["assignment"]["state_error"] = str(error)
                if worker["retired_at"] is None and not idle:
                    worker["status"] = assignment["status"]
            result["workers"].append(worker)
    result["evidence_limit"] = "Worker return is nonterminal evidence. Idle requires the prior task to be settled; runtime state is adapter-observed."
    return result


def describe(workspace: Path, name: str, job: str | None = None, *, model: str | None = None,
             effort: str | None = None,
             environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    if any(value is not None for value in (job, model, effort)):
        _authorize(workspace, environment)
        with closing(_connect(workspace, write=True)) as db:
            db.execute("BEGIN IMMEDIATE")
            worker = _worker(db, name)
            if worker["retired_at"] is not None or not _idle(_latest(db, name)):
                raise WorkerLibraryError("Revise a worker's reusable job only while it is idle")
            from policy_kernel import worker_model_choices
            model, effort = model or worker["model"], effort or worker["effort"]
            if {"model": model, "reasoning_effort": effort} not in worker_model_choices(Path(workspace)):
                raise WorkerLibraryError("Choose an available ordinary-worker model and effort")
            db.execute("UPDATE workers SET job=?,model=?,effort=? WHERE name=?",
                       (_text(job, "Worker job") if job is not None else worker["job"], model, effort, name))
            db.commit()
    return next((worker for worker in catalog(workspace)["workers"] if worker["name"] == _name(name)), None) or _unknown(name)


def _unknown(name: str) -> Any:
    raise WorkerLibraryError("Unknown named worker: " + name)


def assign(workspace: Path, name: str, task_id: str, packet: Path, digest: str,
           state: Path, lineage: str, *, environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    binding = _authorize(workspace, environment)
    state, packet = Path(state).resolve(), Path(packet).resolve()
    if str(state) != binding["deadline_state"] or lineage != binding["lineage"]:
        raise WorkerLibraryError("Assignment state/lineage differs from the current coordinator binding")
    revision = _validate_packet(workspace, state, lineage, task_id, packet, digest)
    task = _task(state, lineage, task_id)
    if task["attempt_terminal_at"] is not None or task["worker_id"]:
        raise WorkerLibraryError("Assign an open, unbound task; use message to continue its existing worker")
    now, request_id, assignment_id = time.time(), uuid.uuid4().hex, uuid.uuid4().hex
    with closing(_connect(workspace, write=True)) as db:
        db.execute("BEGIN IMMEDIATE")
        worker = _worker(db, name)
        if worker["retired_at"] is not None or not _idle(_latest(db, name)):
            raise WorkerLibraryError("Worker is retired or still owns an unsettled assignment")
        if db.execute("""SELECT 1 FROM assignments WHERE state_path=? AND lineage=? AND task_id=?
                      AND status!='rejected'""", (str(state), lineage, task_id)).fetchone():
            raise WorkerLibraryError("This exact task already has a worker-library request; inspect its status")
        try:
            db.execute("""INSERT INTO assignments
                (id,name,task_id,state_path,lineage,packet,digest,assignment_revision,binding,status,
                 request_id,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,'pending',?,?,?)""",
                (assignment_id, name, task_id, str(state), lineage, str(packet), digest, revision,
                 json.dumps(binding), request_id, now, now))
            db.execute("INSERT INTO requests VALUES (?,?,?,'assign',NULL,?,'pending',NULL,?,?)",
                       (request_id, name, assignment_id, json.dumps(binding), now, now))
        except sqlite3.IntegrityError as error:
            raise WorkerLibraryError("This exact task already has a worker-library request; inspect its status") from error
        db.commit()
    return {"request_id": request_id, "worker_name": name, "task_id": task_id, "state": "pending"}


def message(workspace: Path, name: str, text: str, *,
            environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    binding = _authorize(workspace, environment)
    now, request_id = time.time(), uuid.uuid4().hex
    with closing(_connect(workspace, write=True)) as db:
        db.execute("BEGIN IMMEDIATE")
        worker, assignment = _worker(db, name), _latest(db, name)
        if (worker["retired_at"] is not None or assignment is None or _idle(assignment)
                or assignment["status"] not in {"running", "returned", "failed", "interrupted"}
                or json.loads(assignment["binding"])["thread_id"] != binding["thread_id"]):
            raise WorkerLibraryError("Message requires this coordinator's known running or returned assignment")
        previous_binding = json.loads(assignment["binding"])
        if not _same_binding(previous_binding, binding) and _old_server_present(previous_binding):
            raise WorkerLibraryError("Previous worker App Server is still present; reconcile its owner before resuming")
        task = _task(Path(assignment["state_path"]), assignment["lineage"], assignment["task_id"])
        if (task["released_at"] is not None or task["worker_id"] != worker["thread_id"]
                or task["coordinator_session_id"] != binding["thread_id"]
                or task["supervisor_id"] != (assignment["claim_supervisor_id"] or str(previous_binding["supervisor_id"]))):
            raise WorkerLibraryError("Worker no longer owns the task")
        if not _same_binding(previous_binding, binding):
            if assignment["status"] == "running" and not assignment["turn_id"]:
                raise WorkerLibraryError("Previous worker start is ambiguous; do not replay it")
            db.execute("UPDATE assignments SET binding=?,status=?,updated_at=? WHERE id=?",
                       (json.dumps(binding), "interrupted" if assignment["status"] == "running" else assignment["status"],
                        now, assignment["id"]))
        db.execute("INSERT INTO requests VALUES (?,?,?,'message',?,?,'pending',NULL,?,?)",
                   (request_id, name, assignment["id"], _text(text, "Message"), json.dumps(binding), now, now))
        db.commit()
    return {"request_id": request_id, "worker_name": name, "task_id": assignment["task_id"], "state": "pending"}


def retire(workspace: Path, name: str, *, environment: Mapping[str, str] | None = None) -> dict[str, Any]:
    _authorize(workspace, environment)
    with closing(_connect(workspace, write=True)) as db:
        db.execute("BEGIN IMMEDIATE")
        _worker(db, name)
        if not _idle(_latest(db, name)):
            raise WorkerLibraryError("Only an idle worker can be retired")
        db.execute("UPDATE workers SET retired_at=COALESCE(retired_at,?) WHERE name=?", (time.time(), name))
        db.commit()
    return describe(workspace, name)


def owned_assignments(workspace: Path, state: Path, lineage: str,
                      coordinator_session_id: str | None = None) -> dict[str, str]:
    """Return registry-backed durable ownership, not a process-liveness claim."""
    result: dict[str, str] = {}
    db = _connect(workspace)
    if db is None:
        return result
    with closing(db):
        rows = db.execute("""SELECT a.* FROM assignments a JOIN workers w ON a.name=w.name
            WHERE a.state_path=? AND a.lineage=? AND a.worker_id=w.thread_id""",
            (str(Path(state).resolve()), lineage)).fetchall()
        for row in rows:
            binding = json.loads(row["binding"])
            if coordinator_session_id is not None and binding["thread_id"] != coordinator_session_id:
                continue
            task = _task(state, lineage, row["task_id"])
            if (task["attempt_terminal_at"] is None and task["released_at"] is None
                    and task["worker_id"] == row["worker_id"]
                    and task["coordinator_session_id"] == binding["thread_id"]
                    and task["supervisor_id"] == (row["claim_supervisor_id"] or str(binding["supervisor_id"]))):
                result[row["task_id"]] = row["worker_id"]
    return result


def worker_owners(workspace: Path, state: Path, lineage: str) -> dict[str, str]:
    return {worker: _task(state, lineage, task)["coordinator_session_id"]
            for task, worker in owned_assignments(workspace, state, lineage).items()}


def owns_assignment(workspace: Path, task_id: str, worker_id: str, coordinator_session_id: str,
                    state: Path | None = None, lineage: str | None = None) -> bool:
    if state is None or lineage is None:
        return False
    return owned_assignments(workspace, state, lineage, coordinator_session_id).get(task_id) == worker_id


def request_status(workspace: Path, request_id: str) -> dict[str, Any]:
    db = _connect(workspace)
    if db is None:
        raise WorkerLibraryError("Unknown worker request")
    with closing(db):
        row = db.execute("""SELECT r.id AS request_id,r.name AS worker_name,r.status AS state,
            r.error,a.task_id,a.worker_id,a.turn_id,a.result_path FROM requests r
            JOIN assignments a ON a.id=r.assignment_id WHERE r.id=?""", (request_id,)).fetchone()
    if row is None:
        raise WorkerLibraryError("Unknown worker request")
    return dict(row)


def wait_request(workspace: Path, request_id: str, timeout: float = 60) -> dict[str, Any]:
    end = time.monotonic() + max(0, timeout)
    while True:
        value = request_status(workspace, request_id)
        if value["state"] not in {"pending", "submitting"} or time.monotonic() >= end:
            return value
        time.sleep(min(.1, max(0, end - time.monotonic())))


def wait(workspace: Path, name: str, timeout: float = 30) -> dict[str, Any]:
    end = time.monotonic() + max(0, timeout)
    while True:
        value = describe(workspace, name)
        if value["status"] not in {"pending", "starting", "submitting", "running"} or time.monotonic() >= end:
            return value
        time.sleep(min(.2, max(0, end - time.monotonic())))


class WorkerDispatcher:
    """Runs inside the coordinator adapter and shares its long-lived RPC connection."""

    def __init__(self, workspace: Path, rpc: Any, binding: Mapping[str, Any]):
        self.workspace, self.rpc = Path(workspace).resolve(), rpc
        self.binding = {key: binding[key] for key in BINDING_FIELDS}
        self.final_messages: dict[tuple[str, str], dict[str, str]] = {}
        self.reconcile_after: dict[str, float] = {}
        self.inactive_uncertain: set[str] = set()

    def _update(self, assignment_id: str, **fields: Any) -> None:
        fields["updated_at"] = time.time()
        with closing(_connect(self.workspace, write=True)) as db, db:
            db.execute("UPDATE assignments SET " + ",".join(key + "=?" for key in fields) + " WHERE id=?",
                       (*fields.values(), assignment_id))

    def _request_update(self, request_id: str, status: str, error: str | None = None) -> None:
        with closing(_connect(self.workspace, write=True)) as db, db:
            db.execute("UPDATE requests SET status=?,error=?,updated_at=? WHERE id=?",
                       (status, error, time.time(), request_id))

    def _checkpoint(self, assignment: Mapping[str, Any], kind: str, evidence: dict[str, Any]) -> None:
        from deadline_harness import DeadlineError, DeadlineHarness
        try:
            with DeadlineHarness(assignment["state_path"]) as harness:
                harness.checkpoint_worker(assignment["lineage"], assignment["task_id"], assignment["worker_id"],
                                          kind, json.dumps(evidence, ensure_ascii=False, sort_keys=True))
        except DeadlineError:
            task = _task(Path(assignment["state_path"]), assignment["lineage"], assignment["task_id"])
            if task["attempt_terminal_at"] is None or task["released_at"] is None:
                raise
            # A cancellation or settlement can race the last worker event. Preserve
            # its registry evidence without reopening or altering the terminal task.
            self._notice(assignment, "Worker " + assignment["name"] + " " + kind
                         + " arrived after task " + assignment["task_id"]
                         + " was settled. Evidence remains in the worker library; no claim was reopened.")

    def _notice(self, assignment: Mapping[str, Any], text: str) -> None:
        from agent_mailbox import enqueue
        enqueue(self.workspace, "coordinator", "worker:" + assignment["name"], text)

    def _audit(self, assignment: Mapping[str, Any], message: dict[str, Any]) -> None:
        path = _root(self.workspace) / "events" / (assignment["id"] + ".jsonl")
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps({"worker_name": assignment["name"], "task_id": assignment["task_id"],
                                     "observed_at": time.time(), **message}, ensure_ascii=False) + "\n")
        _pit_crew_boundary(self.workspace, assignment)

    def _prepared_delivery(self, assignment: Mapping[str, Any], worker_id: str | None) -> tuple[str, dict[str, Any]]:
        from worker_packet import delivery_text
        current = Path(assignment["packet"]).read_bytes()
        if hashlib.sha256(current).hexdigest() != assignment["digest"]:
            raise WorkerLibraryError("Worker packet changed before delivery")
        previous = None
        previous_assignment_id = None
        if worker_id:
            with closing(_connect(self.workspace)) as db:
                prior = db.execute("""SELECT a.id,a.packet,a.digest FROM assignments a
                    WHERE a.worker_id=? AND a.id!=? AND a.turn_id IS NOT NULL AND a.turn_id!=''
                    AND a.status NOT IN ('pending','starting','submitting','uncertain','rejected')
                    AND EXISTS (SELECT 1 FROM requests r WHERE r.assignment_id=a.id
                                AND r.kind='assign' AND r.status='submitted')
                    ORDER BY a.created_at DESC,a.rowid DESC LIMIT 1""", (worker_id, assignment["id"])).fetchone()
            if prior is not None:
                try:
                    saved = Path(prior["packet"]).read_bytes()
                    if hashlib.sha256(saved).hexdigest() == prior["digest"]:
                        previous = saved.decode("utf-8")
                        previous_assignment_id = prior["id"]
                except (OSError, UnicodeDecodeError):
                    pass  # An unavailable or altered baseline requires full delivery.
        text, metadata = delivery_text(current.decode("utf-8"), previous)
        return text, {**metadata, "previous_assignment_id": previous_assignment_id,
                      "canonical_packet": assignment["packet"], "canonical_sha256": assignment["digest"]}

    @staticmethod
    def _matches_request(item: Mapping[str, Any], assignment: Mapping[str, Any]) -> bool:
        return (item.get("type") == "userMessage"
                and item.get("clientId") == "de67-worker:" + assignment["request_id"])

    def _adopt_turn(self, assignment: dict[str, Any], turn_id: str, source: str) -> bool:
        if not turn_id:
            return False
        with closing(_connect(self.workspace)) as db:
            if db.execute("SELECT 1 FROM assignments WHERE worker_id=? AND turn_id=? AND id!=?",
                          (assignment["worker_id"], turn_id, assignment["id"])).fetchone():
                return False
        self._update(assignment["id"], status="running", turn_id=turn_id, error=None)
        self.inactive_uncertain.discard(assignment["id"])
        self._request_update(assignment["request_id"], "submitted")
        assignment.update(status="running", turn_id=turn_id, error=None)
        self._audit(assignment, {"method": "de67/workerTurn/reconciled", "params": {
            "threadId": assignment["worker_id"], "turnId": turn_id,
            "requestId": assignment["request_id"], "source": source, "replayed": False}})
        return True

    def _reconcile_one(self, assignment: dict[str, Any]) -> None:
        previous_binding = json.loads(assignment["binding"])
        changed_server = not _same_binding(previous_binding, self.binding)
        if previous_binding["thread_id"] != self.binding["thread_id"]:
            return
        if changed_server and _old_server_present(previous_binding):
            return
        if changed_server and assignment["status"] in {"pending", "starting"}:
            # The persisted phase precedes turn/start; no generation was submitted.
            task = _task(Path(assignment["state_path"]), assignment["lineage"], assignment["task_id"])
            status = "failed" if task["worker_id"] == assignment["worker_id"] and task["worker_id"] else "rejected"
            explanation = "Previous owned App Server stopped before turn submission; no generation was sent"
            self._update(assignment["id"], binding=json.dumps(self.binding), status=status, error=explanation)
            self._request_update(assignment["request_id"], "rejected", explanation)
            self._notice(assignment, "Worker " + assignment["name"] + ": " + explanation)
            return
        if not owns_assignment(self.workspace, assignment["task_id"], assignment["worker_id"],
                               self.binding["thread_id"], Path(assignment["state_path"]), assignment["lineage"]):
            return
        thread = self.rpc.call("thread/read", {"threadId": assignment["worker_id"], "includeTurns": True})["thread"]
        if (thread.get("id") != assignment["worker_id"]
                or (thread.get("cwd") is not None and Path(thread["cwd"]).resolve() != self.workspace)):
            raise WorkerLibraryError("Recovery thread identity or workspace does not match its assignment")
        turns = thread.get("turns", [])
        matches = [turn for turn in turns if (
            assignment["turn_id"] and turn.get("id") == assignment["turn_id"]
        ) or any(self._matches_request(item, assignment) for item in turn.get("items", []))]
        if len(matches) > 1:
            raise WorkerLibraryError("More than one stored turn matches this worker request; inspect the conversation")
        active = thread.get("status", {}).get("type") == "active"
        self.inactive_uncertain.discard(assignment["id"])
        if changed_server and active:
            # Subscribe to an already active turn. Resuming a thread generates no work.
            self.rpc.call("thread/resume", {"threadId": assignment["worker_id"], "excludeTurns": True})
        if changed_server:
            self._update(assignment["id"], binding=json.dumps(self.binding))
            assignment["binding"] = json.dumps(self.binding)
        if not matches:
            if not changed_server and thread.get("status", {}).get("type") in {"idle", "notLoaded"}:
                # No work is currently active. Keep delivery uncertainty until the
                # adapter stops its owned server; it must not hold teardown open.
                self.inactive_uncertain.add(assignment["id"])
            if changed_server and not active:
                explanation = ("Previous owned App Server stopped. No stored turn identifies this request; "
                               "its outcome remains uncertain. No work was replayed. Inspect current evidence "
                               "before sending an intentional continuation.")
                self._update(assignment["id"], status="interrupted", error=explanation)
                self._notice(assignment, "Worker " + assignment["name"] + ": " + explanation)
            return
        turn = dict(matches[0])
        if not self._adopt_turn(assignment, turn.get("id"), "stored userMessage clientId or known turn receipt"):
            return
        if turn.get("status") == "inProgress" and changed_server and not active:
            turn.update(status="interrupted", error={"message": "Previous owned App Server stopped during this turn"})
        if turn.get("status") in {"completed", "failed", "interrupted"}:
            self.observe({"method": "turn/completed", "params": {"threadId": assignment["worker_id"], "turn": turn}})

    def reconcile(self) -> None:
        """Read or observe accepted work after a lost receipt; never replay its request."""
        db = _connect(self.workspace)
        if db is None:
            return
        with closing(db):
            rows = [dict(row) for row in db.execute("""SELECT * FROM assignments
                WHERE status IN ('pending','running','starting','submitting','uncertain','interrupted')""")]
        for assignment in rows:
            same_server = _same_binding(json.loads(assignment["binding"]), self.binding)
            if same_server and assignment["status"] not in {"submitting", "uncertain"}:
                continue
            if not assignment["worker_id"] and assignment["status"] not in {"pending", "starting"}:
                continue
            if self.reconcile_after.get(assignment["id"], 0) > time.monotonic():
                continue
            self.reconcile_after[assignment["id"]] = time.monotonic() + 5
            try:
                self._reconcile_one(assignment)
            except Exception as error:
                explanation = "Worker recovery is waiting for readable thread evidence: " + str(error)
                if assignment.get("error") != explanation:
                    self._update(assignment["id"], error=explanation)
                    self._audit(assignment, {"method": "de67/workerTurn/reconciliationPending", "params": {
                        "threadId": assignment["worker_id"], "requestId": assignment["request_id"], "error": explanation}})

    def process_pending(self) -> None:
        db = _connect(self.workspace)
        if db is None:
            return
        with closing(db):
            requests = [dict(row) for row in db.execute("SELECT * FROM requests WHERE status='pending' ORDER BY created_at,rowid")]
        for request in requests:
            phase = "validation"
            try:
                if (not _same_binding(_binding(self.workspace), self.binding)
                        or not _same_binding(json.loads(request["binding"]), self.binding)):
                    raise WorkerLibraryError("Dispatch request belongs to a stale coordinator")
                with closing(_connect(self.workspace)) as db:
                    assignment = dict(db.execute("SELECT * FROM assignments WHERE id=?", (request["assignment_id"],)).fetchone())
                    worker = _worker(db, request["name"])
                if worker["model"] not in {"gpt-6-luna", "gpt-6-sol", "gpt-6-astra"}:
                    raise WorkerLibraryError("Worker model is retired; use GPT-6 Luna, Sol or Astra")
                from policy_kernel import worker_model_choices
                if {"model": worker["model"], "reasoning_effort": worker["effort"]} not in worker_model_choices(self.workspace):
                    raise WorkerLibraryError("Worker model/effort is no longer available in the current roster")
                task = _task(Path(assignment["state_path"]), assignment["lineage"], assignment["task_id"])
                if worker["retired_at"] is not None or task["attempt_terminal_at"] is not None:
                    raise WorkerLibraryError("Worker is retired or task is terminal")
                if request["kind"] == "message" and assignment["status"] == "running":
                    if not owns_assignment(self.workspace, assignment["task_id"], assignment["worker_id"],
                                           self.binding["thread_id"], Path(assignment["state_path"]), assignment["lineage"]):
                        raise WorkerLibraryError("Worker no longer owns this assignment")
                    phase = "steering"
                    self._request_update(request["id"], "submitting")
                    self.rpc.call("turn/steer", {"threadId": assignment["worker_id"], "expectedTurnId": assignment["turn_id"],
                        "clientUserMessageId": "de67-worker:" + request["id"],
                        "input": [{"type": "text", "text": request["message"]}]})
                    self._request_update(request["id"], "delivered")
                    continue
                if request["kind"] == "assign":
                    revision = _validate_packet(self.workspace, Path(assignment["state_path"]), assignment["lineage"],
                                                assignment["task_id"], Path(assignment["packet"]), assignment["digest"])
                    if revision != assignment["assignment_revision"]:
                        raise WorkerLibraryError("Assignment changed after its dispatch was queued")
                    if task["worker_id"]:
                        raise WorkerLibraryError("Task acquired another worker before dispatch")
                    delivered_text, delivery_metadata = self._prepared_delivery(assignment, worker["thread_id"])
                elif assignment["status"] not in {"returned", "failed", "interrupted"}:
                    raise WorkerLibraryError("Worker turn is not safely resumable; inspect its current status")
                elif not owns_assignment(self.workspace, assignment["task_id"], assignment["worker_id"],
                                          self.binding["thread_id"], Path(assignment["state_path"]), assignment["lineage"]):
                    raise WorkerLibraryError("Worker no longer owns this assignment")
                phase = "loading"
                self._update(assignment["id"], status="starting", request_id=request["id"])
                self._request_update(request["id"], "submitting")
                params = {"cwd": str(self.workspace), "model": worker["model"], "approvalPolicy": "never",
                          "sandbox": "danger-full-access", "config": {"model_reasoning_effort": worker["effort"]}}
                if worker["thread_id"]:
                    params.update(threadId=worker["thread_id"], excludeTurns=True)
                thread = self.rpc.call("thread/resume" if worker["thread_id"] else "thread/start", params)["thread"]
                if worker["thread_id"] and thread["id"] != worker["thread_id"]:
                    raise WorkerLibraryError("Resumed worker conversation identity changed")
                if thread.get("cwd") is not None and Path(thread["cwd"]).resolve() != self.workspace:
                    raise WorkerLibraryError("Worker conversation belongs to another workspace")
                if thread.get("status", {}).get("type") == "active":
                    raise WorkerLibraryError("Worker conversation already has active execution")
                if request["kind"] == "assign":
                    revision = _validate_packet(self.workspace, Path(assignment["state_path"]), assignment["lineage"],
                                                assignment["task_id"], Path(assignment["packet"]), assignment["digest"])
                    if revision != assignment["assignment_revision"]:
                        raise WorkerLibraryError("Assignment changed while its worker conversation was loading; prepare its current packet again")
                with closing(_connect(self.workspace, write=True)) as db, db:
                    db.execute("UPDATE workers SET thread_id=? WHERE name=?", (thread["id"], worker["name"]))
                assignment["worker_id"] = thread["id"]
                self._update(assignment["id"], worker_id=thread["id"])
                phase = "claiming"
                from deadline_harness import DeadlineHarness
                with DeadlineHarness(assignment["state_path"]) as harness:
                    supervisor_id = assignment["claim_supervisor_id"] or str(self.binding["supervisor_id"])
                    claimed = harness.claim_worker(assignment["lineage"], assignment["task_id"], thread["id"],
                                                   self.binding["thread_id"], supervisor_id)
                    if claimed["recorded"]:
                        harness.checkpoint_worker(assignment["lineage"], assignment["task_id"], thread["id"],
                                                  "delegated", "Named worker " + worker["name"] + "; request " + request["id"])
                self._update(assignment["id"], claim_supervisor_id=supervisor_id)
                if request["kind"] == "assign":
                    prompt = ("You are named worker " + worker["name"] + ". Your reusable job is: " + worker["job"]
                        + ".\nCurrent assigned task: " + assignment["task_id"] + ". Earlier task instructions are superseded by this assignment.\n"
                        + "Your current prepared assignment is supplied below; use it directly and retrieve missing evidence when it can change the work. "
                        + "Any standing guidance marked unchanged remains from its earlier delivery in this same conversation. "
                        + "Changed standing sections below replace their earlier versions.\n"
                        + "Preserve useful understanding; apply this assignment's current premises and corrections. "
                        + "For communication, the agent_mailbox route to coordinator overrides any native send_message(/root) advice in the brief: "
                        + "native /root is your own worker conversation, not Sol. "
                        + "Your final response is evidence for Sol, not a task-terminal decision.\n\n"
                        + delivered_text)
                    self._audit(assignment, {"method": "de67/workerDelivery/prepared", "params": {
                        "threadId": thread["id"], "requestId": request["id"], **delivery_metadata}})
                else:
                    prompt = request["message"]
                phase = "starting-turn"
                self._update(assignment["id"], status="submitting", turn_id=None, error=None)
                turn = self.rpc.call("turn/start", {"threadId": thread["id"], "effort": worker["effort"],
                    "clientUserMessageId": "de67-worker:" + request["id"],
                    "input": [{"type": "text", "text": prompt + "\nDE67 worker request: " + request["id"]}]})["turn"]
                self._update(assignment["id"], status="running", turn_id=turn["id"])
                self._request_update(request["id"], "submitted")
            except Exception as error:
                uncertain = phase in {"starting-turn", "steering"} and getattr(error, "code", None) is None
                status = "uncertain" if uncertain else "rejected"
                explanation = str(error)
                if uncertain:
                    explanation += "; delivery is uncertain: do not replay this request or start another assignment"
                self._request_update(request["id"], status, explanation)
                if phase != "steering":
                    if phase == "loading" and request["kind"] == "message":
                        # Loading starts no turn. Restore the known previous assignment.
                        self._update(request["assignment_id"], status=assignment["status"],
                                     request_id=assignment["request_id"], error=explanation)
                    else:
                        assignment_status = "failed" if phase == "starting-turn" and not uncertain else status
                        self._update(request["assignment_id"], status=assignment_status, error=explanation)
                self._notice({"name": request["name"]}, "Worker " + request["name"] + " request " + request["id"]
                             + " is " + status + ": " + explanation)

    def observe(self, message: dict[str, Any]) -> bool:
        payload, method = message.get("params", {}), message.get("method")
        worker_id = payload.get("threadId")
        db = _connect(self.workspace)
        if db is None or not worker_id:
            if db is not None:
                db.close()
            return False
        with closing(db):
            row = db.execute("""SELECT * FROM assignments WHERE worker_id=?
                ORDER BY created_at DESC,rowid DESC LIMIT 1""", (worker_id,)).fetchone()
        if row is None or not _same_binding(json.loads(row["binding"]), self.binding):
            return False
        assignment = dict(row)
        if method in {"turn/started", "item/started"}:
            self.inactive_uncertain.discard(assignment["id"])
        if not str(method).endswith(("/delta", "/outputDelta")):
            # Keep worker audit/usage material outside Sol's stream and prompt.
            self._audit(assignment, message)
        turn_id = payload.get("turnId") or payload.get("turn", {}).get("id")
        if assignment["status"] in {"submitting", "uncertain"} and not assignment["turn_id"] and turn_id:
            items = payload.get("turn", {}).get("items", []) + ([payload["item"]] if "item" in payload else [])
            if any(self._matches_request(item, assignment) for item in items):
                self._adopt_turn(assignment, turn_id, "observed userMessage clientId")
        if turn_id != assignment["turn_id"] or not turn_id:
            return True  # A prior turn or ambiguous start cannot settle the current assignment.
        if method == "item/completed":
            item = payload.get("item", {})
            if item.get("type") == "agentMessage" and item.get("phase") in (None, "final", "final_answer") and item.get("text"):
                self.final_messages.setdefault((worker_id, turn_id), {})[item.get("id", "final")] = item["text"]
        elif method == "turn/completed":
            if assignment["status"] not in {"running", "submitting"}:
                return True
            turn = payload["turn"]
            for item in turn.get("items", []):
                if item.get("type") == "agentMessage" and item.get("phase") in (None, "final", "final_answer") and item.get("text"):
                    self.final_messages.setdefault((worker_id, turn_id), {})[item.get("id", "final")] = item["text"]
            text = "\n\n".join(self.final_messages.pop((worker_id, turn_id), {}).values())
            artifact = _root(self.workspace) / "results" / assignment["id"] / (hashlib.sha256(turn_id.encode()).hexdigest() + ".md")
            artifact.parent.mkdir(parents=True, exist_ok=True)
            artifact.write_text(text or "No final worker message was returned.\n", encoding="utf-8")
            status = {"completed": "returned", "failed": "failed", "interrupted": "interrupted"}.get(turn.get("status"), "uncertain")
            self._update(assignment["id"], status=status, result_path=str(artifact),
                         error=json.dumps(turn["error"]) if turn.get("error") else None)
            evidence = {"worker_name": assignment["name"], "task_id": assignment["task_id"], "thread_id": worker_id,
                        "turn_id": turn_id, "status": status, "result_path": str(artifact),
                        "sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(), "terminal_authority": False}
            self._checkpoint(assignment, "worker-return", evidence)
            self._notice(assignment, "Worker " + assignment["name"] + " " + status + " on task " + assignment["task_id"]
                         + ". Result: " + str(artifact) + ". This is nonterminal evidence; inspect it and use the existing receipt/task lifecycle.")
            if status in {"returned", "failed", "interrupted"}:
                # Preserve the durable conversation; release our live subscription so
                # App Server can evict it after its idle grace period. Reuse resumes it.
                try:
                    self.rpc.call("thread/unsubscribe", {"threadId": worker_id})
                except Exception as error:
                    self._audit(assignment, {"method": "de67/workerUnload/failed",
                                "params": {"threadId": worker_id, "error": str(error)}})
        return True

    def has_active_turns(self) -> bool:
        db = _connect(self.workspace)
        if db is None:
            return False
        with closing(db):
            return any(_same_binding(json.loads(row["binding"]), self.binding)
                       and row["id"] not in self.inactive_uncertain
                       for row in db.execute("""SELECT id,binding FROM assignments WHERE worker_id IS NOT NULL
                           AND status IN ('running','submitting','uncertain')"""))

    def shutdown(self, reason: str = "Coordinator App Server stopped") -> None:
        """Call only after the owned server has stopped; preserve conversations and unsettled tasks."""
        db = _connect(self.workspace)
        if db is None:
            return
        with closing(db):
            rows = [dict(row) for row in db.execute("SELECT * FROM assignments")]
        for assignment in rows:
            if not _same_binding(json.loads(assignment["binding"]), self.binding):
                continue
            if assignment["status"] in {"running", "starting", "submitting", "uncertain"}:
                # Server termination ends execution, but cannot prove whether an
                # unreceipted request was accepted. Preserve its request uncertainty.
                status = "interrupted"
                explanation = reason if assignment["turn_id"] else reason + "; previous request outcome remains uncertain; no work was replayed"
                self._update(assignment["id"], status=status, error=explanation)
                if assignment["worker_id"]:
                    self._checkpoint(assignment, "worker-interrupted", {"reason": reason, "status": status,
                                      "turn_id": assignment["turn_id"], "terminal_authority": False})
            elif assignment["status"] == "pending":
                self._update(assignment["id"], status="rejected", error=reason + " before dispatch")
                self._request_update(assignment["request_id"], "rejected", reason + " before dispatch")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    add = commands.add_parser("create")
    add.add_argument("name")
    for flag in ("job", "model", "effort"):
        add.add_argument("--" + flag, required=True)
    commands.add_parser("list")
    for command in ("describe", "status", "retire", "wait"):
        sub = commands.add_parser(command)
        sub.add_argument("name")
        if command == "wait":
            sub.add_argument("--timeout", type=float, default=30)
        if command == "describe":
            sub.add_argument("--job")
            sub.add_argument("--model")
            sub.add_argument("--effort")
    dispatch = commands.add_parser("assign")
    dispatch.add_argument("name")
    dispatch.add_argument("--task", required=True)
    dispatch.add_argument("--packet", type=Path, required=True)
    dispatch.add_argument("--sha256", required=True)
    dispatch.add_argument("--state", type=Path, required=True)
    dispatch.add_argument("--lineage", required=True)
    dispatch.add_argument("--timeout", type=float, default=60)
    send = commands.add_parser("message")
    send.add_argument("name")
    send.add_argument("--message", required=True)
    send.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args(argv)
    try:
        if args.command == "create":
            result = create(args.workspace, args.name, args.job, args.model, args.effort)
        elif args.command == "list":
            result = catalog(args.workspace)
        elif args.command in {"describe", "status"}:
            result = describe(args.workspace, args.name, getattr(args, "job", None),
                              model=getattr(args, "model", None), effort=getattr(args, "effort", None))
        elif args.command == "retire":
            result = retire(args.workspace, args.name)
        elif args.command == "wait":
            result = wait(args.workspace, args.name, args.timeout)
        else:
            queued = (assign(args.workspace, args.name, args.task, args.packet, args.sha256, args.state, args.lineage)
                      if args.command == "assign" else message(args.workspace, args.name, args.message))
            result = wait_request(args.workspace, queued["request_id"], args.timeout)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 2 if result.get("state") in {"rejected", "uncertain"} else 0
    except (WorkerLibraryError, OSError, ValueError, sqlite3.Error) as error:
        print(json.dumps({"state": "rejected", "error": str(error)}))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
