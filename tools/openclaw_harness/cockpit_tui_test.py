#!/usr/bin/env python3
"""Parity and bounded-map controls for the deterministic cockpit TUI route."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit_tui  # noqa: E402
import cockpit  # noqa: E402


def observation() -> dict:
    return {
        "observation_id": "run-1:frame-2", "run_id": "run-1", "advertised_actions": ["world.wait", "world.move.east"],
        "expected_postcondition": "matching_native_receipt_and_fresh_observation",
        "delta": {"kind": "full"}, "toggles": {"master_enabled": True, "keep_watch": False},
        "compact_log": {
            "unsafe": False, "receipt_count": 3, "latest_receipt": {"id": "receipt-1", "accepted": True},
            "first_divergence": None, "contradictory_evidence": [],
            "latest_transition": {"sequence": 8, "kind": "native", "outcome": "accepted"},
            "persistence": "confirmed", "evidence_refs": ["semantic.steps.jsonl#receipt=3"],
        },
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


class RecordingService:
    def __init__(self, service: cockpit.CockpitService) -> None:
        self.service = service
        self.calls: list[dict] = []
        self.run_channel = service.run_channel

    def call(self, request: dict) -> dict:
        self.calls.append(dict(request))
        return self.service.call(request)


def recipe_frame(sequence: int, minutes: int) -> dict:
    return {
        "run_id": "recipe-parity", "frame_id": f"recipe-parity:{sequence}",
        "observed_turn": sequence, "game_minutes": minutes,
        "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1", "avatar": {"name": "Ada"}, "visible_local": [],
        },
        "valid_actions": ["world.wait"], "action_inputs": {"world.wait": "."},
        "keep_watch_safety": {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        },
    }


def recipe_service() -> tuple[RecordingService, list[dict]]:
    frames = [recipe_frame(1, 100), recipe_frame(2, 101)]
    index = [0]
    dispatched: list[dict] = []

    def dispatch(issuing: dict, action_id: str) -> dict:
        dispatched.append({"frame_id": issuing["frame_id"], "action_id": action_id})
        index[0] += 1
        return {
            "native_receipt": {
                "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
            },
            "next_frame": frames[index[0]], "_next_frame": frames[index[0]],
        }

    service = cockpit.CockpitService(run_channel=cockpit.CockpitRunChannel(
        lambda: frames[index[0]], dispatch, binding_id="recipe-binding",
        read_binding_id=lambda: "recipe-binding",
    ))
    return RecordingService(service), dispatched


class CockpitTuiTest(unittest.TestCase):
    def test_native_surface_projection_keeps_exact_descriptor_facts_and_actions(self) -> None:
        native_surface = {
            "schema": "caol-cockpit-observation-v2", "observation_id": "run-1:target-4",
            "frame_id": "run-1:target-4", "run_id": "run-1", "surface_id": "target-4",
            "breadcrumbs": ["World", "Inventory", "Use", "Target"],
            "surface": {
                "family": "Target", "kind": "target", "facts": {"candidate": "zombie"},
                "breadcrumbs": ["World", "Inventory", "Use", "Target"],
                "actions": [{"id": "target.choose", "stable_id": "creature-42",
                             "label": "zombie", "enabled": True}],
            },
            "advertised_actions": ["target.choose"],
            "advertised_action_details": [{"id": "target.choose", "stable_id": "creature-42",
                                            "label": "zombie", "enabled": True}],
            "compact_log": {"unsafe": False, "receipt_count": 1, "latest_receipt": {
                "accepted": True, "resulting_frame_id": "run-1:target-4"},
                "first_divergence": None, "contradictory_evidence": [], "latest_transition": None,
                "persistence": "confirmed", "evidence_refs": []},
        }
        rendered = cockpit_tui.render_state(native_surface, {
            "run_id": "run-1", "binding_id": "binding", "state": "active",
        })
        self.assertEqual(rendered["schema"], "caol-cockpit-tui-v2")
        self.assertEqual(rendered["active_surface"], native_surface["surface"])
        self.assertEqual(rendered["breadcrumbs"], native_surface["breadcrumbs"])
        self.assertEqual(rendered["commands"][0]["request_view"]["request"], {
            "action": "game.act", "observation_id": "run-1:target-4",
            "action_id": "target.choose", "stable_id": "creature-42",
        })

    def test_wait_and_move_contracts_expose_three_agent_selected_danger_modes(self) -> None:
        expected = ["stop_on_interruption", "handle_classified_non_dangerous",
                    "ignore_danger_and_interruptions"]
        for action, field in (("game.wait", "wait"), ("game.move_relative", "move_relative")):
            contract = cockpit_tui._contract(action)
            mode = contract["properties"][field]["properties"]["danger_handling"]
            self.assertEqual(mode["default"], "stop_on_interruption")
            self.assertEqual(mode["enum"], expected)
            self.assertIn("Cloaking", mode["hint"])

    def test_render_is_deterministic_bounded_and_unknown_is_not_terrain(self) -> None:
        rendered = cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"})
        self.assertEqual(rendered, cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"}))
        self.assertEqual(rendered["local_map"]["bound"], {"radius": 2, "source": "native_minimap.radius"})
        self.assertEqual([(cell["dx"], cell["dy"], cell["terrain"]) for cell in rendered["local_map"]["cells"]],
                         [(2, -2, "unknown"), (0, 0, "floor")])
        self.assertEqual(rendered["overmap"]["state"], "available")
        self.assertEqual([field["id"] for field in rendered["fields"]], [
            "field.binding_id", "field.frame_id", "field.run_id", "field.toggles", "field.safety",
            "field.terminal", "field.stop_reason", "field.progress", "field.receipt", "field.mission",
            "field.target", "field.error",
        ])

    def test_full_semantic_view_is_explicit_about_receipts_mission_safety_and_terminal_state(self) -> None:
        status = {
            "run_id": "run-1", "binding_id": "binding-7", "state": "finished",
            "continuation": {"expected_signal": "game_minutes", "maximum": 3},
            "final": {"stop_reason": "target_reached", "target_receipt": {"id": "target-1"}},
        }
        fields = {item["id"]: item["value"] for item in cockpit_tui.render_state(observation(), status)["fields"]}
        self.assertEqual(fields["field.toggles"], {"master_enabled": True, "keep_watch": False})
        self.assertEqual(fields["field.safety"], {
            "state": "clear", "first_divergence": None, "contradictory_evidence": [],
        })
        self.assertEqual(fields["field.receipt"], {
            "state": "available", "count": 3, "latest": {"id": "receipt-1", "accepted": True},
            "first_divergence": None, "contradictory_evidence": [],
            "transition": {"sequence": 8, "kind": "native", "outcome": "accepted"},
            "persistence": "confirmed", "evidence_refs": ["semantic.steps.jsonl#receipt=3"],
        })
        self.assertEqual(fields["field.mission"], {"expected_signal": "game_minutes", "maximum": 3})
        self.assertEqual(fields["field.target"], {
            "expected_postcondition": "matching_native_receipt_and_fresh_observation",
            "terminal_receipt": {"id": "target-1"},
        })
        self.assertEqual(fields["field.terminal"], {"terminal": True, "state": "finished"})
        self.assertEqual(fields["field.stop_reason"], {"state": "available", "reason": "target_reached"})

    def test_missing_and_error_facts_remain_visible_with_stable_field_ids(self) -> None:
        state = cockpit_tui.render_state({"observation_id": "run-1:empty"}, {}, {"ok": False, "error": "stale_frame"})
        fields = {item["id"]: item["value"] for item in state["fields"]}
        self.assertEqual(fields["field.toggles"], {"state": "unavailable"})
        self.assertEqual(fields["field.receipt"]["state"], "unavailable")
        self.assertEqual(fields["field.mission"], {"state": "unavailable"})
        self.assertEqual(fields["field.error"], {"ok": False, "error": "stale_frame"})
        failed = cockpit_tui.CockpitTui(type("Bad", (), {
            "call": lambda _self, _request: {"ok": False, "error": "stale_frame"},
        })()).refresh()
        self.assertEqual([item["id"] for item in failed["fields"]], [item["id"] for item in state["fields"]])

    def test_failed_status_after_successful_observation_is_unavailable_not_active(self) -> None:
        class FailedStatus(FakeService):
            def call(self, request: dict) -> dict:
                if request["action"] == "run.status":
                    self.calls.append(dict(request))
                    return {"ok": False, "error": "status_owner_unavailable"}
                return super().call(request)

        state = cockpit_tui.CockpitTui(FailedStatus()).refresh()
        fields = {item["id"]: item["value"] for item in state["fields"]}

        self.assertEqual(fields["field.terminal"], {"terminal": None, "state": "unavailable"})
        self.assertEqual(fields["field.binding_id"], {"state": "unavailable"})
        self.assertEqual(fields["field.error"]["error"], "status_owner_unavailable")

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

    def test_controlled_recipe_matches_direct_cockpit_requests_receipts_safety_and_terminal_state(self) -> None:
        direct, direct_dispatches = recipe_service()
        tui_service, tui_dispatches = recipe_service()

        direct_observation = direct.call({"action": "game.observe"})["result"]
        direct_action_request = {
            "action": "game.act", "observation_id": direct_observation["observation_id"],
            "action_id": "world.wait",
        }
        direct_action = direct.call(direct_action_request)
        direct_finish_request = {
            "action": "run.finish", "observation_id": direct_action["observation"]["observation_id"],
            "stop_reason": "target_predicate_proved", "unused_authority": "none",
        }
        direct_finish = direct.call(direct_finish_request)

        tui = cockpit_tui.CockpitTui(tui_service)
        state = tui.refresh()
        wait = next(command for command in state["commands"] if command.get("action_id") == "world.wait")
        tui_action = tui.dispatch_key(wait["key"])
        tui_finish = tui.finish(stop_reason="target_predicate_proved", unused_authority="none")

        self.assertEqual(tui_action["receipt"], direct_action["receipt"])
        self.assertEqual(tui_action["observation"], direct_action["observation"])
        self.assertEqual(tui_finish, direct_finish)
        self.assertEqual(tui_dispatches, direct_dispatches)
        self.assertEqual([request for request in tui_service.calls if request["action"] in {
            "game.act", "run.finish",
        }], [direct_action_request, direct_finish_request])
        self.assertEqual(
            tui_service.run_channel.status(), direct.run_channel.status(),  # type: ignore[union-attr]
        )
        direct_terminal_state = cockpit_tui.render_state(
            direct_action["observation"], direct.call({"action": "run.status"})["result"], direct_finish,
        )
        self.assertEqual(tui.state, direct_terminal_state)
        fields = {item["id"]: item["value"] for item in tui.state["fields"]}
        self.assertEqual(fields["field.safety"], {
            "state": "clear", "first_divergence": None, "contradictory_evidence": [],
        })
        self.assertEqual(fields["field.terminal"], {"terminal": True, "state": "finished"})
        self.assertEqual(fields["field.stop_reason"], {
            "state": "available", "reason": "target_predicate_proved",
        })

    def test_every_alias_has_a_noninteractive_contract_without_becoming_a_control(self) -> None:
        state = cockpit_tui.render_state(observation(), {"binding_id": "binding-7", "state": "active"})
        aliases = {command["label"]: command for command in state["commands"] if command["kind"] == "alias"}
        self.assertTrue({
            "WAIT", "KEEP WATCH", "MAKE CAMP", "STOCK UP", "ZAP", "MOVE OUT",
            "EYES UP", "BIG MAP",
        } <= set(aliases))
        self.assertTrue(all(command["legal"] is False for command in aliases.values()))
        self.assertEqual(aliases["KEEP WATCH"]["request_view"]["contract"]["id"],
                         "contract.game.keep_watch.v1")
        self.assertTrue(aliases["KEEP WATCH"]["request_view"]["contract"]["guarded"])
        self.assertEqual(aliases["MOVE OUT"]["request_view"]["contract"]["id"],
                         "contract.game.move_relative.v1")
        self.assertEqual(aliases["WAIT"]["request_view"]["contract"]["id"],
                         "contract.game.wait.v1")
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
        fields = {item["id"]: item["value"] for item in tui.state["fields"]}
        self.assertEqual(fields["field.error"]["error"], "command_is_alias_only")


if __name__ == "__main__":
    unittest.main()
