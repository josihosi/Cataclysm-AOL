import importlib.util
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location(
    "method_provenance", SCRIPTS / "method_provenance.py"
)
provenance = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(provenance)


class MethodProvenanceTests(unittest.TestCase):
    def test_report_is_read_only_and_exposes_baseline_and_local_state(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            local = workspace / ".de67"
            state = local / "state"
            state.mkdir(parents=True)
            for name, text in (
                ("FS.md", "# Frozen FS\n"),
                ("test-and-task-guidelines.md", "test\n"),
            ):
                (local / name).write_text(text, encoding="utf-8")
            database = state / "alternate.sqlite3"
            connection = sqlite3.connect(database)
            connection.executescript("""
                CREATE TABLE coordinator_restart_requests (generation INTEGER);
                INSERT INTO coordinator_restart_requests VALUES (4);
                CREATE TABLE normal_method_receipts (receipt_id TEXT);
                INSERT INTO normal_method_receipts VALUES ('one');
                CREATE TABLE universal_review_receipts (receipt_id TEXT);
                INSERT INTO universal_review_receipts VALUES ('two');
            """)
            connection.commit()
            connection.close()
            (state / "workspace.json").write_text(
                json.dumps({"clock": {"state": str(database)}}), encoding="utf-8"
            )
            subprocess.run(["git", "init", "-q", str(workspace)], check=True)
            subprocess.run(
                ["git", "-C", str(workspace), "config", "user.email", "test@example.invalid"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(workspace), "config", "user.name", "Test"], check=True
            )
            subprocess.run(["git", "-C", str(workspace), "add", "."], check=True)
            subprocess.run(
                ["git", "-C", str(workspace), "commit", "-qm", "baseline"], check=True
            )
            (workspace / "dirty.txt").write_text("dirty\n", encoding="utf-8")
            before = {path: path.read_bytes() for path in local.rglob("*") if path.is_file()}

            result = provenance.report(Path(__file__).parents[1], workspace)

            self.assertEqual(result["workspace"]["clock"]["restart_generation"], 4)
            self.assertEqual(result["workspace"]["clock"]["mutation_receipts"], 2)
            self.assertEqual(len(result["workspace"]["guidance_sha256"]["FS.md"]), 64)
            self.assertNotIn("orchestrator-guidelines.md", result["workspace"]["guidance_sha256"])
            self.assertTrue(result["workspace"]["git"]["uncheckpointed"])
            self.assertEqual(
                before,
                {path: path.read_bytes() for path in local.rglob("*") if path.is_file()},
            )

    def test_cli_emits_json_without_workspace(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(SCRIPTS / "method_provenance.py")],
            capture_output=True,
            text=True,
            check=True,
        )
        result = json.loads(completed.stdout)
        self.assertIn("method_tree_sha256", result)
        self.assertNotIn("workspace", result)


if __name__ == "__main__":
    unittest.main()
