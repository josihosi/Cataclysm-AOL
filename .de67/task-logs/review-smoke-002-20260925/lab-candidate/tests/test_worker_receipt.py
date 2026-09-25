from __future__ import annotations
from contextlib import closing

import hashlib
import sqlite3
import copy
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from worker_receipt import (  # noqa: E402
    WorkerReceiptError,
    compact_worker_receipt,
    normalize_worker_receipt,
    prepare_worker_receipt,
)


class WorkerReceiptTests(unittest.TestCase):
    def test_prepare_collects_fields_and_preserves_supplied_conflicts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / "proof.json"
            artifact.write_text('{"native": true}')
            digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
            value = self.value(artifact, digest)
            state = root / "state.sqlite3"
            with closing(sqlite3.connect(state)) as db, db:
                db.executescript("CREATE TABLE tasks(lineage_id,task_id,claim_id); CREATE TABLE worker_claims(lineage_id,task_id,worker_id); INSERT INTO tasks VALUES('project','task-1','R-1'); INSERT INTO worker_claims VALUES('project','task-1','worker-1');")
            draft = copy.deepcopy(value)
            for key in ("schema", "lineage_id", "task_id", "claim_id", "worker_id"):
                draft.pop(key)
            for entry in draft["artifacts"]:
                entry.pop("sha256")
            def prepare(v):
                return prepare_worker_receipt(v, state=state, workspace=root, lineage_id="project", task_id="task-1")
            self.assertEqual(prepare(draft), normalize_worker_receipt(value, workspace=root, lineage_id="project", task_id="task-1", claim_id="R-1", worker_id="worker-1"))
            for change in (lambda d: d.update(worker_id="wrong"),
                           lambda d: d["artifacts"][0].update(sha256="0" * 64),
                           lambda d: d["artifacts"][0].update(path="missing.json"),
                           lambda d: d["artifacts"][0].update(path="../outside")):
                bad = copy.deepcopy(draft)
                change(bad)
                with self.assertRaises(WorkerReceiptError):
                    prepare(bad)


    def value(self, artifact: Path, digest: str) -> dict[str, object]:
        return {
            "schema": "de67.worker-result-receipt.v1",
            "lineage_id": "project",
            "task_id": "task-1",
            "claim_id": "R-1",
            "worker_id": "worker-1",
            "disposition": "checkpoint",
            "verdict": "continuation",
            "outcome": "Prove the next live boundary.",
            "summary": "The exact footing and next boundary are preserved.",
            "material_changes": [],
            "tests": ["The compact projection was validated."],
            "live_actions": [],
            "evidence_ceiling": ["No product closure is claimed."],
            "bindings": {"run_id": "run-1"},
            "journal_entries": [
                {
                    "id": "event-1",
                    "sequence": 1,
                    "event_type": "observation",
                    "evidence_class": "live",
                    "artifact_ref": artifact.name,
                    "summary": "The first live boundary was observed.",
                }
            ],
            "artifacts": [
                {"path": artifact.name, "sha256": digest, "role": "full transcript"}
            ],
            "first_divergence": {
                "class": "next-boundary",
                "summary": "The next causal response is still open.",
            },
            "accepted_no_replay": ["Do not replay event-1."],
            "active_work": ["Observe the next causal response."],
            "first_open_boundary": "Observe the next causal response.",
            "narrow_queries": ["event_type=observation"],
            "entrypoints": ["src/route.cpp"],
            "context_metrics": {"source_bytes": 42},
        }

    def test_compact_projection_preserves_bindings_and_digest_not_full_journal(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            artifact = workspace / "transcript.jsonl"
            artifact.write_text('{"event":"ready"}\n', encoding="utf-8")
            digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
            receipt = normalize_worker_receipt(
                self.value(artifact, digest),
                lineage_id="project",
                task_id="task-1",
                claim_id="R-1",
                worker_id="worker-1",
                workspace=workspace,
            )
            compact = compact_worker_receipt(receipt)
            self.assertEqual(compact["bindings"]["run_id"], "run-1")
            self.assertEqual(compact["artifact_refs"][0]["sha256"], digest)
            self.assertEqual(compact["journal_entry_ids"], ["event-1"])
            self.assertNotIn("journal_entries", compact)

    def test_digest_mismatch_and_missing_open_boundary_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            artifact = workspace / "transcript.jsonl"
            artifact.write_text('{"event":"ready"}\n', encoding="utf-8")
            value = self.value(artifact, "0" * 64)
            with self.assertRaisesRegex(WorkerReceiptError, "does not match"):
                normalize_worker_receipt(
                    value,
                    lineage_id="project",
                    task_id="task-1",
                    claim_id="R-1",
                    worker_id="worker-1",
                    workspace=workspace,
                )
            value["artifacts"] = []
            value["journal_entries"] = []
            value["first_open_boundary"] = ""
            with self.assertRaisesRegex(WorkerReceiptError, "first_open_boundary"):
                normalize_worker_receipt(
                    value,
                    lineage_id="project",
                    task_id="task-1",
                    claim_id="R-1",
                    worker_id="worker-1",
                )


if __name__ == "__main__":
    unittest.main()
