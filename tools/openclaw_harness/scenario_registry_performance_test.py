"""Connected Python route tests; synthetic native producers earn no live proof."""
from __future__ import annotations

import copy
import hashlib
import io
import json
import os
import subprocess
from pathlib import Path
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from contextlib import redirect_stdout
from dataclasses import asdict
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cockpit
import playtest_witness
import cockpit_file_bridge
import evidence_events
import process_performance
import scenario_registry
import scenario_registry_cli
import scenario_registry_ingestion_test as ingestion_fixtures
import scenario_registry_store as store
import startup_harness
from evidence_display import retain
from scenario_registry_performance import (CONTRACT, CAPABILITY, SCENARIO, performance_contract,
                                          prepare_saved_world_continuation, validate_performance_witness)


CHARTER = {"claim": "Observe a native faction work interval", "material_proof": "Native work, resources, census and save/exit",
           "current_uncertainty": "Which actual interval is supported", "requested_evidence_ceiling": "focused"}


def manifest():
    return {
        "manifest_version": 1, "name": SCENARIO, "profile": "performance-test", "world": "",
        "harness_new_world": "PerformanceTest",
        "harness_raw_seed": "830205385", "harness_new_world_scenario": "evacuee",
        "capabilities": {CAPABILITY: "baseline_then_adaptive_active_party_expansion",
                         "runtime.native_transport": "windows_conpty",
                         "capabilities.performance.target_population_sequence": {"active_parties": [1, 10, 20, 50, 100]}},
        "runtime_contract": {
            "grants_gameplay_proof": False, "setup_only_debug": True, "disposable_copy": True,
            "permitted_input": ["cockpit:game.observe", "cockpit:run.witness"], "forbidden_input": ["direct-state-mutation"],
            "helpers": ["WindowsCursesTerminalTransport"], "permissions": [], "platform": ["windows"],
            "profile": "performance-test", "fixture": "", "playtest_mode": "terminal", "terminal_transport": "pty",
            "requirements": {"os": "windows", "source": "current-worktree", "profile": "performance-test", "fixture": "none",
                             "helper": "WindowsCursesTerminalTransport", "peekaboo": False, "input": ["native"], "ocr": False, "cleanup": True}},
        "steps": [{"kind": "native_semantic_bootstrap", "label": "start"}, {"kind": "cockpit_live_session", "label": "work"}],
        "proof_route": {"precondition": ["start"], "production_behavior": ["work"], "terminal_persistence": ["work"],
                        "artifact_verdict": ["work"], "disallowed_shortcuts": ["work"]},
        "evidence_contract": {"performance_witness": dict(CONTRACT)},
    }


