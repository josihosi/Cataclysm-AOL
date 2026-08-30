#!/usr/bin/env python3
"""Focused causal tests for the live structured transition stream reader."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

from startup_harness import (
    StructuredTransitionEventReader,
    bind_transition_event_stream,
    evaluate_causal_boundary,
    evaluate_structured_proof_gates,
    execute_probe_steps,
    build_first_divergence_diagnostic,
    normalize_saved_artifact_receipt,
    summarize_noncommitted_transition_events,
)


def event(sequence: int, run_id: str = "run-a", **fields):
    value = {
        "schema_version": 1,
        "sequence": sequence,
        "run_id": run_id,
        "game_minutes": sequence,
        "domain": "hostile_operation",
        "transition": "active_sortie_dispatch_contact",
        "outcome": "committed",
        "site_id": "overmap_special:cannibal_camp@1,2,0",
        "operation_id": "camp#raid",
        "generation": 1,
        "handoff_epoch": 1,
        "previous_state": "approaching",
        "new_state": "committed_contact",
        "reason": "local_contact",
        "actor_ids": ["npc-1", "npc-2"],
    }
    value.update(fields)
    return value


class TransitionEventReaderTest(unittest.TestCase):
    def test_causal_owner_boundary_uses_first_exact_same_run_event(self):
        gates = [
            {
                "id": "local_owner", "boundary_step": "persist_local", "expectations": [{
                    "kind": "structured_event", "predicate": {
                        "domain": "bandit_live_world", "transition": "local_pair_handoff",
                        "outcome": "committed", "actor_ids": [101, 102],
                        "simulation_owner": "local",
                    },
                }],
            },
            {
                "id": "abstract_owner", "boundary_step": "persist_abstract", "expectations": [{
                    "kind": "structured_event", "predicate": {
                        "domain": "bandit_live_world", "transition": "local_pair_dematerialization",
                        "outcome": "committed", "actor_ids": [101, 102],
                        "simulation_owner": "abstract",
                    },
                }],
            },
        ]
        wrong_run = event(1, run_id="other", domain="bandit_live_world", transition="local_pair_handoff", actor_ids=[101, 102], simulation_owner="local")
        wrong_actor = event(2, domain="bandit_live_world", transition="local_pair_handoff", actor_ids=[101, 999], simulation_owner="local")
        wrong_owner = event(3, domain="bandit_live_world", transition="local_pair_handoff", actor_ids=[101, 102], simulation_owner="abstract")
        local = event(4, domain="bandit_live_world", transition="local_pair_handoff", actor_ids=[101, 102], simulation_owner="local")
        abstract = event(5, domain="bandit_live_world", transition="local_pair_dematerialization", actor_ids=[101, 102], simulation_owner="abstract")

        pending = evaluate_causal_boundary(gates, gate_id="local_owner", events=[wrong_run, wrong_actor, wrong_owner], run_id="run-a")
        self.assertEqual(pending["status"], "pending")
        stopped = evaluate_causal_boundary(gates, gate_id="local_owner", events=[wrong_run, wrong_actor, wrong_owner, local, abstract], run_id="run-a")
        self.assertEqual(stopped["status"], "matched")
        self.assertEqual(stopped["first_matching_event"]["sequence"], 4)
        later = evaluate_causal_boundary(gates, gate_id="abstract_owner", events=[wrong_run, wrong_actor, wrong_owner, local, abstract], run_id="run-a")
        self.assertEqual(later["status"], "matched")
        self.assertEqual(later["first_matching_event"]["sequence"], 5)

    def test_causal_boundary_stops_before_unmarked_wait_and_allows_declared_persistence(self):
        gates = [{
            "id": "local_owner", "boundary_step": "persist_local", "expectations": [{
                "kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "local_pair_handoff",
                    "outcome": "committed", "actor_ids": [101, 102], "simulation_owner": "local",
                },
            }],
        }]
        matched = event(1, domain="bandit_live_world", transition="local_pair_handoff", actor_ids=[101, 102], simulation_owner="local")
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            (run_dir / "transition.events.binding.json").write_text('{"run_id":"run-a"}', encoding="utf-8")
            blocked = execute_probe_steps(
                0, run_dir, [{"kind": "wait", "label": "queued_wait", "seconds": 0.001}],
                profile="unused", world="unused", structured_events=[matched],
                causal_boundary_gate_id="local_owner", causal_boundary_gates=gates,
            )
            persisted = execute_probe_steps(
                0, run_dir, [{
                    "kind": "wait", "label": "persist_now", "seconds": 0.001,
                    "causal_boundary_persistence_for": "local_owner",
                }], profile="unused", world="unused", structured_events=[matched],
                causal_boundary_gate_id="local_owner", causal_boundary_gates=gates,
            )
        self.assertEqual(blocked[0]["abort"]["status"], "blocked_causal_boundary_progression")
        self.assertEqual(blocked[0]["causal_boundary"]["first_matching_event"]["sequence"], 1)
        self.assertEqual(persisted[0]["label"], "persist_now")

    def test_schema11_local_return_receipt_uses_authoritative_active_outing_identity(self):
        metadata = {
            "matching_sites": [{
                "site_id": "overmap_special:bandit_camp@177,13,0",
                "active_outing": {
                    "schema_version": 11,
                    "is_active": True,
                    "kind": "structural_sortie",
                    "activity_id": "camp-r008#structural",
                    "generation": 1,
                    "member_ids": [4, 5],
                    "simulation_owner": "local",
                    "handoff_epoch": 1,
                    "exact_pair_with_leader": True,
                    "pair_contract_valid": True,
                    "local_return_eligibility": {"valid": True},
                },
                "current_scout_report": {
                    "present": True,
                    "source_activity_id": "stale-scout-report",
                    "source_generation": 8,
                    "carrier_ids": [81, 82],
                },
                "active_hostile_operation": {"reservation": {
                    "activity_id": "replacement-operation",
                    "generation": 9,
                    "member_ids": [91, 92],
                }},
            }],
        }

        receipt = normalize_saved_artifact_receipt(
            metadata, "audit_saved_bandit_live_world_state"
        )

        self.assertEqual(receipt["identity"], {
            "site_id": "overmap_special:bandit_camp@177,13,0",
            "operation_id": "camp-r008#structural",
            "generation": 1,
            "actor_ids": [4, 5],
            "source": "schema11_active_outing_local_return",
            "simulation_owner": "local",
            "handoff_epoch": 1,
        })

    def test_invalid_schema11_local_return_receipt_cannot_fall_back_to_other_identity(self):
        metadata = {
            "matching_sites": [{
                "site_id": "overmap_special:bandit_camp@177,13,0",
                "active_outing": {
                    "schema_version": 11,
                    "is_active": True,
                    "kind": "structural_sortie",
                    "activity_id": "camp-r008#structural",
                    "generation": 1,
                    "member_ids": [4, 4],
                    "simulation_owner": "local",
                    "handoff_epoch": 1,
                    "exact_pair_with_leader": False,
                    "pair_contract_valid": False,
                    "local_return_eligibility": {"valid": False},
                },
                "current_scout_report": {
                    "present": True,
                    "source_activity_id": "stale-scout-report",
                    "source_generation": 8,
                    "carrier_ids": [81, 82],
                },
                "active_hostile_operation": {"reservation": {
                    "activity_id": "replacement-operation",
                    "generation": 9,
                    "member_ids": [91, 92],
                }},
            }],
        }

        receipt = normalize_saved_artifact_receipt(
            metadata, "audit_saved_bandit_live_world_state"
        )

        self.assertEqual(receipt["identity"]["source"], "invalid_schema11_active_outing")
        self.assertEqual(receipt["identity"]["operation_id"], "")
        self.assertEqual(receipt["identity"]["generation"], 0)
        self.assertEqual(receipt["identity"]["actor_ids"], [])

    def test_first_divergence_is_earliest_red_even_when_later_gate_is_green(self):
        gates = [
            {"id": "depart", "label": "departure", "predecessors": [], "expectations": [{"kind": "structured_event", "predicate": {"transition": "depart"}}]},
            {"id": "contact", "label": "contact", "predecessors": ["depart"], "expectations": [{"kind": "structured_event", "predicate": {"transition": "contact", "simulation_owner": "local"}}]},
            {"id": "return", "label": "return", "predecessors": ["contact"], "expectations": [{"kind": "structured_event", "predicate": {"transition": "return"}}]},
        ]
        evidence = [
            {"id": "depart", "status": "green", "event_range": {"sequence_start_exclusive": 0, "sequence_end_inclusive": 1}},
            {"id": "contact", "status": "red", "event_range": {"sequence_start_exclusive": 1, "sequence_end_inclusive": 2}},
            {"id": "return", "status": "green", "event_range": {"sequence_start_exclusive": 2, "sequence_end_inclusive": 3}},
        ]
        result = build_first_divergence_diagnostic(gates, evidence, events=[event(2, transition="contact", simulation_owner="abstract")], run_id="run-a")
        self.assertEqual(result["earliest_first_red"], {"gate_id": "contact", "label": "contact", "index": 1})
        self.assertEqual(result["last_green"], "depart")
        self.assertEqual(result["causal_prefix"], ["depart", "contact"])
        owner = result["first_divergence"]["authoritative_simulation_owner"]
        self.assertEqual(owner["expected"], "local")
        self.assertEqual(owner["observed"], "abstract")
        self.assertEqual(owner["value"], "abstract")
        self.assertEqual(owner["status"], "mismatch")

    def test_each_successive_red_gate_keeps_its_boundary_against_later_green(self):
        gates = [
            {"id": "a", "label": "a", "predecessors": [], "expectations": []},
            {"id": "b", "label": "b", "predecessors": ["a"], "expectations": []},
            {"id": "c", "label": "c", "predecessors": ["b"], "expectations": []},
        ]
        for red_index in range(len(gates)):
            evidence = [{"id": gate["id"], "status": "red" if index == red_index else "green"}
                        for index, gate in enumerate(gates)]
            result = build_first_divergence_diagnostic(gates, evidence)
            self.assertEqual(result["earliest_first_red"]["index"], red_index)
            self.assertEqual(result["causal_prefix"], [gate["id"] for gate in gates[:red_index + 1]])

    def test_first_divergence_rejects_ambiguous_chain_and_recommends_no_capsule_fail_closed(self):
        gates = [
            {"id": "a", "predecessors": [], "expectations": []},
            {"id": "b", "predecessors": ["wrong"], "expectations": []},
        ]
        rejected = build_first_divergence_diagnostic(gates, [{"id": "a", "status": "red"}, {"id": "b", "status": "green"}])
        self.assertEqual(rejected["status"], "rejected")
        gates[1]["predecessors"] = ["a"]
        result = build_first_divergence_diagnostic(
            gates, [{"id": "a", "status": "red", "event_range": {}}, {"id": "b", "status": "green"}],
            binding_id="binding-a", capsule_candidates=[{"capsule_id": "newer", "binding_id": "binding-b", "last_green_index": 9}],
        )
        self.assertFalse(result["capsule_recommendation"]["selected"])
        self.assertIn("deferred to G3", result["capsule_recommendation"]["reason"])
        self.assertEqual(result["first_divergence"]["authoritative_simulation_owner"]["status"], "missing_fail_closed")

    def test_bind_transition_event_stream_publishes_empty_raw_artifact(self):
        with tempfile.TemporaryDirectory() as temp:
            binding = bind_transition_event_stream(Path(temp), run_id="run-a")
            path = Path(binding["event_path"])
            self.assertTrue(path.is_file())
            self.assertEqual(path.read_bytes(), b"")
            poll = StructuredTransitionEventReader(path, binding["run_id"]).poll()
            self.assertEqual(poll["events"], [])
            self.assertEqual(poll["diagnostics"], [])

    def test_sequence_gap_in_gate_range_blocks_matching_committed_event(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "events.jsonl"
            path.write_text(
                json.dumps(event(1)) + "\n" + json.dumps(event(3)) + "\n",
                encoding="utf-8",
            )
            reader = StructuredTransitionEventReader(path, "run-a")
            polled = reader.poll()
            self.assertEqual([item["sequence"] for item in polled["events"]], [1])
            self.assertEqual(polled["diagnostics"][0]["kind"], "invalid_sequence")
            gate = [{
                "id": "dispatch", "label": "dispatch", "boundary_step": "contact", "predecessors": [],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "hostile_operation", "transition": "active_sortie_dispatch_contact", "outcome": "committed",
                }}], "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
            }]
            result = evaluate_structured_proof_gates(
                gate, events=polled["events"],
                watermarks={"contact": polled["watermark"]},
                diagnostics=polled["diagnostics"], run_id="run-a",
            )
        self.assertEqual(result["status"], "red")
        self.assertEqual(result["gates"][0]["integrity_diagnostics"][0]["kind"], "invalid_sequence")

    def test_producer_shaped_integer_actor_ids_drive_dispatch_and_contact_gate(self):
        dispatch = event(
            1,
            domain="bandit_live_world",
            transition="active_sortie_dispatch",
            actor_ids=[101, 102],
            handoff_epoch=0,
            previous_state="assembling",
            new_state="outbound",
            reason="night_dispatch",
        )
        contact = event(
            2,
            domain="bandit_live_world",
            transition="active_sortie_local_contact",
            actor_ids=[101, 102],
            previous_state="outbound",
            new_state="committed_contact",
            reason="local_contact",
        )
        gates = [{
            "id": "cannibal",
            "label": "dispatch/contact",
            "boundary_step": "contact",
            "predecessors": [],
            "expectations": [
                {"kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "active_sortie_dispatch", "outcome": "committed",
                    "handoff_epoch": 0,
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }},
                {"kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "active_sortie_local_contact", "outcome": "committed",
                    "handoff_epoch": 1,
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }},
            ],
            "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
        }]
        result = evaluate_structured_proof_gates(
            gates,
            events=[dispatch, contact],
            watermarks={"contact": {"byte_offset": 1000, "last_sequence": 2, "run_id": "run-a"}},
            run_id="run-a",
        )
        self.assertEqual(result["status"], "green")
        self.assertEqual(result["gates"][0]["identity_correlation"]["status"], "matched")

        mismatched_contact = {**contact, "actor_ids": [101, 999]}
        rejected = evaluate_structured_proof_gates(
            gates,
            events=[dispatch, mismatched_contact],
            watermarks={"contact": {"byte_offset": 1000, "last_sequence": 2, "run_id": "run-a"}},
            run_id="run-a",
        )
        self.assertEqual(rejected["status"], "red")
        self.assertEqual(rejected["gates"][0]["identity_correlation"]["status"], "mismatch")

        wrong_epoch = {**contact, "handoff_epoch": 0}
        rejected_epoch = evaluate_structured_proof_gates(
            gates,
            events=[dispatch, wrong_epoch],
            watermarks={"contact": {"byte_offset": 1000, "last_sequence": 2, "run_id": "run-a"}},
            run_id="run-a",
        )
        self.assertEqual(rejected_epoch["status"], "red")
        self.assertEqual(rejected_epoch["gates"][0]["identity_correlation"]["status"], "matched")

    def test_actor_id_validation_rejects_empty_boolean_and_object_identities(self):
        with tempfile.TemporaryDirectory() as temp:
            invalid_kinds = []
            for index, actor_ids in enumerate(([], [True], [{"id": 3}])):
                path = Path(temp) / f"invalid-{index}.jsonl"
                path.write_text(json.dumps(event(1, actor_ids=actor_ids)) + "\n", encoding="utf-8")
                invalid = StructuredTransitionEventReader(path, "run-a").poll()
                invalid_kinds.append(invalid["diagnostics"][0]["kind"])
            valid_path = Path(temp) / "valid-events.jsonl"
            valid_path.write_text(json.dumps(event(1, actor_ids=[7, "npc-8"])) + "\n", encoding="utf-8")
            valid = StructuredTransitionEventReader(valid_path, "run-a").poll()
        self.assertEqual(invalid_kinds, ["invalid_record"] * 3)
        self.assertEqual(valid["events"][0]["actor_ids"], [7, "npc-8"])

    def test_binary_cursor_preserves_truncated_tail_and_rejects_bad_records(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "events.jsonl"
            path.write_bytes((json.dumps(event(1)) + "\n").encode() + b'{"schema_version":1')
            reader = StructuredTransitionEventReader(path, "run-a")
            first = reader.poll()
            self.assertEqual([item["sequence"] for item in first["events"]], [1])
            self.assertEqual(reader.state.byte_offset, len((json.dumps(event(1)) + "\n").encode()))
            self.assertEqual(first["diagnostics"][0]["kind"], "truncated_tail")
            with path.open("ab") as stream:
                stream.write(b',"sequence":2,"run_id":"run-a","game_minutes":2,"domain":"hostile_operation",'
                             b'"transition":"active_sortie_dispatch_contact","outcome":"committed"}\n')
            second = reader.poll()
            self.assertEqual([item["sequence"] for item in second["events"]], [2])

    def test_gate_uses_only_current_range_and_committed_matching_identity(self):
        events = [
            {**event(1), "transition": "unrelated"},
            event(2),
            event(3, outcome="rejected"),
            event(4, run_id="other"),
        ]
        gates = [{
            "id": "dispatch",
            "label": "dispatch/contact",
            "boundary_step": "contact",
            "predecessors": [],
            "expectations": [{"kind": "structured_event", "predicate": {
                "domain": "hostile_operation",
                "transition": "active_sortie_dispatch_contact",
                "outcome": "committed",
                "operation_id": "camp#raid",
                "generation": 1,
                "handoff_epoch": 1,
                "actor_ids": ["npc-1", "npc-2"],
            }}],
            "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
        }]
        result = evaluate_structured_proof_gates(
            gates,
            events=events,
            watermarks={"contact": {"byte_offset": 1000, "last_sequence": 2, "run_id": "run-a"}},
            run_id="run-a",
        )
        self.assertEqual(result["status"], "green")
        self.assertEqual(result["gates"][0]["expectations"][0]["event_references"][0]["sequence"], 2)

    def test_artifact_only_gate_does_not_consume_later_local_handoff(self):
        dispatch = event(
            1, domain="bandit_live_world", transition="active_sortie_dispatch",
            simulation_owner="abstract", actor_ids=[4, 5], handoff_epoch=0,
        )
        local_handoff = event(
            2, domain="bandit_live_world", transition="local_pair_handoff",
            simulation_owner="local", actor_ids=[4, 5], handoff_epoch=1,
        )
        gates = [
            {
                "id": "dispatch", "boundary_step": "dispatch", "predecessors": [],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "active_sortie_dispatch",
                    "outcome": "committed", "simulation_owner": "abstract",
                    "handoff_epoch": 0,
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }}],
            },
            {
                "id": "shared_route", "boundary_step": "shared_route", "predecessors": ["dispatch"],
                "expectations": [{"kind": "saved_artifact", "predicate": {
                    "same_run": True, "artifact_kind": "route_audit",
                }}],
            },
            {
                # The audit which verifies the abstract route already observed
                # the local receipt; this gate must own the same boundary's
                # event delta, not wait for a later duplicate observation.
                "id": "local_handoff", "boundary_step": "shared_route", "predecessors": ["shared_route"],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "local_pair_handoff",
                    "outcome": "committed", "simulation_owner": "local",
                    "handoff_epoch": 1,
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }}],
            },
        ]
        watermarks = {
            "dispatch": {"last_sequence": 1, "byte_offset": 100, "run_id": "run-a", "step_index": 1},
            # The local receipt is already present by the later artifact audit.
            "shared_route": {"last_sequence": 2, "byte_offset": 200, "run_id": "run-a", "step_index": 2},
        }
        artifact = {
            "artifact_kind": "route_audit", "run_id": "run-a", "producer_step_index": 2,
        }
        result = evaluate_structured_proof_gates(
            gates, events=[dispatch, local_handoff], watermarks=watermarks,
            saved_artifacts=[artifact], run_id="run-a",
        )

        self.assertEqual(result["status"], "green")
        self.assertEqual(result["gates"][1]["event_range"], {
            "sequence_start_exclusive": 1, "sequence_end_inclusive": 1,
            "byte_start": 100, "byte_end_exclusive": 100,
        })
        handoff_refs = result["gates"][2]["expectations"][0]["event_references"]
        self.assertEqual(handoff_refs, [{"sequence": 2, "byte_start": None, "byte_end": None}])

        stale_handoff = {**local_handoff, "sequence": 1}
        delayed_dispatch = {**dispatch, "sequence": 2}
        stale = evaluate_structured_proof_gates(
            gates, events=[stale_handoff, delayed_dispatch], watermarks=watermarks,
            saved_artifacts=[artifact], run_id="run-a",
        )
        self.assertEqual(stale["gates"][2]["status"], "red")
        self.assertEqual(stale["gates"][2]["expectations"][0]["event_references"], [])

    def test_cross_gate_continuity_carries_stable_actor_identity_to_saved_return(self):
        handoff = event(
            1,
            domain="bandit_live_world",
            transition="local_pair_handoff",
            operation_id="camp#scout",
            generation=7,
            actor_ids=[101, 102],
            handoff_epoch=1,
        )
        gates = [
            {
                "id": "handoff",
                "label": "handoff",
                "boundary_step": "handoff",
                "predecessors": [],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "bandit_live_world", "transition": "local_pair_handoff", "outcome": "committed",
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }}],
            },
            {
                "id": "return",
                "label": "return",
                "boundary_step": "return",
                "predecessors": ["handoff"],
                "expectations": [{"kind": "saved_artifact", "predicate": {
                    "artifact_kind": "audit_saved_bandit_live_world_state",
                    "same_run": True,
                    "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                }}],
            },
        ]
        watermarks = {
            "handoff": {"byte_offset": 100, "last_sequence": 1, "run_id": "run-a", "step_index": 1},
            "return": {"byte_offset": 100, "last_sequence": 1, "run_id": "run-a", "step_index": 2},
        }
        receipt = {
            "artifact_kind": "audit_saved_bandit_live_world_state",
            "run_id": "run-a",
            "producer_step_index": 2,
            "identity": {
                "site_id": handoff["site_id"], "operation_id": "camp#scout",
                "generation": 7, "actor_ids": [101, 102],
            },
        }
        green = evaluate_structured_proof_gates(
            gates, events=[handoff], watermarks=watermarks,
            saved_artifacts=[receipt], run_id="run-a",
        )
        self.assertEqual(green["status"], "green")
        bad_receipt = {**receipt, "identity": {**receipt["identity"], "actor_ids": [101, 999]}}
        red = evaluate_structured_proof_gates(
            gates, events=[handoff], watermarks=watermarks,
            saved_artifacts=[bad_receipt], run_id="run-a",
        )
        self.assertEqual(red["status"], "red")
        self.assertEqual(red["gates"][1]["identity_correlation"]["status"], "mismatch")

    def test_pending_truncated_tail_cannot_green_gate_at_its_boundary(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "events.jsonl"
            complete = json.dumps(event(1)) + "\n"
            path.write_bytes(complete.encode() + b'{"schema_version":1,"sequence":2')
            reader = StructuredTransitionEventReader(path, "run-a")
            polled = reader.poll()
            gate = [{
                "id": "contact", "label": "contact", "boundary_step": "contact", "predecessors": [],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "hostile_operation", "transition": "active_sortie_dispatch_contact", "outcome": "committed",
                }}],
            }]
            result = evaluate_structured_proof_gates(
                gate, events=polled["events"],
                watermarks={"contact": polled["watermark"]},
                diagnostics=polled["diagnostics"], run_id="run-a",
            )
        self.assertEqual(result["status"], "red")
        self.assertEqual(result["gates"][0]["integrity_diagnostics"][0]["kind"], "truncated_tail")

    def test_stream_replacement_discards_old_events_and_poison_survives_smaller_range(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "events.jsonl"
            path.write_text(json.dumps(event(1, reason="old-stream-record-with-padding")) + "\n", encoding="utf-8")
            reader = StructuredTransitionEventReader(path, "run-a")
            first = reader.poll()
            self.assertEqual([item["sequence"] for item in first["events"]], [1])
            replacement = {
                "schema_version": 1, "sequence": 1, "run_id": "run-a", "game_minutes": 2,
                "domain": "hostile_operation", "transition": "active_sortie_dispatch_contact",
                "outcome": "committed",
            }
            path.write_text(json.dumps(replacement) + "\n", encoding="utf-8")
            second = reader.poll()
            self.assertTrue(second["discard_prior_events"])
            self.assertEqual([item["sequence"] for item in reader.events], [1])
            self.assertEqual(len(reader.events), 1)
            self.assertTrue(second["watermark"]["integrity_poisoned"])
            gate = [{
                "id": "contact", "label": "contact", "boundary_step": "contact", "predecessors": [],
                "expectations": [{"kind": "structured_event", "predicate": {
                    "domain": "hostile_operation", "transition": "active_sortie_dispatch_contact",
                    "outcome": "committed",
                }}],
            }]
            result = evaluate_structured_proof_gates(
                gate, events=reader.events,
                watermarks={"contact": second["watermark"]},
                diagnostics=second["diagnostics"], run_id="run-a",
            )
        self.assertEqual(result["status"], "red")
        self.assertTrue(result["gates"][0]["integrity_poisoned"])

    def test_consumed_prefix_hash_detects_equal_size_and_larger_overwrite(self):
        replacements = (
            ("old1", "new1", "equal_size"),
            ("x", "replacement-with-larger-prefix", "larger"),
        )
        for old_reason, new_reason, case in replacements:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "events.jsonl"
                old_record = event(1, reason=old_reason)
                new_record = event(1, reason=new_reason)
                old_bytes = (json.dumps(old_record) + "\n").encode()
                new_bytes = (json.dumps(new_record) + "\n").encode()
                if case == "equal_size":
                    self.assertEqual(len(old_bytes), len(new_bytes))
                else:
                    self.assertGreater(len(new_bytes), len(old_bytes))
                path.write_bytes(old_bytes)
                reader = StructuredTransitionEventReader(path, "run-a")
                first = reader.poll()
                self.assertTrue(first["watermark"]["consumed_prefix_sha256"])
                path.write_bytes(new_bytes)
                second = reader.poll()
                self.assertTrue(second["discard_prior_events"])
                self.assertTrue(second["watermark"]["integrity_poisoned"])
                self.assertEqual(second["diagnostics"][0]["replacement_reason"], "consumed_prefix_mismatch")
                self.assertEqual(len(reader.events), 1)
                self.assertEqual(reader.events[0]["reason"], new_reason)

    def test_named_saved_receipt_prefers_completed_scout_over_follow_on_reservation(self):
        with tempfile.TemporaryDirectory() as temp:
            dimension_path = Path(temp) / "o.0.0"
            dimension_path.write_text("saved-bandit-state", encoding="utf-8")

            def receipt(carrier_ids):
                metadata = {
                    "dimension_path": str(dimension_path),
                    "status": "required_state_present",
                    "required_fields": {"required_active_outside_count": 0},
                    "matching_sites": [{
                        "site_id": "overmap_special:bandit_camp@164,39,0",
                        "active_outing": {"is_active": False, "handoff_epoch": 3},
                        "current_scout_report": {
                            "present": True,
                            "source_activity_id": "camp#scout-7",
                            "source_generation": 7,
                            "carrier_ids": carrier_ids,
                        },
                        "active_hostile_operation": {"reservation": {
                            "activity_id": "camp#follow-on-8",
                            "generation": 8,
                            "member_ids": [201, 202],
                        }},
                    }],
                }
                normalized = normalize_saved_artifact_receipt(
                    metadata, "audit_saved_bandit_live_world_state",
                    producer_step_label="return", producer_step_index=2,
                )
                normalized["run_id"] = "run-a"
                return normalized

            handoff = event(
                1, domain="bandit_live_world", transition="local_pair_handoff",
                site_id="overmap_special:bandit_camp@164,39,0",
                operation_id="camp#scout-7", generation=7, actor_ids=[101, 102], handoff_epoch=3,
            )
            gates = [
                {
                    "id": "handoff", "label": "handoff", "boundary_step": "handoff", "predecessors": [],
                    "expectations": [{"kind": "structured_event", "predicate": {
                        "domain": "bandit_live_world", "transition": "local_pair_handoff",
                        "outcome": "committed",
                        "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids", "handoff_epoch"],
                    }}],
                },
                {
                    "id": "return", "label": "return", "boundary_step": "return",
                    "predecessors": ["handoff"],
                    "expectations": [{"kind": "saved_artifact", "predicate": {
                        "artifact_kind": "audit_saved_bandit_live_world_state",
                        "status": "required_state_present", "artifact_hash_present": True,
                        "same_run": True,
                        "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids", "handoff_epoch"],
                    }}],
                },
            ]
            watermarks = {
                "handoff": {"byte_offset": 100, "last_sequence": 1, "run_id": "run-a", "step_index": 1},
                "return": {"byte_offset": 100, "last_sequence": 1, "run_id": "run-a", "step_index": 2},
            }
            scout_receipt = receipt([101, 102])
            self.assertEqual(scout_receipt["identity"]["source"], "current_scout_report")
            self.assertEqual(scout_receipt["identity"]["operation_id"], "camp#scout-7")
            self.assertEqual(scout_receipt["identity"]["actor_ids"], [101, 102])
            self.assertEqual(scout_receipt["identity"]["handoff_epoch"], 3)
            green = evaluate_structured_proof_gates(
                gates, events=[handoff], watermarks=watermarks,
                saved_artifacts=[scout_receipt], run_id="run-a",
            )
            self.assertEqual(green["status"], "green")
            for bad_receipt in (receipt([101, 999]), receipt([])):
                red = evaluate_structured_proof_gates(
                    gates, events=[handoff], watermarks=watermarks,
                    saved_artifacts=[bad_receipt], run_id="run-a",
                )
                self.assertEqual(red["status"], "red")

    def test_saved_artifact_requires_same_run_and_explicit_audit(self):
        gate = [{
            "id": "saved",
            "label": "saved",
            "boundary_step": "saved",
            "predecessors": [],
            "expectations": [{"kind": "saved_artifact", "predicate": {
                "audit": "survivors_home_and_outing_closed", "same_run": True,
            }}],
            "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
        }]
        common = {"audit": "survivors_home_and_outing_closed", "status": "required_state_present", "artifact_path": "save.audit.json", "producer_step_index": 1}
        result = evaluate_structured_proof_gates(
            gate, events=[], watermarks={"saved": {"byte_offset": 0, "last_sequence": 0, "run_id": "run-a", "step_index": 1}},
            saved_artifacts=[{**common, "run_id": "other"}, {**common, "run_id": "run-a"}], run_id="run-a",
        )
        self.assertEqual(result["status"], "green")

    def test_saved_state_receipt_requires_real_status_fields_and_hash(self):
        gate = [{
            "id": "saved",
            "label": "saved",
            "boundary_step": "saved",
            "predecessors": [],
            "expectations": [{"kind": "saved_artifact", "predicate": {
                "artifact_kind": "audit_saved_bandit_live_world_state",
                "status": "required_state_present",
                "artifact_hash_present": True,
                "same_run": True,
                "continuity_fields": ["site_id", "operation_id", "generation", "actor_ids"],
                "required_fields": {"required_active_outside_count": 0},
            }}],
            "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
        }]
        receipt = {
            "artifact_kind": "audit_saved_bandit_live_world_state",
            "status": "required_state_present",
            "artifact_sha256": "abc123",
            "run_id": "run-a",
            "producer_step_index": 1,
            "identity": {"site_id": "camp@1", "operation_id": "camp#1", "generation": 2, "actor_ids": [11, 12]},
            "required_fields": {"required_active_outside_count": 0},
        }
        watermarks = {"saved": {"byte_offset": 0, "last_sequence": 0, "run_id": "run-a", "step_index": 1}}
        self.assertEqual(evaluate_structured_proof_gates(
            gate, events=[], watermarks=watermarks, saved_artifacts=[receipt], run_id="run-a"
        )["status"], "green")
        bad_receipt = {**receipt, "artifact_sha256": ""}
        self.assertEqual(evaluate_structured_proof_gates(
            gate, events=[], watermarks=watermarks, saved_artifacts=[bad_receipt], run_id="run-a"
        )["status"], "red")

    def test_future_saved_artifact_cannot_backfill_earlier_gate(self):
        predicate = {"artifact_kind": "audit_saved_bandit_live_world_state", "same_run": True}
        gates = [
            {
                "id": "early", "label": "early", "boundary_step": "early", "predecessors": [],
                "expectations": [{"kind": "saved_artifact", "predicate": predicate}],
            },
            {
                "id": "late", "label": "late", "boundary_step": "late", "predecessors": ["early"],
                "expectations": [{"kind": "saved_artifact", "predicate": predicate}],
            },
        ]
        artifact = {
            "artifact_kind": "audit_saved_bandit_live_world_state",
            "run_id": "run-a", "producer_step_label": "late", "producer_step_index": 2,
        }
        result = evaluate_structured_proof_gates(
            gates, events=[], saved_artifacts=[artifact], run_id="run-a",
            watermarks={
                "early": {"byte_offset": 0, "last_sequence": 0, "run_id": "run-a", "step_index": 1},
                "late": {"byte_offset": 0, "last_sequence": 0, "run_id": "run-a", "step_index": 2},
            },
        )
        self.assertEqual([gate["status"] for gate in result["gates"]], ["red", "green"])

    def test_noncommitted_semantic_summary_aggregates_repeated_stable_identity(self):
        first = event(
            1, outcome="rejected", previous_state="outbound", new_state="outbound",
            reason="preconditions invalid", _stream_byte_start=0, _stream_byte_end=100,
        )
        repeated = event(
            2, outcome="rejected", previous_state="outbound", new_state="outbound",
            reason="preconditions invalid", _stream_byte_start=100, _stream_byte_end=205,
        )
        diagnostic = event(
            3, outcome="diagnostic", previous_state="outbound", new_state="outbound",
            reason="writeback failed", _stream_byte_start=205, _stream_byte_end=320,
        )
        summary = summarize_noncommitted_transition_events([first, repeated, diagnostic, event(4)])
        self.assertEqual(len(summary), 2)
        rejected = next(row for row in summary if row["identity"]["outcome"] == "rejected")
        self.assertEqual(rejected["count"], 2)
        self.assertEqual(rejected["first_reference"], {"sequence": 1, "byte_start": 0, "byte_end": 100})
        self.assertEqual(rejected["last_reference"], {"sequence": 2, "byte_start": 100, "byte_end": 205})


if __name__ == "__main__":
    unittest.main()
