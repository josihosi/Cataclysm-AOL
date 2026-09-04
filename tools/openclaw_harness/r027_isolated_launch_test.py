#!/usr/bin/env python3
"""Focused non-launching controls for the R-027 isolated runtime selector."""

from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from r027_isolated_launch import R027IsolatedLaunchError, select_r027_isolated_executable


class R027IsolatedLaunchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.binary = self.root / "build" / "r027-receipt-tiles"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b"receipt capable runtime")
        self.binary.chmod(0o755)
        self.receipt = self.root / "build" / "r027-receipt-tiles.build.json"
        self.receipt.write_text(json.dumps({
            "schema": "caol-r027-isolated-build-receipt-v1", "status": "completed",
            "build_returncode": 0, "executable_path": str(self.binary.resolve()),
            "executable_sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest(),
        }), encoding="utf-8")
        self.declaration = {"r027_isolated_launch": {
            "name": "r027-receipt-tiles", "path": "build/r027-receipt-tiles",
            "sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest(),
            "profile": "r027-closure-011", "world": "McWilliams", "fixture": "mcw-fixture",
            "run_identity": "r027-closure-011", "control_endpoint": "semantic-broker",
            "receipt_sidecar": "r027.receipts.jsonl", "cleanup_token": "r027-cleanup-token",
            "build_receipt": "build/r027-receipt-tiles.build.json",
        }}

    def tearDown(self):
        self.temp.cleanup()

    def test_selects_only_digest_bound_named_runtime_with_complete_controls(self):
        selected = select_r027_isolated_executable(self.declaration, repository=self.root)
        self.assertEqual(selected["executable_path"], str(self.binary.resolve()))
        self.assertEqual(selected["profile"], "r027-closure-011")
        self.assertEqual(selected["cleanup_token"], "r027-cleanup-token")

    def test_rejects_shared_stale_escape_and_missing_controls_before_launch(self):
        cases = []
        shared = {"r027_isolated_launch": dict(self.declaration["r027_isolated_launch"], path="cataclysm-tiles")}
        (self.root / "cataclysm-tiles").write_bytes(b"shared")
        (self.root / "cataclysm-tiles").chmod(0o755)
        cases.append(shared)
        stale = {"r027_isolated_launch": dict(self.declaration["r027_isolated_launch"], sha256="0" * 64)}
        cases.append(stale)
        escaped = {"r027_isolated_launch": dict(self.declaration["r027_isolated_launch"], path="../elsewhere")}
        cases.append(escaped)
        missing = {"r027_isolated_launch": {key: value for key, value in self.declaration["r027_isolated_launch"].items() if key != "cleanup_token"}}
        cases.append(missing)
        incomplete = {"r027_isolated_launch": dict(self.declaration["r027_isolated_launch"])}
        self.receipt.write_text(json.dumps({"schema": "caol-r027-isolated-build-receipt-v1",
                                             "status": "interrupted", "build_returncode": 1}), encoding="utf-8")
        cases.append(incomplete)
        for declaration in cases:
            with self.subTest(declaration=declaration):
                with self.assertRaises(R027IsolatedLaunchError):
                    select_r027_isolated_executable(declaration, repository=self.root)


if __name__ == "__main__":
    unittest.main()
