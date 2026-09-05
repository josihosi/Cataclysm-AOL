#!/usr/bin/env python3
"""Run-artifact isolation checks for the canonical startup launcher."""

from __future__ import annotations

import json
import tempfile
import sys
import unittest
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness


class StartupRunDirectoryTest(unittest.TestCase):
    def test_same_second_runs_receive_distinct_owned_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with mock.patch.object(startup_harness, "userdir_for_profile", return_value=root), \
                    mock.patch.object(startup_harness.time, "strftime", return_value="20260828_121922"):
                first = startup_harness.create_run_dir("r009-m095")
                second = startup_harness.create_run_dir("r009-m095")

        self.assertNotEqual(first, second)
        self.assertEqual(first.parent, second.parent)
        self.assertTrue(first.name.startswith("20260828_121922_"))
        self.assertTrue(second.name.startswith("20260828_121922_"))

    def test_explicit_quit_skips_declared_saved_world_reentry(self) -> None:
        terminal_step = {
            "cockpit_live_session": {
                "final": {
                    "schema": "caol-cockpit-live-final-v1",
                    "state": "finished",
                    "stop_detail": {
                        "explicit_player_quit": True,
                        "gameplay_credit": False,
                    },
                },
            },
        }
        self.assertTrue(startup_harness.cockpit_step_explicitly_quit(terminal_step))
        self.assertFalse(startup_harness.cockpit_step_explicitly_quit({
            "cockpit_live_session": {
                "final": {"state": "finished", "stop_detail": {}}
            }
        }))
        self.assertFalse(startup_harness.cockpit_step_explicitly_quit({
            "cockpit_live_session": {
                "final": {
                    "state": "finished",
                    "stop_detail": {"explicit_player_quit": True},
                }
            }
        }))

    def test_execute_probe_steps_stops_after_quit_before_following_step(self) -> None:
        """A terminal quit must leave the next ordinary step untouched."""
        class FakeChannel:
            _binding_id = "native-a"

            def __init__(self) -> None:
                self.calls = []

            def call(self, request):
                self.calls.append(request)
                if request["action"] == "game.observe":
                    return {
                        "ok": True,
                        "result": {
                            "observation_id": "sentinel-frame",
                            "advertised_actions": ["sentinel.action"],
                        },
                    }
                return {
                    "ok": True,
                    "observation": {"observation_id": "sentinel-next"},
                    "receipt": {"native_receipt": {
                        "accepted": True, "action_id": "sentinel.action",
                    }},
                }

        class FakeService:
            def __init__(self, channel) -> None:
                self.run_channel = channel

            def call(self, request):
                return self.run_channel.call(request)

        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=None), \
                mock.patch.object(startup_harness, "adaptive_semantic_session_identity", return_value=("session-a", "")), \
                mock.patch.object(startup_harness, "open_cockpit_game_service") as open_service, \
                mock.patch.object(startup_harness, "serve_cockpit_live", return_value=0):
            run_dir = Path(temporary)
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": "run-a"}), encoding="utf-8"
            )
            channel = FakeChannel()
            open_service.return_value = FakeService(channel)
            live_step = {"kind": "cockpit_live_session", "label": "quit"}
            following = {
                "kind": "cockpit_act", "label": "sentinel",
                "action_id": "sentinel.action",
            }
            final_path = startup_harness.cockpit_live_final_path(run_dir, "quit")
            final_path.write_text(json.dumps({
                "schema": "caol-cockpit-live-final-v1",
                "state": "finished",
                "stop_detail": {"explicit_player_quit": True},
            }), encoding="utf-8")
            reports = startup_harness.execute_probe_steps(
                1, run_dir, [live_step, following], profile="test", world="world",
            )
            self.assertEqual(len(reports), 1)
            self.assertTrue(reports[0]["stop_after_step"])
            self.assertEqual(channel.calls, [])

            final_path.write_text(json.dumps({
                "schema": "caol-cockpit-live-final-v1",
                "state": "finished",
                "stop_detail": {},
            }), encoding="utf-8")
            reports = startup_harness.execute_probe_steps(
                1, run_dir, [live_step, following], profile="test", world="world",
            )
            self.assertEqual(len(reports), 2)
            self.assertTrue(channel.calls)

    def test_interruption_action_uses_the_public_advertised_actions_field(self) -> None:
        observation = {
            "advertised_actions": ["activity.stop", "activity.ignore"],
        }

        self.assertTrue(
            startup_harness.cockpit_observation_advertises_action(
                observation, "activity.ignore",
            )
        )


if __name__ == "__main__":
    unittest.main()
