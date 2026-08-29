#!/usr/bin/env python3
"""Focused proof for the run-only semantic interruption broker."""

from __future__ import annotations

import unittest
from unittest.mock import patch
import tempfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from semantic_broker import SemanticInterruptionBroker
import startup_harness


class SemanticBrokerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.broker = SemanticInterruptionBroker(max_attempts=2)

    def context(self, **extra):
        value = {
            "instance_id": "ui-7", "intent": "hostile_auto_move",
            "valid_actions": ["cancel"], "postcondition": "travel_idle",
            "destination": "camp-3",
        }
        value.update(extra)
        return value

    def recover(self, context, progress=True, **kwargs):
        sent = []
        result = self.broker.recover(
            expected_intent="hostile_auto_move", expected_instance_id="ui-7",
            action="cancel", read_context=lambda: context,
            send_action=lambda identity, action: sent.append((identity, action)),
            observe_progress=lambda postcondition: progress, expected_destination="camp-3",
            **kwargs,
        )
        return result, sent

    def test_expected_ui_receives_advertised_action_and_postcondition(self):
        result, sent = self.recover(self.context())
        self.assertEqual(result.status, "recovered")
        self.assertEqual(sent, [("ui-7", "cancel")])

    def test_unknown_or_stale_identity_receives_no_input(self):
        for context, reason in ((None, "unknown_ui"), (self.context(instance_id="ui-old"), "stale_ui_identity")):
            result, sent = self.recover(context)
            self.assertEqual(result.reason, reason)
            self.assertEqual(sent, [])

    def test_accidental_ui_may_only_take_current_advertised_action(self):
        result, sent = self.recover(self.context(valid_actions=["ignore"]))
        self.assertEqual(result.reason, "action_not_advertised")
        self.assertEqual(sent, [])

    def test_wrong_destination_and_unknown_modal_fail_closed(self):
        result, sent = self.recover(self.context(destination="other-camp"))
        self.assertEqual(result.reason, "wrong_destination")
        self.assertEqual(sent, [])
        result, sent = self.recover(self.context(intent="unknown_modal"))
        self.assertEqual(result.reason, "wrong_ui_intent")
        self.assertEqual(sent, [])

    def test_recovery_requires_progress_and_is_bounded(self):
        result, sent = self.recover(self.context(), progress=False)
        self.assertEqual(result.reason, "recovery_without_progress")
        self.assertEqual(result.attempts, 2)
        self.assertEqual(len(sent), 2)

    def test_screen_text_cannot_change_decision(self):
        results = []
        for ocr in ("cancel auto move", "contradictory text", "", "success", "failure"):
            result, _ = self.recover(self.context(irrelevant_screen_text=ocr))
            results.append(result.as_dict())
        self.assertEqual(results, [results[0]] * len(results))

    def test_live_acknowledger_uses_semantic_trace_and_ignores_contradictory_ocr(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            trace = run_dir / "ui.trace"
            trace.write_text(
                'openclaw_harness_ui_trace: component=semantic_ui event=open '
                'instance_id="ui-7" intent="hostile_auto_move" '
                'valid_actions=["cancel"] postcondition="travel_idle" destination="camp-3"\n',
                encoding="utf-8",
            )

            def send(_pid, _keys, delay_ms=200):
                with trace.open("a", encoding="utf-8") as handle:
                    handle.write(
                        'openclaw_harness_ui_trace: component=semantic_ui event=progress '
                        'instance_id="ui-7" intent="hostile_auto_move" '
                        'valid_actions=["cancel"] postcondition="travel_idle" destination="camp-3"\n'
                    )

            with patch.object(startup_harness, "capture_screenshot", return_value={"ok": True}), \
                    patch.object(startup_harness, "capture_screen_text_artifact",
                                 return_value={"ok": True, "text": "contradictory OCR"}), \
                    patch.object(startup_harness, "peekaboo_press_sequence", side_effect=send):
                result = startup_harness.acknowledge_blocking_interruptions(
                    123, run_dir, "semantic", settle_seconds=0,
                    semantic_ui_trace_log=trace,
                    semantic_ui_expectation={
                        "instance_id": "ui-7", "intent": "hostile_auto_move",
                        "action": "cancel", "destination": "camp-3",
                    },
                )
            self.assertEqual(result["status"], "semantic_recovered")
            self.assertEqual(result["final_classification"]["provenance"], "semantic_ui_trace")
            self.assertEqual(result["acknowledgements"][0]["response_key"], "cancel")

    def test_empty_semantic_trace_does_not_block_ordinary_clear_screen(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            trace = run_dir / "ui.trace"
            trace.write_bytes(b"")
            with patch.object(startup_harness, "capture_screenshot", return_value={"ok": True}), \
                    patch.object(startup_harness, "capture_screen_text_artifact",
                                 return_value={"ok": True, "text": "Move: 100"}), \
                    patch.object(startup_harness, "peekaboo_press_sequence") as press:
                result = startup_harness.acknowledge_blocking_interruptions(
                    123, run_dir, "empty_semantic", settle_seconds=0,
                    semantic_ui_trace_log=trace,
                    semantic_ui_expectation={"intent": "eoc_popup", "action": "space"},
                )
            self.assertEqual(result["status"], "clear")
            press.assert_not_called()

    def test_production_advance_turns_caller_binds_semantic_trace_and_expectation(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            trace = run_dir / "run.log"
            trace.write_bytes(b"")
            with patch.object(startup_harness, "peekaboo_press_sequence"), \
                    patch.object(startup_harness, "acknowledge_blocking_interruptions",
                                 return_value={"status": "clear", "acknowledgement_count": 1}) as acknowledge:
                startup_harness.advance_turns(
                    123, 1, run_dir=run_dir, action_trace_log=trace,
                    max_acknowledgements=1,
                )
            kwargs = acknowledge.call_args.kwargs
            self.assertIs(kwargs["semantic_ui_trace_log"], trace)
            self.assertEqual(kwargs["semantic_ui_trace_start_offset"], 0)
            self.assertEqual(kwargs["semantic_ui_expectation"], {"intent": "eoc_popup", "action": "space"})

    def test_adaptive_eoc_recovery_requires_the_declared_live_modal(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            trace = run_dir / "run.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=semantic_ui event=open '
                'instance_id="eoc-1" intent="eoc_popup" valid_actions=["space"] '
                'postcondition="eoc_popup_returned"\n', encoding="utf-8",
            )
            declaration = {
                "intent": "eoc_popup", "action": "space",
                "postcondition": "eoc_popup_returned",
            }
            with patch.object(startup_harness, "acknowledge_blocking_interruptions",
                              return_value={"status": "semantic_recovered"}) as acknowledge:
                recovered = startup_harness.recover_adaptive_semantic_ui_interruption(
                    pid=123, run_dir=run_dir, action_trace_log=trace,
                    trace_start_offset=0, recovery=declaration, delay_ms=1,
                )

            self.assertEqual(recovered["status"], "recovered")
            self.assertEqual(
                acknowledge.call_args.kwargs["semantic_ui_expectation"],
                {"instance_id": "eoc-1", "intent": "eoc_popup", "action": "space"},
            )

    def test_adaptive_eoc_recovery_rejects_an_unexpected_live_modal(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            trace = run_dir / "run.log"
            trace.write_text(
                'openclaw_harness_ui_trace: component=semantic_ui event=open '
                'instance_id="eoc-1" intent="eoc_popup" valid_actions=["space"] '
                'postcondition="eoc_popup_returned"\n', encoding="utf-8",
            )
            with patch.object(startup_harness, "acknowledge_blocking_interruptions") as acknowledge:
                result = startup_harness.recover_adaptive_semantic_ui_interruption(
                    pid=123, run_dir=run_dir, action_trace_log=trace,
                    trace_start_offset=0,
                    recovery={
                        "intent": "eoc_popup", "action": "space",
                        "postcondition": "other_postcondition",
                    }, delay_ms=1,
                )

            self.assertEqual(result["status"], "blocked_unexpected_semantic_ui")
            acknowledge.assert_not_called()


if __name__ == "__main__":
    unittest.main()
