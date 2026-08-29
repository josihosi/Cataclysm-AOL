import tempfile
import unittest
from pathlib import Path

from certification_route import evaluate_continuous_certification, write_immutable_certification_report


class ContinuousCertificationRouteTest(unittest.TestCase):
    def events(self):
        kinds = ("declared_world", "departure", "overmap_advance", "bubble_crossing_out",
                 "actor_outcomes", "save", "quit", "relaunch", "bubble_crossing_in",
                 "return_report", "camp_decision")
        return [{"kind": kind, "round_id": "r", "binding_id": "b", "world_id": "w",
                 "player_id": "p", "actor_ids": ["a", "c"], "owner": "abstract"}
                for kind in kinds]

    def test_complete_round_is_green(self):
        result = evaluate_continuous_certification(round_id="r", binding_id="b", world_id="w",
                                                   player_id="p", actor_ids=["c", "a"], events=self.events())
        self.assertEqual(result["status"], "green")

    def test_identity_drift_stops_at_first_divergence(self):
        events = self.events(); events[7] = dict(events[7], binding_id="replacement")
        result = evaluate_continuous_certification(round_id="r", binding_id="b", world_id="w",
                                                   player_id="p", actor_ids=["a", "c"], events=events)
        self.assertEqual((result["status"], result["first_divergence"]), ("red", "relaunch"))

    def test_forbidden_replay_cannot_count(self):
        events = self.events(); events[0] = dict(events[0], route="diagnostic_replay")
        result = evaluate_continuous_certification(round_id="r", binding_id="b", world_id="w",
                                                   player_id="p", actor_ids=["a", "c"], events=events)
        self.assertEqual((result["status"], result["first_divergence"]), ("red", "diagnostic_replay"))

    def test_report_is_atomic_and_immutable(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "certification.report.json"
            result = {"status": "green", "round_id": "r"}
            digest = write_immutable_certification_report(path, result)
            self.assertEqual(write_immutable_certification_report(path, result), digest)
            with self.assertRaisesRegex(ValueError, "immutable"):
                write_immutable_certification_report(path, {"status": "red"})


if __name__ == "__main__":
    unittest.main()
