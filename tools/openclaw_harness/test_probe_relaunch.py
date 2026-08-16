#!/usr/bin/env python3
"""Focused lifecycle controls for the canonical post-save probe relaunch."""

from __future__ import annotations

import sys
import tempfile
import unittest
from contextlib import ExitStack
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness as harness  # noqa: E402


class ProbeRelaunchTest(unittest.TestCase):
    def test_post_relaunch_contract_requires_terminal_label_exit_bound_and_steps(self) -> None:
        initial = [{"label": "save_and_exit", "kind": "press"}]
        with self.assertRaisesRegex(SystemExit, "terminal_save_step_label"):
            harness.normalize_post_relaunch_contract({}, initial)
        with self.assertRaisesRegex(SystemExit, "must name an initial"):
            harness.normalize_post_relaunch_contract({
                "terminal_save_step_label": "missing",
                "terminal_exit_timeout_seconds": 1,
                "steps": [{"kind": "wait", "seconds": 1}],
            }, initial)
        with self.assertRaisesRegex(SystemExit, "positive number"):
            harness.normalize_post_relaunch_contract({
                "terminal_save_step_label": "save_and_exit",
                "terminal_exit_timeout_seconds": 0,
                "steps": [{"kind": "wait", "seconds": 1}],
            }, initial)

    def test_relaunch_uses_canonical_start_same_world_new_pid_and_focus(self) -> None:
        start_result = {
            "ok": True,
            "pid": 202,
            "run_dir": "/tmp/relaunch-run",
            "focus": {"ok": True},
            "proof_classification": {"startup_clean_for_feature_steps": True},
        }
        with mock.patch.object(harness, "wait_for_pid_exit", return_value=True), \
                mock.patch.object(harness, "run_json_command", return_value=(0, start_result, "out", "err")) as run:
            result = harness.run_probe_post_relaunch(
                initial_pid=101,
                profile="probe-profile",
                config_profile="dev-harness",
                world="McWilliams",
                scenario_name="test.relaunch",
                registry_launch_receipt="receipt",
                terminal_exit_timeout_seconds=2.5,
            )

        self.assertEqual(result["status"], "ready")
        self.assertEqual(result["pid"], 202)
        command = run.call_args.args[0]
        self.assertEqual(command[command.index("--world") + 1], "McWilliams")
        self.assertNotIn("--fixture", command)
        self.assertNotIn("--profile-snapshot", command)
        self.assertIn("--registry-launch-receipt", command)

    def test_relaunch_rejects_same_pid_and_missing_original_exit(self) -> None:
        with mock.patch.object(harness, "wait_for_pid_exit", return_value=False), \
                mock.patch.object(harness, "run_json_command") as run:
            missing_exit = harness.run_probe_post_relaunch(
                initial_pid=101,
                profile="profile",
                config_profile="config",
                world="McWilliams",
                scenario_name="test",
                registry_launch_receipt="",
                terminal_exit_timeout_seconds=1,
            )
        self.assertEqual(missing_exit["status"], "terminal_process_exit_missing")
        run.assert_not_called()

        with mock.patch.object(harness, "wait_for_pid_exit", return_value=True), \
                mock.patch.object(harness, "run_json_command", return_value=(0, {
                    "ok": True,
                    "pid": 101,
                    "focus": {"ok": True},
                    "proof_classification": {"startup_clean_for_feature_steps": True},
                }, "", "")):
            same_pid = harness.run_probe_post_relaunch(
                initial_pid=101,
                profile="profile",
                config_profile="config",
                world="McWilliams",
                scenario_name="test",
                registry_launch_receipt="",
                terminal_exit_timeout_seconds=1,
            )
        self.assertEqual(same_pid["status"], "same_pid_relaunch_rejected")

    def test_probe_runs_post_relaunch_steps_then_finalizes_once_with_new_pid(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            run_dir = root / "run"
            run_dir.mkdir()
            artifact_log = root / "debug.log"
            artifact_log.write_text("", encoding="utf-8")
            args = SimpleNamespace(
                scenario="test.relaunch", profile="", world="", fixture=None,
                replace_existing_worlds=False, advance_turns=None, settle_seconds=None,
                artifact_pattern="", test_command="", dry_run=False, compact_stdout=True,
                registry_launch_receipt="receipt", registry_post_finalize_hook=None,
            )
            scenario = {
                "name": "test.relaunch",
                "profile": "probe-profile",
                "world": "McWilliams",
                "steps": [{"label": "save_and_exit", "kind": "press", "keys": ["q"]}],
                "post_relaunch": {
                    "terminal_save_step_label": "save_and_exit",
                    "terminal_exit_timeout_seconds": 1,
                    "steps": [{"label": "observe_saved_identity", "kind": "wait", "seconds": 1}],
                },
            }
            first_start = {
                "ok": True, "pid": 101, "run_dir": str(run_dir),
                "screen": {}, "proof_classification": {"startup_clean_for_feature_steps": True},
                "debug_log_identity": {}, "debug_log_classified_size": 0,
            }
            relaunch_start = {
                "ok": True, "pid": 202, "run_dir": str(root / "relaunch"),
                "focus": {"ok": True},
                "proof_classification": {"startup_clean_for_feature_steps": True},
            }
            finalize = mock.Mock()
            execute = mock.Mock(side_effect=[
                [{"label": "save_and_exit", "kind": "press"}],
                [{"label": "observe_saved_identity", "kind": "wait"}],
            ])
            screenshot = mock.Mock(return_value={"screen_summary": {}})
            patches = {
                "load_scenario": mock.Mock(return_value=scenario),
                "scenario_manifest_binding": mock.Mock(return_value={}),
                "scenario_blocker_info": mock.Mock(return_value={"status": "active"}),
                "resolve_profile_name": mock.Mock(return_value="probe-profile"),
                "resolve_startup_config_profile": mock.Mock(return_value="dev-harness"),
                "resolve_scenario_profile_option_overrides": mock.Mock(return_value={}),
                "portal_storm_policy_from_scenario": mock.Mock(return_value={}),
                "run_json_command": mock.Mock(side_effect=[(0, first_start, "", ""), (0, relaunch_start, "", "")]),
                "resolve_artifact_source": mock.Mock(return_value=(artifact_log, False, "debug.log")),
                "probe_runtime_blockers": mock.Mock(return_value=[]),
                "probe_runtime_warnings": mock.Mock(return_value=[]),
                "read_current_saved_weather_audit": mock.Mock(return_value={}),
                "config_dir_for_profile": mock.Mock(return_value=root),
                "capture_screenshot": screenshot,
                "execute_probe_steps": execute,
                "wait_for_pid_exit": mock.Mock(return_value=True),
                "capture_feature_phase_guard": mock.Mock(return_value={"status": "green", "ledger_row": {}}),
                "render_derived_screens": mock.Mock(return_value=[]),
                "declared_screen_artifact_matches": mock.Mock(return_value=[]),
                "summarize_wait_step_ledgers": mock.Mock(return_value={"status": "green"}),
                "build_portal_storm_warning_for_report": mock.Mock(return_value={}),
                "build_probe_step_ledger": mock.Mock(return_value=[]),
                "portal_storm_step_ledger_rows": mock.Mock(return_value=[]),
                "summarize_probe_step_ledger": mock.Mock(return_value={"status": "green"}),
                "probe_proof_classification": mock.Mock(return_value={"feature_proof": False}),
                "finalize_scenario_report": finalize,
            }
            with ExitStack() as stack:
                for name, replacement in patches.items():
                    stack.enter_context(mock.patch.object(harness, name, replacement))
                self.assertEqual(harness.run_probe_mode(args), 0)

            self.assertEqual([call.args[0] for call in execute.call_args_list], [101, 202])
            self.assertTrue(any(call.args[0] == 202 for call in screenshot.call_args_list))
            finalize.assert_called_once()
            self.assertEqual(finalize.call_args.kwargs["cleanup_pid"], 202)
            report = finalize.call_args.args[1]
            self.assertEqual(report["relaunch"]["status"], "ready")
            self.assertEqual(report["steps"][-1]["phase"], "post_relaunch")


if __name__ == "__main__":
    unittest.main()
