from __future__ import annotations

import json
import sqlite3
import subprocess
import tempfile
import unittest
from pathlib import Path

from repository_checkpoint import RepositoryCheckpointError, checkpoint_repository


def git(path: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(path), *arguments],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.stdout.strip()


class RepositoryCheckpointTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.workspace = self.root / "product"
        self.remote = self.root / "remote.git"
        subprocess.run(["git", "init", "--bare", str(self.remote)], check=True, capture_output=True)
        subprocess.run(
            ["git", "init", "-b", "dev", str(self.workspace)],
            check=True,
            capture_output=True,
        )
        git(self.workspace, "config", "user.name", "DE67 Checkpoint Test")
        git(self.workspace, "config", "user.email", "de67-checkpoint@example.invalid")
        git(self.workspace, "remote", "add", "origin", str(self.remote))
        (self.workspace / ".gitignore").write_text(
            "/.de67/state/\n/build_logs/\n", encoding="utf-8"
        )
        (self.workspace / "tracked.txt").write_text("baseline\n", encoding="utf-8")
        git(self.workspace, "add", ".")
        git(self.workspace, "commit", "-m", "baseline")
        git(self.workspace, "push", "origin", "refs/heads/dev:refs/heads/dev")
        self.state = self.workspace / ".de67" / "state" / "deadlines.sqlite3"
        self.state.parent.mkdir(parents=True)
        self.config = self.state.parent / "workspace.json"
        self._write_config()
        with sqlite3.connect(self.state) as connection:
            connection.executescript(
                """
                CREATE TABLE worker_claims (
                    lineage_id TEXT, task_id TEXT, worker_id TEXT,
                    last_checkpoint_at REAL, released_at REAL
                );
                CREATE TABLE supervisor_attempts (
                    lineage_id TEXT, role TEXT, run_id TEXT,
                    owner_id TEXT, started_at REAL, finished_at REAL
                );
                CREATE TABLE coordinator_restart_requests (
                    lineage_id TEXT, generation INTEGER
                );
                CREATE TABLE claim_acceptances (
                    lineage_id TEXT, accepted_at REAL
                );
                CREATE TABLE tasks (
                    lineage_id TEXT, started_at REAL, attempt_terminal_at REAL
                );
                """
            )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _write_config(self, *, targets: list[dict[str, str]] | None = None) -> None:
        self.config.write_text(
            json.dumps(
                {
                    "version": 1,
                    "workspace": str(self.workspace),
                    "source_ref": "refs/heads/dev",
                    "targets": targets
                    or [
                        {
                            "remote": "origin",
                            "remote_url": str(self.remote),
                            "target_ref": "refs/heads/dev",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )

    def test_checkpoint_commits_pushes_and_cross_links_without_generated_bulk(self) -> None:
        (self.workspace / "tracked.txt").write_text("changed\n", encoding="utf-8")
        (self.workspace / "new_source.py").write_text("VALUE = 1\n", encoding="utf-8")
        generated = self.workspace / "build_logs" / "large.json"
        generated.parent.mkdir()
        generated.write_text("generated\n", encoding="utf-8")

        result = checkpoint_repository(self.workspace, self.state, "lineage")

        self.assertEqual(result["status"], "verified")
        self.assertEqual(git(self.workspace, "status", "--short"), "")
        self.assertEqual(
            git(self.workspace, "rev-parse", "HEAD"),
            git(self.remote, "rev-parse", "refs/heads/dev"),
        )
        self.assertNotIn("build_logs/large.json", git(self.workspace, "show", "--name-only", "--format="))
        message = git(self.workspace, "log", "-1", "--format=%B")
        self.assertIn(f"DE67-Checkpoint: {result['checkpoint_id']}", message)
        with sqlite3.connect(self.state) as connection:
            row = connection.execute(
                "SELECT commit_sha, state_revision, status FROM repository_checkpoints WHERE checkpoint_id = ?",
                (result["checkpoint_id"],),
            ).fetchone()
        self.assertEqual(row, (result["commit_sha"], result["state_revision"], "verified"))

    def test_no_changes_makes_no_empty_commit(self) -> None:
        before = git(self.workspace, "rev-parse", "HEAD")
        result = checkpoint_repository(self.workspace, self.state, "lineage")
        self.assertEqual(result["status"], "no_changes")
        self.assertEqual(git(self.workspace, "rev-parse", "HEAD"), before)

    def test_wrong_branch_and_multiple_targets_fail_before_staging(self) -> None:
        git(self.workspace, "checkout", "-b", "wrong")
        with self.assertRaisesRegex(RepositoryCheckpointError, "source branch mismatch"):
            checkpoint_repository(self.workspace, self.state, "lineage")
        git(self.workspace, "checkout", "dev")
        self._write_config(
            targets=[
                {"remote": "origin", "target_ref": "refs/heads/dev"},
                {"remote": "backup", "target_ref": "refs/heads/dev"},
            ]
        )
        with self.assertRaisesRegex(RepositoryCheckpointError, "exactly one"):
            checkpoint_repository(self.workspace, self.state, "lineage")

    def test_crash_after_commit_reconciles_same_identity_without_duplicate_commit(self) -> None:
        (self.workspace / "tracked.txt").write_text("changed\n", encoding="utf-8")

        def crash(event: str, _payload: dict[str, object]) -> None:
            if event == "after_commit_before_sqlite":
                raise RuntimeError("simulated process loss")

        with self.assertRaisesRegex(RuntimeError, "simulated process loss"):
            checkpoint_repository(
                self.workspace, self.state, "lineage", event_hook=crash
            )
        committed_head = git(self.workspace, "rev-parse", "HEAD")
        count = git(self.workspace, "rev-list", "--count", "HEAD")

        recovered = checkpoint_repository(self.workspace, self.state, "lineage")

        self.assertEqual(recovered["status"], "verified")
        self.assertEqual(recovered["commit_sha"], committed_head)
        self.assertEqual(git(self.workspace, "rev-list", "--count", "HEAD"), count)

    def test_remote_mismatch_is_recorded_and_not_reported_as_verified(self) -> None:
        baseline = git(self.workspace, "rev-parse", "HEAD")
        (self.workspace / "tracked.txt").write_text("changed\n", encoding="utf-8")

        def move_remote(event: str, _payload: dict[str, object]) -> None:
            if event == "after_push_before_verify":
                subprocess.run(
                    ["git", "--git-dir", str(self.remote), "update-ref", "refs/heads/dev", baseline],
                    check=True,
                )

        with self.assertRaisesRegex(RepositoryCheckpointError, "verification"):
            checkpoint_repository(
                self.workspace, self.state, "lineage", event_hook=move_remote
            )
        with sqlite3.connect(self.state) as connection:
            status, failure = connection.execute(
                "SELECT status, failure_step FROM repository_checkpoints"
            ).fetchone()
        self.assertEqual((status, failure), ("failed", "push-or-verify"))

    def test_unfinished_worker_and_active_coordinator_allow_snapshot(self) -> None:
        with sqlite3.connect(self.state) as connection:
            connection.execute(
                "INSERT INTO worker_claims VALUES ('lineage', 'task-1', 'worker-1', 1, NULL)"
            )
        self.assertEqual(checkpoint_repository(self.workspace, self.state, "lineage")["status"], "no_changes")
        with sqlite3.connect(self.state) as connection:
            connection.execute(
                "INSERT INTO supervisor_attempts VALUES ('lineage', 'coordinator', 'run-1', 'current-owner', 1, NULL)"
            )
        self.assertEqual(checkpoint_repository(self.workspace, self.state, "lineage")["status"], "no_changes")

    def test_current_supervisor_ignores_unfinished_rows_from_a_prior_owner(self) -> None:
        with sqlite3.connect(self.state) as connection:
            connection.execute(
                "INSERT INTO supervisor_attempts VALUES ('lineage', 'coordinator', 'old-run', 'old-owner', 1, NULL)"
            )

        result = checkpoint_repository(
            self.workspace,
            self.state,
            "lineage",
            supervisor_owner_id="current-owner",
        )

        self.assertEqual(result["status"], "no_changes")


if __name__ == "__main__":
    unittest.main()
