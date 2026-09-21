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
    def service(self, frames: list[dict[str, object]], *, await_completion: bool = False) -> tuple[cockpit.CockpitService, list[str]]:
        index = [0]
        dispatched: list[str] = []

        def dispatch(issuing: dict[str, object], action_id: str,
                     stable_id: str | None = None) -> dict[str, object]:
            dispatched.append(action_id)
            index[0] += 1
            receipt = {
                "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
            }
            if action_id.startswith("wait."):
                receipt["provenance"] = "native_wait_duration_legacy_receipt"
            if "surface_id" in issuing:
                receipt.update({
                    "requested_frame_id": issuing["frame_id"],
                    "requested_surface_id": issuing["surface_id"],
                    "consuming_surface_id": issuing["surface_id"],
                })
            return {
                "native_receipt": receipt,
                "_next_frame": frames[index[0]],
            }

        def collect_activity(_: str) -> None:
            self.assertTrue(await_completion)
            index[0] += 1

        channel = cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch, binding_id="binding-a",
            read_binding_id=lambda: "binding-a",
            await_native_completion=collect_activity if await_completion else None,
            enforce_continuation_bounds=True,
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

    def test_raw_route_collects_one_semantic_menu_operation_without_premature_progress_failure(self) -> None:
        """The same wait crosses World -> menu -> duration before time advances."""
        start = frame(1, 100)
        mode = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:2", "surface_id": "wait-mode", "kind": "menu",
            "breadcrumbs": ["Wait"], "payload": {},
            "valid_actions": [{"id": "menu.choose", "stable_id": "wait-mode:wait-a-while",
                               "label": "Wait a while", "enabled": True}],
        }
        duration = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:3", "surface_id": "wait-duration", "kind": "menu",
            "breadcrumbs": ["Wait", "Duration"], "payload": {},
            "valid_actions": [{"id": "wait.1m", "stable_id": "", "label": "1 minute",
                               "enabled": True}],
        }
        complete = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:4", "surface_id": "world:4", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "game_minutes": 101,
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "Wait",
                               "enabled": True}, {"id": "world.chat", "stable_id": "",
                                                  "label": "Chat", "enabled": True}],
        }
        chat = {**complete, "frame_id": "raw-wait-proof:5", "surface_id": "chat:5",
                "kind": "menu", "breadcrumbs": ["What do you want to do?"],
                "valid_actions": [{"id": "menu.cancel", "stable_id": "", "label": "Cancel",
                                   "enabled": True}]}
        activity = frame(30, 100, state="wait_activity")
        activity["valid_actions"] = []
        service, dispatched = self.service([start, mode, duration, activity, complete, chat], await_completion=True)

        result = service.call(self.request(target=101))

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "menu.choose", "wait.1m"])
        self.assertEqual(result["result"]["partial_progress"], 1)
        self.assertEqual(service.call({"action": "run.status"})["result"]["operation"], None)
        opened = service.call({"action": "game.act", "action_id": "world.chat",
                               "observation_id": result["result"]["terminal_observation"]["observation_id"]})
        self.assertTrue(opened["ok"], opened)
        self.assertEqual(opened["observation"]["surface"]["kind"], "menu")

    def test_semantic_duration_menu_cannot_expand_the_declared_wait_recipe(self) -> None:
        """A current menu owner still cannot authorize an omitted duration."""
        start = frame(1, 100)
        mode = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:2", "surface_id": "wait-mode", "kind": "menu",
            "breadcrumbs": ["Wait"], "payload": {},
            "valid_actions": [{"id": "menu.choose", "stable_id": "wait-mode:wait-a-while",
                               "label": "Wait a while", "enabled": True}],
        }
        duration = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:3", "surface_id": "wait-duration", "kind": "menu",
            "breadcrumbs": ["Wait", "Duration"], "payload": {},
            "valid_actions": [
                {"id": "wait.5m", "stable_id": "", "label": "5 minutes", "enabled": True},
                {"id": "wait.6h", "stable_id": "", "label": "6 hours", "enabled": True},
            ],
        }
        complete = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:4", "surface_id": "world:4", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "game_minutes": 460,
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "Wait",
                               "enabled": True}],
        }
        service, dispatched = self.service([start, mode, duration, complete])

        result = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_game_minutes": 460, "bound": bound(400),
            "recipe": ["world.wait", "wait.5m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "menu.choose", "wait.5m"])

    def test_raw_route_collects_a_receipt_bound_activity_surface_without_replaying_duration(self) -> None:
        start = frame(1, 100)
        duration = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:2", "surface_id": "wait-duration", "kind": "menu",
            "breadcrumbs": ["Wait", "Duration"], "payload": {}, "game_minutes": 100,
            "valid_actions": [{"id": "wait.1m", "stable_id": "", "label": "1 minute",
                               "enabled": True}],
        }
        activity = frame(3, 100, state="wait_activity")
        activity["valid_actions"] = []
        complete = frame(4, 101)
        service, dispatched = self.service([start, duration, activity, complete], await_completion=True)

        result = service.call(self.request(target=101))

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "wait.1m"])

    def test_raw_route_resets_the_recipe_cursor_after_a_semantic_duration(self) -> None:
        start = frame(1, 100)

        def menu(sequence: int, *, duration: bool = False) -> dict[str, object]:
            return {
                "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
                "frame_id": f"raw-wait-proof:{sequence}", "surface_id": f"menu:{sequence}",
                "kind": "menu", "breadcrumbs": ["Wait"], "payload": {},
                "valid_actions": ([{"id": "wait.1m", "stable_id": "", "label": "1 minute",
                                    "enabled": True}] if duration else
                                  [{"id": "menu.choose", "stable_id": "wait-mode:wait-a-while",
                                    "label": "Wait a while", "enabled": True}]),
            }

        after_first = {
            "schema_version": 1, "event": "surface_descriptor", "run_id": "raw-wait-proof",
            "frame_id": "raw-wait-proof:4", "surface_id": "world:4", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "game_minutes": 101,
            "valid_actions": [{"id": "world.wait", "stable_id": "", "label": "Wait",
                               "enabled": True}],
        }
        final = dict(after_first)
        final.update({"frame_id": "raw-wait-proof:7", "surface_id": "world:7", "game_minutes": 102})
        service, dispatched = self.service([
            start, menu(2), menu(3, duration=True), after_first,
            menu(5), menu(6, duration=True), final,
        ])

        result = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(2),
            "recipe": ["world.wait", "wait.1m"],
        }})

        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, [
            "world.wait", "menu.choose", "wait.1m",
            "world.wait", "menu.choose", "wait.1m",
        ])

    def test_menu_selection_keeps_its_native_owner_frame(self) -> None:
        menu = frame(1, 100)
        menu["valid_actions"] = ["menu.select"]
        menu["action_inputs"] = {"menu.select": "s"}

        def dispatch(issuing: dict[str, object], action_id: str) -> dict[str, object]:
            return {
                "native_receipt": {
                    "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
                },
                "_next_frame": issuing,
            }

        channel = cockpit.CockpitRunChannel(lambda: menu, dispatch, binding_id="binding-a")
        service = cockpit.CockpitService(run_channel=channel)
        observed = service.call({"action": "game.observe"})["result"]
        result = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "menu.select",
        })

        self.assertTrue(result["ok"])
        self.assertEqual(result["observation"]["observation_id"], observed["observation_id"])

    def test_raw_route_stops_on_native_prompt_without_recovery(self) -> None:
        prompt = frame(1, 100, state="semantic_ui")
        prompt["provenance"] = "native_semantic_ui_trace"
        prompt["valid_actions"] = ["modal.acknowledge"]
        service, dispatched = self.service([prompt])

        result = service.call(self.request(target=101))

        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "native_wait_interrupted")
        self.assertEqual(result["result"]["native_stop_reason"], "semantic_ui")
        self.assertEqual(result["result"]["guarded_handling_count"], 0)
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
            interrupted["result"]["native_stop_reason"],
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
        self.assertEqual(interrupted["result"]["native_stop_reason"], "monster_spotted")
        self.assertEqual(dispatched, [])

        service, dispatched = self.service([frame(1, 100), frame(2, 101)])
        exhausted = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_game_minutes": 102, "bound": bound(1), "recipe": ["world.wait"],
        }})
        self.assertEqual(exhausted["error"], "derived_bound_exhausted")
        self.assertEqual(exhausted["failure"]["detail"]["partial_progress"], 1.0)
        self.assertEqual(dispatched, ["world.wait"])

    def test_raw_route_stops_on_no_progress_after_preserving_native_receipt(self) -> None:
        stalled = frame(2, 100)
        stalled["observed_turn"] = 1
        service, dispatched = self.service([frame(1, 100), stalled])
        result = service.call(self.request(target=101, maximum=1))
        self.assertEqual(result["error"], "proved_no_progress")
        self.assertEqual(dispatched, ["world.wait"])
        actions = [entry for entry in service.run_channel._transcript
                   if entry.get("kind") == "action"]
        self.assertEqual(actions[-1]["result"]["receipt"]["native_receipt"]["action_id"], "world.wait")

    def test_native_pause_progress_within_one_minute_is_not_a_stall(self) -> None:
        service, dispatched = self.service([frame(1, 100), frame(2, 100), frame(3, 101)])
        result = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_delta_game_minutes": 1,
            "bound": bound(1), "recipe": ["world.wait"],
        }})
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.wait", "world.wait"])
        self.assertEqual(result["result"]["partial_progress"], 1)

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
