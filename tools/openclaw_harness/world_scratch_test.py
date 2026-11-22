"""Public scratch launch selection, copy custody, failure and concurrency tests."""

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import scenario_registry_cli as registry_cli
import startup_harness as harness
import world_scratch as scratch
from scenario_registry_store import ScenarioRegistryStoreError


class ScratchWorldTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.world = self.root / "My prepared world"
        self.world.mkdir()
        (self.world / "worldoptions.json").write_text("[]")
        (self.world / "#Character.sav").write_text("{}")
        (self.world / "dimension_data.gsav").write_text("{}")
        self.registry = self.root / "registry.sqlite"
        self.ready = {"status": "ready", "executable_path": str(self.root / "game"), "captured_head": "abc"}
        self.query = {"token_id": "token123", "next_action": {
            "kind": "launch_selected_scenario", "command": {"argv": ["registry-detached-launch", "token123"]}}}

    def binding(self):
        digest, _ = harness.sha256_tree(self.world)
        return SimpleNamespace(scenario=scratch.SCENARIO, source_path=str(scratch.DECLARATION),
                               source_sha256=registry_cli.path_sha256(scratch.DECLARATION), runtime_binding={},
                               outcome_brief={"outcome": "Explore this world", "scratch_world": {
                                   "attempt_id": "c" * 32, "source": str(self.world), "sha256": digest}})

    def test_public_preflight_requires_saved_player_and_does_not_create_registry(self):
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value=self.ready):
            result = scratch.launch_scratch(self.world, dry_run=True, registry=self.registry)
            self.assertEqual(result["status"], "preflight_passed")
            (self.world / "#Character.sav").unlink()
            blocked = scratch.launch_scratch(self.world, dry_run=True, registry=self.registry)
            self.assertEqual(blocked["status"], "blocked")
        self.assertFalse(self.registry.exists())

    def test_run_uses_fresh_immutable_snapshot_and_original_name(self):
        selection = self.binding()
        namespace = registry_cli._registry_launch_probe_namespace(selection, cockpit_live_session=True)
        self.assertEqual(namespace.world, self.world.name)
        self.assertEqual(namespace.saved_world_snapshot, str(self.world))
        self.assertTrue(namespace.disposable_saved_world)
        self.assertTrue(namespace.post_relaunch_continuation)
        self.assertEqual(namespace.expected_saved_world_sha256, harness.sha256_tree(self.world)[0])
        self.assertEqual(namespace.profile, "scratch-" + "c" * 16)
        for override in ({"profile_override": "another"}, {"saved_world_snapshot": str(self.root)}):
            with self.assertRaisesRegex(ScenarioRegistryStoreError, "differs"):
                registry_cli._registry_launch_probe_namespace(selection, **override)

    def test_mutated_source_rejected_before_copy_or_profile_creation(self):
        digest, _ = harness.sha256_tree(self.world)
        (self.world / "#Character.sav").write_text("changed")
        destination = self.root / "isolated-profile"
        with patch.object(harness, "save_dir_for_profile", return_value=destination):
            with self.assertRaisesRegex(SystemExit, "changed after selection"):
                harness.install_saved_world_snapshot("scratch", "World", self.world, expected_sha256=digest)
        self.assertFalse(destination.exists())

    def test_no_fallback_to_unbound_generic_launch(self):
        selection = self.binding()
        selection.outcome_brief = {}
        with self.assertRaisesRegex(ScenarioRegistryStoreError, "world scratch"):
            scratch.selected_scratch(selection)

    def test_blocked_build_appears_in_history_without_starting_native_process(self):
        blocked = {"status": "build_required", "reason": "compiled product changed", "next_action": "rebuild"}
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value=blocked), \
                patch.object(scratch, "_registry") as launch:
            result = scratch.launch_scratch(self.world, registry=self.registry, question="Test the stove")
        launch.assert_not_called()
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["next_action"], "rebuild")
        self.assertTrue(result["run_id"])

    def test_bridge_receipt_must_identify_an_actual_ready_native_session(self):
        self.assertEqual(scratch._wait_for_bridge(self.root, "b", timeout=0)["status"], "starting")
        status = self.root / "status.json"
        status.write_text(json.dumps({"state": "ready", "binding_id": "other"}))
        self.assertEqual(scratch._wait_for_bridge(self.root, "b", timeout=0)["status"], "blocked")
        status.write_text(json.dumps({"state": "process_dead", "binding_id": "b", "reason": "startup refused"}))
        self.assertEqual(scratch._wait_for_bridge(self.root, "b", timeout=0)["reason"], "startup refused")
        status.write_text(json.dumps({"state": "ready", "binding_id": "b", "session_descriptor": {"run_id": "native"}}))
        self.assertEqual(scratch._wait_for_bridge(self.root, "b", timeout=0)["native_run_id"], "native")

    def test_failed_default_build_discovers_a_compatible_installed_tiles_publication(self):
        with patch.object(registry_cli, "_current_source_executable_readiness", side_effect=[
                {"status": "build_required", "product_source_sha256": "current"}, self.ready]), \
                patch.object(scratch, "_installed_tiles_receipts", return_value=[
                    (self.root / "render.json", {"executable_path": "other", "product_source_sha256": "current"})]):
            result = scratch.launch_scratch(self.world, dry_run=True, registry=self.registry)
        self.assertEqual(result["status"], "preflight_passed")
        self.assertFalse(self.registry.exists())

    def test_stale_product_is_offered_by_explicit_receipt_only(self):
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value={
                "status": "build_required", "reason": "stale", "product_source_sha256": "current"}), \
                patch.object(scratch, "_installed_tiles_receipts", return_value=[
                    (self.root / "old.json", {"executable_path": "other", "captured_head": "old",
                                             "product_source_sha256": "old"})]):
            result = scratch.launch_scratch(self.world, dry_run=True, registry=self.registry)
        self.assertEqual(result["status"], "blocked")
        self.assertEqual(result["available_builds"][0]["captured_head"], "old")

    def test_source_and_session_survive_interrupt_during_detached_startup(self):
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value=self.ready), \
                patch.object(scratch, "_registry", side_effect=[
                    {}, {"result": self.query}, {"session_dir": str(self.root), "binding_id": "b"}]), \
                patch.object(scratch, "_wait_for_bridge", side_effect=KeyboardInterrupt):
            result = scratch.launch_scratch(self.world, registry=self.registry)
        self.assertEqual(result["status"], "starting")
        self.assertEqual(result["session"], str(self.root))
        self.assertTrue(result["run_id"])
        self.assertTrue(self.world.exists())

    def test_history_keeps_recipe_and_both_prepared_and_selected_hashes(self):
        from world_examples import history
        prepared = self.root / "prepared"
        (prepared / "save").mkdir(parents=True)
        source = self.world.rename(prepared / "save" / self.world.name)
        recipe = {"operations": []}
        (prepared / "preparation.json").write_text(json.dumps({
            "schema": "caol-world-preparation-v1", "source": "parent-fixture",
            "source_sha256": "f" * 64, "world_sha256": "a" * 64,
            "player_save": "#Character.sav", "world_dir": str(source), "recipe": recipe}))
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value=self.ready), \
                patch.object(scratch, "_registry", side_effect=ValueError("unavailable")):
            result = scratch.launch_scratch(source, registry=self.registry)
        record = history(registry=self.registry, search=result["run_id"])["runs"][0]
        self.assertEqual(record["recipe"], recipe)
        self.assertEqual(record["source_binding"], {
            "parent": "parent-fixture", "parent_sha256": "f" * 64, "prepared_sha256": "a" * 64,
            "world_sha256": harness.sha256_tree(source)[0], "player_save": "#Character.sav"})

    def test_parallel_scratch_attempts_have_unique_profiles_and_one_declaration(self):
        profiles = []
        def dispatch(command, registry):
            if command[0] == "rebuild":
                self.assertEqual(Path(command[-1]), scratch.DECLARATION)
                return {}
            if command[0] == "registry-query":
                brief = json.loads(Path(command[command.index("--coordinator-brief") + 1]).read_bytes())
                self.assertEqual(brief["outcome"], "Test the stove")
                self.assertEqual(brief["scratch_world"]["source"], str(self.world.resolve()))
                profiles.append(brief["scratch_world"]["attempt_id"])
                return {"result": self.query}
            return {"session_dir": str(self.root), "binding_id": "b"}
        with patch.object(registry_cli, "_current_source_executable_readiness", return_value=self.ready), \
                patch.object(scratch, "_registry", side_effect=dispatch), \
                patch.object(scratch, "_wait_for_bridge", return_value={"status": "launched", "native_run_id": "native"}):
            results = [scratch.launch_scratch(self.world, registry=self.registry, question="Test the stove") for _ in range(2)]
        self.assertEqual(len(set(profiles)), 2)
        self.assertNotEqual(results[0]["profile"], results[1]["profile"])
        self.assertNotEqual(results[0]["run_id"], results[1]["run_id"])

if __name__ == "__main__":
    unittest.main()
