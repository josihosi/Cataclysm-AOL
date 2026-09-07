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
from registry_query_output import _run_observation


class RegistryQueryOutputTest(unittest.TestCase):
    def test_run_bound_compact_projection_keeps_missing_trade_and_declarations_explicit(self):
        report = {
            "steps": [{"action_id": "shakedown.pay", "accepted": True,
                       "run_id": "native-run", "game_minutes": 8380,
                       "game_turn": 5254801}],
            "artifacts": {"matches_by_pattern": [{"lines": [
                "dialogue_return response=pay",
                "bandit_live_world shakedown_fight_advance npc=18 attacked=yes",
                "bandit_live_world shakedown_fight_advance npc=19 attacked=yes",
            ]}]},
            "proof_classification": {"verdict": "blocked_terminal_save_step_not_completed"},
            "scenario_manifest": {"normalized": {"capabilities": {"state": "declared",
                "value": {"local_place.shakedown.reopened_options": ["Pay", "Fight"]}}}},
            "runtime_binding": {"source_sha256": "source-bound"},
        }
        result = _run_observation(report, run_id="20260907_103541_164574e2295845f586c0eeb37ee0bba4",
                                   receipt_id="c0698f3f5a843fb54695bb3bbcb39c18f4ac152854723c494f151746b2341967")
        self.assertTrue(result["accepted_pay"]["accepted"])
        self.assertEqual(result["trade_owner"]["status"], "missing")
        self.assertTrue(result["forced_fight"])
        self.assertEqual(result["actors"], [18, 19])
        self.assertIn("trade_owner", result["missing_fields"])
        self.assertEqual(result["manifest_declarations"]["capabilities"]["state"], "declared")
        self.assertEqual(result["run_observations"]["actor_ids_source"], "run artifact lines")

    def test_query_preserves_build_and_binding_metadata(self):
        readiness = {"status": "ready", "build_entrypoint": {"argv": ["python", "verified-builder.py"]},
                     "executable_sha256": "a" * 64, "product_source_sha256": "b" * 64}
        out = io.StringIO()
        with redirect_stdout(out), mock.patch.object(cli, "_current_source_executable_readiness", return_value=readiness):
            self.assertEqual(cli.main(["--registry", str(self.registry), "registry-query", "--query-json", json.dumps({"requirements": [], "preferences": []})]), 0)
        projected = json.loads(out.getvalue())["result"]["source_executable_readiness"]
        self.assertEqual(projected, readiness)

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
        full_receipt = self.call("registry-query-artifact", "--sha256", digest)
        full = json.loads(Path(full_receipt["result"]["artifact"]["path"]).read_text())
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
        recovered = self.call(*result["full_result"][4:], "--output", str(self.root / "export.json"))
        self.assertEqual(Path(recovered["result"]["export"]["path"]).read_bytes(), artifact.read_bytes())
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

    def test_excluded_candidates_explain_rejection_and_retrieve_exact_evidence(self):
        result = self.query()["result"]
        self.assertEqual(result["rejections"]["causes"][0]["candidate_count"], 1)
        before = self.registry.read_bytes()
        excluded = self.call(*result["rejections"]["details_argv"][4:])["result"]
        self.assertEqual(len(excluded["candidates"]), 1)
        item = excluded["candidates"][0]
        self.assertIsNone(item["rank"])
        self.assertFalse(item["matches"][0]["passed"])
        detail = self.call(*item["details_argv"][4:])["result"]["candidates"][0]
        full = json.loads(Path(result["artifact"]["path"]).read_text())
        expected = next(x for x in full["result"]["evaluation"]["candidates"]
                        if x["scenario_id"] == item["scenario_id"])
        self.assertEqual(detail["evidence"], expected)
        self.assertEqual(self.registry.read_bytes(), before)
        missing = self.call("registry-query-page", "--sha256", result["artifact"]["sha256"],
                            "--scenario-id", "absent", success=False)
        self.assertIn("absent from this saved query", missing["error"])

    def test_full_query_exports_without_printing_evaluation(self):
        result = self.call("registry-query", "--query-json", json.dumps({
            "requirements": [], "preferences": []}), "--full")["result"]
        self.assertNotIn("evaluation", result)
        self.assertEqual(result["export"], result["artifact"])
        self.assertIn("evaluation", json.loads(Path(result["export"]["path"]).read_text())["result"])

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
