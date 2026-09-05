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

    def test_published_wait_example_reaches_target_with_native_receipt(self):
        service, dispatched = self.wait_service()
        controls = service.call({"action": "game.controls"})["result"]
        result = service.call(controls["wait"]["example_request"])
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.pause"])
        self.assertEqual(result["result"]["terminal_observation"]["game_minutes"], 101)
        self.assertEqual(result["result"]["native_action_count"], 1)
        receipts = [entry for entry in service.run_channel._transcript if entry.get("kind") == "action"]
        self.assertEqual(len(receipts), 1)

    def test_documented_absolute_target_variant_uses_cautious_route(self):
        service, dispatched = self.wait_service()
        request = cockpit.player_controls()["wait"]["example_request"]
        wait = request["wait"]
        wait.pop("target_delta_game_minutes")
        wait["target_game_minutes"] = 101
        wait["danger_handling"] = "stop_on_interruption"
        result = service.call(request)
        self.assertTrue(result["ok"], result)
        self.assertEqual(dispatched, ["world.pause"])

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
            "game.wait": True, "game.move_relative": True,
        })
        channel = service.run_channel
        before = copy.deepcopy((channel._observations, channel._last_public_state, channel._transcript))
        channel._read_native_frame = lambda: self.fail("controls must not read a native frame")
        result = service.call({"action": "game.controls"})
        self.assertTrue(result["ok"])
        self.assertEqual(before, (channel._observations, channel._last_public_state, channel._transcript))
        self.assertFalse(dispatched)

    def test_discovery_reports_denied_macros_without_authorizing_them(self):
        service, dispatched = self.wait_service()
        service._allowed_live_operations = {"game.observe", "game.act"}
        controls = service.call({"action": "game.controls"})["result"]
        self.assertEqual(controls["availability"], {"game.wait": False, "game.move_relative": False})
        for name in ("wait", "move_relative"):
            result = service.call(controls[name]["example_request"])
            self.assertEqual(result["error"], "operation_not_authorized_for_live_session")
        self.assertFalse(dispatched)


if __name__ == "__main__":
    unittest.main()
