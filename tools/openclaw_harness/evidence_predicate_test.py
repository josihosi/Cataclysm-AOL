"""Bounded predicates preserve original rows and missing-source uncertainty."""
import base64
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from evidence_events import query, parse_predicate
from evidence_display import recover, exact_select
from gameplay_display import _plain_evidence_output


class EvidencePredicateTest(unittest.TestCase):
    def test_boundaries_identity_raw_expansion_missing_fields_and_source(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "native.jsonl"
            records = [{"event": "actor_state", "game_turn": turn, "actor_id": actor,
                        "ready": ready} for turn, actor, ready in
                       [(9, 4, True), (10, 4, False), (11, 4, True), (12, 5, True), (13, 4, True)]]
            records += [{"event": "actor_state", "actor_id": 4},
                        {"event": "actor_state", "game_turn": None, "actor_id": 4}]
            raw = [json.dumps(row).encode() + b"\n" for row in records]
            path.write_bytes(b"".join(raw))
            result = query([{"path": str(path)}, {"path": str(path.with_name("absent"))}],
                           {"actor_id": 4}, selectors=("game_time.turn", "payload.ready", "payload.absent"),
                           predicates=[parse_predicate("payload.ready!=false")], from_turn=10, to_turn=12)
            self.assertEqual(result["status"], "partial")
            self.assertEqual(result["matched"], 1)
            row = result["rows"][0]
            self.assertEqual(row["fields"]["game_time.turn"], 11)
            self.assertEqual(row["fields"]["payload.absent"], {"unavailable": True})
            source = row["source"]
            self.assertEqual(source["sha256"], hashlib.sha256(raw[2]).hexdigest())
            self.assertEqual(path.read_bytes()[source["offset"]:source["offset"]+source["length"]], raw[2])
            retained = recover(source["retained_raw"]["sha256"])
            self.assertEqual(base64.b64decode(retained["raw_base64"]), raw[2])
            self.assertEqual(query([{"path": str(path)}], {"actor_id": 4},
                                   from_turn=10, to_turn=10)["matched"], 1)
            plain = _plain_evidence_output(result)
            self.assertIn(row["event_id"][:16], plain)
            self.assertIn("/rows/INDEX/fields/FIELD", plain)
            self.assertEqual(exact_select(recover(result["snapshot"]["sha256"]), "/rows/0/fields/game_time.turn"), 11)
            self.assertIn("Unavailable sources: 1", plain)

    def test_missing_or_null_is_not_a_negative_state(self):
        from evidence_events import matches_predicate
        for event in ({"payload": {}}, {"payload": {"ready": None}}):
            self.assertFalse(matches_predicate(event, parse_predicate("payload.ready!=true")))
        self.assertFalse(matches_predicate({"payload": {"ready": True}}, parse_predicate("payload.ready=1")))
        self.assertFalse(matches_predicate({"game_time": {"turn": True}}, parse_predicate("game_time.turn>=1")))

    def test_invalid_or_reversed_range(self):
        for expression in ("field", "field=not-json", "field;rm=1"):
            with self.assertRaises(ValueError):
                parse_predicate(expression)
        with self.assertRaises(ValueError):
            query([], {}, from_turn=12, to_turn=10)

    def test_boolean_numeric_equality_is_symmetric(self):
        from evidence_events import matches_predicate
        for boolean, number in ((True, 1), (False, 0), (True, 1.0), (False, 0.0)):
            for actual, expected in ((boolean, number), (number, boolean)):
                event = {"payload": {"ready": actual}}
                with self.subTest(actual=actual, expected=expected):
                    self.assertFalse(matches_predicate(event, ("payload.ready", "=", expected)))
                    self.assertTrue(matches_predicate(event, ("payload.ready", "!=", expected)))
        for actual, expected in ((True, True), (False, False), (1, 1.0), (0.0, 0)):
            event = {"payload": {"ready": actual}}
            with self.subTest(actual=actual, expected=expected):
                self.assertTrue(matches_predicate(event, ("payload.ready", "=", expected)))
                self.assertFalse(matches_predicate(event, ("payload.ready", "!=", expected)))


if __name__ == "__main__":
    unittest.main()
