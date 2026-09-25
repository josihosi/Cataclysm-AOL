"""Every harness profile receives credentials and the native setup controls."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import startup_harness as harness


class StartupEnvironmentTest(unittest.TestCase):
    def test_fresh_profile_inherits_config_before_overrides_without_mutating_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "configured" / "options.json"
            source.parent.mkdir()
            original = '[{"name":"LLM_INTENT_ENABLE","value":"false"}]'
            source.write_text(original)
            with mock.patch.object(harness, "config_dir_for_profile", side_effect=lambda p: root / p):
                harness.apply_profile_option_overrides(
                    "fresh", {"LLM_INTENT_ENABLE": "true"}, config_profile="configured")
                self.assertEqual(harness.load_game_options("fresh")["LLM_INTENT_ENABLE"], "true")
                harness.apply_profile_option_overrides("fresh", {}, config_profile="configured")
                self.assertEqual(harness.load_game_options("fresh")["LLM_INTENT_ENABLE"], "true")
            self.assertEqual(source.read_text(), original)

    def test_fresh_profile_without_overrides_gets_canonical_options(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(harness, "config_dir_for_profile", return_value=Path(directory)):
            harness.apply_profile_option_overrides("fresh", {})
            self.assertIn("LLM_INTENT_ENABLE", harness.load_game_options("fresh"))

    def test_mac_tool_resolution_without_homebrew_on_path(self):
        with mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness.shutil, "which", return_value=None), \
                mock.patch.object(Path, "is_file", return_value=True), \
                mock.patch.object(harness.os, "access", return_value=True), \
                mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(harness.peekaboo_binary(), "/opt/homebrew/bin/peekaboo")
            self.assertEqual(harness.harness_tool_binary("ffmpeg"), "/opt/homebrew/bin/ffmpeg")

    def test_peekaboo_explicit_override_and_path_take_precedence(self):
        with mock.patch.object(harness.shutil, "which", return_value="/custom/peekaboo"), \
                mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(harness.peekaboo_binary(), "/custom/peekaboo")
            os.environ[harness.PEEKABOO_BINARY_ENV] = "/configured/peekaboo"
            self.assertEqual(harness.peekaboo_binary(), "/configured/peekaboo")

    def test_nonexecutable_fallback_is_not_selected(self):
        with mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness.shutil, "which", return_value=None), \
                mock.patch.object(Path, "is_file", return_value=True), \
                mock.patch.object(harness.os, "access", return_value=False):
            self.assertIsNone(harness.harness_tool_binary("peekaboo"))

    def test_disabled_api_still_loads_key_and_controls_on_every_launch(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(harness, "config_dir_for_profile", return_value=Path(directory)), \
                mock.patch.object(harness, "load_game_options", return_value={}), \
                mock.patch.dict(os.environ, {"CATA_API_KEY": "test-secret"}, clear=True):
            for existing in (None, "[]"):
                target = Path(directory) / "keybindings.json"
                if existing is not None:
                    target.write_text(existing)
                environment = harness.game_child_environment("test")
                self.assertEqual(environment["CATA_API_KEY"], "test-secret")
                bindings = json.loads(target.read_text())
                debug = next(row for row in bindings if row["id"] == "debug")
                self.assertTrue(any(row["key"] == ["}"] for row in debug["bindings"]))

    def test_nonlogin_mac_recovers_key_without_returning_startup_noise(self):
        with mock.patch.dict(os.environ, {}, clear=True), \
                mock.patch.object(harness.sys, "platform", "darwin"), \
                mock.patch.object(harness, "read_secure_llm_credential", return_value=("", "macos_keychain")), \
                mock.patch.object(harness.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "startup noise\0test-secret\n", "")):
            report, environment = harness.provision_llm_api_key_environment({})
        self.assertEqual(environment, {"CATA_API_KEY": "test-secret"})
        self.assertNotIn("test-secret", json.dumps(report))
        self.assertEqual(report["source"], "login_environment")

    def test_missing_key_fails_before_controls_are_modified(self):
        with mock.patch.object(harness, "load_game_options", return_value={}), \
                mock.patch.object(harness, "provision_llm_api_key_environment", return_value=({"status": "missing", "env_var": "CATA_API_KEY", "source": ""}, {})), \
                mock.patch.object(harness, "config_dir_for_profile") as config:
            with self.assertRaisesRegex(RuntimeError, "Harness API credential is unavailable"):
                harness.game_child_environment("test")
            config.assert_not_called()


if __name__ == "__main__":
    unittest.main()
