"""Real registry selection with lossless, read-only browsing of its saved result."""
from __future__ import annotations

import argparse
import io
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from contextlib import redirect_stdout, redirect_stderr
from unittest import mock

import scenario_registry_cli as cli
import scenario_registry_cli_test as fixtures


class RegistryQueryOutputTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.registry = self.root / "registry.sqlite3"
        self.scenarios = self.root / "scenarios"
        self.scenarios.mkdir()
        helper = fixtures.ScenarioRegistryCliTest()
        # The first page holds five; two further matches exercise the next-page boundary.
        for number in range(8):
            declaration = helper.strict_manifest()
            declaration["name"] = "query-fixture-" + str(number)
            declaration["capabilities"]["player.injured"] = number == 7
            (self.scenarios / (str(number) + ".json")).write_text(json.dumps(declaration))
        self.call("rebuild", "--scenarios-root", str(self.scenarios))

    def call(self, *argv, success=True):
        out = io.StringIO(); error = io.StringIO()
        with redirect_stdout(out), redirect_stderr(error), mock.patch.object(
                cli, "_current_source_executable_readiness", return_value={"status": "ready"}):
            status = cli.main(["--registry", str(self.registry), *argv])
        self.assertEqual(status == 0, success, error.getvalue())
        return json.loads(out.getvalue() if success else error.getvalue())

    def query(self):
        return self.call("registry-query", "--query-json", json.dumps({
            "requirements": [{"key": "player.injured", "op": "eq", "value": False,
                              "minimum_evidence": "declared"}], "preferences": []}))

    def test_pages_keep_order_snapshot_and_authority_without_requery(self):
        first = self.query()["result"]
        self.assertEqual(len(first["candidates"]), 5)
        self.assertEqual(first["page"]["total_matches"], 7)
        self.assertEqual(first["page"]["excluded_candidates"], 1)
        self.assertNotIn("evaluation", first)
        self.assertIsNotNone(first["next_action"])
        digest = first["artifact"]["sha256"]
        full = self.call("registry-query-artifact", "--sha256", digest)
        expected = full["result"]["evaluation"]["evaluation"]["ranked_scenario_ids"]
        # Mutate the catalogue after the query. Browsing must retain the original snapshot.
        (self.scenarios / "6.json").unlink()
        self.call("rebuild", "--scenarios-root", str(self.scenarios))
        before = self.registry.read_bytes()
        with mock.patch.object(cli, "open_registry", side_effect=AssertionError("page opened registry")):
            second = self.call(*first["page"]["next"][4:])["result"]
            last = self.call("registry-query-page", "--sha256", digest, "--offset", "7")["result"]
        self.assertEqual(self.registry.read_bytes(), before)
        actual = [item["scenario_id"] for item in first["candidates"] + second["candidates"]]
        self.assertEqual(actual, expected)
        self.assertEqual(len(second["candidates"]), 2)
        self.assertIsNone(second["page"]["next"])
        self.assertEqual(last["candidates"], [])
        self.assertEqual(second["token_id"], first["token_id"])
        self.assertEqual(second["artifact"], first["artifact"])
        all_matches = self.call("registry-query-page", "--sha256", digest, "--page-size", "10")["result"]
        self.assertEqual(len(all_matches["candidates"]), 7)

    def test_browsing_preserves_an_issued_token_and_its_history(self):
        root = self.root / "issued"
        root.mkdir()
        helper = fixtures.ScenarioRegistryCliTest()
        self.registry, _, issued_token = helper.issue_selection_token(root)
        result = self.query()["result"]
        self.assertEqual(result["token_id"], issued_token)
        before = helper.token_events(self.registry, issued_token)
        page = self.call("registry-query-page", "--sha256", result["artifact"]["sha256"],
                         "--offset", "1")["result"]
        self.assertEqual(page["token_id"], issued_token)
        self.assertEqual(helper.token_events(self.registry, issued_token), before)

    def test_full_recovery_is_exact_and_tampering_is_rejected(self):
        result = self.query()["result"]
        artifact = Path(result["artifact"]["path"])
        recovered = self.call(*result["full_result"][4:])
        self.assertEqual(recovered, json.loads(artifact.read_text()))
        artifact.write_bytes(artifact.read_bytes() + b" ")
        failure = self.call("registry-query-page", "--sha256", result["artifact"]["sha256"], success=False)
        self.assertIn("digest drift", failure["error"])

    def test_no_match_and_invalid_offset_remain_explicit(self):
        result = self.call("registry-query", "--query-json", json.dumps({
            "requirements": [{"key": "player.injured", "op": "eq", "value": "unmatchable",
                              "minimum_evidence": "declared"}], "preferences": []}))["result"]
        self.assertEqual(result["candidates"], [])
        self.assertIsNone(result["page"]["next"])
        self.assertIsNotNone(result["next_action"])
        failure = self.call("registry-query-page", "--sha256", result["artifact"]["sha256"],
                            "--offset", "-1", success=False)
        self.assertIn("nonnegative", failure["error"])

    def test_selected_live_route_supplies_charter_and_absent_session_path(self):
        declaration = self.scenarios / "live.json"
        declaration.write_text(json.dumps({"steps": [{"kind": "cockpit_live_session"}]}))
        result = {"token_id": "issued-token", "next_action": None,
                  "source_executable_readiness": {"status": "ready"},
                  "evaluation": {"evaluation": {"ranked_scenario_ids": ["selected"]},
                                 "candidates": [{"scenario_id": "selected", "explanation": {
                                     "manifest": {"source_path": str(declaration)}}}]}}
        charter = self.root / "charter.json"
        action = cli._query_launch_action(result, argparse.Namespace(witness_charter=str(charter)), self.registry)
        args = cli.build_parser().parse_args(action["command"]["argv"][2:])
        self.assertEqual(args.command, "registry-detached-launch")
        self.assertEqual(args.selection_token, "issued-token")
        self.assertEqual(args.witness_charter, str(charter.resolve()))
        self.assertFalse(Path(args.session_dir).exists())
        missing = cli._query_launch_action(result, argparse.Namespace(witness_charter=None), self.registry)
        self.assertEqual(missing["kind"], "provide_witness_charter")


if __name__ == "__main__":
    unittest.main()
