#!/usr/bin/env python3
"""Run authority stays usable when evidence-token eligibility is absent."""

from __future__ import annotations

from contextlib import closing, contextmanager
import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
import scenario_registry_store as registry_store  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    create_source_bound_scenario,
    execute_registry_query,
    final_gate_eligibility,
    open_registry,
    parse_registry_query_request,
)


def _manifest(name: str) -> dict[str, object]:
    label = "observe"
    return {
        "manifest_version": 1,
        "name": name,
        "profile": "dev-harness",
        "world": "AuthorityTest",
        "fixture": "fixture",
        "fixture_profile": "live-debug",
        "capabilities": {"capabilities.cockpit_run_open": True},
        "runtime_contract": {
            "permitted_input": ["cockpit:game.observe"],
            "forbidden_input": ["debug:inject_report"],
            "setup_only_debug": True,
            "disposable_copy": True,
            "helpers": ["none"],
            "permissions": ["none"],
            "platform": ["macos"],
            "profile": "dev-harness",
            "fixture": "fixture",
            "requirements": {
                "os": "macos",
                "source": "current-worktree",
                "profile": "dev-harness",
                "fixture": "fixture",
                "helper": "none",
                "peekaboo": False,
                "ocr": False,
                "input": ["cockpit:game.observe"],
                "cleanup": True,
            },
            "grants_gameplay_proof": False,
        },
        "steps": [{"kind": "observe", "label": label}],
        "proof_route": {
            "precondition": [label],
            "production_behavior": [label],
            "terminal_persistence": [label],
            "artifact_verdict": [label],
            "disallowed_shortcuts": [label],
        },
    }


