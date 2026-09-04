#!/usr/bin/env python3
"""Behavioral counterexamples for the generic playtest witness boundary."""

from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from playtest_witness import (  # noqa: E402
    WitnessError,
    build_evidence_journal,
    compose_evidence_journals,
    normalize_witness_charter,
    review_witness,
    validate_witness_bundle,
    validate_witness_statement,
)


CHARTER = {
    "claim": "Raw bounded waiting advances one minute through native wait actions.",
    "material_proof": "A clean bound run advances exactly one minute and preserves accepted native receipts.",
    "material_contradiction": ["an interpreted interruption", "a rejected native receipt"],
    "already_accepted_evidence": ["the source and executable binding"],
    "current_uncertainty": "whether raw and primitive native sequences are semantically equivalent",
    "forbidden_shortcuts": ["setup state as gameplay proof", "caller evidence promotion"],
    "honest_stop_conditions": ["proof", "contradiction", "missing causal fact"],
    "requested_evidence_ceiling": "focused",
}


def journal(*, contradiction: bool = False, action_id: str = "wait.1m",
            run_id: str = "run-a") -> dict[str, object]:
    observation = {
        "observation_id": run_id + ":2", "run_id": run_id, "game_minutes": 101,
        "visible_entities": [], "advertised_actions": ["world.wait"],
        "delta": {"game_minutes": {"before": 100, "after": 101}},
        "compact_log": {
            "receipt_count": 3,
            "latest_receipt": {"action_id": action_id, "accepted": True},
            "contradictory_evidence": ([{"kind": "rejected_native_receipt"}]
                                       if contradiction else []),
        },
    }
    return build_evidence_journal(
        charter=CHARTER,
        identity={
            "scenario_id": "r018.raw_wait_acceptance_mcw",
            "source_identity": "source-a", "executable_identity": "exe-a",
            "run_id": run_id, "binding_id": "binding-a",
        },
        transcript=[
            {"kind": "action", "action_id": "world.wait", "observation_id": run_id + ":1",
             "result": {"receipt": {"native_receipt": {
                 "run_id": run_id, "frame_id": run_id + ":1",
                 "action_id": "world.wait", "accepted": True,
                 "reason": "native_transition_accepted",
             }}}},
            {"kind": "observation", "value": observation},
        ],
        terminal={"stop_reason": "target_reached", "cleanup": {"status": "terminated"}},
        evidence_ceiling="focused",
    )


def statement(*, wording: str = "The accepted native wait advanced one minute.") -> dict[str, object]:
    return {
        "verdict": "proved",
        "smallest_supported_claim": "This bound run advanced one minute through native wait.",
        "causal_account": wording,
        "citations": [
            {"citation_id": "J0002", "meaning": "the native action was accepted",
             "checks": {"native_receipt.accepted": True}},
            {"citation_id": "J0003", "meaning": "game time changed by one minute",
             "checks": {"value.delta.game_minutes.after": 101}},
            {"citation_id": "J0004", "meaning": "the worker stopped at the target",
             "checks": {"stop_reason": "target_reached"}},
        ],
        "recommended_disposition": "accept",
        "evidence_ceiling": "focused",
    }


def charter_citing(*, predecessor: dict[str, object]) -> dict[str, object]:
    value = copy.deepcopy(CHARTER)
    value["accepted_predecessor_witnesses"] = [predecessor]
    return value


def current_journal(*, charter: dict[str, object], run_id: str) -> dict[str, object]:
    return build_evidence_journal(
        charter=charter,
        identity={
            "scenario_id": "r018.raw_wait_acceptance_mcw",
            "source_identity": "source-a", "executable_identity": "exe-a",
            "run_id": run_id, "binding_id": "binding-a",
        },
        transcript=[], terminal={"stop_reason": "target_reached"},
        evidence_ceiling="focused",
    )