class PerformanceRouteTest(unittest.TestCase):
    def write(self, path, value):
        Path(path).write_text(json.dumps(value), encoding="utf-8")

    def setUp(self):
        self.charter = CHARTER
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.run = self.root / "run"
        self.session = self.root / "session"
        self.generator_run = self.root / "generator-run"
        for path in (self.run, self.generator_run):
            path.mkdir(parents=True)
        self.patch = mock.patch.object(startup_harness, "userdir_for_profile",
                                       side_effect=lambda profile: self.root / "profiles" / profile)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        evidence_store = mock.patch.object(evidence_events, "retain", side_effect=lambda value: retain(value, self.root / "evidence-display"))
        evidence_store.start()
        self.addCleanup(evidence_store.stop)
        self.connection = store.open_registry(str(self.root / "registry.sqlite3"))
        self.addCleanup(self.connection.close)
        (self.root / "game.exe").write_bytes(b"synthetic executable, never launched")
        self.runtime = {"status": "matched", "runtime_source_sha256": "a" * 64,
                        "executable_sha256": hashlib.sha256((self.root / "game.exe").read_bytes()).hexdigest(),
                        "executable_path": str(self.root / "game.exe")}
        self.generator_profile = "performance-test-run-" + "0" * 32
        with mock.patch.object(startup_harness, "create_run_dir", return_value=self.generator_run):
            self.generator_plan = startup_harness.build_plan(
                self.generator_profile, "", "", "PerformanceTest", "830205385",
                explicit_executable=self.runtime["executable_path"], harness_new_world_scenario="evacuee")
        self.initial_world = startup_harness.save_dir_for_profile(self.generator_profile) / "PerformanceTest"
        self.initial_world.mkdir(parents=True)
        # A controlled native generator saved and exited before any live work.
        (self.initial_world / "dimension_data.gsav").write_text("# version 1\n{}")
        (self.initial_world / "player.sav").write_bytes(b"native initial save double")
        marker = startup_harness.latest_world_save_marker(self.generator_profile, "PerformanceTest")
        self.generator_startup = {
            "ok": True, "reason": "harness_world_saved", "strategy": "harness_new_world", "exit_code": 0,
            "feature_proof": False, "profile": self.generator_profile, "pid": 121,
            "run_dir": str(self.generator_run), "harness_new_world": "PerformanceTest", "harness_raw_seed": "830205385",
            "executable_path": self.runtime["executable_path"],
            "readiness": {"kind": "harness_process_exit", "pid": 121, "returncode": 0,
                "marker": marker, "save_marker_created": True, "world_owned_by_run": True,
                "baseline_save_marker_mtime": 0, "debug_error_evidence_count": 0, "feasibility_artifact_ready": True},
            "generation_identity": {"world": "PerformanceTest", "raw_seed": "830205385", "ordinary_scenario": "evacuee",
                "profile": self.generator_profile, "run_dir": str(self.generator_run), "run_id": "setup-run"}}
        self.write(self.generator_run / "startup.result.json", self.generator_startup)
        self.write(self.generator_run / "runtime.binding.json", {**self.runtime, "runtime_source_sha256": "c" * 64})
        self.write(self.generator_run / "terminal.owner.json", {
            "run_id": "setup-run", "game_pid": 121, "game_process_generation": {"pid": 121,
                "birth_identity": "windows-filetime:123", "executable_path": self.runtime["executable_path"]},
            "launch_state": "exited", "child_exit_code": 0, "native_handles_closed": True, "native_lease_handles_closed": True})
        self.declaration = prepare_saved_world_continuation(manifest(), self.generator_run / "startup.result.json")
        self.scenarios = self.root / "scenarios"
        self.scenarios.mkdir()
        self.path = self.scenarios / "performance.json"
        self.write(self.path, self.declaration)
        store.rebuild_manifest_projection(self.connection, self.scenarios)
        self.manifest_id = self.connection.execute("SELECT manifest_id FROM manifest_current").fetchone()[0]
        self.execution = store.execute_registry_query(self.connection, store.parse_registry_query_request({"requirements": [
            {"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}], "preferences": []}),
            witness_charter=CHARTER, drafts_root=self.root / "drafts")
        self.produce_native_interval()

    def produce_native_interval(self):
        self.run_id = self.execution.token_id
        self.assertIsNotNone(self.run_id)
        self.produce_selected_bridge()
        self.binding_id = json.loads((self.session / "bridge.manifest.json").read_text())["binding_id"]
        # Use the real selected saved-start producer, including its independent
        # random profile UUID, parser, copy verification and normal game plan.
        selection = store.RegistryLaunchToken(self.run_id, True, "selected", SCENARIO,
            str(self.path), hashlib.sha256(self.path.read_bytes()).hexdigest())
        args = self.selected_args
        self.launch = scenario_registry_cli._registry_launch_probe_namespace(selection,
            cockpit_live_session=True, profile_override=args.profile,
            post_relaunch_continuation=args.post_relaunch_continuation, saved_world_snapshot=args.saved_world_snapshot)
        self.profile = self.launch.profile
        self.userdir = startup_harness.userdir_for_profile(self.profile)
        self.world = self.userdir / "save" / "PerformanceTest"
        self.installed = startup_harness.install_saved_world_snapshot(
            self.profile, "PerformanceTest", self.launch.saved_world_snapshot)
        with mock.patch.object(startup_harness, "create_run_dir", return_value=self.run):
            self.plan = startup_harness.build_plan(self.profile, "PerformanceTest", "",
                                                  explicit_executable=self.runtime["executable_path"])
        self.write(self.run / "plan.json", asdict(self.plan))
        self.write(self.run / "runtime.binding.json", self.runtime)
        self.generation = {"pid": 123, "birth_identity": "windows-filetime:456", "executable_path": self.runtime["executable_path"]}
        self.owner = {"run_id": self.run_id, "binding_id": self.binding_id, "pid": 123,
                      "process_generation": self.generation, "userdir": str(self.userdir),
                      "world": "PerformanceTest", "playtest_mode": "terminal",
                      "log_paths": {"transition_events": {"path": str(self.run / "transition.events.jsonl"), "scope": "run_bound"}}}
        self.write(self.session / "game-process.json", self.owner)
        self.write(self.session / "game-process-exit.json", {**self.owner, "exit_code": 0,
            "exit_observed_at": (datetime.now(timezone.utc) + timedelta(seconds=1)).isoformat(timespec="milliseconds")})
        self.process = {"run_id": self.run_id, "pid": 123, "alive": False, "exit_code": 0,
                        "identity_status": "exited", "process_generation": self.generation,
                        "evidence_ref": str(self.session / "game-process-exit.json")}
        self.dimension = {"overmapbuffer": {"bandit_live_world": {"sites": [
            {"site_id": "site-a", "site_kind": "camp_style", "members": [{"npc_id": 7, "state": "outbound"}]}]},
            "migo_populations": [], "migo_towers": []}}
        self.save_dimension()
        self.behavior = {"schema_version": 1, "sequence": 1, "run_id": self.run_id, "domain": "bandit_live_world",
                         "site_id": "site-a", "operation_id": "outing-a", "actor_ids": [7], "game_minutes": 101,
                         "transition": "local_pair_abstract_resume_progress", "outcome": "committed"}
        self.write_record(self.run / "transition.events.jsonl", self.behavior)
        # Exercise the real ProcessPerformance producer with controlled process samples.
        samples = iter([self.sample(1), self.sample(2), self.sample(3)])
        performance = process_performance.ProcessPerformance(
            self.session, pid=123, run_id=self.run_id, binding_id=self.binding_id,
            source_binding={"runtime_binding_sha256": hashlib.sha256((self.run / "runtime.binding.json").read_bytes()).hexdigest()},
            sampler=lambda pid: next(samples))
        frame = {"kind": "world", "game_minutes": 100}
        performance.begin_action(frame, "world.wait")
        performance.end_action(frame, {"accepted": True, "next_frame": {"game_minutes": 102}})
        self.assertIsNotNone(performance.latest)
        self.measurement = performance.latest
        # Existing cockpit producer supplies its own save completion matching and exit observation.
        channel = cockpit.CockpitRunChannel(lambda: {}, lambda *args: {}, binding_id=self.binding_id,
            read_process_state=lambda: self.process, witness_charter=CHARTER,
            witness_identity={"scenario_id": SCENARIO, "source_identity": self.runtime["runtime_source_sha256"],
                              "executable_identity": self.runtime["executable_sha256"]})
        channel._run_id = self.run_id
        world_receipt = {"accepted": True, "run_id": self.run_id, "request_id": "save-1", "action_id": "world.save_quit",
                         "requested_run_id": self.run_id, "requested_surface_id": "world-1", "requested_frame_id": "frame-1"}
        completion = {**world_receipt, "schema": "caol-native-save-completion-v1", "serializer_result": "saved",
                      "save_succeeded": True, "world_name": "PerformanceTest", "player_save_id": "player",
                      "artifact_identity": {"kind": "harness_run_directory", "value": str(self.run)}}
        channel._transcript.extend([
            {"kind": "action", "action_id": "world.save_quit", "result": {"receipt": {"native_receipt": world_receipt}}},
            {"kind": "action", "action_id": "prompt.choose", "result": {"receipt": {"native_receipt": {
                "accepted": True, "run_id": self.run_id, "native_save_completion": {"status": "matched", "completion": completion}}}}}])
        observation = channel.observe_process_exit()
        self.assertTrue(channel.seal_witness_journal(observation_id=observation["observation_id"], stop_reason="interval ended", unused_authority="none")["ok"])
        self.journal = channel._sealed_journal
        citation = next(entry["citation_id"] for entry in self.journal["entries"] if entry["kind"] == "observation")
        self.statement = {"verdict": "proved", "smallest_supported_claim": "The named faction member advanced its native route during the resource interval.",
                          "causal_account": "Exact producer records identify the actor, committed route progress and elapsed game-time interval.",
                          "recommended_disposition": "accept", "evidence_ceiling": "focused",
                          "citations": [{"citation_id": citation, "meaning": "bound OS exit", "checks": ["value.surface.facts"]}]}
        initial = {**self.statement, "verdict": "inconclusive", "recommended_disposition": "continue"}
        final_result = channel.finish(observation_id=observation["observation_id"], stop_reason="interval ended", unused_authority="none", witness=initial)
        self.assertTrue(final_result["ok"], final_result)
        self.final = channel._final_report
        self.write(self.run / startup_harness.TRANSITION_EVENT_BINDING_FILENAME, {"run_id": self.run_id})
        with redirect_stdout(io.StringIO()), mock.patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": self.binding_id}), \
                mock.patch.object(startup_harness, "semantic_step_trace_start_for_source", return_value=0), \
                mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=CHARTER), \
                mock.patch.object(startup_harness, "open_cockpit_game_service", return_value=cockpit.CockpitService(run_channel=channel)), \
                mock.patch.object(startup_harness, "serve_cockpit_live", return_value=0), \
                mock.patch.object(startup_harness, "read_completed_cockpit_final", return_value=self.final):
            native_steps = startup_harness.execute_probe_steps(123, self.run,
                [{"kind": "cockpit_live_session", "label": "work"}], profile=self.profile, world="PerformanceTest",
                scenario_identity=SCENARIO, runtime_binding=self.runtime, grants_gameplay_proof=False, semantic_only=True)
        self.report = ingestion_fixtures.ScenarioRegistryIngestionTest().report(self.path)
        self.report.update(scenario=SCENARIO, contract={"profile": self.profile,
                                                     "config_profile": "performance-test", "world": "PerformanceTest", "capture_world_after": True},
                           steps=native_steps,
                           proof_classification={"status": "yellow", "feature_proof": False, "evidence_class": "startup/load-or-inconclusive"},
                           feature_proof=False, evidence_class="startup/load-or-inconclusive", verdict="yellow_interval", certification_lifecycle={})
        self.report["startup"]["screen"].update(runtime_binding_status="matched", runtime_binding_observed=self.runtime)
        self.report["startup"].update(profile=self.profile, strategy=self.plan.strategy, saved_world_snapshot=self.installed)
        self.report["world_snapshot"] = startup_harness.describe_saved_world_state(self.profile, "PerformanceTest", self.run)
        self.report_path = self.run / "probe.report.json"
        self.publish_report()
        self.statement["performance_evidence"] = {"behavior": [self.handle(self.run / "transition.events.jsonl")],
                                                    "measurements": [self.handle(self.session / "performance.jsonl")]}

    def publish_report(self):
        # Exercise the actual source-bound report producer, not synthesized
        # metadata that could conceal an omitted field in production.
        self.report.pop("scenario_manifest", None)
        # Registry source/report identity and native bridge identity are
        # independently produced bindings in the real launcher; do not equate.
        report_binding = "registry-source-" + self.binding_id
        self.report.update(run_id=self.run_id, binding_id=report_binding,
            wec_authority=startup_harness.seal_wec_authority(evidence_class="setup support",
                authority="registry", run_id=self.run_id, binding_id=report_binding,
                source_sha256=hashlib.sha256(self.path.read_bytes()).hexdigest()))
        loaded = json.loads(self.path.read_text())
        loaded["_scenario_registry"] = scenario_registry.validate_manifest(loaded, path=self.path)
        with redirect_stdout(io.StringIO()):
            startup_harness.finalize_scenario_report(self.run, self.report, scenario=loaded, compact_stdout=True)
        from cockpit_report_reference import load_report
        _, _, self.report = load_report(self.report_path)

    def reenter(self, *, change_product=False):
        parent = self.report_path
        self.declaration = prepare_saved_world_continuation(self.declaration,
            self.generator_run / "startup.result.json", parent_report=parent)
        index = len(list(self.root.glob("reentry-*"))) + 1
        self.run = self.root / ("reentry-" + str(index))
        self.run.mkdir()
        self.session = self.root / ("session-" + str(index))
        if change_product:
            executable = self.root / ("successor-" + str(index) + ".exe")
            executable.write_bytes(b"independently source-bound compatible successor")
            self.runtime = {**self.runtime, "runtime_source_sha256": str(index) * 64,
                "executable_path": str(executable), "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
        self.write(self.path, self.declaration)
        store.rebuild_manifest_projection(self.connection, self.scenarios)
        self.execution = store.execute_registry_query(self.connection, store.parse_registry_query_request({"requirements": [
            {"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}], "preferences": []}),
            witness_charter=CHARTER, drafts_root=self.root / "drafts")
        self.produce_native_interval()

    def produce_selected_bridge(self):
        """Keep the actual selected command, both bridge parsers and manifest."""
        charter = self.root / "charter.json"
        self.write(charter, CHARTER)
        self.selected_args = SimpleNamespace(command="registry-detached-launch",
            selection_token=self.run_id, session_dir=str(self.session), witness_charter=str(charter),
            profile="", post_relaunch_continuation=False, saved_world_snapshot="")
        selection = store.RegistryLaunchToken(self.run_id, True, "selected", SCENARIO,
            str(self.path), source_sha256=hashlib.sha256(self.path.read_bytes()).hexdigest(), runtime_binding=self.runtime)

        def launch_serve(command, **kwargs):
            self.assertEqual(command[2], "serve")
            self.assertEqual(cockpit_file_bridge.main(command[2:]), 0)
            return SimpleNamespace(pid=999)

        def launch_bridge(command, **kwargs):
            self.assertEqual(command[2], "start")
            self.selected_bridge_command = command
            output = io.StringIO()
            with redirect_stdout(output), \
                    mock.patch.object(cockpit_file_bridge.subprocess, "Popen", side_effect=launch_serve), \
                    mock.patch.object(cockpit_file_bridge.FileBackedCockpitBridge, "serve",
                                      new=lambda bridge: bridge.prepare() or 0), \
                    mock.patch.object(startup_harness, "process_generation_snapshot", return_value={"pid": 999}):
                self.assertEqual(cockpit_file_bridge.main(command[2:]), 0)
            return subprocess.CompletedProcess(command, 0, output.getvalue(), "")

        with mock.patch.object(scenario_registry_cli, "reload_selection_token_for_launch", return_value=selection), \
                mock.patch.object(scenario_registry_cli, "_selected_executable", return_value=Path(self.runtime["executable_path"])), \
                mock.patch.object(scenario_registry_cli, "_selected_runtime_binding", return_value=self.runtime), \
                mock.patch.object(scenario_registry_cli, "_current_source_executable_readiness", return_value={"status": "ready"}), \
                mock.patch.object(scenario_registry_cli.subprocess, "run", side_effect=launch_bridge), \
                mock.patch.object(scenario_registry_cli, "_write_result"):
            self.assertEqual(scenario_registry_cli._launch_selection_file_bridge(
                self.selected_args, self.root / "registry.sqlite3"), 0)

    def save_dimension(self):
        (self.world / "dimension_data.gsav").write_text("# version 1\n" + json.dumps(self.dimension), encoding="utf-8")

    def sample(self, seconds):
        return {"pid": 123, "platform": "windows", "process_identity": str(456 + 504911232000000000),
                "sampled_monotonic_seconds": seconds, "cpu_seconds": seconds / 2,
                "resident_memory": {"status": "available", "value": 100000, "unit": "bytes"}}

    def write_record(self, path, value):
        path.write_text(json.dumps(value) + "\n", encoding="utf-8")

    def handle(self, path):
        # Consume real evidence query handles including producer and retained_raw.
        result = evidence_events.query([{"path": str(path), "producer": "native_test_double"}], {}, limit=1)
        return result["rows"][0]["source"]

    def ingest(self, *, selected=True):
        adapters = store.BindingAdapters(runtime=lambda expected: {"status": "compatible", "facts": self.runtime},
            fixture=lambda expected: {"status": "compatible", "facts": {"source_sha256": "f" * 64}},
            profile=lambda expected: {"status": "compatible", "facts": {"source_sha256": "b" * 64}})
        if selected:
            result = store.ingest_token_linked_report_reference(self.connection, self.execution.token_id, self.report_path, adapters=adapters)
        else:
            result = store.ingest_report_reference(self.connection, self.report_path, adapters=adapters)
        self.assertEqual(result["status"], "ingested", result)
        self.report_id = result["report_id"]
        return result

    def record(self):
        return store.record_playtest_witness(self.connection, manifest_id=self.manifest_id, report_ids=[self.report_id],
                                             charter=self.charter, journal=self.journal, statement=self.statement)

    def validate_native(self, **changes):
        arguments = dict(declaration=self.declaration, report=self.report, report_path=self.report_path,
                         final=self.final, journal=self.journal, statement=self.statement, runtime=self.runtime)
        arguments.update(changes)
        return validate_performance_witness(**arguments)

    def test_generator_exit_then_selected_saved_start_reaches_native_witness_consumer(self):
        generator = manifest()
        self.assertFalse(performance_contract(generator))
        self.assertEqual(store._run_evidence_ceiling(self.connection, self.manifest_id, generator), "zero-credit")
        generator["steps"] = [{"kind": "native_world_generation", "label": "generate"}]
        generator["capabilities"]["runtime.entry_mode"] = "native_world_generation"
        setup = startup_harness.generation_only_probe_report(generator, 0, self.generator_startup,
            expected_profile=self.generator_profile, expected_run_id="setup-run")
        self.assertTrue(setup["ok"], setup)
        self.assertFalse(setup["feature_proof"])
        self.assertFalse(setup["scenario_setup"]["interventions"][0]["gameplay_credit"])
        self.assertTrue(self.launch.post_relaunch_continuation)
        self.assertEqual(self.launch.fixture, "")
        self.assertEqual(startup_harness.saved_world_continuation_steps(self.declaration["steps"], None),
                         self.declaration["steps"])
        self.assertEqual(self.plan.strategy, "autoload_world")
        self.assertNotEqual(self.profile, "performance-test-run-" + self.run_id)
        command = startup_harness.build_game_command(self.root / "game.exe", self.profile, "PerformanceTest",
                                                      isolated_writable=True)
        self.assertNotIn("--harness-new-world", command)
        self.assertEqual(command[command.index("--world") + 1], "PerformanceTest")
        self.ingest()
        result = self.record()["performance"]
        self.assertEqual(result["scope"], "observed_native_work_interval")
        self.assertEqual(result["initial_world"]["run_id"], "setup-run")
        self.assertFalse(result["whole_sequence_proven"])
        self.assertFalse(result["gameplay_credit"])

    def test_two_native_reentries_keep_origin_parent_and_successor_product_distinct(self):
        from cockpit_report_reference import load_report
        origin = copy.deepcopy(self.declaration["evidence_contract"]["performance_witness"]["initial_world"])
        first_run, first_report = self.run_id, self.report_path
        first_product = copy.deepcopy(self.runtime)
        self.reenter(change_product=True)
        self.assertNotEqual(self.runtime["executable_sha256"], first_product["executable_sha256"])
        self.assertNotEqual(self.runtime["runtime_source_sha256"], first_product["runtime_source_sha256"])
        self.assertNotEqual(self.run_id, first_run)
        self.ingest()
        result = self.record()["performance"]
        self.assertEqual(result["continuation"]["validated_parent_edges"], 1)
        self.assertFalse(result["continuation"]["prior_interval_hash_missing"])
        self.assertEqual(result["continuation"]["parent_report"]["path"], str(first_report.resolve()))
        self.assertEqual(self.declaration["evidence_contract"]["performance_witness"]["initial_world"], origin)
        # Consume ancestor world bytes and product after installation: the
        # already captured report edge stays authoritative, not a proof copy.
        import shutil
        _, _, parent = load_report(first_report)
        shutil.rmtree(parent["world_snapshot"]["source"])
        Path(first_product["executable_path"]).unlink()
        self.reenter()
        self.ingest()
        result = self.record()["performance"]
        self.assertEqual(result["continuation"]["validated_parent_edges"], 2)
        self.assertEqual(result["behavior"][0]["operation_id"], "outing-a")
        self.assertEqual(len(result["measurements"]), 1)
        self.assertFalse(result["gameplay_credit"])
        self.assertFalse(result["population_sequence_credit"])
        # Parent observations cannot substitute for the new interval's work.
        old_statement = copy.deepcopy(self.statement)
        old_statement["performance_evidence"]["behavior"] = [self.handle(first_report.parent / "transition.events.jsonl")]
        with self.assertRaisesRegex(ValueError, "source_path_or_range_mismatch"):
            self.validate_native(statement=old_statement)

    def test_retained_world_missing_prior_hash_requires_fresh_noncredit_report(self):
        # The first report has a native save/exit, but no trustworthy capture.
        # It cannot be retroactively promoted or synthesized into a parent.
        old_report, old_run = self.report_path, self.run_id
        self.report.pop("world_snapshot")
        self.publish_report()
        with self.assertRaisesRegex(ValueError, "saved_world_producer_mismatch"):
            prepare_saved_world_continuation(self.declaration, self.generator_run / "startup.result.json",
                                            parent_report=old_report)
        retained = self.world
        bootstrap = prepare_saved_world_continuation(self.declaration,
            self.generator_run / "startup.result.json", saved_world_snapshot=retained)
        self.assertNotIn("parent_report", bootstrap["evidence_contract"]["performance_witness"])
        self.assertNotEqual(bootstrap["evidence_contract"]["performance_witness"]["input_sha256"],
                            bootstrap["evidence_contract"]["performance_witness"]["initial_world"]["sha256"])
        self.declaration = bootstrap
        self.write(self.path, bootstrap)
        store.rebuild_manifest_projection(self.connection, self.scenarios)
        # New supported selection and report establish only present custody.
        self.run = self.root / "bootstrap"
        self.run.mkdir()
        self.session = self.root / "bootstrap-session"
        self.execution = store.execute_registry_query(self.connection, store.parse_registry_query_request({"requirements": [
            {"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}], "preferences": []}),
            witness_charter=CHARTER, drafts_root=self.root / "drafts")
        self.produce_native_interval()
        self.assertNotEqual(self.run_id, old_run)
        with self.assertRaisesRegex(ValueError, "unattested_input_requires_bootstrap"):
            self.validate_native()
        negative = {key: value for key, value in self.statement.items() if key != "performance_evidence"}
        negative.update(evidence_ceiling="setup-only", verdict="inconclusive")
        self.assertEqual(self.validate_native(statement=negative)["scope"], "none")
        self.reenter(change_product=True)
        result = self.validate_native()
        self.assertEqual(result["continuation"]["validated_parent_edges"], 1)
        self.assertTrue(result["continuation"]["prior_interval_hash_missing"])
        self.assertEqual(result["scope"], "observed_native_work_interval")
        self.assertEqual(len(result["measurements"]), 1)

    def test_parent_report_mutation_missing_sources_world_hash_and_exit_rejected(self):
        parent = self.report_path
        self.reenter()
        good = copy.deepcopy(self.declaration)
        self.assertEqual(self.validate_native()["continuation"]["validated_parent_edges"], 1)
        for mutate, reason in (
            (lambda d: d["evidence_contract"]["performance_witness"]["parent_report"].update(sha256="0" * 64), "parent_report_source_mismatch"),
            (lambda d: d["evidence_contract"]["performance_witness"].update(input_sha256="0" * 64), "input_save_hash_mismatch"),
            (lambda d: d.update(world="wrong-world"), "generated_world_identity_mismatch")):
            bad = copy.deepcopy(good)
            mutate(bad)
            with self.assertRaisesRegex(ValueError, reason):
                self.validate_native(declaration=bad)
        raw = parent.read_bytes()
        parent.write_bytes(raw + b"\n")
        with self.assertRaisesRegex(ValueError, "parent_report_source_mismatch"):
            self.validate_native()
        parent.write_bytes(raw)
        # A hash-updated changed report still must satisfy every source/native
        # guard, including exact output hash, source authority and OS exit.
        original = json.loads(raw)
        for mutate, reason in (
            (lambda r: r["world_snapshot"].update(source_sha256="0" * 64), "saved_world_producer_mismatch"),
            (lambda r: r["scenario_manifest"].pop("performance_lineage"), "parent_lineage_missing"),
            (lambda r: r["scenario_manifest"]["source"].update(sha256="0" * 64), "report_lineage_source_mismatch"),
            (lambda r: r.update(diagnostic_replay=True), "non_native_terminal"),
            (lambda r: r.update(run_id="foreign-parent-run"), "report_lineage_source_mismatch"),
            (lambda r: r["contract"].update(world="same-name-history"), "world_or_transport_owner_mismatch"),
            (lambda r: r["contract"].update(post_relaunch={"steps": [{"kind": "wait"}]}), "extra_native_relaunch_unbound"),
            (lambda r: r["startup"]["screen"]["runtime_binding_observed"].update(executable_sha256="0" * 64), "native_runtime_binding_mismatch"),
            (lambda r: r["startup"]["screen"].update(runtime_binding_status="mismatch"), "native_product_binding_unmatched"),
            (lambda r: r["startup"]["screen"]["runtime_binding_observed"].update(status="mismatch"), "native_product_binding_unmatched"),
            (lambda r: r["scenario_manifest"]["normalized"]["runtime_contract"]["value"].update(fixture="unrelated"), "report_contract_incompatible"),
            (lambda r: r["scenario_manifest"]["normalized"]["capabilities"]["value"].update({CAPABILITY: "counts_only"}), "report_contract_incompatible"),
            (lambda r: r["scenario_manifest"]["normalized"]["proof_route"]["value"].update(production_behavior=["start"]), "report_contract_incompatible"),
            (lambda r: r["scenario_manifest"]["source"].update(path=str(self.root / "other-scenario.json")), "parent_source_path_mismatch"),
            (lambda r: r["steps"][0]["cockpit_live_session"]["descriptor"].update(binding_id="foreign-native-binding"), "native_descriptor_binding_mismatch"),
            (lambda r: r["steps"][0]["cockpit_live_session"]["final"]["stop_detail"].pop("evidence_journal"), "one_native_run_required"),
            (lambda r: r["steps"][0]["cockpit_live_session"]["final"].update(native_save_completion={}), "native_persistence_unproved")):
            changed = copy.deepcopy(original)
            mutate(changed)
            if reason == "report_contract_incompatible":
                changed["scenario_manifest"]["source"]["sha256"] = "d" * 64
                authority = changed["wec_authority"]
                changed["wec_authority"] = startup_harness.seal_wec_authority(**{key: authority[key] for key in
                    ("evidence_class", "authority", "run_id", "binding_id", "owner")}, source_sha256="d" * 64)
            self.write(parent, changed)
            bad = copy.deepcopy(good)
            bad["evidence_contract"]["performance_witness"]["parent_report"]["sha256"] = hashlib.sha256(parent.read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, reason):
                self.validate_native(declaration=bad)
        parent.write_bytes(raw)
        runtime_path = parent.parent / "runtime.binding.json"
        runtime_bytes = runtime_path.read_bytes()
        runtime_path.unlink()
        with self.assertRaises(FileNotFoundError):
            self.validate_native()
        runtime_path.write_bytes(runtime_bytes)
        saved = parent.parent / "saved_world.json"
        saved_bytes = saved.read_bytes()
        saved.unlink()
        with self.assertRaises(FileNotFoundError):
            self.validate_native()
        saved.write_bytes(saved_bytes)
        # Change the actual producer's exit, not only its report summary.
        exit_path = self.root / "session" / "game-process-exit.json"
        exit_record = json.loads(exit_path.read_text())
        self.write(exit_path, {**exit_record, "exit_code": 1})
        with self.assertRaisesRegex(ValueError, "abnormal_exit"):
            self.validate_native()
        self.write(exit_path, exit_record)

    def test_byte_identical_unrelated_snapshot_cannot_borrow_a_parent(self):
        import shutil
        unrelated = startup_harness.userdir_for_profile("performance-test-run-" + "f" * 32) / "save" / "PerformanceTest"
        shutil.copytree(self.world, unrelated)
        self.assertEqual(startup_harness.sha256_tree(unrelated), startup_harness.sha256_tree(self.world))
        with self.assertRaisesRegex(ValueError, "parent_snapshot_path_mismatch"):
            prepare_saved_world_continuation(self.declaration, self.generator_run / "startup.result.json",
                saved_world_snapshot=unrelated, parent_report=self.report_path)
        bootstrap = prepare_saved_world_continuation(self.declaration, self.generator_run / "startup.result.json",
                                                     saved_world_snapshot=unrelated)
        with self.assertRaisesRegex(ValueError, "unattested_input_requires_bootstrap"):
            self.validate_native(declaration=bootstrap)

    def test_parent_path_and_run_cycles_are_rejected_before_any_work_credit(self):
        import scenario_registry_performance as performance_module
        from cockpit_report_reference import load_report
        self.reenter()
        self.reenter()
        good_load = load_report
        immediate = self.declaration["evidence_contract"]["performance_witness"]["parent_report"]
        calls = []
        def cycle_load(path):
            canonical, digest, report = good_load(path)
            calls.append(canonical)
            # Model a resolver returning a cyclic graph. Ordinary on-disk
            # mutations fail the stronger SHA check before this can occur.
            if len(calls) == 2:
                report = copy.deepcopy(report)
                report["scenario_manifest"]["performance_lineage"]["performance_witness"]["parent_report"] = copy.deepcopy(immediate)
            return canonical, digest, report
        with mock.patch("cockpit_report_reference.load_report", side_effect=cycle_load):
            with self.assertRaisesRegex(ValueError, "parent_report_cycle"):
                performance_module._validate_lineage(self.declaration)
        self.assertEqual(len(calls), 2)

    def test_wrong_initial_save_source_or_hash_and_unrelated_history_rejected(self):
        original = copy.deepcopy(self.declaration)
        bad = copy.deepcopy(original)
        bad["evidence_contract"]["performance_witness"]["initial_world"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "initial_save_hash_mismatch"):
            self.validate_native(declaration=bad)
        # Even byte-identical unrelated history cannot borrow the generator.
        import shutil
        historical = self.root / "historical" / "PerformanceTest"
        shutil.copytree(self.initial_world, historical)
        bad = copy.deepcopy(original)
        bad["saved_world_snapshot"] = str(historical)
        with self.assertRaisesRegex(ValueError, "generated_world_identity_mismatch"):
            self.validate_native(declaration=bad)
        for key in ("startup_sha256", "runtime_sha256"):
            bad = copy.deepcopy(original)
            bad["evidence_contract"]["performance_witness"]["initial_world"][key] = "0" * 64
            with self.assertRaisesRegex(ValueError, "generation_.*source_mismatch"):
                self.validate_native(declaration=bad)
        report = copy.deepcopy(self.report)
        report["startup"]["saved_world_snapshot"]["installed_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "initial_save_installation_mismatch"):
            self.validate_native(report=report)

    def test_wrong_generation_world_run_or_native_completion_rejected(self):
        path = self.generator_run / "startup.result.json"
        for mutate, reason in (
            (lambda value: value["generation_identity"].update(world="other-world"), "generated_world_identity_mismatch"),
            (lambda value: value["generation_identity"].update(run_id="other-run"), "generation_native_owner_mismatch"),
            (lambda value: value.update(exit_code=1), "native_generation_unproved"),
            (lambda value: value["readiness"].update(debug_error_evidence_count=1), "native_generation_unproved")):
            with self.subTest(reason=reason):
                startup = copy.deepcopy(self.generator_startup)
                mutate(startup)
                self.write(path, startup)
                declaration = copy.deepcopy(self.declaration)
                declaration["evidence_contract"]["performance_witness"]["initial_world"]["startup_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
                with self.assertRaisesRegex(ValueError, reason):
                    self.validate_native(declaration=declaration)
        self.write(path, self.generator_startup)
        report = copy.deepcopy(self.report)
        report["startup"]["profile"] = "unrelated-profile"
        with self.assertRaisesRegex(ValueError, "profile_owner_mismatch"):
            self.validate_native(report=report)

    def test_setup_run_or_other_product_cannot_supply_the_later_work_interval(self):
        path = self.generator_run / "runtime.binding.json"
        # A different origin product is valid; its identity is independent.
        runtime = {**self.runtime, "executable_sha256": "0" * 64}
        self.write(path, runtime)
        declaration = copy.deepcopy(self.declaration)
        declaration["evidence_contract"]["performance_witness"]["initial_world"]["runtime_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        report = copy.deepcopy(self.report)
        report["scenario_manifest"]["performance_lineage"]["performance_witness"] = copy.deepcopy(declaration["evidence_contract"]["performance_witness"])
        self.assertEqual(self.validate_native(declaration=declaration, report=report)["scope"], "observed_native_work_interval")
        self.write(path, {**self.runtime, "runtime_source_sha256": "c" * 64})
        startup = copy.deepcopy(self.generator_startup)
        startup["generation_identity"]["run_id"] = self.run_id
        self.write(self.generator_run / "startup.result.json", startup)
        owner = json.loads((self.generator_run / "terminal.owner.json").read_text())
        owner["run_id"] = self.run_id
        self.write(self.generator_run / "terminal.owner.json", owner)
        declaration = copy.deepcopy(self.declaration)
        declaration["evidence_contract"]["performance_witness"]["initial_world"]["startup_sha256"] = hashlib.sha256((self.generator_run / "startup.result.json").read_bytes()).hexdigest()
        with self.assertRaisesRegex(ValueError, "generation_source_or_run_mismatch"):
            self.validate_native(declaration=declaration)

    def test_selected_producer_consumer_and_cli_preserve_only_witnessed_interval(self):
        bridge = json.loads((self.session / "bridge.manifest.json").read_text())
        self.assertEqual(bridge["authorization_token_id"], self.run_id)
        self.assertEqual(bridge["binding_id"], self.binding_id)
        self.ingest()
        result = self.record()
        self.assertEqual(result["performance"]["scope"], "observed_native_work_interval")
        self.assertFalse(result["performance"]["whole_sequence_proven"])
        store.review_playtest_witness(self.connection, witness_id=result["witness_id"], decision="accept", rationale="test scoped acceptance")
        self.assertFalse(store.final_gate_eligibility(self.connection)["automated_certification"])
        visible = store.registry_status(self.connection)[0]["playtest_witnesses"]
        scoped = next(row for row in visible if row["witness_id"] == result["witness_id"])
        self.assertEqual(scoped["performance"], {"scope": "observed_native_work_interval", "whole_sequence_proven": False,
                                                "gameplay_credit": False})
        row = self.connection.execute("SELECT route_key, verification_id FROM verification_history").fetchone()
        self.assertEqual(store._playtest_witness_hard_proven_candidates(self.connection, manifest_id=self.manifest_id,
            route_key=row["route_key"], candidate_rows={row["verification_id"]: {}}, report_local_hard_proven={row["verification_id"]}), set())
        for name, value in (("journal", self.journal), ("statement", self.statement), ("charter", CHARTER)):
            self.write(self.root / (name + ".json"), value)
        output = io.StringIO()
        with redirect_stdout(output):
            code = scenario_registry_cli.main(["--registry", str(self.root / "registry.sqlite3"), "registry-record-witness",
                "--manifest-id", self.manifest_id, "--report-id", self.report_id, "--journal", str(self.root / "journal.json"),
                "--statement", str(self.root / "statement.json"), "--charter", str(self.root / "charter.json")])
        self.assertEqual(code, 0, output.getvalue())
        self.assertEqual(json.loads(output.getvalue())["result"]["witness_id"], result["witness_id"])

    def test_count_load_idle_partial_and_mismatched_sources_never_prove(self):
        self.ingest()
        original = copy.deepcopy(self.statement)
        cases = [({"behavior": []}, "native_sources_required"), ({"measurements": []}, "native_sources_required")]
        for delta, reason in cases:
            with self.subTest(delta=delta):
                self.statement = copy.deepcopy(original)
                self.statement["performance_evidence"].update(delta)
                with self.assertRaisesRegex(store.ScenarioRegistryStoreError, reason):
                    self.record()
        self.statement = copy.deepcopy(original)
        for delta in ({"transition": "active_sortie_dispatch"}, {"transition": "idle"}, {"outcome": "cadence_service"},
                      {"actor_ids": [999]}, {"game_minutes": 999}, {"run_id": "other"}):
            with self.subTest(behavior=delta):
                self.write_record(self.run / "transition.events.jsonl", {**self.behavior, **delta})
                self.statement["performance_evidence"]["behavior"] = [self.handle(self.run / "transition.events.jsonl")]
                with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "no_meaningful_native_actor_work"):
                    self.record()
        self.write_record(self.run / "transition.events.jsonl", self.behavior)
        self.statement = copy.deepcopy(original)
        self.write_record(self.session / "performance.jsonl", {**self.measurement, "game_time": {"source": "native_frame", "before_minutes": 100, "after_minutes": 100}})
        self.statement["performance_evidence"]["measurements"] = [self.handle(self.session / "performance.jsonl")]
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "measurement_not_native_work"):
            self.record()

    def test_changed_selected_source_cannot_borrow_bound_bridge_authority(self):
        self.ingest()
        self.write(self.path, {**self.declaration, "description": "changed after selection"})
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "declaration_binding_drift"):
            self.record()

    def test_unselected_and_historical_reports_cannot_borrow_fresh_authority(self):
        self.ingest(selected=False)
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "fresh_selected_report_required"):
            self.record()
        adapters = store.BindingAdapters(runtime=lambda expected: {"status": "compatible", "facts": self.runtime},
            fixture=lambda expected: {"status": "compatible", "facts": {"source_sha256": "f" * 64}},
            profile=lambda expected: {"status": "compatible", "facts": {"source_sha256": "b" * 64}})
        # A fresh token issued for the same bytes must not bless an old native run.
        another = store.execute_registry_query(self.connection, store.parse_registry_query_request({"requirements": [
            {"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}],
            "preferences": []}), witness_charter={**CHARTER, "current_uncertainty": "new attempt"}, drafts_root=self.root / "drafts")
        self.assertNotEqual(another.token_id, self.run_id)
        linked = store.ingest_token_linked_report_reference(self.connection, another.token_id, self.report_path, adapters=adapters)
        self.assertEqual(linked["status"], "ingested", linked)
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "fresh_selected_report_required"):
            self.record()

    def test_changed_native_save_metric_hash_birth_and_exit_rejected(self):
        self.ingest()
        self.write_record(self.session / "performance.jsonl", {**self.measurement, "record_id": "tampered"})
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "source_hash_or_length_mismatch"):
            self.record()
        self.write_record(self.session / "performance.jsonl", self.measurement)
        self.owner["process_generation"] = {**self.generation, "birth_identity": "windows-filetime:999"}
        self.write(self.session / "game-process.json", self.owner)
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "process_owner_mismatch"):
            self.record()
        self.owner["process_generation"] = self.generation
        self.write(self.session / "game-process.json", self.owner)
        self.dimension["overmapbuffer"]["migo_towers"].append("changed")
        self.save_dimension()
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "saved_world_drift"):
            self.record()

    def test_native_persistence_exit_and_producer_identity_negatives(self):
        self.assertEqual(self.validate_native()["scope"], "observed_native_work_interval")
        for mutate in (lambda final: final.update(state="active"),
                       lambda final: final.update(closure_kind="explicit_abort"),
                       lambda final: final["native_save_completion"].update(status="missing"),
                       lambda final: final["native_save_completion"]["completion"].update(save_succeeded=False),
                       lambda final: final["native_save_completion"]["completion"].update(request_id="other-save"),
                       lambda final: final["native_save_completion"]["completion"].update(world_name="old-fixture")):
            final = copy.deepcopy(self.final)
            mutate(final)
            with self.assertRaisesRegex(ValueError, "non_native_terminal|native_persistence_unproved"):
                self.validate_native(final=final)
        # .NET ticks and FILETIME of the same birth pass above; PID reuse and
        # a wrong birth in either representation fail independently.
        for mutate in (lambda record: record["owner"].update(process_identity="456"),
                       lambda record: record["owner"].update(process_identity=str(457 + 504911232000000000)),
                       lambda record: record["owner"].update(pid=124),
                       lambda record: record["owner"].update(run_id="other"),
                       lambda record: record["owner"].update(binding_id="other"),
                       lambda record: record["owner"]["source_binding"].update(runtime_binding_sha256="other")):
            record = copy.deepcopy(self.measurement)
            mutate(record)
            self.write_record(self.session / "performance.jsonl", record)
            statement = copy.deepcopy(self.statement)
            statement["performance_evidence"]["measurements"] = [self.handle(self.session / "performance.jsonl")]
            with self.assertRaisesRegex(ValueError, "measurement_birth_mismatch|measurement_not_native_work"):
                self.validate_native(statement=statement)
        self.write_record(self.session / "performance.jsonl", self.measurement)
        (self.root / "game.exe").write_bytes(b"replacement executable")
        with self.assertRaisesRegex(ValueError, "executable_bytes_drift"):
            self.validate_native()

    def test_bridge_authority_and_save_lifetime_are_checked_without_graphical_snapshot(self):
        report = copy.deepcopy(self.report)
        report.pop("world_snapshot")
        with self.assertRaisesRegex(ValueError, "saved_world_producer_mismatch"):
            self.validate_native(report=report)
        report = copy.deepcopy(self.report)
        for index, (token, binding) in enumerate(((None, self.binding_id),
                ("another-selection", self.binding_id), (self.run_id, "other"))):
            # Parse the real serve command and prepare its manifest, including
            # the absent-token case that the old synthetic fixture concealed.
            command = list(self.selected_bridge_command[2:])
            command[0] = "serve"
            session = self.root / ("negative-bridge-" + str(index))
            command[command.index("--session-dir") + 1] = str(session)
            command[command.index("--binding-id") + 1] = binding
            auth = command.index("--authorization-token-id")
            if token is None:
                del command[auth:auth + 2]
            else:
                command[auth + 1] = token
            with mock.patch.object(cockpit_file_bridge.FileBackedCockpitBridge, "serve",
                                   new=lambda bridge: bridge.prepare() or 0):
                self.assertEqual(cockpit_file_bridge.main(command), 0)
            self.write(self.session / "bridge.manifest.json", json.loads((session / "bridge.manifest.json").read_text()))
            with self.assertRaisesRegex(ValueError, "selected_bridge_authority_mismatch"):
                self.validate_native(report=report)
        self.write(self.session / "bridge.manifest.json", {"authorization_token_id": self.run_id, "binding_id": self.binding_id})
        for delta in ({"world": "other-world"}, {"playtest_mode": "tiles"}):
            self.write(self.session / "game-process.json", {**self.owner, **delta})
            with self.assertRaisesRegex(ValueError, "world_or_transport_owner_mismatch"):
                self.validate_native(report=report)
        self.write(self.session / "game-process.json", self.owner)
        path = self.world / "dimension_data.gsav"
        os.utime(path, (4102444800, 4102444800))  # synthetic 2100 timestamp after the recorded exit
        with self.assertRaisesRegex(ValueError, "save_outside_native_process_lifetime"):
            self.validate_native(report=report)

    def test_terminal_readiness_uses_supported_current_caller(self):
        with mock.patch.object(startup_harness.sys, "platform", "win32"), mock.patch.object(
            startup_harness, "validate_selected_product_build", return_value=({"build_configuration": {
                "renderer": "curses", "make_variables": ["SOUND=0"]}}, "")):
            readiness = startup_harness.terminal_mode_readiness(self.declaration, self.root / "game.exe")
        self.assertEqual(readiness["status"], "ready", readiness)
        self.assertTrue(self.declaration["capture_world_after"])
        self.assertEqual(self.report["world_snapshot"]["status"], "current_saved_world_reference")
        self.assertFalse(self.report["world_snapshot"]["retained_copy"])
        self.assertEqual(self.report["world_snapshot"]["source_sha256"], startup_harness.sha256_tree(self.world)[0])
        graphical = copy.deepcopy(self.declaration)
        graphical["runtime_contract"]["require_screen_observability"] = True
        with mock.patch.object(startup_harness.sys, "platform", "win32"):
            self.assertEqual(startup_harness.terminal_mode_readiness(graphical, self.root / "game.exe")["status"], "unsupported_terminal_mode")
            unrelated = copy.deepcopy(self.declaration)
            unrelated["evidence_contract"].pop("performance_witness")
            self.assertIn("graphical observability", startup_harness.terminal_mode_readiness(unrelated, self.root / "game.exe")["reason"])

    def lower_ceiling_through_live_producer(self, charter, *, authority=None, gameplay=False):
        """Exercise execute_probe_steps' actual ceiling choice, then the real journal producer."""
        self.write(self.run / startup_harness.TRANSITION_EVENT_BINDING_FILENAME, {"run_id": self.run_id})
        channels = []

        def factory(**kwargs):
            channel = cockpit.CockpitRunChannel(lambda: {}, lambda *args: {}, binding_id=self.binding_id,
                read_process_state=lambda: self.process, witness_charter=kwargs["witness_charter"],
                witness_identity=kwargs["witness_identity"], witness_evidence_ceiling=kwargs["witness_evidence_ceiling"])
            channel._run_id = self.run_id
            channel._transcript.extend(copy.deepcopy([event for event in self.final["action_observation_sequence"]
                                                      if event.get("kind") == "action"]))
            channels.append(channel)
            return cockpit.CockpitService(run_channel=channel)

        def serve(service, _input, _output):
            channel = service.run_channel
            observed = channel.observe_process_exit()
            sealed = channel.seal_witness_journal(observation_id=observed["observation_id"], stop_reason="done", unused_authority="none")
            self.assertTrue(sealed["ok"], sealed)
            self.journal = channel._sealed_journal
            citation = next(entry["citation_id"] for entry in self.journal["entries"] if entry["kind"] == "observation")
            initial = {key: value for key, value in self.statement.items() if key != "performance_evidence"}
            initial.update(verdict="inconclusive", recommended_disposition="continue", evidence_ceiling=channel._witness_evidence_ceiling,
                           citations=[{"citation_id": citation, "meaning": "bound exit", "checks": ["value.surface.facts"]}])
            self.live_statement = copy.deepcopy(initial)
            result = channel.finish(observation_id=observed["observation_id"], stop_reason="done", unused_authority="none", witness=initial)
            self.assertTrue(result["ok"], result)
            return 0

        with redirect_stdout(io.StringIO()), mock.patch.object(startup_harness, "semantic_step_trace_start_for_source", return_value=0), \
                mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=charter), \
                mock.patch.object(startup_harness, "open_cockpit_game_service", side_effect=factory), \
                mock.patch.object(startup_harness, "serve_cockpit_live", side_effect=serve), \
                mock.patch.object(startup_harness, "read_completed_cockpit_final", side_effect=lambda service, *args, **kwargs: service.run_channel._final_report):
            self.live_reports = startup_harness.execute_probe_steps(123, self.run, [{"kind": "cockpit_live_session", "label": "work"}],
                profile=self.report["contract"]["profile"], world="", scenario_identity=SCENARIO,
                runtime_binding=self.runtime, registry_authority=authority or {},
                grants_gameplay_proof=gameplay, semantic_only=True)
        self.final = channels[0]._final_report
        self.charter = channels[0]._witness_charter
        self.report["steps"] = [{"cockpit_live_session": {"final": self.final}}]
        self.write(self.report_path, self.report)
        return channels[0]._witness_evidence_ceiling

    def test_setup_authority_caps_focused_charter_in_live_descriptor_and_journal(self):
        for evidence_class, requested, gameplay, expected, credit in (
                ("setup support", "focused", True, "setup-only", False),
                ("setup support", "setup-only", False, "setup-only", False),
                ("setup support", "zero-credit", False, "zero-credit", False),
                ("focused feature proof", "focused", True, "focused", True)):
            with self.subTest(evidence_class=evidence_class, requested=requested):
                fixture = PerformanceRouteTest("test_setup_authority_caps_focused_charter_in_live_descriptor_and_journal")
                fixture.setUp()
                try:
                    ceiling = fixture.lower_ceiling_through_live_producer(
                        {**CHARTER, "requested_evidence_ceiling": requested},
                        authority=store.issue_wec_authority(
                            fixture.connection, evidence_class=evidence_class, authority="registry",
                            run_id=fixture.run_id, binding_id=fixture.binding_id, source_sha256="a" * 64),
                        gameplay=gameplay)
                    self.assertEqual(ceiling, expected)
                    self.assertEqual(fixture.journal["evidence_ceiling"], expected)
                    self.assertEqual(fixture.charter["requested_evidence_ceiling"], requested)
                    live = fixture.live_reports[0]
                    self.assertIs(live["cockpit_live_session"]["descriptor"]["gameplay_credit"], credit)
                    self.assertIs(live["metadata"]["gameplay_credit"], credit)
                    if expected in ("setup-only", "zero-credit"):
                        statement = {**fixture.live_statement, "evidence_ceiling": "focused"}
                        with self.assertRaisesRegex(playtest_witness.WitnessError, "witness_evidence_promotion_rejected"):
                            playtest_witness.validate_witness_statement(
                                charter=fixture.charter, journal=fixture.journal, statement=statement)
                finally:
                    fixture.tearDown()

    def test_lower_launch_authority_cannot_borrow_later_focused_collection(self):
        for ceiling in ("setup-only", "diagnostic"):
            with self.subTest(ceiling=ceiling):
                fixture = PerformanceRouteTest("test_lower_launch_authority_cannot_borrow_later_focused_collection")
                fixture.setUp()
                try:
                    # The standalone receipt is immutable too; it is not a
                    # required row for the independent detached launcher.
                    selected = store.execute_registry_query(fixture.connection, store.parse_registry_query_request({
                        "requirements": [{"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}],
                        "preferences": []}), drafts_root=fixture.root / "drafts")
                    with mock.patch.object(store, "_cockpit_evidence_effect", return_value=ceiling):
                        opening = store.open_cockpit_run(fixture.connection, selection_id=selected.selection_id,
                            owner_id="older-observation", executable=fixture.root / "game.exe")
                    self.assertEqual(fixture.lower_ceiling_through_live_producer({**CHARTER, "requested_evidence_ceiling": ceiling}), ceiling)
                    with mock.patch.object(store, "_cockpit_evidence_effect", return_value="focused"):
                        fixture.ingest()
                        self.assertEqual(store._run_evidence_ceiling(fixture.connection, fixture.manifest_id, fixture.declaration), "focused")
                        self.assertEqual(store.cockpit_run_authority_status(fixture.connection, run_id=opening["run_id"])["evidence_ceiling"], ceiling)
                        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "witness_evidence_promotion_rejected"):
                            fixture.record()
                    persisted = fixture.connection.execute("SELECT validation_json FROM playtest_witness_history").fetchone()[0]
                    self.assertEqual(json.loads(persisted)["performance"]["scope"], "none")
                finally:
                    fixture.doCleanups()

    def test_absent_live_charter_keeps_actual_false_gameplay_producer_diagnostic(self):
        self.assertEqual(store._run_evidence_ceiling(self.connection, self.manifest_id, self.declaration), "focused")
        self.assertEqual(self.lower_ceiling_through_live_producer(None), "diagnostic")
        self.ingest()
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "witness_evidence_promotion_rejected"):
            self.record()

    def test_witness_ceiling_hash_and_exact_identity_negatives(self):
        self.ingest()
        original = copy.deepcopy(self.statement)
        for ceiling in ("zero-credit", "setup-only", "diagnostic", "certification"):
            self.statement = copy.deepcopy(original)
            self.statement["evidence_ceiling"] = ceiling
            with self.assertRaises(store.ScenarioRegistryStoreError):
                self.record()
        self.statement = original
        for identity_key in ("scenario_id", "source_identity", "executable_identity", "run_id", "binding_id"):
            journal = copy.deepcopy(self.journal)
            journal["identity"][identity_key] = "other"
            with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "binding mismatched"):
                store.record_playtest_witness(self.connection, manifest_id=self.manifest_id, report_ids=[self.report_id],
                    charter=CHARTER, journal=journal, statement=self.statement)
        with mock.patch.object(store, "_cockpit_evidence_effect", return_value="diagnostic"):
            with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "collection_ceiling_rejects_proof"):
                self.record()
        self.report_path.write_text(self.report_path.read_text() + "\n", encoding="utf-8")
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "report identity is stale"):
            self.record()

    def test_declaration_ceiling_and_validation_do_not_flip_gameplay_allowlist(self):
        self.assertTrue(performance_contract(self.declaration))
        self.assertEqual(store._run_evidence_ceiling(self.connection, self.manifest_id, self.declaration), "focused")
        for state in ("setup-only", "diagnostic"):
            with mock.patch.object(store, "_cockpit_evidence_effect", return_value=state):
                self.assertEqual(store._run_evidence_ceiling(self.connection, self.manifest_id, self.declaration), state)
        old = copy.deepcopy(self.declaration)
        old["evidence_contract"].pop("performance_witness")
        self.assertEqual(store._run_evidence_ceiling(self.connection, self.manifest_id, old), "zero-credit")
        for key, value in (("name", "other.scenario"), ("capture_world_after", False), ("post_relaunch", [{"kind": "wait"}]), ("saved_world_snapshot", "old-save")):
            bad = copy.deepcopy(self.declaration)
            bad[key] = value
            self.write(self.path, bad)
            with self.assertRaises(scenario_registry.ManifestValidationError):
                scenario_registry.validate_manifest(bad, path=self.path)
            self.assertEqual(scenario_registry.lint_manifest(self.path)["status"], "invalid")
        bad = copy.deepcopy(self.declaration)
        bad["runtime_contract"]["grants_gameplay_proof"] = True
        self.write(self.path, bad)
        with self.assertRaises(scenario_registry.ManifestValidationError):
            scenario_registry.validate_manifest(bad, path=self.path)

    def test_fresh_authority_lifecycle_rechecks_executable_and_never_relabels_old_openings(self):
        execution = store.execute_registry_query(self.connection, store.parse_registry_query_request({
            "requirements": [{"key": CAPABILITY, "op": "eq", "value": "baseline_then_adaptive_active_party_expansion", "minimum_evidence": "declared"}],
            "preferences": []}), drafts_root=self.root / "drafts")
        self.assertIsNone(execution.token_id)
        opened = store.open_cockpit_run(self.connection, selection_id=execution.selection_id,
                                       owner_id="test-owner", executable=self.root / "game.exe")
        self.assertEqual(opened["evidence_ceiling"], "focused")
        self.assertFalse(opened["proof_promotion_authority"])
        with self.assertRaisesRegex(store.ScenarioRegistryStoreError, "already consumed"):
            store.open_cockpit_run(self.connection, selection_id=execution.selection_id,
                                  owner_id="test-owner", executable=self.root / "game.exe")
        (self.root / "game.exe").write_bytes(b"changed after authority opened")
        status = store.cockpit_run_authority_status(self.connection, run_id=opened["run_id"])
        self.assertEqual(status["state"], "invalidated")
        self.assertEqual(status["evidence_ceiling"], "zero-credit")
        self.assertEqual(status["terminal"]["reason"], "executable_binding_drift")
        self.assertEqual(self.connection.execute("SELECT evidence_ceiling FROM cockpit_run_authority").fetchone()[0], "focused")


if __name__ == "__main__":
    unittest.main()
