"""Small, stable public façade over the scenario registry.

The cockpit deliberately returns presentation data only.  Registry authority,
tokens, and launch mechanics remain owned by ``scenario_registry_store`` and
the typed registry CLI.
"""
from __future__ import annotations

import json
import secrets
from typing import Any, Callable, Dict, Mapping, Optional

import startup_harness

from scenario_registry_store import (
    ScenarioRegistryStoreError,
    create_source_bound_scenario,
    execute_registry_query,
    open_registry,
    parse_registry_query_request,
    prepare_selected_scenario,
    registry_status,
    validate_source_bound_scenario,
    capability_contracts,
    capability_gaps,
    cockpit_run_authority_status,
    cockpit_run_status,
    finish_cockpit_run_authority,
    open_cockpit_run,
    record_cockpit_run_receipt,
    report_capability_gap,
)
from playtest_witness import (
    WitnessError,
    build_evidence_journal,
    normalize_witness_charter,
    validate_witness_statement,
)

_FORBIDDEN = {"token", "tokens", "token_id", "query_sha256", "offset", "offsets", "candidate_offsets", "pid", "pids", "ocr", "logs", "raw_logs", "physical_key", "physical_keys", "source_path", "draft_path", "manual", "full_manual", "executable", "executables", "executable_path", "manifest_sha256", "hash", "hashes", "sha256", "path", "paths", "key", "subprocess"}
_ALLOWED = {"action", "frontier", "capability", "scenario", "requirements", "preferences", "id", "name", "detail", "declaration", "world", "required_typeid", "candidate_offsets", "player_save", "handle", "observation_id", "action_id", "recovery", "run_id", "scenario_id", "selection_id", "binding_id", "blocked_intent", "missing_kind", "evidence", "reusable_outcome", "affected_scenarios", "expected_signal", "bound", "stop_reason", "unused_authority", "keep_watch", "raw_wait", "raw_move_relative", "guarded_move_relative", "r019_acceptance_matrix", "witness"}

_BOUND_BASES = {
    "game_mechanic", "scheduler_boundary", "path_progress", "measured_rate",
}

_KEEP_WATCH_SAFE_CLASSIFICATIONS = {"safe_flavour", "safe_prompt"}


