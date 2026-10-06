"""Documented macro requests exercise existing service validators/receipts."""
import copy
import unittest

import cockpit
import cockpit_keep_watch_test as wait_fixture
import r023_relative_movement_test as move_fixture


class ControlsTest(unittest.TestCase):
    def wait_service(self):
        frames = [wait_fixture.frame(i, minutes, {"classification": "clear", "monster": False,
                                          "danger": False, "damage": False})
                  for i, minutes in enumerate((100, 101), 1)]
        for frame in frames:
            frame["valid_actions"] = ["world.pause"]
            frame["action_inputs"] = {"world.pause": "."}
        return wait_fixture.KeepWatchTest().service(frames)

    def test_published_wait_guidance_starts_the_native_wait_menu(self):
        controls = cockpit.player_controls()["wait"]
        self.assertEqual(controls["manual_start_request"], {
            "action": "game.act", "action_id": "world.wait",
        })
        self.assertIn("passes one native turn", controls["recipe"])

    def test_pause_recipe_advances_short_native_turns_before_the_next_minute(self):
        frames = [wait_fixture.frame(i, minutes, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        }) for i, minutes in ((1, 100), (2, 100), (3, 101))]
        for frame in frames:
            frame["schema_version"] = 1
            frame["event"] = "surface_descriptor"
            frame["surface_id"] = f"surface:{frame['frame_id']}"
            frame["kind"] = "world"
            frame["breadcrumbs"] = ["World"]
            frame["payload"] = {}
            frame["valid_actions"] = [{"id": "world.pause", "stable_id": "",
                                       "label": "Pause", "enabled": True}]
            frame["action_inputs"] = {"world.pause": "."}
        service, dispatched = wait_fixture.KeepWatchTest().service(frames)
        result = service.call({"action": "game.wait", "wait": {
            "enabled": True, "target_delta_game_minutes": 1,
            "danger_handling": "handle_classified_non_dangerous", "recipe": ["world.pause"],
            "bound": {"basis": "scheduler_boundary", "source": "test", "unit": "game_minutes",
                      "maximum": 1, "progress_required": True},
        }})
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.pause", "world.pause"])
        self.assertEqual(result["result"]["terminal_game_minutes"], 101)

    def test_pause_recipe_stops_when_native_turn_and_moves_do_not_progress(self):
        frames = [wait_fixture.frame(i, 100, {
            "classification": "clear", "monster": False, "danger": False, "damage": False,
        }) for i in (1, 2)]
        for frame in frames:
            frame["schema_version"] = 1
            frame["event"] = "surface_descriptor"
            frame["surface_id"] = f"surface:{frame['frame_id']}"
            frame["kind"] = "world"
            frame["breadcrumbs"] = ["World"]
            frame["payload"] = {}
            frame["valid_actions"] = [{"id": "world.pause", "stable_id": "",
                                       "label": "Pause", "enabled": True}]
        frames[1]["observed_turn"] = frames[0]["observed_turn"]
        service, dispatched = wait_fixture.KeepWatchTest().service(frames)
        result = service.call({"action": "game.wait", "wait": {
            "enabled": True, "target_delta_game_minutes": 1,
            "danger_handling": "handle_classified_non_dangerous", "recipe": ["world.pause"],
            "bound": {"basis": "scheduler_boundary", "source": "test", "unit": "game_minutes",
                      "maximum": 1, "progress_required": True},
        }})
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "keep_watch_pause_progress_unproved")
        self.assertEqual(dispatched, ["world.pause"])

    def test_documented_absolute_target_variant_uses_cautious_route(self):
        service, dispatched = self.wait_service()
        self.assertEqual(cockpit.player_controls()["wait"]["manual_start_request"]["action_id"],
                         "world.wait")
        self.assertFalse(dispatched)

    def test_changing_only_danger_mode_keeps_relative_wait_target_valid(self):
        service, dispatched = self.wait_service()
        self.assertIn("manual_sequence", cockpit.player_controls()["wait"])
        self.assertFalse(dispatched)

    def test_published_move_example_proves_expected_native_displacement(self):
        service, dispatched, finals = move_fixture.RelativeMovementTest().service([
            move_fixture.frame(1, [10, 20, 0]), move_fixture.frame(2, [11, 20, 0]),
        ])
        result = service.call(cockpit.player_controls()["move_relative"]["example_request"])
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.move.east"])
        self.assertEqual(result["result"]["terminal_absolute_ms"], [11, 20, 0])
        self.assertFalse(finals)

    def test_controls_never_observes_or_consumes_current_authority(self):
        service, dispatched = self.wait_service()
        observed = service.call({"action": "game.observe"})
        self.assertEqual(observed["operation_availability"], {
            "game.act": True, "game.wait": True, "game.move_relative": True,
        })
        channel = service.run_channel
        before = copy.deepcopy((channel._observations, channel._last_public_state, channel._transcript))
        channel._read_native_frame = lambda: self.fail("controls must not read a native frame")
        result = service.call({"action": "game.controls"})
        self.assertTrue(result["ok"])
        setup = result["result"]["setup"]["debug_life_support"]
        self.assertEqual(setup["mutation_id"], "DEBUG_LS")
        self.assertIn("Only if missing", setup["sequence"])
        self.assertIn("Keep Debug Life Support active", setup["sequence"])
        self.assertIn("current advertised target", setup["owner"])
        from gameplay_display import plain_player_output
        readback = plain_player_output(result)
        self.assertIn("inspect exact DEBUG_LS", readback)
        self.assertIn("add only if missing", readback)
        self.assertIn("not combat immunity", readback)
        self.assertNotIn("end playtest → play quit", readback)
        self.assertEqual(before, (channel._observations, channel._last_public_state, channel._transcript))
        self.assertFalse(dispatched)

    def test_discovery_reports_denied_macros_without_authorizing_them(self):
        service, dispatched = self.wait_service()
        service._allowed_live_operations = {"game.observe", "game.act"}
        controls = service.call({"action": "game.controls"})["result"]
        self.assertEqual(controls["availability"], {
            "game.act": True, "game.wait": False, "game.move_relative": False})
        result = service.call(controls["move_relative"]["example_request"])
        self.assertEqual(result["error"], "operation_not_authorized_for_live_session")
        self.assertFalse(dispatched)

    def test_observation_only_continuation_does_not_advertise_action_permission(self):
        service, dispatched = self.wait_service()
        service._allowed_live_operations = {"game.observe"}
        observed = service.call({"action": "game.observe"})
        self.assertFalse(observed["operation_availability"]["game.act"])
        result = service.call({"action": "game.act", "action_id": "world.wait",
                               "observation_id": observed["result"]["observation_id"]})
        self.assertEqual(result["error"], "operation_not_authorized_for_live_session")
        self.assertFalse(dispatched)


if __name__ == "__main__":
    unittest.main()
