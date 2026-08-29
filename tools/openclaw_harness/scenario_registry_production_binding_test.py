#!/usr/bin/env python3
"""Production route bindings retain every declared inherited setup owner."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_cli  # noqa: E402
import startup_harness  # noqa: E402


class ProductionBindingTest(unittest.TestCase):
    def write_json(self, path: Path, value: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value, sort_keys=True), encoding="utf-8")

    def test_source_chain_reuses_one_resolved_directory_within_observation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "shared"
            source.mkdir()
            cache = {}
            with mock.patch.object(startup_harness, "path_sha256", return_value="a" * 64) as digest:
                binding = startup_harness.source_chain_binding(
                    [("one", "shared"), ("two", "shared")],
                    lambda _profile: root,
                    kind="test-source-chain",
                    content_identity_cache=cache,
                )
            self.assertEqual(digest.call_count, 1)
            self.assertEqual(
                [entry["source_sha256"] for entry in binding["source_chain"]],
                ["a" * 64, "a" * 64],
            )

    def test_source_chain_reobserves_changed_directory_in_next_observation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "shared"
            source.mkdir()
            with mock.patch.object(startup_harness, "path_sha256", side_effect=["a" * 64, "b" * 64]) as digest:
                first = startup_harness.source_chain_binding(
                    [("one", "shared")], lambda _profile: root, kind="test-source-chain",
                    content_identity_cache={},
                )
                second = startup_harness.source_chain_binding(
                    [("one", "shared")], lambda _profile: root, kind="test-source-chain",
                    content_identity_cache={},
                )
            self.assertEqual(digest.call_count, 2)
            self.assertNotEqual(first["sha256"], second["sha256"])

    def test_fixture_adapter_rejects_changed_outer_fixture_owner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            outer = root / "live-debug" / "outer"
            inner = root / "live-debug" / "inner"
            self.write_json(outer / "manifest.json", {
                "source_fixture": "inner", "source_profile": "live-debug", "setup": "first",
            })
            self.write_json(inner / "manifest.json", {"name": "inner"})
            (inner / "save" / "McWilliams").mkdir(parents=True)
            (inner / "save" / "McWilliams" / "worldoptions.json").write_text("{}", encoding="utf-8")

            with mock.patch.object(startup_harness, "profile_fixture_root", side_effect=lambda profile: root / profile):
                binding = startup_harness.fixture_source_binding("outer", "live-debug")
                expected = {
                    "fixture": "outer",
                    "fixture_profile": "live-debug",
                    "installed": {"binding": binding},
                }
                self.assertEqual(scenario_registry_cli._fixture_adapter(expected)["status"], "compatible")
                self.write_json(outer / "manifest.json", {
                    "source_fixture": "inner", "source_profile": "live-debug", "setup": "changed",
                })
                result = scenario_registry_cli._fixture_adapter(expected)

            self.assertEqual(result["status"], "stale")
            self.assertEqual(result["facts"]["reason"], "fixture source-chain binding mismatch")

    def test_profile_adapter_rejects_changed_outer_snapshot_owner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            outer = root / "live-debug" / "outer"
            inner = root / "live-debug" / "inner"
            self.write_json(outer / "manifest.json", {
                "source_snapshot": "inner", "source_profile": "live-debug", "setup": "first",
            })
            self.write_json(inner / "manifest.json", {"name": "inner"})
            (inner / "config").mkdir(parents=True)
            (inner / "config" / "options.json").write_text("{}", encoding="utf-8")

            with mock.patch.object(startup_harness, "profile_snapshot_root", side_effect=lambda profile: root / profile):
                binding = startup_harness.profile_snapshot_source_binding("outer", "live-debug")
                expected = {
                    "profile_snapshot": "outer",
                    "profile_snapshot_profile": "live-debug",
                    "snapshot_install": {"binding": binding},
                }
                self.assertEqual(scenario_registry_cli._profile_adapter(expected)["status"], "compatible")
                self.write_json(outer / "manifest.json", {
                    "source_snapshot": "inner", "source_profile": "live-debug", "setup": "changed",
                })
                result = scenario_registry_cli._profile_adapter(expected)

            self.assertEqual(result["status"], "stale")
            self.assertEqual(result["facts"]["reason"], "profile source-chain binding mismatch")

    def test_fixture_current_observation_is_shared_but_expected_bindings_are_independent(self) -> None:
        cache = {}
        current = {"sha256": "a" * 64, "requested_fixture": "same", "requested_fixture_profile": "live-debug"}
        resolved = {"fixture_dir": "/fixture", "fixture": "same", "fixture_profile": "live-debug"}
        with (
            mock.patch.object(scenario_registry_cli, "resolve_fixture_payload", return_value=resolved) as resolve,
            mock.patch.object(scenario_registry_cli, "fixture_source_binding", return_value=current) as bind,
        ):
            compatible = scenario_registry_cli._fixture_adapter({
                "fixture": "same", "fixture_profile": "live-debug",
                "installed": {"binding": {"sha256": "a" * 64}},
            }, observation_cache=cache)
            stale = scenario_registry_cli._fixture_adapter({
                "fixture": "same", "fixture_profile": "live-debug",
                "installed": {"binding": {"sha256": "b" * 64}},
            }, observation_cache=cache)
        self.assertEqual(compatible["status"], "compatible")
        self.assertEqual(stale["status"], "stale")
        self.assertEqual(stale["facts"]["reason"], "fixture source-chain binding mismatch")
        resolve.assert_called_once_with("same", "live-debug")
        bind.assert_called_once_with("same", "live-debug", content_identity_cache={})

    def test_profile_current_observation_is_shared_but_expected_bindings_are_independent(self) -> None:
        cache = {}
        current = {"sha256": "c" * 64, "requested_snapshot": "same", "requested_snapshot_profile": "live-debug"}
        resolved = {"snapshot_dir": "/snapshot", "snapshot": "same", "snapshot_profile": "live-debug"}
        with (
            mock.patch.object(scenario_registry_cli, "resolve_profile_snapshot_payload", return_value=resolved) as resolve,
            mock.patch.object(scenario_registry_cli, "profile_snapshot_source_binding", return_value=current) as bind,
        ):
            compatible = scenario_registry_cli._profile_adapter({
                "profile_snapshot": "same", "profile_snapshot_profile": "live-debug",
                "snapshot_install": {"binding": {"sha256": "c" * 64}},
            }, observation_cache=cache)
            stale = scenario_registry_cli._profile_adapter({
                "profile_snapshot": "same", "profile_snapshot_profile": "live-debug",
                "snapshot_install": {"binding": {"sha256": "d" * 64}},
            }, observation_cache=cache)
        self.assertEqual(compatible["status"], "compatible")
        self.assertEqual(stale["status"], "stale")
        self.assertEqual(stale["facts"]["reason"], "profile source-chain binding mismatch")
        resolve.assert_called_once_with("same", "live-debug")
        bind.assert_called_once_with("same", "live-debug", content_identity_cache={})


if __name__ == "__main__":
    unittest.main()
