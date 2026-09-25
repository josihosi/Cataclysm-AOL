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
    def test_macro_stationary_door_uses_nested_surface_identity(self) -> None:
        initial = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "door-test",
            "surface_id": "world-1", "frame_id": "frame-1", "kind": "world",
            "breadcrumbs": ["World"], "payload": {"avatar": json.dumps({"absolute_ms": [1, 1, 0]})},
            "valid_actions": [{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}],
        }
        successor = {**initial, "surface_id": "world-2", "frame_id": "frame-2"}
        current = [initial]
        calls = []
        def dispatch(frame, action_id):
            calls.append(action_id)
            current[0] = successor
            return {"accepted": True, "next_frame": successor, "native_receipt": {
                "run_id": "door-test", "frame_id": "transient-move", "action_id": action_id,
                "accepted": False, "outcome": "no_progress", "coordinate_space": "absolute_ms",
                "before_absolute_ms": [1, 1, 0], "expected_absolute_ms": [2, 1, 0],
                "after_absolute_ms": [1, 1, 0], "after_terrain": "t_floor",
                "surface_receipt": {"run_id": "door-test", "requested_run_id": "door-test",
                    "requested_frame_id": "frame-1", "requested_surface_id": "world-1",
                    "consuming_frame_id": "frame-1", "consuming_surface_id": "world-1",
                    "action_id": action_id, "accepted": True}}}
        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
        result = channel.raw_move_relative({"enabled": True, "offset_ms": [2, 0],
            "bound": {"basis": "path_progress", "source": "two east", "unit": "steps", "maximum": 2}})
        self.assertEqual(result["error"], "raw_move_relative_no_progress", result)
        self.assertEqual(result["result"]["terminal_absolute_ms"], [1, 1, 0])
        self.assertEqual(result["result"]["partial_progress"], 0)
        self.assertEqual(calls, ["world.move.east"])

    def test_consumed_movement_without_displacement_returns_fresh_world(self) -> None:
        for outcome in ("no_progress", "blocked"):
            with self.subTest(outcome=outcome):
                initial = {
                    "event": "surface_descriptor", "schema_version": 1, "run_id": "door-test",
                    "surface_id": "world-1", "frame_id": "frame-1", "kind": "world",
                    "breadcrumbs": ["World"], "payload": {},
                    "valid_actions": [{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}],
                }
                successor = {**initial, "surface_id": "world-2", "frame_id": "frame-2",
                             "payload": {"terrain": "open door"}}
                current = [initial]
                calls = []
                def dispatch(frame, action_id):
                    calls.append(action_id)
                    current[0] = successor
                    surface = {"run_id": "door-test", "requested_run_id": "door-test",
                               "requested_frame_id": "frame-1", "requested_surface_id": "world-1",
                               "consuming_frame_id": "frame-1", "consuming_surface_id": "world-1",
                               "action_id": action_id, "accepted": True}
                    return {"accepted": True, "next_frame": successor, "native_receipt": {
                        **surface, "accepted": False, "frame_id": "transient-move",
                        "coordinate_space": "absolute_ms", "outcome": outcome,
                        "before_absolute_ms": [1, 1, 0], "after_absolute_ms": [1, 1, 0],
                        "surface_receipt": surface}}
                channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
                observed = channel.observe()
                result = channel.act(observation_id=observed["observation_id"], action_id="world.move.east")
                self.assertTrue(result["ok"], result)
                self.assertEqual(calls, ["world.move.east"])
                self.assertEqual(result["observation"]["frame_id"], "frame-2")
                self.assertFalse(result["receipt"]["native_receipt"]["accepted"])
                self.assertEqual(result["receipt"]["native_receipt"]["outcome"], outcome)

    def test_message_close_refreshes_only_confirmed_same_menu_rejection(self) -> None:
        for case in ("redraw", "different_owner", "disabled", "wrong_receipt", "uncertain", "accepted"):
            with self.subTest(case=case):
                initial = {
                    "event": "surface_descriptor", "schema_version": 1, "run_id": "menu-test",
                    "surface_id": "messages", "frame_id": "messages-1", "kind": "message_log",
                    "breadcrumbs": ["Message log"], "payload": {"matching_lines": "0"},
                    "valid_actions": [{"id": "message_log.close", "stable_id": "", "label": "Close", "enabled": True}],
                }
                populated = {**initial, "frame_id": "messages-2", "payload": {"matching_lines": "232"}}
                if case == "different_owner":
                    populated["surface_id"] = "other-messages"
                if case == "disabled":
                    populated["valid_actions"] = [{**initial["valid_actions"][0], "enabled": False}]
                world = {**initial, "surface_id": "world", "frame_id": "world-3",
                         "kind": "world", "valid_actions": [], "payload": {}}
                current = [initial]
                dispatched = []

                def dispatch(frame, action_id):
                    dispatched.append(frame["frame_id"])
                    accepted = len(dispatched) == 2 or case == "accepted"
                    current[0] = world if accepted else populated
                    receipt = {
                        "run_id": "menu-test", "requested_run_id": "menu-test",
                        "requested_frame_id": frame["frame_id"],
                        "requested_surface_id": frame["surface_id"],
                        "consuming_surface_id": frame["surface_id"],
                        "action_id": action_id, "accepted": accepted,
                        "rejection_reason": "" if accepted else "stale_frame",
                    }
                    if case == "wrong_receipt":
                        receipt["requested_frame_id"] = "unrelated"
                    return {"native_receipt": None if case == "uncertain" else receipt,
                            "next_frame": world if accepted else None}

                channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
                observed = channel.observe()
                result = channel.act(observation_id=observed["observation_id"], action_id="message_log.close")
                self.assertEqual(result["ok"], case in {"redraw", "accepted"}, result)
                self.assertEqual(dispatched, ["messages-1", "messages-2"] if case == "redraw" else ["messages-1"])

    def test_action_displays_populated_menu_after_its_initial_successor(self) -> None:
        before = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "menu-test",
            "surface_id": "world", "frame_id": "world-1", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "_event_offset": 10,
            "valid_actions": [{"id": "world.messages", "stable_id": "", "label": "Messages", "enabled": True}],
        }
        initial = {
            **before, "surface_id": "messages", "frame_id": "messages-1", "kind": "message_log",
            "breadcrumbs": ["Message log"], "payload": {"matching_lines": "0"}, "_event_offset": 20,
            "valid_actions": [{"id": "message_log.close", "stable_id": "", "label": "Close", "enabled": True}],
        }
        populated = {**initial, "frame_id": "messages-2", "payload": {"matching_lines": "246"}, "_event_offset": 30}
        current = [before]

        def dispatch(frame, action_id):
            current[0] = populated
            return {"native_receipt": {
                "requested_frame_id": frame["frame_id"], "requested_surface_id": frame["surface_id"],
                "consuming_surface_id": frame["surface_id"], "action_id": action_id, "accepted": True,
            }, "next_frame": initial}

        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
        observed = channel.observe()
        result = channel.act(observation_id=observed["observation_id"], action_id="world.messages")
        self.assertTrue(result["ok"])
        self.assertEqual(result["observation"]["observation_id"], "messages-2")
        self.assertEqual(result["observation"]["surface"]["facts"]["matching_lines"], "246")

    def test_native_top_descriptor_replaces_world_view_for_every_surface_family(self) -> None:
        descriptors = [
            ("world", "World"), ("overmap", "Overmap"), ("inventory", "Inventory"),
            ("dialogue", "Dialogue"), ("menu", "Menu/Prompt"), ("prompt", "Menu/Prompt"),
            ("direction", "Direction"), ("target", "Target"), ("unsupported", "Unsupported"),
        ]
        frames = [{
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": f"surface-{kind}", "frame_id": f"surface-proof:{index}", "kind": kind,
            "breadcrumbs": ["World", kind], "payload": {"owner": kind},
            "valid_actions": ([] if kind == "unsupported" else [{
                "id": f"{kind}.act", "stable_id": f"{kind}-target", "label": kind,
                "enabled": True,
            }]),
            # A child descriptor must not acquire these legacy parent fields.
            "observation": {"schema": "caol-avatar-visible-v1", "avatar": {"name": "parent"},
                            "visible_local": [{"terrain": "leak"}]},
        } for index, (kind, _family) in enumerate(descriptors)]
        for frame in frames:
            frame.pop("observation")
        index = [0]
        channel = cockpit.CockpitRunChannel(lambda: frames[index[0]])
        for expected_index, (kind, family) in enumerate(descriptors):
            index[0] = expected_index
            observed = channel.observe()
            self.assertEqual(observed["schema"], "caol-cockpit-observation-v2")
            self.assertEqual(observed["surface"]["family"], family)
            self.assertEqual(observed["surface"]["facts"], {"owner": kind})
            self.assertEqual(observed["breadcrumbs"], ["World", kind])
            self.assertNotIn("avatar", observed)
            self.assertNotIn("visible_local", observed)
            if kind == "unsupported":
                self.assertEqual(observed["advertised_actions"], [])
                self.assertEqual(observed["automation"]["state"], "stopped")
            else:
                self.assertEqual(observed["advertised_actions"], [f"{kind}.act"])

    def test_native_top_descriptor_exposes_bound_production_channel_observation(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-world", "frame_id": "surface-proof:channel", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        channel_observation = {
            "status": "green", "eligible": True, "record_count": 6,
            "channels": ["sound", "smoke", "light", "scent", "prior_knowledge", "incidental_contact"],
            "issues": [],
        }
        observed = cockpit.CockpitRunChannel(
            lambda: descriptor,
            read_evidence=lambda: {"production_channel_observation": channel_observation},
        ).observe()
        self.assertEqual(
            observed["compact_log"]["production_channel_observation"], channel_observation,
        )

    def test_unsupported_descriptor_with_an_action_fails_closed(self) -> None:
        frame = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "unsupported", "frame_id": "surface-proof:unsupported", "kind": "unsupported",
            "breadcrumbs": ["World", "Unsupported"], "payload": {}, "valid_actions": [{
                "id": "forbidden", "stable_id": "forbidden", "label": "forbidden", "enabled": True,
            }],
        }
        with self.assertRaisesRegex(ValueError, "unsupported native surface advertised an action"):
            cockpit.CockpitRunChannel(lambda: frame).observe()

    def test_unsupported_descriptor_never_retains_disabled_parent_actions(self) -> None:
        frame = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "unsupported", "frame_id": "surface-proof:unsupported", "kind": "unsupported",
            "breadcrumbs": ["World", "Unsupported"], "payload": {}, "valid_actions": [{
                "id": "world.inventory", "stable_id": "", "label": "Inventory", "enabled": False,
            }],
        }
        with self.assertRaisesRegex(ValueError, "unsupported native surface advertised an action"):
            cockpit.CockpitRunChannel(lambda: frame).observe()

    def test_descriptor_action_does_not_replace_successor_with_legacy_frame(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-world", "frame_id": "surface-proof:1", "kind": "world",
            "breadcrumbs": ["World"], "payload": {"owner": "world"},
            "valid_actions": [{"id": "world.inventory", "stable_id": "", "label": "Inventory",
                               "enabled": True}],
        }
        successor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-inventory", "frame_id": "surface-proof:2", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {"owner": "inventory"}, "_event_offset": 20,
            "valid_actions": [],
        }
        reads = 0
        dispatches: list[dict[str, object]] = []

        def read_frame() -> dict[str, object]:
            nonlocal reads
            reads += 1
            return descriptor if reads == 1 else {
                **native_frame(3), "run_id": "surface-proof", "_event_offset": 30,
            }

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            dispatches.append({"frame_id": frame["frame_id"], "action_id": action_id})
            return {
                "native_receipt": {
                    "requested_frame_id": frame["frame_id"],
                    "requested_surface_id": frame["surface_id"],
                    "consuming_surface_id": frame["surface_id"], "action_id": action_id,
                    "accepted": True,
                },
                "next_frame": successor,
            }

        channel = cockpit.CockpitRunChannel(read_frame, dispatch)
        observed = channel.observe()
        acted = channel.act(observation_id=observed["observation_id"], action_id="world.inventory")

        self.assertTrue(acted["ok"])
        self.assertEqual(dispatches, [{"frame_id": "surface-proof:1", "action_id": "world.inventory"}])
        self.assertEqual(acted["observation"]["frame_id"], "surface-proof:2")
        self.assertEqual(reads, 2)

    def test_activity_continuation_rejects_parent_when_nested_prompt_is_consuming(self) -> None:
        parent = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-activity", "frame_id": "surface-proof:activity:1",
            "kind": "activity_distraction", "breadcrumbs": ["Activity distraction"],
            "payload": {}, "valid_actions": [{
                "id": "activity.continue", "stable_id": "", "label": "Continue", "enabled": True,
            }],
        }
        child = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-prompt", "frame_id": "surface-proof:prompt:1",
            "kind": "prompt", "breadcrumbs": ["Activity distraction", "CANCEL_ACTIVITY_OR_IGNORE_QUERY"],
            "payload": {"title": "CANCEL_ACTIVITY_OR_IGNORE_QUERY", "text": "Stop waiting?"},
            "valid_actions": [{
                "id": "prompt.choose", "stable_id": "prompt-option:1", "label": "NO", "enabled": True,
            }],
        }
        current = [parent]
        dispatched: list[str] = []

        def dispatch(_frame: dict[str, object], action_id: str) -> dict[str, object]:
            dispatched.append(action_id)
            return {}

        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
        observed = channel.observe()
        current[0] = child
        rejected = channel.act(
            observation_id=observed["observation_id"], action_id="activity.continue",
        )

        self.assertFalse(rejected["ok"])
        self.assertEqual(rejected["error"], "stale_observation")
        self.assertEqual(dispatched, [])

    def test_latest_superseding_prompt_accepts_once_and_replay_is_stale(self) -> None:
        """A freshly observed nested prompt owns exactly one native action."""
        prompt = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-prompt", "frame_id": "surface-proof:prompt:1",
            "kind": "prompt", "breadcrumbs": ["Activity distraction", "CANCEL_ACTIVITY_OR_IGNORE_QUERY"],
            "payload": {"title": "CANCEL_ACTIVITY_OR_IGNORE_QUERY", "text": "Stop waiting?"},
            "valid_actions": [{
                "id": "prompt.choose", "stable_id": "prompt-option:2", "label": "NO", "enabled": True,
            }],
        }
        successor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-world", "frame_id": "surface-proof:world:1", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }
        dispatched: list[tuple[str, str, str]] = []

        def dispatch(frame: dict[str, object], action_id: str, stable_id: str | None = None) -> dict[str, object]:
            dispatched.append((str(frame["frame_id"]), action_id, str(stable_id or "")))
            return {
                "native_receipt": {
                    "requested_frame_id": frame["frame_id"],
                    "requested_surface_id": frame["surface_id"],
                    "consuming_surface_id": frame["surface_id"],
                    "action_id": action_id, "stable_id": stable_id or "", "accepted": True,
                },
                "next_frame": successor,
            }

        channel = cockpit.CockpitRunChannel(lambda: prompt, dispatch)
        observed = channel.observe()
        acted = channel.act(
            observation_id=observed["observation_id"], action_id="prompt.choose",
            stable_id="prompt-option:2",
        )
        self.assertTrue(acted["ok"], acted)
        self.assertEqual(dispatched, [("surface-proof:prompt:1", "prompt.choose", "prompt-option:2")])
        replay = channel.act(
            observation_id=observed["observation_id"], action_id="prompt.choose",
            stable_id="prompt-option:2",
        )
        self.assertEqual(replay["error"], "duplicate_submission")

    def test_same_frame_native_selection_publishes_changed_payload_and_clock(self):
        descriptor = {"event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "menu", "frame_id": "surface-proof:1", "kind": "menu",
            "game_minutes": 100, "game_turn": 6000, "breadcrumbs": ["World", "Menu"],
            "payload": {"selected_index": "0"},
            "valid_actions": [{"id": "menu.select", "stable_id": "", "label": "Select", "enabled": True}]}
        successor = {**descriptor, "payload": {"selected_index": "1"}}
        def dispatch(frame, action_id):
            return {"native_receipt": {"requested_frame_id": frame["frame_id"],
                "requested_surface_id": frame["surface_id"], "consuming_surface_id": frame["surface_id"],
                "action_id": action_id, "accepted": True}, "next_frame": successor}
        channel = cockpit.CockpitRunChannel(lambda: descriptor, dispatch)
        before = channel.observe()
        result = channel.act(observation_id=before["observation_id"], action_id="menu.select")
        self.assertTrue(result["ok"], result)
        after = result["observation"]
        self.assertEqual(after["observation_id"], before["observation_id"])
        self.assertEqual(after["surface"]["facts"]["selected_index"], "1")
        self.assertEqual(after["game_minutes"], 100)
        self.assertEqual(after["game_turn"], 6000)

    def test_auto_move_continue_releases_consumed_prompt_without_reusing_it(self):
        prompt = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:auto-move", "frame_id": "surface-proof:auto-move:1",
            "kind": "prompt", "breadcrumbs": ["World", "YESNO"],
            "payload": {"text": "tough zombie spotted! Cancel auto move?"},
            "valid_actions": [
                {"id": "prompt.choose", "stable_id": "prompt-option:no", "label": "NO", "enabled": True},
            ],
        }
        resumed = {
            "event": "native_travel_resume", "run_id": "surface-proof",
            "frame_id": "surface-proof:auto-move:1:travel-resumed:travel-7",
            "state": "native_auto_move_resumed", "provenance": "native_auto_move_resume",
            "native_travel_receipt": {
                "run_id": "surface-proof", "travel_id": "travel-7", "state": "resumed",
                "destination_present": True, "destination_cleared": False,
            }, "valid_actions": [],
        }
        def dispatch(frame, action_id, stable_id=None):
            return {"native_receipt": {
                "run_id": "surface-proof", "requested_frame_id": frame["frame_id"],
                "requested_surface_id": frame["surface_id"],
                "consuming_surface_id": frame["surface_id"],
                "consuming_frame_id": frame["frame_id"], "action_id": action_id,
                "accepted": True,
            }, "next_frame": resumed}
        channel = cockpit.CockpitRunChannel(lambda: prompt, dispatch)
        observed = channel.observe()
        result = channel.act(
            observation_id=observed["observation_id"], action_id="prompt.choose",
            stable_id="prompt-option:no",
        )
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["continuation"], {"state": "released"})
        self.assertEqual(
            result["expected_postcondition"],
            "accepted_native_auto_move_resume_then_later_observation",
        )
        replay = channel.act(
            observation_id=observed["observation_id"], action_id="prompt.choose",
            stable_id="prompt-option:no",
        )
        self.assertEqual(replay["error"], "duplicate_submission")

    def test_descriptor_retains_all_advertised_stable_ids_for_one_action(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-inventory", "frame_id": "surface-proof:stable", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {}, "valid_actions": [
                {"id": "inventory.select", "stable_id": "lighter", "label": "lighter", "enabled": True},
                {"id": "inventory.select", "stable_id": "flashlight", "label": "flashlight", "enabled": True},
            ],
        }
        successor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-menu", "frame_id": "surface-proof:stable:next", "kind": "menu",
            "breadcrumbs": ["World", "Inventory", "Item menu"], "payload": {}, "valid_actions": [],
        }
        dispatches: list[tuple[str, str, str]] = []

        def dispatch(frame: dict[str, object], action_id: str, stable_id: str) -> dict[str, object]:
            dispatches.append((str(frame["frame_id"]), action_id, stable_id))
            return {
                "native_receipt": {
                    "requested_frame_id": frame["frame_id"],
                    "requested_surface_id": frame["surface_id"],
                    "consuming_surface_id": frame["surface_id"], "action_id": action_id,
                    "stable_id": stable_id, "accepted": True,
                },
                "next_frame": successor,
            }

        channel = cockpit.CockpitRunChannel(lambda: descriptor, dispatch)
        observed = channel.observe()
        acted = channel.act(
            observation_id=observed["observation_id"], action_id="inventory.select", stable_id="lighter",
        )

        self.assertTrue(acted["ok"])
        self.assertEqual(dispatches, [("surface-proof:stable", "inventory.select", "lighter")])

    def test_descriptor_action_rejects_a_receipt_from_a_different_surface(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:inventory", "frame_id": "surface-proof:receipt", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {}, "valid_actions": [{
                "id": "inventory.cancel", "stable_id": "", "label": "Cancel", "enabled": True,
            }],
        }
        successor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:world", "frame_id": "surface-proof:receipt:next", "kind": "world",
            "breadcrumbs": ["World"], "payload": {}, "valid_actions": [],
        }

        def dispatch(_frame: dict[str, object], _action_id: str) -> dict[str, object]:
            return {"native_receipt": {
                "requested_frame_id": "surface-proof:receipt", "requested_surface_id": "surface-proof:other",
                "consuming_surface_id": "surface-proof:other", "action_id": "inventory.cancel", "accepted": True,
            }, "next_frame": successor}

        channel = cockpit.CockpitRunChannel(lambda: descriptor, dispatch)
        observed = channel.observe()
        rejected = channel.act(observation_id=observed["observation_id"], action_id="inventory.cancel")
        self.assertEqual(rejected["error"], "native_receipt_mismatch")

    def test_wrong_surface_rejection_revokes_single_use_authority_after_async_frame(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:inventory", "frame_id": "surface-proof:frame:1", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {}, "valid_actions": [{
                "id": "inventory.cancel", "stable_id": "", "label": "Cancel", "enabled": True,
            }],
        }

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            self.assertEqual(frame["surface_id"], "surface-proof:inventory")
            return {"native_receipt": {
                "run_id": "surface-proof",
                "requested_surface_id": "surface-proof:inventory",
                "requested_frame_id": "surface-proof:frame:1",
                "consuming_surface_id": "surface-proof:inventory:async-next",
                "consuming_frame_id": "surface-proof:frame:2",
                "action_id": action_id,
                "accepted": False,
                "rejection_reason": "wrong_surface",
            }, "next_frame": {
                **descriptor, "surface_id": "surface-proof:inventory:async-next",
                "frame_id": "surface-proof:frame:2",
            }}

        channel = cockpit.CockpitRunChannel(lambda: descriptor, dispatch)
        observed = channel.observe()
        rejected = channel.act(observation_id=observed["observation_id"], action_id="inventory.cancel")
        self.assertEqual(rejected["error"], "native_action_rejected")
        replay = channel.act(observation_id=observed["observation_id"], action_id="inventory.cancel")
        self.assertEqual(replay["error"], "duplicate_submission")

    def test_descriptor_action_never_falls_back_to_a_legacy_successor(self) -> None:
        descriptor = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:inventory", "frame_id": "surface-proof:legacy", "kind": "inventory",
            "breadcrumbs": ["World", "Inventory"], "payload": {}, "valid_actions": [{
                "id": "inventory.cancel", "stable_id": "", "label": "Cancel", "enabled": True,
            }],
        }
        legacy_successor = native_frame(99)

        def dispatch(frame: dict[str, object], action_id: str) -> dict[str, object]:
            return {"native_receipt": {
                "requested_frame_id": frame["frame_id"], "requested_surface_id": frame["surface_id"],
                "consuming_surface_id": frame["surface_id"], "action_id": action_id, "accepted": True,
            }, "next_frame": legacy_successor}

        channel = cockpit.CockpitRunChannel(lambda: descriptor, dispatch)
        observed = channel.observe()
        rejected = channel.act(observation_id=observed["observation_id"], action_id="inventory.cancel")
        self.assertFalse(rejected["ok"])
        self.assertEqual(rejected["error"], "fresh_observation_missing")

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

    def test_cockpit_action_chain_reobserves_the_current_top_descriptor_and_fails_closed(self) -> None:
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
        self.assertEqual(reports[0]["metadata"]["status"], "required_state_present")
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

        self.assertEqual(reports[0]["cockpit_act"]["error"], "fresh_authorized_observation_unavailable")
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

    def test_native_visible_stalker_keeps_persisted_lifecycle_state(self) -> None:
        frame = native_frame(82)
        frame["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:stalker-1"},
            "kind": "monster", "name": "the writhing stalker", "attitude": "hostile",
            "dx": 3, "dy": 0, "typeid": "mon_writhing_stalker", "faction": "zombie",
            "friendly": 0, "aggro_character": True,
            "writhing_stalker_state": {
                "provenance": "native_monster_persistent_state",
                "phase": 5, "phase_name": "searching", "evidence_target": 42,
                "evidence_turn": 120, "attempts_spent": 2, "attempt_sequence": 7,
                "has_retreat_waypoint": True, "retreat_waypoint_ms": [10, 11, 0],
            },
        }]
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: frame))

        result = service.call({"action": "game.observe"})

        self.assertTrue(result["ok"])
        entity = result["result"]["visible_entities"][0]
        self.assertEqual(entity["typeid"], "mon_writhing_stalker")
        self.assertEqual(entity["writhing_stalker_state"]["phase_name"], "searching")
        self.assertEqual(entity["writhing_stalker_state"]["attempt_sequence"], 7)
        self.assertEqual(entity["writhing_stalker_state"]["retreat_waypoint_ms"], [10, 11, 0])

    def test_native_visible_stalker_exposes_direct_and_light_memory_owner(self) -> None:
        frame = native_frame(83)
        frame["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:stalker-light-1"},
            "kind": "monster", "name": "the writhing stalker", "attitude": "neutral",
            "dx": 9, "dy": 0, "typeid": "mon_writhing_stalker", "faction": "zombie",
            "friendly": 0, "aggro_character": False,
            "writhing_stalker_state": {
                "provenance": "native_monster_persistent_state",
                "direct_evidence_active": False,
                "light_interest_active": True,
                "has_light_observed_position": True,
                "light_observed_position_ms": [10, 0, 1],
                "light_observed_turn": 80,
                "light_expires_turn": 170,
                "light_sample_id": "lamp#80",
                "has_last_observed_position": False,
                "has_committed_waypoint": True,
                "committed_waypoint_ms": [10, 0, 1],
            },
        }]
        service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(lambda: frame))

        result = service.call({"action": "game.observe"})

        self.assertTrue(result["ok"])
        state = result["result"]["visible_entities"][0]["writhing_stalker_state"]
        self.assertFalse(state["direct_evidence_active"])
        self.assertTrue(state["light_interest_active"])
        self.assertEqual(state["light_sample_id"], "lamp#80")
        self.assertEqual(state["light_expires_turn"], 170)
        self.assertEqual(state["committed_waypoint_ms"], [10, 0, 1])

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

    def test_wait_duration_legacy_receipt_is_projected_to_its_menu_authority(self) -> None:
        duration = {
            "event": "surface_descriptor", "schema_version": 1, "run_id": "surface-proof",
            "surface_id": "surface-proof:duration", "frame_id": "surface-proof:duration:7",
            "kind": "menu", "breadcrumbs": ["World", "Wait duration"], "payload": {},
            "valid_actions": [{"id": "wait.5m", "stable_id": "", "label": "5 minutes", "enabled": True}],
        }
        waiting = {
            "run_id": "surface-proof", "frame_id": "surface-proof:wait:8", "state": "wait_activity",
            "valid_actions": [], "action_inputs": {}, "provenance": "native_wait_duration_receipt",
        }

        def dispatch(frame: dict[str, object], action: str) -> dict[str, object]:
            self.assertEqual(frame["frame_id"], duration["frame_id"])
            return {
                "surface_request": {
                    "run_id": "surface-proof", "surface_id": "surface-proof:duration",
                    "frame_id": "surface-proof:duration:7", "action_id": action,
                },
                "native_receipt": {
                    "run_id": "surface-proof", "requested_run_id": "surface-proof",
                    "requested_surface_id": "surface-proof:duration",
                    "requested_frame_id": "surface-proof:duration:7",
                    "consuming_surface_id": "surface-proof:duration",
                    "consuming_frame_id": "surface-proof:duration:7",
                    "frame_id": "surface-proof:duration:7", "action_id": action,
                    "accepted": True, "provenance": "native_wait_duration_legacy_receipt",
                    "legacy_native_frame_id": "surface-proof:5234989:140",
                },
                "next_frame": waiting,
            }

        current = [duration]
        channel = cockpit.CockpitRunChannel(lambda: current[0], dispatch)
        observed = channel.observe()
        # This directly models the continuation created by a normal bounded
        # game.wait recipe before it opens the wait menus.
        channel._continuation = {
            "run_id": "surface-proof", "phase": "awaiting_wait_dispatch",
            "expected_signal": "game_minutes", "start": 1, "maximum": 5,
            "progress_required": True,
        }
        current[0] = waiting
        accepted = channel.act(observation_id=observed["observation_id"], action_id="wait.5m")
        self.assertTrue(accepted["ok"])
        self.assertEqual(accepted["receipt"]["native_receipt"]["frame_id"], observed["observation_id"])
        self.assertEqual(accepted["receipt"]["native_receipt"]["legacy_native_frame_id"], "surface-proof:5234989:140")
        self.assertEqual(accepted["continuation"], {"state": "awaiting_native_completion"})

    def test_harness_adapter_reads_the_run_bound_native_frame(self) -> None:
        with patch.object(startup_harness, "current_semantic_step_frame", return_value=native_frame(20)) as read:
            service = startup_harness.open_cockpit_game_service(
                profile="dev-harness", run_dir=Path("/tmp/r012-proof"), run_id="r012-proof",
                trace_start_offset=0,
            )
            observed = service.call({"action": "game.observe"})
        self.assertTrue(observed["ok"])
        read.assert_called_once()
        self.assertEqual(read.call_args.kwargs, {
            "profile": "dev-harness", "run_dir": Path("/tmp/r012-proof"),
            "run_id": "r012-proof", "start_offset": 0, "history": {},
        })

    def test_harness_adapter_retains_real_descriptor_across_activity_fallback(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "debug.log"
            source.write_text("x" * 400, encoding="utf-8")
            first = {
                "event": "surface_descriptor", "schema_version": 1, "run_id": "r012-proof",
                "frame_id": "r012-proof:world:1", "surface_id": "world:1",
                "kind": "world", "breadcrumbs": ["World"], "payload": {},
                "valid_actions": [], "_event_offset": 10,
            }
            stale_activity = {
                "event": "frame", "run_id": "r012-proof",
                "frame_id": "r012-proof:activity:2", "state": "activity_distraction",
                "_event_offset": 20,
            }
            newer = first | {"frame_id": "r012-proof:world:3", "surface_id": "world:3",
                             "_event_offset": 30}
            frames = iter((first, stale_activity, newer))
            with patch.object(startup_harness, "semantic_step_source_trace", return_value=source), \
                    patch.object(startup_harness, "current_semantic_step_frame", side_effect=lambda **_: next(frames)):
                service = startup_harness.open_cockpit_game_service(
                    profile="dev-harness", run_dir=Path(temp), run_id="r012-proof",
                    trace_start_offset=0,
                )
                self.assertEqual(service.call({"action": "game.observe"})["result"]["frame_id"], first["frame_id"])
                self.assertEqual(service.call({"action": "game.observe"})["result"]["frame_id"], first["frame_id"])
                self.assertEqual(service.call({"action": "game.observe"})["result"]["frame_id"], newer["frame_id"])


if __name__ == "__main__":
    unittest.main()
