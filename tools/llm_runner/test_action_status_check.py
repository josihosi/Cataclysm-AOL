#!/usr/bin/env python3
"""Focused contracts for the structured action-status log format."""

from __future__ import annotations

import json
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path

RUNNER_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(RUNNER_DIR))

from action_status_check import load_action_status_events, parse_action_status_line  # noqa: E402


class ActionStatusParsingTest(unittest.TestCase):
    def test_json_escaped_fields_round_trip(self) -> None:
        line = (
            r'[CAOL_EVENT] action_status npc="Ada \"Ace\"\\Scout\nNorth" '
            r'kind="look_around_pickup" phase="blocked" reason="pickup.no_path" '
            r'request="req\\\"42" target_hint="crate\\north\nshelf" '
            r'target="red \"tool\"\\box" facts="path=C:\\tmp; note=first\r\nsecond\tline"'
        )

        event = parse_action_status_line(line)

        self.assertIsNotNone(event)
        assert event is not None
        self.assertEqual(event.npc, 'Ada "Ace"\\Scout\nNorth')
        self.assertEqual(event.kind, "look_around_pickup")
        self.assertEqual(event.phase, "blocked")
        self.assertEqual(event.reason, "pickup.no_path")
        self.assertEqual(event.request, 'req\\"42')
        self.assertEqual(event.target_hint, "crate\\north\nshelf")
        self.assertEqual(event.target, 'red "tool"\\box')
        self.assertEqual(event.facts, "path=C:\\tmp; note=first\r\nsecond\tline")

    def test_malformed_json_escape_is_not_accepted_as_an_event(self) -> None:
        line = (
            r'[CAOL_EVENT] action_status npc="Ada\q" kind="look_inventory" '
            r'phase="completed" reason="" request="req_1" target_hint="" target="" facts=""'
        )

        self.assertIsNone(parse_action_status_line(line))

    def test_unframed_or_embedded_status_text_cannot_impersonate_an_event(self) -> None:
        unframed = (
            r'action_status npc="Ada" kind="look_inventory" phase="completed" '
            r'reason="" request="req_1" target_hint="" target="" facts=""'
        )
        embedded = "model response: " + "[CAOL_EVENT] " + unframed

        self.assertIsNone(parse_action_status_line(unframed))
        self.assertIsNone(parse_action_status_line(embedded))

    def test_byte_offset_excludes_stale_events_from_an_earlier_run(self) -> None:
        old_event = (
            '[CAOL_EVENT] action_status npc="Old" kind="look_inventory" phase="completed" '
            'reason="" request="req_0" target_hint="" target="" facts=""\n'
        )
        new_event = old_event.replace('npc="Old"', 'npc="Current"')

        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "llm_intent_events.log"
            log_path.write_text(old_event, encoding="utf-8")
            baseline = log_path.stat().st_size
            with log_path.open("a", encoding="utf-8") as stream:
                stream.write(new_event)

            events = load_action_status_events(log_path, baseline)

        self.assertEqual([event.npc for event in events], ["Current"])

    def test_cli_requires_dedicated_event_log_name(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "ordinary_model_output.log"
            log_path.write_text("", encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(RUNNER_DIR / "action_status_check.py"),
                    "--log-file",
                    str(log_path),
                    "--after-byte-offset",
                    "0",
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        self.assertEqual(result.returncode, 2)
        self.assertIn("Refusing untrusted log source", result.stderr)

    def test_cli_requires_same_run_byte_offset(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "llm_intent_events.log"
            log_path.write_text("", encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(RUNNER_DIR / "action_status_check.py"),
                    "--log-file",
                    str(log_path),
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        self.assertEqual(result.returncode, 2)
        self.assertIn("--after-byte-offset is required", result.stderr)

    def test_cli_rejects_empty_same_run_event_slice_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "llm_intent_events.log"
            log_path.write_text("", encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(RUNNER_DIR / "action_status_check.py"),
                    "--log-file",
                    str(log_path),
                    "--after-byte-offset",
                    "0",
                ],
                capture_output=True,
                text=True,
                check=False,
            )

        self.assertEqual(result.returncode, 1)
        self.assertIn("expected at least 1 matched events, got 0", result.stdout)

    def test_cli_rejects_malformed_expectation_types(self) -> None:
        event = (
            '[CAOL_EVENT] action_status npc="Ada" kind="look_inventory" phase="blocked" '
            'reason="inventory.empty" request="req_1" target_hint="" target="" facts=""\n'
        )
        malformed_cases = [
            ({"phase_any": "blocked"}, "phase_any must be a list of non-empty strings"),
            ({"phase_sequence": ["blocked", 7]}, "phase_sequence must be a list of non-empty strings"),
            ({"reason_any": [""]}, "reason_any must be a list of non-empty strings"),
            ({"terminal_phase": 7}, "terminal_phase must be a non-empty string"),
        ]

        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "llm_intent_events.log"
            expect_path = Path(temp_dir) / "expect.json"
            log_path.write_text(event, encoding="utf-8")
            for malformed, expected_error in malformed_cases:
                with self.subTest(expectation=malformed):
                    expect_path.write_text(json.dumps(malformed), encoding="utf-8")
                    result = subprocess.run(
                        [
                            sys.executable,
                            str(RUNNER_DIR / "action_status_check.py"),
                            "--log-file",
                            str(log_path),
                            "--after-byte-offset",
                            "0",
                            "--expect-file",
                            str(expect_path),
                        ],
                        capture_output=True,
                        text=True,
                        check=False,
                    )

                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(expected_error, result.stderr)


if __name__ == "__main__":
    unittest.main()
