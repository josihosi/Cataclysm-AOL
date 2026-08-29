#!/usr/bin/env python3
"""Focused report-finalization contracts for adaptive semantic windows."""

from __future__ import annotations

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parent))

from startup_harness import (  # noqa: E402
    adaptive_semantic_interrupt_contract,
    adaptive_semantic_materiality_waivers,
    adaptive_semantic_receipt_chain_status,
    build_probe_step_ledger,
    collect_materiality_waivers,
    compact_probe_report_for_stdout,
    finalize_probe_report,
    recover_adaptive_activity_distraction,
)


class AdaptiveSemanticWindowFinalizationTest(unittest.TestCase):
    def receipt(self, action_id: str, frame_id: str, *, accepted: bool = True) -> dict:
        return {
            "accepted": accepted,
            "action_id": action_id,
            "frame_id": frame_id,
            "run_id": "run-1",
            "session_id": "session-1",
            "native_receipt": {"accepted": accepted, "action_id": action_id, "frame_id": frame_id},
            "semantic_response": {"accepted": accepted, "action_id": action_id, "frame_id": frame_id},
            "next_frame": {"frame_id": frame_id + ":next", "state": "world", "valid_actions": []},
        }

    def report(self, *, interruption_proved: bool) -> dict:
        receipts = [
            self.receipt("world.wait", "world"),
            self.receipt("wait.duration_menu", "duration"),
            self.receipt("wait.1m", "minute"),
        ]
        interruption = self.receipt("activity.ignore", "activity", accepted=interruption_proved)
        receipts.append(interruption)
        return {
            "index": 1,
            "label": "adaptive",
            "kind": "adaptive_semantic_window",
            "semantic_progress": {
                "action_chain_proved": True,
                "interruption_receipts_proved": interruption_proved,
                "optional_interrupt_scan_complete": True,
            },
            "semantic_session": {
                "run_id": "run-1",
                "session_id": "session-1",
                "required_action_chain": ["world.wait", "wait.duration_menu", "wait.1m"],
                "required_interrupt_action_chain": ["activity.ignore"],
            },
            "semantic_receipts": receipts,
            "semantic_interruption_receipts": [interruption],
            "next_semantic_frame": {"state": "world"},
        }

    def wait_modal_report(self) -> dict:
        report = self.report(interruption_proved=True)
        receipts = report["semantic_receipts"][:3]
        duration_modal = {
            "frame_id": "wait-duration:4",
            "state": "wait_duration_choice",
            "provenance": "native_semantic_step_trace",
            "valid_actions": ["wait.1m"],
        }
        receipts[1]["next_frame"] = duration_modal
        receipts[2]["frame_id"] = "wait-duration:4"
        receipts[2]["native_receipt"]["frame_id"] = "wait-duration:4"
        receipts[2]["semantic_response"]["frame_id"] = "wait-duration:4"
        receipts[2]["current_frame"] = dict(duration_modal)
        report["semantic_receipts"] = receipts
        report["semantic_interruption_receipts"] = []
        report["semantic_progress"]["interruption_receipts_proved"] = True
        report["semantic_session"]["required_interrupt_action_chain"] = []
        report["semantic_session"]["recovery_contract"] = {
            "issuer_action": "wait.duration_menu",
            "modal_state": "wait_duration_choice",
            "modal_owner": "native_semantic_step_trace",
            "actions": ["wait.1m"],
        }
        return report

    def test_saved_adaptive_receipts_finalize_as_green_step_evidence(self) -> None:
        ledger = build_probe_step_ledger([self.report(interruption_proved=True)])

        self.assertEqual(ledger[0]["verdict"], "green_adaptive_semantic_transaction_proven")
        self.assertEqual(
            ledger[0]["evidence_artifact"],
            "probe.report.json:steps[].semantic_receipts",
        )

    def test_missing_or_rejected_interruption_receipt_fails_closed(self) -> None:
        ledger = build_probe_step_ledger([self.report(interruption_proved=False)])

        self.assertEqual(ledger[0]["verdict"], "red_adaptive_semantic_transaction_unproved")
        self.assertIn("adaptive_semantic_transaction_unproved", ledger[0]["issues"])

    def test_optional_advertised_interruption_does_not_become_a_static_requirement(self) -> None:
        report = self.report(interruption_proved=True)
        report["semantic_session"]["required_interrupt_action_chain"] = []
        report["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
        report["semantic_receipts"] = report["semantic_receipts"][:3]
        report["semantic_interruption_receipts"] = []

        status = adaptive_semantic_receipt_chain_status(report)
        waivers = adaptive_semantic_materiality_waivers(report)

        self.assertTrue(status["proved"])
        self.assertEqual(status["reason"], "accepted_receipt_chain")
        self.assertEqual(len(waivers), 1)
        self.assertEqual(waivers[0]["materiality_decision"], "proved_non_causal")
        self.assertEqual(waivers[0]["original_failure"]["action_id"], "activity.ignore")
        self.assertEqual(waivers[0]["protected_surfaces_affected"], [])
        self.assertEqual(waivers[0]["evidence_effect"], "none")

    def test_adaptive_capability_does_not_default_to_required_occurrence(self) -> None:
        adaptive, required = adaptive_semantic_interrupt_contract({
            "adaptive_interrupt_actions": ["activity.ignore"],
        })

        self.assertEqual(adaptive, ["activity.ignore"])
        self.assertEqual(required, [])

    def test_explicit_required_interruption_remains_required(self) -> None:
        adaptive, required = adaptive_semantic_interrupt_contract({
            "adaptive_interrupt_actions": ["activity.ignore"],
            "required_interrupt_action_chain": ["activity.ignore"],
        })

        self.assertEqual(adaptive, ["activity.ignore"])
        self.assertEqual(required, ["activity.ignore"])

    def test_required_unobserved_interruption_fails_closed(self) -> None:
        report = self.report(interruption_proved=False)
        report["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
        report["semantic_receipts"] = report["semantic_receipts"][:3]
        report["semantic_interruption_receipts"] = []

        status = adaptive_semantic_receipt_chain_status(report)

        self.assertFalse(status["proved"])
        self.assertEqual(status["reason"], "accepted_receipt_missing")
        self.assertEqual(status["missing_actions"], ["activity.ignore"])
        self.assertEqual(adaptive_semantic_materiality_waivers(report), [])

    def test_protected_or_causal_failures_cannot_receive_a_waiver(self) -> None:
        protected_failures = [
            "missing_save_receipt",
            "actor_owner_mismatch",
            "unhandled_interruption",
            "failed_required_postcondition",
            "causal_harness_defect",
        ]
        for failure in protected_failures:
            with self.subTest(failure=failure):
                report = self.report(interruption_proved=True)
                report["semantic_session"]["required_interrupt_action_chain"] = []
                report["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
                report["semantic_receipts"] = report["semantic_receipts"][:3]
                report["abort"] = {"status": failure}

                self.assertEqual(adaptive_semantic_materiality_waivers(report), [])

    def test_missing_fresh_postcondition_cannot_receive_a_waiver(self) -> None:
        report = self.report(interruption_proved=True)
        report["semantic_session"]["required_interrupt_action_chain"] = []
        report["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
        report["semantic_receipts"] = report["semantic_receipts"][:3]
        report["next_semantic_frame"] = {"state": "activity_distraction"}

        self.assertEqual(adaptive_semantic_materiality_waivers(report), [])

    def test_observed_optional_interruption_is_not_recorded_as_absent(self) -> None:
        report = self.report(interruption_proved=True)
        report["semantic_session"]["required_interrupt_action_chain"] = []
        report["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
        report["semantic_receipts"] = report["semantic_receipts"][:3]
        report["native_activity_distraction_interruptions"] = [{
            "status": "recovered",
            "action_id": "activity.ignore",
            "receipt": {"action": "IGNORE"},
        }]

        self.assertEqual(adaptive_semantic_materiality_waivers(report), [])

    def test_final_report_exposes_each_exercised_waiver(self) -> None:
        step = self.report(interruption_proved=True)
        step["semantic_session"]["required_interrupt_action_chain"] = []
        step["semantic_session"]["adaptive_interrupt_actions"] = ["activity.ignore"]
        step["semantic_receipts"] = step["semantic_receipts"][:3]
        step["materiality_waivers"] = adaptive_semantic_materiality_waivers(step)
        report = {"mode": "probe", "steps": [step]}

        self.assertEqual(collect_materiality_waivers(report), step["materiality_waivers"])
        with tempfile.TemporaryDirectory() as root:
            finalize_probe_report(Path(root), report)

        self.assertEqual(report["materiality_waivers"], step["materiality_waivers"])
        compact = compact_probe_report_for_stdout(
            report, run_dir=Path("/tmp/materiality-waiver-test"),
            report_filename="probe.report.json",
        )
        self.assertEqual(compact["materiality_waivers"], step["materiality_waivers"])

    def test_wait_duration_modal_binds_advertised_minute_recovery(self) -> None:
        status = adaptive_semantic_receipt_chain_status(self.wait_modal_report())

        self.assertTrue(status["proved"])
        self.assertEqual(status["recovery_modal_identity"], "wait-duration:4")

    def test_wait_duration_recovery_rejects_a_receipt_from_another_modal(self) -> None:
        report = self.wait_modal_report()
        report["semantic_receipts"][2]["frame_id"] = "stale-modal"
        report["semantic_receipts"][2]["native_receipt"]["frame_id"] = "stale-modal"
        report["semantic_receipts"][2]["semantic_response"]["frame_id"] = "stale-modal"
        report["semantic_receipts"][2]["current_frame"]["frame_id"] = "stale-modal"

        status = adaptive_semantic_receipt_chain_status(report)

        self.assertFalse(status["proved"])
        self.assertEqual(status["reason"], "native_recovery_receipt_unbound")

    def test_finalization_retains_wait_duration_modal_pending_recovery(self) -> None:
        report = self.wait_modal_report()
        report["semantic_receipts"] = report["semantic_receipts"][:2]
        report = {"mode": "probe", "steps": [report]}
        with tempfile.TemporaryDirectory() as root, \
                mock.patch("startup_harness.cleanup_game_process") as cleanup:
            finalize_probe_report(Path(root), report, cleanup_pid=49973)

        cleanup.assert_not_called()
        self.assertEqual(report["cleanup"]["status"], "deferred_pending_adaptive_recovery")
        self.assertEqual(report["cleanup"]["pending_recovery"]["issuing_frame_id"], "wait-duration:4")

    def test_finalization_retains_owned_process_for_advertised_pending_recovery(self) -> None:
        report = self.report(interruption_proved=False)
        report["semantic_receipts"] = report["semantic_receipts"][:-1]
        report["semantic_receipts"][-1]["next_frame"] = {
            "frame_id": "activity:772170",
            "state": "activity_distraction",
            "valid_actions": ["activity.ignore"],
        }
        report = {"mode": "probe", "steps": [report]}
        with tempfile.TemporaryDirectory() as root, \
                mock.patch("startup_harness.cleanup_game_process") as cleanup:
            finalize_probe_report(Path(root), report, cleanup_pid=49971)

        cleanup.assert_not_called()
        self.assertEqual(report["cleanup"]["status"], "deferred_pending_adaptive_recovery")
        self.assertEqual(report["cleanup"]["pending_recovery"]["issuing_frame_id"], "activity:772170")

    def test_finalization_cleans_up_after_accepted_recovery_receipt(self) -> None:
        report = {"mode": "probe", "steps": [self.report(interruption_proved=True)]}
        with tempfile.TemporaryDirectory() as root, \
                mock.patch("startup_harness.cleanup_game_process", return_value={"status": "terminated"}) as cleanup:
            finalize_probe_report(Path(root), report, cleanup_pid=49972)

        cleanup.assert_called_once_with(49972)
        self.assertEqual(report["cleanup"]["status"], "terminated")

    def test_activity_distraction_recovery_requires_matching_native_return(self) -> None:
        active = {
            "event": "open", "type": "withdrawal", "action": "none",
            "truncated": False, "event_offset": 44,
        }
        returned = {
            "event": "return", "type": "withdrawal", "action": "IGNORE",
            "issuing_open_offset": 44,
        }
        with mock.patch("startup_harness.read_active_activity_query_trace", return_value=active), \
                mock.patch("startup_harness.peekaboo_press_sequence") as press, \
                mock.patch("startup_harness.read_latest_activity_query_trace", return_value=returned):
            result = recover_adaptive_activity_distraction(
                pid=49974, action_trace_log=Path("debug.log"), trace_start_offset=10, delay_ms=200,
            )

        self.assertEqual(result["status"], "recovered")
        self.assertEqual(result["action_id"], "activity.ignore")
        press.assert_called_once_with(49974, ["I"], delay_ms=200)

    def test_activity_distraction_rebinds_a_truncated_debug_log_generation(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text(
                'openclaw_harness_semantic_step: {"event":"frame","run_id":"fresh"}\n'
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=withdrawal text="withdrawal" truncated=no action=none\n',
                encoding="utf-8",
            )
            def emit_native_return(*_args: object, **_kwargs: object) -> None:
                with trace.open("a", encoding="utf-8") as handle:
                    handle.write(
                        'openclaw_harness_ui_trace: component=activity_distraction_query '
                        'event=return type=withdrawal text="withdrawal" truncated=no action=IGNORE\n'
                    )

            with mock.patch("startup_harness.peekaboo_press_sequence", side_effect=emit_native_return):
                result = recover_adaptive_activity_distraction(
                    pid=49976, action_trace_log=trace,
                    trace_start_offset=1000000, delay_ms=200,
                )

        self.assertEqual(result["status"], "recovered")
        self.assertLess(result["receipt"]["issuing_open_offset"], 1000000)

    def test_activity_distraction_rejects_stale_cursor_without_current_semantic_generation(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            trace = Path(temp) / "debug.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=activity_distraction_query '
                'event=open type=withdrawal text="withdrawal" truncated=no action=none\n',
                encoding="utf-8",
            )
            with mock.patch("startup_harness.peekaboo_press_sequence") as press:
                result = recover_adaptive_activity_distraction(
                    pid=49977, action_trace_log=trace,
                    trace_start_offset=1000000, delay_ms=200,
                )

        self.assertEqual(result["status"], "blocked_activity_distraction_trace_generation_unbound")
        press.assert_not_called()

    def test_activity_distraction_recovery_fails_closed_on_foreign_return(self) -> None:
        active = {
            "event": "open", "type": "withdrawal", "action": "none",
            "truncated": False, "event_offset": 44,
        }
        returned = {
            "event": "return", "type": "withdrawal", "action": "IGNORE",
            "issuing_open_offset": 45,
        }
        with mock.patch("startup_harness.read_active_activity_query_trace", return_value=active), \
                mock.patch("startup_harness.peekaboo_press_sequence"), \
                mock.patch("startup_harness.read_latest_activity_query_trace", return_value=returned):
            result = recover_adaptive_activity_distraction(
                pid=49975, action_trace_log=Path("debug.log"), trace_start_offset=10, delay_ms=200,
            )

        self.assertEqual(result["status"], "blocked_activity_distraction_return_unproved")


if __name__ == "__main__":
    unittest.main()
