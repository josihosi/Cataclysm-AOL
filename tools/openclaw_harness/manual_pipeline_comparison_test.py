import argparse
import contextlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import manual_pipeline_comparison as comparison


class ManualComparisonTest(unittest.TestCase):
    def test_owned_arms_keep_same_inputs_and_change_only_semantic_transport(self):
        with tempfile.TemporaryDirectory() as raw:
            base = Path(raw)
            executable, receipt = base / "game", base / "receipt"
            executable.write_text("game")
            receipt.write_text(json.dumps({"executable_sha256": "exe", "product_source_sha256": "source"}))
            world, config = base / "world", base / "config"
            world.mkdir()
            config.mkdir()
            (config / "options.json").write_text("[]")
            receipt_hash = comparison.harness.sha256_file(receipt)[0]
            world_hash = comparison.harness.sha256_tree(world)[0]
            launches = []
            for mode in ("semantic-on", "semantic-off"):
                profile_root = base / mode
                args = argparse.Namespace(profile=mode, world="World", executable=str(executable),
                    receipt=str(receipt), receipt_sha256=receipt_hash, saved_world=str(world),
                    saved_world_sha256=world_hash, config_source=str(config), mode=mode)
                process = mock.Mock(pid=7)
                process.wait.return_value = 0
                def launch(*_args, **kwargs):
                    launches.append(kwargs)
                    return process
                with mock.patch.object(comparison.harness, "userdir_for_profile", return_value=profile_root), \
                     mock.patch.object(comparison.harness, "config_dir_for_profile", return_value=profile_root / "config"), \
                     mock.patch.object(comparison.harness, "build_runtime_binding", return_value={"ok": True}), \
                     mock.patch.object(comparison.harness, "game_child_environment", return_value={"PATH": "x", "OPENCLAW_HARNESS_RUN_ID": "stale"}), \
                     mock.patch.object(comparison.harness, "install_saved_world_snapshot", return_value={"status": "installed"}) as install, \
                     mock.patch.object(comparison.harness, "launch_game", side_effect=launch), \
                     mock.patch.object(comparison, "own_writable_root", return_value=contextlib.nullcontext()):
                    self.assertEqual(comparison.run(args), 0)
                    self.assertTrue(install.call_args.kwargs["require_fresh"])
                    self.assertEqual(install.call_args.kwargs["disposable_run_id"], launches[-1]["transition_event_run_id"])
                self.assertFalse(json.loads(next((profile_root / "harness_runs").glob("*/manual-comparison.json")).read_text())["native_action_or_turn_proof"])
            self.assertTrue(launches[0]["diagnostic_semantic_transport"])
            self.assertFalse(launches[1]["diagnostic_semantic_transport"])
            self.assertEqual(launches[0]["child_environment"], launches[1]["child_environment"])
            self.assertNotIn("OPENCLAW_HARNESS_RUN_ID", launches[0]["child_environment"])

    def test_existing_profile_and_changed_snapshot_are_rejected(self):
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            args = argparse.Namespace(profile="existing")
            with mock.patch.object(comparison.harness, "userdir_for_profile", return_value=root):
                with self.assertRaisesRegex(ValueError, "fresh"):
                    comparison.run(args)
            executable, receipt = root / "game", root / "receipt"
            executable.write_text("game")
            receipt.write_text(json.dumps({"executable_sha256": "exe", "product_source_sha256": "source"}))
            world = root / "world"
            world.mkdir()
            args = argparse.Namespace(profile="new", executable=str(executable), receipt=str(receipt),
                receipt_sha256=comparison.harness.sha256_file(receipt)[0], saved_world=str(world),
                saved_world_sha256="wrong")
            with mock.patch.object(comparison.harness, "userdir_for_profile", return_value=root / "new"), \
                 mock.patch.object(comparison.harness, "build_runtime_binding", return_value={"ok": True}), \
                 mock.patch.object(comparison, "own_writable_root") as owner:
                with self.assertRaisesRegex(ValueError, "saved world hash mismatch"):
                    comparison.run(args)
                owner.assert_not_called()


if __name__ == "__main__":
    unittest.main()
