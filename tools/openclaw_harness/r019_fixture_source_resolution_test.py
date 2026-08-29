#!/usr/bin/env python3
"""Focused source-identity controls for the R-019 safe-popup fixture."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness  # noqa: E402


class R019FixtureSourceResolutionTest(unittest.TestCase):
    fixture = "r019_keep_watch_safe_popup_v1"
    profile = "live-debug"

    def test_safe_popup_keeps_declared_identity_and_inherited_payload(self) -> None:
        resolved = startup_harness.resolve_fixture_payload(self.fixture, self.profile)
        binding = startup_harness.fixture_source_binding(self.fixture, self.profile)

        self.assertEqual((resolved["fixture"], resolved["fixture_profile"]), (self.fixture, self.profile))
        self.assertEqual((binding["resolved_fixture"], binding["resolved_fixture_profile"]),
                         (self.fixture, self.profile))
        self.assertEqual(resolved["payload_fixture"], "bandit_r002_m040_post_abort_recenter_return_v0_2026-08-22")
        self.assertEqual(
            [name for _profile, name in resolved["source_chain"]],
            [
                self.fixture,
                "r013_clean_wait_duration_v1",
                "r012_avatar_visible_bootstrap_v1",
                "bandit_r002_m040_post_abort_recenter_return_v0_2026-08-22",
            ],
        )
        scheduled = [item for item in resolved["save_transforms"]
                     if item["kind"] == "scheduled_global_eoc"]
        mutations = [item for item in resolved["save_transforms"]
                     if item["kind"] == "player_mutations"]
        self.assertEqual(scheduled, [{
            "kind": "scheduled_global_eoc",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "eoc": "EOC_OPENCLAW_R019_SAFE_POPUP",
            "offset_seconds": 238,
        }])
        self.assertIn({
            "kind": "player_mutations",
            "player_save": "#Wm9yYWlkYSBWaWNr.sav.zzip",
            "mutations": [
                "DEBUG_LS", "DEBUG_NOTEMP", "DEBUG_STAMINA", "DEBUG_CARDIO",
                "DEBUG_CLAIRVOYANCE", "DEBUG_NIGHTVISION",
            ],
        }, mutations)

    def test_install_fails_closed_if_resolver_leaks_a_payload_identity(self) -> None:
        leaked = {
            "fixture": "bandit_r002_m040_post_abort_recenter_return_v0_2026-08-22",
            "fixture_profile": self.profile,
        }
        with mock.patch.object(startup_harness, "resolve_fixture_payload", return_value=leaked):
            with self.assertRaisesRegex(SystemExit, "did not preserve the declared fixture identity"):
                startup_harness.install_fixture("r009-m095", self.fixture, replace=True,
                                                fixture_profile=self.profile)

    def test_scheduler_trace_is_run_bound_and_preserves_due_decision(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            profile_dir = Path(temporary)
            log_path = profile_dir / "debug.log"
            log_path.write_text(
                "openclaw_harness_scheduler_trace: component=global_eoc "
                "run_id=foreign eoc=EOC_OPENCLAW_R019_SAFE_POPUP due_turn=5286318 "
                "current_turn=5286318 decision=due outcome=consumed\n"
                "openclaw_harness_scheduler_trace: component=global_eoc "
                "run_id=current eoc=EOC_OPENCLAW_R019_SAFE_POPUP due_turn=5286318 "
                "current_turn=5286318 decision=due outcome=before_activation\n"
                "openclaw_harness_scheduler_trace: component=global_eoc "
                "run_id=current eoc=EOC_OPENCLAW_R019_SAFE_POPUP due_turn=5286318 "
                "current_turn=5286318 decision=due outcome=consumed\n",
                encoding="utf-8",
            )
            with mock.patch.object(startup_harness, "config_dir_for_profile", return_value=profile_dir):
                records = startup_harness.read_scheduler_trace("unused", "current")

        self.assertEqual(records, [
            {"eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 5286318,
             "current_turn": 5286318, "decision": "due", "outcome": "before_activation"},
            {"eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 5286318,
             "current_turn": 5286318, "decision": "due", "outcome": "consumed"},
        ])


if __name__ == "__main__":
    unittest.main()
