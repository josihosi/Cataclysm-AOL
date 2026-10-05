#!/usr/bin/env python3
"""Focused POSIX lock and persisted-identity tests using harmless child processes."""
from __future__ import annotations

import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from certification_process_lease import SystemProcessInspector
from writable_run_owner import (
    WritableRootConflict, WritableRunOwner, current_owner, own_writable_root,
)


_SYSTEM_INSPECTOR = SystemProcessInspector()


def inspect_process(pid: int) -> dict:
    snapshot = _SYSTEM_INSPECTOR.inspect(int(pid))
    return {
        "schema": "caol-owned-process-generation-v1",
        "pid": int(pid),
        "alive": bool(snapshot.alive),
        "birth_identity": str(snapshot.birth_identity or ""),
        "command": str(snapshot.command or ""),
        "executable_path": str(snapshot.executable_path or ""),
    }


def matches(expected, observed) -> bool:
    return bool(
        int(expected.get("pid", 0) or 0) == int(observed.get("pid", 0) or 0) > 0
        and expected.get("birth_identity")
        and expected.get("birth_identity") == observed.get("birth_identity")
        and expected.get("command")
        and expected.get("command") == observed.get("command")
    )


def start_waiting_child(owner: WritableRunOwner) -> subprocess.Popen:
    owner.begin_launch()
    child = subprocess.Popen(
        [sys.executable, "-c", "import sys; sys.stdin.buffer.readline()"],
        stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        pass_fds=owner.inherited_fds, start_new_session=True,
    )
    previous = None
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline:
        generation = inspect_process(child.pid)
        if (generation.get("alive") and generation.get("birth_identity") and generation.get("command")
                and previous is not None and matches(previous, generation)
                and previous.get("executable_path") == generation.get("executable_path")):
            break
        previous = generation
        time.sleep(.03)
    try:
        owner.bind_game(generation)
    except BaseException:
        child.stdin.write(b"exit\n")
        child.stdin.flush()
        child.wait(timeout=5)
        child.stdin.close()
        raise
    return child


