#!/usr/bin/env python3
"""Focused regression check for native action-menu startup filtering."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import startup_harness

from startup_harness import (
    recover_declared_startup_action_menu_overlay,
    startup_action_menu_dispatched_since,
    startup_main_menu_overlay_is_active,
    startup_screen_probe_classification,
)

ACTION_MENU_OPEN = b"openclaw_harness_ui_trace: component=action_menu event=open"
ACTION_MENU_CANCELLED = b"openclaw_harness_ui_trace: component=action_menu event=cancelled"
ACTION_MENU_RETURN = b"openclaw_harness_ui_trace: component=action_menu event=return"


class StartupActionMenuProbeTest( unittest.TestCase ):
    def test_native_action_menu_dispatch_blocks_background_hud( self ) -> None:
        result = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["Str:", "Activity:"]},
            capture_warnings=[],
            debug_delta_text=ACTION_MENU_OPEN.decode("utf-8"),
        )
        self.assertFalse(result["gameplay_hud_present"])
        self.assertTrue(result["blocking_overlay_present"])
        self.assertIn("native_action_menu_dispatch", result["blocking_overlay_markers"])

    def test_dispatch_guard_uses_the_run_boundary( self ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            trace.write_bytes(ACTION_MENU_OPEN + b"\n")
            boundary = trace.stat().st_size
            self.assertFalse(startup_action_menu_dispatched_since(trace, boundary))
            with trace.open("ab") as handle:
                handle.write(ACTION_MENU_OPEN + b"\n")
            self.assertTrue(startup_action_menu_dispatched_since(trace, boundary))
            with trace.open("ab") as handle:
                handle.write(ACTION_MENU_RETURN + b"\n")
            self.assertFalse(startup_action_menu_dispatched_since(trace, boundary))

    @patch("startup_harness.time.sleep")
    @patch("startup_harness.run_peekaboo_interaction", return_value={"ok": True})
    @patch("startup_harness.peekaboo_focus_pid", return_value={"ok": True})
    def test_declared_recovery_uses_current_native_state_and_requires_return(
            self, _focus_mock, _transport_mock, _sleep_mock
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            artifact = Path(directory) / "recovery.log"
            trace.write_bytes(ACTION_MENU_OPEN + b"\n")
            def native_return(*_args, **_kwargs):
                with trace.open("ab") as handle:
                    handle.write(ACTION_MENU_CANCELLED + b"\n" + ACTION_MENU_RETURN + b"\n")
                return {"ok": True}
            with patch("startup_harness.run_peekaboo_interaction", side_effect=native_return):
                result = recover_declared_startup_action_menu_overlay(17, trace, artifact)
        self.assertEqual(result["status"], "dismissed_declared_blocking_overlay")
        self.assertTrue(result["transport_receipt"]["ok"])
        self.assertTrue(result["native_handling_observed"])
        self.assertTrue(result["return_trace_observed"])

    @patch("startup_harness.run_peekaboo_interaction")
    def test_declared_recovery_fails_closed_when_no_native_overlay_is_open(self, transport_mock) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            artifact = Path(directory) / "recovery.log"
            trace.write_bytes(ACTION_MENU_OPEN + b"\n" + ACTION_MENU_RETURN + b"\n")
            result = recover_declared_startup_action_menu_overlay(17, trace, artifact)
        self.assertEqual(result["status"], "declared_overlay_not_present")
        transport_mock.assert_not_called()

    @patch("startup_harness.time.sleep")
    @patch("startup_harness.run_peekaboo_interaction", return_value={"ok": True})
    @patch("startup_harness.peekaboo_focus_pid", return_value={"ok": True})
    def test_declared_recovery_ignores_stale_main_menu_trace_before_process_boundary(
            self, _focus_mock, _transport_mock, _sleep_mock
    ) -> None:
        stale_main_menu_open = (
            b"openclaw_harness_ui_trace: component=in_game_main_menu event=open\n"
        )
        current = ACTION_MENU_OPEN + b"\n"
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            artifact = Path(directory) / "recovery.log"
            trace.write_bytes(stale_main_menu_open + current)
            def native_return(*_args, **_kwargs):
                with trace.open("ab") as handle:
                    handle.write(ACTION_MENU_CANCELLED + b"\n" + ACTION_MENU_RETURN + b"\n")
                return {"ok": True}
            with patch("startup_harness.run_peekaboo_interaction", side_effect=native_return):
                result = recover_declared_startup_action_menu_overlay(
                    17, trace, artifact, trace_start_offset=len(stale_main_menu_open)
                )
            artifact_text = artifact.read_text(encoding="utf-8")
        self.assertEqual(result["status"], "dismissed_declared_blocking_overlay")
        self.assertFalse(result["native_main_menu_active_before"])
        self.assertNotIn("in_game_main_menu", artifact_text)

    @patch("startup_harness.run_peekaboo_interaction")
    def test_declared_recovery_fails_closed_on_invalid_process_trace_boundary(self, transport_mock) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "debug.log"
            artifact = Path(directory) / "recovery.log"
            trace.write_bytes(ACTION_MENU_OPEN + b"\n")
            result = recover_declared_startup_action_menu_overlay(
                17, trace, artifact, trace_start_offset=trace.stat().st_size + 1
            )
        self.assertEqual(result["status"], "declared_overlay_trace_boundary_invalid")
        transport_mock.assert_not_called()

    def test_native_main_menu_trace_blocks_background_hud_until_return(self) -> None:
        opened = (
            "openclaw_harness_ui_trace: component=in_game_main_menu event=open\n"
        )
        self.assertTrue(startup_main_menu_overlay_is_active(opened))
        blocked = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["HEAD", "Move:", "Safe:"]},
            capture_warnings=[],
            debug_delta_text=opened,
        )
        self.assertTrue(blocked["blocking_overlay_present"])
        self.assertIn("native_main_menu_overlay", blocked["blocking_overlay_markers"])

        returned = opened + (
            "openclaw_harness_ui_trace: component=in_game_main_menu event=return\n"
        )
        self.assertFalse(startup_main_menu_overlay_is_active(returned))
        clean = startup_screen_probe_classification(
            ocr_payload={"ok": True, "lines": ["HEAD", "Move:", "Safe:"]},
            capture_warnings=[],
            debug_delta_text=returned,
        )
        self.assertTrue(clean["gameplay_hud_present"])

    def test_final_startup_capture_reads_native_modal_before_classifying_hud(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            debug_artifact = run_dir / "debug.final.log"
            debug_artifact.write_text(
                "openclaw_harness_ui_trace: component=in_game_main_menu event=open\n",
                encoding="utf-8",
            )
            screen_summary = {"png_path": str(run_dir / "success.png"), "capture_warnings": []}
            with patch.object(startup_harness, "run_screen_ocr", return_value={
                    "ok": True, "lines": ["HEAD", "Move:", "Safe:"]}), \
                    patch.object(startup_harness, "capture_stable_final_debug_delta", return_value={
                        "artifact_path": str(debug_artifact), "error_evidence_lines": [],
                        "tail_classified": True, "current_size": 1, "raw_current_size": 1,
                        "current_identity": {},
                    }):
                result = startup_harness.capture_final_startup_evidence(
                    profile="ignored", debug_start_size=0, run_dir=run_dir,
                    screen_summary=screen_summary, label="success", serial=1,
                )

            self.assertFalse(result["screen_probe"]["gameplay_hud_present"])
            self.assertIn("native_main_menu_overlay", result["screen_probe"]["blocking_overlay_markers"])


if __name__ == "__main__":
    unittest.main()
