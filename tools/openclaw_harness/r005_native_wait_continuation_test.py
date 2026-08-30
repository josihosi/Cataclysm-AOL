#!/usr/bin/env python3
"""Focused controls for the R-005 native-wait return continuation."""

from __future__ import annotations

import sys
import json
import tempfile
import unittest
from pathlib import Path
from typing import Any, Dict, List, Optional
from unittest.mock import patch

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from startup_harness import (  # noqa: E402
    StructuredTransitionEventReader,
    evaluate_native_wait_return_continuation,
    execute_long_wait_action,
)


class NativeWaitReturnContinuationTest(unittest.TestCase):
    run_id = "run-r005"

    def event(
        self,
        sequence: int,
        actor_id: int,
        *,
        run_id: Optional[str] = None,
        owner: Optional[str] = "local",
        new_state: Optional[str] = "at_home",
    ) -> Dict[str, Any]:
        event: Dict[str, Any] = {
            "schema_version": 1,
            "sequence": sequence,
            "run_id": run_id or self.run_id,
            "game_minutes": 8240,
            "domain": "bandit_live_world",
            "transition": "structural_member_physical_return",
            "outcome": "committed",
            "actor_ids": [actor_id],
            "new_state": new_state,
            "simulation_owner": owner,
            "handoff_epoch": 1 if actor_id == 4 else 2,
        }
        return event

    def evaluate(self, events: List[Dict[str, Any]]) -> Dict[str, Any]:
        return evaluate_native_wait_return_continuation(
            events,
            run_id=self.run_id,
            transition="structural_member_physical_return",
            actor_ids=[4, 5],
            postcondition={"new_state": "at_home"},
            final_postcondition={"simulation_owner": "abstract", "handoff_epoch": 2},
        )

    def test_accepts_exact_same_run_pair_and_final_abstract_owner(self) -> None:
        result = self.evaluate([self.event(4, 4), self.event(5, 5, owner="abstract")])

        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["matched_actor_ids"], [4, 5])
        self.assertEqual(result["receipt_sequences"], [4, 5])

    def test_rejects_stale_or_wrong_run_receipt(self) -> None:
        result = self.evaluate([
            self.event(4, 4, run_id="old-run"),
            self.event(5, 5, owner="abstract"),
        ])

        self.assertEqual(result["status"], "unproved")
        self.assertIn("wrong_or_stale_run", result["failures"])

    def test_rejects_missing_actor_or_owner(self) -> None:
        missing_actor = self.evaluate([self.event(4, 4)])
        missing_owner = self.evaluate([self.event(4, 4), self.event(5, 5, owner=None)])

        self.assertIn("missing_return_actor", missing_actor["failures"])
        self.assertIn("missing_return_owner", missing_owner["failures"])

    def test_rejects_duplicate_receipt(self) -> None:
        result = self.evaluate([
            self.event(4, 4),
            self.event(5, 4),
            self.event(6, 5, owner="abstract"),
        ])

        self.assertEqual(result["status"], "unproved")
        self.assertIn("duplicate_return_receipt", result["failures"])

    def test_rejects_non_monotone_sequence(self) -> None:
        result = self.evaluate([self.event(5, 5, owner="abstract"), self.event(4, 4)])

        self.assertEqual(result["status"], "unproved")
        self.assertIn("non_monotone_sequence", result["failures"])

    def test_rejects_missing_or_mismatched_postcondition(self) -> None:
        missing = self.evaluate([
            self.event(4, 4, new_state=None),
            self.event(5, 5, owner="abstract"),
        ])
        mismatched = self.evaluate([
            self.event(4, 4),
            self.event(5, 5, owner="abstract", new_state="local"),
        ])

        self.assertIn("missing_return_postcondition", missing["failures"])
        self.assertIn("return_postcondition_mismatch", mismatched["failures"])

    def test_long_wait_uses_native_receipt_continuation_without_finish_ocr(self) -> None:
        events = [
            {**self.event(1, 4), "schema_version": 1},
            {**self.event(2, 5, owner="abstract"), "schema_version": 1},
        ]
        with tempfile.TemporaryDirectory() as temporary_directory:
            run_dir = Path(temporary_directory)
            stream = run_dir / "transition.events.jsonl"
            stream.write_text(
                "".join(json.dumps(event) + "\n" for event in events),
                encoding="utf-8",
            )
            reader = StructuredTransitionEventReader(stream, self.run_id)
            with patch("startup_harness.peekaboo_press_sequence"), \
                    patch("startup_harness.capture_screenshot", return_value={"screen_summary": {}}), \
                    patch("startup_harness.capture_screen_text_artifact", return_value={"ok": True, "text": "HUD"}), \
                    patch("startup_harness.sample_child_resources", return_value={}), \
                    patch("startup_harness.time.sleep"):
                result = execute_long_wait_action(
                    42,
                    run_dir,
                    "return_followup",
                    {
                        "choice_key": "3",
                        "native_wait_continuation": {
                            "transition": "structural_member_physical_return",
                            "actor_ids": [4, 5],
                            "postcondition": {"new_state": "at_home"},
                            "final_postcondition": {
                                "simulation_owner": "abstract",
                                "handoff_epoch": 2,
                            },
                        },
                        "menu_settle_seconds": -1,
                        "pre_menu_settle_seconds": -1,
                        "after_choice_settle_seconds": -1,
                        "completion_wait_seconds": 8,
                    },
                    structured_event_reader=reader,
                    semantic_run_id=self.run_id,
                )

        self.assertEqual(result["native_wait_continuation"]["status"], "matched")
        self.assertEqual(result["completion_artifact_poll"]["status"], "matched_native_receipt_continuation")
        self.assertEqual(result["wait_step_ledger"]["verdict"], "green_wait_step_proven")
        self.assertEqual(result["wait_classification"]["completion_signal"], "same_run_native_return_receipts_and_postcondition")
        self.assertNotIn("abort", result)


if __name__ == "__main__":
    unittest.main()
