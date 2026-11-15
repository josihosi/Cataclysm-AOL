#!/usr/bin/env python3
"""Public reference-camp launch preflight for an owned retained world."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import scenario_registry_cli as cli
import startup_harness


class ReferenceCampContinuationTest(unittest.TestCase):
    def test_public_detached_launch_preserves_exact_owned_world_and_rejects_substitution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original = root / "owner-reference" / "TestSetup00"
            original.mkdir(parents=True)
            (original / "master.gsav").write_bytes(b"owner initial")
            profile = "reference-camp-open-001-luna"
            world = root / "profiles" / profile / "save" / "TestSetup00"
            world.mkdir(parents=True)
            (world / "master.gsav").write_bytes(b"retained night")
            other = root / "other-world"
            other.mkdir()
            (other / "master.gsav").write_bytes(b"other")
            scene = {
                "name": "caol.reference_camp_open_focused_mcw", "profile": profile,
                "world": "TestSetup00", "saved_world_snapshot": str(original),
                "steps": [{"kind": "native_semantic_bootstrap"},
                          {"kind": "cockpit_live_session"}],
            }
            token = cli.RegistryLaunchToken("token", True, "current", scene["name"],
                                            str(root / "scene.json"), "source-hash")
            process = subprocess.CompletedProcess([], 0, stdout=json.dumps({"ok": True}), stderr="")
            save_root = lambda name: root / "profiles" / name / "save"
            with mock.patch.object(cli, "open_registry") as registry, \
                 mock.patch.object(cli, "reload_selection_token_for_launch", return_value=token), \
                 mock.patch.object(cli, "_load_selected_scenario", return_value=scene), \
                 mock.patch.object(cli, "_selected_executable", return_value=root / "game"), \
                 mock.patch.object(cli, "_selected_runtime_binding", return_value={"ok": True}), \
                 mock.patch.object(cli, "_current_source_executable_readiness", return_value={"status": "ready"}), \
                 mock.patch.object(cli, "_scenario_requires_bound_live_bridge", return_value=True), \
                 mock.patch.object(cli, "_declared_pre_descriptor_prefix", return_value=[]), \
                 mock.patch.object(cli, "_declared_live_session_reentries", return_value=0), \
                 mock.patch.object(startup_harness, "save_dir_for_profile", side_effect=save_root), \
                 mock.patch.object(cli.subprocess, "run", return_value=process) as bridge, \
                 mock.patch.object(cli, "_write_result") as result:
                def launch(snapshot, selected_profile, continuation=True):
                    args = ["--json", "--registry", str(root / "registry"), "registry-detached-launch",
                            "token", "--session-dir", str(root / "session"),
                            "--profile", selected_profile, "--saved-world-snapshot", str(snapshot)]
                    if continuation:
                        args.append("--post-relaunch-continuation")
                    return cli.main(args)

                for snapshot, selected_profile, continuation in (
                        (other, profile, True), (world, "other-profile", True),
                        (world, profile, False)):
                    self.assertEqual(launch(snapshot, selected_profile, continuation), 1)
                    bridge.assert_not_called()
                    self.assertIn("snapshot override differs", result.call_args.args[0]["error"])
                self.assertEqual((world / "master.gsav").read_bytes(), b"retained night")
                self.assertEqual((original / "master.gsav").read_bytes(), b"owner initial")
                self.assertEqual(launch(world, profile), 0)
                bridge.assert_called_once()
                command = bridge.call_args.args[0]
                self.assertIn("--post-relaunch-continuation", command)
                self.assertEqual(command[command.index("--saved-world-snapshot") + 1], str(world.resolve()))
                self.assertEqual(command[command.index("--profile") + 1], profile)
                self.assertEqual(result.call_args.args[0]["authority"],
                                 "technical run token remains unclaimed until the canonical child launch")
                registry.return_value.close.assert_called()

            with mock.patch.object(startup_harness, "save_dir_for_profile", side_effect=save_root):
                installed = startup_harness.install_saved_world_snapshot(profile, "TestSetup00", world)
            self.assertEqual(installed["status"], "green_saved_world_snapshot_reused")
            self.assertTrue(installed["fixture_reseed_skipped"])
            self.assertEqual((world / "master.gsav").read_bytes(), b"retained night")

    def test_initial_reference_remains_default(self):
        scene = {"name": "caol.reference_camp_open_focused_mcw", "profile": "owned",
                 "world": "TestSetup00", "saved_world_snapshot": "/owner/original"}
        with mock.patch.object(cli, "_load_selected_scenario", return_value=scene):
            self.assertEqual(cli._selected_saved_world_defaults(
                object(), post_relaunch_continuation=False, saved_world_snapshot="", profile_override=""),
                (True, "/owner/original", "owned"))


if __name__ == "__main__":
    unittest.main()
