#!/usr/bin/env python3
"""File-backed transport for a worker-owned live cockpit session.

The bridge, rather than the command that starts it, owns the cockpit process.
Requests travel through a private FIFO.  Every complete JSONL response is
written to a session artifact before a compact correlated receipt is exposed.
This keeps a large native observation out of an execution tool's stdout pipe.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import Any, Mapping, Sequence


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
    """Own one cockpit child and correlate FIFO requests to retained replies."""

    def __init__(self, session_dir: Path, command: Sequence[str], *, binding_id: str,
                 require_session_ready: bool = False,
                 pre_descriptor_prefix: Sequence[Mapping[str, Any]] = ()) -> None:
        self.session_dir = Path(session_dir)
        self.command = [str(part) for part in command]
        self.binding_id = str(binding_id).strip()
        self.require_session_ready = require_session_ready
        self.pre_descriptor_prefix = tuple(dict(item) for item in pre_descriptor_prefix)
        self.input_path = self.session_dir / "requests.fifo"
        self.status_path = self.session_dir / "status.json"
        self.responses_dir = self.session_dir / "responses"
        self._seen: set[str] = set()
        self._sequence = 0
        self._child: subprocess.Popen[str] | None = None
        self._child_stderr = None
        self._bootstrap_run_id = ""
        self._terminal_request_id = ""
        self._startup_progress: dict[str, Any] = {}

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
            "state": state,
            "request_count": self._sequence,
            **extra,
        }

    def _write_status(self, state: str, **extra: Any) -> None:
        _atomic_json(self.status_path, self._status(state, **extra))

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
        self.responses_dir.mkdir()
        os.mkfifo(self.input_path, 0o600)
        _atomic_json(self.session_dir / "bridge.manifest.json", {
            "schema": SCHEMA,
            "binding_id": self.binding_id,
            "command": self.command,
            "input_channel": self.input_path.name,
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
            "binding_id": self.binding_id,
            "request_sha256": _digest(request_bytes),
            "request_identity": _request_identity(request_bytes),
            "response_sha256": _digest(response_bytes),
            "response_artifact": str(artifact.relative_to(self.session_dir)),
        }
        _atomic_json(self.responses_dir / (request_id + ".receipt.json"), receipt)
        self._write_status("ready", last_response=receipt)
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

    def _await_session_descriptor(self) -> Mapping[str, Any]:
        """Run only the declared zero-credit prefix before admitting public input."""
        prefix_index = 0
        while True:
            ready = self._read_complete_response()
            if not ready:
                raise ValueError("pre_descriptor_no_progress")
            try:
                envelope = json.loads(ready)
            except json.JSONDecodeError as exc:
                raise ValueError("malformed_pre_descriptor_output") from exc
            if not isinstance(envelope, Mapping):
                raise ValueError("malformed_pre_descriptor_output")
            if self._consume_startup_progress( envelope ):
                continue
            if prefix_index < len(self.pre_descriptor_prefix):
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
                    # EOF is the only bridge-owned abort route.  It lets the
                    # cockpit emit its fail-closed immutable final result and
                    # lets the scenario own report ingestion and game cleanup.
                    assert self._child is not None and self._child.stdin is not None
                    self._child.stdin.close()
                    terminal_line = self._read_complete_response()
                    terminal = json.loads(terminal_line) if terminal_line else {}
                    final = terminal.get("final") if isinstance(terminal, Mapping) else None
                    if not isinstance(final, Mapping) or final.get("schema") != "caol-cockpit-live-final-v1":
                        raise ValueError("cleanup_failed_to_produce_terminal_result")
                    self._terminal_request_id = "bridge_cleanup"
                    self._write_status("terminalizing", terminal_request_id=self._terminal_request_id,
                                       cleanup={"status": "deferred_to_scenario_terminalization"})
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
            self._child.stdin.write(request_bytes.decode("utf-8"))
            self._child.stdin.flush()
            response_line = self._read_complete_response()
            if not response_line:
                self._write_status("process_dead", failed_request_id=request_id,
                                   child_exit_code=self._child.poll())
                return False
            response = json.loads(response_line)
            if isinstance(response, Mapping) and "cockpit_live_session" in response:
                if self._child.poll() is None:
                    self._child.terminate()
                    self._child.wait()
                self._write_status("process_dead", failed_request_id=request_id,
                                   child_exit_code=self._child.returncode,
                                   reason="duplicate_cockpit_session_descriptor",
                                   cleanup={"status": "accepted"})
                return False
            self._persist_response(request_id, request_bytes, response_line.encode("utf-8"))
            terminal = response.get("result") if isinstance(response, Mapping) else None
            # A fail-closed cockpit action returns its immutable final under
            # ``final`` alongside the public error.  It is just as terminal
            # as a successful ``run.finish`` result: retain the reply, then
            # let the scenario complete ingestion and owned cleanup.
            if not isinstance(terminal, Mapping) and isinstance(response, Mapping):
                terminal = response.get("final")
            if isinstance(terminal, Mapping) and terminal.get("schema") == "caol-cockpit-live-final-v1" and \
                    terminal.get("state") == "finished":
                self._terminal_request_id = request_id
                self._write_status("terminalizing", last_response=(self.responses_dir / (request_id + ".receipt.json")).name,
                                   terminal_request_id=request_id,
                                   cleanup={"status": "deferred_to_scenario_terminalization"})
                return True
        except (BrokenPipeError, OSError):
            exit_code = self._child.poll() if self._child is not None else None
            self._write_status("process_dead", failed_request_id=locals().get("request_id", ""),
                               child_exit_code=exit_code)
        except (ValueError, TypeError, json.JSONDecodeError) as exc:
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
        self.prepare()
        environment = dict(os.environ)
        environment["OPENCLAW_COCKPIT_BRIDGE_BINDING_ID"] = self.binding_id
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
            if descriptor.get("bootstrap_only") is True:
                return self._await_scenario_terminalization()
        else:
            self._write_status("ready", child_pid=self._child.pid)
        # O_RDWR keeps the private FIFO readable between discrete client calls.
        descriptor = os.open(self.input_path, os.O_RDWR)
        try:
            with os.fdopen(descriptor, "r", encoding="utf-8") as source:
                for line in source:
                    if self._handle_request(line):
                        if self._terminal_request_id:
                            return self._await_scenario_terminalization()
                        break
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
            if self._child.poll() is None:
                self._child.terminate()
                self._child.wait()
            previous = json.loads(self.status_path.read_text(encoding="utf-8"))
            terminal_state = str(previous.get("state", "cleaned"))
            terminal_details = {
                key: value for key, value in previous.items()
                if key not in {"schema", "binding_id", "state", "request_count", "child_exit_code", "cleanup"}
            }
            cleanup = previous.get("cleanup") if terminal_state in {
                "safe_to_cleanup", "terminalization_failed",
            } else {"status": "accepted"}
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
        envelope = {"request_id": request_id, "binding_id": binding_id, "request": dict(request)}
        with (session_dir / "requests.fifo").open("w", encoding="utf-8") as sink:
            sink.write(json.dumps(envelope, ensure_ascii=False, separators=(",", ":")) + "\n")
            sink.flush()
        return {"ok": True, "request_id": request_id}

    @staticmethod
    def response_status(session_dir: Path, request_id: str) -> dict[str, Any]:
        path = Path(session_dir) / "responses" / (request_id + ".receipt.json")
        if not path.is_file():
            return {"ok": False, "error": "response_not_available_or_stale"}
        receipt = json.loads(path.read_text(encoding="utf-8"))
        return {"ok": True, "receipt": receipt}

    @staticmethod
    def cleanup(session_dir: Path, binding_id: str) -> dict[str, Any]:
        status = json.loads((Path(session_dir) / "status.json").read_text(encoding="utf-8"))
        if status.get("binding_id") != binding_id:
            return {"ok": False, "error": "request_binding_drift"}
        if status.get("state") == "safe_to_cleanup":
            return {"ok": True, "cleanup": "already_accepted"}
        if status.get("state") == "terminalizing":
            return {"ok": False, "error": "cleanup_requires_scenario_terminalization", "status": status}
        with (Path(session_dir) / "requests.fifo").open("w", encoding="utf-8") as sink:
            sink.write(json.dumps({"control": "cleanup", "binding_id": binding_id}) + "\n")
            sink.flush()
        return {"ok": True, "cleanup": "requested"}

    @staticmethod
    def response_slice(session_dir: Path, request_id: str, selector: str) -> dict[str, Any]:
        if not str(selector).strip():
            return {"ok": False, "error": "selected_response_slice_is_required"}
        receipt_result = FileBackedCockpitBridge.response_status(session_dir, request_id)
        if not receipt_result.get("ok"):
            return receipt_result
        receipt = receipt_result["receipt"]
        artifact = Path(session_dir) / str(receipt["response_artifact"])
        raw = artifact.read_bytes()
        if _digest(raw) != receipt["response_sha256"]:
            return {"ok": False, "error": "response_artifact_hash_mismatch"}
        value: Any = json.loads(raw)
        for part in (piece for piece in selector.split(".") if piece):
            if not isinstance(value, Mapping) or part not in value:
                return {"ok": False, "error": "selected_response_slice_is_unavailable"}
            value = value[part]
        return {"ok": True, "request_id": request_id, "slice": value}


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    serve = commands.add_parser("serve")
    serve.add_argument("--session-dir", required=True)
    serve.add_argument("--binding-id", required=True)
    serve.add_argument("--require-session-ready", action="store_true")
    serve.add_argument("--pre-descriptor-prefix-json", default="[]")
    serve.add_argument("cockpit_command", nargs=argparse.REMAINDER)
    start = commands.add_parser("start")
    start.add_argument("--session-dir", required=True)
    start.add_argument("--binding-id", required=True)
    start.add_argument("--require-session-ready", action="store_true")
    start.add_argument("--pre-descriptor-prefix-json", default="[]")
    start.add_argument("cockpit_command", nargs=argparse.REMAINDER)
    request = commands.add_parser("request")
    request.add_argument("--session-dir", required=True)
    request.add_argument("--binding-id", required=True)
    request.add_argument("--request-id", required=True)
    request.add_argument("--request-json", required=True)
    status = commands.add_parser("response-status")
    status.add_argument("--session-dir", required=True)
    status.add_argument("--request-id", required=True)
    response = commands.add_parser("response-slice")
    response.add_argument("--session-dir", required=True)
    response.add_argument("--request-id", required=True)
    response.add_argument("--selector", required=True)
    cleanup = commands.add_parser("cleanup")
    cleanup.add_argument("--session-dir", required=True)
    cleanup.add_argument("--binding-id", required=True)
    args = parser.parse_args(argv)
    if args.command == "start":
        try:
            prefix = json.loads(args.pre_descriptor_prefix_json)
        except json.JSONDecodeError:
            parser.error("--pre-descriptor-prefix-json must be valid JSON")
        if not isinstance(prefix, list) or any(not isinstance(item, Mapping) for item in prefix):
            parser.error("--pre-descriptor-prefix-json must be an array of objects")
        command = list(args.cockpit_command)
        if command[:1] == ["--"]:
            command = command[1:]
        bridge_command = [
            sys.executable, str(Path(__file__).resolve()), "serve",
            "--session-dir", args.session_dir, "--binding-id", args.binding_id,
            *( ["--require-session-ready"] if args.require_session_ready else [] ),
            "--pre-descriptor-prefix-json", json.dumps(prefix, separators=(",", ":")),
            "--", *command,
        ]
        child = subprocess.Popen(bridge_command, stdin=subprocess.DEVNULL,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
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
        command = list(args.cockpit_command)
        if command[:1] == ["--"]:
            command = command[1:]
        return FileBackedCockpitBridge(
            Path(args.session_dir), command, binding_id=args.binding_id,
            require_session_ready=args.require_session_ready,
            pre_descriptor_prefix=prefix,
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
    print(json.dumps(FileBackedCockpitBridge.response_slice(Path(args.session_dir), args.request_id, args.selector)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
