"""Focused regressions for matrix-discovered presentation and retrieval issues."""
import copy
from pathlib import Path
import unittest
from unittest.mock import patch

from gameplay_display import plain_player_output
from cockpit_file_bridge import FileBackedCockpitBridge as Bridge
from playtest_witness import WitnessError, validate_witness_statement
from playtest_witness_test import CHARTER, journal, statement


class PresentationCleanupTest(unittest.TestCase):
    def test_performance_keeps_measurements_alarm_and_uncertainty_without_receipts(self):
        result = {"ok": True, "latest": {"record_id": "opaque-record", "interpretation": "boilerplate",
            "resources": {"interval_cpu_seconds": .2, "interval_wall_seconds": 1,
                          "resident_memory": {"value": 104857600}}},
            "comparison": {"status": "unavailable", "reason": "no baseline selected"},
            "turn_assessment": {"status": "measured_unassessed", "reason": "missing configuration",
                "metric": {"mean_seconds": .144, "max_seconds": .2, "sample_count": 5,
                           "game_turns_per_simulation_second": 6.9},
                "alarms": [{"kind": "waiting_slow", "mean_seconds": .144,
                            "limit_seconds": .1, "sample_count": 5}],
                "observation_uncertainty": "No comparable baseline"}}
        original = copy.deepcopy(result)
        text = plain_player_output(result)
        for expected in ("144.0 ms/turn", "100 ms", "Tell the coordinator", "Memory: 100.0 MiB",
                         "CPU: 0.2 s", "6.9 turns/s", "missing configuration", "No comparable baseline"):
            self.assertIn(expected, text)
        for unwanted in ("opaque-record", "boilerplate", "comparison.status"):
            self.assertNotIn(unwanted, text)
        self.assertEqual(result, original)

    def test_unavailable_performance_does_not_claim_zero_or_alarm_off(self):
        text = plain_player_output({"latest": None, "turn_assessment": {
            "status": "unavailable", "reason": "trace missing"}})
        self.assertIn("trace missing", text)
        self.assertIn("No process sample", text)
        self.assertNotIn("0 ms", text)
        self.assertNotIn("Off", text)

    def test_repeated_journal_fields_share_description_but_retain_indices(self):
        entries = [{"citation_id": f"J{i}", "kind": "observation", "value": {
            "surface": {"facts": {"avatar": {}, "messages": []}}}} for i in range(2)]
        text = plain_player_output({"selector": "result.evidence_journal.entries", "slice": entries})
        self.assertEqual(text.count("FIELD: avatar, messages"), 1)
        self.assertIn("INDEX 1; same PATH/FIELD as J0", text)
        self.assertIn("INDEX 0; PATH surface.facts.FIELD", text)

    def test_zone_summary_preserves_bounds_state_and_controls(self):
        zone = {"id": "zone-6", "name": "Storage", "type": "LOOT", "enabled": False,
                "selected": True, "start": [1, 2, 0], "end": [3, 4, 0], "faction": "your_followers"}
        snapshot = {"owner": "zone_manager", "current": {"facts": {"zones": [zone]},
                    "actions": [{"id": "zone.enable", "stable_id": "zone-6", "label": "Enable", "enabled": True}]}}
        text = plain_player_output({"state": "collected", "response": {
            "current_input": {"owner": "zone_manager"}}}, snapshot=snapshot)
        for expected in ("zone-6 — Storage; off; selected", "start", "end", "your_followers", "enable"):
            self.assertIn(expected, text)
        self.assertNotIn("play act zone-6", text)

    def test_inspection_failure_explains_selector_and_supported_filter(self):
        with patch.object(Bridge, "response_status", return_value={"ok": True, "receipt": {"response_sha256": "x"}}), \
             patch.object(Bridge, "response_artifact", return_value={"ok": True, "response": {"gear": {"7": {"name": "Shirt"}}}}):
            failed = Bridge.response_slice(Path("unused"), "r", "gear", contains="shirt")
            self.assertFalse(failed["ok"])
            self.assertEqual(failed["selector"], "gear")
            self.assertIn("Remove --contains", plain_player_output(failed))
            self.assertEqual(Bridge.response_slice(Path("unused"), "r", "gear.7")["slice"]["name"], "Shirt")
            missing = Bridge.response_slice(Path("unused"), "r", "performance-turn-state")
            self.assertIn("play performance", plain_player_output(missing))

    def test_missing_witness_path_identifies_citation_and_field(self):
        value = statement()
        value["citations"][0]["checks"] = {"missing_field": True}
        with self.assertRaisesRegex(WitnessError, "witness_citation_path_missing:J0002:missing_field"):
            validate_witness_statement(charter=CHARTER, journal=journal(), statement=value)


if __name__ == "__main__":
    unittest.main()
