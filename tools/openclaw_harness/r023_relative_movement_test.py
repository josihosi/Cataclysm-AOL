#!/usr/bin/env python3
"""Focused contract checks for R-023 raw and guarded relative movement."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
from startup_harness import r023_relative_movement_step_verdict  # noqa: E402


ACTIONS = {
    "world.move.north": "k", "world.move.south": "j",
    "world.move.west": "h", "world.move.east": "l",
}


def frame(number: int, position: list[int], *, state: str = "world",
          entities: list[dict[str, object]] | None = None,
          damage: bool = False, safety_classification: str = "clear",
          recovery_action: str = "") -> dict[str, object]:
    actions = dict(ACTIONS)
    if recovery_action:
        actions[recovery_action] = "semantic-recovery"
    result = {
        "run_id": "r023-relative-proof", "frame_id": f"r023-relative-proof:{number}",
        "state": state, "observed_turn": number, "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1", "avatar": {"name": "Ada", "absolute_ms": position},
            "visible_local": [
                {"dx": 0, "dy": -1, "visibility": "clear", "terrain": "pavement"},
                {"dx": 0, "dy": 1, "visibility": "clear", "terrain": "pavement"},
                {"dx": -1, "dy": 0, "visibility": "clear", "terrain": "pavement"},
                {"dx": 1, "dy": 0, "visibility": "clear", "terrain": "pavement"},
            ],
            "visible_entities": entities or [],
        },
        "valid_actions": list(actions), "action_inputs": actions,
        "keep_watch_safety": {
            "classification": safety_classification,
            "monster": False, "danger": False, "damage": damage,
        },
    }
    if recovery_action:
        recovery = {"modal_id": f"modal-{number}"}
        result["keep_watch_safety"].update({
            "action_id": recovery_action,
            "recovery": recovery,
        })
        result["safe_recovery"] = {
            "modal_id": recovery["modal_id"],
            "actions": [recovery_action],
        }
    return result


def bound(maximum: int) -> dict[str, object]:
    return {"basis": "path_progress", "source": "signed offset Manhattan distance", "unit": "steps",
            "maximum": maximum}


class RelativeMovementTest(unittest.TestCase):
    def service(self, frames: list[dict[str, object]], *, outcomes: list[str] | None = None,
                binding: list[str] | None = None) -> tuple[cockpit.CockpitService, list[str], list[dict[str, object]]]:
        index = [0]
        dispatched: list[str] = []
        finals: list[dict[str, object]] = []
        current_binding = binding or ["binding-a"]
        outcomes = outcomes or ["moved"] * (len(frames) - 1)

        def dispatch(issuing: dict[str, object], action_id: str) -> dict[str, object]:
            dispatched.append(action_id)
            if action_id not in ACTIONS:
                index[0] += 1
                return {"native_receipt": {
                    "run_id": issuing["run_id"], "frame_id": issuing["frame_id"], "action_id": action_id,
                    "accepted": True, "outcome": "recovered_interruption",
                }, "_next_frame": frames[index[0]]}
            before = issuing["observation"]["avatar"]["absolute_ms"]
            delta = {
                "world.move.north": [0, -1, 0], "world.move.south": [0, 1, 0],
                "world.move.west": [-1, 0, 0], "world.move.east": [1, 0, 0],
            }[action_id]
            expected = [before[item] + delta[item] for item in range(3)]
            outcome = outcomes[index[0]]
            index[0] += 1
            after = expected if outcome == "moved" else before
            return {"native_receipt": {
                "run_id": issuing["run_id"], "frame_id": issuing["frame_id"], "action_id": action_id,
                "accepted": outcome == "moved", "outcome": outcome,
                "coordinate_space": "absolute_ms",
                "before_absolute_ms": before, "expected_absolute_ms": expected,
                "after_absolute_ms": after, "after_terrain": "t_pavement",
            }, "_next_frame": frames[index[0]]}

        channel = cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch, binding_id="binding-a",
            read_binding_id=lambda: current_binding[0], enforce_continuation_bounds=True,
            finalize_session=lambda report: finals.append(dict(report)) or {"cleanup": {"status": "terminated"}},
        )
        return cockpit.CockpitService(run_channel=channel), dispatched, finals

    def test_raw_signed_offset_has_ordered_receipts_terminal_position_partial_and_cleanup(self) -> None:
        service, dispatched, finals = self.service([
            frame(1, [10, 20, 0]), frame(2, [11, 20, 0]), frame(3, [12, 20, 0]),
            frame(4, [12, 21, 0]),
        ])
        result = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [2, 1], "bound": bound(3),
        }})
        self.assertTrue(result["ok"])
        self.assertEqual(dispatched, ["world.move.east", "world.move.east", "world.move.south"])
        self.assertEqual(
            [item["native_receipt"]["action_id"] for item in result["result"]["native_receipts"]], dispatched,
        )
        self.assertEqual(result["result"]["target_absolute_ms"], [12, 21, 0])
        self.assertEqual(result["result"]["terminal_absolute_ms"], [12, 21, 0])
        self.assertEqual(result["result"]["partial_progress"], 3)
        terminal = result["result"]["terminal_observation"]
        finished = service.call({"action": "run.finish", "observation_id": terminal["observation_id"],
                                 "stop_reason": "target_reached", "unused_authority": "none"})
        self.assertTrue(finished["ok"])
        self.assertEqual(finished["result"]["cleanup"]["status"], "terminated")
        self.assertEqual(finals[0]["stop_reason"], "target_reached")

    def test_relative_position_reads_avatar_from_current_world_surface(self) -> None:
        channel = cockpit.CockpitRunChannel(lambda: frame(1, [1, 1, 0]), lambda *_: {})
        observation_id = "r023-relative-proof:surface:2"
        channel._observations[observation_id] = {
            "issuing_frame": {
                "event": "surface_descriptor", "schema_version": 1,
                "run_id": "r023-relative-proof", "frame_id": observation_id,
                "surface_id": observation_id, "kind": "world", "breadcrumbs": ["World"],
                "payload": {"avatar": json.dumps({"name": "Ada", "absolute_ms": [12, 21, 0]})},
                "valid_actions": [],
            },
        }
        self.assertEqual(channel._relative_position({"observation_id": observation_id}), [12, 21, 0])

        blocked, actions, _ = self.service([frame(1, [10, 20, 0]), frame(2, [10, 20, 0])], outcomes=["blocked"])
        stopped = blocked.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [-1, 0], "bound": bound(1),
        }})
        self.assertEqual(stopped["error"], "raw_move_relative_blocked")
        self.assertEqual(actions, ["world.move.west"])
        self.assertEqual(stopped["result"]["partial_progress"], 0)

    def test_independent_route_authority_rejects_the_other_relative_operation(self) -> None:
        service, actions, _ = self.service([frame(1, [1, 1, 0])])
        service._allowed_live_operations = {"game.raw_move_relative"}
        result = service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
        }})
        self.assertEqual(result["error"], "operation_not_authorized_for_live_session")
        self.assertEqual(actions, [])

    def test_guarded_rechecks_and_rejects_stale_binding_unknown_blocked_bounds_and_switches(self) -> None:
        for guarded, reason in (
            (frame(1, [1, 1, 0], entities=[{"kind": "monster", "dx": 1, "dy": 0}]),
             "guarded_move_relative_creature"),
            (frame(1, [1, 1, 0], damage=True), "guarded_move_relative_damage"),
            (frame(1, [1, 1, 0], state="semantic_ui"), "guarded_move_relative_prompt_or_unknown_event"),
        ):
            service, actions, _ = self.service([guarded])
            result = service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
                "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
            }})
            self.assertEqual(result["error"], reason)
            self.assertEqual(actions, [])

        blocked, actions, _ = self.service([frame(1, [1, 1, 0]), frame(2, [1, 1, 0])], outcomes=["no_progress"])
        result = blocked.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
        }})
        self.assertEqual(result["error"], "guarded_move_relative_no_progress")
        self.assertEqual(actions, ["world.move.east"])

    def test_explicit_movement_modes_allow_agent_selected_fictional_danger(self) -> None:
        visible = [{"kind": "monster", "handle": "visible-1", "dx": 1, "dy": 0}]
        cautious, actions, _ = self.service([frame(1, [1, 1, 0], entities=visible)])
        stopped = cautious.call({"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
            "danger_handling": "handle_classified_non_dangerous",
        }})
        self.assertEqual(stopped["error"], "guarded_move_relative_creature")
        self.assertEqual(actions, [])

        permissive, actions, _ = self.service([
            frame(1, [1, 1, 0], entities=visible), frame(2, [2, 1, 0], entities=visible),
        ])
        moved = permissive.call({"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
            "danger_handling": "ignore_danger_and_interruptions",
        }})
        self.assertTrue(moved["ok"])
        self.assertEqual(actions, ["world.move.east"])
        self.assertEqual(moved["result"]["danger_handling"], "ignore_danger_and_interruptions")
        self.assertEqual(moved["result"]["handled_interruptions"][0]["decision"],
                         "continue_auto_walk")

    def test_classified_auto_walk_recovers_safe_prompt_but_stop_mode_does_not(self) -> None:
        prompt = frame(
            1, [1, 1, 0], state="semantic_ui",
            safety_classification="safe_prompt", recovery_action="ui.dismiss",
        )
        stopped, actions, _ = self.service([prompt])
        stop_result = stopped.call({"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
            "danger_handling": "stop_on_interruption",
        }})
        self.assertEqual(stop_result["error"], "raw_move_relative_interrupted")
        self.assertEqual(actions, [])

        classified, actions, _ = self.service([
            prompt,
            frame(2, [1, 1, 0]),
            frame(3, [2, 1, 0]),
        ], outcomes=["moved", "moved"])
        handled = classified.call({"action": "game.move_relative", "move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
            "danger_handling": "handle_classified_non_dangerous",
        }})
        self.assertTrue(handled["ok"])
        self.assertEqual(actions, ["ui.dismiss", "world.move.east"])
        self.assertEqual(
            handled["result"]["handled_interruptions"][0]["classification"],
            "safe_prompt",
        )

    def test_rejects_receipt_and_frame_coordinates_that_cannot_be_reconciled(self) -> None:
        packet = {
            "origin_absolute_ms": [3900, 876, 0], "target_absolute_ms": [3901, 876, 0],
            "terminal_absolute_ms": [3900, 876, 0], "partial_progress": 1, "planned_steps": 1,
            "derived_bound": bound(1), "native_receipts": [{
                "native_receipt": {
                    "outcome": "moved", "coordinate_space": "absolute_ms",
                    "before_absolute_ms": [60, 60, 0], "expected_absolute_ms": [61, 60, 0],
                    "after_absolute_ms": [61, 60, 0],
                },
                "next_frame": {"observation": {"avatar": {"absolute_ms": [3900, 876, 0]}}},
            }],
        }
        final = {
            "action_observation_sequence": [{"kind": "guarded_move_relative", "result": packet}],
            "stop_reason": "guarded_move_relative_receipt_mismatch", "stop_detail": packet,
            "cleanup": {"status": "deferred_to_scenario_terminalization"},
        }
        verdict, failures = r023_relative_movement_step_verdict(
            {"relative_movement_operation": "guarded_move_relative",
             "live_operations": ["game.guarded_move_relative"]}, final, {"gameplay_credit": False},
        )
        self.assertEqual(verdict, "yellow_step_r023_relative_movement_receipt_unbound")
        self.assertIn("r023_coordinate_space_receipt_frame_mismatch", failures)

    def test_accepts_matching_absolute_receipt_and_frame_displacement(self) -> None:
        packet = {
            "origin_absolute_ms": [3900, 876, 0], "target_absolute_ms": [3901, 876, 0],
            "terminal_absolute_ms": [3901, 876, 0], "partial_progress": 1, "planned_steps": 1,
            "derived_bound": bound(1), "native_receipts": [{
                "native_receipt": {
                    "outcome": "moved", "coordinate_space": "absolute_ms",
                    "before_absolute_ms": [3900, 876, 0], "expected_absolute_ms": [3901, 876, 0],
                    "after_absolute_ms": [3901, 876, 0],
                },
                "next_frame": {"observation": {"avatar": {"absolute_ms": [3901, 876, 0]}}},
            }],
        }
        final = {
            "action_observation_sequence": [{"kind": "guarded_move_relative", "result": packet}],
            "stop_reason": "target_reached", "stop_detail": packet,
            "cleanup": {"status": "deferred_to_scenario_terminalization"},
        }
        verdict, failures = r023_relative_movement_step_verdict(
            {"relative_movement_operation": "guarded_move_relative",
             "live_operations": ["game.guarded_move_relative"]}, final, {"gameplay_credit": False},
        )
        self.assertEqual(verdict, "green_step_r023_relative_movement_receipt")
        self.assertEqual(failures, [])

        service, actions, _ = self.service([frame(1, [1, 1, 0])])
        self.assertEqual(service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
            "enabled": True, "offset_ms": [2, 0], "bound": bound(1),
        }})["error"], "derived_bound_exhausted")
        self.assertEqual(actions, [])
        self.assertEqual(service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": False, "offset_ms": [1, 0], "bound": bound(1),
        }})["error"], "raw_move_relative_disabled_use_primitive_actions")

        binding = ["binding-a"]
        stale, actions, _ = self.service([frame(1, [1, 1, 0]), frame(2, [2, 1, 0])], binding=binding)
        original_dispatch = stale.run_channel._dispatch_advertised_action
        def drift_dispatch(issuing: dict[str, object], action_id: str) -> dict[str, object]:
            assert original_dispatch is not None
            result = original_dispatch(issuing, action_id)
            binding[0] = "binding-b"
            return result
        stale.run_channel._dispatch_advertised_action = drift_dispatch
        result = stale.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": bound(1),
        }})
        self.assertEqual(result["error"], "binding_drift")
        self.assertEqual(actions, ["world.move.east"])


if __name__ == "__main__":
    unittest.main()
