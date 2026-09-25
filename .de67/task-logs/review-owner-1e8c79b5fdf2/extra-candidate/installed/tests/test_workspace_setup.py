from __future__ import annotations

import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "de-67-3" / "scripts"))

from workspace_setup import (  # noqa: E402
    CONFIG_RELATIVE_PATH,
    DEADLINE_STATE_RELATIVE_PATH,
    MANAGED_HOOK_MARKER,
    PHASE3_ENVIRONMENT_ROOT,
    PHASE3_WORKSPACE_FILES,
    PUSH_STATUS_RELATIVE_PATH,
    SetupError,
    configure,
    push_checkpoints,
)
import workspace_setup  # noqa: E402
from instruction_context import common_guidance  # noqa: E402
from specification import render_functional_specification  # noqa: E402

MODULE_PATH = Path(workspace_setup.__file__).resolve()


VERIFIED_WORKERS = (
    ("gpt-6-luna", "high"),
    ("gpt-6-sol", "low"),
)


class WorkspaceSetupTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.origin = self.root / "origin.git"
        self.workspace = self.root / "work"
        self.git(self.root, "init", "--bare", str(self.origin))
        self.git(self.root, "init", str(self.workspace))
        self.git(self.workspace, "config", "user.name", "DE67 Test")
        self.git(self.workspace, "config", "user.email", "de67@example.invalid")
        self.git(self.workspace, "checkout", "-b", "dev")
        (self.workspace / ".gitignore").write_text(
            ".de67/state/\n", encoding="utf-8"
        )
        (self.workspace / "tracked.txt").write_text("one\n", encoding="utf-8")
        self.git(self.workspace, "add", ".gitignore", "tracked.txt")
        self.git(self.workspace, "commit", "-m", "initial")
        self.git(self.workspace, "remote", "add", "origin", str(self.origin))
        self.git(self.workspace, "push", "-u", "origin", "dev")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    @staticmethod
    def git(cwd: Path, *arguments: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", "-C", str(cwd), *arguments],
            text=True,
            capture_output=True,
            check=check,
        )

    def head(self, repository: Path, reference: str = "HEAD") -> str:
        return self.git(repository, "rev-parse", reference).stdout.strip()

    def configure_push(self) -> dict[str, object]:
        return configure(
            self.workspace, [("origin", "dev")], bind_clock=False
        )

    def commit_file(self, name: str, content: str, message: str) -> str:
        (self.workspace / name).write_text(content, encoding="utf-8")
        self.git(self.workspace, "add", name)
        self.git(self.workspace, "commit", "-m", message)
        return self.head(self.workspace)

    def freeze_dfs(self) -> None:
        (self.workspace / ".de67").mkdir(exist_ok=True)
        (self.workspace / ".de67/FS.md").write_text(
            "# Feature DFS\n\nStatus: Frozen against inspected source baseline\n",
            encoding="utf-8",
        )

    def accepted_projection(self) -> None:
        self.freeze_dfs()
        environment = self.workspace / ".de67"
        (environment / "FS.md").write_text(
            "# DFS\nStatus: Refrozen\n"
            "<!-- DE67:DFS-SLICE:BEGIN id=R-001-S001 claim=R-001 -->\n"
            "Current contract remains outside the historical projection.\n"
            "Implementation status:\n\n"
            "- [ ] 🔴 R-001 — Proof remains open.\n"
            "<!-- DE67:DFS-SLICE:END id=R-001-S001 claim=R-001 -->\n"
            "- [ ] 🔴 R-002 — Fresh campaign proof remains open.\n",
            encoding="utf-8",
        )
        (environment / "work-ledger.md").write_text(
            "- [x] R-001 — Historical proof accepted.\n", encoding="utf-8"
        )
        self.git(self.workspace, "add", ".de67/FS.md")
        self.git(self.workspace, "commit", "-m", "red baseline")
        with workspace_setup._deadline_harness_class()(
            self.workspace / DEADLINE_STATE_RELATIVE_PATH
        ) as harness:
            harness.start_task("project", "explore", "R-001", 100, now=0)
            harness.complete_task("project", "explore", "route proved", now=1)
            harness.transition_claim_to_closure(
                "project", "R-001", "explore", "Close it.", "Run it.",
                "Independent proof remains.", now=2,
            )
            harness.start_task("project", "closure", "R-001", 100, phase="closure", now=3)
            harness.complete_task("project", "closure", "proof complete", now=4)
            harness.accept_claim("project", "R-001", "closure", "accepted proof", now=5)

    def migrate_specification(self) -> None:
        environment = self.workspace / ".de67"
        dfs = environment / "FS.md"
        fs = environment / "FS.md"
        fs.write_text(
            render_functional_specification(dfs.read_text(encoding="utf-8")),
            encoding="utf-8",
        )

    def test_fs_only_setup_preserves_bound_acceptance_and_authoring_files(self) -> None:
        self.accepted_projection()
        self.migrate_specification()
        environment = self.workspace / ".de67"
        paths = [environment / name for name in ("FS.md", "work-ledger.md")]
        before = {path: path.read_bytes() for path in paths}
        self.assertNotIn("Implementation status:", paths[0].read_text(encoding="utf-8"))
        state = self.workspace / DEADLINE_STATE_RELATIVE_PATH
        with closing(sqlite3.connect(state)) as connection:
            accepted_before = sorted(connection.iterdump())

        result = configure(
            self.workspace, [("origin", "dev")], bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(result["clock"]["lineage"], "project")
        self.assertTrue(result["push"]["ok"])
        self.assertEqual({path: path.read_bytes() for path in paths}, before)
        with closing(sqlite3.connect(state)) as connection:
            self.assertEqual(sorted(connection.iterdump()), accepted_before)

    def test_fs_only_setup_rejects_missing_fs_before_binding_or_push(self) -> None:
        self.accepted_projection()
        self.migrate_specification()
        fs = self.workspace / ".de67/FS.md"
        fs.unlink()
        state = self.workspace / DEADLINE_STATE_RELATIVE_PATH
        before = state.read_bytes()

        with patch.object(workspace_setup, "push_checkpoints") as push:
            with self.assertRaisesRegex(SetupError, "Missing functional specification"):
                configure(
                    self.workspace, [("origin", "dev")], bind_clock=True,
                    worker_capabilities=VERIFIED_WORKERS,
                )

        push.assert_not_called()
        self.assertEqual(state.read_bytes(), before)
        self.assertFalse((self.workspace / CONFIG_RELATIVE_PATH).exists())

    def test_refreeze_setup_rejects_incompatible_projection_before_push(self) -> None:
        self.accepted_projection()
        ledger = self.workspace / ".de67/work-ledger.md"
        ledger.write_text("# Ledger\n")
        state = self.workspace / DEADLINE_STATE_RELATIVE_PATH
        before = state.read_bytes()
        with patch.object(workspace_setup, "push_checkpoints") as push:
            with self.assertRaisesRegex(SetupError, "Accepted claim R-001 lacks a work-ledger projection"):
                configure(self.workspace, [("origin", "dev")], bind_clock=True,
                          worker_capabilities=VERIFIED_WORKERS)
        push.assert_not_called()
        self.assertEqual(state.read_bytes(), before)
        self.assertFalse((self.workspace / CONFIG_RELATIVE_PATH).exists())

    def test_refreeze_projection_preserves_history_and_fresh_obligations(self) -> None:
        self.accepted_projection()
        environment = self.workspace / ".de67"
        (environment / "work-ledger.md").write_text(
            "- [x] R-001 — Historical proof accepted.\n"
            "- [ ] R-002 — Fresh campaign proof remains open.\n"
        )
        paths = [environment / "FS.md", environment / "work-ledger.md",
                 self.workspace / DEADLINE_STATE_RELATIVE_PATH]
        before = {path: path.read_bytes() for path in paths}
        workspace_setup._validate_delivery_projection(self.workspace, paths[2])
        self.assertEqual({path: path.read_bytes() for path in paths}, before)

    def test_refreeze_preflight_requires_no_legacy_status_baseline(self) -> None:
        self.accepted_projection()
        baseline = self.workspace / ".de67/state/dfs-status-baselines.json"
        self.assertFalse(baseline.exists())
        state = self.workspace / DEADLINE_STATE_RELATIVE_PATH
        before = state.read_bytes()
        workspace_setup._validate_delivery_projection(self.workspace, state)
        self.assertFalse(baseline.exists())
        self.assertEqual(state.read_bytes(), before)

    def test_configuration_pushes_backlog_and_post_commit_pushes_next_head(self) -> None:
        backlog = self.commit_file("tracked.txt", "two\n", "backlog")
        result = self.configure_push()

        self.assertTrue(result["push"]["ok"])
        self.assertEqual(self.head(self.origin, "refs/heads/dev"), backlog)
        hook = Path(str(result["hook"]))
        self.assertIn(MANAGED_HOOK_MARKER, hook.read_text(encoding="utf-8"))

        next_head = self.commit_file("next.txt", "next\n", "next")
        self.assertEqual(self.head(self.origin, "refs/heads/dev"), next_head)

    def test_configuration_rejects_multiple_automatic_targets(self) -> None:
        checkpoint = self.root / "checkpoint.git"
        self.git(self.root, "init", "--bare", str(checkpoint))
        self.git(self.workspace, "remote", "add", "checkpoint", str(checkpoint))

        with self.assertRaisesRegex(SetupError, "Exactly one --target"):
            configure(
                self.workspace,
                [("origin", "dev"), ("checkpoint", "main")],
                bind_clock=False,
            )

    def test_repeated_setup_is_idempotent_and_rejects_added_target(self) -> None:
        self.freeze_dfs()
        first = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="stable-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )
        initial_config = json.loads(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
        )

        repeated = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )
        repeated_config = json.loads(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertEqual(repeated_config, initial_config)
        self.assertEqual(repeated["clock"], first["clock"])

        checkpoint = self.root / "checkpoint-additive.git"
        self.git(self.root, "init", "--bare", str(checkpoint))
        self.git(self.workspace, "remote", "add", "checkpoint", str(checkpoint))
        with self.assertRaisesRegex(SetupError, "Exactly one --target"):
            configure(
                self.workspace,
                [("origin", "dev"), ("checkpoint", "main")],
                bind_clock=True,
                worker_capabilities=VERIFIED_WORKERS,
            )
        self.assertEqual(
            json.loads(
                (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
            ),
            initial_config,
        )

    def test_repeated_setup_cannot_repin_changed_remote_url(self) -> None:
        self.freeze_dfs()
        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="stable-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )
        original_config = (self.workspace / CONFIG_RELATIVE_PATH).read_text(
            encoding="utf-8"
        )
        replacement = self.root / "replacement-for-setup.git"
        self.git(self.root, "init", "--bare", str(replacement))
        self.git(
            self.workspace,
            "remote",
            "set-url",
            "--push",
            "origin",
            str(replacement),
        )

        with self.assertRaisesRegex(SetupError, "URL cannot be changed"):
            configure(
                self.workspace,
                [("origin", "dev")],
                bind_clock=True,
                worker_capabilities=VERIFIED_WORKERS,
            )
        self.assertEqual(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8"),
            original_config,
        )

    def test_setup_copies_missing_phase3_files_and_preserves_local_mutations(self) -> None:
        self.freeze_dfs()
        local = self.workspace / ".de67" / "test-and-task-guidelines.md"
        local.write_text("# Local mutable policy\n", encoding="utf-8")

        result = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="stable-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(local.read_text(encoding="utf-8"), "# Local mutable policy\n")
        self.assertIn(
            "test-and-task-guidelines.md",
            result["phase3_environment"]["preserved"],
        )
        self.assertFalse((self.workspace / ".de67" / "orchestrator-guidelines.md").exists())
        for name in PHASE3_WORKSPACE_FILES:
            destination = self.workspace / ".de67" / name
            self.assertTrue(destination.is_file(), name)
            if name != "test-and-task-guidelines.md":
                self.assertEqual(
                    destination.read_bytes(),
                    (PHASE3_ENVIRONMENT_ROOT / name).read_bytes(),
                )

    def test_repeated_setup_cannot_repin_changed_source_branch(self) -> None:
        self.freeze_dfs()
        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="stable-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )
        original_config = (self.workspace / CONFIG_RELATIVE_PATH).read_text(
            encoding="utf-8"
        )
        self.git(self.workspace, "checkout", "-b", "other")
        self.git(self.workspace, "push", "-u", "origin", "other")

        with self.assertRaisesRegex(SetupError, "source branch cannot be changed"):
            configure(
                self.workspace,
                [("origin", "other")],
                bind_clock=True,
                worker_capabilities=VERIFIED_WORKERS,
            )
        self.assertEqual(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8"),
            original_config,
        )

    def test_dirty_worktree_pushes_only_committed_head(self) -> None:
        self.configure_push()
        (self.workspace / "tracked.txt").write_text("dirty and local\n", encoding="utf-8")
        committed = self.commit_file("separate.txt", "committed\n", "separate")

        self.assertEqual(self.head(self.origin, "refs/heads/dev"), committed)
        self.assertIn(" M tracked.txt", self.git(self.workspace, "status", "--short").stdout)
        status = json.loads(
            (self.workspace / PUSH_STATUS_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertNotIn("dirty", status)
        self.assertTrue(status["ok"])

    def test_wrong_branch_is_not_pushed(self) -> None:
        self.configure_push()
        original_remote = self.head(self.origin, "refs/heads/dev")
        self.git(self.workspace, "checkout", "-b", "other")
        other_head = self.commit_file("other.txt", "other\n", "other")

        self.assertNotEqual(other_head, original_remote)
        self.assertEqual(self.head(self.origin, "refs/heads/dev"), original_remote)
        with self.assertRaisesRegex(SetupError, "Configured source"):
            push_checkpoints(self.workspace)
        status = json.loads(
            (self.workspace / PUSH_STATUS_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertFalse(status["ok"])
        self.assertIn("Configured source", status["error"])

    def test_unconfigured_sibling_worktree_does_not_use_shared_hook(self) -> None:
        self.configure_push()
        original_remote = self.head(self.origin, "refs/heads/dev")
        sibling = self.root / "sibling"
        self.git(self.workspace, "worktree", "add", "-b", "sibling", str(sibling))
        (sibling / "sibling.txt").write_text("sibling\n", encoding="utf-8")
        self.git(sibling, "add", "sibling.txt")
        self.git(sibling, "commit", "-m", "sibling")

        self.assertFalse((sibling / CONFIG_RELATIVE_PATH).exists())
        self.assertEqual(self.head(self.origin, "refs/heads/dev"), original_remote)

    def test_changed_remote_url_is_recorded_as_a_safe_push_failure(self) -> None:
        self.configure_push()
        replacement = self.root / "replacement.git"
        self.git(self.root, "init", "--bare", str(replacement))
        self.git(
            self.workspace,
            "remote",
            "set-url",
            "--push",
            "origin",
            str(replacement),
        )

        status = push_checkpoints(self.workspace)

        self.assertFalse(status["ok"])
        self.assertIn("URL changed", status["targets"][0]["error"])
        self.assertEqual(
            self.head(self.origin, "refs/heads/dev"), self.head(self.workspace)
        )

    def test_changed_upstream_and_detached_head_are_refused(self) -> None:
        self.configure_push()
        self.git(self.workspace, "push", "origin", "HEAD:refs/heads/other")
        self.git(
            self.workspace,
            "branch",
            "--set-upstream-to=origin/other",
            "dev",
        )
        with self.assertRaisesRegex(SetupError, "upstream changed"):
            push_checkpoints(self.workspace)
        status = json.loads(
            (self.workspace / PUSH_STATUS_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertFalse(status["ok"])
        self.assertIn("upstream changed", status["error"])

        self.git(self.workspace, "branch", "--set-upstream-to=origin/dev", "dev")
        self.git(self.workspace, "checkout", "--detach")
        with self.assertRaisesRegex(SetupError, "attached local branch"):
            push_checkpoints(self.workspace)
        status = json.loads(
            (self.workspace / PUSH_STATUS_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertFalse(status["ok"])
        self.assertIn("attached local branch", status["error"])

    def test_custom_hooks_path_outside_common_git_dir_is_refused(self) -> None:
        custom_hooks = self.root / "shared-hooks"
        self.git(self.workspace, "config", "core.hooksPath", str(custom_hooks))

        with self.assertRaisesRegex(SetupError, "core.hooksPath"):
            self.configure_push()

        self.assertFalse((custom_hooks / "post-commit").exists())

    def test_file_only_state_ignore_rules_do_not_satisfy_setup(self) -> None:
        (self.workspace / ".gitignore").write_text(
            ".de67/state/workspace.json\n",
            encoding="utf-8",
        )

        with self.assertRaisesRegex(SetupError, "whole .de67/state"):
            self.configure_push()

        self.assertFalse((self.workspace / CONFIG_RELATIVE_PATH).exists())

    def test_push_explicitly_disables_force_tags_and_submodule_recursion(self) -> None:
        self.configure_push()
        self.git(self.workspace, "tag", "-a", "local-only", "-m", "local only")
        self.git(self.workspace, "config", "push.followTags", "true")
        self.git(self.workspace, "config", "push.recurseSubmodules", "on-demand")

        with patch("workspace_setup._run_git", wraps=workspace_setup._run_git) as run:
            result = push_checkpoints(self.workspace)

        push_arguments = [
            call.args[1]
            for call in run.call_args_list
            if call.args[1] and call.args[1][0] == "push"
        ]
        self.assertEqual(len(push_arguments), 1)
        self.assertIn("--no-force", push_arguments[0])
        self.assertIn("--no-follow-tags", push_arguments[0])
        self.assertIn("--recurse-submodules=no", push_arguments[0])
        self.assertTrue(result["ok"])
        remote_tag = self.git(
            self.origin,
            "show-ref",
            "--verify",
            "refs/tags/local-only",
            check=False,
        )
        self.assertNotEqual(remote_tag.returncode, 0)

    def test_non_fast_forward_is_rejected_without_losing_local_commit(self) -> None:
        self.configure_push()
        rival = self.root / "rival"
        self.git(self.root, "clone", "--branch", "dev", str(self.origin), str(rival))
        self.git(rival, "config", "user.name", "Rival")
        self.git(rival, "config", "user.email", "rival@example.invalid")
        (rival / "rival.txt").write_text("rival\n", encoding="utf-8")
        self.git(rival, "add", "rival.txt")
        self.git(rival, "commit", "-m", "rival")
        self.git(rival, "push", "origin", "dev")
        remote_head = self.head(self.origin, "refs/heads/dev")

        local_head = self.commit_file("local.txt", "local\n", "local")
        self.assertEqual(self.head(self.workspace), local_head)
        self.assertEqual(self.head(self.origin, "refs/heads/dev"), remote_head)
        self.assertNotEqual(local_head, remote_head)
        status = json.loads(
            (self.workspace / PUSH_STATUS_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertFalse(status["ok"])
        self.assertFalse(status["targets"][0]["ok"])

    def test_unmanaged_hook_is_never_overwritten(self) -> None:
        hook_text = "#!/bin/sh\necho owner\n"
        hook_raw = self.git(
            self.workspace, "rev-parse", "--git-path", "hooks/post-commit"
        ).stdout.strip()
        hook = Path(hook_raw)
        if not hook.is_absolute():
            hook = self.workspace / hook
        hook.parent.mkdir(parents=True, exist_ok=True)
        hook.write_text(hook_text, encoding="utf-8")

        with self.assertRaisesRegex(SetupError, "unmanaged hook"):
            self.configure_push()
        self.assertEqual(hook.read_text(encoding="utf-8"), hook_text)

    def test_managed_marker_does_not_authorize_overwriting_extra_commands(self) -> None:
        result = self.configure_push()
        hook = Path(str(result["hook"]))
        modified = hook.read_text(encoding="utf-8") + "echo user-owned\n"
        hook.write_text(modified, encoding="utf-8")

        with self.assertRaisesRegex(SetupError, "modified managed hook"):
            self.configure_push()
        self.assertEqual(hook.read_text(encoding="utf-8"), modified)

    def test_byte_identical_helper_at_another_path_keeps_managed_hook(self) -> None:
        result = self.configure_push()
        hook = Path(str(result["hook"]))
        alternate = self.root / "alternate" / "workspace_setup.py"
        alternate.parent.mkdir()
        alternate.write_bytes(MODULE_PATH.read_bytes())
        original = hook.read_text(encoding="utf-8")
        hook.write_text(
            original.replace(MODULE_PATH.as_posix(), alternate.as_posix()), encoding="utf-8"
        )
        alternate_hook = hook.read_text(encoding="utf-8")

        self.configure_push()

        self.assertEqual(hook.read_text(encoding="utf-8"), alternate_hook)

    def test_different_helper_at_another_path_is_refused(self) -> None:
        result = self.configure_push()
        hook = Path(str(result["hook"]))
        alternate = self.root / "alternate" / "workspace_setup.py"
        alternate.parent.mkdir()
        alternate.write_text("print('not DE67')\n", encoding="utf-8")
        original = hook.read_text(encoding="utf-8")
        hook.write_text(
            original.replace(MODULE_PATH.as_posix(), alternate.as_posix()), encoding="utf-8"
        )
        modified = hook.read_text(encoding="utf-8")

        with self.assertRaisesRegex(SetupError, "modified managed hook"):
            self.configure_push()
        self.assertEqual(hook.read_text(encoding="utf-8"), modified)

    def test_phase_two_setup_binds_clock_after_frozen_dfs(self) -> None:
        (self.workspace / ".de67").mkdir()
        (self.workspace / ".de67/FS.md").write_text(
            "# Feature DFS\n\nStatus: Refrozen against inspected source baseline\n",
            encoding="utf-8",
        )
        result = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="feature-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(result["clock"]["lineage"], "feature-lineage")
        config = json.loads(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertEqual(config["clock"]["lineage"], "feature-lineage")
        connection = sqlite3.connect(config["clock"]["state"])
        try:
            bound = connection.execute(
                "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
            ).fetchone()
        finally:
            connection.close()
        self.assertEqual(bound, ("feature-lineage",))

    def test_phase_two_records_only_successfully_probed_worker_pairs(self) -> None:
        self.freeze_dfs()
        passed = (("gpt-6-luna", "high"), ("gpt-6-sol", "low"))

        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=passed,
        )

        config = json.loads(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertEqual(
            config["worker_capabilities"],
            [
                {"model": "gpt-6-luna", "reasoning_effort": "high"},
                {"model": "gpt-6-sol", "reasoning_effort": "low"},
            ],
        )

    def test_phase_two_requires_luna_sol_and_distinct_efforts(self) -> None:
        self.freeze_dfs()

        invalid_rosters = (
            (),
            (("gpt-6-luna", "high"),),
            (("gpt-6-luna", "high"), ("gpt-6-sol", "high")),
            (("gpt-5.6-luna", "high"), ("gpt-6-sol", "low")),
            (("gpt-6-luna", "low"), ("gpt-6-luna", "medium")),
            (("gpt-6-luna", "high"), ("gpt-6-sol", "low"), ("gpt-6-luna", "high")),
            (("gpt-6-luna", "high"), ("gpt-6-sol", "invalid effort")),
        )
        for roster in invalid_rosters:
            with self.subTest(roster=roster), self.assertRaises(SetupError):
                configure(
                    self.workspace,
                    [("origin", "dev")],
                    bind_clock=True,
                    worker_capabilities=roster,
                )

        self.assertFalse((self.workspace / CONFIG_RELATIVE_PATH).exists())

    def test_repeated_phase_two_setup_replaces_the_roster_with_fresh_probe_results(self) -> None:
        self.freeze_dfs()
        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )

        replacement = (
            ("gpt-6-luna", "low"),
            ("gpt-6-sol", "medium"),
        )
        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=replacement,
        )

        config = json.loads(
            (self.workspace / CONFIG_RELATIVE_PATH).read_text(encoding="utf-8")
        )
        self.assertEqual(
            config["worker_capabilities"],
            [
                {"model": "gpt-6-luna", "reasoning_effort": "low"},
                {"model": "gpt-6-sol", "reasoning_effort": "medium"},
            ],
        )

    def test_phase_two_setup_initializes_an_empty_sqlite_state_file(self) -> None:
        self.freeze_dfs()
        state_path = self.workspace / DEADLINE_STATE_RELATIVE_PATH
        state_path.parent.mkdir(parents=True, exist_ok=True)
        sqlite3.connect(state_path).close()

        result = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            lineage="empty-state-lineage",
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(result["clock"]["lineage"], "empty-state-lineage")
        connection = sqlite3.connect(state_path)
        try:
            bound = connection.execute(
                "SELECT lineage_id FROM lineage_binding WHERE singleton = 1"
            ).fetchone()
        finally:
            connection.close()
        self.assertEqual(bound, ("empty-state-lineage",))

    def test_phase_two_guidance_source_records_audited_baseline_without_editing_agents(self) -> None:
        self.freeze_dfs()
        agents = self.workspace / "AGENTS.md"
        custom = "# Project notes\n\nKeep this custom instruction.\n"
        agents.write_text(custom, encoding="utf-8")
        global_source = self.root / "global-AGENTS.md"
        global_source.write_text("shared effective rules\n", encoding="utf-8")

        result = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
            guidance_source=global_source,
        )

        self.assertEqual(agents.read_text(encoding="utf-8"), custom)
        guidance = result["guidance"]
        self.assertTrue(guidance["effective"])
        self.assertFalse(guidance["fallback"])
        self.assertEqual(guidance["source"], str(global_source.resolve()))

    def test_phase_two_missing_guidance_source_preserves_custom_agents_without_a_duplicate_copy(self) -> None:
        self.freeze_dfs()
        agents = self.workspace / "AGENTS.md"
        custom = "# Project notes\n\nKeep this custom instruction.\n"
        agents.write_text(custom, encoding="utf-8")

        first = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )
        once = agents.read_text(encoding="utf-8")
        second = configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(once, custom)
        self.assertEqual(agents.read_text(encoding="utf-8"), once)
        self.assertTrue(first["guidance"]["fallback"])
        self.assertFalse(first["guidance"]["effective"])
        self.assertEqual(second["guidance"], first["guidance"])
        self.assertTrue(common_guidance(self.workspace))

    def test_phase_two_reuses_an_unchanged_audited_guidance_source(self) -> None:
        self.freeze_dfs()
        source = self.root / "global-AGENTS.md"
        source.write_text("effective baseline\n", encoding="utf-8")
        first = configure(
            self.workspace, [("origin", "dev")], bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS, guidance_source=source,
        )
        second = configure(
            self.workspace, [("origin", "dev")], bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )
        self.assertEqual(second["guidance"], first["guidance"])

    def test_phase_two_rejects_missing_explicit_guidance_source(self) -> None:
        self.freeze_dfs()
        with self.assertRaisesRegex(SetupError, "Guidance source is not a readable file"):
            configure(
                self.workspace,
                [("origin", "dev")],
                bind_clock=True,
                worker_capabilities=VERIFIED_WORKERS,
                guidance_source=self.root / "missing-AGENTS.md",
            )

    def test_phase_two_guidance_reconciliation_preserves_existing_config_fields(self) -> None:
        self.freeze_dfs()
        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )
        config_path = self.workspace / CONFIG_RELATIVE_PATH
        config = json.loads(config_path.read_text(encoding="utf-8"))
        config["user_preserved_field"] = {"note": "leave this alone"}
        config_path.write_text(json.dumps(config), encoding="utf-8")

        configure(
            self.workspace,
            [("origin", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(
            json.loads(config_path.read_text(encoding="utf-8"))["user_preserved_field"],
            {"note": "leave this alone"},
        )

    def test_default_lineage_uses_primary_upstream_when_origin_is_absent(self) -> None:
        self.git(self.workspace, "remote", "rename", "origin", "primary")
        (self.workspace / ".de67").mkdir()
        (self.workspace / ".de67/FS.md").write_text(
            "# Feature DFS\n\nStatus: Frozen against inspected source baseline\n",
            encoding="utf-8",
        )

        result = configure(
            self.workspace,
            [("primary", "dev")],
            bind_clock=True,
            worker_capabilities=VERIFIED_WORKERS,
        )

        self.assertEqual(result["clock"]["lineage"], "origin:dev")


if __name__ == "__main__":
    unittest.main()
