#!/usr/bin/env python3
"""Focused controls for R-005's zero-credit native-corridor gate."""

from __future__ import annotations

import unittest
from pathlib import Path
import sys
import json
import tempfile

sys.path.insert( 0, str( Path( __file__ ).resolve().parent ) )
from r005_corridor_observation import (
    compose_r005_native_waypoint_segments,
    derive_r005_native_waypoint_segments_from_previews,
    plan_r005_native_waypoint_segments,
    parse_native_planned_corridor,
    select_r005_corridor_candidate,
)
from semantic_state import decide_native_travel_boundary, read_semantic_step_trace


class R005CorridorObservationTest( unittest.TestCase ):
    binding_id = "source-and-executable-binding"
    start_omt = [145, 40, 0]
    destination_omt = [145, 42, 0]

    def failed( self, report_id: str, path: list[list[int]], boundary: list[int] ) -> dict[str, object]:
        return {
            "report_id": report_id,
            "binding_id": self.binding_id,
            "receipt_sha256": f"receipt-{report_id}",
            "start_omt": self.start_omt,
            "destination_omt": self.destination_omt,
            "planned_corridor": path,
            "first_hostile_boundary": {"omt": boundary, "prompt_response": "none"},
            "ingestion": "red_ingested",
            "cleanup": "accepted",
        }

    def candidate( self ) -> dict[str, object]:
        return {
            "candidate_id": "distinct-corridor",
            "binding_id": self.binding_id,
            "receipt_sha256": "candidate-receipt",
            "start_omt": self.start_omt,
            "destination_omt": self.destination_omt,
            "planned_corridor": [[145, 42, 0], [145, 41, 0], [145, 40, 0]],
        }

    def failures( self ) -> list[dict[str, object]]:
        return [
            self.failed( "2336e26446d3", [[145, 42, 0], [142, 35, 0], [145, 40, 0]], [142, 35, 0] ),
            self.failed( "a3b66c9b1746", [[145, 42, 0], [141, 32, 0], [145, 40, 0]], [141, 32, 0] ),
            self.failed( "69963bfcf53c", [[145, 42, 0], [144, 34, 0], [144, 35, 0], [145, 40, 0]], [144, 35, 0] ),
        ]

    def direct_failure( self ) -> dict[str, object]:
        return self.failed(
            "063de674d777",
            [
                [140, 31, 0], [140, 32, 0], [140, 33, 0], [140, 34, 0],
                [140, 35, 0], [140, 36, 0], [140, 37, 0], [140, 38, 0],
                [140, 39, 0], [140, 40, 0], [140, 41, 0],
            ],
            [140, 34, 0],
        )

    def native_segment(
        self, receipt_sha256: str, path: list[list[int]], start: list[int], destination: list[int], *,
        binding_id: str | None = None,
    ) -> dict[str, object]:
        return {
            "binding_id": binding_id or self.binding_id,
            "receipt_sha256": receipt_sha256,
            "planned_corridor": path,
            "native_preview_receipt": {
                "requested_start": start,
                "actual_first": path[0],
                "requested_end": destination,
                "actual_terminal": path[-1],
                "exact_native_corridor": path,
                "run": "bound-run",
                "scenario": "bandit.r005_native_waypoint_observation",
                "source": "bound-source",
                "executable": "bound-executable",
                "world_mutation": False,
            },
        }

    def test_records_the_actual_native_constructor_corridor( self ) -> None:
        result = parse_native_planned_corridor( [
            "INFO : openclaw_harness_ui_trace: component=overmap_route event=constructed "
            "destination=145,40,0 path_size=3 path_nonempty=true travel_result=false "
            "path_omts=\"145,42,0;145,41,0;145,40,0\"",
        ], [145, 40, 0] )

        self.assertEqual( result["status"], "green" )
        self.assertEqual( result["planned_corridor"], self.candidate()["planned_corridor"] )
        self.assertEqual( result["source"], "native_overmap_route_constructor" )

    def test_distinct_corridor_is_only_zero_credit_selection( self ) -> None:
        result = select_r005_corridor_candidate(
            self.candidate(), self.failures(), current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )

        self.assertEqual( result["status"], "selected_zero_credit_only" )
        self.assertEqual( result["compared_reports"], [
            "2336e26446d3", "a3b66c9b1746", "69963bfcf53c",
        ] )
        self.assertEqual( result["qualification_authority"], "not_started" )

    def test_missing_stale_overlapping_and_unbound_evidence_fail_closed( self ) -> None:
        missing = select_r005_corridor_candidate(
            self.candidate(), [{}], current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        stale_failures = self.failures()
        stale_failures[0]["binding_id"] = "old-binding"
        stale = select_r005_corridor_candidate(
            self.candidate(), stale_failures, current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        overlap_candidate = self.candidate()
        overlap_candidate["planned_corridor"] = [[145, 42, 0], [144, 34, 0], [145, 40, 0]]
        overlap = select_r005_corridor_candidate(
            overlap_candidate, self.failures(), current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        unbound_failures = self.failures()
        unbound_failures[1]["receipt_sha256"] = ""
        unbound = select_r005_corridor_candidate(
            self.candidate(), unbound_failures, current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )

        self.assertEqual( missing["reason"], "missing_failed_corridor_evidence" )
        self.assertEqual( stale["reason"], "stale_failed_corridor_evidence" )
        self.assertEqual( overlap["reason"], "candidate_corridor_overlaps_preserved_failure" )
        self.assertEqual( unbound["reason"], "unbound_failed_corridor_evidence" )

    def test_first_hostile_boundary_must_belong_to_its_active_corridor( self ) -> None:
        failures = self.failures()
        failures[2]["first_hostile_boundary"] = {"omt": [999, 999, 0], "prompt_response": "none"}

        result = select_r005_corridor_candidate(
            self.candidate(), failures, current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )

        self.assertEqual( result["reason"], "hostile_boundary_not_on_corridor" )

    def test_fourth_direct_boundary_is_compared_and_required_endpoints_are_enforced( self ) -> None:
        direct = self.direct_failure()
        direct["start_omt"] = [140, 41, 0]
        direct["destination_omt"] = [140, 31, 0]
        result = select_r005_corridor_candidate(
            self.candidate(), self.failures(),
            current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        self.assertEqual( result["status"], "selected_zero_credit_only" )

        wrong_destination = self.candidate()
        wrong_destination["destination_omt"] = [145, 40, 0]
        rejected = select_r005_corridor_candidate(
            wrong_destination, self.failures(),
            current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        self.assertEqual( rejected["reason"], "candidate_corridor_not_bound_to_required_endpoints" )

    def test_endpoint_only_overlap_is_selected_when_both_corridors_are_bound( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        direct = self.direct_failure()
        direct["start_omt"] = start
        direct["destination_omt"] = destination
        selected = select_r005_corridor_candidate(
            {
                "candidate_id": "endpoint-only",
                "binding_id": self.binding_id,
                "receipt_sha256": "endpoint-only-receipt",
                "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [139, 32, 0], [139, 40, 0], start],
            },
            [direct],
            current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        self.assertEqual( selected["status"], "selected_zero_credit_only" )

    def test_four_current_bound_baselines_allow_only_endpoint_overlap( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        baselines = [
            self.failed( "x142", [destination, [142, 35, 0], start], [142, 35, 0] ),
            self.failed( "x143", [destination, [143, 35, 0], start], [143, 35, 0] ),
            self.failed( "x144", [destination, [144, 35, 0], start], [144, 35, 0] ),
            self.failed( "063de674d777", [destination, [140, 34, 0], start], [140, 34, 0] ),
        ]
        for baseline in baselines:
            baseline["start_omt"] = start
            baseline["destination_omt"] = destination
        selected = select_r005_corridor_candidate(
            {
                "candidate_id": "fresh-northern-flank", "binding_id": self.binding_id,
                "receipt_sha256": "selection-receipt", "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [139, 32, 0], [139, 40, 0], start],
            }, baselines, current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        self.assertEqual( selected["status"], "selected_zero_credit_only" )
        self.assertEqual( selected["compared_reports"], ["x142", "x143", "x144", "063de674d777"] )

    def test_fifth_qualification_leg_baseline_rejects_its_hostile_endpoint( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        baselines = [
            self.failed( "x142", [destination, [142, 35, 0], start], [142, 35, 0] ),
            self.failed( "x143", [destination, [143, 35, 0], start], [143, 35, 0] ),
            self.failed( "x144", [destination, [144, 35, 0], start], [144, 35, 0] ),
            self.failed( "direct", [destination, [140, 34, 0], start], [140, 34, 0] ),
            {
                "report_id": "y29-return-terminal",
                "binding_id": self.binding_id,
                "receipt_sha256": "y29-return-receipt",
                "corridor_scope": "qualification_leg",
                "leg_start_omt": [144, 29, 0],
                "leg_destination_omt": [140, 29, 0],
                "planned_corridor": [
                    [140, 29, 0], [141, 29, 0], [142, 29, 0], [143, 29, 0], [144, 29, 0],
                ],
                "first_hostile_boundary": {"omt": [144, 29, 0], "prompt_response": "none"},
                "ingestion": "red_ingested",
                "cleanup": "accepted",
            },
        ]
        for baseline in baselines[:4]:
            baseline["start_omt"] = start
            baseline["destination_omt"] = destination
        rejected = select_r005_corridor_candidate(
            {
                "candidate_id": "reuses-y29-endpoint", "binding_id": self.binding_id,
                "receipt_sha256": "selection-receipt", "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [144, 29, 0], start],
            }, baselines, current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        selected = select_r005_corridor_candidate(
            {
                "candidate_id": "fresh-northern-flank", "binding_id": self.binding_id,
                "receipt_sha256": "selection-receipt", "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [139, 32, 0], [139, 40, 0], start],
            }, baselines, current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )

        self.assertEqual( rejected["reason"], "candidate_corridor_overlaps_hostile_boundary" )
        self.assertEqual( rejected["report_id"], "y29-return-terminal" )
        self.assertEqual( selected["status"], "selected_zero_credit_only" )
        self.assertEqual( selected["compared_reports"], ["x142", "x143", "x144", "direct", "y29-return-terminal"] )

    def test_hostile_boundary_and_interior_overlap_remain_rejected( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        direct = self.direct_failure()
        direct["start_omt"] = start
        direct["destination_omt"] = destination
        boundary = select_r005_corridor_candidate(
            {
                "candidate_id": "touches-boundary", "binding_id": self.binding_id,
                "receipt_sha256": "candidate-receipt", "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [140, 34, 0], start],
            }, [direct], current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        interior = select_r005_corridor_candidate(
            {
                "candidate_id": "shares-interior", "binding_id": self.binding_id,
                "receipt_sha256": "candidate-receipt", "start_omt": start,
                "destination_omt": destination,
                "planned_corridor": [destination, [140, 33, 0], start],
            }, [direct], current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        self.assertEqual( boundary["reason"], "candidate_corridor_overlaps_hostile_boundary" )
        self.assertEqual( interior["reason"], "candidate_corridor_overlaps_preserved_failure" )

    def test_five_bound_failures_derive_native_waypoint_segments_before_selection( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        baselines = [
            self.failed( "x142", [[142, 30, 0], [142, 31, 0], [142, 42, 0]], [142, 31, 0] ),
            self.failed( "x143", [[143, 30, 0], [142, 32, 0], [143, 42, 0]], [142, 32, 0] ),
            self.failed( "x144", [[144, 29, 0], [142, 32, 0], [144, 42, 0]], [142, 32, 0] ),
            self.failed( "direct", [destination, [140, 34, 0], start], [140, 34, 0] ),
            self.failed( "y29", [[140, 29, 0], [144, 29, 0], [145, 29, 0]], [144, 29, 0] ),
        ]
        for baseline in baselines[:3] + baselines[4:]:
            baseline["corridor_scope"] = "qualification_leg"
            baseline["leg_destination_omt"] = baseline["planned_corridor"][0]
            baseline["leg_start_omt"] = baseline["planned_corridor"][-1]
        baselines[3]["start_omt"] = start
        baselines[3]["destination_omt"] = destination
        plan = plan_r005_native_waypoint_segments(
            baselines, current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )

        self.assertEqual( plan["status"], "planned_zero_credit_only" )
        self.assertEqual( plan["waypoints"], [start, [139, 41, 0], [139, 31, 0], destination] )
        self.assertFalse( plan["movement_dispatched"] )
        self.assertEqual( len(plan["hostile_boundaries"]), 5 )

        observed = [
            self.native_segment( "one", [start, [139, 41, 0]], start, [139, 41, 0] ),
            self.native_segment( "two", [[139, 41, 0], [139, 31, 0]], [139, 41, 0], [139, 31, 0] ),
            self.native_segment( "three", [[139, 31, 0], destination], [139, 31, 0], destination ),
        ]
        composed = compose_r005_native_waypoint_segments(
            plan, observed, baselines, current_binding_id=self.binding_id,
        )

        self.assertEqual( composed["status"], "selected_zero_credit_only" )
        self.assertEqual( composed["native_corridor"], [start, [139, 41, 0], [139, 31, 0], destination] )
        self.assertFalse( composed["movement_dispatched"] )
        self.assertEqual( composed["selection"]["compared_reports"], ["x142", "x143", "x144", "direct", "y29"] )

    def test_avatar_origin_previews_cannot_be_sliced_into_native_segments( self ) -> None:
        plan = {
            "status": "planned_zero_credit_only", "current_binding_id": self.binding_id,
            "waypoints": [[140, 41, 0], [139, 41, 0], [139, 31, 0], [140, 31, 0]],
        }
        derived = derive_r005_native_waypoint_segments_from_previews(
            plan,
            [
                {"binding_id": self.binding_id, "receipt_sha256": "one", "planned_corridor": [[139, 41, 0], [140, 41, 0]]},
                {"binding_id": self.binding_id, "receipt_sha256": "two", "planned_corridor": [[139, 31, 0], [139, 41, 0], [140, 41, 0]]},
                {"binding_id": self.binding_id, "receipt_sha256": "three", "planned_corridor": [[140, 31, 0], [139, 31, 0], [139, 41, 0], [140, 41, 0]]},
            ], current_binding_id=self.binding_id,
        )

        self.assertEqual( derived["reason"], "avatar_origin_preview_cannot_bind_segment_start" )

    def test_waypoint_planner_fails_closed_for_missing_evidence_and_native_segment_divergence( self ) -> None:
        start = [140, 41, 0]
        destination = [140, 31, 0]
        baselines = [
            self.failed( "x142", [[142, 30, 0], [142, 31, 0], [142, 42, 0]], [142, 31, 0] ),
            self.failed( "x143", [[143, 30, 0], [142, 32, 0], [143, 42, 0]], [142, 32, 0] ),
            self.failed( "x144", [[144, 29, 0], [142, 32, 0], [144, 42, 0]], [142, 32, 0] ),
            self.failed( "direct", [destination, [140, 34, 0], start], [140, 34, 0] ),
            self.failed( "y29", [[140, 29, 0], [144, 29, 0], [145, 29, 0]], [144, 29, 0] ),
        ]
        for baseline in baselines[:3] + baselines[4:]:
            baseline["corridor_scope"] = "qualification_leg"
            baseline["leg_destination_omt"] = baseline["planned_corridor"][0]
            baseline["leg_start_omt"] = baseline["planned_corridor"][-1]
        baselines[3]["start_omt"] = start
        baselines[3]["destination_omt"] = destination
        missing = plan_r005_native_waypoint_segments(
            baselines[:-1], current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        plan = plan_r005_native_waypoint_segments(
            baselines, current_binding_id=self.binding_id,
            expected_start_omt=start, expected_destination_omt=destination,
        )
        hostile = compose_r005_native_waypoint_segments(
            plan,
            [
                self.native_segment( "one", [start, [140, 34, 0], [139, 41, 0]], start, [139, 41, 0] ),
                self.native_segment( "two", [[139, 41, 0], [139, 31, 0]], [139, 41, 0], [139, 31, 0] ),
                self.native_segment( "three", [[139, 31, 0], destination], [139, 31, 0], destination ),
            ], baselines, current_binding_id=self.binding_id,
        )
        stale = compose_r005_native_waypoint_segments(
            plan,
            [
                self.native_segment( "one", [start, [139, 41, 0]], start, [139, 41, 0], binding_id="stale" ),
                self.native_segment( "two", [[139, 41, 0], [139, 31, 0]], [139, 41, 0], [139, 31, 0] ),
                self.native_segment( "three", [[139, 31, 0], destination], [139, 31, 0], destination ),
            ], baselines, current_binding_id=self.binding_id,
        )
        wrong_waypoint = compose_r005_native_waypoint_segments(
            plan,
            [
                self.native_segment( "one", [start, [139, 40, 0]], start, [139, 41, 0] ),
                self.native_segment( "two", [[139, 41, 0], [139, 31, 0]], [139, 41, 0], [139, 31, 0] ),
                self.native_segment( "three", [[139, 31, 0], destination], [139, 31, 0], destination ),
            ], baselines, current_binding_id=self.binding_id,
        )
        mutating_receipt = self.native_segment(
            "one", [start, [139, 41, 0]], start, [139, 41, 0],
        )
        mutating_receipt["native_preview_receipt"]["world_mutation"] = True
        mutation = compose_r005_native_waypoint_segments(
            plan,
            [
                mutating_receipt,
                self.native_segment( "two", [[139, 41, 0], [139, 31, 0]], [139, 41, 0], [139, 31, 0] ),
                self.native_segment( "three", [[139, 31, 0], destination], [139, 31, 0], destination ),
            ], baselines, current_binding_id=self.binding_id,
        )

        self.assertEqual( missing["reason"], "five_current_bound_hostile_boundaries_required" )
        self.assertEqual( hostile["reason"], "native_segment_contains_prohibited_hostile_cell" )
        self.assertEqual( stale["reason"], "native_segment_not_currently_bound" )
        self.assertEqual( wrong_waypoint["reason"], "native_segment_did_not_reach_declared_waypoint" )
        self.assertEqual( mutation["reason"], "native_segment_receipt_not_bound" )

    def test_failed_endpoint_mismatch_and_missing_binding_fail_closed( self ) -> None:
        failure = self.failures()[0]
        failure.pop( "start_omt" )
        missing = select_r005_corridor_candidate(
            self.candidate(), [failure], current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        mismatch = self.failures()[0]
        mismatch["destination_omt"] = [145, 41, 0]
        unbound_candidate = self.candidate()
        unbound_candidate["receipt_sha256"] = ""
        rejected = select_r005_corridor_candidate(
            self.candidate(), [mismatch], current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        unbound = select_r005_corridor_candidate(
            unbound_candidate, self.failures(), current_binding_id=self.binding_id,
            expected_start_omt=self.start_omt, expected_destination_omt=self.destination_omt,
        )
        self.assertEqual( missing["reason"], "failed_corridor_not_bound_to_required_endpoints" )
        self.assertEqual( rejected["reason"], "failed_corridor_not_bound_to_required_endpoints" )
        self.assertEqual( unbound["reason"], "unbound_candidate_corridor" )

    def test_first_hostile_prompt_is_bound_to_the_active_native_corridor( self ) -> None:
        run_id = "bound-run"
        travel_id = "bound-run:travel:1"
        active = {
            "event": "travel", "run_id": run_id, "travel_id": travel_id,
            "receipt_id": f"{travel_id}:active", "state": "active",
            "destination": [145, 40, 0], "avatar_omt": [145, 42, 0],
            "remaining_omt_path": 3, "destination_present": True,
            "destination_cleared": False, "observed_turn": 1,
            "planned_omt_path": [[145, 42, 0], [145, 41, 0], [145, 40, 0]],
        }
        boundary = {
            **active, "receipt_id": f"{travel_id}:hostile_boundary",
            "state": "hostile_boundary", "avatar_omt": [145, 41, 0],
        }
        with tempfile.TemporaryDirectory() as temporary:
            trace = Path( temporary ) / "semantic.native.log"
            trace.write_text(
                "\n".join(
                    "openclaw_harness_semantic_step: " + json.dumps( event )
                    for event in (active, boundary)
                ) + "\n", encoding="utf-8",
            )
            events, status = read_semantic_step_trace( trace, Path( temporary ), run_id )

        result = decide_native_travel_boundary(
            events, run_id=run_id, expected_destination=[145, 40, 0],
        )
        self.assertEqual( status, "ok" )
        self.assertEqual( result["reason"], "hostile_boundary" )
        self.assertEqual( result["hostile_boundary_omt"], [145, 41, 0] )


if __name__ == "__main__":
    unittest.main()