class CockpitRunChannel:
    """Read avatar-visible facts from the current native semantic frame only."""

    def __init__(
        self, read_native_frame: Callable[[], Mapping[str, Any]],
        dispatch_advertised_action: Optional[Callable[[Mapping[str, Any], str], Mapping[str, Any]]] = None,
        *, read_evidence: Optional[Callable[[], Mapping[str, Any]]] = None,
        binding_id: str = "",
        read_binding_id: Optional[Callable[[], str]] = None,
        mutate_binding_for_control: Optional[Callable[[], Mapping[str, Any]]] = None,
        finalize_session: Optional[Callable[[Mapping[str, Any]], Mapping[str, Any]]] = None,
        await_native_completion: Optional[Callable[[str], None]] = None,
        enforce_continuation_bounds: bool = False,
        proof_step_label: str = "",
        proof_step_index: Optional[int] = None,
        r019_timed_entry: Optional[Mapping[str, Any]] = None,
        diagnostic_terminal: Optional[Mapping[str, Any]] = None,
        witness_charter: Optional[Mapping[str, Any]] = None,
        witness_identity: Optional[Mapping[str, Any]] = None,
        witness_evidence_ceiling: str = "focused",
        causal_boundary_precondition: Optional[Callable[[], Mapping[str, Any]]] = None,
    ) -> None:
        self._read_native_frame = read_native_frame
        self._dispatch_advertised_action = dispatch_advertised_action
        self._run_id = ""
        self._handles: Dict[str, Dict[str, Any]] = {}
        self._observations: Dict[str, Dict[str, Any]] = {}
        self._next_handle = 0
        self._read_evidence = read_evidence
        self._binding_id = str(binding_id)
        self._read_binding_id = read_binding_id
        self._mutate_binding_for_control = mutate_binding_for_control
        self._finalize_session = finalize_session
        self._await_native_completion = await_native_completion
        self._enforce_continuation_bounds = enforce_continuation_bounds
        self._proof_step_label = str(proof_step_label).strip()
        self._proof_step_index = proof_step_index
        self._r019_timed_entry = dict(r019_timed_entry or {})
        self._r019_timed_entry_receipt: Optional[Dict[str, Any]] = None
        self._diagnostic_terminal = dict(diagnostic_terminal or {})
        self._state = "active"
        self._continuation: Optional[Dict[str, Any]] = None
        self._safe_activity_bridge: Optional[Dict[str, Any]] = None
        self._last_public_state: Optional[Dict[str, Any]] = None
        self._transcript: list[Dict[str, Any]] = []
        self._final_report: Optional[Dict[str, Any]] = None
        self._witness_charter = normalize_witness_charter(witness_charter) \
            if isinstance(witness_charter, Mapping) else None
        self._witness_identity = dict(witness_identity or {})
        self._witness_evidence_ceiling = str(witness_evidence_ceiling)
        self._sealed_journal: Optional[Dict[str, Any]] = None
        self._sealed_witness_terminal: Optional[Dict[str, Any]] = None
        self._causal_boundary_precondition = causal_boundary_precondition
        self._relative_recipe_active = False

    @staticmethod
    def _frame(frame: Mapping[str, Any]) -> tuple[str, str, Optional[int], Mapping[str, Any], tuple[str, ...]]:
        run_id = str(frame.get("run_id", "")).strip()
        frame_id = str(frame.get("frame_id", "")).strip()
        turn = frame.get("observed_turn")
        observation = frame.get("observation")
        is_activity_interruption = (
            frame.get("provenance") == "native_activity_distraction_query" and
            frame.get("state") == "activity_distraction"
        )
        is_semantic_ui = (
            frame.get("provenance") == "native_semantic_ui_trace" and
            frame.get("state") == "semantic_ui"
        )
        has_native_observation = isinstance(observation, Mapping)
        is_observed_activity_interruption = (
            is_activity_interruption and
            frame.get("producer") == "activity_distraction_query"
        )
        if not run_id or not frame_id or \
                ((not is_activity_interruption and not is_semantic_ui or
                  is_observed_activity_interruption) and has_native_observation and (
                    isinstance(turn, bool) or not isinstance(turn, int) or
                    observation.get("schema") != "caol-avatar-visible-v1"
                )) or \
                (not has_native_observation and not is_activity_interruption and not is_semantic_ui):
            raise ValueError("current native avatar observation is unavailable")
        # Legacy activity-query and semantic-UI traces carry no avatar
        # observation.  A native activity interruption frame does, and must
        # retain it: replacing it with the old empty placeholder fabricated a
        # disappearance at the cockpit boundary.
        if (is_activity_interruption or is_semantic_ui) and not has_native_observation:
            observation = {
                "schema": "caol-avatar-visible-v1",
                "avatar": {},
                "visible_local": [],
            }
        avatar = observation.get("avatar")
        facts = observation.get("visible_local")
        actions = frame.get("valid_actions")
        if not isinstance(avatar, Mapping) or not isinstance(facts, list) or \
                isinstance(actions, (str, bytes)) or not isinstance(actions, list) or \
                any(not isinstance(action, str) or not action for action in actions):
            raise ValueError("native avatar observation is malformed")
        return run_id, frame_id, turn, observation, tuple(actions)

    @staticmethod
    def _identity(fact: Mapping[str, Any]) -> Optional[str]:
        identity = fact.get("identity")
        if not isinstance(identity, Mapping):
            return None
        kind = str(identity.get("kind", "")).strip()
        durable_id = str(identity.get("id", "")).strip()
        if kind and durable_id:
            return json.dumps({"kind": kind, "id": durable_id}, sort_keys=True)
        terrain = fact.get("terrain")
        if not isinstance(terrain, str):
            return None
        dx = identity.get("dx")
        dy = identity.get("dy")
        terrain_id = identity.get("terrain")
        if isinstance(dx, bool) or isinstance(dy, bool) or not isinstance(dx, int) or \
                not isinstance(dy, int) or not isinstance(terrain_id, str) or not terrain_id:
            return None
        return f"{dx}:{dy}:{terrain_id}"

    def _binding_matches(self) -> bool:
        if self._read_binding_id is None:
            return True
        return bool(self._binding_id) and self._read_binding_id() == self._binding_id

    def controlled_binding_drift(
        self, *, observation_id: str, attempted_action: str,
        r019_acceptance_matrix: Mapping[str, Any],
    ) -> Dict[str, Any]:
        """Exercise the declared zero-credit R-019 binding-drift control.

        The control owns its one harmless runtime-binding mutation.  It cannot
        dispatch an action after that mutation and immediately seals the
        fail-closed terminal receipt.
        """
        if self._state != "active":
            return {"ok": False, "error": "live_session_finished", "final": self._final_report}
        observed = self._observations.get(str(observation_id))
        if observed is None or observed["used"]:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        if self._continuation is None or self._continuation.get("observation_id") != str(observation_id):
            return {"ok": False, "error": "continuation_bound_required"}
        if self._mutate_binding_for_control is None or not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        role = str(r019_acceptance_matrix.get("role", "")).strip()
        if role != "off:enabled" or not str(attempted_action).strip():
            return {"ok": False, "error": "r019_binding_drift_control_requires_disabled_recipe_and_action"}
        before = self._binding_id
        mutation = self._mutate_binding_for_control()
        after = self._read_binding_id() if self._read_binding_id is not None else ""
        if not isinstance(mutation, Mapping) or not after or after == before:
            return {"ok": False, "error": "r019_binding_drift_control_mutation_unproved"}
        packet = self._r019_receipt(r019_acceptance_matrix, require_primitive_dispatch=True)
        if packet is None:
            return {"ok": False, "error": "r019_off_switch_does_not_match_primitive_live_transcript"}
        receipt = {
            "schema": "caol-r019-binding-drift-terminal-receipt-v1",
            "before": {"identity": "runtime_binding", "hash": before},
            "supported_drift_mutation_receipt": dict(mutation),
            "after": {"identity": "runtime_binding", "hash": after},
            "attempted_action": str(attempted_action),
            "stop_reason": "binding_drift",
            "native_dispatch_after_drift": False,
            "native_dispatch_count_after_drift": 0,
            "guarded_recipe_dispatch_count": 0,
            "guarded_handling_count": 0,
            "hidden_batching": False,
        }
        return self._fail_closed("binding_drift", {
            "observation_id": str(observation_id), "unused_authority": "revoked",
            "r019_acceptance_matrix": packet, "binding_drift_receipt": receipt,
        })

    @staticmethod
    def _signal_value(state: Mapping[str, Any], signal: str) -> Optional[float]:
        if signal == "game_minutes":
            value = state.get("game_minutes")
        elif signal == "scheduler_sequence":
            value = state.get("compact_log", {}).get("latest_transition", {}).get("sequence")
        elif signal == "receipt_sequence":
            value = state.get("compact_log", {}).get("receipt_count")
        else:
            value = None
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return None
        return float(value)

    def _stop(self, reason: str, detail: Mapping[str, Any]) -> Dict[str, Any]:
        if self._final_report is not None:
            return self._final_report
        report: Dict[str, Any] = {
            "schema": "caol-cockpit-live-final-v1",
            "run_id": self._run_id,
            "binding_id": self._binding_id,
            "state": "finished",
            "stop_reason": str(reason),
            "stop_detail": dict(detail),
            "action_observation_sequence": list(self._transcript),
            "bound_derivation": dict(self._continuation or {}),
            "unused_authority": str(detail.get("unused_authority", "none")),
        }
        if self._finalize_session is not None:
            finalized = self._finalize_session(report)
            if not isinstance(finalized, Mapping):
                report["cleanup"] = {"status": "invalid_finalizer_result"}
            else:
                report.update(dict(finalized))
        self._state = "finished"
        self._final_report = report
        return report

    def _fail_closed(self, reason: str, detail: Mapping[str, Any]) -> Dict[str, Any]:
        report = self._stop(reason, detail)
        return {"ok": False, "error": reason, "final": report}

    def observe(self) -> Dict[str, Any]:
        if self._state != "active":
            raise ValueError("live session is finished")
        if not self._binding_matches():
            self._fail_closed("binding_drift", {"unused_authority": "revoked"})
            raise ValueError("binding drift stopped the live session")
        issuing_raw = dict(self._read_native_frame())
        run_id, frame_id, turn, observation, actions = self._frame(issuing_raw)
        bridge = self._safe_activity_bridge
        if isinstance(bridge, Mapping) and \
                bridge.get("run_id") == run_id and bridge.get("frame_id") == frame_id:
            if issuing_raw.get("provenance") == "native_activity_distraction_query" and \
                    issuing_raw.get("state") == "activity_distraction" and \
                    issuing_raw.get("activity_type") == bridge.get("activity_type") and \
                    bridge.get("action_id") in actions:
                # A declared safe popup can expose the activity query that
                # interrupted the already-bound wait only after its own
                # acknowledgement returns.  Carry that one exact recovery
                # authority across the native frame; no other activity query
                # inherits it.
                issuing_raw["keep_watch_safety"] = {
                    "classification": "safe_prompt",
                    "monster": False,
                    "danger": False,
                    "damage": False,
                    "action_id": bridge["action_id"],
                    "recovery": {"modal_id": bridge["modal_id"]},
                }
                issuing_raw["safe_recovery"] = {
                    "modal_id": bridge["modal_id"],
                    "actions": [bridge["action_id"]],
                }
            self._safe_activity_bridge = None
        if self._continuation is not None and \
                run_id != str(self._continuation.get("run_id", run_id)):
            self._fail_closed("continuation_wrong_run", {
                "expected_run_id": self._continuation.get("run_id", ""),
                "observed_run_id": run_id,
                "unused_authority": "revoked",
            })
            raise ValueError("continuation belongs to a different run")
        if self._run_id and self._run_id != run_id:
            self._handles = {}
            self._observations = {}
            self._next_handle = 0
        self._run_id = run_id
        facts = []
        for fact in observation["visible_local"]:
            if not isinstance(fact, Mapping):
                continue
            identity = self._identity(fact)
            if identity is None:
                continue
            handle = next((key for key, value in self._handles.items()
                           if value["identity"] == identity), None)
            if handle is None:
                self._next_handle += 1
                handle = f"visible:{run_id}:{self._next_handle}"
                self._handles[handle] = {"identity": identity, "run_id": run_id}
            facts.append({"handle": handle, "terrain": fact["terrain"]})
        entities = []
        raw_entities = observation.get("visible_entities", [])
        if isinstance(raw_entities, list):
            for fact in raw_entities:
                if not isinstance(fact, Mapping):
                    continue
                identity = self._identity(fact)
                if identity is None:
                    continue
                handle = next((key for key, value in self._handles.items()
                               if value["identity"] == identity), None)
                if handle is None:
                    self._next_handle += 1
                    handle = f"visible:{run_id}:{self._next_handle}"
                    self._handles[handle] = {"identity": identity, "run_id": run_id}
                entities.append({
                    "handle": handle,
                    "kind": str(fact.get("kind", "creature")),
                    "name": str(fact.get("name", "")),
                    "attitude": str(fact.get("attitude", "unknown")),
                    "dx": fact.get("dx"),
                    "dy": fact.get("dy"),
                    "fixture_actor_id": str(fact.get("fixture_actor_id", "")),
                    "typeid": str(fact.get("typeid", "")),
                    "faction": str(fact.get("faction", "")),
                    "friendly": fact.get("friendly"),
                    "aggro_character": fact.get("aggro_character"),
                })
        evidence = dict(self._read_evidence()) if self._read_evidence is not None else {}
        compact_log = {
            "receipt_count": int(evidence.get("receipt_count", 0) or 0),
            "latest_receipt": evidence.get("latest_receipt"),
            "first_divergence": evidence.get("first_divergence"),
            "contradictory_evidence": list(evidence.get("contradictory_evidence", [])),
            "latest_transition": evidence.get("latest_transition"),
            "actor_owners": list(evidence.get("actor_owners", [])),
            "persistence": evidence.get("persistence", "unavailable"),
            "evidence_refs": list(evidence.get("evidence_refs", [])),
            "scheduler_trace": list(evidence.get("scheduler_trace", [])),
            "unsafe": evidence.get("unsafe") is True,
        }
        result = {
            "observation_id": frame_id,
            "run_id": run_id,
            "observed_turn": turn,
            "game_minutes": issuing_raw.get("game_minutes"),
            "avatar": {"name": str(observation["avatar"].get("name", ""))},
            "visible_local": facts,
            "minimap": observation.get("minimap", {
                "schema": "caol-native-minimap-v1", "cells": facts,
            }),
            "visible_entities": entities,
            "visible_zones": observation.get("visible_zones", []),
            "advertised_actions": list(actions),
            "compact_log": compact_log,
            "expected_postcondition": "matching_native_receipt_and_fresh_observation",
            "evidence_effect": "native_transition_receipt_persisted",
        }
        if issuing_raw.get("provenance") == "native_activity_distraction_query" and \
                issuing_raw.get("state") == "activity_distraction":
            result["active_interruption"] = {
                "id": frame_id,
                "type": str( issuing_raw.get( "activity_type", "" ) ),
                "owner": "native_activity_distraction_query",
            }
        current_public_state = {
            "observation_id": frame_id,
            "game_minutes": result.get("game_minutes"),
            "handles": sorted(
                [str(item["handle"]) for item in facts] +
                [str(item["handle"]) for item in entities]
            ),
            "receipt_count": compact_log["receipt_count"],
            "latest_transition": compact_log["latest_transition"],
        }
        if self._last_public_state is None:
            result["delta"] = {"kind": "full"}
        else:
            prior_handles = set(self._last_public_state["handles"])
            current_handles = set(current_public_state["handles"])
            result["delta"] = {
                "kind": "change",
                "game_minutes": {
                    "before": self._last_public_state.get("game_minutes"),
                    "after": current_public_state.get("game_minutes"),
                },
                "added_handles": sorted(current_handles - prior_handles),
                "removed_handles": sorted(prior_handles - current_handles),
                "new_receipts": max(
                    0, compact_log["receipt_count"] -
                    int(self._last_public_state.get("receipt_count", 0) or 0)
                ),
                "latest_transition": compact_log["latest_transition"],
            }
        continuation_update = self._resolve_continuation_on_frame(
            issuing_raw, current_public_state,
        )
        if self._state != "active":
            raise ValueError("live session is finished")
        if continuation_update is not None:
            result["continuation"] = continuation_update
        self._last_public_state = current_public_state
        self._observations[frame_id] = {
            "run_id": run_id, "actions": set(actions),
            "handles": {str(fact["handle"]) for fact in facts + entities}, "used": False,
            "public_state": result,
            # Keep this native frame private.  An activity interruption may
            # close before game.act runs; its issuing frame remains the only
            # authority for the advertised recovery input.
            "issuing_frame": issuing_raw,
            "native_interruption": issuing_raw.get("provenance") in {
                "native_activity_distraction_query", "native_semantic_ui_trace",
            },
        }
        self._transcript.append({"kind": "observation", "value": result})
        return result

    def _resolve_continuation_on_frame(
        self, raw_frame: Mapping[str, Any], public_state: Mapping[str, Any],
    ) -> Optional[Dict[str, Any]]:
        """Resolve an asynchronous wait only after its later native completion frame."""
        continuation = self._continuation
        if continuation is None or continuation.get("phase") != "awaiting_native_completion":
            return None
        frame_id = str(raw_frame.get("frame_id", ""))
        if frame_id == str(continuation.get("activity_frame_id", "")) or \
                str(raw_frame.get("state", "")) == "wait_activity":
            return {"state": "awaiting_native_completion"}
        if raw_frame.get("provenance") in {
                "native_activity_distraction_query", "native_semantic_ui_trace",
        }:
            continuation["observation_id"] = frame_id
            return {"state": "awaiting_native_completion"}
        if str(raw_frame.get("state", "")) not in {"world", "wait_activity_complete"}:
            return {"state": "awaiting_native_completion"}
        after = self._signal_value(public_state, continuation["expected_signal"])
        before = float(continuation["start"])
        self._continuation = None
        if after is None:
            self._fail_closed("continuation_completion_signal_missing", {
                "expected_signal": continuation["expected_signal"],
                "unused_authority": "revoked",
            })
            return None
        # A public bound permits the advertised action to consume its declared
        # maximum exactly.  Only progress beyond that evidence-derived span is
        # an exhaustion; equality is the ordinary completion boundary from
        # which a fresh observation may authorize an independent continuation.
        if after > before + float(continuation["maximum"]):
            self._fail_closed("derived_bound_exhausted", {
                "expected_signal": continuation["expected_signal"],
                "observed": after, "bound": continuation,
                "unused_authority": "none",
            })
            return None
        if continuation["progress_required"] and after == before:
            self._fail_closed("proved_no_progress", {
                "expected_signal": continuation["expected_signal"],
                "before": before, "after": after,
                "unused_authority": "revoked",
            })
            return None
        self._transcript.append({
            "kind": "continuation_completion", "value": {
                "expected_signal": continuation["expected_signal"],
                "before": before, "after": after,
            },
        })
        return {"state": "completed", "before": before, "after": after}

    def status(self) -> Dict[str, Any]:
        return {
            "run_id": self._run_id,
            "binding_id": self._binding_id,
            "state": self._state,
            "continuation": dict(self._continuation or {}),
            "final": self._final_report,
        }

    def continue_session(
        self, *, observation_id: str, expected_signal: str, bound: Mapping[str, Any],
    ) -> Dict[str, Any]:
        if self._state != "active":
            return {"ok": False, "error": "live_session_finished", "final": self._final_report}
        if self._causal_boundary_precondition is not None:
            boundary = self._causal_boundary_precondition()
            if boundary.get("status") == "matched":
                return {
                    "ok": False,
                    "error": "causal_boundary_reached",
                    "causal_boundary": dict(boundary),
                    "next_action": "run.finish",
                }
        if not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        observed = self._observations.get(str(observation_id))
        if observed is None or observed["used"]:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        if observed["public_state"].get("compact_log", {}).get("unsafe") is True:
            return self._fail_closed("unsafe_divergence", {
                "first_divergence": observed["public_state"]["compact_log"].get(
                    "first_divergence"
                ),
                "unused_authority": "revoked",
            })
        basis = str(bound.get("basis", "")).strip()
        source = str(bound.get("source", "")).strip()
        unit = str(bound.get("unit", "")).strip()
        supplied_start = bound.get("start")
        maximum = bound.get("maximum")
        progress_required = bound.get("progress_required", False)
        if basis not in _BOUND_BASES or not source or not unit or \
                (supplied_start is not None and (
                    isinstance(supplied_start, bool) or
                    not isinstance(supplied_start, (int, float))
                )) or \
                isinstance(maximum, bool) or not isinstance(maximum, (int, float)) or maximum <= 0 or \
                not isinstance(progress_required, bool):
            return {"ok": False, "error": "invalid_evidence_derived_bound"}
        signal = str(expected_signal).strip()
        current = self._signal_value(observed["public_state"], signal)
        if current is None:
            return {"ok": False, "error": "current_signal_is_unavailable"}
        # The observation is the only authority for a new continuation's
        # origin.  A caller may echo it to make its request auditable, but it
        # may not manufacture a different start from an earlier frame.
        if supplied_start is not None and current != float(supplied_start):
            return {"ok": False, "error": "bound_start_does_not_match_current_signal"}
        start = current
        if current >= start + float(maximum):
            return self._fail_closed("derived_bound_exhausted", {
                "expected_signal": signal, "bound": dict(bound),
                "unused_authority": "none",
            })
        prior_continuation = self._continuation
        self._continuation = {
            "run_id": observed["run_id"],
            "observation_id": str(observation_id),
            "expected_signal": signal,
            "basis": basis,
            "source": source,
            "unit": unit,
            "start": float(start),
            "maximum": float(maximum),
            "progress_required": progress_required,
            "pre_wait_observation_id": str(observation_id),
            "pre_wait_game_minutes": float(start),
        }
        if isinstance(prior_continuation, Mapping) and \
                prior_continuation.get("run_id") == observed["run_id"]:
            # A fresh menu frame needs a renewed one-use observation authority,
            # but remains within the same native wait transaction.  Preserve
            # the private phase so an immediate native interruption can still
            # terminate that transaction rather than being flattened into a
            # generic continuation.
            for key in ("phase", "activity_frame_id"):
                if key in prior_continuation:
                    self._continuation[key] = prior_continuation[key]
            # Menu frames renew the one-use action authority but cannot
            # replace the source-bound time from which this wait began.
            for key in ("pre_wait_observation_id", "pre_wait_game_minutes"):
                if key in prior_continuation:
                    self._continuation[key] = prior_continuation[key]
        event = {"kind": "continuation", "value": dict(self._continuation)}
        self._transcript.append(event)
        return {"ok": True, "result": dict(self._continuation), "state": "active"}

    def keep_watch(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        """Run one classified-safe wait recipe without granting it new input authority.

        The recipe is deliberately made from advertised primitive actions.  Either
        the master or recipe off switch performs no action, and a frame that does
        not carry a native, explicit keep-watch classification is terminal rather
        than guessed at.
        """
        master_enabled = request.get("master_enabled", True)
        if master_enabled is not True:
            result = {"ok": False, "error": "keep_watch_master_disabled_use_primitive_actions"}
            self._transcript.append({"kind": "keep_watch_off", "switch": "master_enabled",
                                     "result": result})
            return result
        enabled = request.get("enabled")
        if enabled is not True:
            result = {"ok": False, "error": "keep_watch_disabled_use_primitive_actions"}
            self._transcript.append({"kind": "keep_watch_off", "switch": "enabled",
                                     "result": result})
            return result
        target = request.get("target_game_minutes")
        bound = request.get("bound")
        recipe = request.get("recipe")
        if isinstance(target, bool) or not isinstance(target, (int, float)) or \
                not isinstance(bound, Mapping) or isinstance(recipe, (str, bytes)) or \
                not isinstance(recipe, list) or not recipe or \
                any(not isinstance(action, str) or not action.strip() for action in recipe):
            return {"ok": False, "error": "invalid_keep_watch_recipe"}
        maximum = bound.get("maximum")
        if isinstance(maximum, bool) or not isinstance(maximum, (int, float)) or maximum <= 0:
            return {"ok": False, "error": "invalid_evidence_derived_bound"}

        try:
            observed = self.observe()
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        start = self._signal_value(observed, "game_minutes")
        if start is None or target <= start or target > start + float(maximum):
            return {"ok": False, "error": "keep_watch_target_outside_derived_bound"}
        recipe_index = 0
        tool_round_trips = 0
        safety_frames = 0
        while True:
            if self._state != "active":
                return {"ok": False, "error": "live_session_finished", "final": self._final_report}
            current = self._signal_value(observed, "game_minutes")
            if current == float(target):
                result = {
                    "stop_reason": "target_reached",
                    "terminal_observation": observed,
                    "native_action_count": tool_round_trips,
                    "model_round_trips": 1,
                    "tool_round_trips": tool_round_trips,
                    "safety_frame_count": safety_frames,
                }
                self._transcript.append({"kind": "keep_watch", "result": result})
                return {"ok": True, "result": result}
            if current is not None and current > float(target):
                return self._fail_closed("target_crossed", {
                    "target_game_minutes": target, "observed_game_minutes": current,
                    "unused_authority": "revoked",
                })
            record = self._observations.get(str(observed.get("observation_id", "")))
            raw = record.get("issuing_frame") if isinstance(record, Mapping) else None
            if isinstance(raw, Mapping) and raw.get("state") == "wait_activity":
                if self._await_native_completion is None:
                    return self._fail_closed("native_wait_completion_unavailable", {
                        "observation_id": observed.get("observation_id", ""),
                        "unused_authority": "revoked",
                    })
                try:
                    self._await_native_completion(str(observed.get("observation_id", "")))
                    observed = self.observe()
                except ValueError as exc:
                    return self._fail_closed("native_wait_completion_unavailable", {
                        "detail": str(exc), "unused_authority": "revoked",
                    })
                continue
            safety = raw.get("keep_watch_safety") if isinstance(raw, Mapping) else None
            if not isinstance(safety, Mapping):
                return self._fail_closed("keep_watch_unknown_safety_frame", {
                    "observation_id": observed.get("observation_id", ""),
                    "unused_authority": "revoked",
                })
            classification = str(safety.get("classification", ""))
            if safety.get("monster") is True or safety.get("danger") is True or \
                    safety.get("damage") is True:
                return self._fail_closed("keep_watch_unsafe_condition", {
                    "classification": classification,
                    "unused_authority": "revoked",
                })
            safety_frames += 1
            if classification == "clear":
                if current is None:
                    return self._fail_closed("keep_watch_progress_signal_missing", {
                        "unused_authority": "revoked",
                    })
                state = str(raw.get("state", "")) if isinstance(raw, Mapping) else ""
                advertised = record.get("actions") if isinstance(record, Mapping) else None
                if not isinstance(advertised, set):
                    return self._fail_closed("keep_watch_advertised_actions_missing", {
                        "observation_id": observed.get("observation_id", ""),
                        "unused_authority": "revoked",
                    })
                if state == "wait_mode_choice":
                    # The native wait chooser is a fresh frame between the
                    # primitive ``world.wait`` action and the recipe's chosen
                    # duration.  Dispatch only the action this exact frame
                    # advertises; never carry a prior recipe action or input
                    # into this native menu.
                    action_id = "wait.duration_menu"
                elif state == "wait_duration_choice":
                    # A prior primitive transaction can leave the duration
                    # chooser open before Keep watch receives its first
                    # observation.  The cursor then still names the earlier
                    # world action, which this frame must never re-dispatch.
                    # Select only one duration action that both the recipe
                    # authorizes and this exact native frame advertises.
                    duration_actions = sorted({
                        action for action in recipe if action in advertised
                    })
                    if len(duration_actions) != 1:
                        return self._fail_closed("keep_watch_recipe_action_not_advertised", {
                            "observation_id": observed.get("observation_id", ""),
                            "state": state,
                            "action_id": (
                                duration_actions[0] if len(duration_actions) == 1 else ""
                            ),
                            "advertised_actions": sorted(advertised),
                            "recipe_actions": list(recipe),
                            "duration_action_resolution": (
                                "absent" if not duration_actions else "ambiguous"
                            ),
                            "recipe_cursor": recipe_index,
                            "unused_authority": "revoked",
                        })
                    action_id = duration_actions[0]
                    recipe_index = (recipe.index(action_id) + 1) % len(recipe)
                else:
                    action_id = recipe[recipe_index % len(recipe)]
                    recipe_index += 1
                if action_id not in advertised:
                    return self._fail_closed("keep_watch_recipe_action_not_advertised", {
                        "observation_id": observed.get("observation_id", ""),
                        "state": state,
                        "action_id": action_id,
                        "advertised_actions": sorted(advertised),
                        "recipe_cursor": recipe_index,
                        "unused_authority": "revoked",
                    })
                recovery = None
            elif classification in _KEEP_WATCH_SAFE_CLASSIFICATIONS:
                action_id = str(safety.get("action_id", "")).strip()
                recovery = safety.get("recovery")
                if not action_id or not isinstance(recovery, Mapping):
                    return self._fail_closed("keep_watch_safe_interruption_unbound", {
                        "classification": classification, "unused_authority": "revoked",
                    })
            else:
                return self._fail_closed("keep_watch_meaningful_or_unknown_event", {
                    "classification": classification, "unused_authority": "revoked",
                })
            if current is None:
                # A run-bound safe semantic UI frame interrupts an already
                # authorized native wait.  It has no independent progress
                # signal, so acknowledge its declared recovery without
                # replacing the pending wait continuation.
                outcome = self.act(
                    observation_id=str(observed["observation_id"]), action_id=action_id,
                    recovery=recovery if isinstance(recovery, Mapping) else None,
                )
                tool_round_trips += 1
                if outcome.get("ok") is not True:
                    return outcome
                next_observation = outcome.get("observation")
                if isinstance(next_observation, Mapping):
                    observed = dict(next_observation)
                    continue
                try:
                    observed = self.observe()
                except ValueError as exc:
                    return {"ok": False, "error": str(exc)}
                continue
            continuation_bound = dict(bound)
            continuation_bound["maximum"] = min(float(maximum), float(target) - current)
            continued = self.continue_session(
                observation_id=str(observed["observation_id"]),
                expected_signal="game_minutes", bound=continuation_bound,
            )
            if continued.get("ok") is not True:
                return continued
            outcome = self.act(
                observation_id=str(observed["observation_id"]), action_id=action_id,
                recovery=recovery if isinstance(recovery, Mapping) else None,
            )
            tool_round_trips += 1
            if outcome.get("ok") is not True:
                return outcome
            next_observation = outcome.get("observation")
            if isinstance(next_observation, Mapping):
                observed = dict(next_observation)
                continue
            try:
                observed = self.observe()
            except ValueError as exc:
                return {"ok": False, "error": str(exc)}

    def raw_wait(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        """Run one bounded primitive wait transaction without interpreting it.

        This is deliberately narrower than :meth:`keep_watch`: no safety
        classification is accepted as permission to recover, and every native
        non-wait frame is terminal.  The public result retains each accepted
        native dispatch receipt in order so it can be compared directly with a
        primitive client executing the same advertised actions.
        """
        if request.get("master_enabled", True) is not True:
            result = {"ok": False, "error": "raw_wait_master_disabled_use_primitive_actions"}
            self._transcript.append({"kind": "raw_wait_off", "switch": "master_enabled", "result": result})
            return result
        if request.get("enabled") is not True:
            result = {"ok": False, "error": "raw_wait_disabled_use_primitive_actions"}
            self._transcript.append({"kind": "raw_wait_off", "switch": "enabled", "result": result})
            return result
        target = request.get("target_game_minutes")
        bound = request.get("bound")
        recipe = request.get("recipe")
        if isinstance(target, bool) or not isinstance(target, (int, float)) or \
                not isinstance(bound, Mapping) or isinstance(recipe, (str, bytes)) or \
                not isinstance(recipe, list) or not recipe or \
                any(not isinstance(action, str) or not action.strip() for action in recipe):
            return {"ok": False, "error": "invalid_raw_wait_recipe"}
        maximum = bound.get("maximum")
        if isinstance(maximum, bool) or not isinstance(maximum, (int, float)) or maximum <= 0:
            return {"ok": False, "error": "invalid_evidence_derived_bound"}
        try:
            observed = self.observe()
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        start = self._signal_value(observed, "game_minutes")
        if start is None or target <= start:
            return {"ok": False, "error": "raw_wait_target_invalid"}
        receipts: list[Mapping[str, Any]] = []
        recipe_index = 0

        def stop(reason: str, detail: Mapping[str, Any]) -> Dict[str, Any]:
            current = self._signal_value(observed, "game_minutes")
            report = self._fail_closed(reason, {
                **dict(detail),
                "target_game_minutes": target,
                "start_game_minutes": start,
                "partial_progress": None if current is None else current - start,
                "native_receipts": [dict(receipt) for receipt in receipts],
                "guarded_handling_count": 0,
                "unused_authority": "revoked",
            })
            return report

        while True:
            current = self._signal_value(observed, "game_minutes")
            if current is None:
                return stop("raw_wait_progress_signal_missing", {})
            if current == float(target):
                result = {
                    "stop_reason": "target_reached",
                    "terminal_observation": observed,
                    "native_receipts": [dict(receipt) for receipt in receipts],
                    "partial_progress": current - start,
                    "derived_bound": dict(bound),
                    "guarded_handling_count": 0,
                }
                self._transcript.append({"kind": "raw_wait", "result": result})
                return {"ok": True, "result": result}
            if current > float(target):
                return stop("target_crossed", {"observed_game_minutes": current})
            if current >= start + float(maximum):
                return stop("derived_bound_exhausted", {"observed_game_minutes": current})
            record = self._observations.get(str(observed.get("observation_id", "")))
            raw = record.get("issuing_frame") if isinstance(record, Mapping) else None
            if not isinstance(raw, Mapping):
                return stop("raw_wait_stale_frame", {})
            state = str(raw.get("state", ""))
            if raw.get("provenance") in {
                    "native_activity_distraction_query", "native_semantic_ui_trace",
            } or state == "activity_distraction":
                return stop("native_wait_interrupted", {
                    "native_stop_reason": str(raw.get("activity_type", state or "native_interruption")),
                    "native_frame_id": observed.get("observation_id", ""),
                })
            safety = raw.get("keep_watch_safety")
            if isinstance(safety, Mapping) and any(safety.get(key) is True for key in
                                                   ("monster", "danger", "damage")):
                return stop("native_wait_interrupted", {
                    "native_stop_reason": str(safety.get("classification", "unsafe_native_frame")),
                    "native_frame_id": observed.get("observation_id", ""),
                })
            if state == "wait_activity":
                if self._await_native_completion is None:
                    return stop("native_wait_completion_unavailable", {})
                try:
                    self._await_native_completion(str(observed.get("observation_id", "")))
                    observed = self.observe()
                except ValueError as exc:
                    return stop("native_wait_completion_unavailable", {"detail": str(exc)})
                continue
            advertised = record.get("actions") if isinstance(record, Mapping) else None
            if not isinstance(advertised, set):
                return stop("raw_wait_stale_frame", {})
            action_id = recipe[recipe_index % len(recipe)]
            recipe_index += 1
            if action_id not in advertised:
                return stop("native_wait_interrupted", {
                    "native_stop_reason": "wait_action_not_advertised",
                    "action_id": action_id,
                    "advertised_actions": sorted(advertised),
                })
            continuation_bound = dict(bound)
            continuation_bound["maximum"] = min(float(maximum) - (current - start),
                                                float(target) - current)
            continued = self.continue_session(
                observation_id=str(observed["observation_id"]),
                expected_signal="game_minutes", bound=continuation_bound,
            )
            if continued.get("ok") is not True:
                return continued
            outcome = self.act(
                observation_id=str(observed["observation_id"]), action_id=action_id,
            )
            if outcome.get("ok") is not True:
                error = str(outcome.get("error", "native_wait_dispatch_failed"))
                if error == "stale_observation":
                    return stop("raw_wait_stale_frame", {"native_stop_reason": error})
                return outcome
            receipt = outcome.get("receipt")
            if isinstance(receipt, Mapping):
                receipts.append(receipt)
            next_observation = outcome.get("observation")
            if isinstance(next_observation, Mapping):
                observed = dict(next_observation)
                continue
            try:
                observed = self.observe()
            except ValueError as exc:
                return stop("raw_wait_stale_frame", {"detail": str(exc)})

    @staticmethod
    def _absolute_ms(observation: Mapping[str, Any]) -> Optional[list[int]]:
        avatar = observation.get("avatar")
        value = avatar.get("absolute_ms") if isinstance(avatar, Mapping) else None
        if not isinstance(value, list) or len(value) != 3 or any(
                isinstance(item, bool) or not isinstance(item, int) for item in value):
            return None
        return list(value)

    def _relative_position(self, observed: Mapping[str, Any]) -> Optional[list[int]]:
        record = self._observations.get(str(observed.get("observation_id", "")))
        raw = record.get("issuing_frame") if isinstance(record, Mapping) else None
        native_observation = raw.get("observation") if isinstance(raw, Mapping) else None
        return self._absolute_ms(native_observation) if isinstance(native_observation, Mapping) else None

    @staticmethod
    def _relative_action_plan(offset: list[int]) -> list[str]:
        horizontal = "world.move.east" if offset[0] > 0 else "world.move.west"
        vertical = "world.move.south" if offset[1] > 0 else "world.move.north"
        return [horizontal] * abs(offset[0]) + [vertical] * abs(offset[1])

    @staticmethod
    def _relative_delta(action_id: str) -> Optional[tuple[int, int]]:
        return {
            "world.move.north": (0, -1), "world.move.south": (0, 1),
            "world.move.west": (-1, 0), "world.move.east": (1, 0),
        }.get(action_id)

    def _relative_movement_request(
        self, request: Mapping[str, Any], *, guarded: bool,
    ) -> tuple[Optional[list[int]], Optional[Mapping[str, Any]], Optional[Dict[str, Any]]]:
        route = "guarded_move_relative" if guarded else "raw_move_relative"
        if request.get("master_enabled", True) is not True:
            result = {"ok": False, "error": f"{route}_master_disabled_use_primitive_actions"}
            self._transcript.append({"kind": f"{route}_off", "switch": "master_enabled", "result": result})
            return None, None, result
        if request.get("enabled") is not True:
            result = {"ok": False, "error": f"{route}_disabled_use_primitive_actions"}
            self._transcript.append({"kind": f"{route}_off", "switch": "enabled", "result": result})
            return None, None, result
        offset = request.get("offset_ms")
        bound = request.get("bound")
        if not isinstance(offset, list) or len(offset) != 2 or any(
                isinstance(item, bool) or not isinstance(item, int) for item in offset) or \
                offset == [0, 0] or not isinstance(bound, Mapping):
            return None, None, {"ok": False, "error": f"invalid_{route}_request"}
        maximum = bound.get("maximum")
        if isinstance(maximum, bool) or not isinstance(maximum, int) or maximum <= 0 or \
                not str(bound.get("basis", "")).strip() or not str(bound.get("source", "")).strip() or \
                str(bound.get("unit", "")).strip() != "steps":
            return None, None, {"ok": False, "error": "invalid_evidence_derived_bound"}
        if sum(abs(value) for value in offset) > maximum:
            return None, None, {"ok": False, "error": "derived_bound_exhausted"}
        return list(offset), bound, None

    def _relative_guard_reason(
        self, observed: Mapping[str, Any], raw: Mapping[str, Any], action_id: str,
    ) -> Optional[str]:
        if str(raw.get("state", "")) != "world" or raw.get("provenance") not in {
                "native_semantic_step_trace", "",
        }:
            return "guarded_move_relative_prompt_or_unknown_event"
        safety = raw.get("keep_watch_safety")
        if not isinstance(safety, Mapping) or any(not isinstance(safety.get(key), bool)
                                                   for key in ("monster", "danger", "damage")):
            return "guarded_move_relative_unknown_event"
        if safety.get("monster") is True or safety.get("danger") is True:
            return "guarded_move_relative_creature_or_danger"
        if safety.get("damage") is True:
            return "guarded_move_relative_damage"
        native_observation = raw.get("observation")
        if not isinstance(native_observation, Mapping):
            return "guarded_move_relative_unknown_event"
        visible_entities = native_observation.get("visible_entities")
        if not isinstance(visible_entities, list):
            return "guarded_move_relative_unknown_event"
        if any(isinstance(entity, Mapping) for entity in visible_entities):
            return "guarded_move_relative_creature"
        delta = self._relative_delta(action_id)
        visible_local = native_observation.get("visible_local")
        if delta is None or not isinstance(visible_local, list):
            return "guarded_move_relative_unknown_event"
        terrain = next((fact for fact in visible_local if isinstance(fact, Mapping) and
                        fact.get("dx") == delta[0] and fact.get("dy") == delta[1]), None)
        if not isinstance(terrain, Mapping) or terrain.get("visibility") != "clear" or \
                not isinstance(terrain.get("terrain"), str) or not terrain.get("terrain"):
            return "guarded_move_relative_terrain_unknown"
        return None

    def _move_relative(self, request: Mapping[str, Any], *, guarded: bool) -> Dict[str, Any]:
        """Dispatch a signed cardinal route from one fresh native frame per step."""
        offset, bound, rejected = self._relative_movement_request(request, guarded=guarded)
        if rejected is not None:
            return rejected
        assert offset is not None and bound is not None
        route = "guarded_move_relative" if guarded else "raw_move_relative"
        try:
            observed = self.observe()
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        origin = self._relative_position(observed)
        if origin is None:
            return self._fail_closed(f"{route}_position_unavailable", {"unused_authority": "revoked"})
        plan = self._relative_action_plan(offset)
        target = [origin[0] + offset[0], origin[1] + offset[1], origin[2]]
        receipts: list[Mapping[str, Any]] = []

        def stop(reason: str, detail: Mapping[str, Any]) -> Dict[str, Any]:
            current = self._relative_position(observed)
            completed = len(receipts)
            return self._fail_closed(reason, {
                **dict(detail), "offset_ms": list(offset), "origin_absolute_ms": origin,
                "target_absolute_ms": target, "terminal_absolute_ms": current,
                "partial_progress": completed, "planned_steps": len(plan),
                "native_receipts": [dict(receipt) for receipt in receipts],
                "derived_bound": dict(bound), "guarded_handling_count": 0 if not guarded else completed,
                "unused_authority": "revoked",
            })

        for index, action_id in enumerate(plan):
            record = self._observations.get(str(observed.get("observation_id", "")))
            raw = record.get("issuing_frame") if isinstance(record, Mapping) else None
            if not isinstance(raw, Mapping):
                return stop(f"{route}_stale_frame", {})
            if guarded:
                guard_reason = self._relative_guard_reason(observed, raw, action_id)
                if guard_reason is not None:
                    return stop(guard_reason, {"step_index": index})
            elif str(raw.get("state", "")) != "world" or raw.get("provenance") not in {
                    "native_semantic_step_trace", "",
            }:
                return stop("raw_move_relative_interrupted", {
                    "step_index": index, "native_stop_reason": str(raw.get("state", "unknown_event")),
                })
            advertised = record.get("actions") if isinstance(record, Mapping) else None
            if not isinstance(advertised, set) or action_id not in advertised:
                return stop(f"{route}_action_not_advertised", {
                    "step_index": index, "action_id": action_id,
                    "advertised_actions": sorted(advertised) if isinstance(advertised, set) else [],
                })
            before = self._relative_position(observed)
            delta = self._relative_delta(action_id)
            if before is None or delta is None:
                return stop(f"{route}_position_unavailable", {"step_index": index})
            expected = [before[0] + delta[0], before[1] + delta[1], before[2]]
            self._relative_recipe_active = True
            try:
                outcome = self.act(observation_id=str(observed["observation_id"]), action_id=action_id)
            finally:
                self._relative_recipe_active = False
            receipt = outcome.get("receipt")
            if isinstance(receipt, Mapping):
                receipts.append(receipt)
            if outcome.get("ok") is not True:
                if "binding drift" in str(outcome.get("error", "")):
                    return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
                native = receipt.get("native_receipt") if isinstance(receipt, Mapping) else None
                failure = str(native.get("outcome", "native_dispatch_failed")) if isinstance(native, Mapping) else \
                          str(outcome.get("error", "native_dispatch_failed"))
                return stop(f"{route}_{failure}", {"step_index": index, "action_id": action_id})
            native = receipt.get("native_receipt") if isinstance(receipt, Mapping) else None
            if not isinstance(native, Mapping):
                return stop(f"{route}_receipt_missing", {"step_index": index})
            if native.get("coordinate_space") != "absolute_ms" or \
                    native.get("before_absolute_ms") != before or native.get("expected_absolute_ms") != expected or \
                    native.get("after_absolute_ms") != expected or native.get("outcome") != "moved" or \
                    not isinstance(native.get("after_terrain"), str) or not native.get("after_terrain"):
                return stop(f"{route}_receipt_mismatch", {"step_index": index, "action_id": action_id,
                            "expected_absolute_ms": expected})
            next_observation = outcome.get("observation")
            if not isinstance(next_observation, Mapping):
                return stop(f"{route}_fresh_frame_missing", {"step_index": index})
            observed = dict(next_observation)
            if self._relative_position(observed) != expected:
                return stop(f"{route}_unexpected_displacement", {"step_index": index,
                            "expected_absolute_ms": expected})
            if index + 1 > int(bound["maximum"]):
                return stop("derived_bound_exhausted", {"step_index": index})
        terminal = self._relative_position(observed)
        if terminal != target:
            return stop(f"{route}_target_progress_failed", {})
        result = {
            "stop_reason": "target_reached", "offset_ms": list(offset),
            "origin_absolute_ms": origin, "target_absolute_ms": target,
            "terminal_absolute_ms": terminal,
            "terminal_observation": observed, "native_receipts": [dict(receipt) for receipt in receipts],
            "partial_progress": len(receipts), "planned_steps": len(plan), "derived_bound": dict(bound),
            "guarded_handling_count": 0 if not guarded else len(receipts),
        }
        self._transcript.append({"kind": route, "result": result})
        return {"ok": True, "result": result}

    def raw_move_relative(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        return self._move_relative(request, guarded=False)

    def guarded_move_relative(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        return self._move_relative(request, guarded=True)

    def look(self, handle: str) -> Dict[str, Any]:
        requested = str(handle)
        known = self._handles.get(requested)
        if known is None:
            return {"ok": False, "error": "unknown_or_stale_visible_handle"}
        try:
            current = self.observe()
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
        fact = next((item for item in current["visible_local"] if item["handle"] == requested), None)
        if fact is None:
            return {"ok": False, "error": "stale_visible_handle"}
        return {"ok": True, "observation_id": current["observation_id"], "fact": fact}

    def qualify_r019_timed_entry(self, *, observation_id: str) -> Dict[str, Any]:
        """Bind the R-019 primitive wait to the actual post-start hostile frame."""
        if self._state != "active":
            return {"ok": False, "error": "live_session_finished", "final": self._final_report}
        if not self._r019_timed_entry:
            return {"ok": False, "error": "r019_live_timed_entry_qualification_not_declared"}
        if not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        observed = self._observations.get(str(observation_id))
        if observed is None or observed["used"]:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        expected_offset = self._r019_timed_entry.get("target_offset_ms")
        if not isinstance(expected_offset, list) or len(expected_offset) != 3 or \
                any(isinstance(value, bool) or not isinstance(value, int) for value in expected_offset):
            return self._fail_closed("r019_live_timed_entry_contract_malformed", {
                "unused_authority": "revoked",
            })
        dangerous_proximity = self._r019_timed_entry.get("dangerous_proximity")
        maximum_steps = self._r019_timed_entry.get("maximum_boundary_entry_steps")
        if isinstance(dangerous_proximity, bool) or not isinstance(dangerous_proximity, int) or \
                isinstance(maximum_steps, bool) or not isinstance(maximum_steps, int) or maximum_steps <= 0:
            return self._fail_closed("r019_live_timed_entry_contract_malformed", {
                "unused_authority": "revoked",
            })
        candidates = [
            entity for entity in observed["public_state"].get("visible_entities", [])
            if isinstance(entity, Mapping) and entity.get("kind") == "monster" and
            entity.get("attitude") == "hostile"
        ]
        required_actor = self._r019_timed_entry.get("fixture_actor")
        if isinstance(required_actor, Mapping):
            candidates = [
                entity for entity in candidates
                if all(entity.get(key) == value for key, value in required_actor.items())
            ]
        if len(candidates) != 1:
            return self._fail_closed("r019_live_timed_entry_unqualified", {
                "observation_id": str(observation_id),
                "expected_offset_ms": list(expected_offset),
                "required_attitude": "hostile",
                "required_fixture_actor": dict(required_actor) if isinstance(required_actor, Mapping) else {},
                "visible_entities": observed["public_state"].get("visible_entities", []),
                "unused_authority": "revoked",
            })
        projection = self._r019_projection_receipt(
            observed["issuing_frame"], candidates[0], expected_offset,
        )
        if projection.get("status") == "not_declared" and (
                candidates[0].get("dx") != expected_offset[0] or
                candidates[0].get("dy") != expected_offset[1]):
            projection = {
                "status": "rejected", "reason": "mismatched_projection",
                "cockpit_relative_offset_ms": [candidates[0].get("dx"), candidates[0].get("dy"), 0],
                "declared_live_offset_ms": list(expected_offset),
            }
        if projection.get("status") not in {"accepted", "not_declared"}:
            return self._fail_closed("r019_live_timed_entry_unqualified", {
                "observation_id": str(observation_id),
                "projection": projection,
                "unused_authority": "revoked",
            })
        initial_distance = max(abs(expected_offset[0]), abs(expected_offset[1]))
        boundary_entry_steps = initial_distance - dangerous_proximity
        if initial_distance <= dangerous_proximity or boundary_entry_steps > maximum_steps:
            return self._fail_closed("r019_live_timed_entry_unqualified", {
                "observation_id": str(observation_id), "initial_distance": initial_distance,
                "dangerous_proximity": dangerous_proximity,
                "boundary_entry_steps": boundary_entry_steps,
                "maximum_boundary_entry_steps": maximum_steps,
                "unused_authority": "revoked",
            })
        self._r019_timed_entry_receipt = {
            "schema": "caol-r019-live-timed-entry-qualification-v1",
            "observation_id": str(observation_id),
            "target_handle": candidates[0].get("handle"),
            "target_offset_ms": list(expected_offset),
            "attitude": "hostile",
            "visible": True,
            "fixture_route_qualification": "bound_before_start_and_preserved_at_live_offset",
            "dangerous_proximity": dangerous_proximity,
            "initial_distance": initial_distance,
            "boundary_entry_steps": boundary_entry_steps,
            "maximum_boundary_entry_steps": maximum_steps,
            "projection": projection,
        }
        self._transcript.append({"kind": "r019_timed_entry_qualification",
                                 "result": dict(self._r019_timed_entry_receipt)})
        return {"ok": True, "result": dict(self._r019_timed_entry_receipt)}

    def _r019_projection_receipt(
        self, frame: Mapping[str, Any], entity: Mapping[str, Any], expected_offset: list[int],
    ) -> Dict[str, Any]:
        """Preserve the fixture-to-live coordinate proof for the timed entry.

        Absolute coordinates remain private to this receipt.  The cockpit still
        exposes only avatar-relative presentation facts to ordinary callers.
        """
        declared = self._r019_timed_entry
        required = ("saved_player_absolute_ms", "saved_actor_absolute_ms",
                    "live_player_absolute_ms", "live_actor_absolute_ms", "saved_offset_ms")
        if not any(key in declared for key in required):
            return {"status": "not_declared"}

        observation = frame.get("observation")
        avatar = observation.get("avatar") if isinstance(observation, Mapping) else None
        raw_entities = observation.get("visible_entities") if isinstance(observation, Mapping) else None
        raw = next((item for item in raw_entities or [] if isinstance(item, Mapping) and
                    item.get("fixture_actor_id") == entity.get("fixture_actor_id")), None)

        def point(value: Any) -> Optional[list[int]]:
            if not isinstance(value, list) or len(value) != 3 or any(
                    isinstance(item, bool) or not isinstance(item, int) for item in value):
                return None
            return list(value)

        saved_player = point(declared.get("saved_player_absolute_ms"))
        saved_actor = point(declared.get("saved_actor_absolute_ms"))
        live_player = point(avatar.get("absolute_ms") if isinstance(avatar, Mapping) else None)
        live_actor = point(raw.get("absolute_ms") if isinstance(raw, Mapping) else None)
        saved_offset = point(declared.get("saved_offset_ms"))
        if None in (saved_player, saved_actor, live_player, live_actor, saved_offset):
            return {"status": "rejected", "reason": "missing_or_malformed_projection_input"}
        assert saved_player is not None and saved_actor is not None
        assert live_player is not None and live_actor is not None and saved_offset is not None
        saved_relative = [saved_actor[index] - saved_player[index] for index in range(3)]
        live_relative = [live_actor[index] - live_player[index] for index in range(3)]
        expected_live_player = point(declared.get("live_player_absolute_ms"))
        expected_live_actor = point(declared.get("live_actor_absolute_ms"))
        errors = []
        if saved_relative != saved_offset:
            errors.append("saved_placement_mismatch")
        if expected_live_player != live_player:
            errors.append("stale_origin")
        if expected_live_actor != live_actor:
            errors.append("actor_drift")
        if live_relative != expected_offset or [entity.get("dx"), entity.get("dy"), 0] != expected_offset:
            errors.append("mismatched_projection")
        return {
            "schema": "caol-r019-fixture-live-projection-v1",
            "status": "accepted" if not errors else "rejected",
            "saved_player_absolute_ms": saved_player,
            "saved_actor_absolute_ms": saved_actor,
            "saved_offset_ms": saved_offset,
            "live_player_absolute_ms": live_player,
            "live_actor_absolute_ms": live_actor,
            "cockpit_relative_offset_ms": live_relative,
            "declared_live_offset_ms": list(expected_offset),
            "errors": errors,
        }

    def act(
        self, *, observation_id: str, action_id: str, handle: Optional[str] = None,
        recovery: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Dispatch one current, observed semantic action through its native owner.

        Cockpit has no input translation.  It only grants a one-use observation
        authority to the supplied native dispatcher after every identity check.
        """
        if self._state != "active":
            return {"ok": False, "error": "live_session_finished", "final": self._final_report}
        if self._causal_boundary_precondition is not None:
            boundary = self._causal_boundary_precondition()
            if boundary.get("status") == "matched":
                return {
                    "ok": False,
                    "error": "causal_boundary_reached",
                    "causal_boundary": dict(boundary),
                    "next_action": "run.finish",
                }
        if not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        observed = self._observations.get(str(observation_id))
        if observed is None:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        if observed["used"]:
            return {"ok": False, "error": "duplicate_submission"}
        if str(action_id) == "world.wait" and self._r019_timed_entry:
            receipt = self._r019_timed_entry_receipt
            if not isinstance(receipt, Mapping) or receipt.get("observation_id") != str(observation_id):
                return {"ok": False, "error": "r019_live_timed_entry_qualification_required"}
        if self._enforce_continuation_bounds and not self._relative_recipe_active and (
                self._continuation is None or
                self._continuation.get("observation_id") != str(observation_id) or
                self._continuation.get("run_id") != observed["run_id"]
        ):
            return {"ok": False, "error": "continuation_bound_required"}
        if str(action_id) not in observed["actions"]:
            return {"ok": False, "error": "action_not_advertised"}
        if handle is not None and str(handle) not in observed["handles"]:
            return {"ok": False, "error": "unknown_or_stale_visible_handle"}
        issuing_raw = observed["issuing_frame"]
        if not observed["native_interruption"]:
            try:
                current_raw = self._read_native_frame()
                run_id, frame_id, _, _, _ = self._frame(current_raw)
            except ValueError as exc:
                return {"ok": False, "error": str(exc)}
            if run_id != observed["run_id"] or frame_id != str(observation_id):
                return {"ok": False, "error": "stale_observation"}
        else:
            # Do not reread here: a later world frame must not revoke the
            # native modal authority already advertised by this frame.
            current_raw = issuing_raw
        if handle is not None:
            identity = self._handles[str(handle)]["identity"]
            current_observation = current_raw.get("observation", {})
            visible = current_observation.get("visible_local", []) if isinstance(
                          current_observation, Mapping) else []
            if not any(isinstance(fact, Mapping) and self._identity(fact) == identity for fact in visible):
                return {"ok": False, "error": "stale_visible_handle"}
        if recovery is not None:
            safe = current_raw.get("safe_recovery")
            if not isinstance(safe, Mapping):
                return {"ok": False, "error": "unknown_native_modal"}
            modal_id = str(recovery.get("modal_id", ""))
            if modal_id != str(safe.get("modal_id", "")):
                return {"ok": False, "error": "stale_native_modal"}
            allowed = safe.get("actions")
            if isinstance(allowed, (str, bytes)) or not isinstance(allowed, list) or \
                    str(action_id) not in allowed:
                return {"ok": False, "error": "unauthorized_recovery_action"}
        if self._dispatch_advertised_action is None:
            return {"ok": False, "error": "native_action_dispatch_unavailable"}
        observed["used"] = True
        receipt = self._dispatch_advertised_action(issuing_raw, str(action_id))
        if not isinstance(receipt, Mapping):
            return self._fail_closed("native_receipt_missing", {"action_id": action_id})
        native = receipt.get("native_receipt")
        next_frame = receipt.get("_next_frame") or receipt.get("next_frame")
        if not isinstance(native, Mapping):
            return self._fail_closed("native_receipt_missing", {"action_id": action_id})
        if native.get("accepted") is not True:
            return {"ok": False, "error": "native_action_rejected", "receipt": dict(receipt)}
        if str(native.get("frame_id", "")) != str(observation_id) or \
                str(native.get("action_id", "")) != str(action_id):
            return {"ok": False, "error": "native_receipt_mismatch", "receipt": dict(receipt)}
        if not isinstance(next_frame, Mapping) or str(next_frame.get("frame_id", "")) == str(observation_id):
            return self._fail_closed("fresh_observation_missing", {"action_id": action_id})
        if recovery is not None and issuing_raw.get("provenance") == "native_semantic_ui_trace" and \
                self._continuation is not None and \
                self._continuation.get("phase") == "awaiting_native_completion":
            safe = issuing_raw.get("safe_recovery")
            activity_bridge = safe.get("activity_bridge") if isinstance(safe, Mapping) else None
            if isinstance(activity_bridge, Mapping) and \
                    str(next_frame.get("state", "")) == "activity_distraction" and \
                    next_frame.get("provenance") == "native_activity_distraction_query" and \
                    str(next_frame.get("activity_type", "")) == str(activity_bridge.get("type", "")) and \
                    str(activity_bridge.get("action_id", "")) in next_frame.get("valid_actions", []):
                self._safe_activity_bridge = {
                    "run_id": observed["run_id"],
                    "frame_id": str(next_frame.get("frame_id", "")),
                    "activity_type": str(activity_bridge["type"]),
                    "action_id": str(activity_bridge["action_id"]),
                    "modal_id": str(recovery.get("modal_id", "")),
                }
        native_activity_return = (
            next_frame.get("provenance") == "native_activity_distraction_return" and
            str( next_frame.get( "state", "" ) ) in {"activity_resumed", "activity_stopped"}
        )
        if native_activity_return:
            # IGNORE acknowledges the native distraction but does not finish
            # the interrupted wait.  There is consequently no avatar frame to
            # publish yet.  Keep the bound continuation alive and require the
            # next game.observe to receive a later native avatar observation;
            # turning this receipt into an empty observation would fabricate
            # sight while treating it as a failed action strands the wait.
            continuation = self._continuation
            if continuation is None or \
                    continuation.get("phase") != "awaiting_native_completion" or \
                    issuing_raw.get("provenance") != "native_activity_distraction_query":
                return self._fail_closed("unexpected_native_activity_return", {
                    "action_id": action_id,
                    "unused_authority": "revoked",
                })
            result: Dict[str, Any] = {
                "ok": True,
                "receipt": {
                    key: value for key, value in receipt.items() if not key.startswith("_")
                },
                "expected_postcondition": "accepted_native_return_then_later_avatar_observation",
                "evidence_effect": "native_transition_receipt_persisted",
                "continuation": {"state": "awaiting_native_completion"},
            }
            self._transcript.append({
                "kind": "action", "action_id": str(action_id),
                "observation_id": str(observation_id), "result": result,
            })
            return result
        try:
            fresh = self.observe()
        except ValueError as exc:
            return {"ok": False, "error": str(exc), "receipt": dict(receipt)}
        result: Dict[str, Any] = {
            "ok": True, "receipt": {key: value for key, value in receipt.items() if not key.startswith("_")},
            "expected_postcondition": "matching_native_receipt_and_fresh_observation",
            "evidence_effect": "native_transition_receipt_persisted",
            "observation": fresh,
        }
        if recovery is not None:
            result["recovery_receipt"] = result["receipt"]
        self._transcript.append({
            "kind": "action", "action_id": str(action_id),
            "observation_id": str(observation_id), "result": result,
        })
        continuation = self._continuation
        if continuation is not None:
            if str( next_frame.get( "state", "" ) ) in {
                    "wait_mode_choice", "wait_duration_choice",
            }:
                # Opening the native wait menus is part of the same bounded
                # wait transaction, but advances no game time by itself.
                # Carry the one evidence-derived authority to its fresh menu
                # observation; only the later wait activity may settle its
                # progress obligation.
                continuation["observation_id"] = str( fresh.get( "observation_id", "" ) )
                continuation["phase"] = "awaiting_wait_dispatch"
                result["continuation"] = {"state": "awaiting_wait_dispatch"}
                return result
            if str(issuing_raw.get("state", "")) != "wait_activity" and \
                    str(next_frame.get("state", "")) == "wait_activity":
                continuation["phase"] = "awaiting_native_completion"
                continuation["activity_frame_id"] = str(fresh.get("observation_id", ""))
                result["continuation"] = {"state": "awaiting_native_completion"}
                return result
            if continuation.get("phase") == "awaiting_wait_dispatch" and \
                    next_frame.get("provenance") in {
                        "native_activity_distraction_query", "native_semantic_ui_trace",
                    }:
                # A native interruption can occur as soon as the duration is
                # dispatched, before the activity frame becomes observable.
                # Its advertised recovery is part of this same bounded wait;
                # carry the authority to exactly that fresh interruption frame.
                continuation["observation_id"] = str(fresh.get("observation_id", ""))
                continuation["phase"] = "awaiting_native_completion"
                result["continuation"] = {"state": "awaiting_native_completion"}
                return result
            if continuation.get("phase") == "awaiting_native_completion" and \
                    issuing_raw.get("provenance") in {
                        "native_activity_distraction_query", "native_semantic_ui_trace",
                    }:
                continuation["observation_id"] = str(fresh.get("observation_id", ""))
                result["continuation"] = {"state": "awaiting_native_completion"}
                return result
            self._continuation = None
            before = self._signal_value(observed["public_state"], continuation["expected_signal"])
            after = self._signal_value(fresh, continuation["expected_signal"])
            if continuation["progress_required"] and before == after:
                return self._fail_closed("proved_no_progress", {
                    "expected_signal": continuation["expected_signal"],
                    "before": before, "after": after,
                    "unused_authority": "revoked",
                })
            if after is not None and after > continuation["start"] + continuation["maximum"]:
                return self._fail_closed("derived_bound_exhausted", {
                    "expected_signal": continuation["expected_signal"],
                    "observed": after, "bound": continuation,
                    "unused_authority": "none",
                })
        return result

    def _r019_receipt(
        self, matrix: Mapping[str, Any], *, require_primitive_dispatch: bool,
    ) -> Optional[Dict[str, Any]]:
        role = str(matrix.get("role", "")).strip()
        off_switch = role.removeprefix("off:") if role.startswith("off:") else ""
        if role not in {"guarded", "primitive"} and off_switch not in {
                "master_enabled", "enabled"}:
            return None
        keep_watch_calls = sum(
            entry.get("kind") == "keep_watch" for entry in self._transcript
            if isinstance(entry, Mapping)
        )
        off_calls = [entry for entry in self._transcript
                     if isinstance(entry, Mapping) and entry.get("kind") == "keep_watch_off"]
        action_calls = sum(
            entry.get("kind") == "action" and isinstance(entry.get("result"), Mapping) and
            entry["result"].get("ok") is True for entry in self._transcript
            if isinstance(entry, Mapping)
        )
        if role in {"guarded", "primitive"} and (role == "guarded") != bool(keep_watch_calls):
            return None
        if off_switch and (keep_watch_calls or len(off_calls) != 1 or
                           off_calls[0].get("switch") != off_switch or
                           (require_primitive_dispatch and not action_calls)):
            return None
        if off_switch and (not str(matrix.get("clean_start_identity", "")).strip() or
                           not str(matrix.get("source_identity", "")).strip()):
            return None
        receipt = {**dict(matrix),
                   "role_receipt": {
                       "schema": "caol-r019-role-receipt-v1", "role": role,
                       "run_id": self._run_id, "binding_id": self._binding_id,
                   },
                   "round_trip_receipt": {
                       "schema": "caol-r019-round-trip-receipt-v1",
                       "model": {"count": keep_watch_calls if role == "guarded" else action_calls,
                                 "measurement": "cockpit transcript decision entries"},
                       "tool": {"count": action_calls,
                                "measurement": "accepted native dispatch receipts"},
                   }}
        if off_switch:
            receipt["off_switch_receipt"] = {
                "schema": "caol-r019-off-switch-receipt-v1", "switch": off_switch,
                "native_dispatch_count": 0, "guarded_recipe_dispatch_count": 0,
                "primitive_native_dispatch_count": action_calls,
                "guarded_handling_count": 0, "hidden_batching": False,
                "native_receipt_actions": [
                    str(entry.get("action_id", "")) for entry in self._transcript
                    if isinstance(entry, Mapping) and entry.get("kind") == "action"
                ],
            }
        return receipt

    def _diagnostic_terminal_receipt(
        self, *, observed: Mapping[str, Any], reason: str,
        r019_acceptance_matrix: Optional[Mapping[str, Any]],
    ) -> Optional[Dict[str, Any]]:
        """Accept the one descriptor-declared zero-credit scheduler stop only.

        This is deliberately not a generic continuation escape hatch.  The
        native wait remains incomplete; this narrow diagnostic terminal merely
        lets its due-turn observation be sealed after the declared popup has
        been acknowledged.
        """
        declared = self._diagnostic_terminal
        if not declared:
            return None
        if r019_acceptance_matrix is not None or self._witness_charter is not None:
            return None
        expected_kind = "r019_scheduler_due_popup_acknowledgement"
        eoc = str(declared.get("scheduler_eoc", "")).strip()
        expected_reason = str(declared.get("stop_reason", "")).strip()
        expected_action = str(declared.get("popup_action", "")).strip()
        if declared.get("kind") != expected_kind or not eoc or not expected_reason or \
                expected_action != "modal.acknowledge" or declared.get("gameplay_credit") is not False:
            return None
        if reason != expected_reason:
            return None
        compact = observed.get("public_state", {}).get("compact_log", {})
        if not isinstance(compact, Mapping) or compact.get("first_divergence") is not None or \
                compact.get("contradictory_evidence") or compact.get("unsafe") is True:
            return None
        traces = compact.get("scheduler_trace")
        if not isinstance(traces, list) or len(traces) != 1:
            return None
        trace = traces[0]
        if not isinstance(trace, Mapping) or trace.get("eoc") != eoc or \
                trace.get("decision") != "due" or \
                trace.get("due_turn") != trace.get("current_turn") or \
                trace.get("current_turn") != observed.get("public_state", {}).get("observed_turn"):
            return None
        raw = observed.get("issuing_frame")
        if not isinstance(raw, Mapping) or raw.get("provenance") != "native_activity_distraction_query" or \
                raw.get("state") != "activity_distraction" or raw.get("activity_type") != "eoc":
            return None
        popup_observations = [
            entry for entry in self._transcript
            if isinstance(entry, Mapping) and entry.get("kind") == "observation" and
            isinstance(entry.get("value"), Mapping) and
            entry["value"].get("advertised_actions") == [expected_action]
        ]
        acknowledgements = [
            entry for entry in self._transcript
            if isinstance(entry, Mapping) and entry.get("kind") == "action" and
            entry.get("action_id") == expected_action and isinstance(entry.get("result"), Mapping) and
            entry["result"].get("ok") is True
        ]
        if len(popup_observations) != 1 or len(acknowledgements) != 1:
            return None
        popup_id = str(popup_observations[0]["value"].get("observation_id", ""))
        acknowledgement = acknowledgements[0]
        receipt = acknowledgement["result"].get("receipt")
        native = receipt.get("native_receipt") if isinstance(receipt, Mapping) else None
        popup_instance = popup_id.split(":semantic-ui:", 1)[-1].split(":", 1)[0]
        if not popup_id or ":semantic-ui:" not in popup_id or \
                acknowledgement.get("observation_id") != popup_id or not isinstance(native, Mapping) or \
                native.get("accepted") is not True or native.get("frame_id") != popup_id or \
                native.get("action_id") != expected_action or \
                native.get("semantic_ui_instance_id") != popup_instance:
            return None
        return {
            "schema": "caol-r019-scheduler-diagnostic-terminal-receipt-v1",
            "scheduler_eoc": eoc,
            "due_turn": trace.get("due_turn"),
            "popup_observation_id": popup_id,
            "popup_instance_id": popup_instance,
            "acknowledgement_action": expected_action,
            "gameplay_credit": False,
        }

    def seal_witness_journal(
        self, *, observation_id: str, stop_reason: str, unused_authority: str,
    ) -> Dict[str, Any]:
        """Stop gameplay input and expose the immutable facts the worker may cite."""
        if self._witness_charter is None:
            return {"ok": False, "error": "playtest_has_no_witness_charter"}
        if self._state != "active":
            return {"ok": False, "error": "live_session_not_actionable"}
        observed = self._observations.get(str(observation_id))
        if observed is None or observed["used"]:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        reason = str(stop_reason).strip()
        disposition = str(unused_authority).strip()
        if not reason or not disposition:
            return {"ok": False, "error": "witness_requires_reason_and_unused_authority"}
        if self._continuation is not None:
            return {"ok": False, "error": "continuation_incomplete"}
        if not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        terminal = {
            "observation_id": str(observation_id),
            "stop_reason": reason,
            "unused_authority": disposition,
            "cleanup": {
                "owner": "scenario_terminalization",
                "status": "deferred_to_scenario_terminalization",
            },
        }
        try:
            self._sealed_witness_terminal = terminal
            self._sealed_journal = build_evidence_journal(
                charter=self._witness_charter,
                identity={
                    **self._witness_identity,
                    "run_id": self._run_id,
                    "binding_id": self._binding_id,
                },
                transcript=self._transcript,
                terminal=terminal,
                evidence_ceiling=self._witness_evidence_ceiling,
            )
        except WitnessError as exc:
            return {"ok": False, "error": str(exc)}
        self._state = "witnessing"
        return {
            "ok": True,
            "result": {
                "action": "WITNESS / FINISH",
                "charter": dict(self._witness_charter),
                "evidence_journal": dict(self._sealed_journal),
                "next_calls": ["run.finish"],
            },
        }

    def finish(
        self, *, observation_id: str, stop_reason: str, unused_authority: str,
        r019_acceptance_matrix: Optional[Mapping[str, Any]] = None,
        witness: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        if self._state not in {"active", "witnessing"}:
            return {"ok": False, "error": "live_session_finished", "final": self._final_report}
        observed = self._observations.get(str(observation_id))
        if observed is None or observed["used"]:
            return {"ok": False, "error": "unknown_or_stale_observation"}
        reason = str(stop_reason).strip()
        disposition = str(unused_authority).strip()
        if not reason or not disposition:
            return {"ok": False, "error": "finish_requires_reason_and_unused_authority"}
        if not self._binding_matches():
            return self._fail_closed("binding_drift", {"unused_authority": "revoked"})
        if r019_acceptance_matrix is not None and not isinstance(r019_acceptance_matrix, Mapping):
            return {"ok": False, "error": "r019_acceptance_matrix must be an object"}
        witness_validation: Optional[Dict[str, Any]] = None
        if self._witness_charter is not None:
            if self._sealed_journal is None or self._state != "witnessing":
                return {"ok": False, "error": "witness_journal_must_be_sealed_before_finish"}
            if not isinstance(witness, Mapping):
                return {"ok": False, "error": "witness_statement_required"}
            sealed_terminal = self._sealed_witness_terminal or {}
            if reason != sealed_terminal.get("stop_reason") or \
                    disposition != sealed_terminal.get("unused_authority"):
                return {"ok": False, "error": "finish_does_not_match_sealed_witness_terminal"}
            try:
                witness_validation = validate_witness_statement(
                    charter=self._witness_charter,
                    journal=self._sealed_journal,
                    statement=witness,
                )
            except WitnessError as exc:
                return {"ok": False, "error": str(exc)}
        r019_receipt: Optional[Dict[str, Any]] = None
        if isinstance(r019_acceptance_matrix, Mapping):
            r019_receipt = self._r019_receipt(
                r019_acceptance_matrix, require_primitive_dispatch=True,
            )
            if r019_receipt is None:
                role = str(r019_acceptance_matrix.get("role", "")).strip()
                if role not in {"guarded", "primitive", "off:master_enabled", "off:enabled"}:
                    return {"ok": False, "error": "r019_role_must_be_guarded_primitive_or_off_switch"}
                return {"ok": False, "error": "r019_role_does_not_match_live_transcript"}
        raw = observed.get("issuing_frame")
        hostile_stop = (
            reason == "hostile_spotted_near" and
            isinstance(r019_receipt, Mapping) and
            r019_receipt.get("role") == "off:master_enabled" and
            isinstance(raw, Mapping) and
            raw.get("provenance") == "native_activity_distraction_query" and
            raw.get("state") == "activity_distraction" and
            raw.get("activity_type") == "hostile_spotted_near" and
            "activity.ignore" in observed.get("actions", set()) and
            self._continuation is not None and
            self._continuation.get("observation_id") == str(observation_id)
        )
        diagnostic_terminal = self._diagnostic_terminal_receipt(
            observed=observed, reason=reason, r019_acceptance_matrix=r019_acceptance_matrix,
        )
        if self._continuation is not None and not hostile_stop and diagnostic_terminal is None:
            return {"ok": False, "error": "continuation_incomplete"}
        if hostile_stop:
            # This is the only terminal path that may leave an activity
            # interruption unanswered.  The active native frame is retained
            # as the post-stop observation; no recovery action is consumed.
            continuation = dict(self._continuation)
            pre_wait_minutes = continuation.get("pre_wait_game_minutes")
            interruption_minutes = raw.get("game_minutes")
            maximum = continuation.get("maximum")
            valid_times = (
                isinstance(pre_wait_minutes, (int, float)) and not isinstance(pre_wait_minutes, bool) and
                isinstance(interruption_minutes, (int, float)) and not isinstance(interruption_minutes, bool) and
                isinstance(maximum, (int, float)) and not isinstance(maximum, bool) and
                raw.get("calendar_time_source") == "native_activity_distraction_query" and
                interruption_minutes > pre_wait_minutes and
                interruption_minutes - pre_wait_minutes <= maximum
            )
            if not valid_times:
                return self._fail_closed("r019_hostile_partial_progress_unproved", {
                    "observation_id": str(observation_id),
                    "pre_wait_game_minutes": pre_wait_minutes,
                    "interruption_game_minutes": interruption_minutes,
                    "calendar_time_source": raw.get("calendar_time_source"),
                    "unused_authority": "revoked",
                })
            partial_progress = interruption_minutes - pre_wait_minutes
            self._continuation = None
        if diagnostic_terminal is not None:
            self._continuation = None
        target_receipt: Dict[str, Any] = {}
        if reason == "target_predicate_proved":
            target_receipt = {
                "schema": "caol-cockpit-target-receipt-v1",
                "run_id": self._run_id,
                "binding_id": self._binding_id,
                "step_label": self._proof_step_label,
                "step_index": self._proof_step_index,
                "observation_id": str(observation_id),
                "observed_game_minutes": observed["public_state"].get("game_minutes"),
                "stop_reason": reason,
            }
        return {"ok": True, "result": self._stop(reason, {
            "observation_id": str(observation_id),
            "unused_authority": disposition,
            **({"target_receipt": target_receipt} if target_receipt else {}),
            **({"r019_hostile_stop_receipt": {
                "schema": "caol-r019-off-hostile-stop-receipt-v1",
                "type": "hostile_spotted_near",
                "advertised_action": "activity.ignore",
                "advertised_action_dispatched": False,
                "post_stop_observation_id": str(observation_id),
                "pre_wait_game_minutes": pre_wait_minutes,
                "interruption_game_minutes": interruption_minutes,
                "partial_progress": partial_progress,
                "continuation_stopped": True,
            }} if hostile_stop else {}),
            **({"r019_acceptance_matrix": r019_receipt} if r019_receipt is not None else {}),
            **({"diagnostic_terminal_receipt": diagnostic_terminal}
               if diagnostic_terminal is not None else {}),
            **({"evidence_journal": self._sealed_journal,
                "witness_validation": witness_validation}
               if witness_validation is not None else {}),
        })}

    def close_unfinished(self) -> Dict[str, Any]:
        return self._fail_closed("client_disconnected", {"unused_authority": "revoked"})


def _public(value: Any, key: str = "") -> Any:
    if isinstance(value, Mapping):
        return {str(k): _public(v, str(k)) for k, v in value.items()
                if str(k).lower() not in _FORBIDDEN}
    if isinstance(value, (list, tuple)):
        return [_public(item) for item in value]
    return value


def _frontier(connection: Any) -> Dict[str, Any]:
    rows = registry_status(connection)
    return {"kind": "frontier", "count": len(rows), "scenarios": [
        {"id": str(row.get("manifest", {}).get("manifest_id", "")),
         "name": str(row.get("manifest", {}).get("name", "")),
         "lifecycle": str(row.get("lifecycle", {}).get("state", ""))}
        for row in rows
    ]}


class CockpitService:
    """Stateless request/response boundary for human-facing exploration."""

    def __init__(self, registry: str | None = None, run_channel: Optional[CockpitRunChannel] = None,
                 allowed_live_operations: Optional[set[str]] = None):
        self.registry = registry
        self.run_channel = run_channel
        self._allowed_live_operations = allowed_live_operations
        self._seen_run_receipts: set[str] = set()
        self._run_owner_id = secrets.token_hex(16)

    def _live_operation_is_allowed(self, action: str) -> bool:
        return action in {"game.observe", "game.look"} or self._allowed_live_operations is None or \
               action in self._allowed_live_operations

    def call(self, request: Mapping[str, Any]) -> Dict[str, Any]:
        if not isinstance(request, Mapping):
            return {"ok": False, "error": "request must be an object"}
        unknown = set(request) - _ALLOWED
        if unknown:
            return {"ok": False, "error": "unsupported fields", "fields": sorted(map(str, unknown))}
        action = str(request.get("action", "")).strip().lower()
        if action.startswith("game.") and not self._live_operation_is_allowed(action):
            return {"ok": False, "error": "operation_not_authorized_for_live_session"}
        if action == "game.keep_watch":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            keep_watch = request.get("keep_watch")
            if not isinstance(keep_watch, Mapping):
                return {"ok": False, "error": "keep_watch needs a structured recipe"}
            return self.run_channel.keep_watch(keep_watch)
        if action == "game.raw_wait":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            raw_wait = request.get("raw_wait")
            if not isinstance(raw_wait, Mapping):
                return {"ok": False, "error": "raw_wait needs a structured recipe"}
            return self.run_channel.raw_wait(raw_wait)
        if action == "game.raw_move_relative":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            raw_move_relative = request.get("raw_move_relative")
            if not isinstance(raw_move_relative, Mapping):
                return {"ok": False, "error": "raw_move_relative needs a structured request"}
            return self.run_channel.raw_move_relative(raw_move_relative)
        if action == "game.guarded_move_relative":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            guarded_move_relative = request.get("guarded_move_relative")
            if not isinstance(guarded_move_relative, Mapping):
                return {"ok": False, "error": "guarded_move_relative needs a structured request"}
            return self.run_channel.guarded_move_relative(guarded_move_relative)
        if action == "game.observe":
            if self.run_channel is None:
                return {"ok": False, "error": "native game observation is unavailable"}
            try:
                return {"ok": True, "result": self.run_channel.observe()}
            except ValueError as exc:
                return {"ok": False, "error": str(exc)}
        if action == "game.look":
            if self.run_channel is None:
                return {"ok": False, "error": "native game observation is unavailable"}
            return self.run_channel.look(str(request.get("handle", "")))
        if action == "game.qualify_r019_timed_entry":
            if self.run_channel is None:
                return {"ok": False, "error": "native game observation is unavailable"}
            return self.run_channel.qualify_r019_timed_entry(
                observation_id=str(request.get("observation_id", "")),
            )
        if action == "game.act":
            if self.run_channel is None:
                return {"ok": False, "error": "native game observation is unavailable"}
            recovery = request.get("recovery")
            if recovery is not None and not isinstance(recovery, Mapping):
                return {"ok": False, "error": "recovery must be an object"}
            return self.run_channel.act(
                observation_id=str(request.get("observation_id", "")),
                action_id=str(request.get("action_id", "")),
                handle=str(request["handle"]) if "handle" in request else None,
                recovery=recovery,
            )
        if action == "run.continue":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            bound = request.get("bound")
            if not isinstance(bound, Mapping):
                return {"ok": False, "error": "run continuation needs a structured bound"}
            return self.run_channel.continue_session(
                observation_id=str(request.get("observation_id", "")),
                expected_signal=str(request.get("expected_signal", "")),
                bound=bound,
            )
        if action == "run.controlled_binding_drift":
            if self.run_channel is None:
                return {"ok": False, "error": "native live session is unavailable"}
            matrix = request.get("r019_acceptance_matrix")
            if not isinstance(matrix, Mapping):
                return {"ok": False, "error": "r019_binding_drift_control_requires_matrix"}
            return self.run_channel.controlled_binding_drift(
                observation_id=str(request.get("observation_id", "")),
                attempted_action=str(request.get("action_id", "")),
                r019_acceptance_matrix=matrix,
            )
        if action == "run.status" and self.run_channel is not None:
            return {"ok": True, "result": self.run_channel.status()}
        if action == "run.witness" and self.run_channel is not None:
            return self.run_channel.seal_witness_journal(
                observation_id=str(request.get("observation_id", "")),
                stop_reason=str(request.get("stop_reason", "")),
                unused_authority=str(request.get("unused_authority", "")),
            )
        if action == "run.finish" and self.run_channel is not None:
            return self.run_channel.finish(
                observation_id=str(request.get("observation_id", "")),
                stop_reason=str(request.get("stop_reason", "")),
                unused_authority=str(request.get("unused_authority", "")),
                r019_acceptance_matrix=request.get("r019_acceptance_matrix"),
                witness=request.get("witness") if isinstance(request.get("witness"), Mapping) else None,
            )
        try:
            with open_registry(self.registry, writable=True) as connection:
                if request.get("frontier") is True or action == "frontier":
                    return {"ok": True, "result": _frontier(connection)}
                if action == "capability.search":
                    matches = capability_contracts(
                        connection, query=str(request.get("requirements", request.get("name", ""))),
                    )
                    return {"ok": True, "result": _public({
                        "kind": "capability", "matches": [
                            {"id": item["id"], "revision": item["revision"],
                             "summary": item["contract"].get("summary", "")} for item in matches
                        ],
                    })}
                if action in {"capability.describe", "capability_detail"}:
                    capability_id = str(request.get("id", request.get("name", ""))).strip()
                    matches = capability_contracts(connection, capability_id=capability_id)
                    if not matches:
                        return {"ok": False, "error": "capability is not in the durable catalog"}
                    return {"ok": True, "result": _public({"kind": "capability", **matches[0]})}
                if action == "run.open":
                    selection_id = str(request.get("selection_id", request.get("id", ""))).strip()
                    if not selection_id:
                        return {"ok": False, "error": "run.open needs a scenario selection"}
                    result = open_cockpit_run(
                        connection, selection_id=selection_id, owner_id=self._run_owner_id,
                    )
                    return {"ok": True, "result": _public(result)}
                if action == "run.status":
                    run_id = str(request.get("run_id", request.get("id", ""))).strip()
                    if not run_id:
                        return {"ok": False, "error": "run status needs a run id"}
                    try:
                        status = cockpit_run_authority_status(connection, run_id=run_id)
                    except ScenarioRegistryStoreError:
                        try:
                            status = cockpit_run_status(connection, run_id=run_id)
                        except ScenarioRegistryStoreError:
                            status = record_cockpit_run_receipt(
                                connection, run_id=run_id, scenario_id=str(request.get("scenario_id", "")),
                                binding_id=str(request.get("binding_id", "")), event_kind="status",
                            )
                    receipt_id = str(status["receipt_id"])
                    if receipt_id in self._seen_run_receipts:
                        return {"ok": True, "result": _public({
                            "run_id": run_id, "state": status["state"], "delta": "unchanged",
                            "evidence_effect": status["evidence_effect"], "observed_cost": status["observed_cost"],
                        })}
                    self._seen_run_receipts.add(receipt_id)
                    return {"ok": True, "result": _public(status)}
                if action == "run.finish":
                    run_id = str(request.get("run_id", request.get("id", ""))).strip()
                    if not run_id:
                        return {"ok": False, "error": "run finish needs a run id"}
                    try:
                        result = finish_cockpit_run_authority(
                            connection, run_id=run_id, details={"final_report": "unavailable"},
                        )
                    except ScenarioRegistryStoreError:
                        result = record_cockpit_run_receipt(
                            connection, run_id=run_id, scenario_id=str(request.get("scenario_id", "")),
                            binding_id=str(request.get("binding_id", "")), event_kind="finish",
                            details={"final_report": "unavailable"},
                        )
                    return {"ok": True, "result": _public(result)}
                if action == "gap.report":
                    evidence = request.get("evidence")
                    affected = request.get("affected_scenarios", [])
                    if not isinstance(evidence, Mapping) or isinstance(affected, (str, bytes)) or not isinstance(affected, list):
                        return {"ok": False, "error": "gap report needs structured evidence and affected scenarios"}
                    result = report_capability_gap(
                        connection, run_id=str(request.get("run_id", "")), scenario_id=str(request.get("scenario_id", "")),
                        binding_id=str(request.get("binding_id", "")), blocked_intent=str(request.get("blocked_intent", "")),
                        missing_kind=str(request.get("missing_kind", "")), evidence=evidence,
                        reusable_outcome=str(request.get("reusable_outcome", "")), affected_scenarios=affected,
                    )
                    return {"ok": True, "result": _public(result)}
                if action in {"gap.search", "gap.describe"}:
                    return {"ok": True, "result": _public({"kind": "capability_gap", "matches": capability_gaps(
                        connection, scenario_id=str(request.get("scenario_id", request.get("id", ""))),
                    )})}
                if action == "scenario.create":
                    declaration = request.get("declaration")
                    if not isinstance(declaration, Mapping):
                        return {"ok": False, "error": "scenario declaration must be an object"}
                    name = str(request.get("name", "")).strip()
                    result = create_source_bound_scenario(
                        connection,
                        scenarios_root=startup_harness.scenarios_root(),
                        name=name,
                        declaration=declaration,
                    )
                    return {"ok": True, "result": _public(result)}
                if action == "scenario.validate":
                    result = validate_source_bound_scenario(
                        connection, scenario_name=str(request.get("id", request.get("name", "")).strip()),
                    )
                    return {"ok": True, "result": _public(result)}
                if action == "scenario.prepare":
                    scenario_name = str(request.get("id", request.get("name", "")).strip())
                    typeid = str(request.get("required_typeid", "")).strip()
                    offsets = request.get("candidate_offsets")
                    if not scenario_name or not typeid or not isinstance(offsets, list):
                        return {"ok": False, "error": "scenario prepare needs id, required_typeid, and candidate_offsets"}
                    source_path = startup_harness.scenario_path(scenario_name).resolve()
                    row = connection.execute(
                        "SELECT declaration_json FROM manifest_current WHERE source_path = ? AND present = 1",
                        (str(source_path),),
                    ).fetchone()
                    if row is None:
                        return {"ok": False, "error": "scenario is not a current projected manifest"}
                    declaration = json.loads(str(row["declaration_json"]))
                    profile = str(declaration.get("profile", "")).strip()
                    fixture = str(declaration.get("fixture", "")).strip()
                    fixture_profile = str(declaration.get("fixture_profile", "")).strip()
                    declared_world = str(declaration.get("world", "")).strip()
                    world = str(request.get("world", declared_world)).strip()
                    if not profile or not fixture or not world:
                        return {"ok": False, "error": "scenario fixture, profile, or world is missing"}
                    fixture_install = startup_harness.install_fixture(
                        profile, fixture, replace=True, fixture_profile=fixture_profile,
                    )
                    result = prepare_selected_scenario(
                        connection,
                        scenario_name=scenario_name,
                        world_dir=startup_harness.save_dir_for_profile(profile) / world,
                        required_typeid=typeid,
                        candidate_offsets=offsets,
                        player_save=str(request.get("player_save", "")),
                        fixture_install=fixture_install,
                    )
                    return {"ok": True, "result": _public(result)}
                kind = request.get("capability") or request.get("scenario")
                if action in {"capability", "capability_search", "capability_summary"}:
                    kind = "capability"
                elif action in {"scenario", "scenario.search", "scenario.select", "scenario_search", "scenario_summary", "scenario_detail", "scenario.describe"}:
                    kind = "scenario"
                if kind is None:
                    return {"ok": False, "error": "one continuation is required"}
                query = {"requirements": request.get("requirements", []), "preferences": request.get("preferences", [])}
                execution = execute_registry_query(connection, parse_registry_query_request(query))
                candidates = execution.evaluation.candidates
                rows = [{"id": c.scenario_id, "facts": _public(c.facts), "lifecycle": c.lifecycle_state}
                        for c in candidates]
                selected = rows[0] if rows else None
                result: Dict[str, Any] = {"kind": "capability" if request.get("capability") else "scenario", "matches": rows}
                if request.get("detail") and selected:
                    snap = candidates[0]
                    result["detail"] = _public(snap.explanation)
                if action == "scenario.select" and execution.selection_id:
                    selection = connection.execute(
                        "SELECT fit_reason FROM scenario_selection_history WHERE selection_id = ?",
                        (execution.selection_id,),
                    ).fetchone()
                    if selection is not None:
                        result["selection"] = {
                            "id": execution.selection_id,
                            "fit_reason": str(selection["fit_reason"]),
                            "evidence_effect": "none_for_manufactured_state",
                        }
                return {"ok": True, "result": result}
        except (ValueError, ScenarioRegistryStoreError, OSError) as exc:
            return {"ok": False, "error": str(exc).splitlines()[0]}
