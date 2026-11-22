"""Run history, native receipt correlation, checkpoint custody and catalogue isolation."""
import argparse
import contextlib
import io
import json
import sqlite3
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from unittest import mock

import scenario_registry_ingestion_test as ingestion_fixture
import scenario_registry_store as store
import world_examples as examples


class WorldExamplesTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.db = str(self.root / "registry.sqlite3")
        self.source = self.root / "closed world"
        self.source.mkdir()
        (self.source / "worldoptions.json").write_text("[]")
        (self.source / "#character.sav.zzip").write_bytes(b"saved player")
        self.parser = argparse.ArgumentParser()
        examples.add_commands(self.parser.add_subparsers(required=True))

    def record(self, ident="abc", **options):
        fields = dict(attempt_id=ident, question="Can the scout return across the bridge?", family="scout-return",
                      outcome="blocked", detail="tiles build required", source="R050",
                      build={"executable_sha256": "abcd"}, recipe={"source": "roof", "radius": 40}, registry=self.db)
        return examples.record_attempt(**(fields | options))

    def invoke(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = examples.run_command(self.parser.parse_args([*args, "--registry", self.db]))
        self.assertEqual(status, 0)
        return out.getvalue()

    def test_empty_query_does_not_create_a_registry(self):
        self.assertEqual(examples.history(registry=self.db)["total"], 0)
        self.assertFalse(Path(self.db).exists())

    def test_repeated_and_parallel_attempts_remain_independent(self):
        # Start once so concurrent writers exercise recording rather than racing first-time migrations.
        self.record()
        with ThreadPoolExecutor(4) as pool:
            list(pool.map(self.record, ["abc", "def", "ghi", "jkl", "abc"]))
        self.assertEqual(examples.history(registry=self.db, limit=1)["total"], 4)
        self.assertEqual(len(examples.history(registry=self.db, limit=1)["runs"]), 1)
        with self.assertRaisesRegex(ValueError, "different recorded result"):
            self.record(outcome="launched")
        self.assertEqual(len(json.loads(self.invoke("history", "TiLes", "bridge", "--family", "scout-return",
                                                    "--json"))["runs"]), 4)
        self.assertEqual(examples.history(registry=self.db, search="other")["total"], 0)

    def test_pin_search_relabel_and_release_without_changing_catalogue(self):
        self.record()
        self.assertEqual(examples.history(registry=self.db, kept=True)["total"], 0)
        self.invoke("keep", "abc", "--reason", "Rural forest retreat", "--family", "return-journeys")
        self.assertIn("Rural forest retreat", self.invoke("history", "rural", "--kept", "--family", "return-journeys"))
        self.invoke("keep", "abc", "--reason", "Rare cover failure")
        self.assertEqual(examples.history(registry=self.db, search="rural")["total"], 0)
        self.assertEqual(examples.history(registry=self.db, search="cover", family="return-journeys")["total"], 1)
        self.assertTrue(examples.unkeep("abc", registry=self.db)["kept"] is False)
        self.assertEqual(examples.history(registry=self.db)["total"], 1)
        with contextlib.closing(store.open_registry(self.db, writable=False)) as conn:
            self.assertEqual(conn.execute("SELECT count(*) FROM manifest_current").fetchone()[0], 0)
        with self.assertRaisesRegex(ValueError, "Unknown run"):
            examples.keep("missing", "not a run", registry=self.db)
        with self.assertRaisesRegex(ValueError, "reason"):
            examples.keep("abc", " ", registry=self.db)

    def test_retained_checkpoint_survives_original_cleanup_and_unkeep_removes_only_our_copy(self):
        self.record()
        path = Path(examples.keep("abc", "native bridge failure", registry=self.db, checkpoint=self.source)["checkpoint"])
        self.assertNotEqual(path, self.source)
        original = self.source / "#character.sav.zzip"
        self.assertEqual(path.joinpath(original.name).read_bytes(), original.read_bytes())
        again = examples.keep("abc", "better note", registry=self.db, checkpoint=self.source)
        self.assertEqual(again["checkpoint"], str(path))
        original.write_bytes(b"next turn")
        with self.assertRaisesRegex(ValueError, "another checkpoint"):
            examples.keep("abc", "wrong turn", registry=self.db, checkpoint=self.source)
        original.unlink()
        self.assertEqual(path.joinpath(original.name).read_bytes(), b"saved player")
        record = examples.history(registry=self.db, kept=True)["runs"][0]
        self.assertEqual(len(record["checkpoint_sha256"]), 64)
        self.assertEqual(record["reason"], "better note")
        self.invoke("unkeep", "abc")
        self.assertFalse(path.exists())
        self.assertTrue((self.source / "worldoptions.json").exists())
        self.assertFalse(examples.unkeep("abc", registry=self.db)["kept"])

    def test_failed_or_changing_capture_does_not_publish_an_example(self):
        self.record()
        with self.assertRaisesRegex(ValueError, "saved world"):
            examples.keep("abc", "empty", registry=self.db, checkpoint=self.root)
        with mock.patch("startup_harness.sha256_tree", side_effect=[("first", ""), ("copy", ""), ("later", "")]):
            with self.assertRaisesRegex(ValueError, "changed"):
                examples.keep("abc", "changed", registry=self.db, checkpoint=self.source)
        self.assertEqual(examples.history(registry=self.db, kept=True)["total"], 0)
        self.assertFalse(any(examples._store(self.db).iterdir()))

    def test_interrupted_release_is_retryable_without_advertising_a_partial_checkpoint(self):
        self.record()
        path = Path(examples.keep("abc", "rare start", registry=self.db, checkpoint=self.source)["checkpoint"])
        with mock.patch.object(examples.shutil, "rmtree", side_effect=PermissionError("locked")):
            with self.assertRaisesRegex(PermissionError, "locked"):
                examples.unkeep("abc", registry=self.db)
        self.assertTrue(path.exists())
        self.assertEqual(examples.history(registry=self.db, kept=True)["total"], 0)
        examples.unkeep("abc", registry=self.db)
        self.assertFalse(path.exists())

    def test_ordinary_ingestion_is_searchable_after_cleanup_and_merges_with_scratch(self):
        fixture = ingestion_fixture.ScenarioRegistryIngestionTest()
        conn, _, manifest, path = fixture.setup_registry(self.root)
        with contextlib.closing(conn):
            report = fixture.report(manifest)
            report["run_id"] = "native1"
            report["steps"] = [{"cockpit_live_session": {"final": {"run_id": "native1", "stop_reason": "native quit",
                "stop_detail": {"gameplay_outcome": {"summary": "Observed a scout cross the bridge.", "verdict": "observed"}}}}}]
            fixture.write_json(path, report)
            receipt = store.ingest_report_reference(conn, path, adapters=fixture.adapters(
                {"runtime": "compatible", "fixture": "compatible", "profile": "compatible"}))
            self.assertEqual(receipt["status"], "ingested")
            conn.execute("UPDATE manifest_current SET current_sha256=NULL")
        path.unlink()
        native = examples.history(registry=self.db)["runs"][0]
        self.assertEqual(native["run_id"], "native1")
        self.assertEqual(native["report_id"], receipt["report_id"])
        self.assertEqual(native["detail"], "Observed a scout cross the bridge.")
        self.assertEqual(native["outcome"], "observed")
        self.assertTrue(native["report_verdict"])
        self.assertTrue(native["recipe"]["manifest_sha256"])
        self.record(outcome="launched", native_run_id="native1", detail="session opened")
        rows = examples.history(registry=self.db)["runs"]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["run_id"], "abc")
        self.assertEqual(rows[0]["outcome"], native["outcome"])
        self.assertNotEqual(rows[0]["outcome"], "launched")
        self.assertEqual(rows[0]["question"], "Can the scout return across the bridge?")
        self.assertEqual(rows[0]["report_id"], receipt["report_id"])

    def test_older_read_only_history_can_be_searched_without_migrating(self):
        with contextlib.closing(sqlite3.connect(self.db, isolation_level=None)) as conn:
            store.apply_migrations(conn, [item for item in store.SCHEMA_MIGRATIONS if item[0] <= 21])
            conn.execute("INSERT INTO report_ingestion_history(report_id,report_path,report_sha256,"
                         "report_kind,ingestion_status) VALUES('old','missing report','hash','setup-only',"
                         "'ingested_non_authoritative')")
        self.assertEqual(examples.history(registry=self.db, search="setup-only")["runs"][0]["run_id"], "old")
        with contextlib.closing(sqlite3.connect(self.db)) as conn:
            self.assertEqual(conn.execute("PRAGMA user_version").fetchone()[0], 21)
        examples.keep("old", "historical setup", registry=self.db)
        self.assertEqual(examples.history(registry=self.db, kept=True)["total"], 1)

    def test_v22_kept_attempt_preserves_recipe_and_can_gain_missing_source_facts(self):
        with contextlib.closing(sqlite3.connect(self.db, isolation_level=None)) as conn:
            store.apply_migrations(conn, [item for item in store.SCHEMA_MIGRATIONS if item[0] <= 22])
            conn.execute("INSERT INTO world_scratch_attempt(run_id,question,family,source,recipe_json,build_json,"
                         "outcome,detail,native_run_id,selection_token) VALUES('old','scouts?','families','roof',"
                         "'{\"radius\":40}','{}','blocked','build not found','','')")
            conn.execute("INSERT INTO kept_world_example(run_id,reason,family) VALUES('old','rare start','families')")
        self.assertEqual(examples.history(registry=self.db, kept=True)["runs"][0]["source_binding"], {})
        binding = {"parent": "roof", "parent_sha256": "a"*64, "world_sha256": "b"*64, "player_save": "Ana.sav"}
        examples.bind_attempt_source("old", binding, registry=self.db)
        examples.bind_attempt_source("old", {"world_sha256": "b"*64}, registry=self.db)
        with self.assertRaisesRegex(ValueError, "different world_sha256"):
            examples.bind_attempt_source("old", {"world_sha256": "c"*64}, registry=self.db)
        row = examples.history(registry=self.db, kept=True, search="Ana.sav")["runs"][0]
        self.assertEqual((row["source_binding"],row["recipe"],row["reason"]), (binding,{"radius":40},"rare start"))
        self.record(source_binding=binding)
        self.assertEqual(examples.history(registry=self.db, search="Ana.sav")["total"], 2)


if __name__ == "__main__":
    unittest.main()
