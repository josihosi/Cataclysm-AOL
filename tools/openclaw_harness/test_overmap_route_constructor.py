#!/usr/bin/env python3
"""Focused controls for the ordinary overmap route constructor preflight."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    apply_screen_text_abort_guard,
    audit_ordinary_overmap_route_constructor,
)


class OrdinaryOvermapRouteConstructorTest(unittest.TestCase):
    def audit(self, lines: list[str]) -> dict[str, object]:
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = Path(temporary)
            log = run_dir / "debug.log"
            log.write_text("\n".join(lines) + "\n", encoding="utf-8")
            return audit_ordinary_overmap_route_constructor(
                run_dir,
                "ordinary_route",
                artifact_log=log,
                artifact_baseline=0,
                origin_omt=[160, 39, 0],
                destination_omt=[162, 36, 0],
            )

    def test_accepts_complete_production_constructor_trace(self) -> None:
        metadata = self.audit([
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=entered position=160,39,0 action=\"\"",
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=position position=162,36,0 action=\"UP\"",
            "openclaw_harness_ui_trace: component=overmap_route_input event=resolved resolved_action=\"CHOOSE_DESTINATION\"",
            "openclaw_harness_ui_trace: component=overmap_route event=constructed destination=162,36,0 path_size=5 path_nonempty=true travel_result=false",
        ])

        self.assertEqual(metadata["status"], "required_state_present")
        self.assertEqual(metadata["origin_omt"], "160,39,0")
        self.assertEqual(metadata["destination_omt"], "162,36,0")

    def test_rejects_route_trace_without_destination_cursor_binding(self) -> None:
        metadata = self.audit([
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=entered position=160,39,0 action=\"\"",
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=position position=162,37,0 action=\"UP\"",
            "openclaw_harness_ui_trace: component=overmap_route_input event=resolved resolved_action=\"CHOOSE_DESTINATION\"",
            "openclaw_harness_ui_trace: component=overmap_route event=constructed destination=162,36,0 path_size=5 path_nonempty=true travel_result=false",
        ])

        self.assertEqual(metadata["status"], "required_state_missing")
        self.assertIn(
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=position && position=162,36,0",
            metadata["missing_required_items"],
        )

    def test_rejects_route_trace_with_wrong_origin(self) -> None:
        metadata = self.audit([
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=entered position=159,39,0 action=\"\"",
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=position position=162,36,0 action=\"UP\"",
            "openclaw_harness_ui_trace: component=overmap_route_input event=resolved resolved_action=\"CHOOSE_DESTINATION\"",
            "openclaw_harness_ui_trace: component=overmap_route event=constructed destination=162,36,0 path_size=5 path_nonempty=true travel_result=false",
        ])

        self.assertEqual(metadata["status"], "required_state_missing")
        self.assertIn(
            "openclaw_harness_ui_trace: component=overmap_route_cursor event=entered && position=160,39,0",
            metadata["missing_required_items"],
        )

    def test_binds_explicit_segment_start_without_avatar_route_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            run_dir = Path(temporary)
            log = run_dir / "debug.log"
            log.write_text("\n".join([
                "openclaw_harness_ui_trace: component=overmap_route_cursor event=entered position=160,39,0 action=\"\"",
                "openclaw_harness_ui_trace: component=overmap_route_cursor event=position position=162,36,0 action=\"UP\"",
                "openclaw_harness_ui_trace: component=overmap_route_input event=resolved resolved_action=\"CHOOSE_DESTINATION\"",
                "openclaw_harness_ui_trace: component=overmap_route event=constructed destination=162,36,0 path_size=4 path_nonempty=true travel_result=false path_omts=\"162,36,0;161,37,0;161,38,0;161,39,0\" native_preview_request=true requested_start=161,39,0 requested_end=162,36,0 actual_first=162,36,0 actual_terminal=161,39,0 world_mutation=false",
            ]) + "\n", encoding="utf-8")
            metadata = audit_ordinary_overmap_route_constructor(
                run_dir,
                "native_segment",
                artifact_log=log,
                artifact_baseline=0,
                origin_omt=[160, 39, 0],
                destination_omt=[162, 36, 0],
                native_preview_segment_start_omt=[161, 39, 0],
                require_native_corridor=True,
                native_preview_receipt_context={
                    "run": "bound-run",
                    "scenario": "bound-scenario",
                    "source": "bound-source",
                    "executable": "bound-executable",
                },
            )

        self.assertEqual(metadata["status"], "required_state_present")
        self.assertEqual(metadata["native_preview_receipt"], {
            "requested_start": [161, 39, 0],
            "actual_first": [162, 36, 0],
            "requested_end": [162, 36, 0],
            "actual_terminal": [161, 39, 0],
            "exact_native_corridor": [[162, 36, 0], [161, 37, 0], [161, 38, 0], [161, 39, 0]],
            "run": "bound-run",
            "scenario": "bound-scenario",
            "source": "bound-source",
            "executable": "bound-executable",
            "world_mutation": False,
            "status": "green",
        })

    def test_rejects_native_segment_receipt_with_mutation_or_wrong_start(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = audit_ordinary_overmap_route_constructor(
                Path(temporary),
                "bad_native_segment",
                artifact_log=None,
                artifact_baseline=0,
                origin_omt=[160, 39, 0],
                destination_omt=[162, 36, 0],
                native_preview_segment_start_omt=[161, 39, 0],
                require_native_corridor=True,
                native_preview_receipt_context={
                    "run": "bound-run",
                    "scenario": "bound-scenario",
                    "source": "bound-source",
                    "executable": "bound-executable",
                },
            )

        self.assertEqual(result["status"], "required_state_missing")
        self.assertEqual(
            result["native_planned_corridor"]["reason"],
            "missing_native_segment_preview_request",
        )

    def test_bandit_route_family_uses_dry_reverse_constructor_contract(self) -> None:
        scenario_path = HARNESS_DIR / "scenarios" / "bandit.scout_to_decision_observer_live_mcw.json"
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        steps = {
            str(step["label"]): step
            for step in [
                *scenario["steps"],
                *scenario.get("post_relaunch", {}).get("steps", []),
            ]
        }

        self.assertEqual(
            steps["preview_ordinary_route_to_persisted_homeward_resume_omt"],
            {
                "kind": "ordinary_overmap_route_constructor",
                "label": "preview_ordinary_route_to_persisted_homeward_resume_omt",
                "origin_omt": [162, 36, 0],
                "destination_omt": [160, 39, 0],
                "cursor_keys": ["left", "left", "down", "down", "down"],
                "route_key": "W",
                "delay_ms": 200,
                "open_settle_seconds": 1.0,
                "cursor_settle_seconds": 0.5,
                "route_settle_seconds": 0.5,
                "abort_on_metadata_failure": True,
                "abort_verdict": "blocked_persisted_homeward_resume_route_preview_missing",
                "abort_reason": "the ordinary route constructor did not enter the overmap at (162,36,0), bind (160,39,0), and construct a nonempty path",
                "expected_visible_fact": "the coherent ordinary route constructor previews travel from (162,36,0) to (160,39,0)",
            },
        )
        self.assertEqual(
            steps["preview_ordinary_route_back_to_observer_footing"]["kind"],
            "ordinary_overmap_route_constructor",
        )
        self.assertEqual(
            steps["preview_reverse_final_native_leg_to_observer_footing"]["origin_omt"], [159, 41, 0],
        )
        self.assertEqual(
            steps["preview_reverse_final_native_leg_to_observer_footing"]["destination_omt"], [159, 40, 0],
        )
        self.assertEqual(
            steps["preview_reverse_first_native_leg_to_observer_footing"]["origin_omt"], [159, 40, 0],
        )
        self.assertEqual(
            steps["preview_reverse_first_native_leg_to_observer_footing"]["destination_omt"], [160, 39, 0],
        )
        self.assertEqual(
            steps["preview_ordinary_route_back_to_observer_footing"]["origin_omt"], [160, 39, 0],
        )
        self.assertEqual(
            steps["preview_ordinary_route_back_to_observer_footing"]["destination_omt"], [161, 38, 0],
        )
        self.assertEqual(
            steps["preview_ordinary_route_to_returned_pair_resume"]["kind"],
            "ordinary_overmap_route_constructor",
        )
        self.assertEqual(
            steps["preview_ordinary_route_to_returned_pair_resume"]["origin_omt"], [163, 33, 0],
        )
        self.assertEqual(
            steps["preview_ordinary_route_to_returned_pair_resume"]["cursor_keys"],
            ["left", "left", "down", "down", "down"],
        )
        self.assertEqual(
            steps["preview_ordinary_route_to_returned_pair_resume"]["destination_omt"], [161, 36, 0],
        )
        self.assertIn("type:c", scenario["runtime_contract"]["permitted_input"])
        self.assertNotIn("finish_first_staged_native_travel_leg_after_stamina_break", steps)
        self.assertNotIn("dismiss_first_staged_harmless_narrative_popup", steps)
        self.assertNotIn("observe_first_staged_catching_breath_completion", steps)
        self.assertNotIn(".", scenario["runtime_contract"]["permitted_input"])
        self.assertNotIn("5", scenario["runtime_contract"]["permitted_input"])
        self.assertEqual(
            steps["preview_first_resumed_native_leg_to_persisted_homeward_resume_omt"]["origin_omt"],
            [160, 39, 0],
        )
        self.assertEqual(
            steps["preview_final_native_leg_to_persisted_homeward_resume_omt"]["destination_omt"],
            [159, 41, 0],
        )
        for label in (
            "staged_persisted_homeward_resume_omt_returns_to_hud",
            "original_observer_footing_restored_after_staging",
            "selected_watch_loaded_for_pair_handoff",
        ):
            self.assertEqual(
                steps[label]["abort_if_text_contains"],
                ["Diving will destroy", "Will be destroyed:"],
            )

    def test_post_incident_return_chain_uses_saved_watch_and_claim_identity(self) -> None:
        scenario_path = HARNESS_DIR / "scenarios" / "bandit.scout_to_decision_observer_live_mcw.json"
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        post_relaunch = scenario["post_relaunch"]["steps"]
        steps = {str(step["label"]): step for step in post_relaunch}

        self.assertEqual(steps["select_authoritative_dispatch"]["keys"], ["["])
        self.assertEqual(
            steps["preview_ordinary_route_to_returned_pair_resume"],
            {
                "kind": "ordinary_overmap_route_constructor",
                "label": "preview_ordinary_route_to_returned_pair_resume",
                "origin_omt": [163, 33, 0],
                "destination_omt": [161, 36, 0],
                "cursor_keys": ["left", "left", "down", "down", "down"],
                "route_key": "W",
                "delay_ms": 200,
                "open_settle_seconds": 1.0,
                "cursor_settle_seconds": 0.5,
                "route_settle_seconds": 0.5,
                "abort_on_metadata_failure": True,
                "abort_verdict": "blocked_returned_pair_resume_route_preview_missing",
                "abort_reason": "the ordinary route constructor did not enter the overmap at (163,33,0), bind the retained pair's persisted resume OMT at (161,36,0), and construct a nonempty path",
                "expected_visible_fact": "the coherent ordinary route constructor previews travel to the retained pair's exact persisted resume OMT at (161,36,0)",
            },
        )
        self.assertEqual(steps["confirm_ordinary_route_to_returned_pair_resume"]["text"], "W")
        self.assertEqual(steps["accept_ordinary_route_to_returned_pair_resume"]["text"], "Y")
        self.assertEqual(
            steps["accept_ordinary_route_to_returned_pair_resume"]["native_travel_stabilization"],
            {"mode": "continue_exact_hostile_auto_move_until_hud"},
        )
        self.assertEqual(
            steps["audit_next_hour_consumes_exact_report_into_claimed_follow_on"]["required_line_patterns"],
            [["bandit_live_world structural maintenance:", "response_operations_applied=1"]],
        )
        saved_audit = steps["audit_saved_survivors_home_and_outing_closed"]
        self.assertEqual(saved_audit["required_camp_decision_state"], "preparing_follow_on")
        self.assertEqual(saved_audit["required_active_hostile_operation_kind"], "shakedown")
        self.assertEqual(saved_audit["required_active_hostile_operation_phase"], "assembling")
        self.assertEqual(saved_audit["required_active_hostile_reservation_job_type"], "toll")
        self.assertTrue(saved_audit["required_report_decision_identity_match"])
        self.assertTrue(saved_audit["required_report_hostile_operation_claim_match"])

    def test_route_water_modal_aborts_even_when_hud_text_is_visible(self) -> None:
        step = {
            "abort_if_text_contains": ["Diving will destroy", "Will be destroyed:"],
            "abort_verdict": "blocked_original_observer_footing_not_restored_or_route_water_modal",
            "abort_reason": "ordinary return did not restore unobstructed gameplay",
        }
        with tempfile.TemporaryDirectory() as temporary:
            temporary_path = Path(temporary)
            modal_path = temporary_path / "modal.json"
            modal_path.write_text(json.dumps({
                "lines": ["Move:", "Wield:", "Diving will destroy the following items.", "Will be destroyed:"],
            }), encoding="utf-8")
            modal_report: dict[str, object] = {}
            self.assertTrue(apply_screen_text_abort_guard(
                modal_report, step, {"json_path": str(modal_path)}
            ))
            self.assertEqual(
                modal_report["abort"]["verdict"],
                "blocked_original_observer_footing_not_restored_or_route_water_modal",
            )

            dry_path = temporary_path / "dry.json"
            dry_path.write_text(json.dumps({
                "lines": ["Move:", "Wield:", "You have reached your destination."],
            }), encoding="utf-8")
            dry_report: dict[str, object] = {}
            self.assertFalse(apply_screen_text_abort_guard(
                dry_report, step, {"json_path": str(dry_path)}
            ))
            self.assertEqual(dry_report, {})


if __name__ == "__main__":
    unittest.main()
