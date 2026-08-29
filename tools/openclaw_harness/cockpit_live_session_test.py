#!/usr/bin/env python3
"""Behavioral counterexamples for worker-owned live cockpit termination."""

from __future__ import annotations

import json
import io
import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import cockpit  # noqa: E402
import startup_harness  # noqa: E402


def frame(sequence: int, minutes: int, *, entity_dx: int = 2) -> dict[str, object]:
    return {
        "run_id": "live-proof",
        "frame_id": f"live-proof:{sequence}",
        "state": "world",
        "observed_turn": sequence,
        "game_minutes": minutes,
        "provenance": "native_semantic_step_trace",
        "observation": {
            "schema": "caol-avatar-visible-v1",
            "avatar": {"name": "Ada"},
            "visible_local": [{
                "identity": {"dx": 0, "dy": 0, "terrain": "t_floor"},
                "terrain": "floor",
            }],
            "minimap": {
                "schema": "caol-native-minimap-v1",
                "radius": 12,
                "cells": [
                    {"dx": 0, "dy": 0, "visibility": "clear", "terrain": "floor"},
                    {"dx": 12, "dy": 12, "visibility": "unknown"},
                ],
            },
            "visible_entities": [{
                "identity": {"kind": "npc", "id": "character:4"},
                "kind": "npc", "name": "Scout", "attitude": "neutral",
                "dx": entity_dx, "dy": 0,
            }],
            "visible_zones": [{"name": "Camp storage", "type": "CAMP_STORAGE", "dx": 3, "dy": 1}],
        },
        "valid_actions": ["world.wait"],
        "action_inputs": {"world.wait": "."},
        "logs": "raw implementation detail",
        "ocr": "incidental screen text",
    }


def wait_activity_frame(sequence: int, minutes: int) -> dict[str, object]:
    value = frame(sequence, minutes)
    value["state"] = "wait_activity"
    value["valid_actions"] = []
    value["action_inputs"] = {}
    return value


def semantic_ui_frame(sequence: int, minutes: int) -> dict[str, object]:
    value = frame(sequence, minutes)
    value["state"] = "semantic_ui"
    value["provenance"] = "native_semantic_ui_trace"
    value["observed_turn"] = None
    value["valid_actions"] = ["modal.acknowledge"]
    value["action_inputs"] = {"modal.acknowledge": "space"}
    return value


def activity_return_frame(sequence: int) -> dict[str, object]:
    return {
        "run_id": "live-proof",
        "frame_id": f"live-proof:activity-return:{sequence}",
        "state": "activity_resumed",
        "observed_turn": None,
        "provenance": "native_activity_distraction_return",
        "resolved_action": "IGNORE",
        "valid_actions": [],
        "action_inputs": {},
    }


def bound(start: int | None = None, maximum: int = 60, *, progress_required: bool = True) -> dict[str, object]:
    value: dict[str, object] = {
        "basis": "scheduler_boundary",
        "source": "production next-eligible minute minus current game minute",
        "unit": "game_minutes",
        "maximum": maximum,
        "progress_required": progress_required,
    }
    if start is not None:
        value["start"] = start
    return value


def r019_fixture_actor(**overrides: object) -> dict[str, object]:
    actor: dict[str, object] = {
        "fixture_actor_id": "r019-zombie-dog-positive-progress-v1",
        "typeid": "mon_zombie_dog", "faction": "zombie",
        "friendly": 0, "aggro_character": True,
    }
    actor.update(overrides)
    return actor


