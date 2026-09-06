#!/usr/bin/env python3
"""Subprocess contracts for the thin scenario-registry maintenance CLI."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
CLI_PATH = HARNESS_DIR / "scenario_registry_cli.py"
sys.path.insert(0, str(HARNESS_DIR))

import scenario_registry_cli  # noqa: E402
import startup_harness  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    BindingAdapters,
    RegistryRepairToken,
    ScenarioRegistryStoreError,
    claim_bootstrap_token_for_launch,
    execute_registry_query,
    issue_registry_bootstrap_token,
    issue_registry_repair_token,
    ingest_report_reference,
    immediate_transaction,
    open_registry,
    parse_registry_query_request,
    revalidate_current_bootstrap_authority,
    rebuild_manifest_projection,
    reload_bootstrap_token_for_launch,
    reload_selection_token_for_launch,
    reconcile_report_bindings,
)
from startup_harness import runtime_source_binding  # noqa: E402
from startup_harness import finalize_probe_report  # noqa: E402


class ScenarioRegistryCliTest(unittest.TestCase):
    def test_r008_pre_descriptor_prefix_uses_declared_objectives_without_step_duplicates(self) -> None:
        scenario = "bandit.r008_natural_return_validation_mcw"
        declaration = startup_harness.load_scenario(scenario)
        selection = scenario_registry_cli.RegistryBootstrapToken(
            "token", True, "current", scenario,
            str(startup_harness.scenario_path(scenario)), {},
        )

        prefix = scenario_registry_cli._declared_pre_descriptor_prefix(selection)

        self.assertEqual(len(prefix), 4)
        self.assertEqual(prefix[0]["label"], "advance_first_semantic_scheduler_window")
        self.assertIn("Zero-credit preparation", prefix[0]["objective"])
        self.assertEqual(prefix[0]["required_action_chain"], [
            "world.wait", "wait.duration_menu", "wait.6h",
        ])
        steps = startup_harness.normalize_scenario_steps(declaration["steps"], 0, 1.0)
        startup_harness.bind_pre_descriptor_bootstrap_objectives(steps, declaration)
        first_window = next(
            step for step in steps
            if step["label"] == "advance_first_semantic_scheduler_window"
        )
        self.assertEqual(first_window["objective"], prefix[0]["objective"])

    def test_query_readiness_routes_stale_product_build_before_repair_authority(self) -> None:
        repair = {
            "kind": "repair_current_contradiction",
            "command": {"cli": ["registry-repair-bootstrap", "--query-id", "query-a"]},
        }
        stale = {
            "status": "build_required",
            "reason": "product_binary_source_is_stale_or_unproved",
            "next_action": "build or select a source-matching executable, then repeat the same registry query",
            "evidence_ceiling": "none until source-matching executable revalidation",
        }
        routed = scenario_registry_cli._apply_source_readiness_to_query(
            {"token_id": None, "next_action": repair}, stale,
        )

        self.assertEqual(routed["next_action"]["kind"], "build_or_select_source_matching_executable")
        self.assertEqual(routed["next_action"]["after_readiness"], repair)
        self.assertNotIn("registry-repair-bootstrap", routed["next_action"]["action"])

        provisional = scenario_registry_cli._apply_source_readiness_to_query(
            {"token_id": None, "next_action": repair},
            {
                **stale,
                "status": "provisional_diagnosis_allowed",
                "evidence_ceiling": "provisional harness diagnosis only",
            },
        )
        self.assertEqual(provisional["next_action"]["kind"], "isolated_harness_diagnosis")
        self.assertEqual(
            provisional["next_action"]["evidence_ceiling"],
            "provisional harness diagnosis only",
        )

    def test_repair_bootstrap_stops_before_token_issue_when_executable_is_stale(self) -> None:
        connection = mock.MagicMock()
        stale = {
            "status": "build_required",
            "reason": "product_binary_source_is_stale_or_unproved",
            "next_action": "build or select a source-matching executable, then repeat the same registry query",
        }
        with tempfile.TemporaryDirectory() as temp_dir, \
                mock.patch.object(scenario_registry_cli, "open_registry", return_value=connection), \
                mock.patch.object(
                    scenario_registry_cli, "_current_source_executable_readiness", return_value=stale,
                ), \
                mock.patch.object(scenario_registry_cli, "issue_registry_repair_token") as issue, \
                mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
            exit_code = scenario_registry_cli.main([
                "--registry", str(Path(temp_dir) / "registry.sqlite3"),
                "registry-repair-bootstrap", "--query-id", "query-a",
            ])

        self.assertEqual(exit_code, 0)
        issue.assert_not_called()
        result = write_result.call_args.args[0]["result"]
        self.assertFalse(result["accepted"])
        self.assertEqual(result["reason"], "source_matching_executable_required")
        connection.close.assert_called_once()

    def test_brief_requested_playtest_uses_validated_charter_and_bound_token_without_human_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            args = argparse.Namespace(
                command="registry-detached-launch",
                selection_token="selected-token",
                session_dir=str(root / "session"),
                witness_charter=str(HARNESS_DIR / "charters" / "r009-macos-witness-rev2.json"),
            )
            selection = scenario_registry_cli.RegistryLaunchToken(
                "selected-token", True, "current", "r009-m095", "scenario.json",
            )
            bridge_result = subprocess.CompletedProcess(
                args=[], returncode=0,
                stdout=json.dumps({"ok": True, "session_id": "session-a"}), stderr="",
            )
            connection = mock.MagicMock()
            with mock.patch.object(scenario_registry_cli, "open_registry", return_value=connection), \
                    mock.patch.object(scenario_registry_cli, "reload_selection_token_for_launch",
                                      return_value=selection), \
                    mock.patch.object(scenario_registry_cli, "_current_source_executable_readiness",
                                      return_value={"status": "ready"}), \
                    mock.patch.object(scenario_registry_cli, "_scenario_requires_bound_live_bridge",
                                      return_value=True), \
                    mock.patch.object(scenario_registry_cli, "_declared_pre_descriptor_prefix",
                                      return_value=[]), \
                    mock.patch.object(scenario_registry_cli, "_declared_live_session_reentries",
                                      return_value=0), \
                    mock.patch.object(scenario_registry_cli.subprocess, "run",
                                      return_value=bridge_result) as run, \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                result = scenario_registry_cli._launch_selection_file_bridge(
                    args, root / "registry.sqlite3",
                )

            self.assertEqual(result, 0)
            command = run.call_args.args[0]
            self.assertIn("registry-launch", command)
            self.assertIn("selected-token", command)
            self.assertNotIn("Josef", command)
            registry_index = command.index("--registry")
            charter_index = command.index("--witness-charter")
            self.assertEqual(command[registry_index + 1], str((root / "registry.sqlite3").resolve()))
            self.assertEqual(command[charter_index + 1], str(Path(args.witness_charter).resolve()))
            receipt = write_result.call_args.args[0]
            self.assertTrue(receipt["ok"])
            self.assertEqual(
                receipt["authority"],
                "technical run token remains unclaimed until the canonical child launch",
            )
            launched_environment = run.call_args.kwargs["env"]
            self.assertIn("OPENCLAW_PLAYTEST_WITNESS_CHARTER", launched_environment)
            connection.close.assert_called_once()

    def test_detached_launch_refuses_stale_product_binary_without_starting_bridge(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            args = argparse.Namespace(
                command="registry-detached-launch",
                selection_token="selected-token",
                session_dir=str(root / "session"),
                witness_charter=str(HARNESS_DIR / "charters" / "r009-macos-witness-rev2.json"),
            )
            selection = scenario_registry_cli.RegistryLaunchToken(
                "selected-token", True, "current", "r009-m095", "scenario.json",
            )
            connection = mock.MagicMock()
            stale = {"status": "build_required", "reason": "product_binary_source_is_stale_or_unproved"}
            with mock.patch.object(scenario_registry_cli, "open_registry", return_value=connection), \
                    mock.patch.object(scenario_registry_cli, "reload_selection_token_for_launch",
                                      return_value=selection), \
                    mock.patch.object(scenario_registry_cli, "_current_source_executable_readiness",
                                      return_value=stale), \
                    mock.patch.object(scenario_registry_cli.subprocess, "run") as run, \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                result = scenario_registry_cli._launch_selection_file_bridge(args, root / "registry.sqlite3")

            self.assertEqual(result, 1)
            run.assert_not_called()
            receipt = write_result.call_args.args[0]
            self.assertEqual(receipt["result"]["reason"], "source_matching_executable_required")
            self.assertEqual(receipt["source_executable_readiness"], stale)

    def test_detached_live_namespaces_force_bounded_terminal_stdout(self) -> None:
        scenario = "bandit.r008_natural_safe_watch_validation_mcw"
        source_path = str(startup_harness.scenario_path(scenario))
        selection = scenario_registry_cli.RegistryBootstrapToken(
            "token", True, "accepted", scenario, source_path, {},
        )
        repair = RegistryRepairToken("token", True, "accepted", scenario, source_path, {})
        bootstrap = scenario_registry_cli._registry_bootstrap_probe_namespace(
            selection, cockpit_live_session=True,
        )
        repaired = scenario_registry_cli._registry_repair_probe_namespace(
            repair, cockpit_live_session=True,
        )
        self.assertTrue(bootstrap.compact_stdout)
        self.assertTrue(repaired.compact_stdout)

    def strict_manifest(self) -> dict:
        return {
            "manifest_version": 1,
            "name": "cli.registry.fixture",
            "steps": [
                {"label": "setup", "kind": "wait"},
                {"label": "production", "kind": "press"},
                {"label": "terminal", "kind": "audit_log_contains"},
                {"label": "artifact", "kind": "audit_log_contains"},
                {"label": "shortcut", "kind": "audit_log_not_contains"},
            ],
            "capabilities": {"player.injured": False},
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

    def write_json(self, path: Path, value: object) -> None:
        path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

    def run_cli(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CLI_PATH), *arguments],
            check=False,
            capture_output=True,
            text=True,
        )

    def report(self, manifest_path: Path, executable_path: Path) -> dict:
        source = runtime_source_binding()
        self.assertTrue(source.get("ok"), source.get("error"))
        return {
            "mode": "probe",
            "scenario": "cli.registry.fixture",
            "contract": {
                "profile": "",
                "config_profile": "",
                "fixture": "",
                "fixture_profile": "",
                "profile_snapshot": "",
                "profile_snapshot_profile": "",
            },
            "scenario_manifest": {
                "source": {
                    "path": str(manifest_path.resolve()),
                    "sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                }
            },
            "startup": {
                "screen": {
                    "runtime_binding_status": "compatible",
                    "runtime_binding_observed": {
                        "executable_path": str(executable_path.resolve()),
                        "executable_sha256": hashlib.sha256(executable_path.read_bytes()).hexdigest(),
                        "runtime_source_sha256": source["sha256"],
                    },
                },
                "fixture_install": {},
                "profile_snapshot": {},
            },
            "proof_classification": {
                "status": "green",
                "verdict": "probe_completed",
                "evidence_class": "feature-path",
                "feature_proof": True,
            },
            "verdict": "green_route_completed",
            "evidence_class": "feature-path",
            "feature_proof": True,
        }

    def issue_selection_token(self, root: Path) -> tuple[Path, Path, str]:
        scenarios = root / "scenarios"
        scenarios.mkdir()
        manifest_path = scenarios / "cli.json"
        self.write_json(manifest_path, self.strict_manifest())
        executable_path = root / "cataclysm-tiles"
        executable_path.write_bytes(b"registry launch runtime fixture")
        report_path = root / "probe.report.json"
        self.write_json(report_path, self.report(manifest_path, executable_path))
        registry_path = root / "registry.sqlite3"
        connection = open_registry(str(registry_path))
        try:
            rebuild_manifest_projection(connection, scenarios)
            adapters = BindingAdapters(
                runtime=lambda _expected: {"status": "compatible", "facts": {}},
                fixture=lambda _expected: {"status": "compatible", "facts": {
                    "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                }},
                profile=lambda _expected: {"status": "compatible", "facts": {
                    "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                }},
            )
            ingested = ingest_report_reference(connection, report_path, adapters=adapters)
            self.assertEqual(ingested["eligibility"], "hard_proven")
            issued = execute_registry_query(
                connection,
                parse_registry_query_request({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertIsNotNone(issued.token_id)
            return registry_path, scenarios, str(issued.token_id)
        finally:
            connection.close()

    def token_events(self, registry_path: Path, token_id: str) -> list[tuple[str, str]]:
        connection = sqlite3.connect(registry_path)
        try:
            return [
                tuple(row)
                for row in connection.execute(
                    "SELECT event_kind, reason FROM token_history WHERE token_id = ? ORDER BY token_event_id",
                    (token_id,),
                ).fetchall()
            ]
        finally:
            connection.close()

    def run_registry_launch(
        self,
        registry_path: Path,
        scenarios: Path,
        token_id: str,
    ) -> tuple[int, mock.Mock]:
        executable = registry_path.parent / "selected-runtime"
        executable.write_bytes(b"selected runtime")
        runtime_binding = {
            "ok": True,
            "schema": 1,
            "executable_path": str(executable.resolve()),
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "runtime_source_sha256": "test-runtime-source",
        }
        with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                mock.patch.object(startup_harness, "detect_executable", return_value=executable), \
                mock.patch.object(startup_harness, "build_runtime_binding", return_value=runtime_binding), \
                mock.patch.object(scenario_registry_cli, "_current_source_executable_readiness",
                                  return_value={"status": "ready"}), \
                mock.patch.object(startup_harness, "run_probe_mode", return_value=23) as run_probe, \
                redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            result = scenario_registry_cli.main([
                "--registry", str(registry_path), "registry-launch", token_id,
            ])
        return result, run_probe

    def test_unreported_native_launch_exception_revokes_selection_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "selected-runtime"
            executable.write_bytes(b"selected runtime")
            runtime_binding = {"ok": True, "schema": 1,
                               "executable_path": str(executable.resolve()),
                               "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                               "runtime_source_sha256": "test-runtime-source"}
            errors = io.StringIO()
            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "detect_executable", return_value=executable), \
                    mock.patch.object(startup_harness, "build_runtime_binding", return_value=runtime_binding), \
                    mock.patch.object(scenario_registry_cli, "_current_source_executable_readiness",
                                      return_value={"status": "ready"}), \
                    mock.patch.object(startup_harness, "run_probe_mode",
                                      side_effect=FileNotFoundError("peekaboo missing after game launch")) as probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(errors):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-launch", token_id,
                ])
            self.assertEqual(result, 1)
            probe.assert_called_once()
            self.assertIn("FileNotFoundError", errors.getvalue())
            self.assertIn("peekaboo missing after game launch", errors.getvalue())
            self.assertIn('"selection_invalidated": true', errors.getvalue())
            self.assertIn('"cleanup": "unconfirmed', errors.getvalue())
            self.assertIn(("invalidated", "registry_launch_adapter_failed_before_report"),
                          self.token_events(registry_path, token_id))
            with open_registry(str(registry_path)) as connection:
                reused = reload_selection_token_for_launch(connection, token_id)
            self.assertFalse(reused.accepted)
            self.assertEqual(reused.reason, "token_invalidated")

    def bootstrap_runtime(self, executable: Path) -> dict:
        source = runtime_source_binding()
        self.assertTrue(source.get("ok"), source.get("error"))
        return {
            "ok": True,
            "schema": 1,
            "executable_path": str(executable.resolve()),
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "runtime_source_sha256": source["sha256"],
        }

    def bootstrap_request(self) -> dict:
        return {
            "requirements": [{
                "key": "player.injured",
                "op": "eq",
                "value": False,
                "minimum_evidence": "declared",
            }],
            "preferences": [],
        }

    def test_bootstrap_authorizes_only_first_query_bound_run_and_is_not_a_selection_token(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "bootstrap.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            connection = open_registry(str(root / "registry.sqlite3"))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
                self.assertTrue(bootstrap.accepted)
                self.assertEqual(bootstrap.scenario, "bootstrap")
                self.assertFalse(reload_selection_token_for_launch(connection, bootstrap.token_id).accepted)
                claimed = claim_bootstrap_token_for_launch(connection, bootstrap.token_id)
                self.assertTrue(claimed.accepted)
                self.assertEqual(claimed.reason, "claimed")
                self.assertFalse(claim_bootstrap_token_for_launch(connection, bootstrap.token_id).accepted)
                self.assertEqual(
                    reload_bootstrap_token_for_launch(connection, bootstrap.token_id, require_claimed=True).reason,
                    "token_invalidated",
                )
            finally:
                connection.close()

    def test_bootstrap_uses_declared_facts_only_for_a_current_stale_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "bootstrap.json"
            manifest = self.strict_manifest()
            self.write_json(manifest_path, manifest)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            report_path = root / "probe.report.json"
            self.write_json(report_path, self.report(manifest_path, executable))
            connection = open_registry(str(root / "registry.sqlite3"))
            try:
                request = parse_registry_query_request(self.bootstrap_request())
                adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                rebuild_manifest_projection(connection, scenarios)
                ingested = ingest_report_reference(connection, report_path, adapters=adapters)
                self.assertEqual(ingested["eligibility"], "hard_proven")
                already_live = issue_registry_bootstrap_token(
                    connection, request, runtime_binding=self.bootstrap_runtime(executable)
                )
                self.assertFalse(already_live.accepted)
                self.assertEqual(already_live.reason, "compatible_run_evidence_already_exists")

                manifest["description"] = "current changed manifest with stale prior verification"
                self.write_json(manifest_path, manifest)
                rebuild_manifest_projection(connection, scenarios)
                ordinary = execute_registry_query(connection, request, drafts_root=root / "drafts")
                self.assertIsNone(ordinary.token_id)
                current_facts = {
                    "runtime": self.bootstrap_runtime(executable),
                    "fixture": {
                        "status": "compatible",
                        "name": "",
                        "profile": "",
                        "source_path": str(root),
                        "source_sha256": hashlib.sha256(b"current fixture").hexdigest(),
                    },
                    "profile": {
                        "status": "compatible",
                        "name": "",
                        "profile": "",
                        "source_path": str(root),
                        "source_sha256": hashlib.sha256(b"current profile").hexdigest(),
                    },
                }
                released = revalidate_current_bootstrap_authority(
                    connection, request, current_facts=lambda _manifest: current_facts,
                )
                self.assertTrue(released["accepted"])
                reconcile_report_bindings(connection, adapters=adapters)
                canonical = execute_registry_query(connection, request, drafts_root=root / "drafts")
                self.assertIsNotNone(canonical.token_id)
                selected = next(
                    candidate for candidate in canonical.evaluation.candidates
                    if candidate.scenario_id == released["manifest_id"]
                )
                self.assertEqual(selected.facts["player.injured"]["evidence_state"], "declared")
                reloaded = reload_selection_token_for_launch(connection, str(canonical.token_id))
                self.assertTrue(reloaded.accepted)
                self.assertEqual(reloaded.reason, "current_bootstrap_authority")

                manifest["description"] = "a second current manifest with stale prior verification"
                self.write_json(manifest_path, manifest)
                rebuild_manifest_projection(connection, scenarios)
                released_again = revalidate_current_bootstrap_authority(
                    connection, request, current_facts=lambda _manifest: current_facts,
                )
                self.assertTrue(released_again["accepted"])
                reconcile_report_bindings(connection, adapters=adapters)
                canonical_again = execute_registry_query(connection, request, drafts_root=root / "drafts")
                self.assertIsNotNone(canonical_again.token_id)

                bootstrap = issue_registry_bootstrap_token(
                    connection, request, runtime_binding=self.bootstrap_runtime(executable)
                )
                self.assertTrue(bootstrap.accepted)
                self.assertTrue(reload_bootstrap_token_for_launch(connection, bootstrap.token_id).accepted)

                invalid = dict(manifest)
                invalid.pop("runtime_contract")
                self.write_json(manifest_path, invalid)
                with self.assertRaises(ScenarioRegistryStoreError):
                    rebuild_manifest_projection(connection, scenarios)

                manifest_path.unlink()
                rebuild_manifest_projection(connection, scenarios)
                absent_bootstrap = issue_registry_bootstrap_token(
                    connection, request, runtime_binding=self.bootstrap_runtime(executable)
                )
                self.assertFalse(absent_bootstrap.accepted)
            finally:
                connection.close()

    def test_bootstrap_fails_closed_when_manifest_or_runtime_binding_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "bootstrap.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            connection = open_registry(str(root / "registry.sqlite3"))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
                changed = self.strict_manifest()
                changed["description"] = "changed after bootstrap issue"
                self.write_json(manifest_path, changed)
                rejected = reload_bootstrap_token_for_launch(connection, bootstrap.token_id)
                self.assertFalse(rejected.accepted)
                self.assertEqual(rejected.reason, "manifest_source_changed")
            finally:
                connection.close()

    def test_bootstrap_cli_claims_the_separate_authority_for_the_canonical_probe_only_once(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            self.write_json(scenarios / "bootstrap.json", self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
            finally:
                connection.close()

            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "detect_executable", return_value=executable), \
                    mock.patch.object(startup_harness, "run_probe_mode", return_value=29) as run_probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-bootstrap-launch", bootstrap.token_id,
                    "--adaptive-semantic-autodrive",
                ])

            self.assertEqual(result, 29)
            run_probe.assert_called_once()
            self.assertTrue(run_probe.call_args.args[0].adaptive_semantic_autodrive)
            receipt = json.loads(run_probe.call_args.args[0].registry_launch_receipt)
            self.assertEqual(receipt["authority_kind"], "registry_bootstrap_first_compatible_run")
            self.assertEqual(receipt["token_id"], bootstrap.token_id)
            with mock.patch.object(startup_harness, "run_probe_mode") as reused_probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-bootstrap-launch", bootstrap.token_id,
                ]), 1)
            reused_probe.assert_not_called()

    def test_bootstrap_cli_rejects_a_changed_runtime_before_probe_claim(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            self.write_json(scenarios / "bootstrap.json", self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
            finally:
                connection.close()
            executable.write_bytes(b"changed bootstrap runtime")

            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "run_probe_mode") as run_probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-bootstrap-launch", bootstrap.token_id,
                ])

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertIn(
                ("bootstrap_invalidated", "runtime_binding_changed"),
                self.token_events(registry_path, bootstrap.token_id),
            )

    def test_bootstrap_live_session_without_bound_bridge_keeps_authority_unclaimed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            self.write_json(scenarios / "bootstrap.json", self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
            finally:
                connection.close()

            with mock.patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": ""}), \
                    mock.patch.object(startup_harness, "run_probe_mode") as run_probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-bootstrap-launch", bootstrap.token_id,
                ])

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, bootstrap.token_id), [
                ("bootstrap_issued", "first_compatible_evidence_run"),
            ])

    def test_bootstrap_detached_launch_uses_file_bridge_without_claiming_authority(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            self.write_json(scenarios / "bootstrap.json", self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"bootstrap runtime")
            registry_path = root / "registry.sqlite3"
            session_dir = root / "bridge-session"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                bootstrap = issue_registry_bootstrap_token(
                    connection,
                    parse_registry_query_request(self.bootstrap_request()),
                    runtime_binding=self.bootstrap_runtime(executable),
                )
            finally:
                connection.close()
            bridge_result = subprocess.CompletedProcess(
                [], 0,
                stdout=json.dumps({"ok": True, "bridge_pid": 123, "session_dir": str(session_dir)}),
                stderr="",
            )
            with mock.patch.object(startup_harness, "compare_runtime_binding", return_value={"status": "matched"}), \
                    mock.patch.object(scenario_registry_cli.subprocess, "run", return_value=bridge_result) as run, \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                exit_code = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-bootstrap-detached-launch",
                    bootstrap.token_id, "--session-dir", str(session_dir),
                ])

            self.assertEqual(exit_code, 0)
            command = run.call_args.args[0]
            self.assertIn("cockpit_file_bridge.py", command[1])
            self.assertIn("start", command)
            self.assertIn("registry-bootstrap-launch", command)
            self.assertIn(bootstrap.token_id, command)
            self.assertIn("--cockpit-live-session", command)
            self.assertIn("--adaptive-semantic-autodrive", command)
            self.assertIn("--session-reentries", command)
            result = write_result.call_args.args[0]
            self.assertTrue(result["ok"])
            self.assertEqual(result["bootstrap_token"], bootstrap.token_id)
            self.assertEqual(result["bridge"]["bridge_pid"], 123)
            self.assertEqual(self.token_events(registry_path, bootstrap.token_id), [
                ("bootstrap_issued", "first_compatible_evidence_run"),
            ])

    def test_query_routes_current_contradiction_to_query_bound_repair(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "repair.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"repair runtime")
            report = self.report(manifest_path, executable)
            report["proof_classification"].update({
                "status": "red",
                "verdict": "repair contradiction",
                "evidence_class": "startup/load",
                "feature_proof": False,
            })
            report.update({
                "verdict": "red route",
                "evidence_class": "startup/load",
                "feature_proof": False,
            })
            report_path = root / "red.probe.report.json"
            self.write_json(report_path, report)
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                red = ingest_report_reference(connection, report_path, adapters=adapters)
                route_key = str(connection.execute(
                    "SELECT route_key FROM verification_history WHERE verification_id = ?",
                    (red["verification_id"],),
                ).fetchone()[0])
                manifest_id = str(connection.execute(
                    "SELECT manifest_id FROM verification_history WHERE verification_id = ?",
                    (red["verification_id"],),
                ).fetchone()[0])
            finally:
                connection.close()

            query = self.bootstrap_request()
            with mock.patch.object(
                    scenario_registry_cli, "_current_source_executable_readiness",
                    return_value={"status": "ready"}), \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                query_exit = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-query",
                    "--query-json", json.dumps(query),
                ])
            self.assertEqual(query_exit, 0)
            query_result = write_result.call_args.args[0]["result"]
            self.assertIsNone(query_result["token_id"])
            self.assertTrue(Path(query_result["draft_path"]).is_file())
            action = query_result["next_action"]
            self.assertEqual(action["reason"], "closest_query_candidate_has_current_unresolved_contradiction")
            self.assertEqual(action["required_identifiers"], {
                "query_id": query_result["query_id"],
                "manifest_id": manifest_id,
                "route_key": route_key,
                "red_verification_id": red["verification_id"],
            })
            self.assertEqual(action["command"]["cli"], [
                "registry-repair-bootstrap", "--query-id", query_result["query_id"],
            ])

            binding = {
                "runtime": self.bootstrap_runtime(executable),
                "fixture": {
                    "status": "compatible", "name": "", "profile": "",
                    "source_path": str(root),
                    "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                },
                "profile": {
                    "status": "compatible", "name": "", "profile": "",
                    "source_path": str(root),
                    "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                },
            }
            with mock.patch.object(scenario_registry_cli, "_current_repair_binding", return_value=binding), \
                    mock.patch.object(
                        scenario_registry_cli, "_current_source_executable_readiness",
                        return_value={"status": "ready"},
                    ), \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                exit_code = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-repair-bootstrap",
                    "--query-id", query_result["query_id"],
                ])
            self.assertEqual(exit_code, 0)
            repair = write_result.call_args.args[0]["result"]
            self.assertTrue(repair["accepted"], repair["reason"])

            connection = open_registry(str(registry_path))
            try:
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM token_history WHERE event_kind = 'issued'"
                ).fetchone()[0], 0)
                self.assertEqual(connection.execute(
                    "SELECT COUNT(*) FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued'",
                    (repair["token_id"],),
                ).fetchone()[0], 1)
                self.assertEqual(
                    connection.execute(
                        "SELECT evidence_state FROM capability_evidence_history "
                        "WHERE manifest_id = ? AND capability_key = '_registry.proof_route' "
                        "ORDER BY capability_evidence_id DESC LIMIT 1",
                        (manifest_id,),
                    ).fetchone()[0],
                    "contradicted",
                )
            finally:
                connection.close()

    def test_repair_cli_uses_its_own_claimed_canonical_launch_and_rejects_ordinary_launch(self) -> None:
        def issue_repair(root: Path) -> tuple[Path, Path, str, dict]:
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "repair.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"repair runtime")
            report = self.report(manifest_path, executable)
            report["proof_classification"].update({
                "status": "red", "verdict": "repair contradiction",
                "evidence_class": "startup/load", "feature_proof": False,
            })
            report["verdict"] = "red route"
            report["evidence_class"] = "startup/load"
            report["feature_proof"] = False
            report_path = root / "red.probe.report.json"
            self.write_json(report_path, report)
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                rebuild_manifest_projection(connection, scenarios)
                adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                red = ingest_report_reference(connection, report_path, adapters=adapters)
                route_key = connection.execute(
                    "SELECT route_key FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                ).fetchone()[0]
                binding = {
                    "runtime": self.bootstrap_runtime(executable),
                    "fixture": {"status": "compatible", "name": "", "profile": "",
                                "source_path": str(root), "source_sha256": hashlib.sha256(b"fixture").hexdigest()},
                    "profile": {"status": "compatible", "name": "", "profile": "",
                                "source_path": str(root), "source_sha256": hashlib.sha256(b"profile").hexdigest()},
                }
                repair = issue_registry_repair_token(
                    connection, parse_registry_query_request(self.bootstrap_request()),
                    manifest_id=connection.execute(
                        "SELECT manifest_id FROM verification_history WHERE verification_id = ?", (red["verification_id"],)
                    ).fetchone()[0],
                    route_key=route_key,
                    red_verification_id=red["verification_id"],
                    binding=binding,
                )
                self.assertTrue(repair.accepted, repair.reason)
                return registry_path, scenarios, repair.token_id, binding
            finally:
                connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id, binding = issue_repair(root)
            ordinary = self.run_cli("--registry", str(registry_path), "registry-launch", token_id)
            self.assertEqual(ordinary.returncode, 1)
            self.assertEqual(json.loads(ordinary.stderr)["result"]["reason"], "repair_token_requires_repair_launch")
            self.assertIn(("repair_invalidated", "ordinary_registry_launch"), self.token_events(registry_path, token_id))
            connection = open_registry(str(registry_path))
            try:
                issued = connection.execute(
                    "SELECT manifest_id, verification_id, route_key, details_json FROM token_history "
                    "WHERE token_id = ? AND event_kind = 'repair_issued'", (token_id,),
                ).fetchone()
                receipt = json.loads(issued["details_json"])
                successor = issue_registry_repair_token(
                    connection, parse_registry_query_request(receipt["query_json"]),
                    manifest_id=issued["manifest_id"], route_key=issued["route_key"],
                    red_verification_id=issued["verification_id"], binding=binding,
                )
                self.assertTrue(successor.accepted, successor.reason)
                self.assertNotEqual(successor.token_id, token_id)
                successor_details = json.loads(connection.execute(
                    "SELECT details_json FROM token_history WHERE token_id = ? AND event_kind = 'repair_issued'",
                    (successor.token_id,),
                ).fetchone()[0])
                self.assertEqual(successor_details["predecessor"]["token_id"], token_id)
                self.assertEqual(
                    successor_details["predecessor"]["terminal_event"]["reason"], "ordinary_registry_launch",
                )
            finally:
                connection.close()

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id, binding = issue_repair(root)
            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(scenario_registry_cli, "_current_repair_binding", return_value=binding), \
                    mock.patch.object(startup_harness, "compare_runtime_binding", return_value={"status": "matched"}), \
                    mock.patch.object(startup_harness, "run_probe_mode", return_value=31) as run_probe, \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-repair-launch", token_id,
                ])
            self.assertEqual(result, 31)
            run_probe.assert_called_once()
            self.assertTrue(run_probe.call_args.args[0].adaptive_semantic_autodrive)
            receipt = json.loads(run_probe.call_args.args[0].registry_launch_receipt)
            self.assertEqual(receipt["authority_kind"], "registry_repair_exact_contradiction")
            self.assertEqual(receipt["token_id"], token_id)
            self.assertEqual(receipt["wec_authority"]["evidence_class"], "focused feature proof")
            self.assertEqual(receipt["wec_authority"]["run_id"], token_id)
            connection = open_registry(str(registry_path))
            try:
                expected_red = connection.execute(
                    "SELECT verification_id FROM token_history "
                    "WHERE token_id = ? AND event_kind = 'repair_issued'",
                    (token_id,),
                ).fetchone()[0]
            finally:
                connection.close()
            self.assertEqual(receipt["red_verification_id"], expected_red)
            self.assertIn(("repair_claimed", "canonical_repair_probe_launch"), self.token_events(registry_path, token_id))
            with mock.patch.object(startup_harness, "compare_runtime_binding", return_value={"status": "matched"}):
                validated = startup_harness.validate_registry_launch_receipt_before_launch(
                    json.dumps(receipt), Path(binding["runtime"]["executable_path"]),
                )
            self.assertEqual(validated["status"], "compatible")
            self.assertEqual(validated["token_id"], token_id)
            self.assertEqual(validated["red_verification_id"], expected_red)

            changed_receipt = dict(receipt)
            changed_receipt["red_verification_id"] = "0" * 64
            with mock.patch.object(startup_harness, "compare_runtime_binding", return_value={"status": "matched"}):
                rejected = startup_harness.validate_registry_launch_receipt_before_launch(
                    json.dumps(changed_receipt), Path(binding["runtime"]["executable_path"]),
                )
            self.assertEqual(rejected["status"], "rejected")
            self.assertEqual(rejected["reason"], "receipt_contradiction_changed")

    def test_repair_detached_launch_uses_file_bridge_without_claiming_authority(self) -> None:
        """Repair authority reaches the live owner only through the bound file bridge."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            session_dir = root / "bridge-session"
            args = argparse.Namespace(
                command="registry-repair-detached-launch",
                repair_token="repair-token",
                session_dir=str(session_dir),
            )
            selection = RegistryRepairToken(
                "repair-token", True, "current", "repair", str(root / "repair.json"),
                {"executable_sha256": "exe", "runtime_source_sha256": "source"},
            )
            issued = {
                "manifest_id": "manifest", "verification_id": "5bb0e5f8", "route_key": "route",
                "details_json": json.dumps({
                    "authority_kind": "registry_repair_exact_contradiction",
                    "manifest_id": "manifest", "route_key": "route",
                    "red_verification_id": "5bb0e5f8", "binding": {"runtime": selection.runtime_binding},
                }),
            }
            connection = mock.Mock()
            connection.execute.side_effect = [mock.Mock(fetchone=mock.Mock(return_value=issued)),
                                              mock.Mock(fetchone=mock.Mock(return_value={
                                                  "declaration_json": "{}"}))]
            bridge_result = subprocess.CompletedProcess(
                [], 0, stdout=json.dumps({"ok": True, "bridge_pid": 123}), stderr="",
            )
            with mock.patch.object(scenario_registry_cli, "open_registry", return_value=connection), \
                    mock.patch.object(scenario_registry_cli, "_current_repair_binding", return_value={}), \
                    mock.patch.object(scenario_registry_cli, "reload_repair_token_for_launch", return_value=selection), \
                    mock.patch.object(startup_harness, "compare_runtime_binding", return_value={"status": "matched"}), \
                    mock.patch.object(scenario_registry_cli, "_declared_pre_descriptor_prefix", return_value=[]), \
                    mock.patch.object(scenario_registry_cli.subprocess, "run", return_value=bridge_result) as run, \
                    mock.patch.object(scenario_registry_cli, "_write_result") as write_result:
                result = scenario_registry_cli._launch_repair_file_bridge(args, root / "registry.sqlite3")

            self.assertEqual(result, 0)
            command = run.call_args.args[0]
            self.assertIn("cockpit_file_bridge.py", command[1])
            self.assertIn("registry-repair-launch", command)
            self.assertIn("--cockpit-live-session", command)
            self.assertIn("--adaptive-semantic-autodrive", command)
            self.assertIn("repair-token", command)
            receipt = write_result.call_args.args[0]
            self.assertTrue(receipt["ok"])
            self.assertEqual(receipt["repair_provenance"]["red_verification_id"], "5bb0e5f8")
            self.assertEqual(receipt["authority"],
                             "repair token remains unclaimed until the canonical child launch")
            connection.close.assert_called_once()

    def test_bound_token_launch_revalidates_runtime_without_a_human_permission_parameter(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
            ])
            other_registry = root / "other.sqlite3"
            other_connection = open_registry(str(other_registry))
            try:
                rebuild_manifest_projection(other_connection, scenarios)
            finally:
                other_connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 23)
            run_probe.assert_called_once()
            self.assertTrue(run_probe.call_args.args[0].adaptive_semantic_autodrive)
            expected = startup_harness.build_parser().parse_args(["probe", "cli"])
            expected.adaptive_semantic_autodrive = True
            received = vars(run_probe.call_args.args[0]).copy()
            receipt = json.loads(received.pop("registry_launch_receipt"))
            post_finalize_hook = received.pop("registry_post_finalize_hook")
            self.assertEqual(received, vars(expected))
            self.assertTrue(callable(post_finalize_hook))
            self.assertEqual(receipt["registry_path"], str(registry_path.resolve()))
            self.assertEqual(receipt["token_id"], token_id)
            self.assertNotIn("human_permission", receipt)
            self.assertEqual(receipt["source_path"], str((scenarios / "cli.json").resolve()))
            self.assertEqual(receipt["runtime_binding"]["schema"], 1)
            self.assertEqual(receipt["wec_authority"]["evidence_class"], "setup support")
            self.assertEqual(receipt["wec_authority"]["run_id"], token_id)
            self.assertEqual(
                receipt["wec_authority"]["binding_id"],
                receipt["runtime_binding"]["executable_sha256"],
            )
            self.assertFalse(receipt["diagnostic_replay"])
            self.assertEqual(self.token_events(registry_path, token_id), [("issued", "query_selection")])
            other_connection = sqlite3.connect(other_registry)
            try:
                self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM token_history").fetchone()[0], 0)
                self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 0)
            finally:
                other_connection.close()

    def test_registry_launch_rejects_changed_source_without_requery_or_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            manifest_path = scenarios / "cli.json"
            changed = self.strict_manifest()
            changed["description"] = "changed after selection"
            self.write_json(manifest_path, changed)

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_manifest_source_changed"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_registry_launch_rejects_stale_token_without_requery_or_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            connection = open_registry(str(registry_path))
            try:
                stale_adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "stale", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=stale_adapters),
                    {"reconciled": 1, "stale": 1},
                )
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "proof_route_stale"),
                ("invalidated", "registry_launch_token_invalidated"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_registry_launch_rejects_changed_route_binding_without_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            connection = open_registry(str(registry_path))
            try:
                changed_binding_adapters = BindingAdapters(
                    runtime=lambda _expected: {"status": "compatible", "facts": {}},
                    fixture=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"changed fixture").hexdigest(),
                    }},
                    profile=lambda _expected: {"status": "compatible", "facts": {
                        "source_sha256": hashlib.sha256(b"profile").hexdigest(),
                    }},
                )
                self.assertEqual(
                    reconcile_report_bindings(connection, adapters=changed_binding_adapters),
                    {"reconciled": 1, "stale": 0},
                )
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, token_id)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_route_binding_changed"),
            ])

    def test_registry_launch_records_malformed_token_rejection_without_probe_call(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            malformed_token = "malformed-selection-token"
            connection = sqlite3.connect(registry_path)
            try:
                issued = connection.execute(
                    "SELECT manifest_id, verification_id, route_key FROM token_history "
                    "WHERE token_id = ? AND event_kind = 'issued'",
                    (token_id,),
                ).fetchone()
                connection.execute(
                    "INSERT INTO token_history( "
                    "token_id, manifest_id, verification_id, route_key, event_kind, reason, details_json "
                    ") VALUES( ?, ?, ?, ?, 'issued', 'test_invalid_receipt', '{}' )",
                    (malformed_token, issued[0], issued[1], issued[2]),
                )
                connection.commit()
            finally:
                connection.close()

            result, run_probe = self.run_registry_launch(registry_path, scenarios, malformed_token)

            self.assertEqual(result, 1)
            run_probe.assert_not_called()
            self.assertEqual(self.token_events(registry_path, malformed_token), [
                ("issued", "test_invalid_receipt"),
                ("invalidated", "registry_launch_receipt_malformed"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM query_history").fetchone()[0], 1)
            finally:
                connection.close()

    def registry_launch_receipt(self, registry_path: Path, token_id: str, source_path: Path, executable: Path) -> str:
        runtime_binding = startup_harness.build_runtime_binding(executable)
        self.assertTrue(runtime_binding.get("ok"), runtime_binding.get("error"))
        return json.dumps({
            "schema": 1,
            "registry_path": str(registry_path),
            "token_id": token_id,
            "source_path": str(source_path),
            "runtime_binding": runtime_binding,
        })

    def run_startup_with_receipt(
        self,
        root: Path,
        executable: Path,
        receipt: str,
        launch_game: mock.Mock,
    ) -> None:
        args = startup_harness.build_parser().parse_args(["start", "--profile", "registry-receipt-test"])
        args.registry_launch_receipt = receipt
        plan = startup_harness.StartupPlan(
            profile="registry-receipt-test",
            userdir=str(root / "userdir"),
            executable=str(executable),
            strategy="harness_new_world",
            reason="test",
            target_world="",
            existing_worlds=[],
            fixture="",
            run_dir=str(root / "run"),
        )
        with mock.patch.object(startup_harness, "scenarios_root",
                               return_value=Path(json.loads(receipt)["source_path"]).parent), \
                mock.patch.object(startup_harness, "load_profile_config", return_value={"startup": {}}), \
                mock.patch.object(startup_harness, "purge_profile_flexbuffer_cache", return_value={}), \
                mock.patch.object(startup_harness, "build_plan", return_value=plan), \
                mock.patch.object(startup_harness, "game_child_environment", return_value={}), \
                mock.patch.object(startup_harness, "startup_gui_automation_required", return_value=False), \
                mock.patch.object(startup_harness, "kill_existing_game_processes", return_value=[]), \
                mock.patch.object(startup_harness, "config_dir_for_profile", return_value=root / "config"), \
                mock.patch.object(startup_harness, "latest_world_save_marker", return_value={}), \
                mock.patch.object(startup_harness, "copy_file_if_exists"), \
                mock.patch.object(startup_harness, "launch_game", launch_game):
            startup_harness.run_startup(args)

    def test_run_startup_rejects_post_receipt_source_change_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"selected binary")
            receipt = self.registry_launch_receipt(registry_path, token_id, scenarios / "cli.json", executable)
            changed = self.strict_manifest()
            changed["description"] = "changed after receipt"
            self.write_json(scenarios / "cli.json", changed)
            launch_game = mock.Mock()

            with self.assertRaisesRegex(SystemExit, "manifest_source_changed"):
                self.run_startup_with_receipt(root, executable, receipt, launch_game)

            launch_game.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_manifest_source_changed"),
            ])

    def test_run_startup_rejects_post_receipt_binary_change_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"selected binary")
            receipt = self.registry_launch_receipt(registry_path, token_id, scenarios / "cli.json", executable)
            executable.write_bytes(b"changed binary")
            launch_game = mock.Mock()

            with self.assertRaisesRegex(SystemExit, "runtime_binding_changed"):
                self.run_startup_with_receipt(root, executable, receipt, launch_game)

            launch_game.assert_not_called()
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("invalidated", "registry_launch_runtime_binding_changed"),
            ])

    def test_run_startup_allows_unchanged_receipt_to_reach_canonical_launch(self) -> None:
        class LaunchReached(RuntimeError):
            pass

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"selected binary")
            receipt = self.registry_launch_receipt(registry_path, token_id, scenarios / "cli.json", executable)
            launch_game = mock.Mock(side_effect=LaunchReached("canonical launch reached"))

            with self.assertRaisesRegex(LaunchReached, "canonical launch reached"):
                self.run_startup_with_receipt(root, executable, receipt, launch_game)

            launch_game.assert_called_once()
            self.assertEqual(self.token_events(registry_path, token_id), [("issued", "query_selection")])

    def test_registry_finalizer_ingests_one_durable_cleaned_probe_report_idempotently(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"selected binary")
            receipt = self.registry_launch_receipt(registry_path, token_id, scenarios / "cli.json", executable)
            hook = scenario_registry_cli._registry_post_finalize_ingest(receipt)
            run_dir = root / "run"
            report = self.report(scenarios / "cli.json", executable)
            report["cleanup"] = {"status": "already_exited", "pid": 42}

            with redirect_stdout(io.StringIO()):
                finalize_probe_report(run_dir, report, post_finalize_hook=hook)
                finalize_probe_report(run_dir, report, post_finalize_hook=hook)

            self.assertTrue((run_dir / "probe.report.json").is_file())
            self.assertEqual(self.token_events(registry_path, token_id), [
                ("issued", "query_selection"),
                ("verification_run", "report_ingested"),
            ])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(
                    connection.execute(
                        "SELECT COUNT(*) FROM report_ingestion_history WHERE report_path = ?",
                        (str((run_dir / "probe.report.json").resolve()),),
                    ).fetchone()[0],
                    1,
                )
            finally:
                connection.close()

    def test_registry_finalizer_skips_handoff_and_non_authoritative_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path, scenarios, token_id = self.issue_selection_token(root)
            executable = root / "cataclysm-tiles"
            executable.write_bytes(b"selected binary")
            receipt = self.registry_launch_receipt(registry_path, token_id, scenarios / "cli.json", executable)
            hook = scenario_registry_cli._registry_post_finalize_ingest(receipt)

            for name, cleanup in (
                ("missing", None),
                ("handoff", {"status": "deferred_handoff", "pid": 42}),
                ("failed", {"status": "failed", "pid": 42}),
            ):
                report = self.report(scenarios / "cli.json", executable)
                if cleanup is not None:
                    report["cleanup"] = cleanup
                if name == "handoff":
                    report["mode"] = "handoff"
                with redirect_stdout(io.StringIO()):
                    finalize_probe_report(root / name, report, post_finalize_hook=hook)

            self.assertEqual(self.token_events(registry_path, token_id), [("issued", "query_selection")])
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(connection.execute("SELECT COUNT(*) FROM report_ingestion_history").fetchone()[0], 1)
            finally:
                connection.close()

    def test_rebuild_ingest_reconcile_use_the_requested_registry_only(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "cli.json"
            self.write_json(manifest_path, self.strict_manifest())
            executable_path = root / "cataclysm-tiles"
            executable_path.write_bytes(b"cli runtime fixture")
            report_path = root / "probe.report.json"
            self.write_json(report_path, self.report(manifest_path, executable_path))
            registry_path = root / "requested.sqlite3"
            other_registry_path = root / "other.sqlite3"

            rebuilt = self.run_cli(
                "--registry", str(registry_path), "rebuild", "--scenarios-root", str(scenarios)
            )
            self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr)
            self.assertEqual(json.loads(rebuilt.stdout)["result"], {
                "staged": 1, "discovered": 1, "changed": 0, "absent": 0,
            })

            ingested = self.run_cli(
                "--registry", str(registry_path), "ingest-report", "--report", str(report_path)
            )
            self.assertEqual(ingested.returncode, 0, ingested.stderr)
            self.assertEqual(json.loads(ingested.stdout)["result"]["status"], "ingested")
            self.assertEqual(json.loads(ingested.stdout)["result"]["eligibility"], "hard_proven")

            reconciled = self.run_cli("--registry", str(registry_path), "reconcile")
            self.assertEqual(reconciled.returncode, 0, reconciled.stderr)
            self.assertEqual(json.loads(reconciled.stdout)["result"], {"reconciled": 1, "stale": 0})

            selected = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-json", json.dumps({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertEqual(selected.returncode, 0, selected.stderr)
            selected_result = json.loads(selected.stdout)["result"]
            self.assertIsNotNone(selected_result["token_id"])
            self.assertIsNone(selected_result["draft_path"])
            self.assertEqual(selected_result["selected_scenario_id"],
                             selected_result["candidates"][0]["scenario_id"])
            self.assertEqual(selected_result["candidates"][0]["lifecycle_state"], "active")

            no_match_query = {
                "requirements": [{
                    "key": "player.injured",
                    "op": "eq",
                    "value": True,
                    "minimum_evidence": "declared",
                }],
                "preferences": [],
            }
            no_match_path = root / "no-match.query.json"
            self.write_json(no_match_path, no_match_query)
            no_match = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-file", str(no_match_path)
            )
            self.assertEqual(no_match.returncode, 0, no_match.stderr)
            no_match_result = json.loads(no_match.stdout)["result"]
            self.assertIsNone(no_match_result["token_id"])
            self.assertTrue(Path(no_match_result["draft_path"]).is_file())
            self.assertFalse(json.loads(Path(no_match_result["draft_path"]).read_text(encoding="utf-8"))["executable"])

            connection = sqlite3.connect(registry_path)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM manifest_current").fetchone()[0], 1)
            self.assertEqual(connection.execute("SELECT COUNT(*) FROM report_ingestion_history").fetchone()[0], 1)
            self.assertEqual(
                connection.execute(
                    "SELECT resolution_kind FROM verification_resolution_history "
                    "ORDER BY resolution_event_id DESC LIMIT 1"
                ).fetchone()[0],
                "compatible",
            )
            self.assertEqual(
                connection.execute(
                    "SELECT evidence_state FROM capability_evidence_history "
                    "WHERE capability_key = '_registry.proof_route' "
                    "ORDER BY capability_evidence_id DESC LIMIT 1"
                ).fetchone()[0],
                "hard_proven",
            )
            connection.close()

            isolated = self.run_cli(
                "--registry", str(other_registry_path), "rebuild", "--scenarios-root", str(scenarios)
            )
            self.assertEqual(isolated.returncode, 0, isolated.stderr)
            other_connection = sqlite3.connect(other_registry_path)
            self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM manifest_current").fetchone()[0], 1)
            self.assertEqual(other_connection.execute("SELECT COUNT(*) FROM report_ingestion_history").fetchone()[0], 0)
            other_connection.close()

            isolated_query = self.run_cli(
                "--registry", str(other_registry_path), "registry-query", "--query-json", json.dumps({
                    "requirements": [{
                        "key": "player.injured",
                        "op": "eq",
                        "value": False,
                        "minimum_evidence": "declared",
                    }],
                    "preferences": [],
                }),
            )
            self.assertEqual(isolated_query.returncode, 0, isolated_query.stderr)
            self.assertIsNone(json.loads(isolated_query.stdout)["result"]["token_id"])

    def migration_report(self, manifest_path: Path, profile: str, *, status: str = "green") -> dict:
        feature_proof = status == "green"
        return {
            "mode": "probe",
            "scenario": manifest_path.stem,
            "contract": {"profile": profile},
            "scenario_manifest": {
                "source": {
                    "path": str(manifest_path.resolve()),
                    "sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                }
            },
            "proof_classification": {
                "status": status,
                "verdict": "feature_route_green" if feature_proof else "feature_route_red",
                "evidence_class": "feature-path" if feature_proof else "startup/load",
                "feature_proof": feature_proof,
            },
            "cleanup": {"status": "already_exited"},
        }

    def test_registry_migrate_all_classifies_invalid_blocked_and_review_only_without_runner(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            (scenarios / "invalid.json").write_text("{ invalid", encoding="utf-8")
            blocked = self.strict_manifest()
            blocked["status"] = "blocked"
            blocked["blocked_reason"] = "missing_fixture"
            self.write_json(scenarios / "blocked.json", blocked)
            self.write_json(scenarios / "review.json", {"name": "review", "steps": []})
            registry_path = root / "registry.sqlite3"

            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "run_probe_mode") as run_probe, \
                    redirect_stdout(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-migrate-all",
                    "--scenarios-root", str(scenarios),
                ])

            self.assertEqual(result, 0)
            run_probe.assert_not_called()
            connection = sqlite3.connect(registry_path)
            try:
                rows = connection.execute(
                    "SELECT source_path, disposition FROM migration_item WHERE event_kind = 'terminal' "
                    "ORDER BY source_path"
                ).fetchall()
                self.assertEqual(
                    [(Path(row[0]).stem, row[1]) for row in rows],
                    [("blocked", "blocked"), ("invalid", "invalid"), ("review", "imported")],
                )
                history = connection.execute(
                    "SELECT event_kind FROM migration_item WHERE source_path = ? ORDER BY migration_item_event_id",
                    (str((scenarios / "invalid.json").resolve()),),
                ).fetchall()
                self.assertEqual([row[0] for row in history], ["snapshot", "attempted", "terminal"])
            finally:
                connection.close()

    def test_retirement_cli_inspects_candidates_and_prepares_without_removal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            subject_path = root / "subject.json"
            successor_path = root / "successor.json"
            source_bytes = b'{"retirement":"cli"}\n'
            subject_path.write_bytes(source_bytes)
            successor_path.write_bytes(source_bytes)
            source_sha256 = hashlib.sha256(source_bytes).hexdigest()
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                with immediate_transaction(connection):
                    for manifest_id, source_path in (("subject", subject_path), ("successor", successor_path)):
                        connection.execute(
                            "INSERT INTO manifest_current( manifest_id, source_path, present, revision, current_sha256, "
                            "last_content_sha256, declaration_json, normalized_json, validation_json, last_seen_at ) "
                            "VALUES( ?, ?, 1, 1, ?, ?, '{}', '{}', '{}', CURRENT_TIMESTAMP )",
                            (manifest_id, str(source_path.resolve()), source_sha256, source_sha256),
                        )
                    connection.execute(
                        "INSERT INTO manifest_relation_current( manifest_id, relation_kind, target_kind, target_key, route_role ) "
                        "VALUES( 'subject', 'exact_duplicate_candidate', 'manifest', 'successor', '' )"
                    )
            finally:
                connection.close()

            candidates_result = self.run_cli(
                "--registry", str(registry_path), "retirement-candidates",
            )
            self.assertEqual(candidates_result.returncode, 0, candidates_result.stderr)
            candidates = json.loads(candidates_result.stdout)["result"]["candidates"]
            self.assertEqual(candidates[0]["manifest_id"], "subject")
            self.assertIn("exact_duplicate", candidates[0]["reasons"])
            prepared_result = self.run_cli(
                "--registry", str(registry_path), "approve-retirement",
                "--manifest-id", "subject", "--successor-manifest-id", "successor",
                "--source-sha256", source_sha256, "--reason", "exact_duplicate",
                "--reviewer", "cli-reviewer", "--approval", "approved",
            )
            self.assertEqual(prepared_result.returncode, 0, prepared_result.stderr)
            prepared = json.loads(prepared_result.stdout)["result"]
            self.assertTrue(prepared["approved"])
            self.assertTrue(subject_path.exists())
            status_result = self.run_cli(
                "--registry", str(registry_path), "registry-status", "--full", "--include-state", "quarantined",
            )
            self.assertEqual(status_result.returncode, 0, status_result.stderr)
            entries = json.loads(status_result.stdout)["result"]["entries"]
            subject = next(entry for entry in entries if entry["manifest"]["manifest_id"] == "subject")
            self.assertEqual(subject["lifecycle"]["state"], "quarantined")
            self.assertEqual(subject["history"]["actions"][0]["action_id"], prepared["action_id"])

    def test_registry_status_default_is_lossless_compact_receipt_with_exact_retrieval(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path = root / "registry.sqlite3"
            calls = []

            def status(connection, *, include_lifecycle_states=(), manifest_ids=()):
                calls.append((tuple(include_lifecycle_states), tuple(manifest_ids)))
                return ({"manifest": {"manifest_id": "only-this"}, "history": {"large": "payload"}},)

            compact_stdout = io.StringIO()
            with mock.patch.object(scenario_registry_cli, "registry_status", side_effect=status), \
                    redirect_stdout(compact_stdout):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-status", "--manifest-id", "only-this",
                ]), 0)
            compact_payload = json.loads(compact_stdout.getvalue())
            receipt = compact_payload["result"]
            self.assertEqual(receipt["schema"], "caol-command-receipt-v1")
            self.assertEqual(receipt["selector"], {
                "manifest_ids": ["only-this"],
                "include_states": [],
            })
            self.assertNotIn("entries", compact_stdout.getvalue())
            self.assertEqual(calls, [((), ("only-this",))])

            direct_full_stdout = io.StringIO()
            with mock.patch.object(scenario_registry_cli, "registry_status", side_effect=status), \
                    redirect_stdout(direct_full_stdout):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-status", "--full",
                    "--manifest-id", "only-this",
                ]), 0)
            direct_full = json.loads(direct_full_stdout.getvalue())
            self.assertEqual(
                direct_full["result"]["entries"][0]["manifest"]["manifest_id"], "only-this",
            )
            self.assertEqual(calls, [((), ("only-this",)), ((), ("only-this",))])

            full_stdout = io.StringIO()
            with redirect_stdout(full_stdout):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-artifact", "--sha256",
                    receipt["artifact"]["sha256"],
                ]), 0)
            recovered = json.loads(full_stdout.getvalue())
            self.assertEqual(recovered["result"]["result"]["entries"][0]["manifest"]["manifest_id"], "only-this")
            self.assertEqual(recovered["result"]["result"]["entries"][0]["history"]["large"], "payload")

            artifact_path = Path(receipt["artifact"]["path"])
            artifact_path.write_text("{}\n", encoding="utf-8")
            failed_stderr = io.StringIO()
            with redirect_stderr(failed_stderr):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-artifact", "--sha256",
                    receipt["artifact"]["sha256"],
                ]), 1)
            self.assertIn("artifact digest drift", failed_stderr.getvalue())

    def test_runtime_status_default_is_lossless_compact_receipt_with_exact_binding_selector(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            registry_path = root / "registry.sqlite3"
            observed = {
                "build_runtime_status": {
                    "status": "ready",
                    "evidence_ceiling": "requested run ceiling",
                    "large_diagnostic": "full-only-payload",
                },
                "runtime_binding": {
                    "runtime_source": {"sha256": "a" * 64},
                    "executable_path": "/exact/cataclysm-tiles",
                    "executable_sha256": "b" * 64,
                    "executable_error": "",
                },
            }
            compact_stdout = io.StringIO()
            with mock.patch.object(scenario_registry_cli, "_runtime_status", return_value=observed) as status, \
                    redirect_stdout(compact_stdout):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "runtime-status",
                    "--executable", "/exact/cataclysm-tiles",
                ]), 0)
            compact_payload = json.loads(compact_stdout.getvalue())
            receipt = compact_payload["result"]
            self.assertEqual(receipt["schema"], "caol-command-receipt-v1")
            self.assertEqual(receipt["command"], "runtime-status")
            self.assertEqual(receipt["selector"], {
                "executable_path": "/exact/cataclysm-tiles",
                "executable_sha256": "b" * 64,
                "runtime_source_sha256": "a" * 64,
                "isolated_harness_diagnosis": False,
            })
            self.assertNotIn("full-only-payload", compact_stdout.getvalue())
            status.assert_called_once_with(
                executable="/exact/cataclysm-tiles", isolated_harness_diagnosis=False,
            )

            full_stdout = io.StringIO()
            with redirect_stdout(full_stdout):
                self.assertEqual(scenario_registry_cli.main([
                    "--registry", str(registry_path), "runtime-status-artifact", "--sha256",
                    receipt["artifact"]["sha256"],
                ]), 0)
            recovered = json.loads(full_stdout.getvalue())
            self.assertEqual(
                recovered["result"]["result"]["build_runtime_status"]["large_diagnostic"],
                "full-only-payload",
            )

    def test_registry_migrate_all_uses_canonical_probe_and_reconciles_finalized_report(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "valid.json"
            self.write_json(manifest_path, self.strict_manifest())
            registry_path = root / "registry.sqlite3"
            seen_namespaces = []

            def run_probe(namespace: argparse.Namespace) -> int:
                seen_namespaces.append(namespace)
                run_dir = root / "finalized"
                report = self.migration_report(manifest_path, namespace.profile)
                with redirect_stdout(io.StringIO()):
                    finalize_probe_report(
                        run_dir,
                        report,
                        post_finalize_hook=namespace.registry_post_finalize_hook,
                    )
                return 17

            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "run_probe_mode", side_effect=run_probe), \
                    redirect_stdout(io.StringIO()):
                result = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-migrate-all",
                    "--scenarios-root", str(scenarios),
                ])

            self.assertEqual(result, 0)
            self.assertEqual(len(seen_namespaces), 1)
            namespace = seen_namespaces[0]
            expected = startup_harness.build_parser().parse_args([
                "probe", "valid", "--profile", namespace.profile,
            ])
            received = vars(namespace).copy()
            receipt = json.loads(received.pop("registry_migration_receipt"))
            received.pop("registry_post_finalize_hook")
            self.assertEqual(received, vars(expected))
            self.assertTrue(namespace.profile.startswith("registry-migration-"))
            self.assertEqual(receipt["source_path"], str(manifest_path.resolve()))
            self.assertEqual(receipt["profile"], namespace.profile)
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(
                    connection.execute(
                        "SELECT event_kind, disposition FROM migration_item ORDER BY migration_item_event_id"
                    ).fetchall(),
                    [("snapshot", "snapshotted"), ("attempted", "attempted"),
                     ("launch_claimed", "launch_claimed"), ("terminal", "verified")],
                )
            finally:
                connection.close()

    def test_registry_migrate_resume_reconciles_report_and_never_relaunches_claim(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            manifest_path = scenarios / "valid.json"
            self.write_json(manifest_path, self.strict_manifest())
            registry_path = root / "registry.sqlite3"
            userdir_root = root / "profiles"
            received = []

            def run_probe(namespace: argparse.Namespace) -> int:
                received.append(namespace)
                return 9

            def userdir_for_profile(profile: str) -> Path:
                return userdir_root / profile

            with mock.patch.object(startup_harness, "scenarios_root", return_value=scenarios), \
                    mock.patch.object(startup_harness, "userdir_for_profile", side_effect=userdir_for_profile), \
                    mock.patch.object(startup_harness, "run_probe_mode", side_effect=run_probe), \
                    redirect_stdout(io.StringIO()):
                initial = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-migrate-all",
                    "--scenarios-root", str(scenarios),
                ])
                self.assertEqual(initial, 0)
                self.assertEqual(len(received), 1)
                receipt = json.loads(received[0].registry_migration_receipt)
                interrupted_resume = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-migrate-all",
                    "--resume", receipt["migration_run_id"],
                ])
                self.assertEqual(interrupted_resume, 0)
                self.assertEqual(len(received), 1)
                interrupted_connection = sqlite3.connect(registry_path)
                try:
                    self.assertEqual(
                        interrupted_connection.execute(
                            "SELECT COUNT(*) FROM migration_item WHERE event_kind = 'terminal'"
                        ).fetchone()[0],
                        0,
                    )
                finally:
                    interrupted_connection.close()
                run_dir = userdir_for_profile(receipt["profile"]) / "harness_runs" / "durable"
                run_dir.mkdir(parents=True)
                self.write_json(run_dir / "probe.report.json", self.migration_report(manifest_path, receipt["profile"]))
                resumed = scenario_registry_cli.main([
                    "--registry", str(registry_path), "registry-migrate-all",
                    "--resume", receipt["migration_run_id"],
                ])

            self.assertEqual(resumed, 0)
            self.assertEqual(len(received), 1)
            connection = sqlite3.connect(registry_path)
            try:
                self.assertEqual(
                    connection.execute(
                        "SELECT event_kind, disposition FROM migration_item ORDER BY migration_item_event_id"
                    ).fetchall(),
                    [("snapshot", "snapshotted"), ("attempted", "attempted"),
                     ("launch_claimed", "launch_claimed"), ("terminal", "verified")],
                )
            finally:
                connection.close()

    def test_migration_durable_report_classification_is_explicit(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest_path = root / "scenario.json"
            self.write_json(manifest_path, self.strict_manifest())
            receipt = {
                "source_path": str(manifest_path.resolve()),
                "source_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                "profile": "registry-migration-test",
            }
            for status, expected in (("red", "contradicted"), ("amber", "failed")):
                report_path = root / f"{status}.report.json"
                self.write_json(report_path, self.migration_report(manifest_path, receipt["profile"], status=status))
                terminal = scenario_registry_cli._migration_report_terminal(
                    receipt,
                    report_path,
                    json.loads(report_path.read_text(encoding="utf-8")),
                )
                self.assertIsNotNone(terminal)
                self.assertEqual(terminal[0], expected)

    def test_migration_failure_report_uses_recorded_startup_profile_when_contract_is_absent(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest_path = root / "scenario.json"
            self.write_json(manifest_path, self.strict_manifest())
            receipt = {
                "source_path": str(manifest_path.resolve()),
                "source_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                "profile": "registry-migration-test",
            }
            report = self.migration_report(manifest_path, receipt["profile"], status="amber")
            report.pop("contract")
            report["startup"] = {"profile": receipt["profile"]}
            report_path = root / "startup-gated.report.json"
            self.write_json(report_path, report)

            terminal = scenario_registry_cli._migration_report_terminal(receipt, report_path, report)

            self.assertIsNotNone(terminal)
            self.assertEqual(terminal[0], "failed")

    def test_migration_completion_reenumerates_changed_added_and_removed_identities(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            scenarios = root / "scenarios"
            scenarios.mkdir()
            changed_path = scenarios / "changed.json"
            removed_path = scenarios / "removed.json"
            self.write_json(changed_path, {"name": "changed", "steps": []})
            self.write_json(removed_path, {"name": "removed", "steps": []})
            registry_path = root / "registry.sqlite3"
            connection = open_registry(str(registry_path))
            try:
                migration = scenario_registry_cli.snapshot_migration_run(
                    connection,
                    scenarios,
                    launcher_identity="scenario_registry_cli.registry-migrate-all",
                )
                self.assertEqual(len(migration.items), 2)
                original_changed_sha = migration.items[0].source_sha256
                self.write_json(changed_path, {"name": "changed", "description": "new bytes", "steps": []})
                removed_path.unlink()
                self.write_json(scenarios / "added.json", {"name": "added", "steps": []})
                self.write_json(scenarios / "added_two.json", {"name": "added_two", "steps": []})

                with mock.patch.object(startup_harness, "run_probe_mode") as run_probe:
                    summary = scenario_registry_cli._reconcile_migration_final_set(
                        connection,
                        registry_path=registry_path,
                        migration=migration,
                    )

                run_probe.assert_not_called()
                self.assertEqual(summary["filesystem_totals"], {
                    "identities": 3,
                    "executable": 0,
                    "non_executable": 3,
                })
                self.assertTrue(summary["final_set_equals_terminal_set"])
                self.assertTrue(summary["once_only_launches"])
                self.assertTrue(summary["completion_ready"])
                self.assertEqual(summary["terminal_rows"], 5)
                self.assertEqual(summary["disposition_counts"], {"failed": 2, "imported": 3})
                self.assertTrue(any(
                    entry[0] == str(removed_path.resolve()) and entry[1] == "source_removed_during_migration"
                    for entry in connection.execute(
                        "SELECT source_path, reason FROM migration_item WHERE event_kind = 'terminal'"
                    ).fetchall()
                ))
                changed_rows = connection.execute(
                    "SELECT source_sha256 FROM migration_item WHERE source_path = ? AND event_kind = 'snapshot' "
                    "ORDER BY migration_item_event_id",
                    (str(changed_path.resolve()),),
                ).fetchall()
                self.assertEqual(len(changed_rows), 2)
                self.assertEqual(changed_rows[0][0], original_changed_sha)
                self.assertNotEqual(changed_rows[1][0], original_changed_sha)
                self.assertEqual(
                    connection.execute("SELECT COUNT(*) FROM migration_run_event WHERE status = 'succeeded'").fetchone()[0],
                    1,
                )
            finally:
                connection.close()

    def test_invalid_invocation_is_machine_readable_and_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            registry_path = Path(temp_dir) / "registry.sqlite3"
            result = self.run_cli("--registry", str(registry_path), "ingest-report")
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, "")
            error = json.loads(result.stderr)
            self.assertFalse(error["ok"])
            self.assertIn("--report", error["error"])
            self.assertFalse(registry_path.exists())

            missing_query = self.run_cli("--registry", str(registry_path), "registry-query")
            self.assertEqual(missing_query.returncode, 2)
            self.assertIn("one of the arguments", json.loads(missing_query.stderr)["error"])

            conflicting_query = self.run_cli(
                "--registry", str(registry_path), "registry-query",
                "--query-json", "{}", "--query-file", str(registry_path),
            )
            self.assertEqual(conflicting_query.returncode, 2)
            self.assertIn("not allowed with argument", json.loads(conflicting_query.stderr)["error"])

            malformed_query = self.run_cli(
                "--registry", str(registry_path), "registry-query", "--query-json", "{",
            )
            self.assertEqual(malformed_query.returncode, 1)
            self.assertIn("valid JSON", json.loads(malformed_query.stderr)["error"])


    def test_post_relaunch_continuation_does_not_count_its_initial_session_as_reentry(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "scenario.json"
            source.write_text(json.dumps({
                "post_relaunch": {
                    "steps": [{"kind": "cockpit_live_session"}],
                },
            }), encoding="utf-8")
            selection = argparse.Namespace(source_path=str(source))
            self.assertEqual(
                scenario_registry_cli._declared_live_session_reentries(selection), 1
            )
            self.assertEqual(
                scenario_registry_cli._declared_live_session_reentries(
                    selection, post_relaunch_continuation=True,
                ), 0
            )
            source.write_text(json.dumps({
                "post_relaunch": {
                    "steps": [
                        {"kind": "cockpit_live_session"},
                        {"kind": "cockpit_live_session"},
                    ],
                },
            }), encoding="utf-8")
            self.assertEqual(
                scenario_registry_cli._declared_live_session_reentries(
                    selection, post_relaunch_continuation=True,
                ), 1
            )


if __name__ == "__main__":
    unittest.main()
