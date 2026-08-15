#!/usr/bin/env python3
"""Focused transactional projection contracts for scenario_registry_store."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    ScenarioRegistryStoreError,
    open_registry,
    rebuild_manifest_projection,
)
import scenario_registry_store  # noqa: E402


class ScenarioRegistryRebuildTest(unittest.TestCase):
    def strict_manifest(self) -> dict:
        return {
            "manifest_version": 1,
            "name": "strict.fixture",
            "steps": [
                {"label": "setup", "kind": "wait"},
                {"label": "production", "kind": "press"},
                {"label": "terminal", "kind": "audit_log_contains"},
                {"label": "artifact", "kind": "audit_log_contains"},
                {"label": "shortcut", "kind": "audit_log_not_contains"},
            ],
            "capabilities": {
                "player.injured": False,
                "actors.visible": ["scout"],
            },
            "runtime_contract": {
                "permitted_input": ["press:f"],
                "forbidden_input": ["debug:spawn"],
                "setup_only_debug": True,
                "disposable_copy": True,
                "helpers": ["Peekaboo"],
                "permissions": ["accessibility"],
                "platform": ["macos"],
                "profile": "dev-harness",
                "fixture": "fixture-a",
                "requirements": {
                    "os": "macos",
                    "source": "worktree",
                    "executable": "cataclysm-tiles",
                    "profile": "dev-harness",
                    "fixture": "fixture-a",
                    "helper": "Peekaboo",
                    "peekaboo": True,
                    "input": ["press:f"],
                    "ocr": True,
                    "cleanup": True,
                },
                "grants_gameplay_proof": False,
            },
            "proof_route": {
                "precondition": ["setup"],
                "production_behavior": ["production"],
                "terminal_persistence": ["terminal"],
                "artifact_verdict": ["artifact"],
                "disallowed_shortcuts": ["shortcut"],
            },
        }

    def write_manifest(self, root: Path, name: str, value: object) -> Path:
        path = root / f"{name}.json"
        path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        return path

    def test_rebuild_is_idempotent_revises_content_and_retains_absent_source(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            legacy_path = self.write_manifest(
                scenarios,
                "legacy",
                {"name": "legacy.fixture", "description": "original", "steps": [{"label": "wait", "kind": "wait"}]},
            )
            self.write_manifest(scenarios, "strict", self.strict_manifest())
            connection = open_registry(str(root / "registry.sqlite3"))

            first = rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(first, {"staged": 2, "discovered": 2, "changed": 0, "absent": 0})
            first_lifecycle = connection.execute("SELECT COUNT(*) FROM lifecycle_history").fetchone()[0]
            first_relations = connection.execute("SELECT COUNT(*) FROM manifest_relation_history").fetchone()[0]
            strict_capability_count = connection.execute(
                "SELECT COUNT(*) FROM manifest_capability_current WHERE manifest_id = ( "
                "SELECT manifest_id FROM manifest_current WHERE source_path LIKE '%/strict.json' )"
            ).fetchone()[0]
            self.assertEqual(strict_capability_count, 2)
            self.assertEqual(
                connection.execute("SELECT COUNT(*) FROM manifest_proof_route_current").fetchone()[0],
                5,
            )

            second = rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(second, {"staged": 2, "discovered": 0, "changed": 0, "absent": 0})
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM lifecycle_history").fetchone()[0], first_lifecycle)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM manifest_relation_history").fetchone()[0], first_relations)

            legacy = json.loads(legacy_path.read_text(encoding="utf-8"))
            legacy["description"] = "changed"
            legacy_path.write_text(json.dumps(legacy, indent=2) + "\n", encoding="utf-8")
            third = rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(third, {"staged": 2, "discovered": 0, "changed": 1, "absent": 0})
            legacy_row = connection.execute(
                "SELECT revision, declaration_json, last_content_sha256 FROM manifest_current "
                "WHERE source_path = ?",
                (str(legacy_path.resolve()),),
            ).fetchone()
            self.assertEqual(legacy_row["revision"], 2)
            self.assertEqual(legacy_row["declaration_json"], legacy_path.read_text(encoding="utf-8"))
            retained_hash = legacy_row["last_content_sha256"]
            legacy_path.unlink()

            fourth = rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(fourth, {"staged": 1, "discovered": 0, "changed": 0, "absent": 1})
            absent_row = connection.execute(
                "SELECT present, current_sha256, last_content_sha256, declaration_json, normalized_json, validation_json "
                "FROM manifest_current WHERE source_path = ?",
                (str(legacy_path.resolve()),),
            ).fetchone()
            self.assertEqual(absent_row["present"], 0)
            self.assertIsNone(absent_row["current_sha256"])
            self.assertEqual(absent_row["last_content_sha256"], retained_hash)
            self.assertEqual(absent_row["declaration_json"], json.dumps(legacy, indent=2) + "\n")
            self.assertTrue(absent_row["normalized_json"])
            self.assertTrue(absent_row["validation_json"])
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM manifest_capability_current WHERE manifest_id = ( "
                    "SELECT manifest_id FROM manifest_current WHERE source_path = ? )",
                    (str(legacy_path.resolve()),),
                ).fetchone()[0],
                0,
            )
            self.assertEqual(
                connection.execute(
                    "SELECT event_kind FROM lifecycle_history WHERE manifest_id = ( "
                    "SELECT manifest_id FROM manifest_current WHERE source_path = ? ) ORDER BY lifecycle_event_id",
                    (str(legacy_path.resolve()),),
                ).fetchall()[-1][0],
                "absence",
            )
            connection.close()

    def test_invalid_source_stages_before_transaction_and_cannot_partially_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            self.write_manifest(scenarios, "legacy", {"name": "legacy", "steps": []})
            connection = open_registry(str(root / "registry.sqlite3"))
            rebuild_manifest_projection(connection, scenarios)
            before = connection.execute(
                "SELECT source_path, revision, current_sha256, declaration_json FROM manifest_current"
            ).fetchall()
            self.write_manifest(scenarios, "invalid", ["not", "an", "object"])

            with self.assertRaises(ScenarioRegistryStoreError):
                rebuild_manifest_projection(connection, scenarios)

            self.assertEqual(
                connection.execute(
                    "SELECT source_path, revision, current_sha256, declaration_json FROM manifest_current"
                ).fetchall(),
                before,
            )
            self.assertIsNone(
                connection.execute(
                    "SELECT manifest_id FROM manifest_current WHERE source_path LIKE '%/invalid.json'"
                ).fetchone()
            )
            self.assertFalse((scenarios / "scenario_registry.sqlite3").exists())
            connection.close()

    def test_source_changed_during_validation_cannot_partially_commit(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            path = self.write_manifest(scenarios, "legacy", {"name": "legacy", "steps": []})
            connection = open_registry(str(root / "registry.sqlite3"))
            rebuild_manifest_projection(connection, scenarios)
            before = connection.execute(
                "SELECT source_path, revision, current_sha256, declaration_json FROM manifest_current"
            ).fetchall()
            real_validate = scenario_registry_store.validate_manifest

            def mutate_after_validation(value: object, *, path: Path) -> dict:
                result = real_validate(value, path=path)
                path.write_text(
                    json.dumps({"name": "legacy", "description": "mutated", "steps": []}, indent=2) + "\n",
                    encoding="utf-8",
                )
                return result

            with mock.patch("scenario_registry_store.validate_manifest", side_effect=mutate_after_validation):
                with self.assertRaisesRegex(ScenarioRegistryStoreError, "changed while staging"):
                    rebuild_manifest_projection(connection, scenarios)

            self.assertEqual(
                connection.execute(
                    "SELECT source_path, revision, current_sha256, declaration_json FROM manifest_current"
                ).fetchall(),
                before,
            )
            self.assertNotEqual(path.read_text(encoding="utf-8"), before[0]["declaration_json"])
            connection.close()


if __name__ == "__main__":
    unittest.main()
