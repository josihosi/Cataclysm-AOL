#!/usr/bin/env python3
"""R-008 must obtain a new authority after its repair makes a red stale."""

import sys
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path( __file__ ).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

import scenario_registry_store as registry_store
from scenario_registry_store import (
    RegistryQueryCandidateSnapshot,
    RegistryQueryEvaluation,
    RegistryStoredQueryEvaluation,
    _registry_query_repair_action,
    parse_registry_query_request,
)


class R008StaleRepairAuthorityTest( unittest.TestCase ):
    def test_stale_exact_contradiction_can_issue_a_fresh_repair_authority( self ):
        request = parse_registry_query_request( {
            "requirements": [{
                "key": "runtime.r008.closure_231_bootstrap",
                "op": "eq",
                "value": "zero_credit_source_route_native_local_pair_after_disposable_player_fixture",
                "minimum_evidence": "declared",
            }],
            "preferences": [],
        } )
        stale = RegistryQueryCandidateSnapshot(
            scenario_id="r008-current-source", lifecycle_state="quarantined",
            token_eligible=False, facts={}, explanation={
                "manifest": {"manifest_id": "r008-current-source", "present": True},
                "route_evidence": [{
                    "route_key": "r008-source-route", "evidence_state": "stale",
                    "details": {"unresolved_contradiction_ids": []},
                }],
            },
        )
        evaluation = RegistryStoredQueryEvaluation(
            candidates=(stale,),
            evaluation=RegistryQueryEvaluation(candidates=(), ranked_scenario_ids=()),
        )
        with mock.patch.object(registry_store, "_repair_query_matches_manifest", return_value=True), \
                mock.patch.object(registry_store, "_stale_repairable_red_ids",
                                  return_value=("r008-red",)):
            action = _registry_query_repair_action(
                object(), query_id="r008-query", request=request, evaluation=evaluation,
            )

        self.assertEqual(action["required_identifiers"]["red_verification_id"], "r008-red")


if __name__ == "__main__":
    unittest.main()
