#!/usr/bin/env python3
"""Parity and bounded-map controls for the deterministic cockpit TUI route."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit_tui  # noqa: E402


def observation() -> dict:
    return {
        "observation_id": "run-1:frame-2", "run_id": "run-1", "advertised_actions": ["world.wait", "world.move.east"],
        "expected_postcondition": "matching_native_receipt_and_fresh_observation",
        "delta": {"kind": "full"}, "compact_log": {"unsafe": False, "latest_receipt": {"id": "receipt-1"}},
        "minimap": {"radius": 2, "cells": [
            {"dx": 0, "dy": 0, "visibility": "clear", "terrain": "floor"},
            {"dx": 2, "dy": -2, "visibility": "unknown"},
            {"dx": 3, "dy": 0, "visibility": "clear", "terrain": "leak"},
        ]},
        "overmap": {"coordinate_system": "avatar_relative_omt", "radius": 1,
                    "bound_source": "native_hud_minimap_width", "center_absolute_omt": [18, -4, 0],
                    "cells": [
                        {"dx": 0, "dy": 0, "state": "clear", "vision": "full", "terrain": "shelter",
                         "provenance": "avatar_discovered_overmap",
                         "recency": {"state": "current_frame", "observed_turn": 40}},
                        {"dx": -1, "dy": 1, "state": "unknown", "terrain": "leak",
                         "provenance": "avatar_discovered_overmap",
                         "recency": {"state": "current_frame", "observed_turn": 40}},
                        {"dx": 1, "dy": -1, "state": "stale", "terrain": "leak",
                         "provenance": "saved_observation",
                         "recency": {"state": "stale", "observed_turn": 12}},
                        {"dx": 2, "dy": 0, "state": "clear", "terrain": "out_of_bounds",
                         "provenance": "avatar_discovered_overmap",
                         "recency": {"state": "current_frame", "observed_turn": 40}},
                    ]},
    }


class FakeService:
    def __init__(self) -> None:
        self.calls: list[dict] = []
        self.current = observation()

    def call(self, request: dict) -> dict:
        self.calls.append(dict(request))
        if request["action"] == "game.observe":
            return {"ok": True, "result": self.current}
        if request["action"] == "run.status":
            return {"ok": True, "result": {"run_id": "run-1", "binding_id": "binding-7", "state": "active"}}
        if request["action"] == "game.act":
            self.current = {**self.current, "observation_id": "run-1:frame-3", "delta": {"kind": "change"}}
            return {"ok": True, "receipt": {"native_receipt": {"accepted": True}}, "observation": self.current}
        raise AssertionError(request)


class CockpitTuiTest(unittest.TestCase):
    def test_render_is_deterministic_bounded_and_unknown_is_not_terrain(self) -> None:
        rendered = cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"})
        self.assertEqual(rendered, cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"}))
        self.assertEqual(rendered["local_map"]["bound"], {"radius": 2, "source": "native_minimap.radius"})
        self.assertEqual([(cell["dx"], cell["dy"], cell["terrain"]) for cell in rendered["local_map"]["cells"]],
                         [(2, -2, "unknown"), (0, 0, "floor")])
        self.assertEqual(rendered["overmap"]["state"], "available")
        self.assertEqual([field["id"] for field in rendered["fields"]], [
            "field.binding_id", "field.frame_id", "field.run_id", "field.safety", "field.terminal",
            "field.stop_reason", "field.progress", "field.receipt", "field.mission", "field.target",
        ])

    def test_overmap_preserves_authoritative_coordinates_provenance_and_stale_states(self) -> None:
        rendered = cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"})
        overmap = rendered["overmap"]
        self.assertEqual(overmap["coordinate_system"], "avatar_relative_omt")
        self.assertEqual(overmap["bound"], {"radius": 1, "source": "native_hud_minimap_width"})
        self.assertEqual(overmap["center_absolute_omt"], [18, -4, 0])
        self.assertEqual([(cell["dx"], cell["dy"], cell["state"], cell["terrain"])
                          for cell in overmap["cells"]], [
                              (1, -1, "stale", "unknown"), (0, 0, "clear", "shelter"),
                              (-1, 1, "unknown", "unknown"),
                          ])
        stale = overmap["cells"][0]
        self.assertEqual(stale["provenance"], "saved_observation")
        self.assertEqual(stale["recency"], {"state": "stale", "observed_turn": 12})

    def test_keyboard_and_command_have_exact_public_action_parity(self) -> None:
        direct, keyed = FakeService(), FakeService()
        direct_tui, keyed_tui = cockpit_tui.CockpitTui(direct), cockpit_tui.CockpitTui(keyed)
        direct_state, keyed_state = direct_tui.refresh(), keyed_tui.refresh()
        command = next(item for item in direct_state["commands"] if item.get("action_id") == "world.wait")
        self.assertEqual(command["request_view"], {
            "id": "request.command.world.wait", "state": "ready",
            "request": {"action": "game.act", "observation_id": "run-1:frame-2", "action_id": "world.wait"},
            "contract": {
                "id": "contract.game.act.v1", "required": ["action", "observation_id", "action_id"],
                "properties": {
                    "action": {"const": "game.act"},
                    "observation_id": {"type": "string", "source": "field.frame_id"},
                    "action_id": {"type": "string", "source": "advertised_actions"},
                },
            },
        })
        self.assertEqual(direct_tui.dispatch(command["id"]), keyed_tui.dispatch_key(command["key"]))
        self.assertEqual([call for call in direct.calls if call["action"] == "game.act"],
                         [call for call in keyed.calls if call["action"] == "game.act"])
        self.assertEqual(direct_tui.state, keyed_tui.state)

    def test_every_alias_has_a_noninteractive_contract_without_becoming_a_control(self) -> None:
        state = cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"})
        aliases = {command["label"]: command for command in state["commands"] if command["kind"] == "alias"}
        self.assertEqual(set(aliases), {
            "KEEP WATCH", "MAKE CAMP", "STOCK UP", "ZAP", "MOVE OUT", "EYES UP", "BIG MAP",
        })
        self.assertTrue(all(command["legal"] is False for command in aliases.values()))
        self.assertEqual(aliases["KEEP WATCH"]["request_view"]["contract"]["id"],
                         "contract.game.keep_watch.v1")
        self.assertTrue(aliases["KEEP WATCH"]["request_view"]["contract"]["guarded"])
        self.assertEqual(aliases["MOVE OUT"]["request_view"]["contract"]["id"],
                         "contract.game.guarded_move_relative.v1")
        self.assertTrue(aliases["MOVE OUT"]["request_view"]["contract"]["guarded"])
        for label in ("MAKE CAMP", "STOCK UP", "ZAP"):
            view = aliases[label]["request_view"]
            self.assertEqual(view["state"], "arguments_required")
            self.assertEqual(view["request"], {"action": "scenario.prepare"})
            self.assertEqual(view["missing"], ["id", "required_typeid", "candidate_offsets"])
        for label in ("EYES UP", "BIG MAP"):
            view = aliases[label]["request_view"]
            self.assertEqual(view["state"], "ready")
            self.assertEqual(view["request"], {"action": "game.observe"})

    def test_alias_is_never_a_fake_control_and_stale_observation_is_visible(self) -> None:
        service = FakeService()
        tui = cockpit_tui.CockpitTui(service)
        tui.refresh()
        self.assertEqual(tui.dispatch("alias.keep_watch")["error"], "command_is_alias_only")
        self.assertEqual(cockpit_tui.CockpitTui(type("Bad", (), {"call": lambda _self, _request: {"ok": False, "error": "stale_frame"}})()).refresh()["error"], "stale_frame")


if __name__ == "__main__":
    unittest.main()