class WritableRunOwnerTest(unittest.TestCase):
    def test_legacy_or_incomplete_nested_owner_boundaries_reject_before_writes(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            base = Path(raw).resolve()
            child = base / 'child'; child.mkdir()
            marker = child / '.harness-owner.lock'
            marker.write_bytes(b'legacy or incomplete file-only owner')
            (child / 'save').write_bytes(b'original save')
            before = {str(path.relative_to(base)): path.read_bytes()
                      for path in base.rglob('*') if path.is_file()}
            inode = marker.stat().st_ino
            for target in [base, child / 'nested']:
                with self.assertRaises(WritableRootConflict) as caught:
                    WritableRunOwner(target, 'contender', inspect_process, matches)
                self.assertEqual(caught.exception.reason, 'root_overlap')
            self.assertFalse((child / 'nested').exists())
            self.assertFalse((base / '.harness-owner.lock').exists())
            self.assertEqual(marker.stat().st_ino, inode)
            self.assertEqual(before, {str(path.relative_to(base)): path.read_bytes()
                                     for path in base.rglob('*') if path.is_file()})

    def test_independent_roots_can_be_owned_concurrently(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp).resolve()
            first = base / "profile-a"
            second = base / "profile-b"
            with own_writable_root(first, "run-a", inspect_process, matches) as owner_a:
                with own_writable_root(second, "run-b", inspect_process, matches) as owner_b:
                    self.assertIsNotNone(owner_a)
                    self.assertIsNotNone(owner_b)
                    self.assertIs(current_owner(first), owner_a)
                    self.assertIs(current_owner(second), owner_b)
                    self.assertNotEqual(owner_a.path, owner_b.path)

    def test_same_root_refusal_preserves_sentinel_and_inherited_lock(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve() / "profile"
            root.mkdir()
            sentinel = root / "sentinel.bin"
            sentinel.write_bytes(b"before")
            child = None
            try:
                with own_writable_root(root, "owner-run", inspect_process, matches) as owner:
                    child = start_waiting_child(owner)
                    with self.assertRaises(WritableRootConflict) as caught:
                        WritableRunOwner(root, "contender-run", inspect_process, matches)
                    self.assertEqual(caught.exception.reason, "live_lock")
                    self.assertEqual(sentinel.read_bytes(), b"before")
                    self.assertTrue(inspect_process(child.pid)["alive"])
                # Launcher return closes only its descriptor; child still owns
                # the inherited flock and the record remains bound to it.
                self.assertTrue(inspect_process(child.pid)["alive"])
                with self.assertRaises(WritableRootConflict) as caught:
                    WritableRunOwner(root, "after-launcher-return", inspect_process, matches)
                self.assertEqual(caught.exception.reason, "live_lock")
                self.assertEqual(sentinel.read_bytes(), b"before")
            finally:
                if child is not None and child.poll() is None:
                    child.stdin.write(b"exit\n")
                    child.stdin.flush()
                    child.wait(timeout=5)
                if child is not None and child.stdin is not None:
                    child.stdin.close()

            # The prior exact process has exited, so the same root can be
            # serially reused while preserving the permanent lock inode.
            lock_path = root / ".harness-owner.lock"
            lock_inode = lock_path.stat().st_ino
            with own_writable_root(root, "next-run", inspect_process, matches) as next_owner:
                self.assertEqual(next_owner.path.stat().st_ino, lock_inode)
                next_owner.begin_launch()
                next_owner.bind_game(inspect_process(os.getpid()))
            self.assertEqual(sentinel.read_bytes(), b"before")

    def test_prior_incomplete_wrong_host_birth_and_malformed_records_reject(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp).resolve()

            def inspector(pid):
                if int(pid) == 424242:
                    return {"pid": 424242, "alive": True,
                            "birth_identity": "current-birth", "command": "dummy command"}
                return inspect_process(pid)

            launcher = {"schema": "caol-owned-process-generation-v1", "pid": 1,
                        "birth_identity": "b", "command": "c"}
            live_game = {"schema": "caol-owned-process-generation-v1", "pid": 424242,
                         "birth_identity": "current-birth", "command": "dummy command"}
            wrong_birth_game = dict(live_game, birth_identity="old-birth")

            cases = (
                ("incomplete", {"schema": "caol-writable-run-owner-v1", "host": socket.gethostname(),
                                "run_id": "old", "launcher_state": "returned",
                                "launcher_generation": launcher, "launch_state": "bound"},
                 "incomplete_generation"),
                ("wrong-host", {"schema": "caol-writable-run-owner-v1", "host": "other-host",
                                "run_id": "old", "launcher_state": "returned",
                                "launcher_generation": launcher, "game_generation": live_game,
                                "launch_state": "bound"},
                 "wrong_owner_scope"),
                ("wrong-birth", {"schema": "caol-writable-run-owner-v1", "host": socket.gethostname(),
                                 "run_id": "old", "launcher_state": "returned",
                                 "launcher_generation": launcher, "game_generation": wrong_birth_game,
                                 "launch_state": "bound"},
                 "generation_mismatch"),
            )
            for label, prior, reason in cases:
                with self.subTest(label=label):
                    root = base / label
                    root.mkdir()
                    prior["userdir"] = str(root)
                    (root / ".harness-owner.lock").write_text(json.dumps(prior))
                    with self.assertRaises(WritableRootConflict) as caught:
                        WritableRunOwner(root, "new", inspector, matches)
                    self.assertEqual(caught.exception.reason, reason)

            malformed = base / "malformed"
            malformed.mkdir()
            (malformed / ".harness-owner.lock").write_text("{broken json")
            with self.assertRaises(WritableRootConflict) as caught:
                WritableRunOwner(malformed, "new", inspector, matches)
            self.assertEqual(caught.exception.reason, "malformed_owner")

    def test_returned_not_started_owner_is_reusable_but_launching_without_binding_is_ambiguous(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            safe_root = Path(temp).resolve() / "safe-before-launch"
            with own_writable_root(safe_root, "setup-failed", inspect_process, matches):
                pass
            with own_writable_root(safe_root, "next-run", inspect_process, matches) as owner:
                self.assertEqual(owner.record["launch_state"], "not_started")

            ambiguous_root = Path(temp).resolve() / "possibly-launched"
            with own_writable_root(ambiguous_root, "uncertain-run", inspect_process, matches) as owner:
                owner.begin_launch()
            with self.assertRaises(WritableRootConflict) as caught:
                WritableRunOwner(ambiguous_root, "next-run", inspect_process, matches)
            self.assertEqual(caught.exception.reason, "incomplete_owner")

    def test_symlink_root_and_subtree_aliases_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp).resolve()
            real = base / "real"
            real.mkdir()
            alias = base / "alias"
            alias.symlink_to(real, target_is_directory=True)
            with self.assertRaises(WritableRootConflict) as caught:
                WritableRunOwner(alias, "alias-run", inspect_process, matches)
            self.assertEqual(caught.exception.reason, "root_alias")

            cache = real / "cache"
            cache.symlink_to(base, target_is_directory=True)
            with self.assertRaises(WritableRootConflict) as caught:
                WritableRunOwner(real, "subtree-run", inspect_process, matches)
            self.assertEqual(caught.exception.reason, "root_alias")


if __name__ == "__main__":
    unittest.main()
