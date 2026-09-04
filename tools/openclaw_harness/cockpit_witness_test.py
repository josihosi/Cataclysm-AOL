#!/usr/bin/env python3
"""Live cockpit contract checks for WITNESS / FINISH."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from cockpit import CockpitRunChannel, CockpitService  # noqa: E402


CHARTER = {
    "claim": "The selected clean playtest began at minute 100.",
    "material_proof": "A bound native avatar observation reports minute 100.",
    "material_contradiction": ["another bound minute"],
    "already_accepted_evidence": [],
    "current_uncertainty": "the current native minute",
    "forbidden_shortcuts": ["OCR"],
    "honest_stop_conditions": ["a bound observation"],
    "requested_evidence_ceiling": "focused",
}


def frame() -> dict[str, object]:
    return {
        "run_id": "run-a", "frame_id": "run-a:1", "observed_turn": 1,
        "game_minutes": 100, "valid_actions": ["world.wait"],
        "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1",
            "avatar": {"name": "A"},
            "visible_local": [], "visible_entities": [], "visible_zones": [],
        },
    }


class CockpitWitnessTest(unittest.TestCase):
    def service(self) -> CockpitService:
        channel = CockpitRunChannel(
            frame, binding_id="binding-a",
            witness_charter=CHARTER,
            witness_identity={
                "scenario_id": "scenario-a", "source_identity": "source-a",
                "executable_identity": "exe-a",
            },
        )
        return CockpitService(run_channel=channel)

    def test_witness_seals_input_then_finish_validates_citations(self) -> None:
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        sealed = service.call({
            "action": "run.witness", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
        })
        self.assertTrue(sealed["ok"])
        self.assertEqual(sealed["result"]["action"], "WITNESS / FINISH")
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })["error"], "live_session_finished")
        witness = {
            "verdict": "proved",
            "smallest_supported_claim": "This run was observed at minute 100.",
            "causal_account": "The native avatar frame directly supplies the minute.",
            "citations": [
                {"citation_id": "J0002", "meaning": "bound native minute",
                 "checks": {"value.game_minutes": 100}},
                {"citation_id": "J0003", "meaning": "the charter stopped as settled",
                 "checks": {"stop_reason": "claim_settled"}},
            ],
            "recommended_disposition": "accept",
            "evidence_ceiling": "focused",
        }
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
            "witness": witness,
        })
        self.assertTrue(finished["ok"])
        detail = finished["result"]["stop_detail"]
        self.assertEqual(detail["witness_validation"]["status"], "mechanically_valid")
        self.assertEqual(detail["evidence_journal"]["identity"]["binding_id"], "binding-a")

    def test_finish_cannot_skip_or_rewrite_the_sealed_witness(self) -> None:
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        missing = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
        })
        self.assertEqual(missing["error"], "witness_journal_must_be_sealed_before_finish")
        service.call({
            "action": "run.witness", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
        })
        rewritten = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "different", "unused_authority": "none", "witness": {},
        })
        self.assertEqual(rewritten["error"], "finish_does_not_match_sealed_witness_terminal")

    def test_finish_accepts_documented_witness_statement_alias(self) -> None:
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        service.call({
            "action": "run.witness", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
        })
        statement = {
            "verdict": "proved",
            "smallest_supported_claim": "This run was observed at minute 100.",
            "causal_account": "The native avatar frame directly supplies the minute.",
            "citations": [
                {"citation_id": "J0002", "meaning": "bound native minute",
                 "checks": {"value.game_minutes": 100}},
                {"citation_id": "J0003", "meaning": "the charter stopped as settled",
                 "checks": {"stop_reason": "claim_settled"}},
            ],
            "recommended_disposition": "accept",
            "evidence_ceiling": "focused",
        }
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "claim_settled", "unused_authority": "none",
            "witness_statement": statement,
        })
        self.assertTrue(finished["ok"])

    def test_finish_rejects_ambiguous_witness_payloads(self) -> None:
        service = self.service()
        self.assertEqual(service.call({
            "action": "run.finish", "observation_id": "unused", "stop_reason": "x",
            "unused_authority": "none", "witness": {}, "witness_statement": {},
        }), {"ok": False, "error": "finish_accepts_one_witness_payload"})

    def test_player_fire_setup_requires_a_fresh_native_result(self) -> None:
        current = frame()
        current["observation"] = {
            "schema": "caol-avatar-visible-v1", "avatar": {"name": "A"},
            "visible_local": [{"dx": 1, "dy": 0, "fields": []}],
            "visible_entities": [], "visible_zones": [],
        }

        def read_frame() -> dict[str, object]:
            return current

        def fire_setup(issuing: dict[str, object]) -> dict[str, object]:
            nonlocal current
            current = {
                **frame(), "frame_id": "run-a:2", "observed_turn": 2,
                "observation": {
                    "schema": "caol-avatar-visible-v1", "avatar": {"name": "A"},
                    "visible_local": [{
                        "dx": 1, "dy": 0, "furniture": "f_brazier", "fields": ["fd_fire"],
                    }], "visible_entities": [], "visible_zones": [],
                },
            }
            return {
                "native_receipt": {
                    "frame_id": issuing["frame_id"], "action_id": "player.fire.setup",
                    "accepted": True,
                },
                "next_frame": current,
            }

        channel = CockpitRunChannel(read_frame, binding_id="binding-a",
                                    dispatch_player_fire_setup=fire_setup)
        service = CockpitService(run_channel=channel)
        observed = service.call({"action": "game.observe"})["result"]
        result = service.call({
            "action": "game.player_fire_setup", "observation_id": observed["observation_id"],
            "player_fire_setup": {"intent": "deploy_and_ignite_brazier"},
        })
        self.assertTrue(result["ok"])
        self.assertEqual(result["receipt"]["native_receipt"]["action_id"], "player.fire.setup")
        self.assertEqual(result["observation"]["observation_id"], "run-a:2")

    def test_finish_accepts_claim_scoped_bundle_and_routes_finding(self) -> None:
        service = self.service()
        observed = service.call({"action": "game.observe"})["result"]
        service.call({
            "action": "run.witness", "observation_id": observed["observation_id"],
            "stop_reason": "mixed_claims_settled", "unused_authority": "none",
        })

        def statement(verdict: str, claim: str) -> dict[str, object]:
            return {
                "verdict": verdict,
                "smallest_supported_claim": claim,
                "causal_account": "The bound native observation settles only this claim.",
                "citations": [{"citation_id": "J0002", "meaning": claim,
                               "checks": {"value.game_minutes": 100}}],
                "recommended_disposition": "repair" if verdict == "contradicted" else "accept",
                "evidence_ceiling": "focused",
            }

        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "mixed_claims_settled", "unused_authority": "none",
            "witness": {
                "schema": "caol-playtest-witness-bundle-v1",
                "claims": [
                    {"claim_id": "bandit_observation", "statement": statement(
                        "proved", "The bandit observation remains useful.",
                    )},
                    {"claim_id": "locker_coherence", "statement": statement(
                        "contradicted", "The locker interaction is defective.",
                    )},
                ],
                "findings": [{
                    "finding_id": "DBG-R008-LOCKER-001",
                    "observed_defect": "The locker interaction lost its reservation.",
                    "citations": ["J0002"],
                    "affected_claims": ["locker_coherence"],
                    "unaffected_claims": ["bandit_observation"],
                    "disposition": "repair",
                    "next_action": "Repair and revalidate locker coherence.",
                }],
            },
        })
        self.assertTrue(finished["ok"])
        validation = finished["result"]["stop_detail"]["witness_validation"]
        self.assertEqual(validation["status"], "mechanically_valid_bundle")
        self.assertEqual(validation["finding_count"], 1)


if __name__ == "__main__":
    unittest.main()
