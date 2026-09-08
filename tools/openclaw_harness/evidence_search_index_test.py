"""Focused recovery/provenance checks for the derived evidence index."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_evidence import record_artifact
from evidence_search_index import EvidenceIndex


class EvidenceSearchIndexTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "events.jsonl"

    def index(self, model="local-model", version="v1") -> EvidenceIndex:
        result = EvidenceIndex(self.root / "index.sqlite3", model_id=model, model_version=version)
        self.addCleanup(result.close)
        return result

    @staticmethod
    def line(**extra) -> bytes:
        return (json.dumps({"event": "surface_receipt", "run_id": "run-a", **extra}, sort_keys=True) + "\n").encode()

    def test_append_restart_partial_crash_retry_and_exact_handle(self) -> None:
        first, second = self.line(request_id="one"), self.line(request_id="two")
        self.path.write_bytes(first + b'{"event":"surface_receipt"')
        index = self.index()
        first_ingest = index.ingest(self.path)
        self.assertEqual((first_ingest["ingested_records"], first_ingest["pending_tail_bytes"]), (1, 26))
        self.path.write_bytes(first + second)
        self.assertEqual(index.ingest(self.path)["ingested_records"], 1)
        self.assertEqual(len(index.occurrences()), 2)
        index.close()  # A restarted worker continues from the durable cursor.
        restarted = EvidenceIndex(self.root / "index.sqlite3", model_id="local-model", model_version="v1")
        self.addCleanup(restarted.close)
        self.path.write_bytes(first + second + self.line(request_id="three"))
        self.assertEqual(restarted.ingest(self.path)["ingested_records"], 1)
        before = restarted.occurrences()
        self.path.write_bytes(self.path.read_bytes() + self.line(request_id="four"))
        with self.assertRaisesRegex(RuntimeError, "simulated_ingest_crash"):
            restarted.ingest(self.path, fail_after_records=1)
        self.assertEqual(restarted.occurrences(), before)  # cursor and record commit together
        self.assertEqual(restarted.ingest(self.path)["ingested_records"], 1)
        hit = restarted.occurrences()[0]
        handle = hit["source_handle"]
        original = record_artifact(Path(handle["path"]), handle["offset"], handle["length"], handle["sha256"], [])
        self.assertTrue(original["ok"])
        self.assertEqual(original["record"]["request_id"], "one")

    def test_replacement_rotation_deletion_and_stale_generations(self) -> None:
        old = self.line(request_id="old")
        self.path.write_bytes(old)
        index = self.index()
        index.ingest(self.path)
        self.path.write_bytes(self.line(request_id="replacement"))
        replacement = index.ingest(self.path)
        self.assertEqual(replacement["generation"], 2)
        self.assertEqual([row["request_id"] for row in index.occurrences()], ["replacement"])
        self.assertEqual(len(index.occurrences(include_stale=True)), 2)
        self.path.unlink()
        index.reconcile([])
        self.assertEqual(index.coverage()[0]["status"], "unavailable")
        self.assertEqual(index.occurrences(), [])
        self.assertEqual(len(index.occurrences(include_stale=True)), 2)

    def test_duplicate_text_reuses_chunk_but_keeps_every_occurrence(self) -> None:
        repeated = self.line(request_id="same")
        other = self.line(request_id="other")
        self.path.write_bytes(repeated + repeated + other)
        index = self.index()
        index.ingest(self.path)
        rows = index.occurrences()
        self.assertEqual(len(rows), 3)
        self.assertNotEqual(rows[0]["occurrence_id"], rows[1]["occurrence_id"])
        self.assertEqual(rows[0]["chunk_sha256"], rows[1]["chunk_sha256"])
        chunks = index.db.execute("SELECT COUNT(*) FROM chunks").fetchone()[0]
        self.assertEqual(chunks, 2)
        versioned = self.index(model="local-model", version="v2")
        self.assertEqual(versioned.rebuild_model_cache()["rebuilt_records"], 3)
        self.assertEqual(versioned.db.execute("SELECT COUNT(*) FROM chunks").fetchone()[0], 4)

    def test_json_artifact_and_discovery_never_enter_no_go_zone(self) -> None:
        artifact = self.root / "response.json"
        artifact.write_text(json.dumps({"run_id": "run-json", "event": "response"}))
        blocked = self.root / ".de67" / "no-go-zone" / "hidden.log"
        blocked.parent.mkdir(parents=True)
        blocked.write_bytes(self.line(request_id="forbidden"))
        self.assertEqual(EvidenceIndex.discover([self.root]), [artifact.resolve()])
        index = self.index()
        self.assertEqual(index.ingest(artifact)["ingested_records"], 1)
        handle = index.occurrences()[0]["source_handle"]
        raw = artifact.read_bytes()
        self.assertEqual(handle["sha256"], hashlib.sha256(raw).hexdigest())

    def test_embedding_reuse_is_content_and_model_bound(self) -> None:
        self.path.write_bytes(self.line(request_id="same") * 2)
        index = self.index()
        index.ingest(self.path)

        class Backend:
            backend_id = "fixture-real-shape"
            model_id, model_version = "local-model", "v1"
            def __init__(self): self.calls = []
            def embed(self, values):
                self.calls.append(values)
                return [[float(len(value)), 1.0] for value in values]

        backend = Backend()
        self.assertEqual(index.embed_missing(backend)["embedded_chunks"], 1)
        self.assertEqual(index.embed_missing(backend)["embedded_chunks"], 0)
        self.assertEqual(len(backend.calls), 1)
        self.assertEqual(index.db.execute("SELECT COUNT(*) FROM chunks WHERE embedding_json IS NOT NULL").fetchone()[0], 1)
        backend.model_version = "wrong"
        with self.assertRaisesRegex(ValueError, "model_identity"):
            index.embed_missing(backend)


if __name__ == "__main__":
    unittest.main()
