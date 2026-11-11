"""Selected diagnostic setup joins native detail and assessment without changing ordinary play."""
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import process_performance as perf
import startup_harness as harness


class DiagnosticSetupTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.config = {"schema": "caol-performance-turn-config-v1", "provenance": "test",
                       "window_turns": 4, "tail_percentile": .95, "spike_seconds": .1,
                       "slow_turn_seconds": .1, "sustained_turns": 2,
                       "incomplete_turn_seconds": 5., "stalled_observation_seconds": 5.,
                       "expected_progress": True}

    def test_omitted_native_option_is_selected_and_both_consumers_receive_config(self):
        scenario = {"saved_world_snapshot": "/owned/checkpoint", "performance_turn_config": self.config}
        overrides = harness.resolve_scenario_profile_option_overrides(scenario)
        effective = harness.continuation_profile_option_overrides(scenario, overrides)
        options = self.root / "options.json"
        options.write_text("[]")
        harness.apply_option_overrides_to_file(options, effective)
        perf.prepare_diagnostic_setup(self.root, self.config, effective)
        self.assertEqual(json.loads(options.read_text())[0]["value"], "normal")
        self.assertEqual(perf._turn_config(self.root), (self.config, None))
        self.assertEqual(perf.diagnostic_readiness(self.config, {})["native_cpu_stages"], "unavailable")
        before = (self.root / perf.TURN_CONFIG_NAME).read_bytes()
        perf.prepare_diagnostic_setup(self.root, self.config, effective)
        self.assertEqual((self.root / perf.TURN_CONFIG_NAME).read_bytes(), before)
        with self.assertRaisesRegex(ValueError, "configuration_differs"):
            perf.prepare_diagnostic_setup(self.root, dict(self.config, spike_seconds=.2), effective)

    def test_disabled_ordinary_mode_unchanged_conflicting_diagnostic_rejected(self):
        ordinary = {"profile_option_overrides": {"RAID_PERFORMANCE_DIAGNOSTIC": "off"}}
        self.assertEqual(harness.resolve_scenario_profile_option_overrides(ordinary), ordinary["profile_option_overrides"])
        for scenario in [dict(ordinary, performance_turn_config=self.config),
                         {"performance_turn_config": self.config,
                          "profile_option_overrides_by_platform": {"linux": {"RAID_PERFORMANCE_DIAGNOSTIC": "off"}}},
                         {"performance_turn_config": dict(self.config, spike_seconds=0)}]:
            with self.assertRaises(ValueError):
                harness.resolve_scenario_profile_option_overrides(scenario, "linux")
        self.assertFalse((self.root / perf.TURN_CONFIG_NAME).exists())
        selected = dict(ordinary, performance_turn_config=self.config,
                        profile_option_overrides_by_platform={"linux": {"RAID_PERFORMANCE_DIAGNOSTIC": "normal"}})
        self.assertEqual(harness.resolve_scenario_profile_option_overrides(selected, "linux")
                         ["RAID_PERFORMANCE_DIAGNOSTIC"], "normal")

    def test_reentry_missing_override_and_launched_session_are_not_mutated(self):
        with self.assertRaisesRegex(ValueError, "override_missing"):
            perf.prepare_diagnostic_setup(self.root, self.config, {})
        (self.root / "game-process.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "unlaunched"):
            perf.prepare_diagnostic_setup(self.root, self.config, {"RAID_PERFORMANCE_DIAGNOSTIC": "normal"})
        self.assertFalse((self.root / perf.TURN_CONFIG_NAME).exists())

    def test_observed_and_disabled_detail_are_distinct_from_assessment_configuration(self):
        snapshot = {"native_process_instance": "native-a", "individual_max": {
            "game_turn": 10, "process_cpu_seconds": .01, "stages_seconds": [.02],
            "metric_unavailable_reason": None}}
        retained = {"status": "retained", "snapshot": snapshot}
        self.assertEqual(perf.diagnostic_readiness(None, retained)["native_cpu_stages"], "observed")
        snapshot["individual_max"].update(process_cpu_seconds=None, stages_seconds=None,
                                          metric_unavailable_reason="native_diagnostic_mode_off")
        result = perf.diagnostic_readiness(self.config, retained)
        self.assertTrue(result["assessment_configured"])
        self.assertEqual(result["reason"], "native_diagnostic_mode_off")

    def test_public_probe_prepares_config_before_start_and_passes_native_override(self):
        args = SimpleNamespace(scenario="diagnostic.test", profile="dev-harness", world="",
                               fixture=None, replace_existing_worlds=False, advance_turns=None,
                               settle_seconds=None, artifact_pattern="", test_command="", dry_run=False)
        scenario = {"name": "diagnostic.test", "profile": "dev-harness", "steps": [],
                    "performance_turn_config": self.config}
        def launch_boundary(command, **kwargs):
            self.assertIn("RAID_PERFORMANCE_DIAGNOSTIC=normal", command)
            self.assertEqual(perf._turn_config(self.root), (self.config, None))
            raise RuntimeError("test stopped before native launch")
        with patch("startup_harness.load_scenario", return_value=scenario), \
                patch.dict("os.environ", {"OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(self.root)}), \
                patch("startup_harness.run_startup_in_process", side_effect=launch_boundary):
            with self.assertRaisesRegex(RuntimeError, "test stopped before native launch"):
                harness.run_probe_mode(args)


if __name__ == "__main__":
    unittest.main()