class CockpitRunAuthorityTest(unittest.TestCase):
    def _select(self, connection, preference: str):
        execution = execute_registry_query(
            connection,
            parse_registry_query_request({
                "requirements": [{
                    "key": "capabilities.cockpit_run_open",
                    "op": "eq",
                    "value": True,
                    "minimum_evidence": "declared",
                }],
                "preferences": [{
                    "key": "runtime.profile",
                    "op": "eq",
                    "value": preference,
                    "minimum_evidence": "declared",
                }],
            }),
            drafts_root=self.root / "drafts",
        )
        self.assertIsNone(execution.token_id)
        self.assertIsNotNone(execution.selection_id)
        return str(execution.selection_id)

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.registry = self.root / "registry.sqlite3"
        self.scenarios = self.root / "scenarios"
        (self.root / "game").write_bytes(b"bound executable v1")
        with closing(open_registry(str(self.registry))) as connection:
            create_source_bound_scenario(
                connection,
                scenarios_root=self.scenarios,
                name="authority",
                declaration=_manifest("authority"),
            )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    @contextmanager
    def registry_connection_probe(self):
        opened = []
        original = cockpit.open_registry

        def track(*args, **kwargs):
            connection = original(*args, **kwargs)
            opened.append(connection)
            return connection

        with mock.patch.object(cockpit, "open_registry", side_effect=track):
            try:
                yield opened
            finally:
                # Release probe references even when the old caller fails the
                # close assertion, before TemporaryDirectory teardown.
                for connection in opened:
                    connection.close()

    def assert_connections_closed(self, connections):
        self.assertTrue(connections)
        for connection in connections:
            with self.assertRaisesRegex(sqlite3.ProgrammingError, "closed"):
                connection.execute("SELECT 1")

    def test_registry_call_closes_on_success_and_early_refusals(self) -> None:
        service = cockpit.CockpitService(str(self.registry))
        requests = [
            ({"action": "frontier"}, True),
            ({"action": "capability.search", "requirements": "absent capability"}, True),
            ({"action": "capability.describe", "id": "absent capability"}, False),
            ({"action": "run.open"}, False),
            ({"action": "run.status"}, False),
            ({"action": "run.finish"}, False),
            ({"action": "gap.report"}, False),
            ({"action": "scenario.create"}, False),
            ({"action": "scenario.prepare"}, False),
            ({"action": "scenario.validate", "id": "absent scenario"}, False),
            ({"action": "unknown"}, False),
        ]
        for request, expected_ok in requests:
            with self.subTest(request=request), self.registry_connection_probe() as opened:
                result = service.call(request)
                self.assertEqual(result["ok"], expected_ok, result)
                self.assert_connections_closed(opened)

    def test_registry_call_keeps_commit_and_rollback_before_closing(self) -> None:
        with closing(open_registry(str(self.registry))) as connection:
            connection.execute("CREATE TABLE fixture_transaction (marker TEXT)")
        service = cockpit.CockpitService(str(self.registry))

        def write(connection, *, query):
            connection.execute("BEGIN")
            connection.execute("INSERT INTO fixture_transaction VALUES (?)", (query,))
            return []

        with self.registry_connection_probe() as opened, \
                mock.patch.object(cockpit, "capability_contracts", side_effect=write):
            result = service.call({"action": "capability.search", "requirements": "committed"})
            self.assertTrue(result["ok"], result)
            self.assert_connections_closed(opened)

        for error in (ValueError("invalid value"), registry_store.ScenarioRegistryStoreError("invalid authority"),
                      OSError("storage unavailable"), RuntimeError("outer adapter failure")):
            def fail(connection, *, query):
                write(connection, query=query)
                raise error

            with self.subTest(error=type(error).__name__), self.registry_connection_probe() as opened, \
                    mock.patch.object(cockpit, "capability_contracts", side_effect=fail):
                request = {"action": "capability.search", "requirements": "rolled back"}
                result = service.call(request)
                self.assertFalse(result["ok"], result)
                self.assertEqual(result["error"], str(error))
                self.assert_connections_closed(opened)

        with closing(open_registry(str(self.registry))) as connection:
            self.assertEqual([row[0] for row in connection.execute("SELECT marker FROM fixture_transaction")],
                             ["committed"])

    def test_valid_unverified_selection_opens_zero_credit_run_without_token(self) -> None:
        with closing(open_registry(str(self.registry))) as connection:
            selection = self._select(connection, "first")
            before = final_gate_eligibility(connection)
        service = cockpit.CockpitService(str(self.registry), run_executable=str(self.root / "game"))
        with mock.patch.object(registry_store, "repository_root", return_value=self.root):
            opened = service.call({"action": "run.open", "selection_id": selection})
            replay = service.call({"action": "run.open", "selection_id": selection})
        self.assertTrue(opened["ok"])
        self.assertEqual(opened["result"]["state"], "active")
        self.assertEqual(opened["result"]["evidence_ceiling"], "zero-credit")
        self.assertFalse(opened["result"]["proof_promotion_authority"])
        self.assertFalse(replay["ok"])
        self.assertIn("already consumed", replay["error"])
        encoded = json.dumps(opened).lower()
        for private in ("token", "source_path", "executable", "sha256", "owner_id"):
            self.assertNotIn(private, encoded)
        with closing(open_registry(str(self.registry))) as connection:
            self.assertEqual(final_gate_eligibility(connection), before)

    def test_conflicting_owner_is_rejected_and_finish_releases_exact_scope(self) -> None:
        with closing(open_registry(str(self.registry))) as connection:
            first_selection = self._select(connection, "first")
            second_selection = self._select(connection, "second")
        service = cockpit.CockpitService(str(self.registry), run_executable=str(self.root / "game"))
        with mock.patch.object(registry_store, "repository_root", return_value=self.root):
            first = service.call({"action": "run.open", "selection_id": first_selection})
            conflict = service.call({"action": "run.open", "selection_id": second_selection})
            finished = service.call({"action": "run.finish", "run_id": first["result"]["run_id"]})
            second = service.call({"action": "run.open", "selection_id": second_selection})
        self.assertTrue(first["ok"])
        self.assertFalse(conflict["ok"])
        self.assertIn("ownership conflicts", conflict["error"])
        self.assertEqual(finished["result"]["state"], "finished")
        self.assertTrue(second["ok"])

    def test_same_saved_scenario_opens_two_run_selected_builds_without_declaration_change(self) -> None:
        source = self.scenarios / "authority.json"
        original = source.read_bytes()
        second_game = self.root / "game-v2"
        second_game.write_bytes(b"independently selected build v2")
        with closing(open_registry(str(self.registry))) as connection:
            first_selection = self._select(connection, "first")
            second_selection = self._select(connection, "second")
        first_service = cockpit.CockpitService(str(self.registry), run_executable=str(self.root / "game"))
        second_service = cockpit.CockpitService(str(self.registry), run_executable=str(second_game))
        first = first_service.call({"action": "run.open", "selection_id": first_selection})
        self.assertTrue(first["ok"], first)
        self.assertTrue(first_service.call({"action": "run.finish", "run_id": first["result"]["run_id"]})["ok"])
        second = second_service.call({"action": "run.open", "selection_id": second_selection})
        self.assertTrue(second["ok"], second)
        self.assertEqual(source.read_bytes(), original)
        with closing(open_registry(str(self.registry))) as connection:
            rows = connection.execute("SELECT executable_path FROM cockpit_run_authority ORDER BY rowid").fetchall()
        self.assertEqual([row[0] for row in rows], [str((self.root / "game").resolve()), str(second_game.resolve())])

    def test_changed_executable_invalidates_open_run_without_proof_promotion(self) -> None:
        with closing(open_registry(str(self.registry))) as connection:
            selection = self._select(connection, "drift")
        service = cockpit.CockpitService(str(self.registry), run_executable=str(self.root / "game"))
        with mock.patch.object(registry_store, "repository_root", return_value=self.root):
            opened = service.call({"action": "run.open", "selection_id": selection})
        (self.root / "game").write_bytes(b"changed executable v2")
        status = service.call({"action": "run.status", "run_id": opened["result"]["run_id"]})
        self.assertTrue(status["ok"])
        self.assertEqual(status["result"]["state"], "invalidated")
        self.assertEqual(status["result"]["evidence_ceiling"], "zero-credit")
        self.assertEqual(status["result"]["terminal"]["reason"], "executable_binding_drift")
        self.assertFalse(status["result"]["proof_promotion_authority"])


if __name__ == "__main__":
    unittest.main()
