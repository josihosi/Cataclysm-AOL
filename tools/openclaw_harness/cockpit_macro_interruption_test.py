#!/usr/bin/env python3
"""Ordinary macro stops return control without destroying the native session."""
import unittest
import json

import cockpit
from cockpit_raw_wait_test import frame as wait_frame, bound as wait_bound
from cockpit_keep_watch_test import frame as watch_frame
import r023_relative_movement_test as movement
from r023_relative_movement_test import frame as move_frame, bound as move_bound


class MacroInterruptionTest(unittest.TestCase):
    def test_raw_wait_partial_prompt_releases_recipe_and_accepts_one_explicit_decision(self):
        start = wait_frame(1, 100)
        duration = wait_frame(2, 100, state="wait_duration_choice")
        duration["valid_actions"] = ["wait.1m"]
        prompt = wait_frame(3, 100.5, state="activity_distraction")
        prompt.update(provenance="native_activity_distraction_query", valid_actions=["activity.stop"])
        returned = {"run_id": start["run_id"], "frame_id": "raw-wait-proof:4",
                    "state": "activity_stopped", "provenance": "native_activity_distraction_return",
                    "valid_actions": []}
        frames = [start, duration, prompt, returned, wait_frame(5, 100.5)]
        index, actions, finals = [0], [], []

        def dispatch(issuing, action):
            actions.append(action)
            index[0] += 1
            return {"native_receipt": {"run_id": issuing["run_id"], "frame_id": issuing["frame_id"], "action_id": action,
                                       "accepted": True}, "_next_frame": frames[index[0]]}

        channel = cockpit.CockpitRunChannel(lambda: frames[index[0]], dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a",
                    enforce_continuation_bounds=True, finalize_session=lambda report: finals.append(report) or {})
        service = cockpit.CockpitService(run_channel=channel)
        stopped = service.call({"action": "game.raw_wait", "raw_wait": {
            "enabled": True, "target_game_minutes": 101, "bound": wait_bound(1),
            "recipe": ["world.wait", "wait.1m"]}})
        self.assertEqual(stopped["error"], "native_wait_interrupted")
        self.assertNotIn("final", stopped)
        self.assertEqual(stopped["result"]["partial_progress"], .5)
        self.assertEqual(stopped["result"]["unused_authority"], "released")
        self.assertEqual(channel.status()["continuation"], {})
        self.assertEqual(actions, ["world.wait", "wait.1m"])
        # A read of the same owner must preserve its explicit decision grant.
        current = service.call({"action": "game.observe"})["result"]
        decided = service.call({"action": "game.act", "observation_id": current["observation_id"],
                                "action_id": "activity.stop"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(channel.status()["continuation"], {})
        self.assertEqual(actions, ["world.wait", "wait.1m", "activity.stop"])
        index[0] = 4  # The game publishes its later world independently of input.
        self.assertEqual(service.call({"action": "game.observe"})["result"]["game_minutes"], 100.5)
        self.assertEqual(channel.status()["state"], "active")
        self.assertEqual(finals, [])

    def test_guarded_wait_danger_returns_owner_for_explicit_action(self):
        danger = watch_frame(1, 100, {"classification": "monster_spotted", "monster": True,
                            "danger": True, "damage": False})
        after = watch_frame(2, 101, {"classification": "clear", "monster": False,
                           "danger": False, "damage": False})
        frames, index, actions, finals = [danger, after], [0], [], []
        def dispatch(issuing, action):
            actions.append(action)
            index[0] += 1
            return {"native_receipt": {"run_id": issuing["run_id"], "frame_id": issuing["frame_id"], "action_id": action,
                                       "accepted": True}, "_next_frame": frames[index[0]]}
        channel = cockpit.CockpitRunChannel(lambda: frames[index[0]], dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a",
                    enforce_continuation_bounds=True, finalize_session=lambda report: finals.append(report) or {})
        service = cockpit.CockpitService(run_channel=channel)
        stopped = service.call({"action": "game.keep_watch", "keep_watch": {
            "enabled": True, "target_game_minutes": 101, "bound": wait_bound(1), "recipe": ["world.wait"]}})
        self.assertEqual(stopped["error"], "keep_watch_unsafe_condition")
        self.assertEqual(stopped["result"]["partial_progress"], 0)
        self.assertEqual(actions, [])
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "world.wait"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(actions, ["world.wait"])
        self.assertEqual(finals, [])

    def test_move_from_item_menu_without_position_preserves_native_cancel(self):
        # Live selected-1c3ab1bca1a74f48b7aa5bdce817f5fa cancelled Light where?
        # back to the lighter menu, then requested movement. No World owner
        # or coordinate existed, but that ordinary misuse terminated the game.
        for operation in ("move_relative", "raw_move_relative", "guarded_move_relative"):
            with self.subTest(operation=operation):
                menu = {"event": "surface_descriptor", "schema_version": 1,
                        "run_id": "menu-start-run", "surface_id": "item-menu", "frame_id": "menu:1",
                        "kind": "inventory_item_menu", "breadcrumbs": ["World", "Inventory", "lighter"],
                        "payload": {"item_name": "lighter", "item_uid": "70463"},
                        "valid_actions": [{"id": "inventory.item_menu.cancel", "stable_id": "",
                                           "label": "Cancel", "enabled": True}]}
                world = {**menu, "surface_id": "world", "frame_id": "world:2", "kind": "world",
                         "breadcrumbs": ["World"], "payload": {"avatar": '{"absolute_ms":[1,1,0]}'},
                         "valid_actions": [{"id": "world.move.east", "stable_id": "",
                                            "label": "East", "enabled": True}]}
                frames, index, actions, finals = [menu, world], [0], [], []

                def dispatch(issuing, action):
                    actions.append(action)
                    index[0] += 1
                    return {"native_receipt": {
                        "run_id": issuing["run_id"], "requested_run_id": issuing["run_id"],
                        "requested_frame_id": issuing["frame_id"], "action_id": action,
                        "requested_surface_id": issuing["surface_id"],
                        "consuming_surface_id": issuing["surface_id"], "accepted": True,
                    }, "_next_frame": frames[index[0]]}

                channel = cockpit.CockpitRunChannel(lambda: frames[index[0]], dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a",
                    enforce_continuation_bounds=True, finalize_session=lambda report: finals.append(report) or {})
                service = cockpit.CockpitService(run_channel=channel)
                stopped = service.call({"action": "game." + operation, operation: {
                    "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
                self.assertNotIn("final", stopped)
                self.assertEqual(stopped["result"]["partial_progress"], 0)
                self.assertIsNone(stopped["result"]["origin_absolute_ms"])
                self.assertIsNone(stopped["result"]["target_absolute_ms"])
                self.assertEqual(stopped["result"]["native_receipts"], [])
                self.assertEqual(actions, [])
                self.assertEqual(channel.status()["state"], "active")
                self.assertEqual(finals, [])
                current = stopped["result"]["terminal_observation"]
                self.assertEqual(current["surface"]["kind"], "inventory_item_menu")
                self.assertTrue(service.call({"action": "game.act", "observation_id": current["observation_id"],
                                             "action_id": "inventory.item_menu.cancel"})["ok"])
                self.assertEqual(actions, ["inventory.item_menu.cancel"])
                self.assertEqual(channel.status()["state"], "active")
                self.assertEqual(finals, [])

    def test_malformed_observation_revokes_surface_grants_then_fresh_look_recovers(self):
        current = {"run_id":"run-a", "frame_id":"frame-a", "event":"surface_descriptor",
                   "schema_version":1, "surface_id":"world-a", "kind":"world", "breadcrumbs":["World"],
                   "payload":{}, "valid_actions":[{"id":"world.pause", "stable_id":"", "label":"Pause", "enabled":True}]}
        frames, actions, finals = [current], [], []
        def dispatch(issuing, action):
            actions.append(action)
            successor = {**current, "frame_id":"frame-b", "surface_id":"world-b"}
            frames[0] = successor
            return {"native_receipt":{"run_id":"run-a", "requested_run_id":"run-a",
                    "requested_frame_id":issuing["frame_id"], "action_id":action,
                    "requested_surface_id":issuing["surface_id"], "consuming_surface_id":issuing["surface_id"],
                    "accepted":True}, "_next_frame":successor}
        channel = cockpit.CockpitRunChannel(lambda:frames[0], dispatch,
                    binding_id="binding-a", read_binding_id=lambda:"binding-a",
                    finalize_session=lambda report:finals.append(report) or {})
        service = cockpit.CockpitService(run_channel=channel)
        observed = service.call({"action":"game.observe"})["result"]
        frames[0] = {**current, "valid_actions":[{"id":"world.pause", "enabled":"untrusted"}]}
        failed = service.call({"action":"game.observe"})
        self.assertFalse(failed["ok"])
        self.assertEqual(len([entry for entry in channel._transcript if entry["kind"] == "operation_failure"]), 1)
        self.assertEqual(channel.status()["state"], "active")
        self.assertEqual(finals, [])
        frames[0] = current
        stale = service.call({"action":"game.act", "observation_id":observed["observation_id"], "action_id":"world.pause"})
        self.assertEqual(stale["error"], "duplicate_submission")
        self.assertEqual(actions, [])
        fresh = service.call({"action":"game.observe"})["result"]
        self.assertTrue(service.call({"action":"game.act", "observation_id":fresh["observation_id"], "action_id":"world.pause"})["ok"])
        self.assertEqual(actions, ["world.pause"])
        self.assertEqual(finals, [])

    def test_world_without_position_fails_without_ending_session(self):
        current = {"event": "surface_descriptor", "schema_version": 1,
                   "run_id": "missing-position-run", "surface_id": "world", "frame_id": "world:1",
                   "kind": "world", "breadcrumbs": ["World"], "payload": {},
                   "valid_actions": [{"id": "world.move.east", "stable_id": "",
                                      "label": "East", "enabled": True}]}
        helper = movement.RelativeMovementTest()
        service, actions, finals = helper.service([current])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        self.assertEqual(stopped["error"], "raw_move_relative_position_unavailable")
        self.assertEqual(actions, [])
        self.assertEqual(finals, [])

    def test_blocked_move_is_zero_progress_and_another_direction_works(self):
        helper = movement.RelativeMovementTest()
        service, actions, finals = helper.service([
            move_frame(1, [10, 20, 0]), move_frame(2, [10, 20, 0]), move_frame(3, [10, 19, 0])
        ], outcomes=["blocked", "moved"])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        self.assertEqual(stopped["error"], "raw_move_relative_blocked")
        self.assertEqual(stopped["result"]["partial_progress"], 0)
        self.assertEqual(stopped["result"]["terminal_observation"]["observation_id"], "r023-relative-proof:2")
        self.assertEqual(len(stopped["result"]["native_receipts"]), 1)
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "world.move.north"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(actions, ["world.move.east", "world.move.north"])
        self.assertEqual(finals, [])

    def test_movement_prompt_is_left_for_explicit_native_choice(self):
        helper = movement.RelativeMovementTest()
        prompt = move_frame(2, [11, 20, 0], state="semantic_ui", recovery_action="ui.dismiss")
        prompt["provenance"] = "native_semantic_ui_trace"
        service, actions, finals = helper.service([
            move_frame(1, [10, 20, 0]), prompt, move_frame(3, [11, 20, 0])
        ])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [2, 0], "bound": move_bound(2)}})
        self.assertEqual(stopped["error"], "raw_move_relative_interrupted")
        self.assertEqual(stopped["result"]["partial_progress"], 1)
        self.assertEqual(actions, ["world.move.east"])
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "ui.dismiss"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(finals, [])

    def test_native_prompt_without_avatar_facts_does_not_invent_position_or_kill_session(self):
        helper = movement.RelativeMovementTest()
        prompt = move_frame(2, [11, 20, 0], state="semantic_ui", recovery_action="ui.dismiss")
        prompt["provenance"] = "native_semantic_ui_trace"
        del prompt["observation"]
        service, actions, finals = helper.service([
            move_frame(1, [10, 20, 0]), prompt, move_frame(3, [11, 20, 0])
        ])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [2, 0], "bound": move_bound(2)}})
        self.assertEqual(stopped["error"], "raw_move_relative_interrupted")
        self.assertIsNone(stopped["result"]["terminal_absolute_ms"])
        self.assertEqual(stopped["result"]["last_confirmed_absolute_ms"], [11, 20, 0])
        self.assertEqual(stopped["result"]["partial_progress"], 1)
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "ui.dismiss"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(finals, [])

    def test_identity_change_after_interruption_revokes_without_input(self):
        helper = movement.RelativeMovementTest()
        binding = ["binding-a"]
        service, actions, finals = helper.service([move_frame(1, [1, 1, 0], damage=True)], binding=binding)
        stopped = service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        self.assertNotIn("final", stopped)
        binding[0] = "different-game"
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "world.move.east"})
        self.assertEqual(decided["error"], "binding_drift")
        self.assertEqual(finals, [])
        self.assertEqual(actions, [])

    def test_corrupt_explicit_decision_receipt_revokes_for_each_owner_identity(self):
        fields = ("requested_frame_id", "action_id", "requested_surface_id", "consuming_surface_id",
                  "run_id", "requested_run_id", "missing_run_id", "missing_requested_run_id")
        for accepted, corrupt_field in [(accepted, field) for accepted in (True, False) for field in fields
                                        if accepted or field != "consuming_surface_id"]:
            with self.subTest(corrupt_field=corrupt_field, accepted=accepted):
                current = move_frame(1, [1, 1, 0], damage=True)
                current.update(event="surface_descriptor", schema_version=1, surface_id="surface:1",
                               kind="world", breadcrumbs=["World"], payload={
                                   "avatar": '{"absolute_ms":[1,1,0]}',
                               }, valid_actions=[{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}])
                finals = []
                def dispatch(issuing, action):
                    receipt = {"run_id": issuing["run_id"], "requested_run_id": issuing["run_id"],
                               "requested_frame_id": issuing["frame_id"], "action_id": action,
                               "requested_surface_id": issuing["surface_id"],
                               "consuming_surface_id": issuing["surface_id"], "accepted": accepted}
                    if corrupt_field.startswith("missing_"):
                        del receipt[corrupt_field.removeprefix("missing_")]
                    else:
                        receipt[corrupt_field] = "wrong-identity"
                    return {"native_receipt": receipt, "_next_frame": move_frame(2, [2, 1, 0])}
                channel = cockpit.CockpitRunChannel(lambda: current, dispatch,
                            binding_id="binding-a", read_binding_id=lambda: "binding-a",
                            enforce_continuation_bounds=True,
                            finalize_session=lambda report: finals.append(report) or {})
                service = cockpit.CockpitService(run_channel=channel)
                stopped = service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
                    "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
                self.assertNotIn("final", stopped)
                decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                        "action_id": "world.move.east"})
                self.assertEqual(decided["error"], "native_receipt_mismatch")
                if corrupt_field.startswith("missing_"):
                    self.assertNotIn(corrupt_field.removeprefix("missing_"), decided["receipt"]["native_receipt"])
                else:
                    self.assertEqual(decided["receipt"]["native_receipt"][corrupt_field], "wrong-identity")
                self.assertEqual(channel.status()["state"], "active")
                self.assertEqual(finals, [])
                again = service.call({"action": "game.act", "observation_id": current["frame_id"],
                                      "action_id": "world.move.east"})
                self.assertEqual(again["error"], "duplicate_submission")

    def test_released_surface_decision_accepts_real_composite_movement_run_identity(self):
        current = move_frame(1, [1, 1, 0], damage=True)
        current.update(event="surface_descriptor", schema_version=1, surface_id="surface:1",
                       kind="world", breadcrumbs=["World"], payload={"avatar": '{"absolute_ms":[1,1,0]}'},
                       valid_actions=[{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}])
        successor = {**current, "frame_id": "r023-relative-proof:2", "surface_id": "surface:2",
                     "payload": {"avatar": '{"absolute_ms":[2,1,0]}'}}
        def dispatch(issuing, action):
            return {"native_receipt": {
                "run_id": issuing["run_id"], "requested_frame_id": issuing["frame_id"],
                "action_id": action, "requested_surface_id": issuing["surface_id"],
                "consuming_surface_id": issuing["surface_id"], "accepted": True,
                "surface_receipt": {"requested_run_id": issuing["run_id"]},
            }, "_next_frame": successor}
        channel = cockpit.CockpitRunChannel(lambda: current, dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a", enforce_continuation_bounds=True)
        service = cockpit.CockpitService(run_channel=channel)
        stopped = service.call({"action": "game.guarded_move_relative", "guarded_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        decided = service.call({"action": "game.act", "observation_id": stopped["result"]["terminal_observation"]["observation_id"],
                                "action_id": "world.move.east"})
        self.assertTrue(decided["ok"], decided)
        self.assertEqual(channel.status()["state"], "active")

    def test_contradictory_successor_does_not_count_an_unproved_step(self):
        helper = movement.RelativeMovementTest()
        service, actions, finals = helper.service([
            move_frame(1, [10, 20, 0]), move_frame(2, [11, 20, 0]), move_frame(3, [11, 20, 0])
        ], outcomes=["moved", "moved"])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [2, 0], "bound": move_bound(2)}})
        self.assertEqual(stopped["error"], "raw_move_relative_unexpected_displacement")
        self.assertEqual(stopped["failure"]["detail"]["partial_progress"], 1)
        self.assertEqual(len(stopped["failure"]["detail"]["native_receipts"]), 2)
        self.assertEqual(finals, [])

    def test_blocked_surface_receipt_requires_both_run_identities_before_granting_control(self):
        for corrupted in ("run_id", "requested_run_id", "missing_run_id", "missing_requested_run_id"):
            with self.subTest(corrupted=corrupted):
                current = move_frame(1, [1, 1, 0])
                current.update(event="surface_descriptor", schema_version=1, surface_id="surface:1",
                               kind="world", breadcrumbs=["World"], payload={"avatar": '{"absolute_ms":[1,1,0]}'},
                               valid_actions=[{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}])
                finals = []
                def dispatch(issuing, action):
                    receipt = {"run_id": issuing["run_id"], "requested_frame_id": issuing["frame_id"],
                        "action_id": action, "requested_surface_id": issuing["surface_id"],
                        "consuming_surface_id": issuing["surface_id"], "accepted": False,
                        "coordinate_space": "absolute_ms", "before_absolute_ms": [1, 1, 0],
                        "expected_absolute_ms": [2, 1, 0], "after_absolute_ms": [1, 1, 0], "outcome": "blocked",
                        "surface_receipt": {"requested_run_id": issuing["run_id"]}}
                    field = corrupted.removeprefix("missing_")
                    owner = receipt if field == "run_id" else receipt["surface_receipt"]
                    if corrupted.startswith("missing_"):
                        del owner[field]
                    else:
                        owner[field] = "another-run"
                    return {"native_receipt": receipt, "_next_frame": current}
                channel = cockpit.CockpitRunChannel(lambda: current, dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a",
                    finalize_session=lambda report: finals.append(report) or {})
                stopped = cockpit.CockpitService(run_channel=channel).call({"action": "game.raw_move_relative", "raw_move_relative": {
                    "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
                self.assertEqual(stopped["error"], "raw_move_relative_receipt_mismatch")
                self.assertEqual(finals, [])
                self.assertEqual(channel.status()["state"], "active")

    def test_blocked_receipt_with_contradictory_successor_is_unproved(self):
        helper = movement.RelativeMovementTest()
        service, _, finals = helper.service([
            move_frame(1, [1, 1, 0]), move_frame(2, [2, 1, 0])
        ], outcomes=["blocked"])
        stopped = service.call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        self.assertEqual(stopped["error"], "raw_move_relative_unexpected_displacement")
        self.assertEqual(stopped["failure"]["detail"]["partial_progress"], 0)
        self.assertEqual(finals, [])

    def test_multistep_move_recognizes_successor_world_descriptors_without_legacy_state(self):
        # Shape retained by live selected-7edb3bacf34e4cdba81bbd810f3d62e6:
        # kind=world, payload avatar, no legacy state/provenance/observation.
        for mode in ("raw", "guarded", "guarded_unknown"):
            frames = [{"event": "surface_descriptor", "schema_version": 1,
                       "run_id": "descriptor-run", "surface_id": f"surface:{i}", "frame_id": f"frame:{i}",
                       "kind": "world", "breadcrumbs": ["World"],
                       "payload": {"avatar": json.dumps({"absolute_ms": [10+i, 20, 0]})},
                       "valid_actions": [{"id": "world.move.east", "stable_id": "", "label": "East", "enabled": True}]}
                      for i in range(3)]
            index, actions, finals = [0], [], []
            def dispatch(issuing, action):
                before = json.loads(issuing["payload"]["avatar"])["absolute_ms"]
                expected = [before[0] + 1, before[1], before[2]]
                actions.append(action)
                index[0] += 1
                return {"native_receipt": {
                    "run_id": issuing["run_id"], "requested_run_id": issuing["run_id"],
                    "requested_frame_id": issuing["frame_id"], "frame_id": "legacy-movement-frame",
                    "requested_surface_id": issuing["surface_id"], "consuming_surface_id": issuing["surface_id"],
                    "action_id": action, "accepted": True, "outcome": "moved", "coordinate_space": "absolute_ms",
                    "before_absolute_ms": before, "expected_absolute_ms": expected, "after_absolute_ms": expected,
                    "after_terrain": "t_floor",
                }, "_next_frame": frames[index[0]]}
            def read():
                if mode != "guarded":
                    return frames[index[0]]
                # The same owner, paired by the native reader with actual safety.
                return {**move_frame(index[0], [10+index[0], 20, 0]), **frames[index[0]]}
            channel = cockpit.CockpitRunChannel(read, dispatch,
                binding_id="binding-a", read_binding_id=lambda: "binding-a", enforce_continuation_bounds=True,
                finalize_session=lambda report: finals.append(report) or {})
            operation = "raw_move_relative" if mode == "raw" else "guarded_move_relative"
            moved = cockpit.CockpitService(run_channel=channel).call({"action": "game." + operation, operation: {
                "enabled": True, "offset_ms": [2, 0], "bound": move_bound(2)}})
            if mode == "guarded_unknown":
                self.assertEqual(moved["error"], "guarded_move_relative_unknown_event")
                self.assertEqual(actions, [])
                self.assertEqual(finals, [])
                continue
            self.assertTrue(moved["ok"], moved)
            self.assertEqual(moved["result"]["partial_progress"], 2)
            self.assertEqual(moved["result"]["terminal_absolute_ms"], [12, 20, 0])
            self.assertEqual(actions, ["world.move.east", "world.move.east"])
            self.assertEqual(finals, [])

    def test_corrupt_blocked_receipt_requires_fresh_observation(self):
        current = move_frame(1, [1, 1, 0])
        finals = []
        def dispatch(issuing, action):
            return {"native_receipt": {"frame_id": "another-frame", "action_id": action,
                    "accepted": False, "outcome": "blocked", "coordinate_space": "absolute_ms",
                    "before_absolute_ms": [1, 1, 0], "expected_absolute_ms": [2, 1, 0],
                    "after_absolute_ms": [1, 1, 0]}, "_next_frame": current}
        channel = cockpit.CockpitRunChannel(lambda: current, dispatch,
                    binding_id="binding-a", read_binding_id=lambda: "binding-a",
                    finalize_session=lambda report: finals.append(report) or {})
        stopped = cockpit.CockpitService(run_channel=channel).call({"action": "game.raw_move_relative", "raw_move_relative": {
            "enabled": True, "offset_ms": [1, 0], "bound": move_bound(1)}})
        self.assertEqual(stopped["error"], "raw_move_relative_receipt_mismatch")
        self.assertEqual(finals, [])


if __name__ == "__main__":
    unittest.main()
