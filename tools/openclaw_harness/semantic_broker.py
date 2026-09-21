"""Run-only semantic interruption broker.

The broker accepts facts from the authoritative game probe.  It never parses
screen text and it never invents an action for a missing or stale UI instance.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
from typing import Any, Callable, Mapping, Optional, Sequence


@dataclass(frozen=True)
class SemanticUIContext:
    """The currently active semantic UI instance advertised by the game."""

    instance_id: str
    intent: str
    valid_actions: tuple[str, ...]
    postcondition: str

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SemanticUIContext":
        instance_id = str(value.get("instance_id", "")).strip()
        intent = str(value.get("intent", "")).strip()
        actions = value.get("valid_actions", ())
        postcondition = str(value.get("postcondition", "")).strip()
        if not instance_id or not intent or not postcondition:
            raise ValueError("semantic UI context requires identity, intent, and postcondition")
        if isinstance(actions, (str, bytes)):
            raise ValueError("semantic UI valid_actions must be a sequence")
        normalized = tuple(str(action).strip() for action in actions if str(action).strip())
        if not normalized:
            raise ValueError("semantic UI context must advertise at least one action")
        return cls(instance_id, intent, normalized, postcondition)


@dataclass(frozen=True)
class BrokerResult:
    """One bounded broker decision and its machine-readable reason."""

    status: str
    reason: str
    action: Optional[str] = None
    instance_id: Optional[str] = None
    attempts: int = 0
    progress_observed: bool = False

    def as_dict(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "reason": self.reason,
            "action": self.action,
            "instance_id": self.instance_id,
            "attempts": self.attempts,
            "progress_observed": self.progress_observed,
        }


class SemanticInterruptionBroker:
    """Apply only an advertised action, then require bounded progress proof."""

    def __init__(self, *, max_attempts: int = 1) -> None:
        if max_attempts < 1:
            raise ValueError("max_attempts must be positive")
        self.max_attempts = max_attempts

    def recover(
        self,
        *,
        expected_intent: str,
        expected_instance_id: str,
        action: str,
        read_context: Callable[[], Optional[Mapping[str, Any]]],
        send_action: Callable[[str, str], None],
        observe_progress: Callable[[str], bool],
        expected_destination: Optional[str] = None,
    ) -> BrokerResult:
        """Recover one interruption through authoritative context only.

        ``read_context`` and ``observe_progress`` are run-only probe calls.  A
        missing context, wrong destination, unknown identity, or stale identity
        closes without input.  Repeated attempts stop when no progress occurs.
        """
        if not expected_instance_id or not expected_intent or not action:
            return BrokerResult("rejected", "incomplete_expected_identity")
        attempts = 0
        for _ in range(self.max_attempts):
            raw = read_context()
            if raw is None:
                return BrokerResult("rejected", "unknown_ui", attempts=attempts)
            try:
                context = SemanticUIContext.from_mapping(raw)
            except (TypeError, ValueError):
                return BrokerResult("rejected", "malformed_ui", attempts=attempts)
            if context.instance_id != expected_instance_id:
                return BrokerResult("rejected", "stale_ui_identity", attempts=attempts)
            if context.intent != expected_intent:
                return BrokerResult("rejected", "wrong_ui_intent", instance_id=context.instance_id, attempts=attempts)
            if expected_destination is not None and str(raw.get("destination", "")) != expected_destination:
                return BrokerResult("rejected", "wrong_destination", instance_id=context.instance_id, attempts=attempts)
            if action not in context.valid_actions:
                return BrokerResult("rejected", "action_not_advertised", instance_id=context.instance_id, attempts=attempts)
            send_action(context.instance_id, action)
            attempts += 1
            if observe_progress(context.postcondition):
                return BrokerResult("recovered", "postcondition_observed", action=action,
                                    instance_id=context.instance_id, attempts=attempts,
                                    progress_observed=True)
        return BrokerResult("rejected", "recovery_without_progress", action=action,
                            instance_id=expected_instance_id, attempts=attempts)


@dataclass(frozen=True)
class SemanticStepFrame:
    """One run-bound semantic observation.

    Native bindings deliberately remain inside CDDA.  A cockpit frame carries
    action identities only; it never exposes a private key or other physical
    input translation.
    """

    run_id: str
    frame_id: str
    state: str
    observed_turn: Optional[int]
    valid_actions: tuple[str, ...]
    provenance: str
    producer: str
    observation: Optional[Mapping[str, Any]]
    game_minutes: Optional[int]

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SemanticStepFrame":
        run_id = str(value.get("run_id", "")).strip()
        frame_id = str(value.get("frame_id", "")).strip()
        state = str(value.get("state", "")).strip()
        actions = value.get("valid_actions", ())
        observed_turn = value.get("observed_turn")
        if not run_id or not frame_id or not state:
            raise ValueError("semantic frame requires run, frame, and state identity")
        if isinstance(actions, (str, bytes)) or not isinstance(actions, Sequence):
            raise ValueError("semantic frame actions must be a sequence")
        normalized = tuple(str(action).strip() for action in actions if str(action).strip())
        if observed_turn is not None and (isinstance(observed_turn, bool) or not isinstance(observed_turn, int)):
            raise ValueError("semantic frame observed turn must be an integer")
        producer = str(value.get("producer", "")).strip()
        observation = value.get("observation")
        is_native_activity_observation = (
            state == "activity_distraction" and producer == "activity_distraction_query"
        )
        if is_native_activity_observation and (
                not isinstance(observation, Mapping) or
                observation.get("schema") != "caol-avatar-visible-v1" or
                not isinstance(observed_turn, int) or isinstance(observed_turn, bool)):
            raise ValueError("native activity interruption requires an integer-turn avatar observation")
        game_minutes = value.get("game_minutes")
        if game_minutes is not None and (isinstance(game_minutes, bool) or not isinstance(game_minutes, int)):
            raise ValueError("semantic frame game minutes must be an integer")
        return cls(
            run_id, frame_id, state, observed_turn, normalized,
            str(value.get("provenance", "native_semantic_step_trace")), producer,
            dict(observation) if isinstance(observation, Mapping) else None, game_minutes,
        )

    def public(self) -> dict[str, Any]:
        public = {
            "run_id": self.run_id,
            "frame_id": self.frame_id,
            "state": self.state,
            "observed_turn": self.observed_turn,
            "valid_actions": list(self.valid_actions),
            "provenance": self.provenance,
        }
        if self.producer:
            public["producer"] = self.producer
        if self.observation is not None:
            public["observation"] = dict(self.observation)
        if self.game_minutes is not None:
            public["game_minutes"] = self.game_minutes
        return public


class SemanticStepChannel:
    """Validate one adaptive choice and persist its native transition receipt."""

    def __init__(
        self, *, run_id: str, session_id: str, receipt_path: Path,
        read_frame: Callable[[], Mapping[str, Any]],
    ) -> None:
        self.run_id = str(run_id).strip()
        self.session_id = str(session_id).strip()
        self.receipt_path = Path(receipt_path)
        self.read_frame = read_frame
        if not self.run_id or not self.session_id:
            raise ValueError("semantic step channel requires run and worker-session identity")

    def observe(self) -> dict[str, Any]:
        frame = SemanticStepFrame.from_mapping(self.read_frame())
        if frame.run_id != self.run_id:
            raise ValueError("semantic frame belongs to another run")
        return frame.public()

    def _persist(self, receipt: Mapping[str, Any]) -> None:
        self.receipt_path.parent.mkdir(parents=True, exist_ok=True)
        durable = {
            key: value for key, value in receipt.items()
            if not str(key).startswith("_")
        }
        line = json.dumps(durable, sort_keys=True, separators=(",", ":")) + "\n"
        with self.receipt_path.open("a", encoding="utf-8") as handle:
            handle.write(line)
            handle.flush()
            os.fsync(handle.fileno())

    def act(
        self, *, frame_id: str, action_id: str,
        submit_request: Callable[[str, str], Mapping[str, Any]],
        await_transition: Callable[[str, str], Mapping[str, Any]],
    ) -> dict[str, Any]:
        current = SemanticStepFrame.from_mapping(self.read_frame())
        return self.act_observed(
            observed_frame=current,
            frame_id=frame_id,
            action_id=action_id,
            submit_request=submit_request,
            await_transition=await_transition,
        )

    def act_observed(
        self, *, observed_frame: SemanticStepFrame | Mapping[str, Any],
        frame_id: str, action_id: str,
        submit_request: Callable[[str, str], Mapping[str, Any]],
        await_transition: Callable[[str, str], Mapping[str, Any]],
    ) -> dict[str, Any]:
        """Act from one immutable issuing frame without rereading the live UI.

        An interruption may resolve between an observer choosing its advertised
        action and this worker receiving CPU time.  The observed frame remains
        the action authority.  Its matching native response must be recorded
        before a later world frame can be accepted as the next state.
        """
        current = observed_frame if isinstance(observed_frame, SemanticStepFrame) \
                  else SemanticStepFrame.from_mapping(observed_frame)
        receipt: dict[str, Any] = {
            "schema": "caol-semantic-step-receipt-v1",
            "run_id": self.run_id,
            "session_id": self.session_id,
            "frame_id": str(frame_id),
            "action_id": str(action_id),
            "accepted": False,
            "reason": "",
            "current_frame": current.public(),
            "next_frame": None,
            "native_receipt": None,
            "semantic_response": None,
        }
        if current.run_id != self.run_id:
            receipt["reason"] = "run_mismatch"
        elif str(frame_id) != current.frame_id:
            receipt["reason"] = "stale_frame"
        elif str(action_id) not in current.valid_actions:
            receipt["reason"] = "action_not_advertised"
        else:
            submission = submit_request(current.frame_id, str(action_id))
            if not isinstance(submission, Mapping) or submission.get("accepted") is not True:
                receipt["reason"] = "native_surface_transport_failed"
                self._persist(receipt)
                return receipt
            transition = await_transition(current.frame_id, str(action_id))
            native = transition.get("native_receipt")
            response = transition.get("semantic_response")
            next_value = transition.get("next_frame")
            receipt["native_receipt"] = native
            receipt["semantic_response"] = response
            if not isinstance(native, Mapping):
                receipt["reason"] = "native_receipt_missing"
            elif native.get("accepted") is not True:
                receipt["reason"] = "native_action_rejected"
            elif str(native.get("frame_id", "")) != current.frame_id or \
                    str(native.get("action_id", "")) != str(action_id):
                receipt["reason"] = "native_receipt_mismatch"
            elif not isinstance(response, Mapping):
                receipt["reason"] = "semantic_response_missing"
            elif response.get("accepted") is not True:
                receipt["reason"] = "semantic_response_rejected"
            elif str(response.get("frame_id", "")) != current.frame_id or \
                    str(response.get("action_id", "")) != str(action_id):
                receipt["reason"] = "semantic_response_mismatch"
            elif not isinstance(next_value, Mapping):
                receipt["reason"] = "next_frame_missing"
            else:
                next_frame = SemanticStepFrame.from_mapping(next_value)
                if next_frame.run_id != self.run_id or next_frame.frame_id == current.frame_id:
                    receipt["reason"] = "next_frame_not_fresh"
                else:
                    receipt.update({
                        "accepted": True,
                        "reason": "native_transition_accepted",
                        "next_frame": next_frame.public(),
                        "_next_frame": dict(next_value),
                    })
        self._persist(receipt)
        return receipt


__all__ = [
    "BrokerResult", "SemanticInterruptionBroker", "SemanticStepChannel",
    "SemanticStepFrame", "SemanticUIContext",
]
