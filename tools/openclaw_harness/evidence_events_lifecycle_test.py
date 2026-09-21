"""Focused lifecycle scoping and growth controls for exact evidence queries."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import evidence_display
import evidence_events


class EvidenceEventsLifecycleTest(unittest.TestCase):
    def query(self, directory, records, filters):
        source = directory / "events.jsonl"
        source.write_text("".join(json.dumps(record) + "\n" for record in records))
        with patch.object(
                evidence_events, "retain",
                lambda value: evidence_display.retain(value, directory / "retained")):
            return evidence_events.query(
                [{"path": str(source), "producer": "synthetic-native"}],
                filters,
                limit=1,
            )

    def test_exact_request_scope_is_constant_under_unrelated_history_growth(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            base = [
                {"event": "accepted", "accepted": True, "run_id": "run",
                 "process_instance": "proc", "request_id": "wanted"},
                {"event": "completed", "run_id": "run", "process_instance": "proc",
                 "request_id": "wanted"},
                {"event": "accepted", "accepted": True, "run_id": "run",
                 "process_instance": "proc", "request_id": "adjacent"},
                # Same request is a different lifecycle identity when its
                # process instance differs.
                {"event": "outcome", "outcome": "accepted", "run_id": "run",
                 "process_instance": "other-proc", "request_id": "wanted"},
            ]
            exact_filters = {"request_id": "wanted", "process_instance": "proc"}
            first = self.query(directory, base, exact_filters)
            grown = self.query(
                directory,
                base + [{"event": "completed", "run_id": "unrelated-run",
                         "process_instance": "proc", "request_id": f"noise-{i}"}
                        for i in range(100)],
                exact_filters,
            )
            self.assertEqual(first["matched"], grown["matched"])
            self.assertEqual([link["request_id"] for link in first["links"]], ["wanted"])
            self.assertEqual(first["links"], grown["links"])
            link = first["links"][0]
            self.assertEqual(link["status"], "complete")
            self.assertEqual(link["outcome"], "accepted")
            self.assertEqual(link["missing"], [])
            self.assertGreater(grown["scanned_records"], first["scanned_records"])
            self.assertEqual(grown["displayed_link_bytes"], first["displayed_link_bytes"])

    def test_outcome_is_observed_not_inferred_from_absence(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            cases = [
                ([{"event": "rejection", "accepted": False, "rejection_reason": "stale",
                   "run_id": "run", "process_instance": "proc", "request_id": "rejected"}],
                 "rejected", "complete", []),
                ([{"event": "completed", "run_id": "run", "process_instance": "proc",
                   "request_id": "unknown"}],
                 "unknown", "partial", ["acceptance", "rejection"]),
                ([{"event": "accepted", "accepted": True, "run_id": "run",
                   "process_instance": "proc", "request_id": "contradictory"},
                  {"event": "outcome", "outcome": "rejected", "run_id": "run",
                   "process_instance": "proc", "request_id": "contradictory"}],
                 "contradictory", "contradictory", []),
            ]
            for records, outcome, status, missing in cases:
                result = self.query(directory, records, {"request_id": records[0]["request_id"]})
                link = result["links"][0]
                self.assertEqual(link["outcome"], outcome)
                self.assertEqual(link["status"], status)
                self.assertEqual(link["missing"], missing)


if __name__ == "__main__":
    unittest.main()
