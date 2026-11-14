#!/usr/bin/env python3
"""Check the canonical R050 save as Git would package it from a clean index."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path("tools/openclaw_harness/fixtures/saves/live-debug/r050_rural_roof_bandit_only_v1")
WORLD = FIXTURE / "save/TestSetup00"
PLAYER = "#VGVzdDAw.sav.zzip"


class R050FixturePackagingTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="r050-packaging-index-")
        self.addCleanup(temporary.cleanup)
        self.environment = {**os.environ, "GIT_INDEX_FILE": str(Path(temporary.name) / "index")}
        self.git("read-tree", "--empty")
        self.git("add", "--", FIXTURE.as_posix())
        self.manifest = json.loads((ROOT / FIXTURE / "manifest.json").read_text(encoding="utf-8"))

    def git(self, *arguments: str) -> bytes:
        return subprocess.run(
            ["git", *arguments], cwd=ROOT, env=self.environment,
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        ).stdout

    def packaged_world(self) -> dict[str, bytes]:
        result = {}
        for entry in self.git("ls-files", "--stage", "-z", "--", WORLD.as_posix()).split(b"\0"):
            if not entry:
                continue
            metadata, path = entry.split(b"\t", 1)
            _mode, object_id, stage = metadata.split()
            self.assertEqual(stage, b"0")
            relative = Path(os.fsdecode(path)).relative_to(WORLD).as_posix()
            result[relative] = self.git("cat-file", "blob", object_id.decode("ascii"))
        return result

    def assert_canonical_world(self, payload: dict[str, bytes]) -> None:
        self.assertTrue(PLAYER in payload,
                        "clean-index package is missing the declared initial player")
        self.assertEqual(hashlib.sha256(payload[PLAYER]).hexdigest(),
                         self.manifest["saved_player_sha256"], "initial player bytes changed")
        self.assertEqual(len(payload), self.manifest["saved_file_count"])
        digest = hashlib.sha256()
        for name, data in sorted(payload.items()):
            relative = name.encode("utf-8")
            digest.update(len(relative).to_bytes(8, "big"))
            digest.update(relative)
            digest.update(data)
        self.assertEqual(digest.hexdigest(), self.manifest["saved_sha256_tree"],
                         "packaged world differs from the canonical initial save")

    def test_clean_index_contains_the_complete_canonical_initial_save(self) -> None:
        self.assert_canonical_world(self.packaged_world())

    def test_local_player_does_not_hide_its_absence_from_the_package(self) -> None:
        self.assertTrue((ROOT / WORLD / PLAYER).is_file())
        self.git("update-index", "--force-remove", "--", (WORLD / PLAYER).as_posix())
        with self.assertRaisesRegex(AssertionError, "missing the declared initial player"):
            self.assert_canonical_world(self.packaged_world())

    def test_replacement_player_blob_is_rejected(self) -> None:
        # Git index expressions use repository paths on every host, including Windows.
        other_blob = self.git("rev-parse", ":" + (WORLD / "master.gsav").as_posix()).strip().decode("ascii")
        self.git("update-index", "--cacheinfo", "100644," + other_blob + "," + (WORLD / PLAYER).as_posix())
        with self.assertRaisesRegex(AssertionError, "initial player bytes changed"):
            self.assert_canonical_world(self.packaged_world())

    def test_unrelated_character_and_log_files_remain_ignored(self) -> None:
        for path in [WORLD / "#unrelated.sav.zzip", WORLD / "debug.log",
                     WORLD / "#VGVzdDAw.mm1/#unrelated.zzip",
                     WORLD / "#VGVzdDAw.mm1/unrelated.bin"]:
            with self.subTest(path=path):
                self.git("check-ignore", "--quiet", "--no-index", "--", path.as_posix())


if __name__ == "__main__":
    unittest.main()
