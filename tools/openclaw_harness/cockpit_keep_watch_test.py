#!/usr/bin/env python3
"""Focused contract checks for the guarded Keep watch cockpit recipe."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402


def frame(sequence: int, minutes: int, safety: dict[str, object]) -> dict[str, object]:
    return {
        "run_id": "keep-watch-proof",
        "frame_id": f"keep-watch-proof:{sequence}",
        "state": "world",
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
        "keep_watch_safety": safety,
    }


def bound() -> dict[str, object]:
    return {
        "basis": "scheduler_boundary",
        "source": "native scheduler target minus current game minute",
        "unit": "game_minutes",
        "maximum": 2,
        "progress_required": True,
    }


class KeepWatchTest(unittest.TestCase):
    def service(self, frames: list[dict[str, object]]) -> tuple[cockpit.CockpitService, list[str]]:
        index = [0]
        dispatched: list[str] = []

        def dispatch(issuing: dict[str, object], action_id: str,
                     stable_id: str | None = None) -> dict[str, object]:
            dispatched.append(action_id)
            index[0] += 1
            native_receipt = {
                "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
            }
            if "surface_id" in issuing:
                native_receipt.update({
                    "requested_frame_id": issuing["frame_id"],
                    "requested_surface_id": issuing["surface_id"],
                    "consuming_surface_id": issuing["surface_id"],
                })
            return {
                "native_receipt": native_receipt,
                "next_frame": frames[index[0]],
                "_next_frame": frames[index[0]],
            }

        def await_native_completion(activity_frame_id: str) -> None:
            self.assertEqual(frames[index[0]]["frame_id"], activity_frame_id)
            index[0] += 1

        channel = cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch,
            binding_id="binding-a", read_binding_id=lambda: "binding-a",
            await_native_completion=await_native_completion,
            enforce_continuation_bounds=True,
        )
        return cockpit.CockpitService(run_channel=channel), dispatched

    @staticmethod
    def menu_frame(sequence: int, *, stable_id: str = "wait-choice", label: str = "Wait a while") -> dict[str, object]:
        return {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "keep-watch-proof",
            "frame_id": f"keep-watch-proof:{sequence}", "surface_id": f"surface:{sequence}",
            "kind": "menu", "breadcrumbs": ["Wait"], "payload": {},
            "valid_actions": [{"id": "menu.choose", "stable_id": stable_id,
                               "label": label, "enabled": True}],
        }

    def test_declared_stable_menu_choice_is_the_only_generic_menu_authority(self) -> None:
        start = frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False})
        target = frame(3, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False})
        target.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:3",
            "kind": "world", "breadcrumbs": ["World"], "payload": {},
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "world.wait", "enabled": True}],
        })
        service, dispatched = self.service([start, self.menu_frame(2), target])
        recipe = ["world.wait", {"action_id": "menu.choose", "stable_id": "wait-choice", "label": "Wait a while"}]
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": recipe,
        }})
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "menu.choose"])

    def test_semantic_wait_menus_use_advertised_stable_choices(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = self.menu_frame(
            2, stable_id="wait-mode:wait-a-while", label="Wait a while",
        )
        duration = self.menu_frame(3, stable_id="", label="wait.1m")
        duration["valid_actions"] = [{
            "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
        }]
        after_first_minute = self.menu_frame(4, stable_id="world-wait", label="Wait")
        after_first_minute.update({
            "kind": "world", "game_minutes": 101,
            "valid_actions": [{
                "id": "world.wait", "stable_id": "world-wait", "label": "Wait", "enabled": True,
            }],
        })
        next_mode = self.menu_frame(
            5, stable_id="wait-mode:wait-a-while", label="Wait a while",
        )
        next_duration = self.menu_frame(6, stable_id="", label="wait.1m")
        next_duration["valid_actions"] = [{
            "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
        }]
        target = self.menu_frame(7, stable_id="world-wait", label="Wait")
        target.update({
            "kind": "world", "game_minutes": 102,
            "valid_actions": [{
                "id": "world.wait", "stable_id": "world-wait", "label": "Wait", "enabled": True,
            }],
        })
        service, dispatched = self.service([
            start, mode, duration, after_first_minute, next_mode, next_duration, target,
        ])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, [
            "world.wait", "menu.choose", "wait.1m",
            "world.wait", "menu.choose", "wait.1m",
        ])

    def test_permissive_watch_receipts_the_explicit_semantic_damage_ignore_prompt(self) -> None:
        safety = {"classification": "clear", "monster": False, "danger": False, "damage": False}
        start = frame(1, 100, safety)
        damage_prompt = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "keep-watch-proof",
            "frame_id": "keep-watch-proof:2", "surface_id": "surface:2", "kind": "prompt",
            "breadcrumbs": ["Activity distraction", "CANCEL_ACTIVITY_OR_IGNORE_QUERY"],
            "payload": {
                "title": "CANCEL_ACTIVITY_OR_IGNORE_QUERY",
                "text": "Ouch, something hurts! Stop waiting? (Case Sensitive)",
            },
            "valid_actions": [
                {"id": "prompt.choose", "stable_id": "prompt-option:1", "label": "YES", "enabled": True},
                {"id": "prompt.choose", "stable_id": "prompt-option:2", "label": "NO", "enabled": True},
                {"id": "prompt.choose", "stable_id": "prompt-option:3", "label": "MANAGER", "enabled": True},
                {"id": "prompt.choose", "stable_id": "prompt-option:4", "label": "IGNORE", "enabled": True},
            ],
        }
        target = self.menu_frame(3, stable_id="world-wait", label="Wait")
        target.update({
            "kind": "world", "breadcrumbs": ["World"], "game_minutes": 101,
            "valid_actions": [{
                "id": "world.wait", "stable_id": "world-wait", "label": "Wait", "enabled": True,
            }],
        })
        service, dispatched = self.service([start, damage_prompt, target])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"], "danger_handling": "ignore_danger_and_interruptions",
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "prompt.choose"])
        self.assertEqual(result["result"]["handled_interruptions"], [{
            "classification": "damage_detected",
            "decision": "ignore_explicit_damage_prompt",
            "observation_id": "keep-watch-proof:2",
            "action_id": "prompt.choose",
            "stable_id": "prompt-option:4",
        }])

    def test_wait_menu_choice_reobserves_past_a_transient_world_successor(self) -> None:
        safety = {"classification": "clear", "monster": False, "danger": False, "damage": False}
        start = frame(1, 100, safety)
        mode = self.menu_frame(2, stable_id="wait-mode:wait-a-while", label="Wait a while")
        transient = self.menu_frame(3, stable_id="world-wait", label="Wait")
        transient.update({
            "kind": "world", "breadcrumbs": ["World"],
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "world.wait",
                               "enabled": True}],
        })
        duration = self.menu_frame(4, stable_id="", label="wait.1m")
        duration["valid_actions"] = [{
            "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
        }]
        target = self.menu_frame(5, stable_id="world-wait", label="Wait")
        target.update({
            "kind": "world", "breadcrumbs": ["World"], "game_minutes": 101,
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "world.wait",
                               "enabled": True}],
        })
        frames = [start, mode, transient, duration, target]
        index = [0]
        dispatched: list[str] = []

        def read_frame() -> dict[str, object]:
            return frames[index[0]]

        def dispatch(issuing: dict[str, object], action_id: str,
                     stable_id: str | None = None) -> dict[str, object]:
            dispatched.append(action_id)
            if action_id == "world.wait":
                index[0] = 1
                successor = mode
            elif action_id == "menu.choose":
                # The native receipt can expose World before the duration
                # descriptor is emitted; the next observation owns wait.1m.
                index[0] = 3
                successor = transient
            else:
                index[0] = 4
                successor = target
            receipt = {"accepted": True, "action_id": action_id, "frame_id": issuing["frame_id"]}
            if "surface_id" in issuing:
                receipt.update({"requested_frame_id": issuing["frame_id"],
                                "requested_surface_id": issuing["surface_id"],
                                "consuming_surface_id": issuing["surface_id"]})
            return {"native_receipt": receipt,
                    "next_frame": successor, "_next_frame": successor}

        channel = cockpit.CockpitRunChannel(
            read_frame, dispatch, binding_id="binding-a", read_binding_id=lambda: "binding-a",
        )
        result = cockpit.CockpitService(run_channel=channel).call({
            "action": "game.keep_watch", "keep_watch": {
                "enabled": True, "target_game_minutes": 101, "bound": bound(),
                "recipe": ["world.wait", "wait.1m"],
            },
        })

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "menu.choose", "wait.1m"])

    def test_semantic_world_owner_consumes_its_primitive_before_menu_handoff(self) -> None:
        start = self.menu_frame(1, stable_id="world-wait", label="Wait")
        start.update({
            "kind": "world", "game_minutes": 100,
            "valid_actions": [{"id": "world.wait", "stable_id": "world-wait",
                               "label": "Wait", "enabled": True}],
        })
        target = self.menu_frame(3, stable_id="world-wait", label="Wait")
        target.update({
            "kind": "world", "game_minutes": 101,
            "valid_actions": [{"id": "world.wait", "stable_id": "world-wait",
                               "label": "Wait", "enabled": True}],
        })
        service, dispatched = self.service([start, self.menu_frame(2), target])
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_delta_game_minutes": 1, "bound": bound(),
            "recipe": ["world.wait", {"action_id": "menu.choose", "stable_id": "wait-choice",
                                         "label": "Wait a while"}],
        }})
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "menu.choose"])


    def test_declared_menu_choice_fails_closed_when_the_label_changes(self) -> None:
        start = frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False})
        service, dispatched = self.service([start, self.menu_frame(2, label="Set an alarm")])
        recipe = ["world.wait", {"action_id": "menu.choose", "stable_id": "wait-choice", "label": "Wait a while"}]
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": recipe,
        }})
        self.assertEqual(result["error"], "keep_watch_menu_choice_not_advertised")
        self.assertEqual(dispatched, ["world.wait"])

    def test_descriptor_without_authorized_recipe_action_fails_closed_without_reobserving(self) -> None:
        unavailable = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        unavailable.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:1",
            "kind": "world", "breadcrumbs": ["World"], "payload": {}, "valid_actions": [{
                "id": "menu.choose", "stable_id": "unrelated-choice",
                "label": "Set an alarm", "enabled": True,
            }],
        })
        reads = [0]

        def read_frame() -> dict[str, object]:
            reads[0] += 1
            return unavailable

        channel = cockpit.CockpitRunChannel(
            read_frame, lambda *_: self.fail("a non-advertised action must not dispatch"),
            binding_id="binding-a", read_binding_id=lambda: "binding-a",
        )
        result = cockpit.CockpitService(run_channel=channel).call({
            "action": "game.keep_watch", "keep_watch": {
                "enabled": True, "target_game_minutes": 101, "bound": bound(),
                "recipe": ["world.wait"],
            },
        })

        self.assertEqual(result["error"], "keep_watch_recipe_action_not_advertised")
        self.assertEqual(reads[0], 1)

    def test_rejected_owner_must_change_before_retry(self) -> None:
        raw = frame(8, 100, {"classification": "clear", "monster": False,
                             "danger": False, "damage": False})
        raw.update({"surface_id": "surface:8", "valid_actions": [
            {"id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True}]})
        dispatched = []
        def dispatch(*args):
            dispatched.append(args)
            if len(dispatched) > 1:
                self.fail("redispatched an already rejected native frame")
            return {"native_receipt": {"accepted": False, "rejection_reason": "wrong_surface"}}
        channel = cockpit.CockpitRunChannel(lambda: raw, dispatch, binding_id="a",
                                            read_binding_id=lambda: "a")
        result = channel.keep_watch({"enabled": True, "target_game_minutes": 101,
                                     "bound": bound(), "recipe": ["wait.1m"]})
        self.assertEqual(result["error"], "keep_watch_rejected_owner_unchanged")
        self.assertEqual(len(dispatched), 1)

    def test_stale_primitive_retries_only_when_the_fresh_owner_declares_it(self) -> None:
        safety = {"classification": "clear", "monster": False, "danger": False, "damage": False}
        start = frame(8, 100, safety)
        fresh = frame(11, 100, safety)
        target = frame(12, 101, safety)
        fresh_target = frame(13, 101, safety)
        for raw, surface_id in ((start, "surface:8"), (fresh, "surface:11"),
                                (target, "surface:12"), (fresh_target, "surface:13")):
            raw.update({"surface_id": surface_id, "valid_actions": [
                {"id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True},
            ]})
        current = [start]
        dispatched: list[str] = []

        def dispatch(issuing: dict[str, object], action_id: str, stable_id: str | None = None) -> dict[str, object]:
            dispatched.append(action_id)
            if len(dispatched) == 1:
                current[0] = fresh
                return {"native_receipt": {"accepted": False, "rejection_reason": "wrong_surface"}}
            current[0] = fresh_target
            return {"native_receipt": {"accepted": True, "frame_id": issuing["frame_id"],
                                        "action_id": action_id,
                                        "requested_surface_id": issuing["surface_id"],
                                        "consuming_surface_id": issuing["surface_id"]},
                    "next_frame": target, "_next_frame": target}

        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch, binding_id="binding-a",
                                            read_binding_id=lambda: "binding-a")
        service = cockpit.CockpitService(run_channel=channel)
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["wait.1m"],
        }})
        # The synthetic dispatcher deliberately does not publish the final
        # post-receipt observation.  Reaching this existing fail-closed
        # boundary proves recovery retried the same declaration exactly once.
        self.assertEqual(result["error"], "fresh_observation_missing")
        self.assertEqual(dispatched, ["wait.1m", "wait.1m"])

    def test_stale_primitive_fails_when_the_fresh_owner_changes_the_declaration(self) -> None:
        safety = {"classification": "clear", "monster": False, "danger": False, "damage": False}
        start = frame(8, 100, safety)
        changed = frame(11, 100, safety)
        start.update({"surface_id": "surface:8", "valid_actions": [
            {"id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True},
        ]})
        changed.update({"surface_id": "surface:11", "valid_actions": [
            {"id": "wait.5m", "stable_id": "", "label": "wait.5m", "enabled": True},
        ]})
        current = [start]

        def dispatch(issuing: dict[str, object], action_id: str, stable_id: str | None = None) -> dict[str, object]:
            current[0] = changed
            return {"native_receipt": {"accepted": False, "rejection_reason": "wrong_surface"}}

        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch, binding_id="binding-a",
                                            read_binding_id=lambda: "binding-a")
        service = cockpit.CockpitService(run_channel=channel)
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["wait.1m"],
        }})
        self.assertEqual(result["error"], "keep_watch_stale_action_declaration_changed")

    def test_guarded_recipe_matches_primitive_wait_transitions_and_terminal_state(self) -> None:
        clean = [
            frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(2, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(3, 102, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
        ]
        guarded, guarded_actions = self.service(clean)
        result = guarded.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertTrue(result["ok"])
        self.assertEqual(result["result"]["stop_reason"], "target_reached")
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 102)
        self.assertEqual(guarded_actions, ["world.wait", "world.wait"])
        self.assertEqual(result["result"]["model_round_trips"], 1)
        self.assertEqual(result["result"]["tool_round_trips"], 2)

        primitive, primitive_actions = self.service(clean)
        observed = primitive.call({"action": "game.observe"})["result"]
        for _ in range(2):
            self.assertTrue(primitive.call({
                "action": "run.continue", "observation_id": observed["observation_id"],
                "expected_signal": "game_minutes", "bound": {
                    **bound(), "maximum": 1,
                },
            })["ok"])
            observed = primitive.call({
                "action": "game.act", "observation_id": observed["observation_id"],
                "action_id": "world.wait",
            })["observation"]
        self.assertEqual(primitive_actions, guarded_actions)
        self.assertEqual(observed["game_minutes"], result["result"]["terminal_observation"]["game_minutes"])

    def test_explicit_danger_modes_keep_cautious_default_and_receipt_permissive_choice(self) -> None:
        danger = frame(1, 100, {
            "classification": "monster_spotted", "monster": True,
            "danger": True, "damage": False,
        })
        clean = frame(2, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        cautious, cautious_actions = self.service([danger])
        stopped = cautious.call({"action": "game.wait", "wait": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"], "danger_handling": "handle_classified_non_dangerous",
        }})
        self.assertEqual(stopped["error"], "keep_watch_unsafe_condition")
        self.assertEqual(cautious_actions, [])

        permissive, permissive_actions = self.service([danger, clean])
        continued = permissive.call({"action": "game.wait", "wait": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"], "danger_handling": "ignore_danger_and_interruptions",
        }})
        self.assertTrue(continued["ok"])
        self.assertEqual(permissive_actions, ["world.wait"])
        self.assertEqual(continued["result"]["danger_handling"], "ignore_danger_and_interruptions")
        self.assertEqual(continued["result"]["handled_interruptions"][0]["decision"], "continue_wait")

    def test_finish_generates_bound_role_and_measured_cost_receipts(self) -> None:
        clean = [
            frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(2, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
        ]
        service, _ = self.service(clean)
        watched = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
        }})
        terminal = watched["result"]["terminal_observation"]
        finished = service.call({
            "action": "run.finish", "observation_id": terminal["observation_id"],
            "stop_reason": "target_predicate_proved", "unused_authority": "none",
            "r019_acceptance_matrix": {"role": "guarded"},
        })

        packet = finished["result"]["stop_detail"]["r019_acceptance_matrix"]
        self.assertEqual(packet["role_receipt"]["role"], "guarded")
        self.assertEqual(packet["role_receipt"]["run_id"], "keep-watch-proof")
        self.assertEqual(packet["round_trip_receipt"]["model"]["count"], 1)
        self.assertEqual(packet["round_trip_receipt"]["tool"]["count"], 1)

    def test_finish_rejects_a_role_contradicted_by_the_live_transcript(self) -> None:
        service, _ = self.service([frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })])
        observed = service.call({"action": "game.observe"})["result"]
        result = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "target_predicate_proved", "unused_authority": "none",
            "r019_acceptance_matrix": {"role": "guarded"},
        })
        self.assertEqual(result, {"ok": False, "error": "r019_role_does_not_match_live_transcript"})

    def test_native_wait_completion_frame_ends_the_same_guarded_transaction(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity["state"] = "wait_activity"
        activity["valid_actions"] = []
        activity["action_inputs"] = {}
        complete = frame(3, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        complete["state"] = "wait_activity_complete"
        service, dispatched = self.service([start, activity, complete])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
        }})

        self.assertTrue(result["ok"])
        self.assertEqual(result["result"]["stop_reason"], "target_reached")
        self.assertEqual(dispatched, ["world.wait"])

    def test_same_minute_world_after_wait_activity_keeps_one_completion_bridge(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        completion_world = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_activity = frame(4, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        target = frame(5, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target.update({"state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}})
        service, dispatched = self.service([
            start, activity, completion_world, next_activity, target,
        ])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "world.wait"])

    def test_empty_completion_marker_awaits_its_following_world_frame(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        completion = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        completion.update({"state": "wait_activity_complete", "valid_actions": [],
                           "action_inputs": {}})
        completion_world = frame(4, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_activity = frame(5, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        target = frame(6, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target.update({"state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}})
        service, dispatched = self.service([
            start, activity, completion, completion_world, next_activity, target,
        ])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "world.wait"])

    def test_guarded_recipe_from_pre_menu_selects_the_fresh_advertised_duration_action(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode["state"] = "wait_mode_choice"
        mode["valid_actions"] = ["wait.duration_menu", "alarm.duration_menu"]
        mode["action_inputs"] = {"wait.duration_menu": "w", "alarm.duration_menu": "a"}
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        activity = frame(4, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity["state"] = "wait_activity"
        activity["valid_actions"] = []
        activity["action_inputs"] = {}
        complete = frame(5, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        complete["state"] = "wait_activity_complete"
        service, dispatched = self.service([start, mode, duration, activity, complete])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"])
        self.assertEqual(dispatched, ["world.wait", "wait.duration_menu", "wait.1m"])
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)

    def test_guarded_recipe_from_open_duration_chooser_selects_the_advertised_duration_action(self) -> None:
        duration = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        activity = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity["state"] = "wait_activity"
        activity["valid_actions"] = []
        activity["action_inputs"] = {}
        complete = frame(3, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        complete["state"] = "wait_activity_complete"
        service, dispatched = self.service([duration, activity, complete])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"])
        self.assertEqual(dispatched, ["wait.1m"])
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)

    def test_guarded_recipe_reobserves_a_retired_duration_owner_before_retrying(self) -> None:
        stale_duration = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        stale_duration.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:1",
            "kind": "menu", "breadcrumbs": ["Wait"], "payload": {},
            "state": "wait_duration_choice", "valid_actions": [{
                "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
            }],
        })
        fresh_duration = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        fresh_duration.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:2",
            "kind": "menu", "breadcrumbs": ["Wait"], "payload": {},
            "state": "wait_duration_choice", "valid_actions": [{
                "id": "wait.1m", "stable_id": "", "label": "wait.1m", "enabled": True,
            }],
        })
        activity = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:3",
            "kind": "menu", "breadcrumbs": ["Wait"], "payload": {},
            "state": "wait_activity", "valid_actions": [],
        })
        target = frame(4, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target.update({
            "schema_version": 1, "event": "surface_descriptor", "surface_id": "surface:4",
            "kind": "world", "breadcrumbs": ["World"], "payload": {},
            "state": "wait_activity_complete", "valid_actions": [],
        })
        current = [stale_duration]
        dispatched: list[str] = []

        def dispatch(issuing: dict[str, object], action_id: str,
                     stable_id: str | None = None) -> dict[str, object]:
            dispatched.append(str(issuing["frame_id"]))
            if issuing["frame_id"] == stale_duration["frame_id"]:
                current[0] = fresh_duration
                return {"native_receipt": {"accepted": False, "rejection_reason": "wrong_surface"}}
            current[0] = activity
            return {"native_receipt": {"accepted": True, "frame_id": issuing["frame_id"],
                                        "action_id": action_id,
                                        "requested_frame_id": issuing["frame_id"],
                                        "requested_surface_id": issuing["surface_id"],
                                        "consuming_surface_id": issuing["surface_id"]}, "next_frame": activity,
                    "_next_frame": activity}

        def await_native_completion(activity_frame_id: str) -> None:
            self.assertEqual(activity_frame_id, activity["frame_id"])
            current[0] = target

        channel = cockpit.CockpitRunChannel(
            lambda: current[0], dispatch, binding_id="binding-a",
            read_binding_id=lambda: "binding-a", await_native_completion=await_native_completion,
            enforce_continuation_bounds=True,
        )
        result = cockpit.CockpitService(run_channel=channel).call({
            "action": "game.keep_watch", "keep_watch": {
                "enabled": True, "target_game_minutes": 101, "bound": bound(),
                "recipe": ["world.wait", "wait.1m"],
            },
        })
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, [stale_duration["frame_id"], fresh_duration["frame_id"]])

    def test_completed_wait_bridges_only_to_the_next_declared_world_primitive(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                     "action_inputs": {"wait.duration_menu": "w"}})
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                         "action_inputs": {"wait.1m": "1"}})
        activity = frame(4, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        complete = frame(5, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        complete.update({"state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}})
        completion_world = frame(6, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_mode = frame(7, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                          "action_inputs": {"wait.duration_menu": "w"}})
        next_duration = frame(8, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                              "action_inputs": {"wait.1m": "1"}})
        next_activity = frame(9, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        target = frame(10, 102, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target.update({"state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}})
        service, dispatched = self.service([
            start, mode, duration, activity, complete, completion_world, next_mode, next_duration,
            next_activity, target,
        ])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, [
            "world.wait", "wait.duration_menu", "wait.1m",
            "world.wait", "wait.duration_menu", "wait.1m",
        ])

    def test_guarded_recipe_from_open_duration_chooser_rejects_absent_or_ambiguous_recipe_actions(self) -> None:
        for recipe, advertised, resolution in (
                (["world.wait"], ["wait.1m"], "absent"),
                (["world.wait", "wait.1m", "wait.5m"], ["wait.1m", "wait.5m"], "ambiguous"),
        ):
            with self.subTest(recipe=recipe, advertised=advertised):
                duration = frame(1, 100, {
                    "classification": "clear", "monster": False, "danger": False, "damage": False,
                })
                duration["state"] = "wait_duration_choice"
                duration["valid_actions"] = advertised
                duration["action_inputs"] = {action: action for action in advertised}
                service, dispatched = self.service([duration])

                result = service.call({"action": "game.keep_watch", "keep_watch": {
                    "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": recipe,
                }})

                self.assertEqual(result["error"], "keep_watch_recipe_action_not_advertised")
                self.assertEqual(result["result"]["duration_action_resolution"], resolution)
                self.assertEqual(dispatched, [])

    def test_guarded_recipe_rejects_wrong_intermediate_advertisement_without_dispatch(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode["state"] = "wait_mode_choice"
        mode["valid_actions"] = ["alarm.duration_menu"]
        mode["action_inputs"] = {"alarm.duration_menu": "a"}
        service, dispatched = self.service([start, mode])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertEqual(result["error"], "keep_watch_recipe_action_not_advertised")
        self.assertEqual(result["result"]["action_id"], "wait.duration_menu")
        self.assertEqual(dispatched, ["world.wait"])

    def test_guarded_recipe_rejects_open_duration_chooser_without_a_recipe_duration(self) -> None:
        duration = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        service, dispatched = self.service([duration])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait"],
        }})

        self.assertEqual(result["error"], "keep_watch_recipe_action_not_advertised")
        self.assertEqual(result["result"]["duration_action_resolution"], "absent")
        self.assertEqual(dispatched, [])

    def test_guarded_recipe_reobserves_stale_intermediate_frame_before_dispatch(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode["state"] = "wait_mode_choice"
        mode["valid_actions"] = ["wait.duration_menu"]
        mode["action_inputs"] = {"wait.duration_menu": "w"}
        refreshed_mode = dict(mode)
        refreshed_mode["frame_id"] = "keep-watch-proof:refreshed-mode"
        target = frame(3, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        service, dispatched = self.service([start, mode, target])
        original_read = service.run_channel._read_native_frame
        reads = [0]

        def stale_after_first_dispatch() -> dict[str, object]:
            reads[0] += 1
            if 4 <= reads[0] <= 6:
                return refreshed_mode
            return original_read()

        service.run_channel._read_native_frame = stale_after_first_dispatch
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"])
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)
        self.assertEqual(dispatched, ["world.wait", "wait.duration_menu"])

    def test_both_off_switches_and_every_nonclear_safety_frame_fail_without_extra_input(self) -> None:
        unsafe = frame(1, 100, {
            "classification": "hostile_spotted", "monster": True, "danger": True, "damage": False,
        })
        service, dispatched = self.service([unsafe, unsafe])
        master_off = service.call({"action": "game.keep_watch", "keep_watch": {
            "master_enabled": False, "enabled": True, "target_game_minutes": 101,
            "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertEqual(master_off, {
            "ok": False, "error": "keep_watch_master_disabled_use_primitive_actions",
        })
        off = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": False, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertEqual(off, {"ok": False, "error": "keep_watch_disabled_use_primitive_actions"})
        stopped = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertEqual(stopped["error"], "keep_watch_unsafe_condition")
        self.assertEqual(dispatched, [])

    def test_each_off_switch_produces_a_primitive_only_live_receipt(self) -> None:
        clean = [
            frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(2, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
        ]
        for switch, request in (
                ("master_enabled", {"master_enabled": False, "enabled": True}),
                ("enabled", {"enabled": False}),
        ):
            with self.subTest(switch=switch):
                service, dispatched = self.service(clean)
                disabled = service.call({"action": "game.keep_watch", "keep_watch": {
                    **request, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
                }})
                self.assertFalse(disabled["ok"])
                observed = service.call({"action": "game.observe"})["result"]
                self.assertTrue(service.call({
                    "action": "run.continue", "observation_id": observed["observation_id"],
                    "expected_signal": "game_minutes", "bound": {**bound(), "maximum": 1},
                })["ok"])
                acted = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                      "action_id": "world.wait"})
                finished = service.call({
                    "action": "run.finish", "observation_id": acted["observation"]["observation_id"],
                    "stop_reason": "target_predicate_proved", "unused_authority": "none",
                    "r019_acceptance_matrix": {
                        "role": "off:" + switch,
                        "clean_start_identity": "fixture:clean:100",
                        "source_identity": "fixture:source",
                    },
                })
                receipt = finished["result"]["stop_detail"]["r019_acceptance_matrix"]["off_switch_receipt"]
                self.assertEqual(dispatched, ["world.wait"])
                self.assertEqual(receipt["native_dispatch_count"], 0)
                self.assertEqual(receipt["primitive_native_dispatch_count"], 1)
                self.assertEqual(receipt["native_receipt_actions"], ["world.wait"])

    def test_disabled_recipe_binding_drift_records_the_terminal_without_dispatch(self) -> None:
        clean = [
            frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(2, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
            frame(3, 102, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
        ]
        binding = ["binding-a"]
        service, dispatched = self.service(clean)
        service.run_channel._read_binding_id = lambda: binding[0]
        def mutate() -> dict[str, object]:
            binding[0] = "binding-b"
            return {"schema": "caol-r019-supported-binding-drift-v1",
                    "effect": "disposable_control_only"}
        service.run_channel._mutate_binding_for_control = mutate
        disabled = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": False, "target_game_minutes": 102, "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertEqual(disabled["error"], "keep_watch_disabled_use_primitive_actions")
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({"action": "run.continue", "observation_id": observed["observation_id"],
                                      "expected_signal": "game_minutes", "bound": {**bound(), "maximum": 1}})["ok"])
        advanced = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                 "action_id": "world.wait"})["observation"]
        self.assertTrue(service.call({"action": "run.continue", "observation_id": advanced["observation_id"],
                                      "expected_signal": "game_minutes", "bound": {**bound(), "maximum": 1}})["ok"])
        stopped = service.call({"action": "run.controlled_binding_drift",
                                "observation_id": advanced["observation_id"], "action_id": "world.wait",
                                "r019_acceptance_matrix": {"role": "off:enabled",
                                    "clean_start_identity": "fixture:clean:100",
                                    "source_identity": "fixture:source"}})
        self.assertEqual(stopped["error"], "binding_drift")
        receipt = stopped["final"]["stop_detail"]["binding_drift_receipt"]
        self.assertEqual(receipt["before"]["identity"], "runtime_binding")
        self.assertNotEqual(receipt["before"]["hash"], receipt["after"]["hash"])
        self.assertEqual(receipt["attempted_action"], "world.wait")
        self.assertFalse(receipt["native_dispatch_after_drift"])
        self.assertEqual(receipt["guarded_handling_count"], 0)
        # A supported mutator must itself create the drift; this externally
        # changed binding is therefore a fail-closed counterexample.
        self.assertEqual(dispatched, ["world.wait"])

    def test_binding_drift_control_rejects_missing_unchanged_or_mismatched_control(self) -> None:
        for mutation, expected in (
                (None, "binding_drift"),
                (lambda: {}, "r019_binding_drift_control_mutation_unproved"),
                (lambda: {"schema": "caol-r019-supported-binding-drift-v1"}, "r019_binding_drift_control_mutation_unproved"),
        ):
            with self.subTest(expected=expected):
                service, _ = self.service([
                    frame(1, 100, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
                    frame(2, 101, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
                    frame(3, 102, {"classification": "clear", "monster": False, "danger": False, "damage": False}),
                ])
                observed = service.call({"action": "game.observe"})["result"]
                service.call({"action": "game.keep_watch", "keep_watch": {
                    "enabled": False, "target_game_minutes": 102, "bound": bound(), "recipe": ["world.wait"],
                }})
                self.assertTrue(service.call({"action": "run.continue", "observation_id": observed["observation_id"],
                                              "expected_signal": "game_minutes", "bound": {**bound(), "maximum": 1}})["ok"])
                advanced = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                         "action_id": "world.wait"})["observation"]
                self.assertTrue(service.call({"action": "run.continue", "observation_id": advanced["observation_id"],
                                              "expected_signal": "game_minutes", "bound": {**bound(), "maximum": 1}})["ok"])
                service.run_channel._mutate_binding_for_control = mutation
                result = service.call({"action": "run.controlled_binding_drift",
                                       "observation_id": advanced["observation_id"], "action_id": "world.wait",
                                       "r019_acceptance_matrix": {"role": "off:enabled",
                                           "clean_start_identity": "fixture:clean", "source_identity": "fixture:source"}})
                self.assertEqual(result["error"], expected)

    def test_declared_safe_prompt_is_the_only_interruption_the_recipe_acknowledges(self) -> None:
        prompt = frame(1, 100, {
            "classification": "safe_prompt", "monster": False, "danger": False, "damage": False,
            "action_id": "modal.acknowledge", "recovery": {"modal_id": "flavour:1"},
        })
        prompt["state"] = "semantic_ui"
        prompt["provenance"] = "native_semantic_ui_trace"
        prompt["observed_turn"] = None
        prompt["valid_actions"] = ["modal.acknowledge"]
        prompt["safe_recovery"] = {"modal_id": "flavour:1", "actions": ["modal.acknowledge"]}
        target = frame(2, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        service, dispatched = self.service([prompt, target])
        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(), "recipe": ["world.wait"],
        }})
        self.assertTrue(result["ok"])
        self.assertEqual(dispatched, ["modal.acknowledge"])
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)

    def test_safe_prompt_without_progress_signal_is_recovered_before_progress_check(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode["state"] = "wait_mode_choice"
        mode["valid_actions"] = ["wait.duration_menu"]
        mode["action_inputs"] = {"wait.duration_menu": "w"}
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        prompt = frame(4, 100, {
            "classification": "safe_prompt", "monster": False, "danger": False, "damage": False,
            "action_id": "modal.acknowledge", "recovery": {"modal_id": "flavour:1"},
        })
        prompt["state"] = "semantic_ui"
        prompt["provenance"] = "native_semantic_ui_trace"
        prompt["observed_turn"] = None
        prompt.pop("game_minutes")
        prompt["valid_actions"] = ["modal.acknowledge"]
        prompt["safe_recovery"] = {"modal_id": "flavour:1", "actions": ["modal.acknowledge"]}
        activity = frame(5, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity["state"] = "wait_activity"
        activity["valid_actions"] = []
        activity["action_inputs"] = {}
        target = frame(6, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target["state"] = "wait_activity_complete"
        service, dispatched = self.service([start, mode, duration, prompt, activity, target])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"])
        self.assertEqual(dispatched, [
            "world.wait", "wait.duration_menu", "wait.1m", "modal.acknowledge",
        ])
        self.assertEqual(result["result"]["stop_reason"], "target_reached")

    def test_safe_popup_bridge_keeps_its_bound_continuation_for_one_eoc_activity_frame(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                     "action_inputs": {"wait.duration_menu": "w"}})
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                         "action_inputs": {"wait.1m": "1"}})
        prompt = frame(4, 100, {
            "classification": "safe_prompt", "monster": False, "danger": False, "damage": False,
            "action_id": "modal.acknowledge", "recovery": {"modal_id": "flavour:1"},
        })
        prompt["state"] = "semantic_ui"
        prompt["provenance"] = "native_semantic_ui_trace"
        prompt["observed_turn"] = None
        prompt.pop("game_minutes")
        prompt["valid_actions"] = ["modal.acknowledge"]
        prompt["safe_recovery"] = {
            "modal_id": "flavour:1", "actions": ["modal.acknowledge"],
            "activity_bridge": {"type": "eoc", "action_id": "activity.continue"},
        }
        activity = frame(5, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({
            "state": "activity_distraction", "provenance": "native_activity_distraction_query",
            "producer": "activity_distraction_query",
            "activity_type": "eoc", "valid_actions": ["activity.continue"],
            "action_inputs": {"activity.continue": "N"},
        })
        returned = frame(6, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        returned.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        target = frame(7, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target["state"] = "wait_activity_complete"
        service, dispatched = self.service([start, mode, duration, prompt, activity, returned, target])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "wait.duration_menu", "wait.1m",
                                      "modal.acknowledge", "activity.continue"])
        self.assertEqual(result["result"]["stop_reason"], "target_reached")

    def test_clear_native_activity_uses_its_fresh_ignore_not_the_wait_recipe(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                     "action_inputs": {"wait.duration_menu": "w"}})
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                         "action_inputs": {"wait.1m": "1"}})
        distraction = frame(4, 100, {
            "classification": "damage_detected", "monster": False, "danger": False,
            "damage": True,
        })
        distraction.update({
            "state": "activity_distraction",
            "producer": "activity_distraction_query",
            "activity_type": "noise",
            "valid_actions": ["activity.ignore"],
            "action_inputs": {"activity.ignore": "I"},
        })
        resumed = frame(5, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        resumed.update({
            "state": "activity_resumed",
            "provenance": "native_activity_distraction_return",
            "valid_actions": [],
            "action_inputs": {},
        })
        target = frame(6, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        service, dispatched = self.service([start, mode, duration, distraction, resumed, target])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
            "danger_handling": "ignore_danger_and_interruptions",
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "wait.duration_menu", "wait.1m",
                                      "activity.ignore"])
        self.assertEqual(result["result"]["handled_interruptions"], [{
            "classification": "damage_detected",
            "decision": "continue_wait",
            "observation_id": "keep-watch-proof:4",
        }, {
            "classification": "clear",
            "decision": "ignore_clear_activity_distraction",
            "observation_id": "keep-watch-proof:4",
            "action_id": "activity.ignore",
        }])

    def test_clear_activity_same_minute_world_recovery_restarts_only_the_declared_wait(self) -> None:
        start = frame(1, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode = frame(2, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                     "action_inputs": {"wait.duration_menu": "w"}})
        duration = frame(3, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                         "action_inputs": {"wait.1m": "1"}})
        distraction = frame(4, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        distraction.update({
            "state": "activity_distraction",
            "provenance": "native_activity_distraction_query",
            "producer": "activity_distraction_query",
            "activity_type": "noise",
            "valid_actions": ["activity.ignore"],
            "action_inputs": {"activity.ignore": "I"},
        })
        recovered = frame(5, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_mode = frame(6, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_mode.update({"state": "wait_mode_choice", "valid_actions": ["wait.duration_menu"],
                          "action_inputs": {"wait.duration_menu": "w"}})
        next_duration = frame(7, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        next_duration.update({"state": "wait_duration_choice", "valid_actions": ["wait.1m"],
                              "action_inputs": {"wait.1m": "1"}})
        activity = frame(8, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        activity.update({"state": "wait_activity", "valid_actions": [], "action_inputs": {}})
        target = frame(9, 101, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        })
        target.update({"state": "wait_activity_complete", "valid_actions": [], "action_inputs": {}})
        service, dispatched = self.service([
            start, mode, duration, distraction, recovered, next_mode, next_duration, activity, target,
        ])

        result = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": bound(),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, [
            "world.wait", "wait.duration_menu", "wait.1m", "activity.ignore",
            "world.wait", "wait.duration_menu", "wait.1m",
        ])


if __name__ == "__main__":
    unittest.main()
