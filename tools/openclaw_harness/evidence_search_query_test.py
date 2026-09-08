from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from evidence_search_index import EvidenceIndex
from evidence_search_query import EvidenceSearch


class EvidenceSearchQueryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "events.jsonl"
        rows = [
            {"event": "text", "run_id": "run-a", "actor_id": "player", "feature": "crafting", "text": "payment completed and peace held"},
            {"event": "text", "run_id": "run-b", "actor_id": "npc", "text": "payment failed; attack followed"},
            {"event": "text", "run_id": "run-a", "text": "a quiet weather camp note"},
        ]
        self.path.write_bytes(b"".join((json.dumps(row) + "\n").encode() for row in rows))
        self.index = EvidenceIndex(self.root / "index.sqlite3", model_id="m", model_version="1")
        self.addCleanup(self.index.close)
        self.index.ingest(self.path)
        self.search = EvidenceSearch(self.index)

    def test_filters_paging_and_verified_expansion(self):
        found = self.search.query("payment peace", filters={"run_id": "run-a"}, limit=1, expand=1)
        self.assertEqual(found["status"], "matched")
        self.assertEqual(found["matched"], 1)
        self.assertIn("payment completed", found["rows"][0]["excerpt"])
        self.assertEqual(len(found["rows"][0]["context"]), 2)
        self.assertIsNone(found["next_offset"])

    def test_changed_source_is_not_served(self):
        found = self.search.query("payment", limit=5)
        self.assertEqual(found["matched"], 2)
        self.path.write_bytes(self.path.read_bytes().replace(b"payment completed", b"tampered completed"))
        changed = self.search.query("payment", limit=5)
        self.assertEqual(changed["status"], "degraded")
        self.assertEqual(len(changed["unavailable"]), 3)

    def test_exact_fallback_and_no_match(self):
        result = self.search.query("attack followed", filters={"run_id": "run-b"})
        self.assertEqual(result["rows"][0]["match_reason"], "exact text")
        self.assertEqual(self.search.query("does not exist")["status"], "no_match")

    def test_null_identity_filter_and_unavailable_semantic_backend(self):
        found = self.search.query("weather", filters={"actor_id": None})
        self.assertEqual(found["rows"][0]["bindings"]["run_id"], "run-a")  # null actor is explicit

        class Down:
            def embed(self, values):
                raise RuntimeError("embedding_backend_unavailable")
        degraded = EvidenceSearch(self.index, Down()).query("payment")
        self.assertEqual(degraded["status"], "partial")
        self.assertFalse(degraded["semantic_backend"]["available"])
        self.assertIn("embedding_backend_unavailable", degraded["semantic_backend"]["errors"])

    def test_original_record_filter_is_applied_after_occurrence_prefilter(self):
        found = self.search.query("payment", filters={"feature": "crafting"})
        self.assertEqual(found["status"], "matched")
        self.assertEqual(found["matched"], 1)
        self.assertIn("payment completed", found["rows"][0]["excerpt"])

        absent = self.search.query("payment", filters={"feature": "missing"})
        self.assertEqual(absent["status"], "no_match")
        self.assertEqual(absent["matched"], 0)


if __name__ == "__main__":
    unittest.main()
