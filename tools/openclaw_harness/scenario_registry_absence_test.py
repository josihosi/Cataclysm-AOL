#!/usr/bin/env python3
"""Exact owner-settled source-absence CLI behavior."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))
from scenario_registry_store import open_registry


class ScenarioRegistryAbsenceTest(unittest.TestCase):
    def run_cli(self, *arguments):
        return subprocess.run([sys.executable, str(HARNESS_DIR / "scenario_registry_cli.py"),
                               "--json", *arguments], capture_output=True, text=True, check=False)

    def test_reconcile_one_absent_source_requires_exact_identity_and_never_removes_files(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw).resolve()
            source = root / "owned.json"
            other = root / "other.json"
            source.write_text(json.dumps({"name": "owned", "steps": []}))
            other.write_text(json.dumps({"name": "other", "steps": []}))
            registry = root / "registry.sqlite3"
            result = self.run_cli("--registry", str(registry), "rebuild", "--source", str(source),
                                  "--source", str(other))
            self.assertEqual(result.returncode, 0, result.stderr)
            connection = open_registry(str(registry))
            try:
                row = connection.execute(
                    "SELECT manifest_id, last_content_sha256 FROM manifest_current WHERE source_path = ?",
                    (str(source),)).fetchone()
                manifest_id, digest = row
                connection.execute(
                    "INSERT INTO token_history(token_id,manifest_id,route_key,event_kind,reason,details_json) "
                    "VALUES('owned-token',?,'route','issued','test','{}')", (manifest_id,))
                connection.commit()
                def reconcile(path=source, identity=manifest_id, sha=digest):
                    return self.run_cli("--registry", str(registry), "reconcile-absent-source",
                                        "--source", str(path), "--manifest-id", identity,
                                        "--source-sha256", sha)
                self.assertNotEqual(reconcile().returncode, 0)
                self.assertTrue(source.is_file())
                source.write_text('{"name":"changed","steps":[]}')
                self.assertNotEqual(reconcile().returncode, 0)
                self.assertEqual(json.loads(source.read_text())["name"], "changed")
                source.unlink()
                for result in (reconcile(identity="foreign"), reconcile(sha="changed"),
                               reconcile(path=root / "missing-parent" / "owned.json")):
                    self.assertNotEqual(result.returncode, 0)
                source.symlink_to(root / "missing-target")
                self.assertNotEqual(reconcile().returncode, 0)
                source.unlink()
                result = reconcile()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout)["result"], {"absent": 1})
                self.assertEqual(json.loads(reconcile().stdout)["result"], {"absent": 0})
                self.assertEqual(connection.execute(
                    "SELECT present FROM manifest_current WHERE source_path = ?", (str(source),)
                ).fetchone()[0], 0)
                self.assertEqual(connection.execute(
                    "SELECT present FROM manifest_current WHERE source_path = ?", (str(other),)
                ).fetchone()[0], 1)
                self.assertTrue(other.is_file())
                self.assertEqual(connection.execute(
                    "SELECT count(*) FROM lifecycle_history WHERE manifest_id = ? AND event_kind = 'absence'",
                    (manifest_id,)).fetchone()[0], 1)
                self.assertEqual(connection.execute(
                    "SELECT count(*) FROM token_history WHERE token_id = 'owned-token' AND event_kind = 'invalidated'"
                ).fetchone()[0], 1)
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main()
