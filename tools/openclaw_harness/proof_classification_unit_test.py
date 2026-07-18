#!/usr/bin/env python3
"""Unit tests for OpenClaw harness proof classification.

These tests freeze the report-writer seam that prevents load-only, stale-startup,
wait-ledger, or non-green step-ledger runs from being reported as feature proof
just because a log/artifact pattern matched.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from typing import Any, Dict, List
from unittest.mock import patch

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    audit_saved_weather_state,
    apply_direct_child_liveness,
    build_feature_debug_guard,
    build_probe_step_ledger,
    build_portal_storm_warning,
    capture_debug_delta,
    capture_final_startup_evidence,
    capture_feature_phase_guard,
    capture_stable_final_debug_delta,
    classify_wait_step_ledger,
    compact_probe_report_for_stdout,
    extract_window_build_info,
    filter_debug_log_text,
    log_file_identity,
    missing_peekaboo_capabilities,
    peekaboo_command,
    peekaboo_focus_pid,
    portal_storm_policy_from_scenario,
    portal_storm_step_ledger_rows,
    probe_proof_classification,
    render_repeatability_text_report,
    require_peekaboo_permissions,
    repeatability_run_is_green,
    repeatability_run_summary,
    run_repeatability,
    runtime_relevant_worktree_changes,
    screen_checkpoint_verdict,
    startup_proof_classification,
    startup_result_status,
    startup_screen_probe_classification,
    summarize_peekaboo_image_capture,
    summarize_probe_step_ledger,
)


def startup(*, clean: bool = True, status: str = "green") -> Dict[str, Any]:
    return {
        "evidence_class": "startup/load",
        "status": status,
        "verdict": "startup_load_only",
        "feature_proof": False,
        "startup_clean_for_feature_steps": clean,
        "feature_gate": "clean_startup_ready_for_feature_steps" if clean else "startup_not_clean",
    }


def matches() -> List[Dict[str, Any]]:
    return [{"pattern": "claim scoped line", "lines": ["claim scoped line"]}]


class PeekabooTransportAndCaptureReportTest(unittest.TestCase):
    def test_split_permission_contract_accepts_actual_local_input_and_bridge_capture_shapes(self) -> None:
        local_payload = {
            "success": True,
            "data": {
                "source": "local",
                "permissions": [
                    {"name": "Screen Recording", "isRequired": True, "isGranted": False},
                    {"name": "Accessibility", "isRequired": True, "isGranted": True},
                    {"name": "Event Synthesizing", "isRequired": False, "isGranted": False},
                ],
            },
        }
        bridge_payload = {
            "success": True,
            "data": {
                "source": "bridge",
                "permissions": [
                    {"name": "Screen Recording", "isRequired": True, "isGranted": True},
                    {"name": "Accessibility", "isRequired": True, "isGranted": True},
                    {"name": "Event Synthesizing", "isRequired": False, "isGranted": False},
                ],
            },
        }

        self.assertEqual(
            missing_peekaboo_capabilities(local_payload, ["Accessibility"]),
            [],
        )
        self.assertEqual(missing_peekaboo_capabilities(bridge_payload, ["Screen Recording"]), [])
        self.assertEqual(
            [entry["name"] for entry in missing_peekaboo_capabilities(local_payload, ["Screen Recording"])],
            ["Screen Recording"],
        )

        with patch("startup_harness.peekaboo_permission_preflight") as preflight:
            preflight.return_value = {"status": "green"}
            require_peekaboo_permissions()

        self.assertEqual(preflight.call_args_list[0].args, ("input", ["Accessibility"]))
        self.assertEqual(preflight.call_args_list[1].args, ("capture", ["Screen Recording"]))

    def test_input_is_local_while_capture_remains_bridge_aware(self) -> None:
        saved_env = dict(os.environ)
        try:
            os.environ.update({
                "CAOL_PEEKABOO_BIN": "/test/peekaboo",
                "CAOL_PEEKABOO_INPUT_TRANSPORT": "local",
                "CAOL_PEEKABOO_CAPTURE_TRANSPORT": "bridge",
                "CAOL_PEEKABOO_BRIDGE_SOCKET": "/tmp/peekaboo-bridge.sock",
            })
            input_cmd = peekaboo_command(["press", "return", "--pid", "42"], channel="input")
            capture_cmd = peekaboo_command(["image", "--json", "--pid", "42"], channel="capture")
        finally:
            os.environ.clear()
            os.environ.update(saved_env)

        self.assertEqual(input_cmd[0], "/test/peekaboo")
        self.assertIn("--no-remote", input_cmd)
        self.assertNotIn("--no-remote", capture_cmd)
        self.assertEqual(capture_cmd[-2:], ["--bridge-socket", "/tmp/peekaboo-bridge.sock"])

    def test_capture_summary_preserves_actual_bridge_warning_and_stderr_shape(self) -> None:
        payload = {
            "success": True,
            "data": {
                "files": [{"window_title": "Cataclysm: Dark Days Ahead", "window_id": 8138}],
                "observations": [{
                    "warnings": [
                        "Captured window image appears solid black; target may be occluded, transparent, or non-renderable."
                    ]
                }],
            },
            "debug_logs": [
                "DEBUG: Runtime host: remote gui via /Users/test/Library/Application Support/Peekaboo/bridge.sock"
            ],
        }
        summary = summarize_peekaboo_image_capture(
            json.dumps(payload),
            Path("success.png"),
            Path("success.peekaboo.json"),
            stderr="bridge capture diagnostic",
            command=["peekaboo", "image", "--json"],
            returncode=0,
        )

        self.assertEqual(summary["peekaboo_stderr"], "bridge capture diagnostic")
        self.assertIn("solid black", summary["capture_warnings"][0])
        self.assertIn("remote gui via", summary["peekaboo_runtime_host"])

    def test_current_sdl_window_title_yields_build_identity(self) -> None:
        info = extract_window_build_info("Cataclysm: Dark Days Ahead - 1987e94ba13-dirty+SDL3")

        self.assertEqual(info["captured_head"], "1987e94ba13")
        self.assertTrue(info["captured_dirty"])

    def test_matching_dirty_build_is_blocked_when_runtime_worktree_is_dirty(self) -> None:
        payload = {
            "success": True,
            "data": {
                "files": [{
                    "window_title": "Cataclysm: Dark Days Ahead - 1987e94ba13-dirty+SDL3",
                    "window_id": 8138,
                }],
            },
        }
        with (
            patch("startup_harness.current_head_short", return_value="1987e94ba13"),
            patch("startup_harness.runtime_relevant_changes_since", return_value=([], "")),
            patch(
                "startup_harness.runtime_relevant_worktree_changes",
                return_value=(["src/savegame.cpp"], ""),
            ),
        ):
            summary = summarize_peekaboo_image_capture(
                json.dumps(payload),
                Path("success.png"),
                Path("success.peekaboo.json"),
            )

        self.assertTrue(summary["captured_dirty"])
        self.assertFalse(summary["version_matches_runtime_paths"])
        self.assertEqual(summary["runtime_relevant_worktree_diff"], ["src/savegame.cpp"])

    def test_dirty_llm_runner_is_a_runtime_relevant_worktree_change(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            runner = root / "tools" / "llm_runner" / "runner.py"
            runner.parent.mkdir(parents=True)
            runner.write_text("print('committed')\n", encoding="utf-8")
            subprocess.run(
                ["git", "init", "--quiet", str(root)],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                ["git", "-C", str(root), "add", "tools/llm_runner/runner.py"],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(root),
                    "-c",
                    "user.name=Harness Test",
                    "-c",
                    "user.email=harness@example.invalid",
                    "commit",
                    "--quiet",
                    "-m",
                    "fixture",
                ],
                check=True,
                capture_output=True,
            )
            runner.write_text("print('dirty')\n", encoding="utf-8")

            with patch("startup_harness.repo_root", return_value=root):
                changes, error = runtime_relevant_worktree_changes()

        self.assertEqual(error, "")
        self.assertEqual(changes, ["tools/llm_runner/runner.py"])

    def test_build_version_dirty_check_ignores_agents_but_catches_staged_and_untracked_runtime(self) -> None:
        repository_root = HARNESS_DIR.parents[1]
        makefile = (repository_root / "Makefile").read_text(encoding="utf-8")
        cmake_version = (repository_root / "src" / "version.cmake").read_text(encoding="utf-8")
        for build_version_source in (makefile, cmake_version):
            self.assertIn("status --porcelain --untracked-files=all", build_version_source)
            self.assertIn(":(exclude)Agents.md", build_version_source)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            agents = root / "Agents.md"
            runner = root / "tools" / "llm_runner" / "runner.py"
            runner.parent.mkdir(parents=True)
            agents.write_text("committed guidance\n", encoding="utf-8")
            runner.write_text("print('committed')\n", encoding="utf-8")
            subprocess.run(
                ["git", "init", "--quiet", str(root)],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                ["git", "-C", str(root), "add", "Agents.md", "tools/llm_runner/runner.py"],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(root),
                    "-c",
                    "user.name=Harness Test",
                    "-c",
                    "user.email=harness@example.invalid",
                    "commit",
                    "--quiet",
                    "-m",
                    "fixture",
                ],
                check=True,
                capture_output=True,
            )
            dirty_command = [
                "git",
                "-C",
                str(root),
                "status",
                "--porcelain",
                "--untracked-files=all",
                "--",
                ".",
                ":(exclude)lang/po/**",
                ":(exclude)Agents.md",
            ]

            agents.write_text("Josef's local guidance\n", encoding="utf-8")
            agents_only = subprocess.run(
                dirty_command,
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertEqual(agents_only.stdout, "")

            runner.write_text("print('staged runtime')\n", encoding="utf-8")
            subprocess.run(
                ["git", "-C", str(root), "add", "tools/llm_runner/runner.py"],
                check=True,
                capture_output=True,
            )
            staged_runtime = subprocess.run(
                dirty_command,
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertIn("tools/llm_runner/runner.py", staged_runtime.stdout)

            subprocess.run(
                [
                    "git",
                    "-C",
                    str(root),
                    "restore",
                    "--staged",
                    "--worktree",
                    "tools/llm_runner/runner.py",
                ],
                check=True,
                capture_output=True,
            )
            untracked_runtime = root / "src" / "new_runtime.cpp"
            untracked_runtime.parent.mkdir()
            untracked_runtime.write_text("// untracked runtime\n", encoding="utf-8")
            untracked_result = subprocess.run(
                dirty_command,
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertIn("src/new_runtime.cpp", untracked_result.stdout)

    def test_dirty_build_remains_unproven_after_clean_runtime_comparisons(self) -> None:
        payload = {
            "success": True,
            "data": {
                "files": [{
                    "window_title": "Cataclysm: Dark Days Ahead - 1987e94ba13-dirty+SDL3",
                    "window_id": 8138,
                }],
            },
        }
        with (
            patch("startup_harness.current_head_short", return_value="1987e94ba13"),
            patch("startup_harness.runtime_relevant_changes_since", return_value=([], "")),
            patch("startup_harness.runtime_relevant_worktree_changes", return_value=([], "")),
        ):
            summary = summarize_peekaboo_image_capture(
                json.dumps(payload),
                Path("success.png"),
                Path("success.peekaboo.json"),
            )

        self.assertTrue(summary["captured_dirty"])
        self.assertFalse(summary["version_matches_runtime_paths"])

    def test_info_and_warning_log_growth_is_artifact_only_not_popup_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text(
                "00:00:00.000 INFO : ordinary startup banner\n"
                "00:00:00.001 WARNING : ordinary startup warning\n",
                encoding="utf-8",
            )
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                report = capture_debug_delta("master", 0, run_dir, 1)

            artifact_path = Path(report["artifact_path"])
            self.assertTrue(artifact_path.exists())
            self.assertIn("ordinary startup banner", artifact_path.read_text(encoding="utf-8"))
            self.assertEqual(report["error_evidence_lines"], [])

    def test_classed_cata_error_log_is_archived_and_release_blocking(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text(
                "00:00:00.001 ERROR SDL : renderer failed\n",
                encoding="utf-8",
            )
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                report = capture_debug_delta("master", 0, run_dir, 1)

        self.assertEqual(
            report["error_evidence_lines"],
            ["00:00:00.001 ERROR SDL : renderer failed"],
        )
        result = startup_proof_classification(
            ok=True,
            screen_summary={
                "capture_success": True,
                "version_matches_runtime_paths": True,
                "startup_screen_probe": {
                    "classification": "green_gameplay_hud_present",
                    "gameplay_hud_present": True,
                    "visible_error_popup": False,
                    "startup_error_logged": False,
                },
            },
            focus_result={"ok": True},
            debug_errors_recorded=1,
        )
        self.assertEqual(result["feature_gate"], "startup_error_logged")

    def test_split_unit_buffered_error_line_is_reassembled_before_offset_advances(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text("12:34:56.789 ERROR SDL", encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                first = capture_debug_delta("master", 0, run_dir, 1)
                with debug_log.open("a", encoding="utf-8") as stream:
                    stream.write(" : renderer failed\n")
                second = capture_debug_delta(
                    "master",
                    int(first["current_size"]),
                    run_dir,
                    1,
                )

        self.assertEqual(first["current_size"], 0)
        self.assertGreater(first["unclassified_trailing_bytes"], 0)
        self.assertEqual(first["error_evidence_lines"], [])
        self.assertEqual(
            second["error_evidence_lines"],
            ["12:34:56.789 ERROR SDL : renderer failed"],
        )

    def test_one_shot_final_scan_blocks_incomplete_error_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            (config_dir / "debug.log").write_text(
                "12:34:56.789 ERROR SDL",
                encoding="utf-8",
            )
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                report = capture_debug_delta(
                    "master",
                    0,
                    run_dir,
                    1,
                    include_incomplete_line=True,
                )

        self.assertEqual(report["current_size"], 0)
        self.assertGreater(report["unclassified_trailing_bytes"], 0)
        self.assertEqual(
            report["error_evidence_lines"],
            ["12:34:56.789 ERROR SDL"],
        )

    def test_timestamp_only_final_tail_is_unproven_after_bounded_retry(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            (config_dir / "debug.log").write_text("12:34:56.789 ", encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                capture = capture_stable_final_debug_delta(
                    "master",
                    0,
                    run_dir,
                    1,
                    artifact_name="debug.final.log",
                    max_attempts=1,
                    retry_seconds=0,
                )
        guard = build_feature_debug_guard(
            debug_capture=capture,
            screen_probe={"ocr_ok": True, "black_capture_warning": False},
            screen_summary={"peekaboo_success": True},
            process_alive=True,
        )

        self.assertFalse(capture["tail_classified"])
        self.assertEqual(capture["tail_classification"], "ambiguous_after_retry")
        self.assertEqual(guard["status"], "yellow")
        self.assertIn("feature_phase_debug_tail_unclassified", guard["ledger_row"]["issues"])

    def test_unterminated_complete_info_line_is_classified_benign(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            info_line = "12:34:56.789 INFO : background task still healthy"
            debug_log = config_dir / "debug.log"
            debug_log.write_text(info_line, encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                capture = capture_stable_final_debug_delta(
                    "master",
                    0,
                    run_dir,
                    1,
                    artifact_name="debug.final.log",
                    max_attempts=1,
                    retry_seconds=0,
                )
        guard = build_feature_debug_guard(
            debug_capture=capture,
            screen_probe={"ocr_ok": True, "black_capture_warning": False},
            screen_summary={"peekaboo_success": True},
            process_alive=True,
        )

        self.assertTrue(capture["tail_classified"])
        self.assertEqual(capture["tail_classification"], "benign_info_or_warning")
        self.assertEqual(capture["current_size"], len(info_line.encode("utf-8")))
        self.assertEqual(guard["status"], "green")

    def test_direct_start_marks_ambiguous_final_tail_unsafe(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            ambiguous_capture = {
                "artifact_path": "",
                "error_evidence_lines": [],
                "current_size": 0,
                "raw_current_size": 13,
                "tail_classified": False,
                "tail_classification": "ambiguous_after_retry",
            }
            with (
                patch("startup_harness.capture_startup_screen_probe", return_value={
                    "visible_error_popup": False,
                    "ocr_ok": True,
                    "black_capture_warning": False,
                }),
                patch("startup_harness.capture_stable_final_debug_delta", return_value=ambiguous_capture),
                patch("startup_harness.pid_is_alive", return_value=True),
            ):
                report = capture_final_startup_evidence(
                    profile="master",
                    debug_start_size=0,
                    run_dir=run_dir,
                    screen_summary={},
                    label="success",
                    serial=1,
                    pid=42,
                )

        self.assertFalse(report["debug_tail_classified"])
        self.assertFalse(report["safe_for_startup"])

    def test_owned_child_poll_rejects_an_unreaped_exited_process(self) -> None:
        class ExitedChild:
            def poll(self) -> int:
                return 7

        evidence = {
            "process_alive": True,
            "debug_tail_classified": True,
            "safe_for_startup": True,
        }

        alive = apply_direct_child_liveness(evidence, ExitedChild())  # type: ignore[arg-type]

        self.assertFalse(alive)
        self.assertFalse(evidence["process_alive"])
        self.assertFalse(evidence["safe_for_startup"])

    def test_four_digit_rounded_timestamp_is_release_blocking(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            (config_dir / "debug.log").write_text(
                "12:34:56.1000 ERROR MAP_GEN : rounded timestamp error\n",
                encoding="utf-8",
            )
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                report = capture_debug_delta("master", 0, run_dir, 1)

        self.assertEqual(
            report["error_evidence_lines"],
            ["12:34:56.1000 ERROR MAP_GEN : rounded timestamp error"],
        )

    def test_replaced_larger_debug_log_resets_stale_byte_offset(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            baseline_text = "12:00:00.000 INFO : old session\n" + ("x" * 128) + "\n"
            debug_log.write_text(baseline_text, encoding="utf-8")
            baseline_size = debug_log.stat().st_size
            baseline_identity = log_file_identity(debug_log)
            debug_log.rename(config_dir / "debug.log.prev")
            replacement = (
                "12:34:56.789 ERROR SDL : early rotated error\n"
                + ("12:34:56.790 INFO : replacement padding\n" * 8)
            )
            debug_log.write_text(replacement, encoding="utf-8")
            self.assertGreater(debug_log.stat().st_size, baseline_size)
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                report = capture_debug_delta(
                    "master",
                    baseline_size,
                    run_dir,
                    1,
                    expected_identity=baseline_identity,
                )

        self.assertTrue(report["identity_changed"])
        self.assertEqual(report["previous_size"], 0)
        self.assertIn("early rotated error", report["error_evidence_lines"][0])

    def test_final_startup_classifier_records_late_log_and_visible_modal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            (config_dir / "debug.log").write_text(
                "12:34:56.789 ERROR : late startup failure\n",
                encoding="utf-8",
            )
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with (
                patch("startup_harness.config_dir_for_profile", return_value=config_dir),
                patch("startup_harness.capture_startup_screen_probe", return_value={
                    "visible_error_popup": True,
                    "artifact_path": str(run_dir / "failure.startup_screen_probe.json"),
                }),
            ):
                report = capture_final_startup_evidence(
                    profile="master",
                    debug_start_size=0,
                    run_dir=run_dir,
                    screen_summary={},
                    label="failure_timeout",
                    serial=1,
                )

        self.assertTrue(report["debug_error_recorded"])
        self.assertTrue(report["visible_error_popup_recorded"])
        self.assertTrue(report["debug_delta_recorded"])

    def test_direct_start_scan_captures_error_emitted_during_ocr(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text("", encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()

            def emit_error_during_ocr(*args: Any, **kwargs: Any) -> Dict[str, Any]:
                debug_log.write_text(
                    "12:34:56.789 ERROR : emitted while OCR ran\n",
                    encoding="utf-8",
                )
                return {
                    "visible_error_popup": False,
                    "ocr_ok": True,
                    "black_capture_warning": False,
                    "artifact_path": str(run_dir / "success.startup_screen_probe.json"),
                }

            with (
                patch("startup_harness.config_dir_for_profile", return_value=config_dir),
                patch("startup_harness.capture_startup_screen_probe", side_effect=emit_error_during_ocr),
            ):
                report = capture_final_startup_evidence(
                    profile="master",
                    debug_start_size=0,
                    run_dir=run_dir,
                    screen_summary={},
                    label="success",
                    serial=1,
                )

        self.assertTrue(report["debug_error_recorded"])
        self.assertEqual(report["screen_probe"]["classification"], "red_startup_error_logged")

    def test_feature_scan_captures_error_emitted_during_ocr(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text("", encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()

            def emit_error_during_ocr(*args: Any, **kwargs: Any) -> Dict[str, Any]:
                debug_log.write_text(
                    "12:34:56.789 ERROR NPC : background action failed\n",
                    encoding="utf-8",
                )
                return {
                    "visible_error_popup": False,
                    "ocr_ok": True,
                    "black_capture_warning": False,
                    "artifact_path": str(run_dir / "probe.feature_screen_probe.json"),
                }

            with (
                patch("startup_harness.config_dir_for_profile", return_value=config_dir),
                patch("startup_harness.capture_startup_screen_probe", side_effect=emit_error_during_ocr),
                patch("startup_harness.pid_is_alive", return_value=True),
            ):
                guard = capture_feature_phase_guard(
                    profile="master",
                    debug_start_size=0,
                    run_dir=run_dir,
                    label="probe_after_feature_guard",
                    artifact_name="probe.feature_debug.log",
                    screen_summary={"peekaboo_success": True},
                    pid=42,
                )

        self.assertEqual(guard["status"], "red")
        self.assertEqual(guard["verdict"], "red_feature_phase_error_logged")
        self.assertIn("background action failed", guard["error_evidence_lines"][0])

    def test_known_mod_tracker_backtrace_is_filtered_as_one_record(self) -> None:
        preamble = (
            "12:34:56.789 ERROR : (error message will follow backtrace)\n"
            "    frame one\n"
            "Backtrace emission took 0 seconds.\n"
        )
        continuation = (
            "(continued from above) ERROR : src/mod_tracker.h:77: "
            "Tried check if 'vector_null' had a duplicate, but type 'attack_vector' "
            "does not track object sources\n"
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            config_dir = Path(temp_dir) / "config"
            config_dir.mkdir()
            debug_log = config_dir / "debug.log"
            debug_log.write_text(preamble, encoding="utf-8")
            run_dir = Path(temp_dir) / "run"
            run_dir.mkdir()
            with patch("startup_harness.config_dir_for_profile", return_value=config_dir):
                first = capture_debug_delta("master", 0, run_dir, 1)
                with debug_log.open("a", encoding="utf-8") as stream:
                    stream.write(continuation)
                    stream.write("12:34:56.790 INFO : ordinary line\n")
                second = capture_debug_delta(
                    "master",
                    int(first["current_size"]),
                    run_dir,
                    1,
                )

        self.assertEqual(first["error_evidence_lines"], [])
        self.assertLess(first["current_size"], first["raw_current_size"])
        self.assertEqual(second["error_evidence_lines"], [])
        filtered = filter_debug_log_text(preamble + continuation)
        self.assertNotIn("ERROR", filtered)

    def test_ignored_backtrace_never_consumes_an_earlier_real_error_record(self) -> None:
        real_record = (
            "12:34:55.000 ERROR : (error message will follow backtrace)\n"
            "    real frame\n"
            "Backtrace emission took 0 seconds.\n"
            "(continued from above) ERROR : src/savegame.cpp:42: real save failure\n"
        )
        ignored_record = (
            "12:35:56.000 ERROR : (error message will follow backtrace)\n"
            "    ignored frame\n"
            "Backtrace emission took 0 seconds.\n"
            "(continued from above) ERROR : src/mod_tracker.h:77: "
            "Tried check if 'vector_null' had a duplicate, but type 'attack_vector' "
            "does not track object sources\n"
        )

        filtered = filter_debug_log_text(real_record + ignored_record)

        self.assertIn("real save failure", filtered)
        self.assertIn("12:34:55.000 ERROR", filtered)
        self.assertNotIn("mod_tracker", filtered)
        self.assertNotIn("ignored frame", filtered)

    @patch("startup_harness.subprocess.run")
    def test_focus_timeout_with_byte_diagnostics_is_a_bounded_failure(self, run_mock: Any) -> None:
        run_mock.side_effect = subprocess.TimeoutExpired(
            cmd=["peekaboo", "window", "focus"],
            timeout=10.0,
            output=b"partial focus output",
            stderr=b"Timeout while waiting for condition",
        )

        result = peekaboo_focus_pid(42)

        self.assertEqual(result["returncode"], 124)
        self.assertFalse(result["ok"])
        self.assertEqual(result["stdout"], "partial focus output")
        self.assertIn("Timeout while waiting for condition", result["stderr"])
        self.assertIn("peekaboo focus timed out", result["stderr"])

    def test_compact_report_keeps_startup_gate_and_peekaboo_stderr(self) -> None:
        compact = compact_probe_report_for_stdout(
            {
                "startup": {
                    "screen": {
                        "png_path": "success.png",
                        "peekaboo_stderr": "capture failed over local TCC",
                        "capture_warnings": ["solid black"],
                        "startup_screen_probe": {
                            "classification": "red_visible_error_popup",
                            "visible_error_popup": True,
                            "gameplay_hud_present": False,
                        },
                    },
                    "proof_classification": {
                        "status": "red",
                        "feature_gate": "visible_error_popup",
                        "startup_clean_for_feature_steps": False,
                        "focus_proven": True,
                        "debug_popups_recorded": 1,
                    },
                },
            },
            run_dir=None,
            report_filename="probe.report.json",
        )

        self.assertEqual(compact["startup_screen"]["screenshot"], "success.png")
        self.assertEqual(compact["startup_screen"]["peekaboo_stderr"], "capture failed over local TCC")
        self.assertTrue(compact["startup_screen"]["visible_error_popup"])
        self.assertEqual(compact["startup_gate"]["feature_gate"], "visible_error_popup")
        self.assertEqual(compact["startup_gate"]["debug_popups_recorded"], 1)


class StartupScreenGateTest(unittest.TestCase):
    def gameplay_probe(self) -> Dict[str, Any]:
        # These are real OCR line shapes from a captured gameplay HUD fixture.
        return startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": [
                    "LEG",
                    "HEAD",
                    "TORSO",
                    "ARM",
                    "Speed:",
                    "Move: 100",
                    "Safe:",
                    "Activity:",
                    "Weary Malus:",
                ],
            },
            capture_warnings=[],
            debug_delta_text="",
        )

    def screen_summary(self, probe: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "capture_success": True,
            "version_matches_runtime_paths": True,
            "startup_screen_probe": probe,
        }

    def test_actual_debug_popup_shape_is_red_even_when_capture_command_succeeded(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": [
                    "An error has occurred",
                    "DEBUG: invalid dimension loaded",
                    "Press space bar to continue the game",
                ],
            },
            capture_warnings=[
                "Captured window image appears solid black; target may be occluded, transparent, or non-renderable."
            ],
            debug_delta_text=(
                "00:51:37.589 ERROR : (error message will follow backtrace)\n"
                "(continued from above) ERROR : src/savegame.cpp:264 invalid dimension loaded, using default dimension instead\n"
            ),
        )

        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(probe),
            focus_result={"ok": True},
        )

        self.assertEqual(probe["classification"], "red_visible_error_popup")
        self.assertTrue(probe["visible_error_popup"])
        self.assertTrue(probe["startup_error_logged"])
        self.assertEqual(result["status"], "red")
        self.assertEqual(result["feature_gate"], "visible_error_popup")
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_debug_error_remains_red_when_gameplay_hud_is_visible(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": ["HEAD", "TORSO", "ARM", "LEG", "Move: 100", "Safe:"],
            },
            capture_warnings=[],
            debug_delta_text="00:51:37.589 ERROR : invalid dimension loaded",
        )
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(probe),
            focus_result={"ok": True},
        )

        self.assertTrue(probe["gameplay_hud_present"])
        self.assertFalse(probe["visible_error_popup"])
        self.assertTrue(probe["startup_error_logged"])
        self.assertEqual(probe["classification"], "red_startup_error_logged")
        self.assertEqual(result["status"], "red")
        self.assertEqual(result["feature_gate"], "startup_error_logged")

    def test_polled_log_only_error_stays_red_without_popup_evidence(self) -> None:
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(self.gameplay_probe()),
            focus_result={"ok": True},
            debug_errors_recorded=1,
            debug_popups_recorded=0,
        )

        self.assertEqual(result["status"], "red")
        self.assertEqual(result["feature_gate"], "startup_error_logged")
        self.assertEqual(result["debug_errors_recorded"], 1)
        self.assertEqual(result["debug_popups_recorded"], 0)
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_medical_screen_body_parts_and_current_speed_are_not_a_gameplay_hud(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": [
                    "HEAD",
                    "TORSO",
                    "LEFT ARM",
                    "RIGHT ARM",
                    "LEFT LEG",
                    "RIGHT LEG",
                    "Base Move Cost: 100",
                    "Current Speed: 100",
                    "Pain: 0",
                ],
            },
            capture_warnings=[],
            debug_delta_text="",
        )

        self.assertFalse(probe["gameplay_hud_present"])
        self.assertEqual(probe["classification"], "yellow_gameplay_hud_absent")
        self.assertGreaterEqual(len(probe["hud_body_marker_types"]), 3)
        self.assertLess(len(probe["hud_status_marker_types"]), 2)

    def test_compact_mac_hud_with_one_visible_body_label_is_green(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": ["ARM", "Move: 0(W)", "Weary Malus:"],
            },
            capture_warnings=[],
            debug_delta_text="",
        )

        self.assertTrue(probe["gameplay_hud_present"])
        self.assertEqual(probe["classification"], "green_gameplay_hud_present")

    def test_one_body_label_and_one_status_label_match_the_compact_mac_hud(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": ["ARM", "Move: 0(W)"],
            },
            capture_warnings=[],
            debug_delta_text="",
        )

        self.assertTrue(probe["gameplay_hud_present"])
        self.assertEqual(probe["classification"], "green_gameplay_hud_present")

    def test_body_label_without_map_status_is_not_enough(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["ARM"]},
            capture_warnings=[],
            debug_delta_text="",
        )

        self.assertFalse(probe["gameplay_hud_present"])
        self.assertEqual(probe["classification"], "yellow_gameplay_hud_absent")

    def test_map_status_without_body_label_is_not_enough(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["Move: 0(W)"]},
            capture_warnings=[],
            debug_delta_text="",
        )

        self.assertFalse(probe["gameplay_hud_present"])
        self.assertEqual(probe["classification"], "yellow_gameplay_hud_absent")

    def test_unproven_focus_blocks_an_otherwise_real_gameplay_hud(self) -> None:
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(self.gameplay_probe()),
            focus_result={"ok": False, "stderr": "Timeout while waiting for condition"},
        )

        self.assertEqual(result["feature_gate"], "focus_unproven")
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_unproven_build_identity_blocks_an_otherwise_real_gameplay_hud(self) -> None:
        screen_summary = self.screen_summary(self.gameplay_probe())
        screen_summary["version_matches_runtime_paths"] = None
        result = startup_proof_classification(
            ok=True,
            screen_summary=screen_summary,
            focus_result={"ok": True},
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["feature_gate"], "runtime_version_unproven")
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_dismissed_debug_popup_still_blocks_real_gameplay_hud(self) -> None:
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(self.gameplay_probe()),
            focus_result={"ok": True},
            debug_popups_recorded=1,
        )

        self.assertEqual(result["status"], "red")
        self.assertEqual(result["feature_gate"], "debug_popups_recorded")
        self.assertEqual(result["debug_popups_recorded"], 1)
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_loading_or_menu_text_without_hud_blocks_feature_steps(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["CATACLYSM!!!", "Finalizing Overmap locations"]},
            capture_warnings=[],
            debug_delta_text="",
        )
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(probe),
            focus_result={"ok": True},
        )

        self.assertEqual(probe["classification"], "yellow_gameplay_hud_absent")
        self.assertEqual(result["feature_gate"], "gameplay_hud_absent")
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_actions_overlay_blocks_background_gameplay_hud_markers(self) -> None:
        probe = startup_screen_probe_classification(
            ocr_payload={
                "ok": True,
                "lines": [
                    "Actions",
                    "HEAD",
                    "TORSO",
                    "ARM",
                    "LEG",
                    "Move: 100",
                    "Safe:",
                ],
            },
            capture_warnings=[],
            debug_delta_text="",
        )
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(probe),
            focus_result={"ok": True},
        )

        self.assertFalse(probe["gameplay_hud_present"])
        self.assertTrue(probe["blocking_overlay_present"])
        self.assertEqual(probe["classification"], "yellow_blocking_overlay_present")
        self.assertEqual(result["feature_gate"], "blocking_overlay_present")
        self.assertFalse(result["startup_clean_for_feature_steps"])

    def test_verified_focus_and_real_hud_are_the_only_clean_startup_path(self) -> None:
        probe = self.gameplay_probe()
        result = startup_proof_classification(
            ok=True,
            screen_summary=self.screen_summary(probe),
            focus_result={"ok": True},
        )

        self.assertEqual(probe["classification"], "green_gameplay_hud_present")
        self.assertEqual(result["status"], "green")
        self.assertTrue(result["startup_clean_for_feature_steps"])


class ScreenCheckpointVerdictTest(unittest.TestCase):
    def test_named_screenshot_without_state_guard_is_not_green(self) -> None:
        verdict, issues = screen_checkpoint_verdict(
            screen_summary={"peekaboo_success": True},
            expected_visible_fact="NPC reached the requested destination",
        )

        self.assertEqual(verdict, "yellow_step_screen_checkpoint_caveated")
        self.assertIn("screen_fact_not_verified", issues)

    def test_matching_ocr_guard_can_make_screenshot_green(self) -> None:
        verdict, issues = screen_checkpoint_verdict(
            screen_summary={
                "peekaboo_success": True,
                "version_matches_runtime_paths": True,
            },
            expected_visible_fact="inventory window is visible",
            text_expectation={"status": "matched"},
            ocr_requested=True,
        )

        self.assertEqual(verdict, "green_step_screen_text_guarded")
        self.assertEqual(issues, [])

    def test_matching_ocr_guard_is_not_green_with_unknown_build_identity(self) -> None:
        verdict, issues = screen_checkpoint_verdict(
            screen_summary={
                "peekaboo_success": True,
                "version_matches_runtime_paths": None,
            },
            expected_visible_fact="inventory window is visible",
            text_expectation={"status": "matched"},
            ocr_requested=True,
        )

        self.assertEqual(verdict, "yellow_step_screen_text_matched_with_caveats")
        self.assertIn("runtime_version_unproven", issues)

    def test_deferred_step_points_at_decisive_artifact_not_decorative_screen(self) -> None:
        ledger = build_probe_step_ledger([
            {
                "label": "mechanical_press",
                "kind": "press",
                "proof_deferred_to_label": "decisive_audit",
                "expected_visible_fact": "requested state should be reached",
                "screen_after": {
                    "peekaboo_success": True,
                    "png_path": "decorative-screen.png",
                },
            },
            {
                "label": "decisive_audit",
                "kind": "audit_log_contains",
                "metadata": {
                    "status": "required_state_present",
                    "artifact_path": "decisive-audit.metadata.json",
                },
            },
        ])

        self.assertEqual(ledger[0]["verdict"], "green_step_proof_deferred_to_guard")
        self.assertEqual(ledger[0]["evidence_artifact"], "decisive-audit.metadata.json")


class ProbeProofClassificationTest(unittest.TestCase):
    def test_load_only_run_never_becomes_feature_proof(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "startup_load_only_no_feature_steps")
        self.assertEqual(result["evidence_class"], "startup/load-or-inconclusive")
        self.assertFalse(result["feature_proof"])

    def test_artifact_match_is_inconclusive_when_startup_gate_is_not_clean(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=False, status="yellow"),
            step_reports=[{"label": "feature_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "startup_gate_not_clean_artifact_match_inconclusive")
        self.assertFalse(result["feature_proof"])

    def test_non_green_step_ledger_overrides_artifact_match(self) -> None:
        yellow_ledger = summarize_probe_step_ledger([
            {"primitive_step": "unguarded_press", "verdict": "yellow_step_expected_fact_missing"}
        ])

        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "unguarded_press"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=yellow_ledger,
        )

        self.assertEqual(result["status"], "yellow")
        self.assertEqual(result["verdict"], "yellow_step_local_proof_incomplete")
        self.assertFalse(result["feature_proof"])

    def test_wait_ledger_blocker_overrides_artifact_match(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "long_wait"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            wait_step_summary={"status": "blocked_wait_step"},
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "red")
        self.assertFalse(result["feature_proof"])

    def test_yellow_wait_ledger_keeps_artifact_match_non_feature_proof(self) -> None:
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "long_wait"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            wait_step_summary={"status": "yellow_wait_step_unverified"},
            step_ledger_summary={"status": "green_step_local_proof"},
        )

        self.assertEqual(result["status"], "yellow")
        self.assertFalse(result["feature_proof"])

    def test_feature_proof_requires_clean_startup_green_steps_and_matched_artifact(self) -> None:
        green_ledger = summarize_probe_step_ledger([
            {"primitive_step": "guarded_step", "verdict": "green_step_expected_fact_present"}
        ])

        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "guarded_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=green_ledger,
        )

        self.assertEqual(result["status"], "green")
        self.assertEqual(result["evidence_class"], "feature-path")
        self.assertTrue(result["feature_proof"])

    def test_feature_phase_error_blocks_otherwise_green_artifacts_and_ledger(self) -> None:
        guard = build_feature_debug_guard(
            debug_capture={
                "artifact_path": "probe.feature_debug.log",
                "error_evidence_lines": ["12:34:56.789 ERROR NPC : feature failed"],
            },
            screen_probe={"visible_error_popup": False},
        )
        ledger = [{"primitive_step": "guarded_step", "verdict": "green_step_expected_fact_present"}]
        ledger.append(guard["ledger_row"])
        summary = summarize_probe_step_ledger(ledger)
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "guarded_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=summary,
        )

        self.assertEqual(guard["status"], "red")
        self.assertEqual(summary["status"], "red_step_local_proof_failed")
        self.assertEqual(result["status"], "red")
        self.assertFalse(result["feature_proof"])

    def test_feature_guard_requires_observable_screen_and_live_process(self) -> None:
        unobservable = build_feature_debug_guard(
            debug_capture={"artifact_path": "", "error_evidence_lines": []},
            screen_probe={"ocr_ok": False, "black_capture_warning": False},
            screen_summary={"peekaboo_success": True},
            process_alive=True,
        )
        exited = build_feature_debug_guard(
            debug_capture={"artifact_path": "", "error_evidence_lines": []},
            screen_probe={"ocr_ok": True, "black_capture_warning": False},
            screen_summary={"peekaboo_success": True},
            process_alive=False,
        )

        self.assertEqual(unobservable["status"], "yellow")
        self.assertIn("feature_phase_screen_ocr_failed", unobservable["ledger_row"]["issues"])
        self.assertEqual(exited["status"], "red")
        self.assertEqual(exited["verdict"], "red_feature_phase_process_exited")

    def test_repeatability_rejects_red_feature_proof_even_with_zero_returncode(self) -> None:
        report = {
            "ok": True,
            "feature_proof": False,
            "verdict": "blocked_feature_phase_error_logged",
            "proof_classification": {"status": "red", "feature_proof": False},
            "startup": {"screen": {"version_matches_runtime_paths": True}},
            "cleanup": {"status": "terminated"},
        }
        run = repeatability_run_summary(1, 0, report, [])

        self.assertEqual(run["proof_status"], "red")
        self.assertFalse(run["feature_proof"])
        self.assertFalse(repeatability_run_is_green(run))

    def test_startup_command_status_rejects_non_green_proof(self) -> None:
        for status in ("red", "yellow", ""):
            with self.subTest(status=status or "unclassified"):
                ok, reason = startup_result_status(
                    base_ok=True,
                    proof_classification={"status": status},
                )
                self.assertFalse(ok)
                self.assertEqual(reason, f"startup_proof_{status or 'unclassified'}")

        ok, reason = startup_result_status(
            base_ok=True,
            proof_classification={"status": "green"},
        )
        self.assertTrue(ok)
        self.assertEqual(reason, "")

    def test_compact_repeatability_preserves_proof_and_runtime_fields(self) -> None:
        report = {
            "ok": True,
            "scenario": "repeatability.compact",
            "feature_proof": True,
            "verdict": "artifacts_matched",
            "proof_classification": {
                "status": "green",
                "verdict": "green_feature_path_proven",
                "evidence_class": "feature-path",
                "feature_proof": True,
            },
            "startup": {
                "run_dir": "/tmp/repeatability-compact",
                "screen": {
                    "window_title": "C-AOL 69a04c7783",
                    "version_matches_runtime_paths": True,
                },
                "proof_classification": {
                    "status": "green",
                    "startup_clean_for_feature_steps": True,
                },
            },
            "cleanup": {"status": "terminated"},
            "portal_storm_warning": {
                "classification": "not_observed",
                "contaminates_result": False,
            },
        }
        compact = compact_probe_report_for_stdout(
            report,
            run_dir=Path("/tmp/repeatability-compact"),
            report_filename="probe.report.json",
        )
        run = repeatability_run_summary(1, 0, compact, [])

        self.assertEqual(run["proof_status"], "green")
        self.assertTrue(run["feature_proof"])
        self.assertTrue(run["version_matches_runtime_paths"])
        self.assertFalse(run["portal_storm_warning"]["contaminates_result"])
        self.assertTrue(repeatability_run_is_green(run))

    def test_repeatability_command_returns_failure_for_mixed_proof(self) -> None:
        report = {
            "ok": True,
            "feature_proof": False,
            "verdict": "blocked_feature_phase_error_logged",
            "proof_classification": {"status": "red", "feature_proof": False},
            "startup": {"screen": {"version_matches_runtime_paths": True}},
            "cleanup": {"status": "terminated"},
        }
        args = SimpleNamespace(
            scenario="repeatability.mixed",
            profile="",
            world="",
            fixture=None,
            count=1,
            replace_existing_worlds=False,
            compact_stdout=True,
            dry_run=False,
        )
        scenario = {
            "name": "repeatability.mixed",
            "profile": "repeatability-test",
            "world": "Repeatability Test",
            "repeatability_count": 1,
        }
        with tempfile.TemporaryDirectory() as temp_dir:
            run_dir = Path(temp_dir)
            with (
                patch("startup_harness.load_scenario", return_value=scenario),
                patch("startup_harness.resolve_profile_name", return_value="repeatability-test"),
                patch("startup_harness.create_run_dir", return_value=run_dir),
                patch("startup_harness.run_json_command", return_value=(0, report, "", "")),
                patch("builtins.print"),
            ):
                returncode = run_repeatability(args)

            summary = json.loads((run_dir / "repeatability.report.json").read_text(encoding="utf-8"))
        self.assertEqual(returncode, 1)
        self.assertFalse(summary["ok"])
        self.assertEqual(summary["overall_verdict"], "mixed_repeatability")

    def test_wait_ledger_accepts_claim_scoped_artifact_delta_after_bounded_wait(self) -> None:
        result = classify_wait_step_ledger(
            label="long_wait",
            choice_key="4",
            expected_duration="30m",
            before_text={"text": ""},
            menu_text={"text": "Wait how long? 30 minutes"},
            after_text={"text": ""},
            wait_classification={"status": "unknown_after_wait"},
            artifact_after_wait={
                "patterns": ["dispatch plan", "active_job=stalk"],
                "matches_by_pattern": [
                    {"pattern": "dispatch plan", "lines": ["dispatch plan"]},
                    {"pattern": "active_job=stalk", "lines": ["active_job=stalk"]},
                ],
            },
        )

        self.assertEqual(result["verdict"], "green_wait_step_proven")
        self.assertEqual(result["elapsed"]["status"], "artifact_delta_after_bounded_wait")
        self.assertEqual(result["finish_or_interrupt_status"], "completed_by_artifact_delta")
        self.assertEqual(result["issues"], [])

    def test_wait_ledger_does_not_accept_partial_artifact_delta(self) -> None:
        for matches_by_pattern in (
            [
                {"pattern": "dispatch plan", "lines": ["dispatch plan"]},
                {"pattern": "active_job=stalk", "lines": []},
            ],
            [
                {"pattern": "dispatch plan", "lines": ["dispatch plan"]},
            ],
        ):
            with self.subTest(matches_by_pattern=matches_by_pattern):
                result = classify_wait_step_ledger(
                    label="long_wait",
                    choice_key="4",
                    expected_duration="30m",
                    before_text={"text": ""},
                    menu_text={"text": "Wait how long? 30 minutes"},
                    after_text={"text": ""},
                    wait_classification={"status": "unknown_after_wait"},
                    artifact_after_wait={
                        "patterns": ["dispatch plan", "active_job=stalk"],
                        "matches_by_pattern": matches_by_pattern,
                    },
                )

                self.assertEqual(result["verdict"], "yellow_wait_finish_or_interrupt_not_classified")
                self.assertIn("before_after_clock_or_turn_not_parsed", result["issues"])
                self.assertIn("missing_finish_or_interruption_signal", result["issues"])
                self.assertIn("active_job=stalk", result["artifact_elapsed_evidence"]["missing_patterns"])



class PortalStormWarningTest(unittest.TestCase):
    def write_dimension_weather(self, root: Path, weather_id: str) -> Path:
        world_dir = root / "World"
        world_dir.mkdir(parents=True)
        payload = {
            "weather": {
                "weather_id": weather_id,
                "temperature": 62.0,
                "forced_temperature": None,
            }
        }
        (world_dir / "dimension_data.gsav").write_text(
            "# version 1\n" + json.dumps(payload, separators=(",", ":")),
            encoding="utf-8",
        )
        return world_dir

    def test_saved_weather_audit_detects_portal_storm_positive_row(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            world_dir = self.write_dimension_weather(Path(tmp), "portal_storm")
            audit = audit_saved_weather_state(world_dir)
            warning = build_portal_storm_warning(policy={"allowed": False, "required": False}, current_audit=audit)

        self.assertEqual(audit["observed_weather_id"], "portal_storm")
        self.assertEqual(warning["status"], "active")
        self.assertEqual(warning["classification"], "contaminating")
        self.assertTrue(warning["contaminates_result"])
        self.assertIn("PORTAL STORM ACTIVE", warning["summary"])

    def test_saved_weather_audit_negative_control_does_not_raise_warning(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            world_dir = self.write_dimension_weather(Path(tmp), "clear")
            audit = audit_saved_weather_state(world_dir)
            warning = build_portal_storm_warning(policy={"allowed": False, "required": False}, current_audit=audit)

        self.assertEqual(audit["observed_weather_id"], "clear")
        self.assertEqual(warning["status"], "not_observed")
        self.assertEqual(warning["classification"], "clear")
        self.assertFalse(warning["contaminates_result"])

    def test_unallowed_portal_storm_adds_yellow_ledger_row_and_blocks_feature_proof(self) -> None:
        warning = build_portal_storm_warning(
            policy={"allowed": False, "required": False},
            current_audit={"status": "scanned", "observed_weather_id": "close_portal_storm"},
        )
        ledger = [{"primitive_step": "guarded_step", "verdict": "green_step_expected_fact_present"}]
        ledger.extend(portal_storm_step_ledger_rows(warning))
        summary = summarize_probe_step_ledger(ledger)
        result = probe_proof_classification(
            verdict="artifacts_matched",
            startup_classification=startup(clean=True),
            step_reports=[{"label": "guarded_step"}],
            artifact_patterns=["claim scoped line"],
            matches_by_pattern=matches(),
            step_ledger_summary=summary,
        )

        self.assertEqual(summary["status"], "yellow_step_local_proof_incomplete")
        self.assertIn("yellow_step_portal_storm_contamination", summary["verdicts"])
        self.assertEqual(result["status"], "yellow")
        self.assertFalse(result["feature_proof"])

    def test_allowed_portal_storm_stays_green_and_remains_visible(self) -> None:
        policy = portal_storm_policy_from_scenario({"portal_storm": {"allow": True}})
        warning = build_portal_storm_warning(
            policy=policy,
            current_audit={"status": "scanned", "observed_weather_id": "WEATHER_PORTAL_STORM"},
        )
        ledger = [{"primitive_step": "guarded_step", "verdict": "green_step_expected_fact_present"}]
        ledger.extend(portal_storm_step_ledger_rows(warning))
        summary = summarize_probe_step_ledger(ledger)

        self.assertEqual(warning["status"], "active")
        self.assertEqual(warning["classification"], "expected_allowed")
        self.assertIn("EXPECTED BY SCENARIO", warning["summary"])
        self.assertEqual(summary["status"], "green_step_local_proof")
        self.assertIn("green_step_portal_storm_expected_allowed", summary["verdicts"])

    def test_repeatability_text_report_surfaces_portal_storm_warning(self) -> None:
        text = render_repeatability_text_report({
            "scenario": "portal.weather_probe",
            "run_count": 1,
            "overall_verdict": "mixed_repeatability",
            "runs": [{
                "run": 1,
                "verdict": "yellow_step_local_proof_incomplete",
                "cleanup": {"status": "terminated"},
                "version_matches_runtime_paths": True,
                "portal_storm_warning": {
                    "status": "active",
                    "summary": "⚠ PORTAL STORM ACTIVE / HARNESS RESULT MAY BE CONTAMINATED",
                },
                "expectations": [],
            }],
            "aggregate_expectations": [],
        })

        self.assertIn("portal_storm=active", text)
        self.assertIn("PORTAL STORM ACTIVE / HARNESS RESULT MAY BE CONTAMINATED", text)


if __name__ == "__main__":
    unittest.main()
