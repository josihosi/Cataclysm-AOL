#!/usr/bin/env python3
"""File-backed transport for a worker-owned live cockpit session.

The bridge, rather than the command that starts it, owns the cockpit process.
Requests travel through a private FIFO.  Every complete JSONL response is
written to a session artifact before a compact correlated receipt is exposed.
This keeps a large native observation out of an execution tool's stdout pipe.
"""
from __future__ import annotations

from cockpit_archive import ArchiveSequence, is_sequence, json_chunks, resolve_wire

import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import threading
import time
from typing import Any, Mapping, Sequence

try:
    from . import cockpit_evidence
except ImportError:
    import cockpit_evidence


SCHEMA = "caol-cockpit-file-bridge-v1"
TERMINALIZATION_SIGNAL = "cockpit.bridge.safe_to_cleanup.json"


def _digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _atomic_json(path: Path, value: Mapping[str, Any]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def _request_identity(request_bytes: bytes) -> dict[str, str]:
    """Retain only the public authorization tuple for a bridged request.

    The response hash detects replacement, but it cannot show whether a
    rejected action named the observation that was actually advertised.  Keep
    that causal identity beside the hash without retaining arbitrary request
    payload or any native implementation detail.
    """
    try:
        request = json.loads(request_bytes)
    except (TypeError, ValueError, json.JSONDecodeError):
        return {}
    if not isinstance(request, Mapping):
        return {}
    return {
        key: str(request.get(key, ""))
        for key in ("action", "observation_id", "action_id")
        if str(request.get(key, "")).strip()
    }


class FileBackedCockpitBridge:
    """Own one cockpit child and correlate durable requests to retained replies."""

    def __init__(self, session_dir: Path, command: Sequence[str], *, binding_id: str,
                 authorization_token_id: str = "",
                 require_session_ready: bool = False,
                 pre_descriptor_prefix: Sequence[Mapping[str, Any]] = (),
                 session_reentries: int = 0) -> None:
        self.session_dir = Path(session_dir)
        self.command = [str(part) for part in command]
        self.binding_id = str(binding_id).strip()
        self.authorization_token_id = str(authorization_token_id).strip()
        self.require_session_ready = require_session_ready
        self.pre_descriptor_prefix = tuple(dict(item) for item in pre_descriptor_prefix)
        self.session_reentries = session_reentries
        self.input_path = self.session_dir / "requests.fifo"
        self.status_path = self.session_dir / "status.json"
        self.requests_dir = self.session_dir / "requests"
        self.responses_dir = self.session_dir / "responses"
        self.controls_dir = self.session_dir / "controls"
        self.active_request_path = self.session_dir / "active-request.json"
        self._seen: set[str] = set()
        self._spooled_request_ids: set[str] = set()
        self._sequence = 0
        self._session_generation = 0
        self._child: subprocess.Popen[str] | None = None
        self._child_stderr = None
        self._bootstrap_run_id = ""
        self._terminal_request_id = ""
        self._startup_progress: dict[str, Any] = {}
        self._startup_failure: dict[str, Any] = {}
        self._active_session_descriptor: dict[str, Any] = {}

    def _bootstrap_receipt(self, *, sequence: int, stage: Mapping[str, Any],
                           descriptor: Mapping[str, Any]) -> None:
        """Preserve setup-only proof without letting it become gameplay credit."""
        receipt = {
            "schema": "caol-cockpit-pre-descriptor-receipt-v1",
            "sequence": sequence,
            "binding_id": self.binding_id,
            "run_id": self._bootstrap_run_id,
            "stage": dict(stage),
            "semantic_session": dict(descriptor),
            "evidence_effect": "setup_or_diagnostic_only",
            "gameplay_credit": False,
        }
        with (self.session_dir / "pre_descriptor.receipts.jsonl").open("a", encoding="utf-8") as sink:
            sink.write(json.dumps(receipt, ensure_ascii=False, sort_keys=True) + "\n")

    def _consume_pre_descriptor(self, envelope: Mapping[str, Any], sequence: int) -> bool:
        """Accept one declared adaptive setup stage and nothing resembling public proof."""
        if sequence >= len(self.pre_descriptor_prefix):
            return False
        descriptor = envelope.get("semantic_session")
        if not isinstance(descriptor, Mapping) or descriptor.get("schema") != "caol-adaptive-semantic-session-v1":
            raise ValueError("undeclared_pre_descriptor_output")
        expected = self.pre_descriptor_prefix[sequence]
        required_actions = expected.get("required_action_chain")
        interrupts = expected.get("adaptive_interrupt_actions", [])
        if not isinstance(required_actions, list) or not isinstance(interrupts, list):
            raise ValueError("malformed_pre_descriptor_prefix")
        if descriptor.get("objective") != expected.get("objective") or \
                descriptor.get("required_action_chain") != required_actions or \
                descriptor.get("adaptive_interrupt_actions") != interrupts:
            raise ValueError("undeclared_pre_descriptor_action")
        if descriptor.get("gameplay_credit") not in (None, False) or \
                envelope.get("gameplay_credit") not in (None, False):
            raise ValueError("pre_descriptor_credit_firewall")
        run_id = str(descriptor.get("run_id", "")).strip()
        if not run_id:
            raise ValueError("wrong_run_pre_descriptor")
        if self._bootstrap_run_id and self._bootstrap_run_id != run_id:
            raise ValueError("wrong_run_pre_descriptor")
        self._bootstrap_run_id = run_id
        self._bootstrap_receipt(sequence=sequence + 1, stage=expected, descriptor=descriptor)
        self._write_status("preparing", bootstrap_receipts=sequence + 1,
                           bootstrap_gameplay_credit=False)
        return True

    def _status(self, state: str, **extra: Any) -> dict[str, Any]:
        return {
            "schema": SCHEMA,
            "binding_id": self.binding_id,
            "bridge_pid": os.getpid(),
            "state": state,
            "request_count": self._sequence,
            "session_generation": self._session_generation,
            **({"startup_failure": self._startup_failure} if self._startup_failure else {}),
            **extra,
        }

    def _write_status(self, state: str, **extra: Any) -> None:
        _atomic_json(self.status_path, self._status(state, **extra))

    def _termination_signal(self, signum: int, _frame: Any) -> None:
        raise SystemExit(128 + signum)

    def _emergency_cleanup(self, *, explicit_quit: bool = False) -> dict[str, Any]:
        """Reap the failed controller; preserve the game unless the player quits."""
        cleanup: dict[str, Any] = {"native_exit_credit": False}
        if self._child is not None and self._child.poll() is None:
            self._child.terminate()
            try:
                self._child.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._child.kill()
                self._child.wait(timeout=2)
        # Progress is diagnostic, not process ownership authority.
        game_pid = 0
        from startup_harness import cleanup_game_process, pid_command
        ownership_path = self.session_dir / "game-process.json"
        if ownership_path.exists():
            ownership = json.loads(ownership_path.read_text(encoding="utf-8"))
            owned_pid = int(ownership.get("pid", 0) or 0)
            if ownership.get("binding_id") == self.binding_id and owned_pid > 0 and \
                    ownership.get("command") and pid_command(owned_pid) == ownership["command"]:
                game_pid = owned_pid
            else:
                game_pid = 0
                cleanup["ownership"] = "process_exited_or_identity_changed"
        else:
            cleanup["ownership"] = "unconfirmed_missing_process_record"
        if game_pid:
            cleanup["game"] = cleanup_game_process(game_pid, explicit_quit=explicit_quit)
        self._close_child_streams()
        return cleanup

    def _close_child_streams(self) -> None:
        if self._child is not None:
            if self._child.stdin is not None:
                try:
                    self._child.stdin.close()
                except BrokenPipeError:
                    pass
            if self._child.stdout is not None:
                self._child.stdout.close()
        if self._child_stderr is not None:
            self._child_stderr.close()
            self._child_stderr = None

    def prepare(self) -> None:
        if self.session_dir.exists():
            raise ValueError("bridge session directory already exists")
        if not self.command or not self.binding_id:
            raise ValueError("bridge needs a cockpit command and binding identity")
        self.session_dir.mkdir(parents=True)
        self.requests_dir.mkdir()
        self.responses_dir.mkdir()
        self.controls_dir.mkdir()
        os.mkfifo(self.input_path, 0o600)
        _atomic_json(self.session_dir / "bridge.manifest.json", {
            "schema": SCHEMA,
            "binding_id": self.binding_id,
            "authorization_token_id": self.authorization_token_id,
            "command": self.command,
            "input_channel": self.input_path.name,
            "request_directory": self.requests_dir.name,
            "response_directory": self.responses_dir.name,
        })
        self._write_status("starting")

    def _persist_response(self, request_id: str, request_bytes: bytes, response_bytes: bytes) -> dict[str, Any]:
        self._sequence += 1
        artifact = self.responses_dir / (request_id + ".json")
        if artifact.exists():
            raise ValueError("stale_response_identity")
        artifact.write_bytes(response_bytes)
        receipt = {
            "schema": SCHEMA,
            "request_id": request_id,
            "sequence": self._sequence,
            "session_generation": self._session_generation,
            "binding_id": self.binding_id,
            "request_sha256": _digest(request_bytes),
            "request_identity": _request_identity(request_bytes),
            "response_sha256": _digest(response_bytes),
            "response_artifact": str(artifact.relative_to(self.session_dir)),
        }
        response = json.loads(response_bytes)
        terminal = response.get("result") if isinstance(response, Mapping) else None
        if not isinstance(terminal, Mapping) and isinstance(response, Mapping):
            terminal = response.get("final")
        is_terminal = response.get("ok") is True and \
            receipt["request_identity"].get("action") in {"run.finish", "run.quit"} and \
            isinstance(terminal, Mapping) and terminal.get("schema") == "caol-cockpit-live-final-v1" and \
            terminal.get("state") == "finished"
        terminal_action = receipt["request_identity"].get("action")
        next_state = ("transitioning" if self.session_reentries and terminal_action == "run.finish"
                      else "terminalizing") if is_terminal else "ready"
        # Publish admission state before making the response collectible. A
        # client may submit its next action immediately after seeing a receipt.
        # Final responses must never briefly reopen ordinary admission.
        self._write_status(
            next_state,
            last_response=receipt,
            **({"session_descriptor": self._active_session_descriptor}
               if self._active_session_descriptor else {}),
        )
        _atomic_json(self.responses_dir / (request_id + ".receipt.json"), receipt)
        return receipt

    def _read_complete_response(self) -> str:
        """Read one complete JSON value, including a pretty-printed startup descriptor."""
        assert self._child is not None and self._child.stdout is not None
        parts: list[str] = []
        while True:
            line = self._child.stdout.readline()
            if not line:
                return ""
            parts.append(line)
            candidate = "".join(parts)
            try:
                json.loads(candidate)
            except json.JSONDecodeError:
                continue
            return candidate

    def _record_startup_output(self, response: str) -> None:
        """Retain each child startup envelope before deciding whether to admit it.

        The bridge owns child stdout while it waits for the live-session
        descriptor.  A canonical launch rejection is therefore evidence, not
        disposable transport noise: without this record a child that returns
        an ordinary structured rejection is misreported as merely missing its
        descriptor.
        """
        with (self.session_dir / "child.startup.stdout.jsonl").open("a", encoding="utf-8") as sink:
            sink.write(response)
            if not response.endswith("\n"):
                sink.write("\n")

    def _session_descriptor(self, ready: str) -> Mapping[str, Any]:
        """Accept exactly one bound live-session descriptor before public input."""
        try:
            envelope = json.loads(ready)
        except json.JSONDecodeError as exc:
            raise ValueError("malformed_cockpit_session_descriptor") from exc
        descriptor = envelope.get("cockpit_live_session") if isinstance(envelope, Mapping) else None
        if not isinstance(descriptor, Mapping):
            raise ValueError("missing_cockpit_session_descriptor")
        if descriptor.get("schema") != "caol-cockpit-live-session-v1" or \
                descriptor.get("entry_mode") != "cockpit_live_session":
            raise ValueError("legacy_or_wrong_cockpit_session_descriptor")
        run_id = str(descriptor.get("run_id", "")).strip()
        if not run_id or (self._bootstrap_run_id and self._bootstrap_run_id != run_id):
            raise ValueError("wrong_run_cockpit_session_descriptor")
        if not str(descriptor.get("binding_id", "")).strip() or \
                descriptor.get("bridge_binding_id") != self.binding_id:
            raise ValueError("wrong_binding_cockpit_session_descriptor")
        return descriptor

    def _consume_startup_progress( self, envelope: Mapping[str, Any] ) -> bool:
        """Record a bound, zero-credit HUD transition before live entry."""
        progress = envelope.get( "cockpit_bridge_progress" )
        if not isinstance( progress, Mapping ):
            return False
        if progress.get( "schema" ) != "caol-cockpit-bridge-progress-v1" or \
                progress.get( "state" ) != "startup_hud_ready" or \
                progress.get( "bridge_binding_id" ) != self.binding_id or \
                progress.get( "gameplay_credit" ) is not False:
            raise ValueError( "invalid_cockpit_startup_progress" )
        if not str( progress.get( "run_id", "" ) ).strip() or \
                not str( progress.get( "pid", "" ) ).strip():
            raise ValueError( "incomplete_cockpit_startup_progress" )
        self._startup_progress = dict( progress )
        self._write_status( "preparing", child_pid=self._child.pid if self._child else 0,
                            startup_progress=self._startup_progress,
                            gameplay_credit=False )
        return True

    def _await_session_descriptor(self, *, consume_pre_descriptor_prefix: bool = True) -> Mapping[str, Any]:
        """Run only the declared zero-credit prefix before admitting public input."""
        prefix_index = 0
        while True:
            ready = self._read_complete_response()
            if not ready:
                raise ValueError("pre_descriptor_no_progress")
            self._record_startup_output(ready)
            try:
                envelope = json.loads(ready)
            except json.JSONDecodeError as exc:
                raise ValueError("malformed_pre_descriptor_output") from exc
            if not isinstance(envelope, Mapping):
                raise ValueError("malformed_pre_descriptor_output")
            if envelope.get("ok") is False:
                startup = envelope.get("startup")
                startup = startup if isinstance(startup, Mapping) else envelope
                preflight = startup.get("contract_preflight", {})
                preflight = preflight if isinstance(preflight, Mapping) else {}
                audit = preflight.get("installed_save_audit", {})
                self._startup_failure = {
                    "reason": startup.get("reason", envelope.get("reason", envelope.get("error", "child_startup_failed"))),
                    "preflight_status": preflight.get("status"),
                    "artifact": "child.startup.stdout.jsonl",
                }
                if isinstance(audit, Mapping):
                    self._startup_failure["installed_save_audit"] = {
                        key: audit[key] for key in ("status", "missing_traits", "missing_needs",
                                                   "observed_forbidden_traits") if key in audit
                    }
                raise ValueError(str(self._startup_failure["reason"]))
            if self._consume_startup_progress( envelope ):
                continue
            if consume_pre_descriptor_prefix and prefix_index < len(self.pre_descriptor_prefix):
                if not self._consume_pre_descriptor(envelope, prefix_index):
                    raise ValueError("missing_declared_pre_descriptor_stage")
                prefix_index += 1
                continue
            descriptor = self._session_descriptor(ready)
            return descriptor

    def _handle_request(self, line: str) -> bool:
        try:
            envelope = json.loads(line)
            if envelope.get("control") == "cleanup":
                if str(envelope.get("binding_id", "")) != self.binding_id:
                    raise ValueError("request_binding_drift")
                if self.require_session_ready:
                    # Cleanup is an explicit player decision, unlike losing
                    # a client connection. Send that decision as a real request.
                    assert self._child is not None and self._child.stdin is not None
                    self._child.stdin.write(json.dumps({"action": "run.quit",
                        "stop_reason": "explicit_bridge_cleanup"}) + "\n")
                    self._child.stdin.flush()
                    terminal_line = self._read_complete_response()
                    terminal = json.loads(terminal_line) if terminal_line else {}
                    final = terminal.get("result") if isinstance(terminal, Mapping) else None
                    if not isinstance(final, Mapping) or final.get("schema") != "caol-cockpit-live-final-v1":
                        cleanup = self._emergency_cleanup(explicit_quit=True)
                        self._write_status("cleaned", reason="explicit_quit_without_terminal_report", cleanup=cleanup)
                        return True
                    self._terminal_request_id = "bridge_cleanup"
                    self._write_status("terminalizing", terminal_request_id=self._terminal_request_id,
                                       cleanup={"status": "deferred_to_scenario_terminalization"})
                else:
                    self._write_status("cleaned", cleanup=self._emergency_cleanup(explicit_quit=True))
                return True
            request_id = str(envelope.get("request_id", "")).strip()
            request = envelope.get("request")
            if not request_id or not isinstance(request, Mapping) or request_id in self._seen:
                raise ValueError("invalid_or_stale_request_identity")
            if str(envelope.get("binding_id", "")) != self.binding_id:
                raise ValueError("request_binding_drift")
            self._seen.add(request_id)
            request_bytes = (json.dumps(request, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
            assert self._child is not None and self._child.stdin is not None and self._child.stdout is not None
            # Once admitted, a durable envelope is in flight until its receipt
            # exists.  A stale ready status must not invite a duplicate submit.
            self._write_status(
                "awaiting_response",
                child_pid=self._child.pid,
                inflight_request_id=request_id,
                **({"session_descriptor": self._active_session_descriptor}
                   if self._active_session_descriptor else {}),
            )
            _atomic_json(self.active_request_path, {
                "schema": "caol-cockpit-active-request-v1",
                "request_id": request_id, "binding_id": self.binding_id,
                "run_id": str(self._active_session_descriptor.get("run_id", "")),
                "request_identity": _request_identity(request_bytes),
                "session_generation": self._session_generation,
            })
            self._child.stdin.write(request_bytes.decode("utf-8"))
            self._child.stdin.flush()
            response_line = self._read_complete_response()
            if not response_line:
                self._write_status("process_dead", failed_request_id=request_id,
                                   child_exit_code=self._child.poll())
                return False
            response = json.loads(response_line)
            if isinstance(response, Mapping) and "cockpit_live_session" in response:
                response = {"ok": False, "error": "duplicate_cockpit_session_descriptor",
                            "action_outcome": "unknown", "next_action": "game.observe"}
                response_line = json.dumps(response)

            self._persist_response(request_id, request_bytes, response_line.encode("utf-8"))
            self.active_request_path.unlink(missing_ok=True)
            terminal = response.get("result") if isinstance(response, Mapping) else None
            # Only a successful explicit player finish/quit can terminalize
            # the controller. An error response cannot authorize game cleanup.
            if request.get("action") in {"run.finish", "run.quit"} and response.get("ok") is True and \
                    isinstance(terminal, Mapping) and terminal.get("schema") == "caol-cockpit-live-final-v1" and \
                    terminal.get("state") == "finished":
                if request.get("action") == "run.finish" and self.session_reentries:
                    self.session_reentries -= 1
                    self._write_status(
                        "transitioning",
                        last_response=(self.responses_dir / (request_id + ".receipt.json")).name,
                        remaining_session_reentries=self.session_reentries,
                        phase="awaiting_declared_reentry_descriptor",
                    )
                    descriptor = self._await_session_descriptor(
                        consume_pre_descriptor_prefix=False,
                    )
                    self._session_generation += 1
                    self._write_status(
                        "ready",
                        child_pid=self._child.pid if self._child else 0,
                        session_descriptor=descriptor,
                        remaining_session_reentries=self.session_reentries,
                        **({"startup_progress": self._startup_progress}
                           if self._startup_progress else {}),
                    )
                    self._active_session_descriptor = dict(descriptor)
                    return False
                self._terminal_request_id = request_id
                self._write_status("terminalizing", last_response=(self.responses_dir / (request_id + ".receipt.json")).name,
                                   terminal_request_id=request_id,
                                   child_pid=self._child.pid if self._child else 0,
                                   cleanup={"status": "deferred_to_scenario_terminalization"})
                return True
        except (BrokenPipeError, OSError):
            self.active_request_path.unlink(missing_ok=True)
            exit_code = self._child.poll() if self._child is not None else None
            self._write_status("process_dead", failed_request_id=locals().get("request_id", ""),
                               child_exit_code=exit_code)
        except (ValueError, TypeError, json.JSONDecodeError) as exc:
            self.active_request_path.unlink(missing_ok=True)
            if "request_bytes" in locals():
                response = {"ok": False, "error": str(exc), "action_outcome": "unknown",
                            "next_action": "Observe the current game; do not replay the request."}
                self._persist_response(request_id, request_bytes, json.dumps(response).encode("utf-8"))
            else:
                self._write_status("rejected", reason=str(exc))
        return False

    def _await_scenario_terminalization(self) -> int:
        """Only the scenario owner may certify that its final report and ingestion are done."""
        assert self._child is not None
        # The terminal request reply has already been retained.  The scenario
        # child can now emit its compact immutable-report index before exit;
        # no client is entitled to consume that private stdout.  Drain it to
        # EOF before waiting so a large terminal index cannot leave both sides
        # blocked after the scenario has written its authoritative completion
        # signal.
        assert self._child.stdout is not None
        terminal_stdout = self._child.stdout.read()
        if terminal_stdout:
            (self.session_dir / "terminal.stdout.log").write_text(
                terminal_stdout, encoding="utf-8"
            )
        self._child.wait()
        signal_path = self.session_dir / TERMINALIZATION_SIGNAL
        try:
            signal = json.loads(signal_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            signal = {}
        accepted = self._child.returncode == 0 and isinstance(signal, Mapping) and \
                   signal.get("schema") == "caol-cockpit-scenario-terminalization-v1" and \
                   signal.get("binding_id") == self.binding_id and \
                   signal.get("state") == "safe_to_cleanup"
        if not accepted:
            self._write_status("terminalization_failed", terminal_request_id=self._terminal_request_id,
                               child_exit_code=self._child.returncode, child_stderr="child.stderr.log",
                               cleanup={"status": "rejected_terminalization_incomplete"})
            return 1
        self._write_status("safe_to_cleanup", terminal_request_id=self._terminal_request_id,
                           child_exit_code=self._child.returncode, terminalization=signal,
                           cleanup={"status": "accepted", "owner": "scenario_terminalization"})
        return 0

    def serve(self) -> int:
        prior_handler = None
        main_thread = threading.current_thread() is threading.main_thread()
        if main_thread:
            prior_handler = signal.signal(signal.SIGTERM, self._termination_signal)
        try:
            result = self._serve()
            if result != 0:
                previous = json.loads(self.status_path.read_text(encoding="utf-8"))
                previous["emergency_cleanup"] = self._emergency_cleanup()
                _atomic_json(self.status_path, previous)
            return result
        except BaseException as exc:
            cleanup = self._emergency_cleanup()
            if self.status_path.exists():
                self._write_status("bridge_failed", reason=f"{type(exc).__name__}: {exc}",
                                   cleanup=cleanup, child_stderr="child.stderr.log")
            raise
        finally:
            if main_thread:
                signal.signal(signal.SIGTERM, prior_handler)

    def _serve(self) -> int:
        self.prepare()
        environment = dict(os.environ)
        environment["OPENCLAW_COCKPIT_BRIDGE_BINDING_ID"] = self.binding_id
        if self.authorization_token_id:
            environment["OPENCLAW_COCKPIT_AUTHORIZATION_TOKEN_ID"] = self.authorization_token_id
        environment["OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR"] = str(self.session_dir)
        stderr_path = self.session_dir / "child.stderr.log"
        self._child_stderr = stderr_path.open("w", encoding="utf-8")
        self._child = subprocess.Popen(self.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=self._child_stderr, text=True, bufsize=1, env=environment)
        if self.require_session_ready:
            self._write_status( "starting", child_pid=self._child.pid,
                                phase="awaiting_startup_hud_or_session_descriptor" )
            try:
                descriptor = self._await_session_descriptor()
            except ValueError as exc:
                self._write_status("process_dead", child_pid=self._child.pid,
                                   reason=str(exc),
                                   **({"startup_progress": self._startup_progress}
                                      if self._startup_progress else {}))
                if self._child.poll() is None:
                    self._child.terminate()
                    self._child.wait()
                self._close_child_streams()
                self._write_status("process_dead", child_exit_code=self._child.returncode,
                                   reason=str(exc),
                                   child_stderr="child.stderr.log", cleanup={"status": "accepted"},
                                   **({"startup_progress": self._startup_progress}
                                      if self._startup_progress else {}))
                return 1
            self._write_status("ready", child_pid=self._child.pid,
                               session_descriptor=descriptor,
                               **({"startup_progress": self._startup_progress}
                                  if self._startup_progress else {}),
                               **({"bootstrap_receipts": len(self.pre_descriptor_prefix),
                                   "bootstrap_gameplay_credit": False}
                                  if self.pre_descriptor_prefix else {}))
            self._active_session_descriptor = dict(descriptor)
            if descriptor.get("bootstrap_only") is True:
                return self._await_scenario_terminalization()
        else:
            self._write_status("ready", child_pid=self._child.pid)
        # Keep independent nonblocking ends open for the bridge lifetime.  A
        # single O_RDWR descriptor can be reported readable on macOS because
        # of its own writer, then block forever in ``read`` before a client
        # request is delivered.  The anchor writer prevents EOF while the
        # read end remains a genuine externally-written FIFO stream.
        descriptor = os.open(self.input_path, os.O_RDONLY | os.O_NONBLOCK)
        anchor_writer = os.open(self.input_path, os.O_WRONLY | os.O_NONBLOCK)
        pending = b""
        try:
            while True:
                # A request is first persisted by the public client.  The
                # spool is the authoritative handoff: a FIFO wake-up may be
                # missed on macOS, but a retained envelope cannot vanish
                # between a client accepting the request and the bridge
                # forwarding it to the cockpit.
                for request_path in sorted(self.requests_dir.glob("*.json")):
                    request_id = request_path.stem
                    if request_id in self._spooled_request_ids:
                        continue
                    self._spooled_request_ids.add(request_id)
                    if self._handle_request(request_path.read_text(encoding="utf-8")):
                        if self._terminal_request_id:
                            return self._await_scenario_terminalization()
                        return 0
                # macOS can leave an O_RDWR FIFO descriptor parked in a
                # blocking read after the startup child has emitted its
                # session descriptor.  Ask the kernel for readability before
                # every drain so an accepted public request cannot be lost
                # behind that startup handoff.
                readable, _, _ = select.select([descriptor], [], [], 0.01)
                if readable:
                    try:
                        received = os.read(descriptor, 65536)
                    except BlockingIOError:
                        received = b""
                else:
                    received = b""
                if received:
                    pending += received
                    while b"\n" in pending:
                        raw_line, pending = pending.split(b"\n", 1)
                        if not raw_line:
                            continue
                        if self._handle_request(raw_line.decode("utf-8")):
                            if self._terminal_request_id:
                                return self._await_scenario_terminalization()
                            return 0
                if self._child.poll() is not None:
                    prior = json.loads(self.status_path.read_text(encoding="utf-8"))
                    details = {
                        key: value for key, value in prior.items()
                        if key not in {"schema", "binding_id", "state", "request_count", "child_exit_code"}
                    }
                    self._write_status("process_dead", child_exit_code=self._child.returncode,
                                       child_stderr="child.stderr.log", **details)
                    return 1
        finally:
            os.close(descriptor)
            os.close(anchor_writer)
            if self._child.poll() is None:
                self._child.terminate()
                try:
                    self._child.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self._child.kill()
                    self._child.wait(timeout=2)
            previous = json.loads(self.status_path.read_text(encoding="utf-8"))
            terminal_state = str(previous.get("state", "cleaned"))
            terminal_details = {
                key: value for key, value in previous.items()
                if key not in {"schema", "binding_id", "state", "request_count", "child_exit_code", "cleanup"}
            }
            cleanup = previous.get("cleanup", {"status": "controller_stopped_game_retained"})
            self._write_status(terminal_state, **terminal_details,
                               child_exit_code=self._child.returncode, cleanup=cleanup)
            self._close_child_streams()
        return 0

    @staticmethod
    def send_request(session_dir: Path, *, request_id: str, binding_id: str,
                     request: Mapping[str, Any]) -> dict[str, Any]:
        session_dir = Path(session_dir)
        status = json.loads((session_dir / "status.json").read_text(encoding="utf-8"))
        if status.get("state") != "ready" or status.get("binding_id") != binding_id:
            return {"ok": False, "status": status}
        request_path = session_dir / "requests" / (request_id + ".json")
        if request_path.exists():
            # Preserve the accepted public submission so the bridge, rather
            # than a client-side race, records the duplicate as a rejected
            # stale request identity.
            request_path = session_dir / "requests" / (
                request_id + ".duplicate-" + str(time.time_ns()) + ".json"
            )
        envelope = {"request_id": request_id, "binding_id": binding_id, "request": dict(request)}
        _atomic_json(request_path, envelope)
        return {"ok": True, "request_id": request_id}

    @staticmethod
    def send_cancel(session_dir: Path, *, request_id: str, binding_id: str,
                    run_id: str = "", reason: str = "player_cancelled") -> dict[str, Any]:
        session_dir = Path(session_dir)
        try:
            status = json.loads((session_dir / "status.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return {"ok": False, "error": "session_status_unavailable"}
        if status.get("binding_id") != binding_id:
            return {"ok": False, "error": "request_binding_drift"}
        if status.get("state") != "awaiting_response" or status.get("inflight_request_id") != request_id:
            return {"ok": False, "error": "request_not_in_flight", "request_id": request_id}
        active = session_dir / "active-request.json"
        if not active.is_file():
            return {"ok": False, "error": "active_request_unavailable", "request_id": request_id}
        try:
            identity = json.loads(active.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return {"ok": False, "error": "active_request_unavailable", "request_id": request_id}
        if identity.get("request_id") != request_id or identity.get("binding_id") != binding_id:
            return {"ok": False, "error": "active_request_identity_mismatch", "request_id": request_id}
        if not run_id or identity.get("run_id") != run_id:
            return {"ok": False, "error": "cancel_run_mismatch", "request_id": request_id}
        generation = status.get("session_generation", 0)
        if identity.get("session_generation", 0) != generation:
            return {"ok": False, "error": "cancel_generation_mismatch", "request_id": request_id}
        cancel_id = "cancel-" + hashlib.sha256(request_id.encode("utf-8")).hexdigest()
        marker = session_dir / "controls" / (cancel_id + ".json")
        _atomic_json(marker, {"schema": "caol-cockpit-cancel-v1", "cancel_id": cancel_id,
                              "request_id": request_id, "binding_id": binding_id,
                              "run_id": run_id, "session_generation": generation, "reason": reason})
        return {"ok": True, "cancel_id": cancel_id, "request_id": request_id}

    @staticmethod
    def response_status(session_dir: Path, request_id: str, *, summary: bool = True) -> dict[str, Any]:
        path = Path(session_dir) / "responses" / (request_id + ".receipt.json")
        if not path.is_file():
            return {"ok": False, "error": "response_not_available_or_stale"}
        try:
            receipt = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {"ok": False, "error": "response_receipt_unavailable_or_invalid"}
        if not isinstance(receipt, dict) or receipt.get("request_id") != request_id:
            return {"ok": False, "error": "response_receipt_identity_mismatch"}
        if not isinstance(receipt.get("response_sha256"), str) or not receipt.get("response_artifact"):
            return {"ok": False, "error": "response_receipt_unavailable_or_invalid"}
        result = {"ok": True, "receipt": receipt}
        if summary:
            recovered = FileBackedCockpitBridge.response_artifact(
                session_dir, request_id, receipt["response_sha256"])
            if not recovered.get("ok"):
                return {**recovered, "receipt": receipt}
            result["response"] = cockpit_evidence.compact(recovered["response"])
            result["retrieval"] = {
                "fields": "response-slice --session-dir SESSION --request-id REQUEST --selector FIELD [--contains TEXT] [--offset N --limit N]",
                "full": ["response-artifact", "--session-dir", str(session_dir),
                         "--request-id", request_id, "--sha256", receipt["response_sha256"]],
                "logs": "log-query (--path EXACT_LOG | --session-dir SESSION) [--run-id RUN] [--request-id REQUEST] [--event EVENT] [--where FIELD=JSON] [--select FIELD]",
                "omissions": "Omitted values carry selectors, types and sizes. Explicit fields/full retrieval preserve exact values; transport ok is separate from response ok and native acceptance.",
            }
        return result

    @staticmethod
    def response_artifact(session_dir: Path, request_id: str, sha256: str) -> dict[str, Any]:
        """Recover one complete retained response only through its exact receipt digest."""
        expected = str(sha256).strip().lower()
        if len(expected) != 64 or any(char not in "0123456789abcdef" for char in expected):
            return {"ok": False, "error": "response SHA-256 must be a lowercase hexadecimal digest"}
        receipt_result = FileBackedCockpitBridge.response_status(session_dir, request_id, summary=False)
        if not receipt_result.get("ok"):
            return receipt_result
        receipt = receipt_result["receipt"]
        if receipt.get("response_sha256") != expected:
            return {"ok": False, "error": "response_artifact_digest_mismatch"}
        artifact = (Path(session_dir) / str(receipt.get("response_artifact", ""))).resolve()
        expected_parent = (Path(session_dir) / "responses").resolve()
        if artifact.parent != expected_parent:
            return {"ok": False, "error": "response_artifact_path_invalid"}
        try:
            raw = artifact.read_bytes()
        except OSError:
            return {"ok": False, "error": "response_artifact_unavailable"}
        if _digest(raw) != expected:
            return {"ok": False, "error": "response_artifact_hash_mismatch"}
        try:
            value = json.loads(raw)
        except (TypeError, ValueError, json.JSONDecodeError):
            return {"ok": False, "error": "response_artifact_is_not_json"}
        if not isinstance(value, Mapping):
            return {"ok": False, "error": "response_artifact_is_not_an_object"}
        try:
            value = resolve_wire(value, directory=session_dir, binding_id=receipt["binding_id"])
        except (OSError, ValueError, KeyError) as error:
            return {"ok": False, "error": str(error)}
        return {"ok": True, "request_id": request_id, "response": value}

    @staticmethod
    def cleanup(session_dir: Path, binding_id: str) -> dict[str, Any]:
        status = json.loads((Path(session_dir) / "status.json").read_text(encoding="utf-8"))
        if status.get("binding_id") != binding_id:
            return {"ok": False, "error": "request_binding_drift"}
        if status.get("state") == "safe_to_cleanup":
            return {"ok": True, "cleanup": "already_accepted"}
        if status.get("state") == "terminalizing":
            return {"ok": False, "error": "cleanup_requires_scenario_terminalization", "status": status}
        if status.get("state") in {"process_dead", "bridge_failed", "terminalization_failed"}:
            bridge = FileBackedCockpitBridge(Path(session_dir), [], binding_id=binding_id)
            cleanup = bridge._emergency_cleanup(explicit_quit=True)
            _atomic_json(Path(session_dir) / "status.json", {**status, "cleanup": cleanup})
            return {"ok": cleanup.get("game", {}).get("status") in {
                "terminated", "already_exited", "killed", "terminated_during_kill_escalation",
            }, "cleanup": cleanup}
        with (Path(session_dir) / "requests.fifo").open("w", encoding="utf-8") as sink:
            sink.write(json.dumps({"control": "cleanup", "binding_id": binding_id}) + "\n")
            sink.flush()
        return {"ok": True, "cleanup": "requested"}

    @staticmethod
    def response_slice(session_dir: Path, request_id: str, selector: str,
                       offset: int = 0, limit: int | None = None, contains: str | None = None) -> dict[str, Any]:
        if not str(selector).strip():
            return {"ok": False, "error": "selected_response_slice_is_required"}
        receipt_result = FileBackedCockpitBridge.response_status(session_dir, request_id, summary=False)
        if not receipt_result.get("ok"):
            return receipt_result
        receipt = receipt_result["receipt"]
        recovered = FileBackedCockpitBridge.response_artifact(session_dir, request_id, receipt["response_sha256"])
        if not recovered.get("ok"):
            return recovered
        try:
            value = cockpit_evidence.select(recovered["response"], selector)
        except (KeyError, IndexError):
            return {"ok": False, "error": "selected_response_slice_is_unavailable"}
        result = {"ok": True, "request_id": request_id, "response_sha256": receipt["response_sha256"],
                  "selector": selector}
        if isinstance(value, ArchiveSequence):
            if offset < 0 or (limit is not None and limit <= 0):
                return {"ok": False, "error": "slice_paging_requires_array_and_valid_range"}
            if contains is None:
                end = len(value) if limit is None else min(len(value), offset + limit)
                page = value if offset == 0 and limit is None else value[offset:end]
                return {**result, "page": {"offset": offset, "total": len(value),
                        "next_offset": end if end < len(value) else None}, "slice": page}
            matched, selected, indices = 0, [], []
            for index, row in enumerate(value):
                text = "".join(json_chunks(row))
                if contains.casefold() not in text.casefold():
                    continue
                if matched >= offset and (limit is None or len(selected) < limit):
                    selected.append(row)
                    indices.append(index)
                matched += 1
            return {**result, "filter": {"contains": contains, "total": len(value), "matched": matched},
                    "source_indices": indices, "page": {"offset": offset, "total": matched,
                    "next_offset": offset + len(selected) if offset + len(selected) < matched else None},
                    "slice": selected}
        if contains is not None:
            if not isinstance(value, list):
                return {"ok": False, "error": "slice_filter_requires_array"}
            total = len(value)
            indices = [i for i, row in enumerate(value) if contains.casefold() in json.dumps(row, ensure_ascii=False).casefold()]
            value = [value[i] for i in indices]
            result["filter"] = {"contains": contains, "total": total, "matched": len(value)}
            result["source_indices"] = indices[offset:None if limit is None else offset + limit]
        if limit is not None or offset:
            if not isinstance(value, list) or offset < 0 or (limit is not None and limit <= 0):
                return {"ok": False, "error": "slice_paging_requires_array_and_valid_range"}
            end = len(value) if limit is None else offset + limit
            result["page"] = {"offset": offset, "total": len(value),
                              "next_offset": end if end < len(value) else None}
            value = value[offset:end]
        return {**result, "slice": value}


class FileBackedCockpitClient:
    """Submit a live request once and collect its retained response separately.

    A delayed artifact is an in-flight request, not authority to submit the
    same request identity again.  Callers retain the returned request id and
    use ``collect_response`` until the bridge publishes its receipt.
    """

    @staticmethod
    def submit_once(session_dir: Path, *, request_id: str, binding_id: str,
                    request: Mapping[str, Any]) -> dict[str, Any]:
        return FileBackedCockpitBridge.send_request(
            session_dir, request_id=request_id, binding_id=binding_id, request=request,
        )

    @staticmethod
    def collect_response(session_dir: Path, request_id: str) -> dict[str, Any]:
        return FileBackedCockpitBridge.response_status(session_dir, request_id)


class FreshObservationSequence:
    """Own a single observe/act chain without re-submitting delayed requests.

    The file bridge deliberately rejects repeated request ids.  This small
    client records the outstanding id separately from its response collection
    and permits an action only for the newest, still-unconsumed observation.
    """

    def __init__(self, session_dir: Path, binding_id: str, request_prefix: str):
        self.session_dir = session_dir
        self.binding_id = binding_id
        self.request_prefix = request_prefix
        self._serial = 0
        self._latest_observation_id = ""
        self._consumed_observation_ids: set[str] = set()
        self._submitted_request_ids: set[str] = set()

    def _submit(self, suffix: str, request: Mapping[str, Any]) -> str:
        self._serial += 1
        request_id = f"{self.request_prefix}-{suffix}-{self._serial:04d}"
        if request_id in self._submitted_request_ids:
            raise ValueError("duplicate_cockpit_request_id")
        submitted = FileBackedCockpitClient.submit_once(
            self.session_dir, request_id=request_id, binding_id=self.binding_id, request=request,
        )
        if not submitted.get("ok") or submitted.get("request_id") != request_id:
            raise ValueError("cockpit_request_submission_failed")
        self._submitted_request_ids.add(request_id)
        return request_id

    def collect(self, request_id: str) -> dict[str, Any]:
        """Collect a retained response only; this never sends another request."""
        return FileBackedCockpitClient.collect_response(self.session_dir, request_id)

    def observe(self) -> str:
        request_id = self._submit("observe", {"action": "game.observe"})
        return request_id

    def accept_observation(self, request_id: str) -> str:
        response = self.collect(request_id)
        if not response.get("ok"):
            return ""
        artifact = self.session_dir / str(response["receipt"]["response_artifact"])
        result = json.loads(artifact.read_text(encoding="utf-8"))
        observation_id = str(result.get("result", {}).get("observation_id", "")).strip()
        if not observation_id:
            raise ValueError("fresh_observation_id_is_required")
        self._latest_observation_id = observation_id
        return observation_id

    def act(self, action_id: str) -> str:
        observation_id = self._latest_observation_id
        if not observation_id or observation_id in self._consumed_observation_ids:
            raise ValueError("fresh_unconsumed_observation_is_required")
        request_id = self._submit(
            "act", {"action": "game.act", "observation_id": observation_id, "action_id": action_id},
        )
        self._consumed_observation_ids.add(observation_id)
        return request_id


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve")
    serve.add_argument("--session-dir", required=True)
    serve.add_argument("--binding-id", required=True)
    serve.add_argument("--authorization-token-id", default="")
    serve.add_argument("--require-session-ready", action="store_true")
    serve.add_argument("--pre-descriptor-prefix-json", default="[]")
    serve.add_argument("--session-reentries", type=int, default=0)
    serve.add_argument("cockpit_command", nargs=argparse.REMAINDER)
    start = commands.add_parser("start")
    start.add_argument("--session-dir", required=True)
    start.add_argument("--binding-id", required=True)
    start.add_argument("--authorization-token-id", default="")
    start.add_argument("--require-session-ready", action="store_true")
    start.add_argument("--pre-descriptor-prefix-json", default="[]")
    start.add_argument("--session-reentries", type=int, default=0)
    start.add_argument("cockpit_command", nargs=argparse.REMAINDER)
    request = commands.add_parser("request")
    request.add_argument("--session-dir", required=True)
    request.add_argument("--binding-id", required=True)
    request.add_argument("--request-id", required=True)
    request.add_argument("--request-json", required=True)
    status = commands.add_parser("response-status", help="Verified compact observation, action availability, outcomes and field discovery")
    status.add_argument("--session-dir", required=True)
    status.add_argument("--request-id", required=True)
    response = commands.add_parser("response-slice")
    response.add_argument("--session-dir", required=True)
    response.add_argument("--request-id", required=True)
    response.add_argument("--selector", required=True)
    response.add_argument("--offset", type=int, default=0)
    response.add_argument("--limit", type=int)
    response.add_argument("--contains", help="Filter a selected array by case-insensitive text before paging; source indices are preserved")
    artifact = commands.add_parser("response-artifact")
    artifact.add_argument("--session-dir", required=True)
    artifact.add_argument("--request-id", required=True)
    artifact.add_argument("--sha256", required=True)
    logs = commands.add_parser("log-query", help="Filter exact JSONL/debug logs before compact rendering; retain raw record handles")
    log_source = logs.add_mutually_exclusive_group(required=True)
    log_source.add_argument("--path", action="append")
    log_source.add_argument("--session-dir", help="Query retained response artifacts in this exact bridge session")
    for field in ("run-id", "request-id", "frame-id", "event"):
        logs.add_argument("--" + field)
    logs.add_argument("--where", action="append", default=[], metavar="FIELD=JSON")
    logs.add_argument("--select", action="append", default=[], metavar="FIELD")
    logs.add_argument("--contains", help="Case-insensitive text filter within records already selected by semantic identities/fields")
    logs.add_argument("--offset", type=int, default=0)
    logs.add_argument("--limit", type=int, default=20, help="Rows per page; all matches are counted and pageable")
    record = commands.add_parser("record-artifact", help="Verify and retrieve an exact retained log record or selected fields")
    record.add_argument("--path", required=True)
    record.add_argument("--offset", type=int, required=True)
    record.add_argument("--length", type=int, required=True)
    record.add_argument("--sha256", required=True)
    record.add_argument("--select", action="append", default=[], metavar="FIELD")
    cleanup = commands.add_parser("cleanup")
    cleanup.add_argument("--session-dir", required=True)
    cleanup.add_argument("--binding-id", required=True)
    args = parser.parse_args(argv)
    if args.command == "log-query":
        if args.offset < 0 or args.limit <= 0:
            parser.error("offset must be nonnegative and limit positive")
        filters = {k: getattr(args, k) for k in ("run_id", "request_id", "frame_id", "event")
                   if getattr(args, k) is not None}
        try:
            for expression in args.where:
                key, value = expression.split("=", 1)
                filters[key] = json.loads(value)
        except (ValueError, json.JSONDecodeError):
            parser.error("--where requires FIELD=JSON (quote string values as JSON)")
        if args.session_dir:
            responses_dir = Path(args.session_dir) / "responses"
            if not responses_dir.is_dir():
                print(json.dumps({"ok": False, "error": "session_responses_unavailable"}))
                return 1
            paths = sorted(p for p in responses_dir.glob("*.json") if not p.name.endswith(".receipt.json"))
        else:
            paths = [Path(p) for p in args.path]
        result = cockpit_evidence.query(paths, filters, args.select, args.offset, args.limit, args.contains)
        print(json.dumps(result))
        return 0 if result["ok"] else 1
    if args.command == "record-artifact":
        result = cockpit_evidence.record_artifact(Path(args.path), args.offset, args.length, args.sha256, args.select)
        print(json.dumps(result))
        return 0 if result["ok"] else 1
    if args.command == "start":
        try:
            prefix = json.loads(args.pre_descriptor_prefix_json)
        except json.JSONDecodeError:
            parser.error("--pre-descriptor-prefix-json must be valid JSON")
        if not isinstance(prefix, list) or any(not isinstance(item, Mapping) for item in prefix):
            parser.error("--pre-descriptor-prefix-json must be an array of objects")
        if args.session_reentries < 0:
            parser.error("--session-reentries must be non-negative")
        command = list(args.cockpit_command)
        if command[:1] == ["--"]:
            command = command[1:]
        bridge_command = [
            sys.executable, str(Path(__file__).resolve()), "serve",
            "--session-dir", args.session_dir, "--binding-id", args.binding_id,
            "--authorization-token-id", args.authorization_token_id,
            *( ["--require-session-ready"] if args.require_session_ready else [] ),
            "--pre-descriptor-prefix-json", json.dumps(prefix, separators=(",", ":")),
            "--session-reentries", str(args.session_reentries),
            "--", *command,
        ]
        error_path = Path(str(args.session_dir) + ".bridge.stderr.log")
        error_path.parent.mkdir(parents=True, exist_ok=True)
        with error_path.open("a", encoding="utf-8") as error_log:
            child = subprocess.Popen(bridge_command, stdin=subprocess.DEVNULL,
                                     stdout=subprocess.DEVNULL, stderr=error_log,
                                     start_new_session=True)
        print(json.dumps({"ok": True, "schema": SCHEMA, "bridge_pid": child.pid,
                          "session_dir": args.session_dir, "binding_id": args.binding_id}))
        return 0
    if args.command == "serve":
        try:
            prefix = json.loads(args.pre_descriptor_prefix_json)
        except json.JSONDecodeError:
            parser.error("--pre-descriptor-prefix-json must be valid JSON")
        if not isinstance(prefix, list) or any(not isinstance(item, Mapping) for item in prefix):
            parser.error("--pre-descriptor-prefix-json must be an array of objects")
        if args.session_reentries < 0:
            parser.error("--session-reentries must be non-negative")
        command = list(args.cockpit_command)
        if command[:1] == ["--"]:
            command = command[1:]
        return FileBackedCockpitBridge(
            Path(args.session_dir), command, binding_id=args.binding_id,
            authorization_token_id=args.authorization_token_id,
            require_session_ready=args.require_session_ready,
            pre_descriptor_prefix=prefix,
            session_reentries=args.session_reentries,
        ).serve()
    if args.command == "request":
        value = json.loads(args.request_json)
        if not isinstance(value, Mapping):
            parser.error("--request-json must decode to an object")
        print(json.dumps(FileBackedCockpitBridge.send_request(Path(args.session_dir), request_id=args.request_id,
                                                              binding_id=args.binding_id, request=value)))
        return 0
    if args.command == "response-status":
        print(json.dumps(FileBackedCockpitBridge.response_status(Path(args.session_dir), args.request_id)))
        return 0
    if args.command == "cleanup":
        print(json.dumps(FileBackedCockpitBridge.cleanup(Path(args.session_dir), args.binding_id)))
        return 0
    if args.command == "response-artifact":
        result = FileBackedCockpitBridge.response_artifact(
            Path(args.session_dir), args.request_id, args.sha256)
    else:
        result = FileBackedCockpitBridge.response_slice(
            Path(args.session_dir), args.request_id, args.selector, args.offset, args.limit, args.contains)
    for chunk in json_chunks(result):
        sys.stdout.write(chunk)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
