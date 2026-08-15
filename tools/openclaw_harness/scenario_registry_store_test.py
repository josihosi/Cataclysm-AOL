#!/usr/bin/env python3
"""Focused schema and transaction contracts for scenario_registry_store."""

from __future__ import annotations

import hashlib
import json
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
    claim_migration_item_launch,
    build_registry_query_candidate_snapshot,
    approve_retirement,
    execute_retirement_action,
    immediate_transaction,
    migration_item_current,
    open_registry,
    quarantine_scenario,
    record_migration_attempt,
    record_migration_terminal,
    rebuild_manifest_projection,
    registry_status,
    retirement_candidates,
    resolve_registry_path,
    snapshot_migration_run,
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
        "retirement_action_history",
        "schema_migration_history",
        "migration_run",
        "migration_item",
        "migration_run_event",
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
            migration_indexes = {
                row[1] for row in connection.execute("PRAGMA index_list( migration_item )")
            }
            self.assertTrue({
                "migration_item_one_snapshot",
                "migration_item_one_attempt",
                "migration_item_one_launch_claim",
                "migration_item_one_terminal",
            } <= migration_indexes)
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

    def test_quarantine_owner_is_idempotent_invalidates_tokens_and_preserves_sources(self) -> None:
        outcomes = ("invalid", "blocked", "broken", "contradicted", "stale")
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            connection = open_registry(str(root / "registry.sqlite3"))
            source_bytes: dict[str, bytes] = {}
            declarations: dict[str, str] = {}
            with immediate_transaction(connection):
                for outcome in outcomes:
                    source_path = root / f"{outcome}.json"
                    source_bytes[outcome] = ("{\"outcome\":\"" + outcome + "\"}\n").encode("utf-8")
                    source_path.write_bytes(source_bytes[outcome])
                    manifest_id = f"manifest-{outcome}"
                    declarations[outcome] = "{\"retained\":true}"
                    source_sha256 = hashlib.sha256(source_bytes[outcome]).hexdigest()
                    connection.execute(
                        "INSERT INTO manifest_current( manifest_id, source_path, present, revision, current_sha256, "
                        "last_content_sha256, declaration_json, normalized_json, validation_json, last_seen_at ) "
                        "VALUES( ?, ?, 1, 1, ?, ?, ?, '{}', '{}', CURRENT_TIMESTAMP )",
                        (manifest_id, str(source_path.resolve()), source_sha256, source_sha256, declarations[outcome]),
                    )
                    connection.execute(
                        "INSERT INTO token_history( token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json ) "
                        "VALUES( ?, ?, NULL, 'route', 'issued', 'test', '{}' )",
                        (f"token-{outcome}", manifest_id),
                    )

            for outcome in outcomes:
                source_path = root / f"{outcome}.json"
                result = quarantine_scenario(
                    connection,
                    reason=outcome,
                    source_path=source_path,
                    source_sha256=hashlib.sha256(source_bytes[outcome]).hexdigest(),
                    details={"injected_outcome": outcome},
                )
                self.assertTrue(result["quarantined"])
                self.assertEqual(result["invalidated_tokens"], 1)
                self.assertEqual(source_path.read_bytes(), source_bytes[outcome])

            before_replay = {
                table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("quarantine_history", "lifecycle_history", "token_history")
            }
            replay = quarantine_scenario(
                connection,
                reason="invalid",
                source_path=root / "invalid.json",
                source_sha256=hashlib.sha256(source_bytes["invalid"]).hexdigest(),
                details={"injected_outcome": "invalid"},
            )
            self.assertTrue(replay["quarantined"])
            self.assertEqual(replay["invalidated_tokens"], 0)
            self.assertEqual(
                {
                    table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                    for table in before_replay
                },
                before_replay,
            )
            self.assertEqual(
                connection.execute("SELECT COUNT(*) FROM retirement_history").fetchone()[0],
                0,
            )
            self.assertEqual(
                {
                    str(row["source_path"]): str(row["declaration_json"])
                    for row in connection.execute("SELECT source_path, declaration_json FROM manifest_current")
                },
                {str((root / f"{outcome}.json").resolve()): declarations[outcome] for outcome in outcomes},
            )
            self.assertEqual(build_registry_query_candidate_snapshot(connection), ())
            quarantined = build_registry_query_candidate_snapshot(
                connection,
                include_lifecycle_states=("quarantined",),
            )
            self.assertEqual(len(quarantined), len(outcomes))
            self.assertTrue(all(not entry.token_eligible for entry in quarantined))
            connection.close()

    def test_source_absence_is_not_retirement(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            source_path = scenarios / "absent.json"
            source_path.write_text('{"name":"absent","steps":[]}\n', encoding="utf-8")
            connection = open_registry(str(root / "registry.sqlite3"))
            rebuild_manifest_projection(connection, scenarios)
            source_path.unlink()
            rebuild_manifest_projection(connection, scenarios)
            self.assertEqual(build_registry_query_candidate_snapshot(connection), ())
            absent = build_registry_query_candidate_snapshot(
                connection,
                include_lifecycle_states=("absent",),
            )
            self.assertEqual(absent[0].lifecycle_state, "absent")
            self.assertFalse(absent[0].token_eligible)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM retirement_history").fetchone()[0], 0)
            connection.close()

    def test_reviewer_approved_retirement_is_coverage_guarded_and_resumable(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            subject_path = root / "subject.json"
            successor_path = root / "successor.json"
            source_bytes = b'{"retirement":"fixture"}\n'
            subject_path.write_bytes(source_bytes)
            successor_path.write_bytes(source_bytes)
            source_sha256 = hashlib.sha256(source_bytes).hexdigest()
            connection = open_registry(str(root / "registry.sqlite3"))
            with immediate_transaction(connection):
                for manifest_id, source_path in (("subject", subject_path), ("successor", successor_path)):
                    connection.execute(
                        "INSERT INTO manifest_current( manifest_id, source_path, present, revision, current_sha256, "
                        "last_content_sha256, declaration_json, normalized_json, validation_json, last_seen_at ) "
                        "VALUES( ?, ?, 1, 1, ?, ?, '{\"name\":\"retirement.fixture\"}', '{}', '{}', CURRENT_TIMESTAMP )",
                        (manifest_id, str(source_path.resolve()), source_sha256, source_sha256),
                    )
                    connection.execute(
                        "INSERT INTO manifest_capability_current( manifest_id, capability_key, value_json, declared_state, review_required ) "
                        "VALUES( ?, 'player.required', 'true', 'declared', 0 )",
                        (manifest_id,),
                    )
                    connection.executemany(
                        "INSERT INTO manifest_proof_route_current( manifest_id, route_role, step_label ) VALUES( ?, ?, ? )",
                        [(manifest_id, "production_behavior", "production"),
                         (manifest_id, "disallowed_shortcuts", "shortcut"),
                         (manifest_id, "failure_control", "failure")],
                    )
                connection.execute(
                    "INSERT INTO manifest_relation_current( manifest_id, relation_kind, target_kind, target_key, route_role ) "
                    "VALUES( 'subject', 'exact_duplicate_candidate', 'manifest', 'successor', '' )"
                )

            before = {
                table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("lifecycle_history", "retirement_history", "retirement_action_history")
            }
            candidates = retirement_candidates(connection)
            self.assertEqual(candidates[0]["manifest_id"], "subject")
            self.assertIn("exact_duplicate", candidates[0]["reasons"])
            self.assertEqual(
                {table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0] for table in before},
                before,
            )
            approval_args = {
                "manifest_id": "subject",
                "successor_manifest_id": "successor",
                "source_sha256": source_sha256,
                "reason": "exact_duplicate",
                "reviewer_identity": "reviewer@example.test",
                "approval": "approved",
            }
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "explicit reviewer approval"):
                approve_retirement(connection, **{**approval_args, "approval": "pending"})
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "SHA-256 is stale"):
                approve_retirement(connection, **{**approval_args, "source_sha256": "0" * 64})
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "owner-approved"):
                approve_retirement(connection, **{**approval_args, "reason": "relation_only"})
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "not supported"):
                approve_retirement(connection, **{**approval_args, "successor_manifest_id": "missing"})
            with immediate_transaction(connection):
                connection.execute("UPDATE manifest_current SET present = 0, current_sha256 = NULL, absent_at = 'test' WHERE manifest_id = 'successor'")
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "successor is not source-present"):
                approve_retirement(connection, **approval_args)
            with immediate_transaction(connection):
                connection.execute(
                    "UPDATE manifest_current SET present = 1, current_sha256 = ?, absent_at = NULL WHERE manifest_id = 'successor'",
                    (source_sha256,),
                )
                connection.execute(
                    "DELETE FROM manifest_proof_route_current WHERE manifest_id = 'successor' AND route_role = 'failure_control'"
                )
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "required active coverage"):
                approve_retirement(connection, **approval_args)
            with immediate_transaction(connection):
                connection.execute(
                    "INSERT INTO manifest_proof_route_current( manifest_id, route_role, step_label ) "
                    "VALUES( 'successor', 'failure_control', 'failure' )"
                )

            prepared = approve_retirement(connection, **approval_args)
            self.assertTrue(prepared["approved"])
            self.assertTrue(subject_path.exists())
            self.assertEqual(
                tuple(entry.scenario_id for entry in build_registry_query_candidate_snapshot(connection)),
                ("successor",),
            )
            status = registry_status(connection, include_lifecycle_states=("quarantined",))
            self.assertEqual(status[0]["lifecycle"]["state"], "quarantined")
            self.assertEqual(status[0]["history"]["actions"][0]["events"][0]["event_kind"], "approved")

            subject_path.write_bytes(b'{"retirement":"changed"}\n')
            failed = execute_retirement_action(connection, str(prepared["action_id"]))
            self.assertFalse(failed["retired"])
            self.assertTrue(failed["resumable"])
            self.assertTrue(subject_path.exists())
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM retirement_history").fetchone()[0], 0)
            subject_path.write_bytes(source_bytes)
            retired = execute_retirement_action(connection, str(prepared["action_id"]))
            self.assertTrue(retired["retired"])
            self.assertFalse(subject_path.exists())
            self.assertTrue(successor_path.exists())
            row = connection.execute(
                "SELECT present, current_sha256, declaration_json, normalized_json FROM manifest_current WHERE manifest_id = 'subject'"
            ).fetchone()
            self.assertEqual(row["present"], 0)
            self.assertIsNone(row["current_sha256"])
            self.assertTrue(row["declaration_json"])
            self.assertTrue(row["normalized_json"])
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM manifest_relation_current WHERE manifest_id = 'subject'").fetchone()[0], 1)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM retirement_history WHERE manifest_id = 'subject'").fetchone()[0], 1)
            repeated = execute_retirement_action(connection, str(prepared["action_id"]))
            self.assertTrue(repeated["idempotent"])
            retired_status = registry_status(connection, include_lifecycle_states=("retired",))
            self.assertEqual(retired_status[0]["lifecycle"]["state"], "retired")
            self.assertGreaterEqual(len(retired_status[0]["history"]["actions"][0]["events"]), 2)
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
                apply_migrations(connection, ((6, "forced_failure", failing_migration),))
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
                    "SELECT version FROM schema_migration_history WHERE version = 6"
                ).fetchone()
            )
            connection.close()

    def test_migration_snapshot_attempt_and_invalid_terminal_are_append_only(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            invalid_path = scenarios / "invalid.json"
            invalid_path.write_text("{ not json", encoding="utf-8")
            connection = open_registry(str(root / "registry.sqlite3"))
            snapshot = snapshot_migration_run(
                connection,
                scenarios,
                launcher_identity="store-test",
            )
            self.assertEqual(len(snapshot.items), 1)
            item = snapshot.items[0]
            self.assertEqual(item.source_path, str(invalid_path.resolve()))
            self.assertEqual(
                item.source_sha256,
                hashlib.sha256(invalid_path.read_bytes()).hexdigest(),
            )
            self.assertEqual(
                connection.execute("SELECT COUNT(*) FROM manifest_current").fetchone()[0],
                0,
            )

            attempted = record_migration_attempt(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=invalid_path,
                source_sha256=item.source_sha256,
            )
            self.assertEqual(attempted.status, "attempted")
            with self.assertRaises(ValueError):
                json.loads(invalid_path.read_text(encoding="utf-8"))
            terminal = record_migration_terminal(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=invalid_path,
                source_sha256=item.source_sha256,
                disposition="invalid",
                reason="json_decode_error",
            )
            self.assertEqual(terminal.status, "invalid")
            self.assertEqual(terminal.terminal_disposition, "invalid")
            self.assertEqual(
                [event.event_kind for event in terminal.history],
                ["snapshot", "attempted", "terminal"],
            )
            self.assertEqual(
                [event.disposition for event in terminal.history],
                ["snapshotted", "attempted", "invalid"],
            )
            connection.close()

    def test_migration_claims_resume_idempotently_and_changed_sha_is_new_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            source_path = scenarios / "scenario.json"
            source_path.write_text("{}", encoding="utf-8")
            connection = open_registry(str(root / "registry.sqlite3"))
            snapshot = snapshot_migration_run(connection, scenarios, launcher_identity="store-test")
            item = snapshot.items[0]
            repeated_snapshot = snapshot_migration_run(connection, scenarios, launcher_identity="store-test")
            self.assertEqual(repeated_snapshot.migration_run_id, snapshot.migration_run_id)

            attempted = record_migration_attempt(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=source_path,
                source_sha256=item.source_sha256,
            )
            self.assertEqual(attempted.status, "attempted")
            resumed = record_migration_attempt(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=source_path,
                source_sha256=item.source_sha256,
            )
            self.assertEqual([event.event_kind for event in resumed.history], ["snapshot", "attempted"])

            claimed = claim_migration_item_launch(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=source_path,
                source_sha256=item.source_sha256,
                launch_identity="canonical-probe:one",
            )
            self.assertTrue(claimed.launch_claimed)
            self.assertEqual(
                len(claim_migration_item_launch(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=source_path,
                    source_sha256=item.source_sha256,
                    launch_identity="canonical-probe:one",
                ).history),
                3,
            )
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "different launch claim"):
                claim_migration_item_launch(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=source_path,
                    source_sha256=item.source_sha256,
                    launch_identity="canonical-probe:two",
                )

            terminal = record_migration_terminal(
                connection,
                migration_run_id=snapshot.migration_run_id,
                source_path=source_path,
                source_sha256=item.source_sha256,
                disposition="failed",
                reason="probe_red",
                details={"report": "one"},
            )
            self.assertEqual(terminal.status, "failed")
            self.assertEqual(
                len(record_migration_terminal(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=source_path,
                    source_sha256=item.source_sha256,
                    disposition="failed",
                    reason="probe_red",
                    details={"report": "one"},
                ).history),
                4,
            )
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "conflicting terminal"):
                record_migration_terminal(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=source_path,
                    source_sha256=item.source_sha256,
                    disposition="verified",
                    reason="probe_green",
                )

            source_path.write_text('{"changed": true}', encoding="utf-8")
            changed = snapshot_migration_run(connection, scenarios, launcher_identity="store-test")
            changed_item = changed.items[0]
            self.assertEqual(changed.migration_run_id, snapshot.migration_run_id)
            self.assertNotEqual(changed_item.attempt_identity, item.attempt_identity)
            self.assertNotEqual(changed_item.source_sha256, item.source_sha256)
            self.assertEqual(
                record_migration_attempt(
                    connection,
                    migration_run_id=changed.migration_run_id,
                    source_path=source_path,
                    source_sha256=changed_item.source_sha256,
                ).status,
                "attempted",
            )
            connection.close()

    def test_each_supported_migration_terminal_disposition_is_queryable(self) -> None:
        dispositions = ("invalid", "blocked", "imported", "verified", "contradicted", "failed")
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            for disposition in dispositions:
                (scenarios / f"{disposition}.json").write_text("{}", encoding="utf-8")
            connection = open_registry(str(root / "registry.sqlite3"))
            snapshot = snapshot_migration_run(connection, scenarios, launcher_identity="store-test")
            self.assertEqual(len(snapshot.items), len(dispositions))
            for item in snapshot.items:
                record_migration_attempt(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=item.source_path,
                    source_sha256=item.source_sha256,
                )
                disposition = Path(item.source_path).stem
                terminal = record_migration_terminal(
                    connection,
                    migration_run_id=snapshot.migration_run_id,
                    source_path=item.source_path,
                    source_sha256=item.source_sha256,
                    disposition=disposition,
                    reason=f"{disposition}_test",
                )
                self.assertEqual(terminal.status, disposition)
                self.assertEqual(
                    migration_item_current(
                        connection,
                        migration_run_id=snapshot.migration_run_id,
                        source_path=item.source_path,
                        source_sha256=item.source_sha256,
                    ).terminal_disposition,
                    disposition,
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
