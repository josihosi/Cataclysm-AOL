#!/usr/bin/env python3
"""Small persistent player client for an already authorized file-backed cockpit.

This client owns transport bookkeeping, never game state or launch authority.
A pending request is collected, not replayed. Actions use the exact last frame
shown to the player; stale frames remain the native owner's decision.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import time
from typing import Any
import uuid

from cockpit import player_controls
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge, _atomic_json


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

    def save(self):
        _atomic_json(self.state_path, self.state)

    def controls(self) -> dict[str, Any]:
        # Local metadata only: safe while a native request is pending. Do not
        # refresh the frame, enqueue traffic, or save client ownership state.
        return {"ok": True, "result": player_controls(self.state.get("operation_availability"))}

    def collect(self, wait_seconds: float = 0) -> dict[str, Any]:
        pending = self.state.get("pending")
        if not pending:
            if self.state.get("finished"):
                status = json.loads((self.session / "status.json").read_text())
                terminal = status.get("terminalization", {})
                cleanup = terminal.get("cleanup", status.get("cleanup"))
                state = status.get("state")
                failed = state in {"bridge_failed", "terminalization_failed", "process_dead"}
                complete = state == "safe_to_cleanup"
                return {"ok": not failed, "state": "finished" if complete else "cleanup_failed" if failed else "finishing",
                        "bridge_state": state, "cleanup": cleanup,
                        "bridge_cleanup": status.get("cleanup"), "terminalization": terminal,
                        "reason": status.get("reason", status.get("error")),
                        "next": "inspect retained evidence" if complete or failed else "collect",
                        "note": "Cleanup disposition is reported by the scenario owner; it is separate from native exit or save proof."}
            return {"ok": False, "error": "no_pending_request", "next": "look"}
        request_id = pending["request_id"]
        deadline = time.monotonic() + max(0, wait_seconds)
        while True:
            result = Bridge.response_status(self.session, request_id)
            if result.get("ok") or result.get("error") != "response_not_available_or_stale":
                break
            if time.monotonic() >= deadline:
                status = json.loads((self.session / "status.json").read_text())
                if status.get("state") in {"process_dead", "bridge_failed", "terminalization_failed", "safe_to_cleanup"} or status.get("child_exit_code") is not None:
                    return {"ok": False, "state": "session_ended_without_response",
                            "error": "bridge_ended_before_response", "request_id": request_id,
                            "bridge_state": status.get("state"),
                            "reason": status.get("reason", status.get("error", "cockpit child exited")),
                            "child_exit_code": status.get("child_exit_code"),
                            "log_path": str(self.session / "child.stderr.log"),
                            "next": "Inspect the failure and retained evidence; this request will not be replayed."}
                return {"ok": True, "state": "pending", "request_id": request_id,
                        "bridge_state": status.get("state"), "next": "collect",
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
        if isinstance(response.get("operation_availability"), dict):
            self.state["operation_availability"] = response["operation_availability"]
        observation = response.get("observation", response.get("result", {}))
        if isinstance(observation, dict) and not observation.get("observation_id"):
            observation = observation.get("terminal_observation", observation)
        if isinstance(observation, dict) and observation.get("observation_id"):
            self.state["observation_id"] = observation["observation_id"]
            self.state["observation_request_id"] = request_id
            if observation.get("surface", {}).get("kind") == "process_exited":
                self.state["process_exited"] = True
        if response.get("ok") and pending["request"]["action"] == "run.witness":
            self.state["sealed_terminal"] = {
                key: pending["request"][key]
                for key in ("observation_id", "stop_reason", "unused_authority")
            }
        final = response.get("final", {})
        if (response.get("ok") and pending["request"]["action"] == "run.finish") or (
                isinstance(final, dict) and final.get("schema") == "caol-cockpit-live-final-v1"
                and final.get("state") == "finished"):
            self.state["finished"] = True
            self.state.pop("observation_id", None)
        self.state["last_request_id"] = request_id
        self.state.pop("pending", None)
        self.save()
        output = {**result, "ok": response.get("ok") is True,
                  "state": "collected" if response.get("ok") else "rejected", "request_id": request_id,
                  "next": "collect" if self.state.get("finished") else
                          "finish --witness FILE" if self.state.get("sealed_terminal") else
                          "journal --reason REASON" if self.state.get("process_exited") else
                          "act, look, inspect, or journal" if self.state.get("observation_id") else "look"}
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
                "evidence_ceiling": "Do not exceed the sealed journal ceiling",
            }
        return output

    def submit(self, request: dict[str, Any], wait_seconds: float) -> dict[str, Any]:
        if self.state.get("pending"):
            return {"ok": False, "error": "request_in_flight", "next": "collect",
                    "request_id": self.state["pending"]["request_id"]}
        if self.state.get("finished"):
            return {"ok": False, "error": "session_already_finished"}
        request_id = "play-" + uuid.uuid4().hex
        self.state["pending"] = {"request_id": request_id, "request": request}
        # Persist ownership before submission: even a crash cannot cause replay.
        previous_frame = self.state.pop("observation_id", None)
        self.save()
        result = Bridge.send_request(self.session, request_id=request_id,
                                     binding_id=self.binding, request=request)
        if not result.get("ok"):
            self.state.pop("pending", None)
            if previous_frame:
                self.state["observation_id"] = previous_frame
            self.save()
            return result
        return self.collect(wait_seconds)

    def frame(self) -> str:
        if self.state.get("pending"):
            raise ValueError("request_in_flight: use collect")
        if self.state.get("sealed_terminal"):
            raise ValueError("journal_is_sealed: submit finish --witness FILE")
        frame = self.state.get("observation_id")
        if not frame:
            raise ValueError("look_required: no current unconsumed observation")
        return str(frame)

    def act(self, action: str, target: str | None, parameters: dict[str, str], wait_seconds: float):
        if self.state.get("process_exited"):
            return {"ok": False, "error": "game_process_exited", "next": "journal --reason REASON"}
        request: dict[str, Any] = {"action": "game.act", "action_id": action,
                                   "observation_id": self.frame()}
        if target is not None:
            request["stable_id"] = target
        if parameters:
            request["parameters"] = parameters
        return self.submit(request, wait_seconds)

    def call(self, request: dict[str, Any], wait_seconds: float):
        if not isinstance(request, dict) or not isinstance(request.get("action"), str) or not request["action"].strip().lower().startswith("game."):
            raise ValueError("call_requires_a_structured_game_request")
        if self.state.get("sealed_terminal"):
            raise ValueError("journal_is_sealed: submit finish --witness FILE")
        if self.state.get("process_exited") and request["action"].strip().lower() not in {"game.observe", "game.look"}:
            return {"ok": False, "error": "game_process_exited", "next": "journal --reason REASON"}
        # Keep recipes and their types intact. The service owns operation
        # authorization and validation; this client only owns transport.
        return self.submit(request, wait_seconds)

    def journal(self, reason: str, unused: str, wait_seconds: float):
        return self.submit({"action": "run.witness", "observation_id": self.frame(),
                            "stop_reason": reason, "unused_authority": unused}, wait_seconds)

    def finish(self, witness: dict[str, Any], wait_seconds: float):
        terminal = self.state.get("sealed_terminal")
        if not terminal:
            return {"ok": False, "error": "journal_required_before_finish",
                    "next": "journal --reason REASON; inspect result.evidence_journal.entries --limit 10"}
        return self.submit({"action": "run.finish", **terminal, "witness": witness}, wait_seconds)

    def inspect(self, selector: str, offset: int, limit: int | None, contains: str | None,
                request_id: str | None = None):
        request_id = request_id or self.state.get("last_request_id")
        if not request_id:
            return {"ok": False, "error": "no_collected_response", "next": "look or collect"}
        receipt = Bridge.response_status(self.session, request_id, summary=False)
        if receipt.get("ok") and receipt["receipt"].get("binding_id") != self.binding:
            return {"ok": False, "error": "response_binding_mismatch"}
        return Bridge.response_slice(self.session, request_id, selector, offset, limit, contains)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, required=True, help="Existing registry-launched session directory")
    parser.add_argument("--wait-seconds", type=float, default=1,
                        help="Wait for this response, then return pending; never resubmit")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("look", help="Observe the current input owner and its legal actions")
    commands.add_parser("collect", help="Collect the outstanding response without replaying input")
    act = commands.add_parser("act", help="Act on the last displayed frame; native authority remains unchanged")
    act.add_argument("action")
    act.add_argument("--target", help="Exact advertised stable ID")
    act.add_argument("--param", action="append", default=[], metavar="KEY=VALUE")
    commands.add_parser("controls", help="Read wait/movement request examples, permissions and interruption behavior without sending input")
    call = commands.add_parser("call", help="Submit an existing structured game.* request; service authorization still applies")
    call.add_argument("--request", type=Path, required=True,
                      help="JSON request object, including action and its existing recipe; no defaults are invented")
    inspect = commands.add_parser("inspect", help="Read a field of the last retained response")
    inspect.add_argument("selector")
    inspect.add_argument("--offset", type=int, default=0)
    inspect.add_argument("--limit", type=int)
    inspect.add_argument("--contains")
    inspect.add_argument("--request-id", help="Inspect an earlier retained response without sending input")
    journal = commands.add_parser("journal", help="Seal immutable evidence before writing the witness; ends play")
    journal.add_argument("--reason", required=True)
    journal.add_argument("--unused-authority", default="released")
    finish = commands.add_parser("finish", help="Submit your witness against the sealed journal")
    finish.add_argument("--witness", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if not math.isfinite(args.wait_seconds) or args.wait_seconds < 0:
            raise ValueError("wait_seconds_must_be_finite_and_nonnegative")
        with session_lock(args.session / "play-client.lock"):
            client = PlayerClient(args.session)
            if args.command == "look":
                if client.state.get("sealed_terminal"):
                    raise ValueError("journal_is_sealed: submit finish --witness FILE")
                result = client.submit({"action": "game.observe"}, args.wait_seconds)
            elif args.command == "collect":
                result = client.collect(args.wait_seconds)
            elif args.command == "act":
                params = {}
                for item in args.param:
                    key, separator, value = item.partition("=")
                    if not separator or not key or key in params:
                        raise ValueError("parameters_need_unique_KEY=VALUE")
                    params[key] = value
                result = client.act(args.action, args.target, params, args.wait_seconds)
            elif args.command == "controls":
                result = client.controls()
            elif args.command == "call":
                result = client.call(json.loads(args.request.read_text()), args.wait_seconds)
            elif args.command == "inspect":
                result = client.inspect(args.selector, args.offset, args.limit, args.contains, args.request_id)
            elif args.command == "journal":
                result = client.journal(args.reason, args.unused_authority, args.wait_seconds)
            else:
                result = client.finish(json.loads(args.witness.read_text()), args.wait_seconds)
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {"ok": False, "error": str(error)}
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
