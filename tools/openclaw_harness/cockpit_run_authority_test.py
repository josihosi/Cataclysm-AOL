#!/usr/bin/env python3
"""Run authority stays usable when evidence-token eligibility is absent."""

from __future__ import annotations

import json
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
                "executable": "game",
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
        with open_registry(str(self.registry)) as connection:
            create_source_bound_scenario(
                connection,
                scenarios_root=self.scenarios,
                name="authority",
                declaration=_manifest("authority"),
            )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_unverified_selection_opens_zero_credit_run_without_token(self) -> None:
        with open_registry(str(self.registry)) as connection:
            selection = self._select(connection, "first")
            before = final_gate_eligibility(connection)
        service = cockpit.CockpitService(str(self.registry))
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
        with open_registry(str(self.registry)) as connection:
            self.assertEqual(final_gate_eligibility(connection), before)

    def test_conflicting_owner_is_rejected_and_finish_releases_exact_scope(self) -> None:
        with open_registry(str(self.registry)) as connection:
            first_selection = self._select(connection, "first")
            second_selection = self._select(connection, "second")
        service = cockpit.CockpitService(str(self.registry))
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

    def test_changed_executable_invalidates_open_run_without_proof_promotion(self) -> None:
        with open_registry(str(self.registry)) as connection:
            selection = self._select(connection, "drift")
        service = cockpit.CockpitService(str(self.registry))
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