class LiveSessionTest(unittest.TestCase):
    def service(
        self, frames: list[dict[str, object]], *, evidence: dict[str, object] | None = None,
        binding: list[str] | None = None, missing_receipt: bool = False,
        r019_timed_entry: dict[str, object] | None = None,
        diagnostic_terminal: dict[str, object] | None = None,
    ) -> tuple[cockpit.CockpitService, list[dict[str, object]]]:
        index = [0]
        finals: list[dict[str, object]] = []
        current_binding = binding or ["binding-a"]

        def dispatch(issuing: dict[str, object], action_id: str) -> dict[str, object]:
            if missing_receipt:
                return {}
            index[0] += 1
            return {
                "accepted": True,
                "native_receipt": {
                    "frame_id": issuing["frame_id"], "action_id": action_id, "accepted": True,
                    **({"semantic_ui_instance_id": str(issuing["frame_id"]).split(
                        ":semantic-ui:", 1)[1].split(":", 1)[0]}
                       if ":semantic-ui:" in str(issuing["frame_id"]) else {}),
                },
                "next_frame": frames[index[0]],
                "_next_frame": frames[index[0]],
            }

        channel = cockpit.CockpitRunChannel(
            lambda: frames[index[0]], dispatch,
            read_evidence=lambda: evidence or {
                "receipt_count": index[0],
                "latest_receipt": None if index[0] == 0 else {
                    "action_id": "world.wait", "accepted": True,
                },
                "latest_transition": {
                    "sequence": index[0], "kind": "scheduler_progress",
                },
                "actor_owners": [{"actor_id": "4", "owner": "local"}],
                "persistence": "unavailable",
                "evidence_refs": [f"semantic.steps.jsonl#receipt={index[0]}"],
            },
            binding_id="binding-a",
            read_binding_id=lambda: current_binding[0],
            finalize_session=lambda report: finals.append(dict(report)) or {
                "cleanup": {"status": "terminated"},
                "final_report_ref": "cockpit.live.final.json",
            },
            enforce_continuation_bounds=True,
            r019_timed_entry=r019_timed_entry,
            diagnostic_terminal=diagnostic_terminal,
        )
        service = cockpit.CockpitService(run_channel=channel)
        service._test_frame_index = index
        return service, finals

    def test_progressing_observation_stays_live_and_worker_explicitly_finishes(self) -> None:
        service, finals = self.service([frame(1, 100), frame(2, 101), frame(3, 102, entity_dx=3)])
        first = service.call({"action": "game.observe"})["result"]
        self.assertEqual(first["minimap"]["schema"], "caol-native-minimap-v1")
        entity_handle = first["visible_entities"][0]["handle"]
        public = json.dumps(first).lower()
        self.assertNotIn("raw implementation detail", public)
        self.assertNotIn("incidental screen text", public)

        continued = service.call({
            "action": "run.continue", "observation_id": first["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        self.assertTrue(continued["ok"])
        acted = service.call({
            "action": "game.act", "observation_id": first["observation_id"],
            "action_id": "world.wait",
        })
        self.assertTrue(acted["ok"])
        self.assertEqual(service.call({"action": "run.status"})["result"]["state"], "active")

        second = acted["observation"]
        self.assertEqual(second["visible_entities"][0]["handle"], entity_handle)
        self.assertEqual(second["delta"]["game_minutes"], {"before": 100, "after": 101})
        service.call({
            "action": "run.continue", "observation_id": second["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(101),
        })
        acted_again = service.call({
            "action": "game.act", "observation_id": second["observation_id"],
            "action_id": "world.wait",
        })
        finished = service.call({
            "action": "run.finish",
            "observation_id": acted_again["observation"]["observation_id"],
            "stop_reason": "target_predicate_proved",
            "unused_authority": "remaining wait authority declined",
        })
        self.assertTrue(finished["ok"])
        self.assertEqual(finished["result"]["cleanup"]["status"], "terminated")
        self.assertEqual(len(finals), 1)
        self.assertEqual(finals[0]["stop_reason"], "target_predicate_proved")

    def test_r019_timed_entry_requires_actual_hostile_frame_before_wait(self) -> None:
        initial = frame(1, 100)
        initial["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:dog"}, "kind": "monster",
            "name": "zombie dog", "attitude": "hostile", "dx": 6, "dy": -1,
            **r019_fixture_actor(),
        }]
        wait_menu = wait_activity_frame(2, 100)
        service, _ = self.service([initial, wait_menu], r019_timed_entry={
            "target_offset_ms": [6, -1, 0], "dangerous_proximity": 5,
            "maximum_boundary_entry_steps": 1,
            "fixture_actor": r019_fixture_actor(),
        })
        observed = service.call({"action": "game.observe"})["result"]
        denied = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(denied["error"], "r019_live_timed_entry_qualification_required")
        qualified = service.call({
            "action": "game.qualify_r019_timed_entry", "observation_id": observed["observation_id"],
        })
        self.assertTrue(qualified["ok"])
        self.assertEqual(qualified["result"]["target_offset_ms"], [6, -1, 0])
        service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        accepted = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertTrue(accepted["ok"])

    def test_r019_timed_entry_rejects_neutral_fixture_actor(self) -> None:
        initial = frame(1, 100)
        initial["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:dog"}, "kind": "monster",
            "name": "zombie dog", "attitude": "neutral", "dx": 6, "dy": -1,
            **r019_fixture_actor(),
        }]
        service, finals = self.service([initial], r019_timed_entry={
            "target_offset_ms": [6, -1, 0], "dangerous_proximity": 5,
            "maximum_boundary_entry_steps": 1,
            "fixture_actor": r019_fixture_actor(),
        })
        observed = service.call({"action": "game.observe"})["result"]
        rejected = service.call({
            "action": "game.qualify_r019_timed_entry", "observation_id": observed["observation_id"],
        })
        self.assertFalse(rejected["ok"])
        self.assertEqual(rejected["error"], "r019_live_timed_entry_unqualified")
        self.assertEqual(finals[0]["stop_reason"], "r019_live_timed_entry_unqualified")

    def test_r019_timed_entry_rejects_fixture_identity_drift(self) -> None:
        initial = frame(1, 100)
        initial["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:replacement"}, "kind": "monster",
            "name": "zombie dog", "attitude": "hostile", "dx": 6, "dy": -1,
            **r019_fixture_actor(fixture_actor_id="replacement"),
        }]
        service, finals = self.service([initial], r019_timed_entry={
            "target_offset_ms": [6, -1, 0], "dangerous_proximity": 5,
            "maximum_boundary_entry_steps": 1, "fixture_actor": r019_fixture_actor(),
        })
        observed = service.call({"action": "game.observe"})["result"]
        rejected = service.call({
            "action": "game.qualify_r019_timed_entry", "observation_id": observed["observation_id"],
        })
        self.assertEqual(rejected["error"], "r019_live_timed_entry_unqualified")
        self.assertEqual(finals[0]["stop_reason"], "r019_live_timed_entry_unqualified")

    def test_r019_timed_entry_rejects_fixture_coordinate_drift(self) -> None:
        initial = frame(1, 100)
        initial["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:dog"}, "kind": "monster",
            "name": "zombie dog", "attitude": "hostile", "dx": 5, "dy": -2,
            **r019_fixture_actor(),
        }]
        service, finals = self.service([initial], r019_timed_entry={
            "target_offset_ms": [6, -1, 0], "dangerous_proximity": 5,
            "maximum_boundary_entry_steps": 1, "fixture_actor": r019_fixture_actor(),
        })
        observed = service.call({"action": "game.observe"})["result"]
        rejected = service.call({
            "action": "game.qualify_r019_timed_entry", "observation_id": observed["observation_id"],
        })
        self.assertEqual(rejected["error"], "r019_live_timed_entry_unqualified")
        self.assertEqual(finals[0]["stop_reason"], "r019_live_timed_entry_unqualified")

    def test_r019_projection_receipt_accepts_only_the_saved_to_live_chain(self) -> None:
        initial = frame(1, 100)
        initial["observation"]["avatar"]["absolute_ms"] = [3844, 957, 0]
        initial["observation"]["visible_entities"] = [{
            "identity": {"kind": "monster", "id": "process:dog"}, "kind": "monster",
            "name": "zombie dog", "attitude": "hostile", "dx": 4, "dy": 6,
            "absolute_ms": [3848, 963, 0], **r019_fixture_actor(),
        }]
        contract = {
            "saved_offset_ms": [4, 6, 0],
            "saved_player_absolute_ms": [3844, 957, 0],
            "saved_actor_absolute_ms": [3848, 963, 0],
            "live_player_absolute_ms": [3844, 957, 0],
            "live_actor_absolute_ms": [3848, 963, 0],
            "target_offset_ms": [4, 6, 0], "dangerous_proximity": 5,
            "maximum_boundary_entry_steps": 1, "fixture_actor": r019_fixture_actor(),
        }
        service, _ = self.service([initial], r019_timed_entry=contract)
        observed = service.call({"action": "game.observe"})["result"]
        accepted = service.call({
            "action": "game.qualify_r019_timed_entry", "observation_id": observed["observation_id"],
        })
        self.assertTrue(accepted["ok"])
        projection = accepted["result"]["projection"]
        self.assertEqual(projection["status"], "accepted")
        self.assertEqual(projection["cockpit_relative_offset_ms"], [4, 6, 0])
        public = json.dumps(observed, sort_keys=True)
        self.assertNotIn("absolute_ms", public)

        for field, value, error in (
            ("live_player_absolute_ms", [3843, 957, 0], "stale_origin"),
            ("live_actor_absolute_ms", [3849, 963, 0], "actor_drift"),
            ("target_offset_ms", [4, 5, 0], "mismatched_projection"),
        ):
            changed = dict(contract)
            changed[field] = value
            control, finals = self.service([initial], r019_timed_entry=changed)
            control_observed = control.call({"action": "game.observe"})["result"]
            rejected = control.call({
                "action": "game.qualify_r019_timed_entry",
                "observation_id": control_observed["observation_id"],
            })
            self.assertEqual(rejected["error"], "r019_live_timed_entry_unqualified")
            self.assertIn(error, finals[0]["stop_detail"]["projection"]["errors"])

        for owner, value in (
            ("avatar", None),
            ("visible_entities", None),
            ("avatar", [3844, 957]),
            ("visible_entities", [3848, 963]),
        ):
            changed_frame = json.loads(json.dumps(initial))
            if owner == "avatar":
                if value is None:
                    changed_frame["observation"]["avatar"].pop("absolute_ms")
                else:
                    changed_frame["observation"]["avatar"]["absolute_ms"] = value
            elif value is None:
                changed_frame["observation"]["visible_entities"][0].pop("absolute_ms")
            else:
                changed_frame["observation"]["visible_entities"][0]["absolute_ms"] = value
            control, finals = self.service([changed_frame], r019_timed_entry=contract)
            control_observed = control.call({"action": "game.observe"})["result"]
            rejected = control.call({
                "action": "game.qualify_r019_timed_entry",
                "observation_id": control_observed["observation_id"],
            })
            self.assertEqual(rejected["error"], "r019_live_timed_entry_unqualified")
            self.assertEqual(finals[0]["stop_detail"]["projection"], {
                "status": "rejected", "reason": "missing_or_malformed_projection_input",
            })

    def test_jsonl_worker_route_uses_public_minimap_frame_to_continue_and_finish(self) -> None:
        service, finals = self.service([frame(1, 100), frame(2, 101)])
        requests = [
            {"action": "game.observe"},
            {
                "action": "run.continue", "observation_id": "live-proof:1",
                "expected_signal": "game_minutes", "bound": bound(),
            },
            {
                "action": "game.act", "observation_id": "live-proof:1",
                "action_id": "world.wait",
            },
            {
                "action": "run.finish", "observation_id": "live-proof:2",
                "stop_reason": "target_predicate_proved",
                "unused_authority": "remaining native actions declined",
            },
        ]
        source = io.StringIO("".join(json.dumps(request) + "\n" for request in requests))
        output = io.StringIO()
        status = startup_harness.serve_cockpit_live(service, source, output)
        results = [json.loads(line) for line in output.getvalue().splitlines()]

        self.assertEqual(status, 0)
        self.assertEqual(results[0]["result"]["minimap"]["schema"], "caol-native-minimap-v1")
        self.assertTrue(results[1]["ok"])
        self.assertEqual(results[2]["observation"]["game_minutes"], 101)
        self.assertEqual(results[3]["result"]["stop_reason"], "target_predicate_proved")
        self.assertEqual(len(finals), 1)

    def test_eof_persists_an_explicit_fail_closed_terminal(self) -> None:
        service, finals = self.service([frame(1, 100)])
        output = io.StringIO()

        status = startup_harness.serve_cockpit_live(service, io.StringIO(), output)
        results = [json.loads(line) for line in output.getvalue().splitlines()]

        self.assertEqual(status, 1)
        self.assertEqual(results[-1]["error"], "client_disconnected")
        self.assertEqual(len(finals), 1)
        self.assertEqual(finals[0]["stop_reason"], "client_disconnected")
        self.assertEqual(service.run_channel.status()["state"], "finished")

    def test_continuation_derives_start_from_its_current_observation(self) -> None:
        service, _ = self.service([frame(1, 8460), frame(2, 8461)])
        observed = service.call({"action": "game.observe"})["result"]

        continued = service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(),
        })

        self.assertTrue(continued["ok"])
        self.assertEqual(continued["result"]["start"], 8460.0)

    def test_continuation_rejects_mismatched_or_stale_start(self) -> None:
        service, _ = self.service([frame(1, 8460), frame(2, 8461)])
        observed = service.call({"action": "game.observe"})["result"]

        self.assertEqual(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(8400),
        })["error"], "bound_start_does_not_match_current_signal")
        self.assertEqual(service.call({
            "action": "run.continue", "observation_id": "live-proof:stale",
            "expected_signal": "game_minutes", "bound": bound(),
        })["error"], "unknown_or_stale_observation")

    def test_no_progress_and_bound_exhaustion_stop_without_relaunch(self) -> None:
        no_progress, finals = self.service([frame(1, 100), frame(2, 100)])
        observed = no_progress.call({"action": "game.observe"})["result"]
        no_progress.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        stopped = no_progress.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(stopped["error"], "proved_no_progress")
        self.assertEqual(len(finals), 1)

        exhausted, exhausted_finals = self.service([frame(1, 100), frame(2, 102)])
        observed = exhausted.call({"action": "game.observe"})["result"]
        exhausted.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100, maximum=1),
        })
        stopped = exhausted.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(stopped["error"], "derived_bound_exhausted")
        self.assertEqual(len(exhausted_finals), 1)

    def test_exact_bound_completion_permits_a_fresh_independent_continuation(self) -> None:
        service, finals = self.service([frame(1, 100), frame(2, 101), frame(3, 102)])
        first = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": first["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100, maximum=1),
        })["ok"])
        first_action = service.call({
            "action": "game.act", "observation_id": first["observation_id"],
            "action_id": "world.wait",
        })
        self.assertTrue(first_action["ok"])
        self.assertEqual(service.call({"action": "run.status"})["result"]["state"], "active")

        second = first_action["observation"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": second["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(101, maximum=1),
        })["ok"])
        self.assertTrue(service.call({
            "action": "game.act", "observation_id": second["observation_id"],
            "action_id": "world.wait",
        })["ok"])
        self.assertEqual(finals, [])

    def test_native_wait_activity_keeps_continuation_until_later_world_completion(self) -> None:
        service, finals = self.service([
            frame(1, 100), wait_activity_frame(2, 100), frame(3, 101),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        pending = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertTrue(pending["ok"])
        self.assertEqual(pending["continuation"]["state"], "awaiting_native_completion")
        self.assertEqual(service.call({"action": "run.status"})["result"]["state"], "active")
        self.assertEqual(finals, [])
        self.assertEqual(service.call({
            "action": "run.finish",
            "observation_id": pending["observation"]["observation_id"],
            "stop_reason": "premature",
            "unused_authority": "none",
        }), {"ok": False, "error": "continuation_incomplete"})

        service._test_frame_index[0] = 2
        completed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(completed["continuation"], {
            "state": "completed", "before": 100.0, "after": 101.0,
        })
        self.assertEqual(service.call({"action": "run.status"})["result"]["continuation"], {})
        self.assertEqual(finals, [])

    def test_activity_ignore_return_waits_for_later_avatar_observation(self) -> None:
        activity = frame(2, 100)
        activity["state"] = "activity_distraction"
        activity["provenance"] = "native_activity_distraction_query"
        activity["observed_turn"] = None
        activity["valid_actions"] = ["activity.ignore"]
        activity["action_inputs"] = {"activity.ignore": "I"}
        service, finals = self.service([
            activity, activity_return_frame(3), frame(4, 101),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        service.run_channel._continuation = {
            **service.run_channel._continuation,
            "phase": "awaiting_native_completion",
            "activity_frame_id": observed["observation_id"],
        }
        ignored = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "activity.ignore",
        })
        self.assertTrue(ignored["ok"])
        self.assertNotIn("observation", ignored)
        self.assertEqual(ignored["continuation"], {"state": "awaiting_native_completion"})
        self.assertEqual(finals, [])

        service._test_frame_index[0] = 2
        completed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(completed["continuation"], {
            "state": "completed", "before": 100.0, "after": 101.0,
        })

    def test_disabled_master_stops_at_hostile_interruption_without_ignore(self) -> None:
        wait_menu = frame(2, 100)
        wait_menu["state"] = "wait_mode_choice"
        wait_menu["valid_actions"] = ["wait.duration_menu"]
        wait_menu["action_inputs"] = {"wait.duration_menu": "w"}
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        interruption = frame(4, 101)
        interruption["state"] = "activity_distraction"
        interruption["provenance"] = "native_activity_distraction_query"
        interruption["activity_type"] = "hostile_spotted_near"
        interruption["calendar_time_source"] = "native_activity_distraction_query"
        interruption["observed_turn"] = None
        interruption["valid_actions"] = ["activity.ignore"]
        interruption["action_inputs"] = {"activity.ignore": "I"}
        service, finals = self.service([frame(1, 100), wait_menu, duration, interruption])

        disabled = service.call({"action": "game.keep_watch", "keep_watch": {
            "master_enabled": False, "enabled": True, "target_game_minutes": 101,
            "bound": bound(100, 1), "recipe": ["world.wait"],
        }})
        self.assertEqual(disabled["error"], "keep_watch_master_disabled_use_primitive_actions")
        observed = service.call({"action": "game.observe"})["result"]
        for action in ("world.wait", "wait.duration_menu", "wait.1m"):
            self.assertTrue(service.call({
                "action": "run.continue", "observation_id": observed["observation_id"],
                "expected_signal": "game_minutes", "bound": bound(100, 1),
            })["ok"])
            observed = service.call({
                "action": "game.act", "observation_id": observed["observation_id"],
                "action_id": action,
            })["observation"]
        self.assertEqual(observed["active_interruption"]["type"], "hostile_spotted_near")
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "hostile_spotted_near", "unused_authority": "revoked",
            "r019_acceptance_matrix": {
                "role": "off:master_enabled", "clean_start_identity": "fixture:clean:100",
                "source_identity": "native-spawn:mon_zombie_dog:6,0,0",
            },
        })
        self.assertTrue(finished["ok"])
        receipt = finished["result"]["stop_detail"]["r019_hostile_stop_receipt"]
        self.assertFalse(receipt["advertised_action_dispatched"])
        self.assertEqual(receipt["pre_wait_game_minutes"], 100.0)
        self.assertEqual(receipt["interruption_game_minutes"], 101)
        self.assertEqual(receipt["partial_progress"], 1.0)
        self.assertGreater(receipt["partial_progress"], 0.0)
        self.assertEqual(len(finals), 1)

    def test_disabled_master_hostile_stop_fails_closed_without_positive_progress(self) -> None:
        wait_menu = frame(2, 100)
        wait_menu["state"] = "wait_mode_choice"
        wait_menu["valid_actions"] = ["wait.duration_menu"]
        wait_menu["action_inputs"] = {"wait.duration_menu": "w"}
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        interruption = frame(4, 100)
        interruption["state"] = "activity_distraction"
        interruption["provenance"] = "native_activity_distraction_query"
        interruption["activity_type"] = "hostile_spotted_near"
        interruption["calendar_time_source"] = "native_activity_distraction_query"
        interruption["observed_turn"] = None
        interruption["valid_actions"] = ["activity.ignore"]
        interruption["action_inputs"] = {"activity.ignore": "I"}
        service, _ = self.service([frame(1, 100), wait_menu, duration, interruption])
        service.call({"action": "game.keep_watch", "keep_watch": {
            "master_enabled": False, "enabled": True, "target_game_minutes": 101,
            "bound": bound(100, 1), "recipe": ["world.wait"],
        }})
        observed = service.call({"action": "game.observe"})["result"]
        for action in ("world.wait", "wait.duration_menu", "wait.1m"):
            service.call({"action": "run.continue", "observation_id": observed["observation_id"],
                          "expected_signal": "game_minutes", "bound": bound(100, 1)})
            observed = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                     "action_id": action})["observation"]
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "hostile_spotted_near", "unused_authority": "revoked",
            "r019_acceptance_matrix": {
                "role": "off:master_enabled", "clean_start_identity": "fixture:clean:100",
                "source_identity": "native-spawn:mon_zombie_dog:12,0,0",
            },
        })
        self.assertFalse(finished["ok"])
        self.assertEqual(finished["error"], "r019_hostile_partial_progress_unproved")

    def test_disabled_master_hostile_stop_fails_closed_without_native_calendar_time(self) -> None:
        wait_menu = frame(2, 100)
        wait_menu["state"] = "wait_mode_choice"
        wait_menu["valid_actions"] = ["wait.duration_menu"]
        wait_menu["action_inputs"] = {"wait.duration_menu": "w"}
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        interruption = frame(4, 101)
        interruption["state"] = "activity_distraction"
        interruption["provenance"] = "native_activity_distraction_query"
        interruption["activity_type"] = "hostile_spotted_near"
        interruption["observed_turn"] = None
        interruption.pop("game_minutes")
        interruption["valid_actions"] = ["activity.ignore"]
        interruption["action_inputs"] = {"activity.ignore": "I"}
        service, _ = self.service([frame(1, 100), wait_menu, duration, interruption])
        service.call({"action": "game.keep_watch", "keep_watch": {
            "master_enabled": False, "enabled": True, "target_game_minutes": 101,
            "bound": bound(100, 1), "recipe": ["world.wait"],
        }})
        observed = service.call({"action": "game.observe"})["result"]
        for action in ("world.wait", "wait.duration_menu", "wait.1m"):
            service.call({"action": "run.continue", "observation_id": observed["observation_id"],
                          "expected_signal": "game_minutes", "bound": bound(100, 1)})
            observed = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                     "action_id": action})["observation"]
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "hostile_spotted_near", "unused_authority": "revoked",
            "r019_acceptance_matrix": {
                "role": "off:master_enabled", "clean_start_identity": "fixture:clean:100",
                "source_identity": "native-spawn:mon_zombie_dog:6,0,0",
            },
        })
        self.assertFalse(finished["ok"])
        self.assertEqual(finished["error"], "r019_hostile_partial_progress_unproved")

    def test_disabled_master_hostile_stop_fails_closed_for_contradictory_calendar_time(self) -> None:
        wait_menu = frame(2, 100)
        wait_menu["state"] = "wait_mode_choice"
        wait_menu["valid_actions"] = ["wait.duration_menu"]
        wait_menu["action_inputs"] = {"wait.duration_menu": "w"}
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1m"]
        duration["action_inputs"] = {"wait.1m": "1"}
        interruption = frame(4, 102)
        interruption["state"] = "activity_distraction"
        interruption["provenance"] = "native_activity_distraction_query"
        interruption["activity_type"] = "hostile_spotted_near"
        interruption["calendar_time_source"] = "native_activity_distraction_query"
        interruption["observed_turn"] = None
        interruption["valid_actions"] = ["activity.ignore"]
        interruption["action_inputs"] = {"activity.ignore": "I"}
        service, _ = self.service([frame(1, 100), wait_menu, duration, interruption])
        service.call({"action": "game.keep_watch", "keep_watch": {
            "master_enabled": False, "enabled": True, "target_game_minutes": 101,
            "bound": bound(100, 1), "recipe": ["world.wait"],
        }})
        observed = service.call({"action": "game.observe"})["result"]
        for action in ("world.wait", "wait.duration_menu", "wait.1m"):
            service.call({"action": "run.continue", "observation_id": observed["observation_id"],
                          "expected_signal": "game_minutes", "bound": bound(100, 1)})
            observed = service.call({"action": "game.act", "observation_id": observed["observation_id"],
                                     "action_id": action})["observation"]
        finished = service.call({
            "action": "run.finish", "observation_id": observed["observation_id"],
            "stop_reason": "hostile_spotted_near", "unused_authority": "revoked",
            "r019_acceptance_matrix": {
                "role": "off:master_enabled", "clean_start_identity": "fixture:clean:100",
                "source_identity": "native-spawn:mon_zombie_dog:6,0,0",
            },
        })
        self.assertFalse(finished["ok"])
        self.assertEqual(finished["error"], "r019_hostile_partial_progress_unproved")

    def test_semantic_ui_recovery_preserves_native_wait_continuation(self) -> None:
        service, finals = self.service([
            frame(1, 100), wait_activity_frame(2, 100),
            semantic_ui_frame(3, 120), wait_activity_frame(4, 120), frame(5, 121),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        pending = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(pending["continuation"], {"state": "awaiting_native_completion"})
        service._test_frame_index[0] = 2
        modal = service.call({"action": "game.observe"})["result"]
        self.assertEqual(modal["advertised_actions"], ["modal.acknowledge"])
        recovered = service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        })
        self.assertEqual(recovered["continuation"], {"state": "awaiting_native_completion"})
        service._test_frame_index[0] = 4
        completed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(completed["continuation"], {
            "state": "completed", "before": 100.0, "after": 121.0,
        })
        self.assertEqual(finals, [])

    def test_duration_dispatch_modal_recovery_carries_the_same_wait_continuation(self) -> None:
        first = frame(1, 100)
        menu = frame(2, 100)
        menu["state"] = "wait_mode_choice"
        menu["valid_actions"] = ["wait.duration_menu"]
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.1h"]
        service, finals = self.service([
            first, menu, duration, semantic_ui_frame(4, 100),
            wait_activity_frame(5, 100), frame(6, 160),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        opened = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        selected = service.call({
            "action": "game.act", "observation_id": opened["observation"]["observation_id"],
            "action_id": "wait.duration_menu",
        })
        modal = service.call({
            "action": "game.act", "observation_id": selected["observation"]["observation_id"],
            "action_id": "wait.1h",
        })
        self.assertEqual(modal["observation"]["advertised_actions"], ["modal.acknowledge"])
        self.assertEqual(modal["continuation"], {"state": "awaiting_native_completion"})
        recovered = service.call({
            "action": "game.act", "observation_id": modal["observation"]["observation_id"],
            "action_id": "modal.acknowledge",
        })
        self.assertTrue(recovered["ok"])
        self.assertEqual(recovered["continuation"], {"state": "awaiting_native_completion"})
        service._test_frame_index[0] = 5
        completed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(completed["continuation"], {
            "state": "completed", "before": 100.0, "after": 160.0,
        })
        self.assertEqual(finals, [])

    def test_modal_recovery_rejects_missing_wrong_frame_duplicate_and_unsupported_actions(self) -> None:
        service, _ = self.service([semantic_ui_frame(1, 100), frame(2, 100)])
        modal = service.call({"action": "game.observe"})["result"]
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        }), {"ok": False, "error": "continuation_bound_required"})

        service, _ = self.service([frame(1, 100), semantic_ui_frame(2, 100), frame(3, 101)])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        service.run_channel._continuation = {
            **service.run_channel._continuation, "phase": "awaiting_native_completion",
            "activity_frame_id": observed["observation_id"],
        }
        service._test_frame_index[0] = 1
        modal = service.call({"action": "game.observe"})["result"]
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": "wrong-frame",
            "action_id": "modal.acknowledge",
        })["error"], "unknown_or_stale_observation")
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "unsupported.modal.action",
        })["error"], "action_not_advertised")
        recovered = service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        })
        self.assertTrue(recovered["ok"])
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        })["error"], "duplicate_submission")

    def test_current_modal_observation_authorizes_only_its_advertised_acknowledgement(self) -> None:
        modal_frame = semantic_ui_frame(2, 100)
        modal_frame["safe_recovery"] = {
            "modal_id": "eoc-1", "actions": ["modal.acknowledge"],
        }
        service, _ = self.service([
            frame(1, 100), modal_frame, frame(3, 101),
        ])
        initial = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": initial["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        service.run_channel._continuation = {
            **service.run_channel._continuation, "phase": "awaiting_native_completion",
            "activity_frame_id": initial["observation_id"],
        }
        service._test_frame_index[0] = 1
        modal = service.call({"action": "game.observe"})["result"]
        self.assertEqual(modal["advertised_actions"], ["modal.acknowledge"])
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": "foreign-run:semantic-ui:eoc-1:2",
            "action_id": "modal.acknowledge",
        })["error"], "unknown_or_stale_observation")
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.dismiss",
        })["error"], "action_not_advertised")
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge", "recovery": {"modal_id": "changed-modal"},
        })["error"], "stale_native_modal")
        recovered = service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        })
        self.assertTrue(recovered["ok"])

    def test_descriptor_bound_scheduler_diagnostic_terminal_seals_only_due_popup_acknowledgement(self) -> None:
        initial = frame(1, 100)
        popup = semantic_ui_frame(2, 100)
        popup["frame_id"] = "live-proof:semantic-ui:eoc-1:2"
        after_ack = frame(3, 100)
        after_ack.update({
            "state": "activity_distraction",
            "provenance": "native_activity_distraction_query",
            "activity_type": "eoc",
            "observed_turn": 30,
            "valid_actions": ["activity.ignore"],
        })
        terminal = {
            "kind": "r019_scheduler_due_popup_acknowledgement",
            "scheduler_eoc": "EOC_OPENCLAW_R019_SAFE_POPUP",
            "popup_action": "modal.acknowledge",
            "stop_reason": "r019_scheduler_due_popup_acknowledged",
            "gameplay_credit": False,
        }
        evidence = {
            "receipt_count": 4,
            "first_divergence": None,
            "contradictory_evidence": [],
            "unsafe": False,
            "scheduler_trace": [{
                "eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 30,
                "current_turn": 30, "decision": "due", "outcome": "before_activation",
            }],
        }
        service, _ = self.service([initial, popup, after_ack], evidence=evidence,
                                  diagnostic_terminal=terminal)
        first = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": first["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100, 1),
        })["ok"])
        service.run_channel._continuation.update({"phase": "awaiting_native_completion"})
        service._test_frame_index[0] = 1
        modal = service.call({"action": "game.observe"})["result"]
        acknowledged = service.call({
            "action": "game.act", "observation_id": modal["observation_id"],
            "action_id": "modal.acknowledge",
        })
        finished = service.call({
            "action": "run.finish", "observation_id": acknowledged["observation"]["observation_id"],
            "stop_reason": "r019_scheduler_due_popup_acknowledged",
            "unused_authority": "diagnostic_no_feature_credit",
        })
        self.assertTrue(finished["ok"])
        self.assertEqual(finished["result"]["stop_detail"]["diagnostic_terminal_receipt"], {
            "schema": "caol-r019-scheduler-diagnostic-terminal-receipt-v1",
            "scheduler_eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 30,
            "popup_observation_id": "live-proof:semantic-ui:eoc-1:2",
            "popup_instance_id": "eoc-1", "acknowledgement_action": "modal.acknowledge",
            "gameplay_credit": False,
        })

    def test_scheduler_diagnostic_terminal_rejects_duplicate_or_feature_credit_evidence(self) -> None:
        for label, evidence, matrix in (
            ("duplicate", {"scheduler_trace": [
                {"eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 30, "current_turn": 30, "decision": "due"},
                {"eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 30, "current_turn": 30, "decision": "due"},
            ]}, None),
            ("feature_credit", {"scheduler_trace": [
                {"eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "due_turn": 30, "current_turn": 30, "decision": "due"},
            ]}, {"role": "guarded"}),
        ):
            with self.subTest(label=label):
                initial = frame(1, 100)
                popup = semantic_ui_frame(2, 100)
                popup["frame_id"] = "live-proof:semantic-ui:eoc-1:2"
                after_ack = frame(3, 100)
                after_ack.update({"state": "activity_distraction", "provenance": "native_activity_distraction_query",
                                  "activity_type": "eoc", "observed_turn": 30, "valid_actions": ["activity.ignore"]})
                service, _ = self.service([initial, popup, after_ack], evidence={
                    "receipt_count": 4, "first_divergence": None, "contradictory_evidence": [],
                    "unsafe": False, **evidence,
                }, diagnostic_terminal={
                    "kind": "r019_scheduler_due_popup_acknowledgement",
                    "scheduler_eoc": "EOC_OPENCLAW_R019_SAFE_POPUP", "popup_action": "modal.acknowledge",
                    "stop_reason": "r019_scheduler_due_popup_acknowledged", "gameplay_credit": False,
                })
                first = service.call({"action": "game.observe"})["result"]
                service.call({"action": "run.continue", "observation_id": first["observation_id"],
                              "expected_signal": "game_minutes", "bound": bound(100, 1)})
                service.run_channel._continuation.update({"phase": "awaiting_native_completion"})
                service._test_frame_index[0] = 1
                modal = service.call({"action": "game.observe"})["result"]
                acknowledged = service.call({"action": "game.act", "observation_id": modal["observation_id"],
                                              "action_id": "modal.acknowledge"})
                finished = service.call({
                    "action": "run.finish", "observation_id": acknowledged["observation"]["observation_id"],
                    "stop_reason": "r019_scheduler_due_popup_acknowledged", "unused_authority": "none",
                    **({"r019_acceptance_matrix": matrix} if matrix is not None else {}),
                })
                self.assertFalse(finished["ok"])
                self.assertIn(finished["error"], {"continuation_incomplete", "r019_role_does_not_match_live_transcript"})

    def test_continuation_rejects_a_modal_from_a_different_run(self) -> None:
        first = frame(1, 100)
        wrong_run_modal = semantic_ui_frame(2, 100)
        wrong_run_modal["run_id"] = "different-run"
        service, finals = self.service([first, wrong_run_modal])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })["ok"])
        service.run_channel._continuation = {
            **service.run_channel._continuation, "phase": "awaiting_native_completion",
            "activity_frame_id": observed["observation_id"],
        }
        service._test_frame_index[0] = 1
        self.assertEqual(service.call({"action": "game.observe"}), {
            "ok": False, "error": "continuation belongs to a different run",
        })
        self.assertEqual(service.call({"action": "run.status"})["result"]["final"]["stop_reason"],
                         "continuation_wrong_run")
        self.assertEqual(len(finals), 1)

    def test_wait_menu_navigation_carries_bound_until_native_wait_dispatch(self) -> None:
        first = frame(1, 100)
        menu = frame(2, 100)
        menu["state"] = "wait_mode_choice"
        menu["valid_actions"] = ["wait.duration_menu"]
        duration = frame(3, 100)
        duration["state"] = "wait_duration_choice"
        duration["valid_actions"] = ["wait.6h"]
        service, finals = self.service([
            first, menu, duration, wait_activity_frame(4, 100), frame(5, 460),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertTrue(service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100, maximum=720),
        })["ok"])
        opened = service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(opened["continuation"], {"state": "awaiting_wait_dispatch"})
        selected = service.call({
            "action": "game.act", "observation_id": opened["observation"]["observation_id"],
            "action_id": "wait.duration_menu",
        })
        self.assertEqual(selected["continuation"], {"state": "awaiting_wait_dispatch"})
        dispatched = service.call({
            "action": "game.act", "observation_id": selected["observation"]["observation_id"],
            "action_id": "wait.6h",
        })
        self.assertEqual(dispatched["continuation"], {"state": "awaiting_native_completion"})
        service._test_frame_index[0] = 4
        completed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(completed["continuation"], {
            "state": "completed", "before": 100.0, "after": 460.0,
        })
        self.assertEqual(finals, [])

    def test_delayed_native_completion_without_progress_fails_closed(self) -> None:
        service, finals = self.service([
            frame(1, 100), wait_activity_frame(2, 100), frame(3, 100),
        ])
        observed = service.call({"action": "game.observe"})["result"]
        service.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        self.assertTrue(service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })["ok"])
        service._test_frame_index[0] = 2
        self.assertEqual(service.call({"action": "game.observe"}), {
            "ok": False, "error": "live session is finished",
        })
        self.assertEqual(service.call({"action": "run.status"})["result"]["final"]["stop_reason"], "proved_no_progress")
        self.assertEqual(len(finals), 1)

    def test_binding_receipt_and_unsafe_failures_are_terminal(self) -> None:
        binding = ["binding-a"]
        drifted, finals = self.service([frame(1, 100)], binding=binding)
        observed = drifted.call({"action": "game.observe"})["result"]
        binding[0] = "binding-b"
        stopped = drifted.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        self.assertEqual(stopped["error"], "binding_drift")
        self.assertEqual(len(finals), 1)

        missing, missing_finals = self.service(
            [frame(1, 100), frame(2, 101)], missing_receipt=True,
        )
        observed = missing.call({"action": "game.observe"})["result"]
        missing.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        stopped = missing.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })
        self.assertEqual(stopped["error"], "native_receipt_missing")
        self.assertEqual(len(missing_finals), 1)

        unsafe, unsafe_finals = self.service([frame(1, 100)], evidence={
            "unsafe": True,
            "first_divergence": {"kind": "hostile_contact", "detail": "avatar threatened"},
            "contradictory_evidence": [{"kind": "owner_mismatch"}],
        })
        observed = unsafe.call({"action": "game.observe"})["result"]
        self.assertEqual(observed["compact_log"]["first_divergence"]["kind"], "hostile_contact")
        self.assertEqual(observed["compact_log"]["contradictory_evidence"], [{"kind": "owner_mismatch"}])
        stopped = unsafe.call({
            "action": "run.continue", "observation_id": observed["observation_id"],
            "expected_signal": "game_minutes", "bound": bound(100),
        })
        self.assertEqual(stopped["error"], "unsafe_divergence")
        self.assertEqual(len(unsafe_finals), 1)

    def test_stale_frame_cannot_authorize_continuation_or_action(self) -> None:
        service, _ = self.service([frame(1, 100), frame(2, 101)])
        observed = service.call({"action": "game.observe"})["result"]
        self.assertEqual(service.call({
            "action": "game.act", "observation_id": observed["observation_id"],
            "action_id": "world.wait",
        })["error"], "continuation_bound_required")
        self.assertEqual(service.call({
            "action": "run.continue", "observation_id": "stale-frame",
            "expected_signal": "game_minutes", "bound": bound(100),
        })["error"], "unknown_or_stale_observation")


if __name__ == "__main__":
    unittest.main()
