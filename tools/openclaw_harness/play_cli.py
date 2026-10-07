#!/usr/bin/env python3
"""Small persistent player client for an already authorized file-backed cockpit.

This client owns transport bookkeeping, never game state or launch authority.
A pending request is collected, not replayed. Actions use the exact last frame
shown to the player; stale frames remain the native owner's decision.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
from contextlib import contextmanager
import json
import math
import os
import re
import shlex
from pathlib import Path
import sys
import time
from typing import Any
import uuid

from cockpit_archive import ArchiveSequence, json_chunks
from evidence_display import emit, recover
from gameplay_display import display, bounded_player_output, plain_player_output, world_look
from cockpit import player_controls
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge, _atomic_json
from startup_dialog import (observe_startup_dialog, pre_world_startup_phase,
                            recover_startup_debug_dialog)


def _persistent_result_cache(value: Any) -> Any:
    """Project lazy evidence before placing a collected reply in client state.

    ``response_artifact`` intentionally resolves archive references so the
    caller can inspect them.  That live object is not JSON-serializable,
    however, and persisting it verbatim made otherwise accepted ``run.witness``
    and ``run.finish`` replies fail at the PlayerClient state boundary.  Keep
    the public result lazy and durable by retaining the authenticated archive
    reference rather than materializing its history into play-client.json.
    """
    if isinstance(value, ArchiveSequence):
        return {"schema": "caol-archive-sequence-ref-v1", **value.reference()}
    if isinstance(value, dict):
        return {key: _persistent_result_cache(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_persistent_result_cache(item) for item in value]
    if isinstance(value, tuple):
        return [_persistent_result_cache(item) for item in value]
    return value


def _merge_turn_assessments(previous: dict[str, Any] | None, current: dict[str, Any]) -> dict[str, Any]:
    """Retain every new alarm/recovery observed during one public collect call."""
    if previous is None:
        return current
    merged = dict(current)
    for field, identity in (("alarms", "key"), ("recoveries", "recovered_alarm")):
        seen: set[str] = set()
        values: list[dict[str, Any]] = []
        for assessment in (previous, current):
            for item in assessment.get(field, []):
                key = item.get(identity) if isinstance(item, dict) else None
                deduplication_key = str(key) if key is not None else json.dumps(item, sort_keys=True)
                if deduplication_key not in seen:
                    seen.add(deduplication_key)
                    values.append(item)
        merged[field] = values
    return merged


@contextmanager
def session_lock(path: Path):
    # OS-owned locks are released after a crash; a leftover filename is harmless.
    with path.open("a+b") as lock:
        if os.name == "nt":
            import msvcrt
            lock.seek(0, os.SEEK_END)
            if lock.tell() == 0:
                lock.write(b"0")
                lock.flush()
            lock.seek(0)
            try:
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise ValueError("another_play_client_is_active") from error
            try:
                yield
            finally:
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise ValueError("another_play_client_is_active") from error
            try:
                yield
            finally:
                fcntl.flock(lock, fcntl.LOCK_UN)


class PlayerClient:
    def __init__(self, session: Path):
        self.session = session.resolve()
        manifest = json.loads((self.session / "bridge.manifest.json").read_text())
        self.binding = str(manifest.get("binding_id", ""))
        if not self.binding:
            raise ValueError("session_manifest_has_no_binding")
        self.state_path = self.session / "play-client.json"
        self.state = json.loads(self.state_path.read_text()) if self.state_path.exists() else {
            "binding_id": self.binding,
        }
        if self.state.get("binding_id") != self.binding:
            raise ValueError("play_client_binding_changed")
        self.reentered = False
        status_path = self.session / "status.json"
        status = json.loads(status_path.read_text()) if status_path.exists() else {}
        generation = status.get("session_generation", 0)
        terminal_generation = self.state.get("finished_generation", self.state.get("session_generation", 0))
        if ((self.state.get("finished") or self.state.get("sealed_terminal")) and
                status.get("binding_id") == self.binding and status.get("state") == "ready" and
                isinstance(generation, int) and isinstance(terminal_generation, int) and
                generation > terminal_generation):
            for key in ("finished", "finished_generation", "sealed_terminal", "process_exited",
                        "observation_id", "observation_request_id", "observation_generation",
                        "observation_binding_id", "observation_owner", "terminal_observation_id",
                        "operation_availability", "display", "display_sha256", "display_generation",
                        "last_request_id", "last_collected_result",
                        "read_only_observation_request_id"):
                self.state.pop(key, None)
            self.state["session_generation"] = generation
            self.state["reentry_required_generation"] = generation
            self.reentered = True

    def save(self):
        _atomic_json(self.state_path, self.state)

    def display_snapshot(self):
        if isinstance(self.state.get("display"), dict):
            return self.state["display"]
        # Read old sessions without creating another permanent cache copy.
        return recover(self.state["display_sha256"]) if self.state.get("display_sha256") else None

    def _reentry_pending(self) -> bool:
        """Keep a declared reentry boundary across short-lived CLI processes."""
        return self.reentered or self.state.get("reentry_required_generation") == self._current_generation()

    def _evidence_logs(self) -> dict[str, Any]:
        """Describe producer-published logs without observing or changing the live owner."""
        root = Path(__file__).resolve().parents[2]
        status: dict[str, Any] = {}
        try:
            value = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
            if isinstance(value, dict):
                status = value
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            pass
        owner: dict[str, Any] = {}
        try:
            value = json.loads((self.session / "game-process.json").read_text(encoding="utf-8"))
            if isinstance(value, dict):
                owner = value
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            pass
        descriptor = status.get("session_descriptor")
        descriptor_run_id = str(descriptor.get("run_id", "")).strip() if isinstance(descriptor, dict) else ""
        owner_run_id = str(owner.get("run_id", "")).strip()
        run_id = owner_run_id or descriptor_run_id
        if (not owner or not owner_run_id or not str(owner.get("binding_id", "")).strip()
                or not str(status.get("binding_id", "")).strip()):
            identity_status = "unavailable"
        elif (owner.get("binding_id") != self.binding or
              status.get("binding_id") != self.binding):
            identity_status = "binding_mismatch"
        elif not descriptor_run_id:
            identity_status = "unavailable"
        elif descriptor_run_id != owner_run_id:
            identity_status = "run_mismatch"
        else:
            identity_status = "bound"
        published = owner.get("log_paths") if isinstance(owner.get("log_paths"), dict) else {}
        bridge_query = Path(__file__).with_name("cockpit_file_bridge.py")

        def entry(name: str, metadata: Any, default_scope: str) -> dict[str, Any]:
            metadata = metadata if isinstance(metadata, dict) else {}
            raw_path = str(metadata.get("path", "")).strip()
            path = Path(raw_path).expanduser() if raw_path else None
            scope = str(metadata.get("scope", default_scope)).strip() or default_scope
            query = None
            if path is not None and identity_status == "bound":
                query = [sys.executable, str(bridge_query), "log-query", "--path", str(path)]
                if scope == "run_bound":
                    query.extend(("--run-id", run_id))
                query.extend(("--limit", "20"))
            available = path is not None and identity_status == "bound" and path.is_file()
            if identity_status != "bound":
                state = identity_status
            elif path is None:
                state = "unavailable"
            else:
                state = "available" if available else "missing"
            return {
                "name": name, "path": str(path) if path is not None else None,
                "present": path.is_file() if path is not None else False,
                "status": state, "scope": scope, "run_id": run_id or None,
                "binding_id": self.binding, "query": query,
                "query_note": "Read-only retained records. Correlate run_id, timestamps, actor and request; shared logs may contain other runs." if scope != "run_bound" else "The --run-id filter is bound to this session run; records do not by themselves prove gameplay outcomes.",
            }

        entries = [entry(name, published.get(name), "run_bound") for name in (
            "native_semantic_events", "native_semantic_snapshot", "transition_events")]
        entries.append(entry("profile_diagnostic_debug", published.get("profile_diagnostic_debug"), "profile_shared"))
        for name in ("llm_intent.log", "llm_intent_events.log", "llm_intent_runner.log"):
            entries.append(entry("npc_" + name.removesuffix(".log"),
                                 {"path": str(root / "config" / name), "scope": "repository_shared"},
                                 "repository_shared"))
        return {
            "run_id": run_id or None, "binding_id": self.binding, "identity_status": identity_status,
            "metadata_source": "game-process.json log_paths" if published else "missing_game_process_log_metadata",
            "entries": entries,
            "scope_note": "run_bound files come from the launch producer and belong to this run; profile_shared and repository_shared files can contain other runs. Missing metadata is reported without guessing a path.",
            "log_query": "Use each entry's query argv when present, then add --event, --contains, --from-turn/--to-turn, --where FIELD_OPERATOR_JSON, or --select FIELD. Numeric native actor IDs use --where actor_id=4. Use record-artifact with a returned artifact handle for exact bytes.",
        }

    def controls(self) -> dict[str, Any]:
        # Local metadata only: safe while a native request is pending. Do not
        # refresh the frame, enqueue traffic, or save client ownership state.
        logs = self._evidence_logs()
        return {"ok": True, "result": player_controls(self.state.get("operation_availability")),
                "evidence_logs": logs,
                "evidence_tools": {
                    "messages": "messages [--contains TEXT] [--offset N --limit N] reads the displayed frame's native messages as JSON; no game input.",
                    "logs_selector": "evidence_logs",
                    "log_scope": logs["scope_note"],
                    "query": logs["log_query"],
                    "decisions": "evidence --decisions (--operation-id OP | --group-id SCENARIO) [--actor ID ...] [--from-turn TURN --to-turn TURN] [--limit N --offset N --snapshot HASH]; exact source handles and raw --select fields remain available.",
                }}

    def evidence(self, args):
        from evidence_events import query, parse_predicate
        logs = self._evidence_logs()
        if args.decisions:
            from cockpit_evidence import query as log_query
            if args.limit < 1 or args.offset < 0:
                raise ValueError("invalid_decision_page")
            if logs["identity_status"] != "bound":
                raise ValueError("decision_trace_requires_bound_run")
            if args.run_id is not None and args.run_id != logs["run_id"]:
                raise ValueError("decision_trace_run_mismatch")
            if (bool(args.operation_id) == bool(args.group_id) or args.actor_id or args.actor_name or args.request_id or
                    args.event or args.process_instance or args.where or args.contains):
                raise ValueError("decision_trace_requires_operation_id_or_group_id_and_uses_actor_turn_scope")
            native = next((entry for entry in logs["entries"]
                           if entry["name"] == "native_semantic_events"), None)
            if native is None or native["status"] != "available":
                raise ValueError("decision_trace_native_log_unavailable")
            selectors = [part.strip() for value in args.select for part in value.split(",") if part.strip()]
            decision = {"operation_id": args.operation_id, "group_id": args.group_id,
                        "actor_ids": sorted(set(args.actor)),
                        "from_turn": args.from_turn, "to_turn": args.to_turn}
            return log_query([Path(native["path"])], {"run_id": logs["run_id"]}, selectors,
                             args.offset, args.limit, snapshot=args.snapshot, decision=decision)
        if args.operation_id or args.group_id or args.actor or args.offset or args.snapshot:
            raise ValueError("decision_scope_options_require_decisions")
        sources = [{"path": entry["path"], "producer": entry["name"]}
                   for entry in logs["entries"] if entry.get("path") and logs["identity_status"] == "bound"]
        sources.extend({"path": str(path), "producer": "cockpit", "response": True}
                       for path in sorted((self.session / "responses").glob("*.json"))
                       if not path.name.endswith(".receipt.json"))
        filters = {}
        for key in ("actor_id", "actor_name", "request_id", "run_id", "event", "process_instance"):
            value = getattr(args, key)
            if value is not None:
                filters[key] = value
        predicates = [parse_predicate(expression) for expression in args.where]
        if args.limit < 1:
            raise ValueError("evidence_limit_must_be_positive")
        selectors = [part.strip() for value in args.select for part in value.split(",") if part.strip()]
        result = query(sources, filters, args.contains, selectors, args.limit,
                       predicates=predicates, from_turn=args.from_turn, to_turn=args.to_turn)
        result["log_identity_status"] = logs["identity_status"]
        return result

    def performance(self, args):
        from process_performance import (read_json, read_records, sample_owned_session,
                                         compare_records, collect_turn_assessment)
        if args.sample_seconds is not None:
            record = sample_owned_session(self.session, self.binding, args.sample_seconds)
        else:
            record = read_json(self.session / "performance.latest.json")
        if record and record.get("owner", {}).get("binding_id") != self.binding:
            raise ValueError("performance_binding_mismatch")
        assessment = collect_turn_assessment(
            self.session, self.binding, pending=self.state.get("pending"),
        )
        result = {"ok": True, "latest": record or None,
                  "turn_assessment": assessment,
                  "comparison": {"status": "unavailable", "reason": "no baseline selected"},
                  "note": "Read-only game telemetry, independent of pending requests. Native turn assessment reads the run-bound trace without sending input. A session performance-turn-config.json with scenario provenance supplies alarm thresholds; without it measurements stay unassessed. --sample-seconds 1 measures the owned process now. --offset 0 --limit 5 pages exact retained records. --tag labels your workload, not a native fact."}
        if args.offset is not None:
            page = read_records(self.session, args.offset, args.limit)
            if any(item.get("owner", {}).get("binding_id") != self.binding for item in page["records"]):
                raise ValueError("performance_record_binding_mismatch")
            result.update(page)
        if args.baseline:
            result["comparison"] = compare_records(record, read_json(args.baseline), args.tag)
        if args.save_baseline:
            if not record or not args.tag.strip():
                raise ValueError("saving_baseline_requires_a_record_and_explicit_workload_tag")
            with args.save_baseline.open("x") as destination:
                json.dump({"comparison_tag": args.tag, "record": record}, destination)
            result["baseline_saved"] = str(args.save_baseline)
        return result

    def cancel(self, reason: str) -> dict[str, Any]:
        pending = self.state.get("pending")
        if not isinstance(pending, dict) or not pending.get("request_id"):
            return {"ok": False, "error": "no_pending_request", "next": "look"}
        status = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
        run_id = str(status.get("session_descriptor", {}).get("run_id", ""))
        if not run_id:
            try:
                active = json.loads((self.session / "active-request.json").read_text(encoding="utf-8"))
                run_id = str(active.get("run_id", ""))
            except (OSError, json.JSONDecodeError):
                pass
        if not run_id:
            request = pending.get("request", {})
            run_id = str(request.get("run_id", "")) if isinstance(request, dict) else ""
        return Bridge.send_cancel(self.session, request_id=str(pending["request_id"]),
                                  binding_id=self.binding, run_id=run_id, reason=reason)

    def _restore_pending_request(self, request_id: str) -> dict[str, Any]:
        """Rebuild client-side pending state from the durable bridge envelope.

        This is deliberately reconciliation, not a submission retry.  The
        bridge remains the single owner of the request/result record.
        """
        response = Bridge.response_status(self.session, request_id, summary=False)
        if response.get("ok"):
            return self._read_only_recovery(request_id, "request_already_recorded_read_only")
        if response.get("error") != "response_not_available_or_stale":
            return {"ok": False, "error": "request_recovery_response_unresolved",
                    "request_id": request_id, "response_status": response,
                    "next": "inspect retained evidence before deciding whether the request is outstanding"}
        artifact = Bridge.request_artifact(self.session, request_id)
        if not artifact.get("ok"):
            return artifact
        envelope = artifact.get("envelope")
        request = artifact.get("request")
        if not isinstance(envelope, dict) or not isinstance(request, dict):
            return {"ok": False, "error": "request_artifact_is_not_recoverable"}
        envelope_binding = str(envelope.get("binding_id", "")).strip()
        if envelope_binding and envelope_binding != self.binding:
            return {"ok": False, "error": "request_binding_mismatch"}
        self.state["pending"] = {"request_id": request_id, "request": request,
                                 "submitted_unix_seconds": envelope.get("submitted_unix_seconds")}
        self.save()
        return {"ok": True, "request_id": request_id}

    def _read_only_recovery(self, request_id: str, error: str) -> dict[str, Any]:
        """Name the exact retained-result route without changing client state."""
        return {
            "ok": False, "error": error, "request_id": request_id,
            "retrieval": {
                "request_result": f"request-result --request-id {request_id}",
                "response_fields": f"inspect SELECTOR --request-id {request_id}",
            },
            "next": "retrieve this retained result read-only; do not collect or resume it as a new operation",
        }

    def resume(self, request_id: str | None, wait_seconds: float = 0) -> dict[str, Any]:
        """Recover one already-submitted operation after output/client loss."""
        if self.state.get("pending"):
            pending_id = str(self.state["pending"].get("request_id", ""))
            if request_id and request_id != pending_id:
                return {"ok": False, "error": "different_request_is_already_pending",
                        "request_id": pending_id, "next": "collect"}
            return self.collect(wait_seconds)
        if self._reentry_pending():
            if request_id:
                return self._read_only_recovery(request_id, "request_recovery_superseded_by_reentry")
            return self.collect(wait_seconds)
        if self.state.get("finished"):
            if request_id:
                return self._read_only_recovery(request_id, "request_recovery_session_finished")
            return self.collect(wait_seconds)
        requested = request_id or self.state.get("last_request_id")
        if not requested:
            for path in (self.session / "active-request.json", self.session / "status.json"):
                try:
                    value = json.loads(path.read_text(encoding="utf-8"))
                except (OSError, ValueError, json.JSONDecodeError):
                    continue
                candidate = value.get("request_id", value.get("inflight_request_id"))
                if isinstance(candidate, str) and candidate:
                    requested = candidate
                    break
        if not isinstance(requested, str) or not requested:
            return {"ok": False, "error": "resume_request_id_required",
                    "next": "resume --request-id REQUEST_ID"}
        latest = self.state.get("last_request_id")
        if isinstance(latest, str) and latest and requested != latest:
            return self._read_only_recovery(requested, "request_recovery_superseded")
        cached = self.state.get("last_collected_result")
        if requested == latest and isinstance(cached, dict) and cached.get("request_id") == requested:
            return dict(cached)
        if requested == latest:
            return self._read_only_recovery(requested, "latest_collected_result_not_cached")
        restored = self._restore_pending_request(requested)
        if not restored.get("ok"):
            return restored
        result = self.collect(wait_seconds)
        return {**result, "recovered_request": requested}

    def collect(self, wait_seconds: float = 0, request_id: str | None = None) -> dict[str, Any]:
        pending = self.state.get("pending")
        if not pending:
            if self._reentry_pending():
                if request_id:
                    return self._read_only_recovery(request_id, "request_recovery_superseded_by_reentry")
                self.save()
                return {"ok": True, "state": "reentered", "next": "look",
                        "session_generation": self.state["session_generation"],
                        "note": "The declared saved-world continuation is ready. Old frame grants were released; observe its current owner."}
            if self.state.get("finished"):
                if request_id:
                    return self._read_only_recovery(request_id, "request_recovery_session_finished")
                status = json.loads((self.session / "status.json").read_text())
                terminal = status.get("terminalization", {})
                cleanup = terminal.get("cleanup", status.get("cleanup"))
                state = status.get("state")
                failed = state in {"bridge_failed", "terminalization_failed", "process_dead", "reentry_failed"}
                complete = state == "safe_to_cleanup"
                return {"ok": not failed, "state": "finished" if complete else "cleanup_failed" if failed else "finishing",
                        "bridge_state": state, "cleanup": cleanup,
                        "cleanup_pending": not complete and not failed,
                        "bridge_cleanup": status.get("cleanup"), "terminalization": terminal,
                        "reason": status.get("reason", status.get("error")),
                        **({"reentry_failure": status["reentry_failure"]}
                           if isinstance(status.get("reentry_failure"), dict) else {}),
                        "next": "inspect retained evidence" if complete or failed else "collect",
                        "note": "Cleanup disposition is reported by the scenario owner; it is separate from native exit or save proof."}
            requested = request_id or self.state.get("last_request_id")
            cached = self.state.get("last_collected_result")
            if isinstance(requested, str) and isinstance(cached, dict) and \
                    cached.get("request_id") == requested:
                # Collection is a read of an immutable receipt, not a consume
                # operation.  Return the original decision response verbatim.
                return dict(cached)
            if isinstance(requested, str) and requested:
                latest = self.state.get("last_request_id")
                if isinstance(latest, str) and latest and requested != latest:
                    return self._read_only_recovery(requested, "request_recovery_superseded")
                if requested == latest:
                    return self._read_only_recovery(requested, "latest_collected_result_not_cached")
                restored = self._restore_pending_request(requested)
                if not restored.get("ok"):
                    return restored
                return self.collect(wait_seconds, request_id=requested)
            return {"ok": False, "error": "no_pending_request", "next": "look"}
        pending_request_id = pending["request_id"]
        if request_id is not None and request_id != pending_request_id:
            return {"ok": False, "error": "different_request_is_already_pending",
                    "request_id": pending_request_id, "next": "collect"}
        request_id = pending_request_id
        deadline = time.monotonic() + max(0, wait_seconds)
        terminal_states = {"process_dead", "bridge_failed", "terminalization_failed",
                           "reentry_failed", "safe_to_cleanup"}

        def turn_assessment() -> dict[str, Any]:
            from process_performance import collect_turn_assessment
            return collect_turn_assessment(self.session, self.binding, pending={
                "request_id": request_id,
                "action": pending.get("request", {}).get("action"),
                "action_id": pending.get("request", {}).get("action_id"),
                "submitted_unix_seconds": pending.get("submitted_unix_seconds"),
            })

        def terminal_failure(status: dict[str, Any]) -> dict[str, Any]:
            return {"ok": False, "state": "session_ended_without_response",
                    "error": "bridge_ended_before_response", "request_id": request_id,
                    "bridge_state": status.get("state"),
                    "reason": status.get("reason", status.get("error", "cockpit child exited")),
                    "child_exit_code": status.get("child_exit_code"),
                    "log_path": str(self.session / "child.stderr.log"),
                    "next": "Inspect the failure and retained evidence; this request will not be replayed."}

        def cancellation_requested() -> bool:
            controls = self.session / "controls"
            if not controls.is_dir():
                return False
            for marker in controls.glob("cancel-*.json"):
                try:
                    value = json.loads(marker.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    continue
                if value.get("request_id") == request_id and value.get("binding_id") == self.binding:
                    return True
            return False

        assessment: dict[str, Any] | None = None
        while True:
            result = Bridge.response_status(self.session, request_id)
            assessment = _merge_turn_assessments(assessment, turn_assessment())
            if result.get("ok") or result.get("error") != "response_not_available_or_stale":
                break
            # Wake on owner death rather than sleeping until the requested
            # deadline. The pending request remains owned and is never replayed.
            try:
                status = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                status = {}
            if status.get("state") in terminal_states or status.get("child_exit_code") is not None:
                return {**terminal_failure(status), "turn_assessment": assessment}
            # Cancellation is an out-of-band cooperative wake. The bridge may
            # still be producing the original response, so retain pending state
            # and let the next collect retrieve its exact receipt.
            if cancellation_requested():
                return {"ok": True, "state": "cancellation_requested", "request_id": request_id,
                        "bridge_state": status.get("state"), "next": "collect",
                        "turn_assessment": assessment,
                        "note": "Cancellation was requested for this submission; collect the original request for its terminal receipt."}
            if assessment.get("alarms"):
                # Publish a newly observed simulation alarm promptly. The
                # request and its frame ownership remain pending, unchanged;
                # only its authenticated terminal receipt can complete it.
                return {"ok": True, "state": "pending", "request_id": request_id,
                        "bridge_state": status.get("state"), "next": "collect",
                        "turn_assessment": assessment, "performance_alarm": True,
                        "note": "New simulation alarm; inspect play performance, then collect this same request. No action was replayed or cancelled."}
            if time.monotonic() >= deadline:
                return {"ok": True, "state": "pending", "request_id": request_id,
                        "bridge_state": status.get("state"), "next": "collect",
                        "turn_assessment": assessment,
                        "note": "The request was submitted once. Collect its response; do not repeat the action."}
            time.sleep(min(0.05, max(0, deadline - time.monotonic())))
        if not result.get("ok"):
            return result  # Preserve unresolved ownership when evidence is unavailable/corrupt.
        if result["receipt"].get("binding_id") != self.binding:
            return {"ok": False, "error": "response_binding_mismatch"}
        full = Bridge.response_artifact(self.session, request_id, result["receipt"]["response_sha256"])
        if not full.get("ok"):
            return full
        response = full["response"]
        receipt_generation = result["receipt"].get("session_generation", 0)
        try:
            current_status = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            current_status = {}
        current_generation = current_status.get("session_generation", 0)
        generation_matches = receipt_generation == current_generation
        binding_matches = current_status.get("binding_id") == self.binding
        if isinstance(response.get("failure"), dict) and response["failure"].get("unused_authority") == "revoked" or \
                response.get("error") == "player_cancelled" or response.get("reason") == "player_cancelled":
            # Cancellation revokes the displayed frame. Require a fresh look
            # before the player can choose another action.
            self.state.pop("observation_id", None)
            self.state.pop("observation_request_id", None)
            self.state.pop("observation_generation", None)
            self.state.pop("observation_binding_id", None)
            self.state.pop("observation_owner", None)
            self.state["cancellation"] = {
                "request_id": request_id,
                "reason": response.get("reason", response.get("error")),
                "action_outcome": response.get("failure", {}).get("detail", {}).get(
                    "action_outcome", response.get("action_outcome", "unknown")),
            }
        if isinstance(response.get("operation_availability"), dict):
            self.state["operation_availability"] = response["operation_availability"]
        observation = response.get("observation", response.get("result", {}))
        if isinstance(observation, dict) and not isinstance(observation.get("surface"), dict):
            observation = observation.get("terminal_observation", observation)
        surface = observation.get("surface") if isinstance(observation, dict) else None
        macro_result = response.get("result") or {}
        released_decision = (isinstance(macro_result, dict) and
                             macro_result.get("state") == "interrupted" and
                             macro_result.get("unused_authority") == "released" and
                             macro_result.get("session_state") == "active" and
                             isinstance(observation, dict) and
                             observation.get("continuation", {}).get("state") == "released")
        # A stopped macro may return ok=False while explicitly handing its
        # existing native decision back to the player. Preserve that grant;
        # ordinary rejected or revoked responses still require a fresh look.
        reusable = ((response.get("ok") is True or released_decision) and generation_matches and binding_matches and
                    isinstance(observation, dict) and bool(observation.get("observation_id")) and
                    isinstance(surface, dict) and bool(str(surface.get("kind", "")).strip()) and
                    isinstance(surface.get("actions"), list) and
                    surface.get("kind") != "process_exited")
        if reusable:
            self.state["observation_id"] = observation["observation_id"]
            self.state["observation_request_id"] = request_id
            self.state["observation_generation"] = receipt_generation
            self.state["observation_binding_id"] = self.binding
            self.state["observation_owner"] = surface["kind"]
        else:
            # A rejected, factless, stale-generation, or terminal result never
            # grants authority for a successor action. The caller must look or
            # follow the supported terminal/recovery route.
            self.state.pop("observation_id", None)
            self.state.pop("observation_request_id", None)
            self.state.pop("observation_generation", None)
            self.state.pop("observation_binding_id", None)
            self.state.pop("observation_owner", None)
            if isinstance(observation, dict) and observation.get("surface", {}).get("kind") == "process_exited":
                self.state["process_exited"] = True
                if observation.get("observation_id"):
                    self.state["terminal_observation_id"] = observation["observation_id"]
        if response.get("ok") and pending["request"]["action"] == "run.witness":
            self.state["sealed_terminal"] = {
                key: pending["request"][key]
                for key in ("observation_id", "stop_reason", "unused_authority")
            }
        terminal_request = response.get("ok") and pending["request"]["action"] in {"run.finish", "run.quit"}
        terminal = response.get("result")
        confirmed_terminal = (isinstance(terminal, dict)
                              and terminal.get("schema") == "caol-cockpit-live-final-v1"
                              and terminal.get("state") == "finished")
        if terminal_request and confirmed_terminal:
            self.state["finished"] = True
            self.state["finished_generation"] = result["receipt"].get("session_generation", 0)
            self.state.pop("observation_id", None)
        try:
            previous = self.display_snapshot()
        except (OSError, ValueError):
            previous = None  # A lost presentation cache never strands native grants.
        view, display_state = display(response, previous, refresh=pending["request"]["action"] == "game.observe" or self._reentry_pending() or self.state.get("display_generation") != result["receipt"].get("session_generation", 0))
        self.state["display_generation"] = receipt_generation
        if display_state is not None:
            self.state["display"] = display_state
            self.state.pop("display_sha256", None)
        world_controls_unchanged = False
        current = (display_state or {}).get("current", {})
        world_observation = pending["request"]["action"] == "game.observe" and (display_state or {}).get("owner") == "world"
        if world_observation:
            controls_key = sha256(json.dumps([receipt_generation, current.get("actions", []),
                                      self.state.get("operation_availability")], sort_keys=True).encode()).hexdigest()
            world_controls_unchanged = self.state.get("shown_world_controls") == controls_key
            self.state["shown_world_controls"] = controls_key
        result = {**result, "response": view}
        result.pop("retrieval", None)
        self.state["last_request_id"] = request_id
        if pending["request"].get("action") == "game.observe" and reusable:
            self.state.pop("reentry_required_generation", None)
            self.reentered = False
        self.state.pop("read_only_observation_request_id", None)
        self.state.pop("pending", None)
        self.save()
        output = {**result, "ok": response.get("ok") is True,
                  "world_controls_unchanged": world_controls_unchanged,
                  "world_observation": world_observation,
                  "turn_assessment": assessment,
                  "state": "collected" if response.get("ok") else "rejected", "request_id": request_id,
                  "next": "collect" if self.state.get("finished") else
                          ("finish --witness FILE" if self.state.get("process_exited") else
                           "status; preserve the sealed journal and live game; native exit is unverified") if self.state.get("sealed_terminal") else
                          "journal --reason REASON" if self.state.get("process_exited") else
                          "act, look, inspect, evidence, messages, or status" if self.state.get("observation_id") else "look"}
        if self.state.get("process_exited") and not self.state.get("finished"):
            output["state"] = "process_exited"
        if self.state.get("sealed_terminal") and not self.state.get("finished"):
            output["witness_fields"] = {
                "citation_paths": "Checks are relative to entry.value, not the whole response. Observation checks start value.surface.facts. Inspect the actual journal entry first; do not guess paths.",
                "value_types": "Copy exact JSON values from the journal: false differs from the string \"false\", and 0 differs from \"0\". Do not coerce native string facts.",
                "verdict": "proved | contradicted | inconclusive",
                "smallest_supported_claim": "Your conclusion limited to cited facts",
                "causal_account": "Why the observed facts support that conclusion",
                "citations": [{"citation_id": "J... from journal entries", "meaning": "What this establishes",
                               "checks": {"value.surface.facts.FIELD": "Exact observed value; nested JSON paths supported"}}],
                "recommended_disposition": "accept | continue | repair | change-strategy",
                "contradictions": "List every supplied contradiction with its citation_id and meaning",
                "remaining_unknowns": "Optional list of unresolved questions",
                "evidence_ceiling": "Optional: omit this field to use the sealed journal's evidence_ceiling. If supplied, copy that exact value; this is not a prose explanation.",
            }
        if terminal_request and confirmed_terminal:
            output.update(state="finishing", cleanup_pending=True,
                          note="Finish accepted; collect reports scenario cleanup. Native save/exit evidence remains separate.")
        if terminal_request and not confirmed_terminal:
            output.update(ok=False, state="unconfirmed_terminal_response",
                          error="explicit_finish_or_quit_has_no_terminal_receipt",
                          next="quit --reason REASON or inspect retained evidence" if self.state.get("sealed_terminal") else "look",
                          note="The reply did not establish session termination. The client remains available and does not replay the request.")
        self.state["last_collected_result"] = _persistent_result_cache(output)
        self.save()
        return output

    def submit(self, request: dict[str, Any], wait_seconds: float, *, repeat_internal: bool = False) -> dict[str, Any]:
        repeat = self.state.get("repeat")
        if (isinstance(repeat, dict) and repeat.get("status") in {"running", "pending"}
                and not repeat_internal and request.get("action") != "run.quit"):
            return {"ok": False, "error": "repeat_in_progress_use_repeat_resume_or_abort"}
        if self.state.get("pending"):
            return {"ok": False, "error": "request_in_flight", "next": "collect",
                    "request_id": self.state["pending"]["request_id"]}
        if self.state.get("finished"):
            return {"ok": False, "error": "session_already_finished"}
        request_id = "play-" + uuid.uuid4().hex
        if repeat_internal and request.get("action") == "game.act":
            outstanding = repeat.get("outstanding") if isinstance(repeat, dict) else None
            if not isinstance(outstanding, dict) or outstanding.get("request_id"):
                raise ValueError("repeat_outstanding_request_invalid")
            outstanding["request_id"] = request_id
        self.state["pending"] = {"request_id": request_id, "request": request,
                                 "submitted_unix_seconds": time.time()}
        # Persist ownership before submission: even a crash cannot cause replay.
        previous_frame = self.state.pop("observation_id", None)
        previous_frame_metadata = {
            key: self.state.pop(key, None)
            for key in ("observation_request_id", "observation_generation",
                        "observation_binding_id", "observation_owner")
        }
        # Consuming a frame revokes its input grant, not its immutable
        # displayed evidence.  Keep that request identity for read-only
        # messages/inspection while this operation is pending.
        if previous_frame_metadata.get("observation_request_id"):
            self.state["read_only_observation_request_id"] = \
                previous_frame_metadata["observation_request_id"]
        self.save()
        result = Bridge.send_request(self.session, request_id=request_id,
                                     binding_id=self.binding, request=request)
        if not result.get("ok"):
            self.state.pop("pending", None)
            if repeat_internal and isinstance(repeat, dict) and request.get("action") == "game.act":
                repeat["outstanding"] = None
            if previous_frame:
                self.state["observation_id"] = previous_frame
                self.state.update({key: value for key, value in previous_frame_metadata.items()
                                   if value is not None})
            self.save()
            return result
        return self.collect(wait_seconds)

    def frame(self, *, terminal: bool = False) -> str:
        if self.state.get("pending"):
            raise ValueError("request_in_flight: use collect")
        if self.state.get("sealed_terminal"):
            raise ValueError("journal_is_sealed: immutable historical seal; verify native exit before reporting; preserve any live game and consult its owner")
        frame = self.state.get("terminal_observation_id") if terminal else self.state.get("observation_id")
        if terminal:
            if not frame or not self.state.get("process_exited"):
                raise ValueError("no_terminal_observation: look or collect")
            return str(frame)
        if not frame:
            raise ValueError("look_required: no current unconsumed observation")
        if (self.state.get("observation_binding_id") != self.binding or
                self._current_binding() != self.binding or
                self.state.get("observation_generation") != self._current_generation() or
                not self.state.get("observation_owner")):
            raise ValueError("look_required: current observation authority is stale")
        return str(frame)

    def _current_generation(self) -> int:
        try:
            status = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return -1
        generation = status.get("session_generation", 0)
        return generation if isinstance(generation, int) and not isinstance(generation, bool) else -1

    def _current_binding(self) -> str:
        try:
            status = json.loads((self.session / "status.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return ""
        return str(status.get("binding_id", "")).strip()

    def act(self, action: str, target: str | None, parameters: dict[str, str], wait_seconds: float,
            *, repeat_internal: bool = False):
        if self.state.get("process_exited"):
            return {"ok": False, "error": "game_process_exited", "next": "journal --reason REASON"}
        request: dict[str, Any] = {"action": "game.act", "action_id": action,
                                   "observation_id": self.frame()}
        if target is not None:
            request["stable_id"] = target
        if parameters:
            request["parameters"] = parameters
        return self.submit(request, wait_seconds, repeat_internal=repeat_internal)

    def trade_letters(self, letters: str, wait_seconds: float):
        """One native selection batch, authenticated against this current Trade frame."""
        from cockpit_evidence import decode
        frame = self.frame()
        snapshot = self.display_snapshot() or {}
        current = snapshot.get("current", {})
        facts = current.get("facts", {})
        if snapshot.get("owner") != "inventory" or facts.get("selection_source") != "trade_selector::to_use":
            return {"ok": False, "error": "current_owner_is_not_trade"}
        if current.get("observation_id") != frame:
            return {"ok": False, "error": "stale_trade_rows"}
        rows = decode(facts.get("trade_rows"))
        if not isinstance(rows, list) or not letters:
            return {"ok": False, "error": "trade_letters_unavailable"}
        if len(set(letters)) != len(letters):
            return {"ok": False, "error": "duplicate_trade_letter"}
        for letter in letters:
            matching = [row for row in rows if isinstance(row, dict) and row.get("letter") == letter]
            if len(matching) != 1 or matching[0].get("enabled") is not True:
                return {"ok": False, "error": "unavailable_or_ambiguous_trade_letter", "letter": letter}
            if matching[0].get("party") != facts.get("active_party") or matching[0].get("actor_id") != facts.get("active_actor_id"):
                return {"ok": False, "error": "foreign_trade_pane"}
        return self.act("trade.toggle_letters", None, {"letters": letters,
                        "party": facts["active_party"], "actor_id": facts["active_actor_id"]}, wait_seconds)

    @staticmethod
    def _repeat_native_state(snapshot: dict[str, Any] | None) -> dict[str, Any]:
        snapshot = snapshot if isinstance(snapshot, dict) else {}
        facts = snapshot.get("current", {}).get("facts", {})
        status = facts.get("avatar_status", {}) if isinstance(facts, dict) else {}
        parts = status.get("health", {}).get("body_parts", {}) if isinstance(status, dict) else {}
        health = {key: value.get("current") for key, value in parts.items()
                  if isinstance(value, dict) and type(value.get("current")) is int} if isinstance(parts, dict) else {}
        moves = snapshot.get("avatar_moves", facts.get("avatar_moves") if isinstance(facts, dict) else None)
        return {"owner": snapshot.get("owner"), "turn": snapshot.get("game_turn"),
                "moves": moves if type(moves) is int else None, "health": health}

    def _repeat_result(self) -> dict[str, Any]:
        repeat = self.state["repeat"]
        outstanding = repeat.get("outstanding")
        result = {"schema": "caol-play-count-repeat-v1",
                  "ok": repeat["status"] != "failed", "state": repeat["status"],
                  "action_id": repeat["action_id"], "requested_count": repeat["requested_count"],
                  "completed_count": repeat["completed_count"],
                  "unused_count": repeat["requested_count"] - repeat["completed_count"],
                  "stop_reason": repeat.get("stop_reason"),
                  "first_native_turn": repeat.get("first_native_turn"),
                  "last_native_turn": repeat.get("last_native_turn"),
                  "current_owner": repeat.get("current_owner"),
                  "receipt_handles": list(repeat.get("receipt_handles", [])),
                  "outstanding_request_id": outstanding.get("request_id") if isinstance(outstanding, dict) else None}
        if repeat["status"] == "pending":
            result["next"] = "collect, then repeat --resume; never press the action again"
        elif repeat["status"] == "running":
            result["next"] = "repeat --resume"
        return result

    def _repeat_finish(self, status: str, reason: str) -> dict[str, Any]:
        repeat = self.state["repeat"]
        repeat.update(status=status, stop_reason=reason)
        self.save()
        return self._repeat_result()

    def repeat(self, action: str | None, count: int | None, wait_seconds: float,
               *, resume: bool = False, abort: bool = False) -> dict[str, Any]:
        allowed = {"world.pause", "world.autoattack"}
        if not resume and not abort and (action not in allowed or type(count) is not int or count <= 0):
            return {"ok": False, "error": "repeat_requires_pause_or_autoattack_and_positive_count"}
        repeat = self.state.get("repeat")
        if abort:
            if not isinstance(repeat, dict) or repeat.get("status") not in {"running", "pending"}:
                return {"ok": False, "error": "no_active_repeat"}
            if self.state.get("pending") or repeat.get("outstanding"):
                return {"ok": False, "error": "repeat_request_outstanding_collect_or_cancel_first",
                        "request_id": (repeat.get("outstanding") or {}).get("request_id")}
            return self._repeat_finish("interrupted", "user_aborted")
        if resume:
            if action is not None or count is not None or not isinstance(repeat, dict):
                return {"ok": False, "error": "repeat_resume_requires_existing_operation"}
            if repeat.get("status") not in {"running", "pending"}:
                return self._repeat_result()
        else:
            if isinstance(repeat, dict) and repeat.get("status") in {"running", "pending"}:
                return {"ok": False, "error": "repeat_in_progress_use_repeat_resume_or_abort",
                        "repeat": self._repeat_result()}
            repeat = {"schema": "caol-play-count-repeat-state-v1", "action_id": action,
                      "requested_count": count, "completed_count": 0, "status": "running",
                      "stop_reason": None, "first_native_turn": None, "last_native_turn": None,
                      "current_owner": None, "receipt_handles": [], "outstanding": None}
            self.state["repeat"] = repeat
            self.save()
        if self.state.get("finished") or self.state.get("process_exited"):
            return self._repeat_finish("interrupted", "terminal_session")
        while repeat["completed_count"] < repeat["requested_count"]:
            outstanding = repeat.get("outstanding")
            if isinstance(outstanding, dict):
                request_id = outstanding.get("request_id")
                if not isinstance(request_id, str) or not request_id:
                    return self._repeat_finish("failed", "outstanding_request_identity_missing")
                cached = self.state.get("last_collected_result")
                if isinstance(cached, dict) and cached.get("request_id") == request_id:
                    result = cached
                elif (self.state.get("pending") or {}).get("request_id") == request_id:
                    result = self.collect(wait_seconds, request_id)
                else:
                    return self._repeat_finish("failed", "exact_response_unavailable")
                if result.get("state") in {"pending", "cancellation_requested"}:
                    repeat["status"] = "pending"
                    self.save()
                    return self._repeat_result()
                repeat["status"] = "running"
                bridge_receipt = result.get("receipt", {})
                native = (result.get("response", {}).get("outcome", {}).get("native_receipt", {})
                          if isinstance(result.get("response"), dict) else {})
                repeat["receipt_handles"].append({
                    "request_id": request_id,
                    "response_sha256": bridge_receipt.get("response_sha256"),
                    "native_action_id": native.get("action_id"),
                    "native_accepted": native.get("accepted"),
                })
                repeat["outstanding"] = None
                after = self._repeat_native_state(self.display_snapshot())
                repeat["current_owner"] = after["owner"]
                if type(after["turn"]) is int:
                    repeat["last_native_turn"] = after["turn"]
                before = outstanding["before"]
                if not result.get("ok") or result.get("state") not in {"collected", "process_exited"}:
                    return self._repeat_finish("interrupted", "native_rejected_or_cancelled")
                if native.get("accepted") is not True or native.get("action_id") != repeat["action_id"]:
                    return self._repeat_finish("failed", "native_receipt_rejected_or_mismatched")
                progressed = ((type(before.get("turn")) is int and type(after["turn"]) is int and
                               after["turn"] > before["turn"]) or
                              (type(before.get("moves")) is int and type(after["moves"]) is int and
                               after["moves"] < before["moves"]))
                if not progressed:
                    return self._repeat_finish("failed", "native_action_progress_unproved")
                repeat["completed_count"] += 1
                self.save()
                if after["owner"] != "world":
                    return self._repeat_finish("interrupted", "owner_changed")
                if not before.get("health") or not after["health"] or before["health"].keys() != after["health"].keys():
                    return self._repeat_finish("interrupted", "avatar_health_unavailable")
                if any(after["health"][part] < hp for part, hp in before["health"].items()):
                    return self._repeat_finish("interrupted", "avatar_harm")
                if result.get("state") == "process_exited":
                    return self._repeat_finish("interrupted", "terminal_session")
                if repeat["completed_count"] == repeat["requested_count"]:
                    return self._repeat_finish("completed", "count_reached")
                continue
            pending = self.state.get("pending")
            if isinstance(pending, dict):
                if pending.get("request", {}).get("action") != "game.observe":
                    return self._repeat_finish("failed", "unrelated_request_pending")
                observed = self.collect(wait_seconds, pending.get("request_id"))
                if observed.get("state") in {"pending", "cancellation_requested"}:
                    repeat["status"] = "pending"
                    self.save()
                    return self._repeat_result()
                if not observed.get("ok"):
                    return self._repeat_finish("failed", "world_observation_failed")
            if not self.state.get("observation_id"):
                observed = self.submit({"action": "game.observe"}, wait_seconds, repeat_internal=True)
                if observed.get("state") in {"pending", "cancellation_requested"}:
                    repeat["status"] = "pending"
                    self.save()
                    return self._repeat_result()
                if not observed.get("ok"):
                    return self._repeat_finish("failed", "world_observation_failed")
            try:
                self.frame()
            except ValueError:
                return self._repeat_finish("failed", "stale_world_authority")
            snapshot = self.display_snapshot()
            before = self._repeat_native_state(snapshot)
            repeat["current_owner"] = before["owner"]
            if before["owner"] != "world":
                return self._repeat_finish("interrupted", "owner_changed")
            offered = [item for item in snapshot.get("current", {}).get("actions", [])
                       if item.get("id") == repeat["action_id"] and item.get("enabled") is True
                       and not item.get("stable_id")]
            if len(offered) != 1:
                return self._repeat_finish("interrupted", "action_not_advertised")
            if repeat["first_native_turn"] is None and type(before["turn"]) is int:
                repeat["first_native_turn"] = before["turn"]
            repeat["outstanding"] = {"request_id": None, "before": before}
            self.save()
            sent = self.act(repeat["action_id"], None, {}, wait_seconds, repeat_internal=True)
            if not isinstance(repeat.get("outstanding"), dict):
                return self._repeat_finish("failed", "request_submission_failed")
            if sent.get("state") in {"pending", "cancellation_requested"}:
                repeat["status"] = "pending"
                self.save()
                return self._repeat_result()
            # The collected reply is cached before the next press. Process it
            # through the exact outstanding request on the next loop iteration.
        return self._repeat_finish("completed", "count_reached")

    def call(self, request: dict[str, Any], wait_seconds: float):
        if isinstance(request, dict) and request.get("action") == "run.quit":
            return self.submit(request, wait_seconds)
        if not isinstance(request, dict) or not isinstance(request.get("action"), str) or not request["action"].strip().lower().startswith("game."):
            raise ValueError("call_requires_a_structured_game_request")
        if self.state.get("sealed_terminal"):
            raise ValueError("journal_is_sealed: immutable historical seal; verify native exit before reporting; preserve any live game and consult its owner")
        if self.state.get("process_exited") and request["action"].strip().lower() not in {"game.observe", "game.look"}:
            return {"ok": False, "error": "game_process_exited", "next": "journal --reason REASON"}
        # Keep recipes and their types intact. The service owns operation
        # authorization and validation; this client only owns transport.
        return self.submit(request, wait_seconds)

    def wait(self, *, target_game_minutes: float | None, target_delta_game_minutes: float | None,
             duration_action: str, bound_maximum: float, bound_basis: str, bound_source: str,
             danger_handling: str, wait_seconds: float):
        """Submit a player-chosen bounded wait without a hand-written JSON file."""
        wait: dict[str, Any] = {
            "enabled": True,
            "recipe": ["world.wait", duration_action],
            "bound": {"basis": bound_basis, "source": bound_source,
                      "unit": "game_minutes", "maximum": bound_maximum,
                      "progress_required": True},
            "danger_handling": danger_handling,
        }
        if target_game_minutes is not None:
            wait["target_game_minutes"] = target_game_minutes
        else:
            wait["target_delta_game_minutes"] = target_delta_game_minutes
        return self.submit({"action": "game.wait", "wait": wait}, wait_seconds)

    def move_relative(self, *, east: int, south: int, bound_maximum: int,
                      bound_basis: str, bound_source: str, danger_handling: str,
                      wait_seconds: float):
        """Submit a chosen relative movement intent without a request file."""
        return self.submit({"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [east, south], "danger_handling": danger_handling,
            "bound": {"basis": bound_basis, "source": bound_source,
                      "unit": "steps", "maximum": bound_maximum},
        }}, wait_seconds)

    def _reporting_exit_refusal(self):
        if not self.state.get("process_exited"):
            return {"ok": False, "error": "native_exit_required_before_reporting",
                    "next": "Use inspect/evidence/messages/status for live evidence. When authorized, use the current advertised native save/quit action and look to observe exit. No reporting request submitted."}
        return None

    def journal(self, reason: str, unused: str, wait_seconds: float):
        refusal = self._reporting_exit_refusal()
        if refusal is not None:
            return refusal
        return self.submit({"action": "run.witness", "observation_id": self.frame(terminal=self.state.get("process_exited", False)),
                            "stop_reason": reason, "unused_authority": unused}, wait_seconds)

    def finish(self, witness: dict[str, Any], wait_seconds: float):
        refusal = self._reporting_exit_refusal()
        if refusal is not None:
            return refusal
        if isinstance(witness, dict):
            statements = [claim.get("statement") for claim in witness.get("claims", [])
                          if isinstance(claim, dict)] if witness.get("schema") == \
                "caol-playtest-witness-bundle-v1" else [witness]
            for statement in statements if isinstance(statements, list) else []:
                for citation in statement.get("citations", []) if isinstance(statement, dict) else []:
                    checks = citation.get("checks", {}) if isinstance(citation, dict) else {}
                    for path, value in checks.items() if isinstance(checks, dict) else []:
                        if path.endswith(".last_save_checkpoint") and not isinstance(value, str):
                            return {"ok": False, "error": "witness_checkpoint_requires_string",
                                    "path": path, "supplied_type": type(value).__name__,
                                    "expected_shape": {"type": "string", "example":
                                        json.dumps({"confirmed_turn": 5248280}, separators=(",", ":"))},
                                    "next": "Copy the exact STRING from the cited journal row; no request submitted."}
        terminal = self.state.get("sealed_terminal")
        if not terminal:
            return {"ok": False, "error": "journal_required_before_finish",
                    "next": "journal --reason REASON; inspect result.evidence_journal.entries --limit 10"}
        return self.submit({"action": "run.finish", **terminal, "witness": witness}, wait_seconds)

    def messages(self, offset: int | None, limit: int, contains: str | None):
        from cockpit_evidence import select
        request_id = self.state.get("observation_request_id") or \
            self.state.get("read_only_observation_request_id")
        if not request_id:
            return {"ok": False, "error": "no_displayed_observation", "next": "look or collect"}
        status = Bridge.response_status(self.session, request_id, summary=False)
        if not status.get("ok"):
            return status
        receipt = status["receipt"]
        if receipt.get("binding_id") != self.binding:
            return {"ok": False, "error": "response_binding_mismatch"}
        result = Bridge.response_artifact(self.session, request_id, receipt["response_sha256"])
        if not result.get("ok"):
            return result
        response = result["response"]
        from cockpit_evidence import resolve_surface_selector
        resolved = resolve_surface_selector(response, "surface.facts.messages")
        if not resolved["ok"]:
            return {"ok": False, "error": resolved["reason"],
                    "candidate_selectors": resolved["candidate_selectors"]}
        selector = resolved["resolved_selector"]
        base = selector.rsplit(".surface", 1)[0] if ".surface" in selector else ""
        observation = select(response, base) if base else response
        try:
            messages = select(response, selector)
        except KeyError:
            return {"ok": False, "error": "messages_not_available_on_current_owner",
                    "owner": observation.get("surface", {}).get("kind"),
                    "next": "Inspect this owner's facts or return to World when appropriate."}
        if not isinstance(messages, list) or limit <= 0 or (offset is not None and offset < 0):
            raise ValueError("invalid_message_page")
        if offset is None:
            count = sum(contains is None or contains.casefold() in json.dumps(message, ensure_ascii=False).casefold()
                        for message in messages)
            offset = max(0, count - limit)
        page = Bridge.response_slice(self.session, request_id, selector, offset, limit, contains)
        return {**page, "observation_id": observation.get("observation_id"),
                "scope": "Native messages in the displayed frame, including any retained fixture history. Compare the time, actor and action before attributing a message."}

    def inspect(self, selector: str, offset: int, limit: int | None, contains: str | None,
                request_id: str | None = None, selected_fields: list[str] | None = None,
                view: str | None = None, source_role: str = "observation"):
        request_id = request_id or self.state.get("last_request_id")
        if not request_id:
            return {"ok": False, "error": "no_collected_response", "next": "look or collect"}
        receipt = Bridge.response_status(self.session, request_id, summary=False)
        if receipt.get("ok") and receipt["receipt"].get("binding_id") != self.binding:
            return {"ok": False, "error": "response_binding_mismatch"}
        if view == "trade":
            return Bridge.response_slice(self.session, request_id, selector or "surface.facts.trade_rows",
                                         offset, limit if limit is not None else 8, contains)
        if selected_fields or view:
            if not receipt.get("ok"):
                return receipt
            verified = Bridge.response_artifact(self.session, request_id, receipt["receipt"]["response_sha256"])
            if not verified.get("ok"):
                return verified
            from cockpit_evidence import recursive_view, selected_view
            if view:
                projection = recursive_view(verified["response"], selector, view,
                                            offset=offset, limit=limit if limit is not None else 20,
                                            contains=contains, source_role=source_role)
                return {"ok": projection["ok"], "request_id": request_id,
                        "response_sha256": receipt["receipt"]["response_sha256"],
                        "projection": projection}
            paths = [selector + "." + field if selector else field
                     for fields in selected_fields for field in fields.split(",") if field]
            return {"ok": True, "request_id": request_id,
                    "response_sha256": receipt["receipt"]["response_sha256"],
                    "projection": selected_view(verified["response"], paths)}
        return Bridge.response_slice(self.session, request_id, selector, offset, limit, contains)

    def compare(self, before_request_id: str, after_request_id: str, selectors: list[str]):
        """Read-only comparison of two retained response artifacts."""
        return Bridge.response_compare(self.session, before_request_id, after_request_id, selectors)

    def request_result(self, request_id: str):
        """Retrieve the recorded request/result link without sending input."""
        return Bridge.request_result(self.session, request_id)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    entered_argv = list(argv)
    # Short spelling only: feed the original parser and dispatch unchanged.
    if "wait" in argv:
        index = argv.index("wait")
        if index + 1 < len(argv) and re.fullmatch(r"[1-9][0-9]*[mh]", argv[index + 1]):
            duration = argv[index + 1]
            minutes = int(duration[:-1]) * (60 if duration.endswith("h") else 1)
            mode = "ignore"
            end = index + 2
            if end < len(argv) and argv[end] in {"ignore", "safe", "stop"}:
                mode = argv[end]
                end += 1
            argv[index + 1:end] = [
                "--target-delta-game-minutes", str(minutes), "--duration-action", "wait." + duration,
                "--bound-maximum", str(minutes), "--bound-basis", "game_mechanic",
                "--bound-source", "Player requested " + duration, "--danger-handling",
                {"ignore": "ignore_danger_and_interruptions", "safe": "handle_classified_non_dangerous",
                 "stop": "stop_on_interruption"}[mode]]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--diagnostics", action="store_true",
                        help="Full transport JSON; use ordinary output or targeted inspect/evidence for routine decisions")
    parser.add_argument("--session", type=Path, default=os.environ.get("CAOL_PLAY_SESSION"),
                        required=not os.environ.get("CAOL_PLAY_SESSION"),
                        help="Existing registry-launched session directory; defaults to CAOL_PLAY_SESSION")
    parser.add_argument("--wait-seconds", type=float, default=1,
                        help="Wait for this response, then return pending; never resubmit")
    commands = parser.add_subparsers(dest="command", required=True)
    look = commands.add_parser("look", help="Observe the current input owner and its legal actions")
    look.add_argument("--map", action="store_true", help="Add a compact current avatar-visible local text map")
    debug_ignore = commands.add_parser("debug-ignore", help="Send one Ignore to a currently captured startup native debug dialog")
    debug_ignore.add_argument("--capture", required=True, help="SHA-256 from the current play look window or terminal capture")
    debug_ignore.add_argument("--note", default="",
                              help="Optional consequence note; known warning handling is in debug-errors.md")
    for answer in ("yes", "no", "ignore"):
        commands.add_parser(answer, help="Choose the matching advertised prompt option")
    commands.add_parser("stop", help="Use the existing activity.pause action")
    collect = commands.add_parser("collect", help="Collect an outstanding or already-collected response without replaying input")
    collect.add_argument("--request-id", help="Exact latest retained request; older results stay read-only")
    collect.add_argument("--map", action="store_true", help="Show the local map when the collected owner is World")
    resume = commands.add_parser("resume", help="Rebuild one genuinely outstanding request; never resubmits")
    resume.add_argument("--request-id", help="Required when no active/last request identity is retained")
    cancel = commands.add_parser("cancel", help="Request cooperative cancellation of the outstanding request")
    cancel.add_argument("--reason", default="player_cancelled")
    quit_command = commands.add_parser("quit", help="Native close guidance; process termination requires explicit --abort")
    quit_command.add_argument("--reason", default="player requested quit")
    quit_command.add_argument("--abort", action="store_true",
                              help="Explicitly terminate the owned process; no native save or normal-exit credit")
    messages = commands.add_parser("messages", help="Read native messages from the displayed observation; latest matching page by default")
    messages.add_argument("--contains")
    messages.add_argument("--offset", type=int)
    messages.add_argument("--limit", type=int, default=5)
    act = commands.add_parser("act", help="Act on the last displayed frame; native authority remains unchanged")
    act.add_argument("action")
    act.add_argument("--target", help="Exact advertised stable ID")
    act.add_argument("--param", action="append", default=[], metavar="KEY=VALUE")
    trade = commands.add_parser("trade", help="Toggle current Trade item letters together; never commit")
    trade.add_argument("letters", help="Exact current native letters, e.g. ab; invalid/duplicate letters reject the whole batch")
    repeat = commands.add_parser("repeat", help="Repeat native pause or autoattack by exact receipt count")
    repeat.add_argument("action", nargs="?", help="world.pause or world.autoattack")
    repeat.add_argument("--count", type=int, help="Positive native action budget")
    repeat_mode = repeat.add_mutually_exclusive_group()
    repeat_mode.add_argument("--resume", action="store_true", help="Collect the exact pending step and continue")
    repeat_mode.add_argument("--abort", action="store_true", help="Stop a repeat with no outstanding request")
    wait = commands.add_parser(
        "wait", help="Wait in game (bare duration uses ignore mode)",
        description="Short form: wait 5m [ignore|safe|stop]. Choose the mode by what the test must observe.",
        epilog=("If a stopped prompt is independently known to be harmless, use its currently "
                "advertised IGNORE choice (short command: ignore). The game remembers that "
                "distraction type for the "
                "current activity and its backlog. Reobserve before waiting again. "
                "ignore wait mode: deliberately continues through supported danger and damage prompts. "
                "safe: automatically ignores recognized typed activity distractions, including "
                "conversation, thirst and weather changes, then finishes the remaining wait. "
                "It stops for near hostiles, pain, attacks, actual damage, and untyped or "
                "unknown distractions. "
                "stop: deliberately stops at each interruption."),
    )
    target = wait.add_mutually_exclusive_group(required=True)
    target.add_argument("--target-game-minutes", type=float)
    target.add_argument("--target-delta-game-minutes", type=float)
    wait.add_argument("--duration-action", required=True,
                      help="Exact advertised native duration action, for example wait.5m")
    wait.add_argument("--bound-maximum", required=True, type=float,
                      help="Evidence-derived maximum game-minute allowance")
    wait.add_argument("--bound-basis", required=True,
                      choices=("game_mechanic", "scheduler_boundary", "path_progress", "measured_rate"))
    wait.add_argument("--bound-source", required=True,
                      help="Why this bound is appropriate for this chosen wait")
    wait.add_argument("--danger-handling", default="stop_on_interruption",
                      help="safe returns to inspect danger/damage; ignore continues through combat; stop checks each interruption",
                      choices=("stop_on_interruption", "handle_classified_non_dangerous",
                               "ignore_danger_and_interruptions"))
    move = commands.add_parser("move", help="Submit bounded relative movement without a JSON request file")
    move.add_argument("--east", type=int, default=0, help="Chosen relative map-square offset east/west")
    move.add_argument("--south", type=int, default=0, help="Chosen relative map-square offset south/north")
    move.add_argument("--bound-maximum", required=True, type=int,
                      help="Evidence-derived maximum native step allowance")
    move.add_argument("--bound-basis", required=True,
                      choices=("game_mechanic", "scheduler_boundary", "path_progress", "measured_rate"))
    move.add_argument("--bound-source", required=True,
                      help="Why this bound is appropriate for this chosen move")
    move.add_argument("--danger-handling", default="stop_on_interruption",
                      choices=("stop_on_interruption", "handle_classified_non_dangerous",
                               "ignore_danger_and_interruptions"))
    performance = commands.add_parser("performance", help="Read CPU/RSS/action intervals, including while a request is pending")
    performance.add_argument("--sample-seconds", type=float, help="Measure a fresh owned-process CPU interval without bridge input")
    performance.add_argument("--offset", type=int, help="Page exact retained records from this zero-based offset")
    performance.add_argument("--limit", type=int, default=5)
    performance.add_argument("--tag", default="", help="Explicit comparable-workload annotation; never a native fact")
    performance.add_argument("--baseline", type=Path, help="Compare latest/sample with a previously saved tagged record")
    performance.add_argument("--save-baseline", type=Path, help="Save latest/sample as a tagged baseline; refuses overwrite")
    evidence = commands.add_parser("evidence", help="Query one immutable event snapshot across native, NPC and runner sources")
    for field in ("actor-id", "actor-name", "request-id", "run-id", "event", "process-instance"):
        evidence.add_argument("--" + field)
    evidence.add_argument("--where", action="append", default=[], metavar="FIELD_OPERATOR_JSON",
                          help="Exact field predicate: =, !=, >, >=, <, <= and a JSON value; quote shell comparisons")
    evidence.add_argument("--select", action="append", default=[], help="Exact envelope field path; repeat or separate fields with commas")
    evidence.add_argument("--contains")
    evidence.add_argument("--limit", type=int, default=20)
    evidence.add_argument("--decisions", action="store_true", help="Compact source-handled NPC and group decision rows")
    evidence.add_argument("--operation-id", help="Exact hostile operation for --decisions")
    evidence.add_argument("--group-id", help="Exact scenario trace group for --decisions")
    evidence.add_argument("--actor", action="append", type=int, default=[], help="Selected actor ID; repeat for --decisions")
    evidence.add_argument("--from-turn", type=int)
    evidence.add_argument("--to-turn", type=int)
    evidence.add_argument("--offset", type=int, default=0, help="Decision row offset; pair with --snapshot for stable paging")
    evidence.add_argument("--snapshot", help="Snapshot digest from an earlier decision page")
    compare = commands.add_parser("compare", aliases=["response-compare"], help="Compare selected fields from two retained responses")
    compare.add_argument("--before-request-id", "--before", dest="before_request_id", required=True)
    compare.add_argument("--after-request-id", "--after", dest="after_request_id", required=True)
    compare.add_argument("--select", action="append", default=[],
                         help="Exact response field path; repeat or separate fields with commas")
    request_result = commands.add_parser("request-result", help="Retrieve a recorded request and its verified result")
    request_result.add_argument("--request-id", required=True)
    commands.add_parser("controls", help="Read wait/movement request examples, permissions and interruption behavior without sending input")
    call = commands.add_parser("call", help="Submit an existing structured game.* request; service authorization still applies")
    call.add_argument("--request", type=Path, required=True,
                      help="JSON request object, including action and its existing recipe; no defaults are invented")
    inspect = commands.add_parser("inspect", help="Read a field of the last retained response")
    inspect.add_argument("selector", nargs="?", default="",
                         help="surface.facts is relative to the sole retained surface; explicit result/observation paths stay exact")
    inspect.add_argument("--select", action="append", default=[],
                         help="Selected subfields of selector (or full response paths); read-only with exact response binding")
    inspect.add_argument("--view", choices=["items", "actors", "trade"],
                         help="Compact items/actors, or native Trade groups (default 8 rows); exact source retained")
    inspect.add_argument("--source-role", choices=["observation", "startup", "final", "unknown"],
                         default="observation", help="Explicit source label; never inferred as final from a path")
    inspect.add_argument("--offset", type=int, default=0)
    inspect.add_argument("--limit", type=int)
    inspect.add_argument("--contains")
    inspect.add_argument("--request-id", help="Inspect an earlier retained response without sending input")
    journal = commands.add_parser("journal", help="Terminal seal after native exit; live evidence uses inspect/evidence/messages/status")
    journal.add_argument("--reason", required=True)
    journal.add_argument("--unused-authority", default="released")
    finish = commands.add_parser("finish", help="Submit your witness against the sealed journal")
    finish.add_argument("--witness", type=Path, required=True)
    # argparse accepts global options before a subcommand only.  Keep the
    # same destination and default when a caller places this display choice
    # after any subcommand; absent subparser options must not reset a true
    # global --diagnostics parsed earlier.
    for subparser in dict.fromkeys(commands.choices.values()):
        subparser.add_argument("--diagnostics", action="store_true", default=argparse.SUPPRESS,
                               help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        if not math.isfinite(args.wait_seconds) or args.wait_seconds < 0:
            raise ValueError("wait_seconds_must_be_finite_and_nonnegative")
        if args.command == "evidence":
            result = PlayerClient(args.session).evidence(args)
        elif args.command in {"compare", "response-compare"}:
            selectors = [part.strip() for value in args.select for part in value.split(",") if part.strip()]
            result = PlayerClient(args.session).compare(args.before_request_id, args.after_request_id, selectors)
        elif args.command == "request-result":
            result = PlayerClient(args.session).request_result(args.request_id)
        elif args.command == "performance":
            # Independent telemetry must remain readable while another CLI is
            # waiting with the request-ownership lock. It never edits that state.
            result = PlayerClient(args.session).performance(args)
        elif args.command == "cancel":
            # Cancellation is an out-of-band control and must remain usable
            # while the submitting client owns the request lock.
            result = PlayerClient(args.session).cancel(args.reason)
        elif args.command in {"controls", "messages", "inspect"}:
            # These only read retained state/response artifacts.  Keep them
            # useful while another process owns the blocking collect lock.
            client = PlayerClient(args.session)
            if args.command == "controls":
                result = client.controls()
            elif args.command == "messages":
                result = client.messages(args.offset, args.limit, args.contains)
            else:
                result = client.inspect(args.selector, args.offset, args.limit,
                                        args.contains, args.request_id, args.select, args.view, args.source_role)
        else:
            with session_lock(args.session / "play-client.lock"):
                client = PlayerClient(args.session)
                if args.command == "look":
                    if client.state.get("sealed_terminal"):
                        status = json.loads((args.session / "status.json").read_text(encoding="utf-8"))
                        if (status.get("binding_id") != client.binding or
                                pre_world_startup_phase(status) != "declared_reentry"):
                            raise ValueError("journal_is_sealed: immutable historical seal; verify native exit before reporting; preserve any live game and consult its owner")
                        # The prior journal stays sealed until a new World owner
                        # publishes its descriptor. This is a read-only view of
                        # the replacement process, never a game.observe request.
                        result = {"ok": False, "state": "startup_awaiting_descriptor",
                                  "status": status, "next": "look"}
                    else:
                        result = client.submit({"action": "game.observe"}, args.wait_seconds)
                elif args.command == "debug-ignore":
                    result = recover_startup_debug_dialog(args.session, client.binding,
                                                          args.capture, args.note)
                elif args.command == "collect":
                    result = client.collect(args.wait_seconds, args.request_id)
                elif args.command == "resume":
                    result = client.resume(args.request_id, args.wait_seconds)
                elif args.command == "act":
                    params = {}
                    for item in args.param:
                        key, separator, value = item.partition("=")
                        if not separator or not key or key in params:
                            raise ValueError("parameters_need_unique_KEY=VALUE")
                        params[key] = value
                    result = client.act(args.action, args.target, params, args.wait_seconds)
                elif args.command == "trade":
                    result = client.trade_letters(args.letters, args.wait_seconds)
                elif args.command == "repeat":
                    result = client.repeat(args.action, args.count, args.wait_seconds,
                                           resume=args.resume, abort=args.abort)
                elif args.command == "stop":
                    result = client.act("activity.pause", None, {}, args.wait_seconds)
                elif args.command in {"yes", "no", "ignore"}:
                    client.frame()
                    snapshot = client.display_snapshot()
                    choices = [action for action in snapshot.get("current", {}).get("actions", [])
                               if action.get("id") == "prompt.choose" and action.get("enabled", True)
                               and str(action.get("label", "")).strip().casefold() == args.command]
                    if len(choices) != 1:
                        raise ValueError("The current prompt does not offer " + args.command.upper())
                    result = client.act("prompt.choose", choices[0].get("stable_id"), {}, args.wait_seconds)
                elif args.command == "wait":
                    result = client.wait(target_game_minutes=args.target_game_minutes,
                                         target_delta_game_minutes=args.target_delta_game_minutes,
                                         duration_action=args.duration_action,
                                         bound_maximum=args.bound_maximum,
                                         bound_basis=args.bound_basis,
                                         bound_source=args.bound_source,
                                         danger_handling=args.danger_handling,
                                         wait_seconds=args.wait_seconds)
                elif args.command == "move":
                    result = client.move_relative(east=args.east, south=args.south,
                                                  bound_maximum=args.bound_maximum,
                                                  bound_basis=args.bound_basis,
                                                  bound_source=args.bound_source,
                                                  danger_handling=args.danger_handling,
                                                  wait_seconds=args.wait_seconds)
                elif args.command == "quit":
                    result = client.submit({"action": "run.quit", "stop_reason": args.reason,
                                            "abort": args.abort}, args.wait_seconds)
                elif args.command == "call":
                    result = client.call(json.loads(args.request.read_text()), args.wait_seconds)
                elif args.command == "journal":
                    result = client.journal(args.reason, args.unused_authority, args.wait_seconds)
                else:
                    result = client.finish(json.loads(args.witness.read_text()), args.wait_seconds)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {"ok": False, "error": str(error)}
    if args.command == "look" and "client" in locals() and isinstance(result.get("status"), dict):
        status = result["status"]
        if pre_world_startup_phase(status) is not None:
            # The bridge cannot publish a native input owner before World.
            # This read-only UI/log view neither submits a game action nor
            # grants recovery input authority.
            result["startup_dialog"] = observe_startup_dialog(args.session, client.binding, status)
    if not args.diagnostics:
        # Complete responses and receipts remain in the session; ordinary play
        # should not spend its display budget on integrity bookkeeping.
        snapshot = None
        operation_availability = None
        if (result.get("state") in {"collected", "rejected"}
                and result.get("response", {}).get("current_input", {}).get("owner")):
            try:
                snapshot = client.display_snapshot()
                operation_availability = client.state.get("operation_availability")
            except (OSError, ValueError, KeyError):
                pass  # The existing projected reply remains usable without the presentation cache.
        startup_error = None
        status = result.get("status")
        if isinstance(status, dict) and status.get("state") in {"process_dead", "bridge_failed", "reentry_failed", "terminalization_failed"}:
            try:
                for line in reversed((args.session / "child.stderr.log").read_text(encoding="utf-8").splitlines()):
                    if line.startswith("Cannot proceed: "):
                        startup_error = line.removeprefix("Cannot proceed: ")
                        break
                    try:
                        failure = json.loads(line)
                    except ValueError:
                        continue
                    if isinstance(failure, dict) and failure.get("ok") is False and failure.get("error"):
                        startup_error = failure["error"]
                        break
            except OSError:
                pass
        show_local_map = getattr(args, "map", False)
        text = plain_player_output(result, snapshot=snapshot,
                                   full_look=args.command == "look" or result.get("world_observation", False) or show_local_map,
                                   startup_error=startup_error, operation_availability=operation_availability,
                                   inspection_request_id=getattr(args, "request_id", None) if args.command == "inspect" else None,
                                   show_local_map=show_local_map)
        if args.command == "look" and show_local_map and result.get("state") == "pending":
            text += "\nLocal map when ready → play collect --map"
        if args.command == "controls":
            try:
                retained = client.display_snapshot()
            except (OSError, ValueError):
                retained = None
            if retained and retained.get("owner") == "world":
                text += "\n\n" + world_look(retained, client.state.get("operation_availability"), controls_only=True)
        print(text)
        if args.session.is_dir():
            entered = entered_argv[entered_argv.index(args.command):]
            with (args.session / "playtest.txt").open("a", encoding="utf-8") as transcript:
                transcript.write("> play " + shlex.join(entered) + "\n" + text + "\n\n")
    else:
        emit(result)
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
