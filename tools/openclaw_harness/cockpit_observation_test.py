#!/usr/bin/env python3
"""Focused proof for native avatar observation and run-scoped opaque handles."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
import startup_harness  # noqa: E402


def native_frame(marker: int, terrain_id: str = "t_wall", terrain: str = "wall") -> dict[str, object]:
    return {
        "run_id": "r012-proof",
        "frame_id": f"r012-proof:{marker}",
        "state": "world",
        "observed_turn": marker,
        "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1",
            "avatar": {"name": "Ada"},
            "visible_local": [
                {"identity": {"dx": 0, "dy": 0, "terrain": "t_floor"}, "terrain": "floor"},
                {"identity": {"dx": 1, "dy": 0, "terrain": terrain_id}, "terrain": terrain},
            ],
        },
        "valid_actions": ["world.wait"],
        "action_inputs": {"world.wait": "."},
        "ocr": "forbidden screen text",
        "logs": "forbidden logs",
        "ecology": {"offscreen": "forbidden global state"},
    }


def activity_interruption_frame(marker: int) -> dict[str, object]:
    return {
        "run_id": "r012-proof",
        "frame_id": f"r012-proof:activity:{marker}",
        "state": "activity_distraction",
        "observed_turn": None,
        "provenance": "native_activity_distraction_query",
        "valid_actions": ["activity.ignore"],
        "action_inputs": {"activity.ignore": "I"},
    }


def observed_activity_interruption_frame(marker: int) -> dict[str, object]:
    frame = native_frame(marker)
    frame.update({
        "state": "activity_distraction",
        "producer": "activity_distraction_query",
        "provenance": "native_activity_distraction_query",
        "activity_type": "hostile_spotted_near",
        "valid_actions": ["activity.stop", "activity.continue", "activity.manage", "activity.ignore"],
        "action_inputs": {
            "activity.stop": "Y", "activity.continue": "N",
            "activity.manage": "M", "activity.ignore": "I",
        },
    })
    frame["observation"]["visible_entities"] = [{
        "identity": {"kind": "monster", "id": "process:dog-1"},
        "kind": "monster", "name": "the zombie dog", "attitude": "hostile",
        "dx": 4, "dy": 6, "fixture_actor_id": "r019-zombie-dog-positive-progress-v1",
        "typeid": "mon_zombie_dog", "faction": "zombie", "friendly": 0,
        "aggro_character": True,
    }]
    return frame


class CockpitObservationTest(unittest.TestCase):
    def test_causal_boundary_revokes_action_before_native_dispatch(self) -> None:
        dispatched: list[str] = []
        frame = native_frame(1)
        channel = cockpit.CockpitRunChannel(
            lambda: frame,
            dispatch_advertised_action=lambda _frame, action: dispatched.append(action),
            causal_boundary_precondition=lambda: {
                "status": "matched", "gate_id": "local_owner",
                "first_matching_event": {"sequence": 7},
            },
        )
        observation = channel.observe()
        result = channel.act(
            observation_id=str(observation["observation_id"]), action_id="world.wait",
        )
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "causal_boundary_reached")
        self.assertEqual(result["next_action"], "run.finish")
        self.assertEqual(dispatched, [])

    def test_executable_cockpit_step_issues_one_public_semantic_action(self) -> None:
        calls: list[dict[str, object]] = []

        class PublicGameService:
            def call(self, request: dict[str, object]) -> dict[str, object]:
                calls.append(request)
                if request["action"] == "game.observe":
                    return {"ok": True, "result": {"observation_id": "r013-live:1"}}
                return {
                    "ok": True,
                    "receipt": {"native_receipt": {
                        "frame_id": "r013-live:1", "action_id": "world.wait", "accepted": True,
                    }},
                    "observation": {"observation_id": "r013-live:2"},
                }

        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": "r013-live"}), encoding="utf-8",
            )
            with patch.object(startup_harness, "open_cockpit_game_service", return_value=PublicGameService()):
                reports = startup_harness.execute_probe_steps(
                    42, run_dir,
                    [{"kind": "cockpit_act", "label": "r013_wait", "action_id": "world.wait"}],
                    profile="dev-harness", world="McWilliams",
                )

            transaction = json.loads((run_dir / "r013_wait.game_act.json").read_text(encoding="utf-8"))

        self.assertEqual(calls, [
            {"action": "game.observe"},
            {"action": "game.act", "observation_id": "r013-live:1", "action_id": "world.wait"},
        ])
        self.assertEqual(reports[0]["semantic_action_count"], 1)
        self.assertTrue(transaction["outcome"]["ok"])

    def test_cockpit_action_chain_uses_fresh_observations_and_fails_closed(self) -> None:
        calls: list[dict[str, object]] = []

        class PublicGameService:
            def call(self, request: dict[str, object]) -> dict[str, object]:
                calls.append(request)
                if request["action"] == "game.observe":
                    return {"ok": True, "result": {"observation_id": f"r013-live:{len(calls)}"}}
                action_id = str(request["action_id"])
                return {"ok": True, "receipt": {"native_receipt": {
                    "frame_id": request["observation_id"], "action_id": action_id, "accepted": True,
                }}, "observation": {"observation_id": f"next:{action_id}"}}

        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": "r013-live"}), encoding="utf-8",
            )
            with patch.object(startup_harness, "open_cockpit_game_service", return_value=PublicGameService()):
                reports = startup_harness.execute_probe_steps(
                    42, run_dir, [{"kind": "cockpit_act", "label": "r013_chain", "action_chain": [
                        "world.wait", "wait.duration_menu", "wait.1m", "activity.ignore",
                    ]}], profile="dev-harness", world="McWilliams",
                )

        self.assertTrue(reports[0]["cockpit_act"]["ok"])
        self.assertEqual(reports[0]["semantic_action_count"], 4)
        self.assertEqual(reports[0]["metadata"]["observation_ids"], [
            "r013-live:1", "r013-live:3", "r013-live:5", "r013-live:7",
        ])
        self.assertEqual([call["action_id"] for call in calls if call["action"] == "game.act"], [
            "world.wait", "wait.duration_menu", "wait.1m", "activity.ignore",
        ])

        class StaleGameService:
            def call(self, request: dict[str, object]) -> dict[str, object]:
                if request["action"] == "game.observe":
                    return {"ok": True, "result": {"observation_id": "r013-live:stale"}}
                return {"ok": True, "receipt": {"native_receipt": {
                    "frame_id": request["observation_id"], "action_id": request["action_id"],
                    "accepted": True,
                }}}

        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": "r013-live"}), encoding="utf-8",
            )
            with patch.object(startup_harness, "open_cockpit_game_service", return_value=StaleGameService()):
                reports = startup_harness.execute_probe_steps(
                    42, run_dir, [{"kind": "cockpit_act", "label": "r013_stale", "action_chain": [
                        "world.wait", "activity.ignore",
                    ]}], profile="dev-harness", world="McWilliams",
                )

        self.assertEqual(reports[0]["cockpit_act"]["transactions"][1]["outcome"]["error"],
                         "fresh_authorized_observation_unavailable")
        self.assertEqual(reports[0]["abort"]["status"], "blocked_r013_native_transaction_rejected")

    def test_expected_interruption_wait_requires_current_native_identity(self) -> None:
        class PublicGameService:
            observations = iter([
                {"observation_id": "r013-live:1", "advertised_actions": ["world.wait"]},
                {"observation_id": "r013-live:2", "advertised_actions": []},
                {"observation_id": "r013-live:activity:3", "advertised_actions": ["activity.ignore"],
                 "active_interruption": {"id": "r013-live:activity:3", "type": "hostile_spotted_far",
                                         "owner": "native_activity_distraction_query"}},
            ])

            def call(self, request: dict[str, object]) -> dict[str, object]:
                if request["action"] == "game.observe":
                    return {"ok": True, "result": next(self.observations)}
                return {"ok": True, "receipt": {"native_receipt": {
                    "frame_id": request["observation_id"], "action_id": request["action_id"], "accepted": True,
                }}, "observation": {"observation_id": "r013-live:2", "advertised_actions": []}}

        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": "r013-live"}), encoding="utf-8",
            )
            with patch.object(startup_harness, "open_cockpit_game_service", return_value=PublicGameService()):
                reports = startup_harness.execute_probe_steps(
                    42, run_dir, [{"kind": "cockpit_act", "label": "r013_interrupt", "action_id": "world.wait",
                                   "expected_final_action": "activity.ignore", "transition_timeout_seconds": 0.1,
                                   "observe_interval_seconds": 0.0}], profile="dev-harness", world="McWilliams",
                )

        self.assertTrue(reports[0]["cockpit_act"]["ok"])
        self.assertEqual(reports[0]["cockpit_act"]["interruption"], {
            "id": "r013-live:activity:3", "type": "hostile_spotted_far",
            "owner": "native_activity_distraction_query",
        })

    def test_expected_interruption_wait_fails_closed_without_identity(self) -> None:
        service = type("Service", (), {"call": lambda self, request: {
            "ok": True, "result": {"observation_id": "r013-live:activity:3",
            "advertised_actions": ["activity.ignore"]}}})()
        result = startup_harness.await_cockpit_advertised_action(
            service, action_id="activity.ignore", prior_observation_id="r013-live:2",
            timeout_seconds=0.01, observe_interval_seconds=0.0,
        )
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "advertised_action_has_no_current_native_interruption_identity")

    def test_native_visible_facts_handles_staleness_and_fresh_recovery(self) -> None:
        frames = [native_frame(10), native_frame(11), native_frame(12, "t_door_c", "closed door")]
        index = [0]
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: frames[index[0]]))

        initial = service.call({"action": "game.observe"})
        self.assertTrue(initial["ok"])
        observed = initial["result"]
        self.assertEqual(observed["observation_id"], "r012-proof:10")
        self.assertEqual(observed["avatar"], {"name": "Ada"})
        wall = next(fact for fact in observed["visible_local"] if fact["terrain"] == "wall")
        self.assertTrue(wall["handle"].startswith("visible:r012-proof:"))
        self.assertNotIn("forbidden", json.dumps(observed))
        self.assertNotIn("offscreen", json.dumps(observed))

        index[0] = 1
        marker_changed = service.call({"action": "game.observe"})
        stable_wall = next(fact for fact in marker_changed["result"]["visible_local"] if fact["terrain"] == "wall")
        self.assertEqual(stable_wall["handle"], wall["handle"])
        self.assertTrue(service.call({"action": "game.look", "handle": wall["handle"]})["ok"])

        index[0] = 2
        stale = service.call({"action": "game.look", "handle": wall["handle"]})
        self.assertEqual(stale, {"ok": False, "error": "stale_visible_handle"})
        fresh = service.call({"action": "game.observe"})
        self.assertTrue(fresh["ok"])
        self.assertEqual(next(fact for fact in fresh["result"]["visible_local"]
                              if fact["terrain"] == "closed door")["terrain"], "closed door")

    def test_non_native_or_missing_observation_is_rejected(self) -> None:
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: {"run_id": "r012-proof"}))
        self.assertEqual(service.call({"action": "game.observe"}), {
            "ok": False, "error": "current native avatar observation is unavailable",
        })

    def test_native_activity_interruption_keeps_current_visible_entity(self) -> None:
        frames = [observed_activity_interruption_frame(80)]
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: frames[0]))

        result = service.call({"action": "game.observe"})

        self.assertTrue(result["ok"])
        observed = result["result"]
        self.assertEqual(observed["observed_turn"], 80)
        self.assertEqual(observed["active_interruption"]["type"], "hostile_spotted_near")
        self.assertEqual(observed["visible_entities"][0]["fixture_actor_id"],
                         "r019-zombie-dog-positive-progress-v1")
        handle = observed["visible_entities"][0]["handle"]

        self.assertEqual(service.call({"action": "game.observe"})["result"]
                         ["visible_entities"][0]["handle"], handle)

        frames[0] = observed_activity_interruption_frame(81)
        frames[0]["observation"]["visible_entities"][0]["identity"]["id"] = "process:dog-2"
        self.assertNotEqual(service.call({"action": "game.observe"})["result"]
                            ["visible_entities"][0]["handle"], handle)

    def test_observed_activity_rejects_a_stale_or_wrong_turn_frame(self) -> None:
        frame = observed_activity_interruption_frame(90)
        frame["observed_turn"] = None
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: frame))

        self.assertEqual(service.call({"action": "game.observe"}), {
            "ok": False, "error": "current native avatar observation is unavailable",
        })

    def test_act_uses_one_fresh_observation_and_returns_native_proof(self) -> None:
        frames = [native_frame(30), native_frame(31)]
        index = [0]
        dispatched: list[tuple[str, str]] = []

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            dispatched.append((str(frame["frame_id"]), action_id))
            index[0] = 1
            return {
                "accepted": True,
                "native_receipt": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "semantic_response": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "next_frame": frames[1],
                "_next_frame": frames[1],
            }

        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch,
        ))
        observed = service.call({"action": "game.observe"})["result"]
        acted = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertTrue(acted["ok"])
        self.assertEqual(dispatched, [("r012-proof:30", "world.wait")])
        self.assertEqual(acted["receipt"]["native_receipt"]["accepted"], True)
        self.assertEqual(acted["observation"]["observation_id"], "r012-proof:31")
        duplicate = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(duplicate["error"], "duplicate_submission")

    def test_act_rejects_stale_unadvertised_and_unauthorized_recovery_without_dispatch(self) -> None:
        frames = [native_frame(40), native_frame(41)]
        index = [0]
        dispatched: list[str] = []
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
            lambda: frames[index[0]], lambda _frame, action: dispatched.append(action) or {},
        ))
        observed = service.call({"action": "game.observe"})["result"]
        unadvertised = service.call({
            "action": "game.act", "observation_id": observed["observation_id"], "action_id": "world.move",
        })
        self.assertEqual(unadvertised["error"], "action_not_advertised")
        recovery = service.call({
            "action": "game.act", "observation_id": observed["observation_id"], "action_id": "world.wait",
            "recovery": {"modal_id": "not-a-modal"},
        })
        self.assertEqual(recovery["error"], "unknown_native_modal")
        index[0] = 1
        stale = service.call({
            "action": "game.act", "observation_id": observed["observation_id"], "action_id": "world.wait",
        })
        self.assertEqual(stale["error"], "stale_observation")
        self.assertEqual(dispatched, [])

    def test_named_current_modal_recovery_returns_its_receipt(self) -> None:
        frames = [native_frame(50), native_frame(51)]
        frames[0]["safe_recovery"] = {"modal_id": "activity:50", "actions": ["world.wait"]}
        index = [0]

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            index[0] = 1
            return {
                "accepted": True,
                "native_receipt": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "semantic_response": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "next_frame": frames[1], "_next_frame": frames[1],
            }

        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch,
        ))
        observed = service.call({"action": "game.observe"})["result"]
        result = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait", "recovery": {"modal_id": "activity:50"},
        })
        self.assertTrue(result["ok"])
        self.assertEqual(result["recovery_receipt"]["native_receipt"]["accepted"], True)

    def test_activity_recovery_uses_issuing_observation_after_later_world_frame(self) -> None:
        interruption = activity_interruption_frame(60)
        world = native_frame(61)
        frames = [interruption, world]
        index = [0]
        dispatched: list[tuple[str, str]] = []

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            dispatched.append((str(frame["frame_id"]), action_id))
            return {
                "native_receipt": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "semantic_response": {
                    "frame_id": frame["frame_id"], "action_id": action_id, "accepted": True,
                },
                "next_frame": world, "_next_frame": world,
            }

        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch,
        ))
        observed = service.call({"action": "game.observe"})["result"]
        index[0] = 1
        accepted = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.ignore",
        })
        self.assertTrue(accepted["ok"])
        self.assertEqual(dispatched, [("r012-proof:activity:60", "activity.ignore")])
        self.assertEqual(accepted["receipt"]["native_receipt"]["frame_id"], observed["observation_id"])
        self.assertEqual(accepted["observation"]["observation_id"], "r012-proof:61")
        duplicate = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.ignore",
        })
        self.assertEqual(duplicate["error"], "duplicate_submission")

    def test_activity_recovery_rejects_bad_receipts_and_unauthorized_actions_without_extra_input(self) -> None:
        interruption = activity_interruption_frame(70)
        world = native_frame(71)
        dispatched: list[str] = []

        def service_for(receipt: dict[str, object]) -> cockpit.CockpitService:
            return cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
                lambda: interruption,
                lambda _frame, action: dispatched.append(action) or receipt,
            ))

        unauthorized = service_for({})
        observed = unauthorized.call({"action": "game.observe"})["result"]
        rejected = unauthorized.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.stop",
        })
        self.assertEqual(rejected["error"], "action_not_advertised")
        self.assertEqual(dispatched, [])

        missing = service_for({"next_frame": world})
        observed = missing.call({"action": "game.observe"})["result"]
        self.assertEqual(missing.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.ignore",
        })["error"], "native_receipt_missing")

        mismatched = service_for({
            "native_receipt": {
                "frame_id": "other", "action_id": "activity.ignore", "accepted": True,
            }, "next_frame": world,
        })
        observed = mismatched.call({"action": "game.observe"})["result"]
        self.assertEqual(mismatched.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.ignore",
        })["error"], "native_receipt_mismatch")
        self.assertEqual(dispatched, ["activity.ignore", "activity.ignore"])

    def test_harness_adapter_reads_the_run_bound_native_frame(self) -> None:
        with patch.object(startup_harness, "current_semantic_step_frame", return_value=native_frame(20)) as read:
            service = startup_harness.open_cockpit_game_service(
                profile="dev-harness", run_dir=Path("/tmp/r012-proof"), run_id="r012-proof",
                trace_start_offset=0,
            )
            observed = service.call({"action": "game.observe"})
        self.assertTrue(observed["ok"])
        read.assert_called_once_with(
            profile="dev-harness", run_dir=Path("/tmp/r012-proof"), run_id="r012-proof", start_offset=0,
        )


if __name__ == "__main__":
    unittest.main()
