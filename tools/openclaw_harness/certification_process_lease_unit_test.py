#!/usr/bin/env python3
"""Focused controls for atomic certification process/world lease ownership."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from certification_process_lease import (  # noqa: E402
    CertificationLeaseConflict,
    ProcessSnapshot,
    release_certification_lease_handle,
    reserve_certification_lease,
    transfer_certification_lease,
)
from identity_binding import canonical_digest, component_identity  # noqa: E402
from scenario_registry_store import _issue_registry_certification_authority, open_registry  # noqa: E402
import startup_harness as harness  # noqa: E402


class FakeInspector:
    def __init__(self, snapshots):
        self.snapshots = snapshots
        self.signals = []

    def inspect(self, pid):
        return self.snapshots.get(pid, ProcessSnapshot(pid=pid, alive=False))

    def signal(self, pid, sig):
        self.signals.append((pid, sig))


class CertificationProcessLeaseTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.exe = self.root / "cataclysm-tiles"
        self.exe.write_bytes(b"certification-game")
        self.digest = hashlib.sha256(self.exe.read_bytes()).hexdigest()
        self.db = open_registry(str(self.root / "registry.sqlite3"))
        self.inspector = FakeInspector({
            101: self.snapshot(101, "birth-101"),
            202: self.snapshot(202, "birth-202"),
            303: self.snapshot(303, "birth-303"),
            404: self.snapshot(404, "birth-404", executable=self.root / "unrelated-game-like-process"),
        })

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def snapshot(self, pid, birth, executable=None, command=None):
        executable = self.exe if executable is None else executable
        return ProcessSnapshot(pid=pid, alive=True, executable_path=str(executable),
                               birth_identity=birth,
                               command=command or f"{executable} --world certification")

    def manifest(self, round_id):
        names = ("worktree", "executable", "data_config", "harness", "scenario", "fixture", "profile", "world_save", "player", "actors")
        authoritative = {name: {"identity": name} for name in names}
        authoritative["executable"] = {"path": str(self.exe), "identity": "executable",
                                        "content_sha256": self.digest}
        authoritative["scenario"] = {"identity": "scenario", "content_sha256": "a" * 64}
        authoritative["world_save"] = {"world": str(self.root / "world-A"), "identity": "world-A"}
        components = {name: component_identity(name, authoritative[name]) for name in names}
        binding = {"schema": 1, "components": components, "authoritative_components": authoritative}
        binding["sha256"] = canonical_digest({key: value["sha256"] for key, value in components.items()}, domain="caol-complete-binding:v1")
        result = {"schema": 1, "version": 1, "round_id": round_id, "scenario_lineage_id": "lineage",
                  "authority_id": "authority", "authority_kind": "automated-certification", "event_stream_id": "stream-" + round_id,
                  "event_stream_schema": 1, "binding_id": binding["sha256"], "binding": binding}
        result["manifest_sha256"] = canonical_digest(result, domain="caol-round-manifest:v1")
        authority = _issue_registry_certification_authority(
            self.db, round_id=round_id, binding_id=binding["sha256"],
            source_sha256="a" * 64, launch_token="lease-test-token",
        )
        result["authority_id"] = authority["authority_id"]
        result["manifest_sha256"] = canonical_digest(
            {key: value for key, value in result.items() if key != "manifest_sha256"},
            domain="caol-round-manifest:v1",
        )
        return result

    def reserve_and_bind(self, round_id="round-a", lease_id="lease-a", pid=101):
        manifest = self.manifest(round_id)
        reserved = reserve_certification_lease(self.db, manifest=manifest, lease_id=lease_id,
                                               executable_path=str(self.exe), executable_sha256=self.digest,
                                               inspector=self.inspector)
        bound = transfer_certification_lease(self.db, manifest=manifest, lease_id=lease_id, pid=pid,
                                             executable_path=str(self.exe), executable_sha256=self.digest,
                                             inspector=self.inspector)
        return manifest, reserved, bound

    def test_atomic_conflict_and_identical_reservation_are_safe(self):
        manifest = self.manifest("round-a")
        reserved = reserve_certification_lease(self.db, manifest=manifest, lease_id="lease-a",
                                               executable_path=str(self.exe), executable_sha256=self.digest,
                                               inspector=self.inspector)
        again = reserve_certification_lease(self.db, manifest=manifest, lease_id="lease-a",
                                            executable_path=str(self.exe), executable_sha256=self.digest,
                                            inspector=self.inspector)
        self.assertTrue(again["idempotent"])
        self.assertEqual(reserved["state"], "reserved")
        transfer_certification_lease(self.db, manifest=manifest, lease_id="lease-a", pid=101,
                                     executable_path=str(self.exe), executable_sha256=self.digest,
                                     inspector=self.inspector)
        with self.assertRaises(CertificationLeaseConflict):
            reserve_certification_lease(self.db, manifest=self.manifest("round-b"), lease_id="lease-b",
                                        executable_path=str(self.exe), executable_sha256=self.digest,
                                        inspector=self.inspector)

    def test_stale_pid_reuse_is_recovered_without_signal(self):
        manifest, _, bound = self.reserve_and_bind()
        self.inspector.snapshots[101] = self.snapshot(101, "reused-birth")
        other = reserve_certification_lease(self.db, manifest=self.manifest("round-b"), lease_id="lease-b",
                                            executable_path=str(self.exe), executable_sha256=self.digest,
                                            inspector=self.inspector)
        self.assertEqual(other["state"], "reserved")
        self.assertEqual(self.inspector.signals, [])
        events = self.db.execute("SELECT event_kind FROM certification_round_lease_history WHERE round_id = ? ORDER BY event_sequence", (manifest["round_id"],)).fetchall()
        self.assertIn("stale_recovered", [row[0] for row in events])
        self.assertEqual(bound["pid"], 101)

    def test_same_round_stale_pid_reuse_cannot_be_reserved_for_relaunch(self):
        manifest, _, _bound = self.reserve_and_bind()
        self.inspector.snapshots[101] = self.snapshot(101, "reused-birth")
        with self.assertRaisesRegex(Exception, "reused or replaced"):
            reserve_certification_lease(self.db, manifest=manifest, lease_id="lease-a",
                                        executable_path=str(self.exe), executable_sha256=self.digest,
                                        inspector=self.inspector)
        self.assertEqual(self.inspector.signals, [])

    def test_release_rejects_wrong_lease_round_world_or_executable(self):
        manifest, _, bound = self.reserve_and_bind()
        wrong_world = dict(bound, world_identity="other-world")
        self.assertEqual(release_certification_lease_handle(self.db, manifest=manifest, lease=wrong_world, inspector=self.inspector)["status"], "rejected_world_identity")
        wrong_exe = dict(bound, executable_path=str(self.root / "other"))
        self.assertEqual(release_certification_lease_handle(self.db, manifest=manifest, lease=wrong_exe, inspector=self.inspector)["status"], "rejected_stored_identity")
        wrong_lease = dict(bound, lease_id="other-lease")
        self.assertEqual(release_certification_lease_handle(self.db, manifest=manifest, lease=wrong_lease, inspector=self.inspector)["status"], "rejected_lease_unknown")
        self.assertEqual(release_certification_lease_handle(self.db, manifest=self.manifest("round-b"), lease=bound, inspector=self.inspector)["status"], "rejected_lease_unknown")
        self.assertEqual(self.inspector.signals, [])

    def test_safe_release_and_explicit_same_round_relaunch_transfer(self):
        manifest, _, bound = self.reserve_and_bind()
        self.inspector.snapshots[101] = ProcessSnapshot(pid=101, alive=False)
        reserved = reserve_certification_lease(self.db, manifest=manifest, lease_id="lease-a",
                                               executable_path=str(self.exe), executable_sha256=self.digest,
                                               inspector=self.inspector)
        self.assertTrue(reserved["relaunch_transfer"])
        self.assertEqual(reserved["state"], "reserved")
        relaunched = transfer_certification_lease(self.db, manifest=manifest, lease_id="lease-a", pid=202,
                                                  executable_path=str(self.exe), executable_sha256=self.digest,
                                                  inspector=self.inspector)
        self.assertEqual(relaunched["pid"], 202)
        released = release_certification_lease_handle(self.db, manifest=manifest, lease=relaunched,
                                                      inspector=self.inspector)
        self.assertEqual(released["status"], "signaled")
        self.assertEqual([pid for pid, _ in self.inspector.signals], [202])
        self.assertNotEqual(bound["process_birth_identity"], relaunched["process_birth_identity"])

    def test_unrelated_game_like_process_is_never_selected_or_signalled(self):
        manifest, _, bound = self.reserve_and_bind()
        unrelated = self.inspector.snapshots[404]
        self.assertIn("game", unrelated.executable_path)
        rejected = release_certification_lease_handle(self.db, manifest=manifest,
                                                       lease=dict(bound, pid=404, process_birth_identity="birth-404"),
                                                       inspector=self.inspector)
        self.assertEqual(rejected["status"], "rejected_stored_identity")
        self.assertEqual(self.inspector.signals, [])

    def test_startup_route_requires_explicit_registry_manifest_and_lease(self):
        manifest_path = self.root / "round.manifest.json"
        manifest_path.write_text(json.dumps(self.manifest("round-a")), encoding="utf-8")
        args = Namespace(
            certification_registry=str(self.root / "startup-registry.sqlite3"),
            certification_round_manifest=str(manifest_path),
            certification_lease_id="lease-a",
        )
        context = harness.certification_startup_lease_context(
            args, executable=self.exe, target_world="world-A"
        )
        self.assertEqual(context["lease_id"], "lease-a")
        context["connection"].close()
        with self.assertRaisesRegex(SystemExit, "requires explicit"):
            harness.certification_startup_lease_context(
                Namespace(certification_registry="registry-only", certification_round_manifest="", certification_lease_id=""),
                executable=self.exe, target_world="world-A",
            )


if __name__ == "__main__":
    unittest.main()
