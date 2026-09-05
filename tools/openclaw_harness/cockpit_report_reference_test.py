import json
import hashlib
from unittest.mock import patch
import cockpit_report_reference as report_reference
from pathlib import Path
import tempfile
import unittest

from cockpit_archive import Archive, ArchiveSequence, json_chunks, write_json_stream
from cockpit_report_reference import (file_identity, journal_reference, load_report,
                                      load_stored_journal, write_report_reference)


class ReportReferenceTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.directory = Path(temp.name)
        self.path = self.directory / "probe.report.json"
        self.archive = Archive(self.directory / "cockpit-evidence.sqlite",
                               run_id="run-a", binding_id="binding-a")
        self.addCleanup(self.archive.close)
        self.entries = self.archive.sequence()
        for index in range(20):
            self.entries.append({"citation_id": f"J{index}", "fact": "ö", "index": index})
        self.journal = {"schema": "caol-playtest-evidence-journal-v1", "entries": self.entries,
                        "journal_sha256": "separately-validated-journal-sha"}
        self.report = {"schema": "report-v1", "steps": [{"journal": self.journal}]}
        write_json_stream(self.path, self.report)
        write_report_reference(self.path, self.report)

    def test_load_preserves_complete_values_and_original_byte_hash(self):
        path, digest, restored = load_report(self.path)
        self.assertEqual(path, str(self.path.resolve()))
        self.assertEqual(digest, file_identity(self.path)["sha256"])
        self.assertIsInstance(restored["steps"][0]["journal"]["entries"], ArchiveSequence)
        self.assertEqual("".join(json_chunks(restored)), self.path.read_text(encoding="utf-8"))
        self.assertEqual(restored["steps"][0]["journal"]["entries"][12]["index"], 12)

    def test_plain_legacy_report_preserves_whitespace_and_identity(self):
        legacy = self.directory / "legacy.json"
        legacy.write_text(json.dumps({"steps": [], "value": "ö"}, indent=2) + "\n", encoding="utf-8")
        _, digest, restored = load_report(legacy)
        self.assertEqual(digest, file_identity(legacy)["sha256"])
        self.assertEqual(restored, {"steps": [], "value": "ö"})

    def test_changed_full_report_rejects_existing_sidecar(self):
        self.path.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "export_identity"):
            load_report(self.path)

    def test_scalar_forgery_cannot_hide_behind_valid_archive_hash(self):
        sidecar = self.path.with_suffix(".ref.json")
        wire = json.loads(sidecar.read_text(encoding="utf-8"))
        wire["schema"] = "forged-report"
        sidecar.write_text(json.dumps(wire), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "reconstruction"):
            load_report(self.path)

    def test_archive_corruption_cannot_become_valid_report(self):
        with self.archive.connection:
            self.archive.connection.execute("DELETE FROM records WHERE stream=? AND seq=4", (self.entries.stream,))
        with self.assertRaisesRegex(ValueError, "gap|truncated"):
            load_report(self.path)

    def test_new_journal_reference_and_legacy_inline_both_resolve(self):
        reference = journal_reference(report_path=self.path,
            report_sha256=file_identity(self.path)["sha256"], selector=["steps", 0, "journal"], journal=self.journal)
        self.assertNotIn("entries", reference)
        restored = load_stored_journal(reference)
        self.assertEqual(restored["entries"][3]["index"], 3)
        self.assertIs(load_stored_journal(self.journal), self.journal)
        with self.assertRaisesRegex(ValueError, "report_identity"):
            load_stored_journal({**reference, "report_sha256": "wrong"})
        with self.assertRaisesRegex(ValueError, "journal_identity"):
            load_stored_journal({**reference, "journal_sha256": "wrong"})

    def test_legacy_parse_and_hash_use_one_byte_snapshot(self):
        path = self.directory / "racing-legacy.json"
        original = b'{"version":1}'
        changed = b'{"version":2}'
        path.write_bytes(original)
        def replace_after_hash(selected):
            identity = file_identity(selected)
            path.write_bytes(changed)
            return identity
        with patch.object(report_reference, "file_identity", side_effect=replace_after_hash):
            _, digest, value = load_report(path)
        snapshots = {1: original, 2: changed}
        self.assertEqual(digest, hashlib.sha256(snapshots[value["version"]]).hexdigest())

    def test_intentional_plain_rewrite_removes_obsolete_archive_index(self):
        from startup_harness import write_json
        self.assertTrue(self.path.with_suffix(".ref.json").is_file())
        write_json(self.path, {"ordinary": "replacement"})
        self.assertFalse(self.path.with_suffix(".ref.json").exists())
        _, _, value = load_report(self.path)
        self.assertEqual(value, {"ordinary": "replacement"})


if __name__ == "__main__":
    unittest.main()
