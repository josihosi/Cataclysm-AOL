#!/usr/bin/env python3
"""Focused lifecycle controls for the canonical post-save probe relaunch."""

from __future__ import annotations

import sys
import hashlib
import hmac
import tempfile
import unittest
import json
import os
import subprocess
from contextlib import ExitStack
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness as harness  # noqa: E402
from identity_binding import _plain, seal_complete_round_manifest  # noqa: E402
from scenario_registry_store import (  # noqa: E402
    append_certification_lease_event,
    append_certification_lifecycle_event,
    _issue_registry_certification_authority,
    open_registry,
    register_certification_round,
)


class ProbeRelaunchTest(unittest.TestCase):
    @staticmethod
    def receipt_proof(receipt, capability):
        facts = "\n".join([
            str(receipt["round_id"]), str(receipt["lease_id"]),
            str(receipt["save_sequence"]),
            str(receipt["previous_world_tree_sha256"]),
            str(receipt["previous_world_save_sha256"]),
            str(receipt["current_world_tree_sha256"]),
            str(receipt["current_world_save_sha256"]),
            str(receipt["process_pid"]),
        ])
        return hmac.new(capability.encode(), facts.encode(), hashlib.sha256).hexdigest()

    def test_world_save_progression_rejects_replacement_rollback_gap_and_wrong_stream(self) -> None:
        """The same path is not a save lineage without the ordered receipt chain."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)

            def make_case(name: str):
                case = root / name; case.mkdir()
                repo = case / "repo"; repo.mkdir(); (repo / "tracked").write_bytes(b"base")
                subprocess.run(["git", "init", "-q", str(repo)], check=True)
                subprocess.run(["git", "-C", str(repo), "add", "tracked"], check=True)
                subprocess.run(["git", "-C", str(repo), "-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "base"], check=True)
                executable = case / "game"; executable.write_bytes(b"game")
                scenario = case / "scenario"; scenario.write_bytes(b"scenario")
                fixture = case / "fixture"; fixture.write_bytes(b"fixture")
                profile = case / "profile"; profile.write_bytes(b"profile")
                data = case / "data"; data.mkdir(); (data / "input").write_bytes(b"data")
                harness_root = case / "harness"; harness_root.mkdir(); (harness_root / "input").write_bytes(b"harness")
                world = case / "world"; world.mkdir(); (world / "player.sav").write_bytes(b"initial")
                inputs = {
                    "repo_root": str(repo), "executable": str(executable), "runtime_paths": ["tracked"],
                    "data_config_roots": [str(data)], "harness_roots": [str(harness_root)],
                    "scenario_path": str(scenario), "fixture_path": str(fixture), "profile_path": str(profile),
                    "world_dir": str(world), "player_save": "player.sav",
                    "saved_player_payload": {"player": {"id": "player-1"}},
                    "ecology_audit": {"actors": [{"actor_id": "actor-1"}]},
                }
                manifest = seal_complete_round_manifest(
                    round_id="round-" + name, scenario_lineage_id="lineage", authority_id="authority",
                    authority_kind="automated-certification", event_stream_id="events-" + name,
                    repo_root=repo, executable=executable, runtime_paths=["tracked"],
                    data_config_roots=[data], harness_roots=[harness_root], scenario_path=scenario,
                    fixture_path=fixture, profile_path=profile, world_dir=world, player_save="player.sav",
                    saved_player_payload=inputs["saved_player_payload"], ecology_audit=inputs["ecology_audit"],
                )
                recheck_path = case / "inputs.json"; recheck_path.write_text(json.dumps(inputs), encoding="utf-8")
                connection = open_registry(str(case / "registry.sqlite3"))
                authority = _issue_registry_certification_authority(
                    connection, round_id=manifest["round_id"], binding_id=manifest["binding_id"],
                    source_sha256=manifest["binding"]["authoritative_components"]["scenario"]["content_sha256"],
                    launch_token="relaunch-test-token-" + name,
                )
                manifest = _plain(manifest)
                manifest["authority_id"] = authority["authority_id"]
                manifest["manifest_sha256"] = harness.canonical_digest(
                    {key: value for key, value in manifest.items() if key != "manifest_sha256"},
                    domain="caol-round-manifest:v1",
                )
                register_certification_round(connection, manifest)
                save_capability = "game-only-capability-" + name
                connection.execute(
                    "INSERT INTO certification_save_capability(round_id, capability_commitment, owner_identity) VALUES(?,?,?)",
                    (manifest["round_id"], hashlib.sha256(save_capability.encode()).hexdigest(), "test"),
                )
                event_path = case / "events.jsonl"
                return world, manifest, connection, {
                    "connection": connection, "manifest": manifest, "lease_id": "lease",
                    "recheck_inputs_path": str(recheck_path), "event_stream_path": str(event_path),
                    "save_capability": save_capability,
                }

            def write_save_receipt(manifest, connection, context):
                append_certification_lease_event(
                    connection, round_id=manifest["round_id"], lease_id="lease", event_sequence=1,
                    event_kind="transferred", process_identity="bound-process", world_identity="world",
                )
                inputs = harness._certification_recheck_producer_inputs(context["recheck_inputs_path"])
                current = harness._certification_world_save_fact(
                    harness.authoritative_identity_binding(**inputs)["authoritative_components"]["world_save"]
                )
                prior = harness._certification_world_save_chain(connection, manifest)["accepted"]
                receipt = {
                    "round_id": manifest["round_id"], "event_stream_id": manifest["event_stream_id"],
                    "lease_id": "lease", "process_identity": "bound-process", "process_pid": 101,
                    "save_sequence": 1,
                    **{"previous_" + key: value for key, value in prior.items()},
                    **{"current_" + key: value for key, value in current.items()},
                }
                receipt["proof"] = self.receipt_proof(receipt, context["save_capability"])
                event = {
                    "schema_version": 1, "sequence": 1, "run_id": manifest["event_stream_id"],
                    "game_minutes": 1, "domain": "certification", "transition": "save_receipt",
                    "outcome": "committed", "certification_save_receipt": receipt,
                }
                Path(context["event_stream_path"]).write_text(json.dumps(event) + "\n", encoding="utf-8")

            world, _manifest, connection, context = make_case("replacement")
            try:
                (world / "player.sav").write_bytes(b"same-path-replacement")
                rejected = harness.certification_record_world_save_progression(
                    context, event_stream_id=context["manifest"]["event_stream_id"],
                )
                self.assertFalse(rejected["ok"])
                self.assertEqual(rejected["reason"], "world_save_progression_receipt_stream_invalid")
                self.assertEqual(rejected["invalidation"]["first_component"], "world_save")
            finally:
                connection.close()

            world, manifest, connection, context = make_case("auth-missing")
            try:
                (world / "player.sav").write_bytes(b"progressed")
                append_certification_lease_event(
                    connection, round_id=manifest["round_id"], lease_id="lease", event_sequence=1,
                    event_kind="transferred", process_identity="bound-process", world_identity="world",
                )
                current = harness._certification_world_save_fact(
                    harness.authoritative_identity_binding(
                        **harness._certification_recheck_producer_inputs(context["recheck_inputs_path"])
                    )["authoritative_components"]["world_save"]
                )
                prior = harness._certification_world_save_chain(connection, manifest)["accepted"]
                Path(context["event_stream_path"]).write_text(json.dumps({
                    "schema_version": 1, "sequence": 1, "run_id": manifest["event_stream_id"],
                    "game_minutes": 1, "domain": "certification", "transition": "save_receipt",
                    "outcome": "committed", "certification_save_receipt": {
                        "round_id": manifest["round_id"], "event_stream_id": manifest["event_stream_id"],
                        "lease_id": "lease", "process_identity": "bound-process", "save_sequence": 1,
                        "authentication": context["save_capability"],
                        **{"previous_" + key: value for key, value in prior.items()},
                        **{"current_" + key: value for key, value in current.items()},
                    },
                }) + "\n", encoding="utf-8")
                rejected = harness.certification_record_world_save_progression(
                    context, event_stream_id=manifest["event_stream_id"],
                )
                self.assertFalse(rejected["ok"])
                self.assertEqual(rejected["reason"], "world_save_progression_receipt_legacy_authentication_forbidden")
            finally:
                connection.close()

            world, manifest, connection, context = make_case("rollback")
            try:
                (world / "player.sav").write_bytes(b"progressed")
                register_certification_round(connection, manifest)
                write_save_receipt(manifest, connection, context)
                accepted = harness.certification_record_world_save_progression(
                    context, event_stream_id=manifest["event_stream_id"]
                )
                self.assertTrue(accepted["ok"])
                (world / "player.sav").write_bytes(b"initial")
                rollback = harness.certification_recheck_round(context, segment="post_relaunch")
                self.assertFalse(rollback["ok"])
                self.assertEqual(rollback["invalidation"]["first_component"], "world_save")
            finally:
                connection.close()

            world, manifest, connection, context = make_case("gap")
            try:
                (world / "player.sav").write_bytes(b"gap")
                register_certification_round(connection, manifest)
                append_certification_lifecycle_event(
                    connection, round_id=manifest["round_id"], event_sequence=1,
                    event_kind="world_save_progressed", details={"save_sequence": 2},
                )
                gap = harness.certification_recheck_round(context, segment="post_relaunch")
                self.assertFalse(gap["ok"])
                self.assertEqual(gap["invalidation"]["first_component"], "world_save")
            finally:
                connection.close()

            world, manifest, connection, context = make_case("wrong-stream")
            try:
                (world / "player.sav").write_bytes(b"progressed")
                register_certification_round(connection, manifest)
                append_certification_lifecycle_event(
                    connection, round_id=manifest["round_id"], event_sequence=1,
                    event_kind="world_save_progressed",
                    details={"save_sequence": 1, "event_stream_id": "another-run"},
                )
                wrong_stream = harness.certification_recheck_round(context, segment="post_relaunch")
                self.assertFalse(wrong_stream["ok"])
                self.assertEqual(wrong_stream["invalidation"]["first_component"], "world_save")
            finally:
                connection.close()

    def test_certification_segments_recheck_same_round_across_relaunch_then_invalidate_drift(self) -> None:
        """Exercise the production recheck/registry seam without launching C-AOL."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            repo = root / "repo"; repo.mkdir(); (repo / "tracked").write_bytes(b"base")
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "add", "tracked"], check=True)
            subprocess.run(["git", "-C", str(repo), "-c", "user.name=test", "-c", "user.email=test@example.invalid", "commit", "-qm", "base"], check=True)
            executable = root / "cataclysm-tiles"; executable.write_bytes(b"game")
            scenario = root / "scenario.json"; scenario.write_bytes(b"scenario")
            fixture = root / "fixture.json"; fixture.write_bytes(b"fixture")
            profile = root / "profile.json"; profile.write_bytes(b"profile")
            data = root / "data"; data.mkdir(); (data / "config.json").write_bytes(b"config")
            harness_root = root / "harness"; harness_root.mkdir(); (harness_root / "runner.py").write_bytes(b"runner")
            world = root / "world"; world.mkdir(); (world / "player.sav").write_bytes(b"save")
            inputs = {
                "repo_root": str(repo), "executable": str(executable), "runtime_paths": ["tracked"],
                "data_config_roots": [str(data)], "harness_roots": [str(harness_root)],
                "scenario_path": str(scenario), "fixture_path": str(fixture), "profile_path": str(profile),
                "world_dir": str(world), "player_save": "player.sav",
                "saved_player_payload": {"player": {"id": "player-1"}},
                "ecology_audit": {"actors": [{"actor_id": "actor-1"}]},
            }
            manifest = seal_complete_round_manifest(
                round_id="round-1", scenario_lineage_id="lineage-1", authority_id="authority-1",
                authority_kind="automated-certification", event_stream_id="events-1",
                repo_root=repo, executable=executable, runtime_paths=["tracked"],
                data_config_roots=[data], harness_roots=[harness_root], scenario_path=scenario,
                fixture_path=fixture, profile_path=profile, world_dir=world, player_save="player.sav",
                saved_player_payload=inputs["saved_player_payload"], ecology_audit=inputs["ecology_audit"],
            )
            manifest_path = root / "round.json"; manifest_path.write_text(json.dumps(_plain(manifest)), encoding="utf-8")
            recheck_path = root / "current-inputs.json"; recheck_path.write_text(json.dumps(inputs), encoding="utf-8")
            connection = open_registry(str(root / "registry.sqlite3"))
            authority = _issue_registry_certification_authority(
                connection, round_id=manifest["round_id"], binding_id=manifest["binding_id"],
                source_sha256=manifest["binding"]["authoritative_components"]["scenario"]["content_sha256"],
                launch_token="relaunch-round-test-token",
            )
            manifest = _plain(manifest)
            manifest["authority_id"] = authority["authority_id"]
            manifest["manifest_sha256"] = harness.canonical_digest(
                {key: value for key, value in manifest.items() if key != "manifest_sha256"},
                domain="caol-round-manifest:v1",
            )
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            register_certification_round(connection, manifest)
            save_capability = "game-only-capability-round"
            connection.execute(
                "INSERT INTO certification_save_capability(round_id, capability_commitment, owner_identity) VALUES(?,?,?)",
                (manifest["round_id"], hashlib.sha256(save_capability.encode()).hexdigest(), "test"),
            )
            event_path = root / "events.jsonl"
            context = {"connection": connection, "manifest": manifest, "lease_id": "lease-1",
                       "recheck_inputs_path": str(recheck_path), "event_stream_path": str(event_path),
                       "save_capability": save_capability}
            try:
                startup_check = harness.certification_recheck_round(context, segment="startup_prelaunch")
                self.assertTrue(startup_check["ok"], startup_check)
                self.assertTrue(harness.certification_recheck_round(context, segment="initial_evidence_segment")["ok"])
                # A normal save changes bytes but stays on the sealed world/save
                # lineage; player and actor identities are rechecked separately.
                (world / "player.sav").write_bytes(b"ordinary-save-progress")
                append_certification_lease_event(
                    connection, round_id=manifest["round_id"], lease_id="lease-1", event_sequence=1,
                    event_kind="transferred", process_identity="bound-process", world_identity="world",
                )
                current = harness._certification_world_save_fact(
                    harness.authoritative_identity_binding(
                        **harness._certification_recheck_producer_inputs(str(recheck_path))
                    )["authoritative_components"]["world_save"]
                )
                prior = harness._certification_world_save_chain(connection, manifest)["accepted"]
                receipt = {
                    "round_id": "round-1", "event_stream_id": "events-1", "lease_id": "lease-1",
                    "process_identity": "bound-process", "process_pid": 101, "save_sequence": 1,
                    **{"previous_" + key: value for key, value in prior.items()},
                    **{"current_" + key: value for key, value in current.items()},
                }
                receipt["proof"] = self.receipt_proof(receipt, save_capability)
                event_path.write_text(json.dumps({
                    "schema_version": 1, "sequence": 1, "run_id": "events-1", "game_minutes": 1,
                    "domain": "certification", "transition": "save_receipt", "outcome": "committed",
                    "certification_save_receipt": receipt,
                }) + "\n", encoding="utf-8")
                with mock.patch.dict(os.environ, {"OPENCLAW_CERTIFICATION_SAVE_CAPABILITY": save_capability}), \
                        mock.patch.object(harness, "wait_for_pid_exit", return_value=True), \
                        mock.patch.object(harness, "run_json_command", return_value=(0, {
                            "ok": True, "pid": 202, "focus": {"ok": True},
                            "proof_classification": {"startup_clean_for_feature_steps": True},
                        }, "", "")) as launch:
                    relaunch = harness.run_probe_post_relaunch(
                        initial_pid=101, initial_process_command="/tmp/cataclysm-tiles", profile="profile", config_profile="profile", world="world",
                        scenario_name="scenario", registry_launch_receipt="", terminal_exit_timeout_seconds=1,
                        certification_registry=str(root / "registry.sqlite3"),
                        certification_round_manifest=str(manifest_path), certification_lease_id="lease-1",
                        certification_recheck_inputs=str(recheck_path),
                        transition_event_run_id="events-1",
                        transition_event_path=str(event_path),
                    )
                self.assertEqual(relaunch["status"], "ready", relaunch)
                self.assertIn("--certification-recheck-inputs", launch.call_args.args[0])
                post_relaunch_check = harness.certification_recheck_round(context, segment="post_relaunch_evidence_segment")
                self.assertTrue(post_relaunch_check["ok"], post_relaunch_check)
                inputs["ecology_audit"] = {"actors": [{"actor_id": "replacement-actor"}]}
                recheck_path.write_text(json.dumps(inputs), encoding="utf-8")
                drift = harness.certification_recheck_round(context, segment="later_evidence_segment")
                self.assertFalse(drift["ok"])
                self.assertEqual(drift["invalidation"]["first_component"], "actors", drift)
                lifecycle = connection.execute(
                    "SELECT event_kind FROM certification_round_lifecycle WHERE round_id = ? ORDER BY event_sequence",
                    ("round-1",),
                ).fetchall()
                self.assertEqual([row[0] for row in lifecycle], [
                    "segment_rechecked", "segment_rechecked", "world_save_progressed",
                    "segment_rechecked", "segment_recheck_failed",
                ])
            finally:
                connection.close()

    def test_post_relaunch_contract_requires_terminal_label_exit_bound_and_steps(self) -> None:
        initial = [{"label": "save_and_exit", "kind": "press"}]
        with self.assertRaisesRegex(SystemExit, "terminal_save_step_label"):
            harness.normalize_post_relaunch_contract({}, initial)
        with self.assertRaisesRegex(SystemExit, "must name an initial"):
            harness.normalize_post_relaunch_contract({
                "terminal_save_step_label": "missing",
                "terminal_exit_timeout_seconds": 1,
                "steps": [{"kind": "wait", "seconds": 1}],
            }, initial)
        with self.assertRaisesRegex(SystemExit, "positive number"):
            harness.normalize_post_relaunch_contract({
                "terminal_save_step_label": "save_and_exit",
                "terminal_exit_timeout_seconds": 0,
                "steps": [{"kind": "wait", "seconds": 1}],
            }, initial)

    def test_relaunch_uses_canonical_start_same_world_new_pid_and_focus(self) -> None:
        start_result = {
            "ok": True,
            "pid": 202,
            "run_dir": "/tmp/relaunch-run",
            "focus": {"ok": True},
            "proof_classification": {"startup_clean_for_feature_steps": True},
        }
        with mock.patch.object(harness, "wait_for_pid_exit", return_value=True), \
                mock.patch.object(harness, "run_json_command", return_value=(0, start_result, "out", "err")) as run:
            result = harness.run_probe_post_relaunch(
                initial_pid=101,
                initial_process_command="/tmp/cataclysm-tiles",
                profile="probe-profile",
                config_profile="dev-harness",
                world="McWilliams",
                scenario_name="test.relaunch",
                registry_launch_receipt="receipt",
                terminal_exit_timeout_seconds=2.5,
                artifact_run_dir=Path("/tmp/parent-run"),
            )

        self.assertEqual(result["status"], "ready")
        self.assertEqual(result["pid"], 202)
        command = run.call_args.args[0]
        self.assertEqual(command[command.index("--world") + 1], "McWilliams")
        self.assertNotIn("--fixture", command)
        self.assertNotIn("--profile-snapshot", command)
        self.assertIn("--registry-launch-receipt", command)
        self.assertEqual(
            command[command.index("--harness-artifact-run-dir") + 1],
            str(Path("/tmp/parent-run").resolve()),
        )

    def test_relaunch_rejects_same_pid_and_missing_original_exit(self) -> None:
        with mock.patch.object(harness, "wait_for_pid_exit", return_value=False), \
                mock.patch.object(harness, "run_json_command") as run:
            missing_exit = harness.run_probe_post_relaunch(
                initial_pid=101,
                initial_process_command="/tmp/cataclysm-tiles",
                profile="profile",
                config_profile="config",
                world="McWilliams",
                scenario_name="test",
                registry_launch_receipt="",
                terminal_exit_timeout_seconds=1,
            )
        self.assertEqual(missing_exit["status"], "terminal_process_exit_missing")
        run.assert_not_called()

        with mock.patch.object(harness, "wait_for_pid_exit", return_value=True), \
                mock.patch.object(harness, "run_json_command", return_value=(0, {
                    "ok": True,
                    "pid": 101,
                    "focus": {"ok": True},
                    "proof_classification": {"startup_clean_for_feature_steps": True},
                }, "", "")):
            same_pid = harness.run_probe_post_relaunch(
                initial_pid=101,
                initial_process_command="/tmp/cataclysm-tiles",
                profile="profile",
                config_profile="config",
                world="McWilliams",
                scenario_name="test",
                registry_launch_receipt="",
                terminal_exit_timeout_seconds=1,
            )
        self.assertEqual(same_pid["status"], "same_pid_relaunch_rejected")

    def test_probe_runs_post_relaunch_steps_then_finalizes_once_with_new_pid(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            run_dir = root / "run"
            run_dir.mkdir()
            artifact_log = root / "debug.log"
            artifact_log.write_text("", encoding="utf-8")
            args = SimpleNamespace(
                scenario="test.relaunch", profile="", world="", fixture=None,
                replace_existing_worlds=False, advance_turns=None, settle_seconds=None,
                artifact_pattern="", test_command="", dry_run=False, compact_stdout=True,
                registry_launch_receipt="", registry_post_finalize_hook=None,
            )
            scenario = {
                "name": "test.relaunch",
                "profile": "probe-profile",
                "world": "McWilliams",
                "steps": [{"label": "save_and_exit", "kind": "press", "keys": ["q"]}],
                "post_relaunch": {
                    "terminal_save_step_label": "save_and_exit",
                    "terminal_exit_timeout_seconds": 1,
                    "steps": [{"label": "observe_saved_identity", "kind": "wait", "seconds": 1}],
                },
            }
            first_start = {
                "ok": True, "pid": 101, "run_dir": str(run_dir),
                "screen": {}, "proof_classification": {"startup_clean_for_feature_steps": True},
                "debug_log_identity": {}, "debug_log_classified_size": 0,
            }
            relaunch_start = {
                "ok": True, "pid": 202, "run_dir": str(root / "relaunch"),
                "focus": {"ok": True},
                "proof_classification": {"startup_clean_for_feature_steps": True},
            }
            finalize = mock.Mock()
            execute = mock.Mock(side_effect=[
                [{"label": "save_and_exit", "kind": "press"}],
                [{"label": "observe_saved_identity", "kind": "wait"}],
            ])
            screenshot = mock.Mock(return_value={"screen_summary": {}})
            patches = {
                "load_scenario": mock.Mock(return_value=scenario),
                "scenario_manifest_binding": mock.Mock(return_value={}),
                "scenario_blocker_info": mock.Mock(return_value={"status": "active"}),
                "resolve_profile_name": mock.Mock(return_value="probe-profile"),
                "resolve_startup_config_profile": mock.Mock(return_value="dev-harness"),
                "resolve_scenario_profile_option_overrides": mock.Mock(return_value={}),
                "portal_storm_policy_from_scenario": mock.Mock(return_value={}),
                "run_json_command": mock.Mock(side_effect=[(0, first_start, "", ""), (0, relaunch_start, "", "")]),
                "resolve_artifact_source": mock.Mock(return_value=(artifact_log, False, "debug.log")),
                "probe_runtime_blockers": mock.Mock(return_value=[]),
                "probe_runtime_warnings": mock.Mock(return_value=[]),
                "read_current_saved_weather_audit": mock.Mock(return_value={}),
                "config_dir_for_profile": mock.Mock(return_value=root),
                "capture_screenshot": screenshot,
                "execute_probe_steps": execute,
                "wait_for_pid_exit": mock.Mock(return_value=True),
                "capture_feature_phase_guard": mock.Mock(return_value={"status": "green", "ledger_row": {}}),
                "render_derived_screens": mock.Mock(return_value=[]),
                "declared_screen_artifact_matches": mock.Mock(return_value=[]),
                "summarize_wait_step_ledgers": mock.Mock(return_value={"status": "green"}),
                "build_portal_storm_warning_for_report": mock.Mock(return_value={}),
                "build_probe_step_ledger": mock.Mock(return_value=[]),
                "portal_storm_step_ledger_rows": mock.Mock(return_value=[]),
                "summarize_probe_step_ledger": mock.Mock(return_value={"status": "green"}),
                "probe_proof_classification": mock.Mock(return_value={"feature_proof": False}),
                "finalize_scenario_report": finalize,
            }
            with ExitStack() as stack:
                for name, replacement in patches.items():
                    stack.enter_context(mock.patch.object(harness, name, replacement))
                self.assertEqual(harness.run_probe_mode(args), 0)

            self.assertEqual([call.args[0] for call in execute.call_args_list], [101, 202])
            self.assertEqual(execute.call_args_list[1].kwargs["artifact_baseline"], 0)
            self.assertTrue(any(call.args[0] == 202 for call in screenshot.call_args_list))
            finalize.assert_called_once()
            self.assertEqual(finalize.call_args.kwargs["cleanup_pid"], 202)
            report = finalize.call_args.args[1]
            self.assertEqual(report["relaunch"]["status"], "ready")
            self.assertEqual(report["relaunch"]["artifact_log_pre_relaunch_size"], 0)
            self.assertEqual(report["steps"][-1]["phase"], "post_relaunch")
            relaunch_command = patches["run_json_command"].call_args_list[1].args[0]
            self.assertEqual(
                relaunch_command[relaunch_command.index("--harness-artifact-run-dir") + 1],
                str(run_dir.resolve()),
            )


if __name__ == "__main__":
    unittest.main()
