from contextlib import closing
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from deadline_harness import DeadlineHarness
from mutation_guard import broader_mutation_from_incident


class DeadlineIncidentCompatibilityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.state = Path(self.temporary.name) / "state.sqlite3"
        with DeadlineHarness(self.state) as h:
            h.start_task("project", "charter", "R-1", 100, now=0)
            h.complete_task("project", "charter", "Preparation only", now=5)
            with h.connection:
                h.connection.execute("INSERT INTO claim_deadline_generations (lineage_id,claim_id,generation,estimate_seconds,started_at,deadline_at) VALUES ('project','R-1',4,100,10,110)")
                h.connection.execute("INSERT INTO claim_deadline_generation_incidents (lineage_id,claim_id,generation,source_task_id,recorded_at,short_verdict,long_detail,reviewed_at) VALUES ('project','R-1',4,'charter',111,'Claim missed','Preparation completed; claim remains unaccepted',112)")
                h.connection.execute("INSERT INTO claim_deadline_incidents (lineage_id,claim_id,source_task_id,recorded_at,short_verdict,long_detail,reviewed_at) VALUES ('project','R-1','charter',111,'Claim missed','Preparation completed; claim remains unaccepted',112)")

    def test_startup_and_explicit_lookup_do_not_recreate_newer_incident(self):
        for _ in range(2):
            with DeadlineHarness(self.state) as h:
                self.assertIsNone(h._claim_deadline_incident("project", "R-1", 1))
                rows = h.connection.execute("SELECT generation FROM claim_deadline_generation_incidents").fetchall()
                self.assertEqual([r[0] for r in rows], [4])

    def test_existing_mirror_preserves_history_and_resolves_current_incident(self):
        with closing(sqlite3.connect(self.state)) as c, c:
            c.execute("INSERT INTO claim_deadline_generation_incidents (lineage_id,claim_id,generation,source_task_id,recorded_at,short_verdict,long_detail) VALUES ('project','R-1',1,'charter',111,'deadline miss','')")
        with DeadlineHarness(self.state) as h:
            history = [tuple(r) for r in h.connection.execute("SELECT * FROM claim_deadline_generation_incidents WHERE generation=1")]
            self.assertEqual([r["deadline_generation"] for r in h._pending_deadline_mutations("project")], [4])
            self.assertTrue(broader_mutation_from_incident(self.state, "project", "charter", "deadline_miss"))
            h.resolve_deadline_mutation("project", "R-1", "micro", "Preserve completed preparation", now=113)
            h.resolve_deadline_mutation("project", "R-1", "macro", "Compatibility recovery verified", now=114, no_change_required=True)
            self.assertEqual(h._pending_deadline_mutations("project"), [])
            self.assertEqual([tuple(r) for r in h.connection.execute("SELECT * FROM claim_deadline_generation_incidents WHERE generation=1")], history)
            self.assertEqual(h.connection.execute("SELECT terminal_at FROM tasks WHERE task_id='charter'").fetchone()[0], 5)
            self.assertEqual(h.connection.execute("SELECT count(*) FROM claim_acceptances").fetchone()[0], 0)

    def test_distinct_legacy_incident_still_migrates_and_blocks(self):
        with closing(sqlite3.connect(self.state)) as c, c:
            c.execute("UPDATE claim_deadline_incidents SET recorded_at=101")
        with DeadlineHarness(self.state) as h:
            pending = h._pending_deadline_mutations("project")
            self.assertEqual({r["deadline_generation"] for r in pending}, {1, 4})

    def test_same_timestamp_different_task_is_not_a_mirror(self):
        with closing(sqlite3.connect(self.state)) as c, c:
            c.execute("INSERT INTO tasks (lineage_id,task_id,claim_id,estimate_seconds,started_at,deadline_at) VALUES ('project','older','R-1',100,0,100)")
            c.execute("UPDATE claim_deadline_incidents SET source_task_id='older'")
        with DeadlineHarness(self.state) as h:
            self.assertEqual({r["deadline_generation"] for r in h._pending_deadline_mutations("project")}, {1, 4})


if __name__ == "__main__":
    unittest.main()
