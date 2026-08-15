#!/usr/bin/env python3
"""Focused schema and transaction contracts for scenario_registry_store."""

from __future__ import annotations

import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from scenario_registry_store import (  # noqa: E402
    SCHEMA_VERSION,
    ScenarioRegistryStoreError,
    apply_migrations,
    immediate_transaction,
    open_registry,
    resolve_registry_path,
)


class ScenarioRegistryStoreTest(unittest.TestCase):
    REQUIRED_TABLES = {
        "manifest_current",
        "manifest_capability_current",
        "manifest_proof_route_current",
        "manifest_relation_current",
        "lifecycle_history",
        "manifest_relation_history",
        "binding_history",
        "report_ingestion_history",
        "verification_history",
        "capability_evidence_history",
        "verification_resolution_history",
        "quarantine_history",
        "token_history",
        "query_history",
        "retirement_history",
        "schema_migration_history",
        "migration_run",
        "migration_item",
    }

    def test_default_and_override_paths(self) -> None:
        default = resolve_registry_path()
        self.assertEqual(
            default,
            HARNESS_DIR.parents[1] / ".userdata" / "openclaw_harness" / "scenario_registry.sqlite3",
        )
        self.assertEqual(
            default.name,
            "scenario_registry.sqlite3",
        )
        self.assertEqual(default.parent.name, "openclaw_harness")
        self.assertEqual(default.parent.parent.name, ".userdata")
        with tempfile.TemporaryDirectory() as temp_dir:
            override = Path(temp_dir) / "nested" / "registry.sqlite3"
            self.assertEqual(resolve_registry_path(str(override)), override.resolve())
            self.assertFalse(override.parent.exists())
            with self.assertRaises(sqlite3.OperationalError):
                open_registry(str(override), writable=False)
            self.assertFalse(override.parent.exists())
            connection = open_registry(str(override))
            connection.close()
            self.assertTrue(override.parent.exists())

    def test_fresh_schema_contains_contract_surface_foreign_keys_indexes_and_version(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "registry.sqlite3"
            connection = open_registry(str(path))
            tables = {
                row[0]
                for row in connection.execute(
                    "SELECT name FROM sqlite_master WHERE type = 'table'"
                )
            }
            self.assertTrue(self.REQUIRED_TABLES <= tables)
            self.assertEqual(connection.execute("PRAGMA user_version").fetchone()[0], SCHEMA_VERSION)
            self.assertEqual(connection.execute("PRAGMA foreign_keys").fetchone()[0], 1)
            manifest_columns = {
                row[1] for row in connection.execute("PRAGMA table_info( manifest_current )")
            }
            self.assertTrue({
                "manifest_id", "source_path", "present", "revision", "current_sha256",
                "last_content_sha256", "declaration_json", "normalized_json", "validation_json",
            } <= manifest_columns)
            self.assertTrue(connection.execute("PRAGMA foreign_key_list( verification_history )").fetchall())
            self.assertTrue(connection.execute("PRAGMA foreign_key_list( token_history )").fetchall())
            self.assertTrue(connection.execute("PRAGMA foreign_key_list( migration_item )").fetchall())
            indexes = {
                row[1] for row in connection.execute("PRAGMA index_list( verification_history )")
            }
            self.assertIn("idx_verification_route", indexes)
            migration_item_columns = {
                row[1] for row in connection.execute("PRAGMA table_info( migration_item )")
            }
            self.assertTrue({
                "migration_run_id", "attempt_identity", "source_path", "source_sha256",
                "event_kind", "completion_status", "disposition", "reason",
            } <= migration_item_columns)
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, details_json ) "
                    "VALUES( 'missing', 'created', 0, '{}' )"
                )
            with immediate_transaction(connection):
                connection.execute(
                    "INSERT INTO manifest_current( "
                    "manifest_id, source_path, present, revision, declaration_json, normalized_json, "
                    "validation_json, absent_at ) VALUES( 'manifest-a', '/a.json', 0, 0, '{}', '{}', '{}', 'absent' )"
                )
                connection.execute(
                    "INSERT INTO lifecycle_history( manifest_id, event_kind, revision, details_json ) "
                    "VALUES( 'manifest-a', 'absent', 0, '{}' )"
                )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute("DELETE FROM lifecycle_history")
            with immediate_transaction(connection):
                connection.execute(
                    "INSERT INTO migration_run( "
                    "migration_run_id, run_identity, launch_status, launch_reason, launcher_identity, details_json ) "
                    "VALUES( 'run-a', 'inventory:a', 'launched', 'operator_request', 'test', '{}' )"
                )
                connection.execute(
                    "INSERT INTO migration_item( "
                    "migration_run_id, manifest_id, attempt_identity, source_path, source_sha256, event_kind, "
                    "completion_status, disposition, reason, details_json ) "
                    "VALUES( 'run-a', 'manifest-a', 'attempt-a', '/a.json', 'hash-a', 'completed', "
                    "'completed', 'retained', 'validated', '{}' )"
                )
            row = connection.execute(
                "SELECT run_identity, source_path, source_sha256, launch_status, completion_status, disposition, reason "
                "FROM migration_run JOIN migration_item USING( migration_run_id )"
            ).fetchone()
            self.assertEqual(
                tuple(row),
                ("inventory:a", "/a.json", "hash-a", "launched", "completed", "retained", "validated"),
            )
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute("DELETE FROM migration_item")
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "UPDATE migration_run SET launch_status = 'rewritten' WHERE migration_run_id = 'run-a'"
                )
            connection.close()

    def test_reopening_is_migration_idempotent_and_history_is_append_only(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "registry.sqlite3"
            connection = open_registry(str(path))
            initial_history = connection.execute(
                "SELECT version, migration_name FROM schema_migration_history"
            ).fetchall()
            connection.close()

            connection = open_registry(str(path))
            repeated_history = connection.execute(
                "SELECT version, migration_name FROM schema_migration_history"
            ).fetchall()
            self.assertEqual(repeated_history, initial_history)
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute("DELETE FROM schema_migration_history WHERE version = 1")
            connection.close()

    def test_forced_migration_and_transaction_failure_roll_back_without_erasing_prior_rows(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "registry.sqlite3"
            connection = open_registry(str(path))
            with immediate_transaction(connection):
                connection.execute(
                    "INSERT INTO manifest_current( "
                    "manifest_id, source_path, present, revision, declaration_json, normalized_json, "
                    "validation_json, absent_at ) VALUES( ?, ?, 0, 0, '{}', '{}', '{}', ? )",
                    ("manifest-a", "/scenario/a.json", "retained"),
                )

            with self.assertRaisesRegex(RuntimeError, "forced transaction failure"):
                with immediate_transaction(connection):
                    connection.execute(
                        "INSERT INTO manifest_current( "
                        "manifest_id, source_path, present, revision, declaration_json, normalized_json, "
                        "validation_json, absent_at ) VALUES( 'manifest-b', '/scenario/b.json', 0, 0, '{}', '{}', '{}', 'absent' )"
                    )
                    raise RuntimeError("forced transaction failure")
            self.assertIsNone(
                connection.execute(
                    "SELECT manifest_id FROM manifest_current WHERE manifest_id = 'manifest-b'"
                ).fetchone()
            )

            def failing_migration(conn: sqlite3.Connection) -> None:
                conn.execute("CREATE TABLE should_not_survive( value TEXT )")
                raise RuntimeError("forced migration failure")

            with self.assertRaisesRegex(RuntimeError, "forced migration failure"):
                apply_migrations(connection, ((3, "forced_failure", failing_migration),))
            self.assertEqual(
                connection.execute("SELECT source_path FROM manifest_current WHERE manifest_id = 'manifest-a'").fetchone()[0],
                "/scenario/a.json",
            )
            self.assertIsNone(
                connection.execute(
                    "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'should_not_survive'"
                ).fetchone()
            )
            self.assertEqual(connection.execute("PRAGMA user_version").fetchone()[0], SCHEMA_VERSION)
            self.assertIsNone(
                connection.execute(
                    "SELECT version FROM schema_migration_history WHERE version = 3"
                ).fetchone()
            )
            connection.close()

    def test_nested_transactions_are_rejected_before_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            connection = open_registry(str(Path(temp_dir) / "registry.sqlite3"))
            with immediate_transaction(connection):
                with self.assertRaises(ScenarioRegistryStoreError):
                    with immediate_transaction(connection):
                        pass
            connection.close()


if __name__ == "__main__":
    unittest.main()