class PlaytestWitnessTest(unittest.TestCase):
    def test_mixed_run_routes_one_defect_without_erasing_other_claims(self) -> None:
        transcript = [
            {"kind": "observation", "value": {
                "observation_id": "mixed:1", "run_id": "mixed", "game_minutes": 100,
                "visible_entities": [{"handle": "bandit-4", "kind": "npc", "name": "bandit"}],
                "advertised_actions": ["world.wait"], "delta": {}, "compact_log": {},
            }},
            {"kind": "observation", "value": {
                "observation_id": "mixed:2", "run_id": "mixed", "game_minutes": 101,
                "visible_entities": [], "advertised_actions": ["world.wait"], "delta": {},
                "compact_log": {"contradictory_evidence": [{
                    "kind": "locker_reservation_lost", "zone": "CAMP_LOCKER",
                }]},
            }},
            {"kind": "observation", "value": {
                "observation_id": "mixed:3", "run_id": "mixed", "game_minutes": 102,
                "visible_entities": [{"handle": "patrol-2", "kind": "npc", "name": "patrol"}],
                "advertised_actions": ["world.wait"], "delta": {}, "compact_log": {},
            }},
        ]
        mixed = build_evidence_journal(
            charter=CHARTER,
            identity={
                "scenario_id": "r008.fire_signal_roof_bandit_mcw",
                "source_identity": "source-a", "executable_identity": "exe-a",
                "run_id": "mixed", "binding_id": "binding-a",
            },
            transcript=transcript, terminal={"stop_reason": "claims_settled"},
            evidence_ceiling="focused",
        )

        def claim(verdict: str, citation_id: str, supported: str) -> dict[str, object]:
            return {
                "verdict": verdict,
                "smallest_supported_claim": supported,
                "causal_account": "The cited bound observation settles only this claim.",
                "citations": [{"citation_id": citation_id, "meaning": supported, "checks": {}}],
                "contradictions": [{"citation_id": "J0004", "meaning": "locker defect"}],
                "recommended_disposition": "repair" if verdict == "contradicted" else "accept",
                "evidence_ceiling": "focused",
            }

        bundle = validate_witness_bundle(
            charter=CHARTER, journal=mixed, bundle={
                "schema": "caol-playtest-witness-bundle-v1",
                "claims": [
                    {"claim_id": "bandit_detection", "statement": claim(
                        "proved", "J0002", "The bound bandit remained observable.",
                    )},
                    {"claim_id": "locker_coherence", "statement": claim(
                        "contradicted", "J0004", "The locker lost its reservation.",
                    )},
                    {"claim_id": "patrol_observation", "statement": claim(
                        "proved", "J0005", "Patrol observation continued after the defect.",
                    )},
                ],
                "findings": [{
                    "finding_id": "DBG-R008-LOCKER-001",
                    "observed_defect": "CAMP_LOCKER lost a live reservation during contact.",
                    "citations": ["J0004"],
                    "affected_claims": ["locker_coherence"],
                    "unaffected_claims": ["bandit_detection", "patrol_observation"],
                    "disposition": "repair",
                    "next_action": "Repair reservation retention and revalidate the locker claim.",
                }],
            },
        )
        self.assertEqual(bundle["status"], "mechanically_valid_bundle")
        self.assertEqual(bundle["claim_count"], 3)
        self.assertEqual(bundle["finding_count"], 1)

        invalid = copy.deepcopy(bundle["bundle"])
        invalid["claims"] = [
            {"claim_id": item["claim_id"], "statement": item["validation"]["witness"]}
            for item in invalid["claims"]
        ]
        invalid["findings"][0]["unaffected_claims"] = ["locker_coherence"]
        with self.assertRaisesRegex(WitnessError, "finding_claim_scope_invalid"):
            validate_witness_bundle(charter=CHARTER, journal=mixed, bundle=invalid)

    def test_action_receipt_without_a_run_id_does_not_fabricate_none_identity(self) -> None:
        result = build_evidence_journal(
            charter=CHARTER,
            identity={
                "scenario_id": "r018.raw_wait_acceptance_mcw",
                "source_identity": "source-a", "executable_identity": "exe-a",
                "run_id": "run-a", "binding_id": "binding-a",
            },
            transcript=[{
                "kind": "action", "action_id": "activity.continue",
                "observation_id": "run-a:2",
                "result": {"receipt": {"native_receipt": {
                    "frame_id": "run-a:2", "action_id": "activity.continue", "accepted": True,
                }}},
            }],
            terminal={"stop_reason": "target_reached"}, evidence_ceiling="focused",
        )
        self.assertEqual(result["entries"][1]["value"]["native_receipt"]["run_id"], None)

    def test_multiple_valid_wordings_and_native_sequences_are_not_matrix_fields(self) -> None:
        first = validate_witness_statement(
            charter=CHARTER, journal=journal(), statement=statement(),
        )
        second_journal = journal(action_id="wait.duration_menu")
        second = validate_witness_statement(
            charter=CHARTER, journal=second_journal,
            statement=statement(wording="A semantically equivalent advertised sequence reached the same causal boundary."),
        )
        self.assertEqual(first["status"], "mechanically_valid")
        self.assertEqual(second["status"], "mechanically_valid")

    def test_independent_runs_form_one_citable_semantic_comparison(self) -> None:
        combined = compose_evidence_journals(
            charter=CHARTER,
            journals=[journal(run_id="raw-run"), journal(
                run_id="primitive-run", action_id="wait.duration_menu",
            )],
        )
        comparison = {
            "verdict": "proved",
            "smallest_supported_claim": "Both bound runs advanced one native game minute.",
            "causal_account": "Distinct run identities preserve equivalent terminal deltas.",
            "citations": [
                {"citation_id": "R1:J0003", "meaning": "raw terminal delta",
                 "checks": {"value.delta.game_minutes.after": 101}},
                {"citation_id": "R2:J0003", "meaning": "primitive terminal delta",
                 "checks": {"value.delta.game_minutes.after": 101}},
            ],
            "recommended_disposition": "accept",
            "evidence_ceiling": "focused",
        }
        validation = validate_witness_statement(
            charter=CHARTER, journal=combined, statement=comparison,
        )
        self.assertEqual(validation["status"], "mechanically_valid")
        self.assertNotEqual(combined["identities"][0]["run_id"],
                            combined["identities"][1]["run_id"])

    def test_current_charter_can_cite_an_exact_predecessor_journal(self) -> None:
        predecessor_charter = copy.deepcopy(CHARTER)
        predecessor_charter["claim"] = "The earlier raw and primitive comparison is accepted."
        predecessor = build_evidence_journal(
            charter=predecessor_charter,
            identity={
                "scenario_id": "r018.raw_wait_acceptance_mcw",
                "source_identity": "source-a", "executable_identity": "exe-a",
                "run_id": "accepted-predecessor", "binding_id": "binding-a",
            },
            transcript=[], terminal={"stop_reason": "target_reached"},
            evidence_ceiling="focused",
        )
        predecessor_identity = {
            "witness_id": "a" * 64,
            "charter_id": predecessor["charter_id"],
            "journal_sha256": predecessor["journal_sha256"],
        }
        current_charter = charter_citing(predecessor=predecessor_identity)
        combined = compose_evidence_journals(
            charter=current_charter,
            journals=[predecessor, current_journal(charter=current_charter, run_id="current-run")],
        )
        self.assertEqual(combined["source_charter_ids"], [
            predecessor["charter_id"],
            normalize_witness_charter(current_charter)["charter_id"],
        ])

    def test_uncited_mismatched_or_changed_predecessor_identity_fails_closed(self) -> None:
        predecessor_charter = copy.deepcopy(CHARTER)
        predecessor_charter["claim"] = "The earlier raw and primitive comparison is accepted."
        predecessor = build_evidence_journal(
            charter=predecessor_charter,
            identity={
                "scenario_id": "r018.raw_wait_acceptance_mcw",
                "source_identity": "source-a", "executable_identity": "exe-a",
                "run_id": "accepted-predecessor", "binding_id": "binding-a",
            },
            transcript=[], terminal={"stop_reason": "target_reached"},
            evidence_ceiling="focused",
        )
        citation = {
            "witness_id": "b" * 64,
            "charter_id": predecessor["charter_id"],
            "journal_sha256": predecessor["journal_sha256"],
        }
        with self.assertRaisesRegex(WitnessError, "journal_charter_mismatch"):
            compose_evidence_journals(
                charter=CHARTER, journals=[predecessor, journal(run_id="current-run")],
            )
        mismatched = dict(citation)
        mismatched["charter_id"] = "c" * 64
        with self.assertRaisesRegex(WitnessError, "journal_charter_mismatch"):
            compose_evidence_journals(
                charter=charter_citing(predecessor=mismatched),
                journals=[predecessor, current_journal(
                    charter=charter_citing(predecessor=mismatched), run_id="current-run",
                )],
            )
        changed_identity = build_evidence_journal(
            charter=predecessor_charter,
            identity={
                "scenario_id": "r018.raw_wait_acceptance_mcw",
                "source_identity": "source-a", "executable_identity": "exe-a",
                "run_id": "altered-predecessor", "binding_id": "binding-a",
            },
            transcript=[], terminal={"stop_reason": "target_reached"},
            evidence_ceiling="focused",
        )
        with self.assertRaisesRegex(WitnessError, "journal_charter_mismatch"):
            compose_evidence_journals(
                charter=charter_citing(predecessor=citation),
                journals=[changed_identity, current_journal(
                    charter=charter_citing(predecessor=citation), run_id="current-run",
                )],
            )

    def test_optional_clerical_lists_are_derived_without_replay(self) -> None:
        validation = validate_witness_statement(
            charter=CHARTER, journal=journal(), statement=statement(),
        )
        self.assertEqual(validation["witness"]["material_deviations"], [])
        self.assertEqual(validation["witness"]["remaining_unknowns"], [])
        reviewed = review_witness(
            validation, decision="accept", rationale="The cited native receipt and time delta settle the charter.",
        )
        self.assertEqual(reviewed["clerical_normalization"], "derived_without_gameplay_replay")

    def test_nonexistent_and_mismatched_citations_are_rejected(self) -> None:
        missing = statement()
        missing["citations"][0]["citation_id"] = "J9999"  # type: ignore[index]
        with self.assertRaisesRegex(WitnessError, "citation_missing_or_unknown"):
            validate_witness_statement(charter=CHARTER, journal=journal(), statement=missing)
        mismatch = statement()
        mismatch["citations"][1]["checks"] = {"value.delta.game_minutes.after": 999}  # type: ignore[index]
        with self.assertRaisesRegex(WitnessError, "citation_value_mismatch"):
            validate_witness_statement(charter=CHARTER, journal=journal(), statement=mismatch)

    def test_supplied_contradiction_cannot_be_hidden(self) -> None:
        hidden = statement()
        hidden["citations"][-1]["citation_id"] = "J0005"  # type: ignore[index]
        with self.assertRaisesRegex(WitnessError, "omits_supplied_contradiction"):
            validate_witness_statement(
                charter=CHARTER, journal=journal(contradiction=True), statement=hidden,
            )

    def test_identity_digest_and_evidence_promotion_fail_closed(self) -> None:
        drifted = copy.deepcopy(journal())
        drifted["identity"]["binding_id"] = "binding-b"  # type: ignore[index]
        with self.assertRaisesRegex(WitnessError, "journal_digest_mismatch"):
            validate_witness_statement(charter=CHARTER, journal=drifted, statement=statement())
        promoted = statement()
        promoted["evidence_ceiling"] = "certification"
        with self.assertRaisesRegex(WitnessError, "evidence_promotion"):
            validate_witness_statement(charter=CHARTER, journal=journal(), statement=promoted)

    def test_genuinely_absent_causal_fact_remains_inconclusive(self) -> None:
        value = statement()
        value.update({
            "verdict": "inconclusive",
            "smallest_supported_claim": "The native action was accepted, but terminal time is unavailable.",
            "causal_account": "The receipt proves dispatch only; it cannot prove the requested time transition.",
            "recommended_disposition": "continue",
            "citations": [value["citations"][0]],
            "remaining_unknowns": ["terminal game time"],
        })
        validation = validate_witness_statement(charter=CHARTER, journal=journal(), statement=value)
        with self.assertRaisesRegex(WitnessError, "concrete_causal_risk"):
            review_witness(validation, decision="continue", rationale="More evidence is needed.")
        reviewed = review_witness(
            validation, decision="continue", rationale="The terminal transition is not cited.",
            concrete_risk="Accepting could confuse action dispatch with completed time passage.",
        )
        self.assertEqual(reviewed["decision"], "continue")


if __name__ == "__main__":
    unittest.main()
