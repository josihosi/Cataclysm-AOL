#!/usr/bin/env python3
"""Regression coverage for R-019 stale-source authority recovery."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest import mock

import scenario_registry_store as registry_store


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

from scenario_registry_store import (  # noqa: E402
    RegistryQueryCandidateSnapshot,
    RegistryQueryEvaluation,
    RegistryStoredQueryEvaluation,
    _current_stale_bootstrap_candidate,
    _registry_query_repair_action,
    parse_registry_query_request,
)


class R019CurrentSourceRevalidationTest( unittest.TestCase ):
    def test_stale_yellow_source_is_revalidation_eligible_not_repair_eligible( self ) -> None:
        request = parse_registry_query_request( {
            "requirements": [ {
                "key": "capabilities.r019.keep_watch_meaningful_event_validation",
                "op": "eq",
                "value": "separately_authorized_guarded_stop_at_declared_hostile_sighting",
                "minimum_evidence": "declared",
            }, {
                "key": "runtime.r019.source_binding",
                "op": "eq",
                "value": "r019_keep_watch_meaningful_event_bootstrap_v1:r009-m095",
                "minimum_evidence": "declared",
            } ],
            "preferences": [],
        } )
        stale = RegistryQueryCandidateSnapshot(
            scenario_id="r019-current-source",
            lifecycle_state="quarantined",
            token_eligible=False,
            facts={
                "capabilities.r019.keep_watch_meaningful_event_validation": {
                    "value": "separately_authorized_guarded_stop_at_declared_hostile_sighting",
                    "evidence_state": "stale",
                },
                "runtime.r019.source_binding": {
                    "value": "r019_keep_watch_meaningful_event_bootstrap_v1:r009-m095",
                    "evidence_state": "stale",
                },
            },
            explanation={
                "manifest": {
                    "manifest_id": "r019-current-source",
                    "present": True,
                    "validation": { "status": "valid", "review_required": False },
                },
                "lifecycle": { "state": "quarantined", "reason": "route_stale" },
                "route_evidence": [ {
                    "route_key": "r019-source-route",
                    "evidence_state": "stale",
                    "bindings": [ { "resolution": "stale" } ],
                    "details": { "unresolved_contradiction_ids": [] },
                } ],
            },
        )
        evaluation = RegistryStoredQueryEvaluation(
            candidates=( stale, ),
            evaluation=RegistryQueryEvaluation( candidates=(), ranked_scenario_ids=() ),
        )

        self.assertTrue( _current_stale_bootstrap_candidate( stale ) )
        with mock.patch.object( registry_store, "_repair_query_matches_manifest", return_value=True ), \
                mock.patch.object( registry_store, "_r019_stale_repairable_red_ids", return_value=() ):
            self.assertIsNone(
                _registry_query_repair_action(
                    object(), query_id="r019-query", request=request, evaluation=evaluation,
                )
            )


if __name__ == "__main__":
    unittest.main()
