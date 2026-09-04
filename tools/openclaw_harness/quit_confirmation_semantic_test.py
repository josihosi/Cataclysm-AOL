"""Focused tests for the authoritative main-menu quit modal gate."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from startup_harness import (
    classify_ordinary_wait_interruption,
    evaluate_semantic_ui_expectation,
)


class QuitConfirmationSemanticTest(unittest.TestCase):
    HUD_SAVE_AND_QUIT = {"ok": True, "text": "Actions\nS Save and quit\nMove: 100"}

    def test_main_menu_semantic_quit_releases_the_withheld_owner_before_its_confirmation(self) -> None:
        source = (Path(__file__).resolve().parents[2] / "src" / "main_menu.cpp").read_text(
            encoding="utf-8"
        )
        callback_start = source.index('}, [semantic_manager, &semantic_action]')
        callback_end = source.index('} );\n            semantic_scope->consume_request();', callback_start)
        callback = source[callback_start:callback_end]

        self.assertIn('semantic_action = "QUIT";', callback)
        self.assertIn('withhold_parent_authority_until_recreated', callback)
        self.assertNotIn('queue_native_intent', callback)
        self.assertNotIn('take_native_intent', source)
        release = source.index('if( semantic_action_consumed ) {')
        action = source.index('std::string action = semantic_action_consumed ? semantic_action')
        self.assertIn('semantic_scope.reset();', source[release:action])
        self.assertLess(release, action)

    def test_m095_uses_semantic_quit_gate_and_keeps_normal_relaunch_route(self) -> None:
        scenario_path = Path(__file__).resolve().parent / "scenarios" / (
            "bandit.r009_m095_current_route_safe_watch_mcw.json"
        )
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        steps = {str(step["label"]): step for step in scenario["steps"]}
        quit_step = steps["open_native_main_menu_quit_confirmation"]
        self.assertNotIn("expected_screen_text_after_contains", quit_step)
        self.assertTrue(quit_step["abort_on_semantic_ui_failure"])
        self.assertEqual(
            quit_step["semantic_ui_expectation"],
            {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
        )
        self.assertEqual(
            scenario["post_relaunch"]["terminal_save_step_label"],
            "confirm_native_process_exit_after_route_arrival",
        )

    def test_r005_moves_persistence_audit_behind_bound_main_menu_exit(self) -> None:
        scenario_path = Path(__file__).resolve().parent / "scenarios" / (
            "bandit.r005_continuous_hostile_ecology_certification.json"
        )
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        self.assertEqual(
            scenario["runtime_contract"]["permitted_input"][-3:],
            ["press:q", "press:left", "press:enter"],
        )
        initial_steps = {str(step["label"]): step for step in scenario["steps"]}
        self.assertEqual(
            initial_steps["open_native_main_menu_quit_confirmation"]["keys"],
            ["q"],
        )
        self.assertEqual(
            initial_steps["open_native_main_menu_quit_confirmation"]["semantic_ui_expectation"],
            {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
        )
        self.assertEqual(
            initial_steps["open_native_main_menu_quit_confirmation"]["checkpoint_evidence"],
            {"classification": "incidental_lifecycle_observation", "authoritative_owner": "bound_semantic_ui"},
        )
        self.assertTrue(initial_steps["open_native_main_menu_quit_confirmation"]["abort_on_semantic_ui_failure"])
        self.assertEqual(
            initial_steps["confirm_native_process_exit_after_stationary_lifecycle"]["keys"],
            ["left", "enter"],
        )
        self.assertEqual(
            initial_steps["confirm_native_process_exit_after_stationary_lifecycle"]["checkpoint_evidence"],
            {"classification": "incidental_lifecycle_observation", "authoritative_owner": "post_relaunch_lifecycle"},
        )
        self.assertEqual(
            scenario["post_relaunch"]["terminal_save_step_label"],
            "confirm_native_process_exit_after_stationary_lifecycle",
        )
        post_labels = [str(step["label"]) for step in scenario["post_relaunch"]["steps"]]
        self.assertEqual(post_labels[0], "post_relaunch_gameplay_hud")
        self.assertIn("audit_normalized_persistence_after_relaunch", post_labels)
        self.assertNotIn("audit_normalized_persistence_after_relaunch", initial_steps)

    def write_trace(self, root: Path, body: str) -> Path:
        path = root / "feature.debug.log"
        path.write_text(body, encoding="utf-8")
        return path

    @staticmethod
    def quit_trace(event: str = "open", run_id: str = "current-run") -> str:
        return (
            "openclaw_harness_ui_trace: component=semantic_ui "
            f'event={event} instance_id="main-menu-quit-7" '
            f'run_id="{run_id}" '
            'intent="main_menu_quit_confirmation" '
            'valid_actions=["left","enter"] '
            'postcondition="quit_confirmation_resolved"\n'
        )

    def classify_ordinary_wait(self, trace_body: str, start_offset: int = 0) -> dict:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(Path(temp), trace_body)
            return classify_ordinary_wait_interruption(
                self.HUD_SAVE_AND_QUIT,
                semantic_ui_trace_log=trace,
                semantic_ui_trace_start_offset=start_offset,
            )

    def test_persistent_hud_save_and_quit_text_is_not_a_confirmation(self) -> None:
        result = self.classify_ordinary_wait("")

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "inactive_save_and_quit_hud_text")

    def test_active_current_run_quit_modal_remains_unsafe(self) -> None:
        result = self.classify_ordinary_wait(self.quit_trace())

        self.assertEqual(result["status"], "unsafe_prompt")
        self.assertEqual(result["provenance"], "current_run_semantic_ui_trace")
        self.assertEqual(result["semantic_ui_trace"]["intent"], "main_menu_quit_confirmation")

    def test_stale_modal_state_before_the_current_run_is_ignored(self) -> None:
        stale = self.quit_trace()
        result = self.classify_ordinary_wait(stale, len(stale.encode("utf-8")))

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "inactive_save_and_quit_hud_text")

    def test_closed_current_run_modal_is_not_treated_as_active(self) -> None:
        result = self.classify_ordinary_wait(self.quit_trace() + self.quit_trace("return"))

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["classification"], "inactive_save_and_quit_hud_text")

    def test_wrong_run_modal_cannot_block_the_current_wait(self) -> None:
        previous_run = self.quit_trace()
        current_run_offset = len(previous_run.encode("utf-8"))
        result = self.classify_ordinary_wait(previous_run, current_run_offset)

        self.assertEqual(result["status"], "clear")
        self.assertEqual(result["semantic_ui_trace_start_offset"], current_run_offset)

    def test_conflicting_current_run_overlay_remains_fail_closed(self) -> None:
        conflicting = (
            "openclaw_harness_ui_trace: component=semantic_ui "
            'event=open instance_id="other-modal-1" intent="eoc_popup" '
            'valid_actions=["space"] postcondition="popup_resolved"\n'
        )
        result = self.classify_ordinary_wait(conflicting)

        self.assertEqual(result["status"], "unknown_prompt")
        self.assertEqual(
            result["classification"],
            "save_and_quit_text_with_conflicting_active_semantic_modal",
        )

    def test_active_quit_modal_is_proven_without_screen_text(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(
                Path(temp),
                'openclaw_harness_ui_trace: component=semantic_ui '
                'event=open instance_id="main-menu-quit-7" '
                'run_id="current-run" '
                'intent="main_menu_quit_confirmation" '
                'valid_actions=["left","enter"] '
                'postcondition="quit_confirmation_resolved"\n',
            )
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "green")
        self.assertEqual(result["provenance"], "semantic_ui_trace")
        self.assertEqual(result["observed"]["instance_id"], "main-menu-quit-7")

    def test_current_tail_bound_quit_modal_is_proven(self) -> None:
        """A retained current-run open is live unless its return is also retained."""
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(Path(temp), "x" * 300000 + self.quit_trace())
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "green")
        self.assertTrue(result["tail_bound_active"])

    def test_tail_bound_wrong_run_and_closed_modal_remain_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(Path(temp), "x" * 300000 + self.quit_trace("open", "other-run"))
            wrong_run = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
            self.assertEqual(wrong_run["status"], "blocked")

            trace = self.write_trace(Path(temp), "x" * 300000 + self.quit_trace() + self.quit_trace("return"))
            closed = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
            self.assertEqual(closed["status"], "blocked")

    def test_missing_or_closed_quit_modal_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(
                Path(temp),
                'openclaw_harness_ui_trace: component=semantic_ui '
                'event=open instance_id="main-menu-quit-7" '
                'run_id="current-run" '
                'intent="main_menu_quit_confirmation" '
                'valid_actions=["left","enter"] '
                'postcondition="quit_confirmation_resolved"\n'
                'openclaw_harness_ui_trace: component=semantic_ui '
                'event=return instance_id="main-menu-quit-7" '
                'run_id="current-run" '
                'intent="main_menu_quit_confirmation" '
                'valid_actions=["left","enter"] '
                'postcondition="quit_confirmation_resolved"\n',
            )
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["verdict"], "blocked_semantic_ui_expectation_missing")

    def test_contradictory_ocr_is_not_a_semantic_input(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(
                Path(temp),
                'screenshot OCR says Really quit?\n'
                'openclaw_harness_ui_trace: component=semantic_ui '
                'event=open instance_id="other-modal-1" run_id="current-run" intent="unrelated" '
                'valid_actions=["space"] postcondition="other_modal_resolved"\n',
            )
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["issues"], ["wrong_intent", "advertised_input_missing"])

    def test_wrong_run_or_missing_run_cannot_prove_the_confirmation(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(Path(temp), self.quit_trace(run_id="previous-run"))
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["verdict"], "blocked_semantic_ui_expectation_missing")

        with tempfile.TemporaryDirectory() as temp:
            trace = self.write_trace(
                Path(temp),
                'openclaw_harness_ui_trace: component=semantic_ui '
                'event=open instance_id="main-menu-quit-7" '
                'intent="main_menu_quit_confirmation" '
                'valid_actions=["left","enter"] '
                'postcondition="quit_confirmation_resolved"\n',
            )
            result = evaluate_semantic_ui_expectation(
                trace, 0,
                {"intent": "main_menu_quit_confirmation", "valid_actions": ["left", "enter"]},
                run_id="current-run",
            )
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["expected_run_id"], "current-run")

    def test_native_quit_trace_emits_the_harness_run_id(self) -> None:
        popup_source = (Path(__file__).resolve().parents[2] / "src" / "popup.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('std::getenv( "OPENCLAW_HARNESS_RUN_ID" )', popup_source)
        self.assertIn('<< " run_id=\\\"" << ( run_id ? run_id : "" ) << "\\\""', popup_source)


if __name__ == "__main__":
    unittest.main()
