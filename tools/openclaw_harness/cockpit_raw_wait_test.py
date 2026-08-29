#!/usr/bin/env python3
"""Focused contract checks for the R-018 raw bounded wait route."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402


def frame(sequence: int, minutes: int, *, state: str = "world") -> dict[str, object]:
    return {
        "run_id": "raw-wait-proof",
        "frame_id": f"raw-wait-proof:{sequence}",
        "state": state,
        "observed_turn": sequence,
        "game_minutes": minutes,
        "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1",
            "avatar": {"name": "Ada"},
            "visible_local": [],
        },
        "valid_actions": ["world.wait"],
        "action_inputs": {"world.wait": "."},
        "keep_watch_safety": {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        },
    }


def bound(maximum: int = 2) -> dict[str, object]:
    return {
        "basis": "scheduler_boundary",
        "source": "native scheduler target minus current game minute",
        "unit": "game_minutes",
        "maximum": maximum,
        "progress_required": True,
    }


class RawWaitTest(unittest.TestCase):
    def service(self, frames: list[dict[str, object]]) -> tuple[cockpit.CockpitService, list[str]]:
        index = [0]
        dispatched: list[str] = []

        def dispatch(issuing: dict[str, object], action_id: str) -> dict[str, object]:
            dispatched.append(action_id)
            index[0] += 1
            return {
                "native_receipt": {
                    "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
                },
                "_next_frame": frames[index[0]],
            }

        channel = cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch, binding_id="binding-a",
            read_binding_id=lambda: "binding-a", enforce_continuation_bounds=True,
        )
        return cockpit.CockpitService(run_channel=channel), dispatched

    def request(self, *, target: int, maximum: int = 2, enabled: bool = True) -> dict[str, object]:
        return {"action": "game.raw_wait", "raw_wait": {
            "enabled": enabled, "target_game_minutes": target, "bound": bound(maximum),
            "recipe": ["world.wait", "wait.1m"],
        }}

    def test_raw_route_matches_primitive_chain_and_preserves_ordered_receipts(self) -> None:
        start = frame(1, 100)
        duration = frame(2, 100, state="wait_duration_choice")
        duration["valid_actions"] = ["wait.1m"]
        complete = frame(3, 101)
        raw, raw_actions = self.service([start, duration, complete])
        result = raw.call(self.request(target=101))

        self.assertTrue(result["ok"])
        self.assertEqual(raw_actions, ["world.wait", "wait.1m"])
        self.assertEqual(
            [receipt["native_receipt"]["action_id"] for receipt in result["result"]["native_receipts"]],
            raw_actions,
        )
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)

        primitive, primitive_actions = self.service([start, duration, complete])
        observed = primitive.call({"action": "game.observe"})["result"]
        for action in ("world.wait", "wait.1m"):
            self.assertTrue(primitive.call({
                "action": "run.continue", "observation_id": observed["observation_id"],
                "expected_signal": "game_minutes", "bound": bound(1),
            })["ok"])
            observed = primitive.call({
                "action": "game.act", "observation_id": observed["observation_id"],
                "action_id": action,
            })["observation"]
        self.assertEqual(primitive_actions, raw_actions)
        self.assertEqual(observed["game_minutes"], result["result"]["terminal_observation"]["game_minutes"])

    def test_raw_route_stops_on_native_prompt_without_recovery(self) -> None:
        prompt = frame(1, 100, state="semantic_ui")
        prompt["provenance"] = "native_semantic_ui_trace"
        prompt["valid_actions"] = ["modal.acknowledge"]
        service, dispatched = self.service([prompt])

        result = service.call(self.request(target=101))

        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "native_wait_interrupted")
        self.assertEqual(result["final"]["stop_detail"]["native_stop_reason"], "semantic_ui")
        self.assertEqual(result["final"]["stop_detail"]["guarded_handling_count"], 0)
        self.assertEqual(dispatched, [])

    def test_raw_route_stops_on_interpretive_activity_and_stale_frame(self) -> None:
        activity = frame(1, 100, state="activity_distraction")
        activity["provenance"] = "native_activity_distraction_query"
        activity["activity_type"] = "requires_player_interpretation"
        activity["observed_turn"] = None
        activity["valid_actions"] = ["activity.ignore"]
        service, dispatched = self.service([activity])
        interrupted = service.call(self.request(target=101))
        self.assertEqual(interrupted["error"], "native_wait_interrupted")
        self.assertEqual(
            interrupted["final"]["stop_detail"]["native_stop_reason"],
            "requires_player_interpretation",
        )
        self.assertEqual(dispatched, [])

        first = frame(1, 100)
        stale = frame(2, 100)
        reads = [0]

        def read_native_frame() -> dict[str, object]:
            reads[0] += 1
            return first if reads[0] == 1 else stale

        channel = cockpit.CockpitRunChannel(
            read_native_frame, lambda *_: self.fail("stale frame must not dispatch"),
            binding_id="binding-a", read_binding_id=lambda: "binding-a",
            enforce_continuation_bounds=True,
        )
        stale_result = cockpit.CockpitService(run_channel=channel).call(self.request(target=101))
        self.assertEqual(stale_result["error"], "raw_wait_stale_frame")

    def test_raw_route_stops_on_monster_and_exhausted_bound_with_partial_progress(self) -> None:
        monster = frame(1, 100)
        monster["keep_watch_safety"] = {
            "classification": "monster_spotted", "monster": True, "danger": False, "damage": False,
        }
        service, dispatched = self.service([monster])
        interrupted = service.call(self.request(target=101))
        self.assertEqual(interrupted["error"], "native_wait_interrupted")
        self.assertEqual(interrupted["final"]["stop_detail"]["native_stop_reason"], "monster_spotted")
        self.assertEqual(dispatched, [])

        service, dispatched = self.service([frame(1, 100), frame(2, 101)])
        exhausted = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(1), "recipe": ["world.wait"],
        }})
        self.assertEqual(exhausted["error"], "derived_bound_exhausted")
        self.assertEqual(exhausted["final"]["stop_detail"]["partial_progress"], 1.0)
        self.assertEqual(dispatched, ["world.wait"])

    def test_raw_route_stops_on_no_progress_after_preserving_native_receipt(self) -> None:
        service, dispatched = self.service([frame(1, 100), frame(2, 100)])
        result = service.call(self.request(target=101, maximum=1))
        self.assertEqual(result["error"], "proved_no_progress")
        self.assertEqual(dispatched, ["world.wait"])
        actions = [entry for entry in result["final"]["action_observation_sequence"]
                   if entry.get("kind") == "action"]
        self.assertEqual(actions[-1]["result"]["receipt"]["native_receipt"]["action_id"], "world.wait")

    def test_raw_route_off_switch_is_primitive_only(self) -> None:
        service, dispatched = self.service([frame(1, 100)])
        disabled = service.call(self.request(target=101, enabled=False))
        self.assertEqual(disabled["error"], "raw_wait_disabled_use_primitive_actions")
        self.assertEqual(dispatched, [])

    def test_finish_requires_the_raw_terminal_observation_and_retains_native_transcript(self) -> None:
        start = frame(1, 100)
        duration = frame(2, 100, state="wait_duration_choice")
        duration["valid_actions"] = ["wait.1m"]
        complete = frame(3, 101)
        service, _ = self.service([start, duration, complete])
        original = service.call({"action": "game.observe"})["result"]
        raw = service.call(self.request(target=101))
        terminal = raw["result"]["terminal_observation"]
        stale = service.call({
            "action": "run.finish", "observation_id": original["observation_id"],
            "stop_reason": "target_reached", "unused_authority": "none",
        })
        self.assertEqual(stale["error"], "unknown_or_stale_observation")
        finished = service.call({
            "action": "run.finish", "observation_id": terminal["observation_id"],
            "stop_reason": "target_reached", "unused_authority": "none",
        })
        self.assertTrue(finished["ok"])
        actions = [entry for entry in finished["result"]["action_observation_sequence"]
                   if entry.get("kind") == "action"]
        self.assertEqual([entry["action_id"] for entry in actions], ["world.wait", "wait.1m"])
        self.assertTrue(all(entry["result"]["receipt"]["native_receipt"]["accepted"]
                            for entry in actions))


if __name__ == "__main__":
    unittest.main()
