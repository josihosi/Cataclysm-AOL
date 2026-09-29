"""Production startup-dialog capture and player projection without live input."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import play_cli
from gameplay_display import plain_player_output
from startup_dialog import (_debug_log_path, correlate_visible_dialog, debug_log_clues,
                            observe_startup_dialog, recover_startup_debug_dialog)


ERRORS = [
    (388, "bool item_location::impl::item_on_person::ensure_who_unpacked() const",
     "Failed to find item_location owner with character_id 2"),
    (174, "void item_location::impl::ensure_unpacked() const",
     "item_location lost its target item during a save/load cycle"),
    (1012, "void item_location::deserialize(const JsonObject &)",
     "parent location doesn't exist.  Item_location has lost its target over a save/load cycle."),
]


class StartupDialogTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.session = root / "session"
        self.session.mkdir()
        self.userdir = root / "profile"
        self.log = self.userdir / "config" / "debug.log"
        self.log.parent.mkdir(parents=True)
        self.log.write_text("".join(
            f"20:33:53.928 ERROR : src/item_location.cpp:{line} [{function}] {message}\n"
            for line, function, message in ERRORS), encoding="utf-8")
        self.binding = "bound-r014"
        self.status = {"binding_id": self.binding, "state": "starting", "session_generation": 0}
        self.command = f"/repo/cataclysm-tiles --userdir {self.userdir} --world TestSetup00"
        self.generation = {"pid": 61861, "alive": True,
                           "birth_identity": "posix-lstart:Sat Sep 26 20:33:17 2026",
                           "command": self.command}
        self._write("status.json", self.status)
        self._write("bridge.manifest.json", {"binding_id": self.binding})
        self._write("game-process.json", {"binding_id": self.binding, "pid": 61861,
            "run_id": "run-r014", "command": self.command,
            "process_generation": self.generation,
            "log_paths": {"profile_diagnostic_debug": {"path": str(self.log),
                                                       "scope": "profile_shared"}}})

    def _write(self, name, value):
        (self.session / name).write_text(json.dumps(value), encoding="utf-8")

    def _runner(self, lines, *, image_ok=True):
        def run(args, **_kwargs):
            if args[0] == "/opt/homebrew/bin/peekaboo":
                self.assertIn("--bridge-socket", args)
                self.assertIn("--pid", args)
                self.assertEqual(args[args.index("--pid") + 1], "61861")
                self.assertNotIn("type", args)
            if args[1:3] == ["list", "windows"]:
                value = {"success": True, "data": {"windows": [
                    {"window_id": 48694, "title": "Cataclysm: Dark Days Ahead - source-bound"}]}}
            elif args[1] == "image":
                if image_ok:
                    Path(args[args.index("--path") + 1]).write_bytes(b"\x89PNG\r\nstartup-dialog-test")
                value = {"success": image_ok}
            elif Path(args[0]).name == "swift":
                value = {"ok": True, "lines": lines}
            else:
                raise AssertionError(args)
            return subprocess.CompletedProcess(args, 0, json.dumps(value), "")
        return run

    def test_current_third_error_matches_exact_log_and_keeps_other_clues(self):
        lines = ["An error has occurred! Written below is the error report:",
                 "DEBUG : parent location doesn't exist. Item_location has lost its target over",
                 "a save/load cycle.", "REPORTING FUNCTION : void item_location::deserialize"]
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=self._runner(lines))
        self.assertEqual(result["state"], "confirmed_debug_dialog")
        self.assertEqual(result["message"], ERRORS[2][2])
        self.assertEqual(result["source_file"], "src/item_location.cpp")
        self.assertEqual(result["source_line"], 1012)
        self.assertEqual([clue["source_line"] for clue in result["log_clues"]], [388, 174, 1012])
        self.assertEqual(result["birth_identity"], self.generation["birth_identity"])
        self.assertEqual(result["session"], str(self.session.resolve()))
        self.assertEqual(result["window_id"], 48694)
        self.assertTrue(Path(result["image_path"]).is_file())
        self.assertEqual(len(result["image_sha256"]), 64)
        self.assertIn("--userdir", result["command"])
        text = plain_player_output({"ok": False, "status": self.status, "startup_dialog": result})
        self.assertIn("startup_ui_only", text)
        self.assertIn("src/item_location.cpp:1012", text)
        self.assertIn("UI capture:", text)
        self.assertIn("No input sent", text)
        self.assertIn("do not prove save integrity", text)
        self.assertIn("Other profile log clues", text)

    def test_first_known_error_is_distinct_from_later_item_location_messages(self):
        lines = ["An error has occurred! Written below is the error report:",
                 "DEBUG : Failed to find item_location owner with character_id 2",
                 "REPORTING FUNCTION : bool item_location::impl::item_on_person::ensure_who_unpacked()"]
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=self._runner(lines))
        self.assertEqual(result["source_line"], 388)
        self.assertEqual(result["message"], ERRORS[0][2])

    def test_nested_build_executable_uses_owned_launch_cwd_for_relative_userdir(self):
        root = Path(self.temp.name)
        nested_executable = root / "build" / "r032" / "cataclysm-tiles"
        nested_executable.parent.mkdir(parents=True)
        relative_userdir = Path(".userdata") / "profile"
        actual_log = root / relative_userdir / "config" / "debug.log"
        actual_log.parent.mkdir(parents=True)
        actual_log.write_text(
            f"20:33:53.928 ERROR : src/item_location.cpp:388 [{ERRORS[0][1]}] {ERRORS[0][2]}\n",
            encoding="utf-8")
        command = f"{nested_executable} --userdir {relative_userdir} --world TestSetup00"
        generation = {**self.generation, "command": command}
        self._write("game-process.json", {"binding_id": self.binding, "pid": 61861,
            "run_id": "nested-build-run", "command": command, "launch_cwd": str(root),
            "process_generation": generation,
            "log_paths": {"profile_diagnostic_debug": {"path": str(actual_log),
                                                       "scope": "profile_shared"}}})
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: generation, runner=self._runner([
                "An error has occurred! Written below is the error report:",
                "DEBUG : Failed to find item_location owner with character_id 2",
                "REPORTING FUNCTION : bool item_location::impl::item_on_person::ensure_who_unpacked()"])
        )
        self.assertEqual(result["state"], "confirmed_debug_dialog")
        self.assertEqual(result["source_line"], 388)
        self.assertEqual(result["log_path"], str(actual_log.resolve()))
        output = plain_player_output({"ok": False, "status": self.status, "startup_dialog": result})
        self.assertIn(ERRORS[0][2], output)
        self.assertIn("src/item_location.cpp:388", output)

    def test_legacy_relative_userdir_requires_verified_workspace_session(self):
        root = Path(self.temp.name).resolve()
        session = root / ".userdata/openclaw_harness/bridge-sessions/selected-legacy"
        session.mkdir(parents=True)
        actual_log = root / ".userdata/profile/config/debug.log"
        actual_log.parent.mkdir(parents=True, exist_ok=True)
        actual_log.write_text("old diagnostic\n", encoding="utf-8")
        owner = {"command": f"{root}/build/r032/cataclysm-tiles --userdir .userdata/profile/ --world TestSetup00",
                 "log_paths": {"profile_diagnostic_debug": {"path": str(actual_log),
                                                            "scope": "profile_shared"}}}
        with patch("startup_dialog.WORKSPACE_ROOT", root):
            self.assertEqual(_debug_log_path(owner, session), actual_log.resolve())
            self.assertIsNone(_debug_log_path(owner, self.session))
            malformed_cwd = {**owner, "launch_cwd": "build/r032"}
            self.assertIsNone(_debug_log_path(malformed_cwd, session))
            wrong_profile = {**owner, "log_paths": {"profile_diagnostic_debug": {
                "path": str(root / ".userdata/other/config/debug.log"), "scope": "profile_shared"}}}
            self.assertIsNone(_debug_log_path(wrong_profile, session))

    def test_unrelated_loading_does_not_promote_old_log_error_to_current_dialog(self):
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation,
            runner=self._runner(["Loading world TestSetup00..."]))
        self.assertEqual(result["state"], "loading_unknown")
        self.assertNotIn("message", result)
        self.assertEqual(len(result["log_clues"]), 3)
        text = plain_player_output({"ok": False, "status": self.status, "startup_dialog": result})
        self.assertIn("not confirmed as the current screen", text)
        self.assertNotIn("one I/i", text)

    def test_other_dialog_and_unmatched_debug_remain_separate(self):
        other = correlate_visible_dialog(["Do you really want to quit?"], [])
        self.assertEqual(other["state"], "other_surface")
        unmatched = correlate_visible_dialog([
            "An error has occurred! Written below is the error report:",
            "DEBUG : This is a materially different failure", "REPORTING FUNCTION : test"],
            [{"message": ERRORS[0][2]}])
        self.assertEqual(unmatched["state"], "debug_dialog_unmatched")
        self.assertNotIn("message", unmatched)

    def test_capture_failure_shows_exact_log_clues_as_unconfirmed(self):
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=self._runner([], image_ok=False))
        self.assertEqual(result["state"], "unavailable")
        self.assertEqual(len(result["log_clues"]), 3)
        self.assertNotIn("message", result)
        text = plain_player_output({"ok": False, "status": self.status, "startup_dialog": result})
        self.assertIn("Current blocking screen unknown", text)
        self.assertIn("src/item_location.cpp:1012", text)
        self.assertNotIn("one I/i", text)

    def test_authorized_socket_must_not_accept_a_local_fallback(self):
        def local_fallback(args, **_kwargs):
            value = {"success": True, "data": {"windows": [{"window_id": 48694,
                "title": "Cataclysm: Dark Days Ahead"}]},
                "debug_logs": ["Runtime host: local (in-process fallback)"]}
            return subprocess.CompletedProcess(args, 0, json.dumps(value), "")
        result = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=local_fallback)
        self.assertEqual(result["reason"], "window_list_unauthorized_local_capture_host")
        self.assertEqual(len(result["log_clues"]), 3)

    def test_wrong_birth_exit_and_ready_status_never_capture(self):
        def no_capture(*_args, **_kwargs):
            raise AssertionError("Peekaboo must not inspect an invalid game identity")
        changed = {**self.generation, "birth_identity": "reused PID"}
        wrong = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: changed, runner=no_capture)
        self.assertEqual(wrong["state"], "identity_changed")
        self.assertEqual(len(wrong["log_clues"]), 3)
        exited = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: {**self.generation, "alive": False}, runner=no_capture)
        self.assertEqual(exited["state"], "exited")
        ready = observe_startup_dialog(self.session, self.binding,
            {**self.status, "state": "ready"}, snapshot=lambda _pid: self.generation,
            runner=no_capture)
        self.assertEqual(ready["reason"], "not_bound_pre_world_startup")

    def test_capture_budget_and_terminal_bridge_keep_current_state_unknown(self):
        captures = self.session / "startup-dialog-captures"
        captures.mkdir()
        for index in range(8):
            (captures / f"capture-{index}.png").write_bytes(b"old capture")
        result = observe_startup_dialog(self.session, self.binding,
            {**self.status, "state": "process_dead", "reason": "startup_proof_red"},
            snapshot=lambda _pid: self.generation,
            runner=lambda *_args, **_kwargs: self.fail("capture budget must stop Peekaboo"))
        self.assertEqual(result["reason"], "capture_limit_reached")
        self.assertEqual(len(result["log_clues"]), 3)
        self.assertNotIn("message", result)

    def test_player_look_uses_read_only_dialog_view_without_request(self):
        dialog = {"state": "confirmed_debug_dialog", "pid": 61861,
                  "birth_identity": self.generation["birth_identity"],
                  "message": ERRORS[0][2], "source_file": "src/item_location.cpp",
                  "source_line": 388, "log_path": str(self.log), "log_byte_offset": 0,
                  "evidence_class": "startup_ui_only", "session": str(self.session),
                  "image_sha256": "a" * 64}
        output = io.StringIO()
        with (patch.object(play_cli, "observe_startup_dialog", return_value=dialog),
              contextlib.redirect_stdout(output)):
            code = play_cli.main(["--session", str(self.session), "look"])
        self.assertEqual(code, 1)  # No World authority was fabricated.
        self.assertIn(ERRORS[0][2], output.getvalue())
        self.assertIn("debug-ignore --capture", output.getvalue())
        self.assertEqual(list((self.session / "requests").glob("*.json")) if
                         (self.session / "requests").exists() else [], [])

    def test_three_distinct_debug_ignores_reobserve_before_world_verification(self):
        stage = [0]
        pressed = []
        messages = [entry[2] for entry in ERRORS]

        def run(args, **_kwargs):
            if args[1:3] == ["list", "windows"]:
                value = {"success": True, "data": {"windows": [
                    {"window_id": 48694, "title": "Cataclysm: Dark Days Ahead - source-bound"}]}}
            elif args[1] == "image":
                Path(args[args.index("--path") + 1]).write_bytes(b"\x89PNG\r\nmodal-" + bytes([stage[0]]))
                value = {"success": True}
            elif Path(args[0]).name == "swift":
                value = {"ok": True, "lines": ["An error has occurred! Written below is the error report:",
                         "DEBUG : " + messages[stage[0]], "REPORTING FUNCTION : test"]}
            elif args[1:3] == ["type", "i"]:
                self.assertEqual(args, ["/opt/homebrew/bin/peekaboo", "type", "i",
                    "--pid", "61861", "--bridge-socket",
                    "/Users/josefhorvath/Library/Application Support/Peekaboo/bridge.sock",
                    "--json"])
                pressed.append(stage[0])
                stage[0] += 1
                if stage[0] == 3:
                    self._write("status.json", {"binding_id": self.binding, "state": "ready",
                                                "session_generation": 0})
                value = {"success": True}
            else:
                raise AssertionError(args)
            return subprocess.CompletedProcess(args, 0, json.dumps(value), "")

        first = observe_startup_dialog(self.session, self.binding, self.status,
                                       snapshot=lambda _pid: self.generation, runner=run)
        self.assertEqual(first["message"], messages[0])
        one = recover_startup_debug_dialog(self.session, self.binding, first["image_sha256"],
                                           snapshot=lambda _pid: self.generation, runner=run)
        self.assertTrue(one["ok"])
        self.assertEqual(one["after"]["message"], messages[1])
        self.assertEqual(pressed, [0])
        two = recover_startup_debug_dialog(self.session, self.binding,
                                           one["after"]["image_sha256"],
                                           "Verify item state if inventory matters",
                                           snapshot=lambda _pid: self.generation, runner=run)
        self.assertTrue(two["ok"])
        self.assertEqual(two["after"]["message"], messages[2])
        self.assertEqual(pressed, [0, 1])
        three = recover_startup_debug_dialog(self.session, self.binding,
                                             two["after"]["image_sha256"],
                                             snapshot=lambda _pid: self.generation, runner=run)
        self.assertTrue(three["ok"])
        self.assertEqual(three["after"]["status"], "ready")
        self.assertEqual(pressed, [0, 1, 2])
        self.assertIn("native World still needs verification", plain_player_output(three))
        for response in (one, two, three):
            attempt = json.loads(Path(response["attempt_path"]).read_text())
            self.assertEqual(attempt["run_id"], "run-r014")
            self.assertEqual(attempt["pid"], 61861)
            self.assertEqual(attempt["birth_identity"], self.generation["birth_identity"])
            self.assertTrue(attempt["current_dialog"]["image_path"])
            self.assertTrue(Path(response["result_path"]).is_file())
        self.assertEqual(json.loads(Path(one["attempt_path"]).read_text())["note"], "")
        self.assertEqual(json.loads(Path(two["attempt_path"]).read_text())["note"],
                         "Verify item state if inventory matters")

    def test_type_delivery_error_keeps_modal_and_records_failed_input(self):
        lines = ["An error has occurred!", "DEBUG : " + ERRORS[0][2]]
        base = self._runner(lines)
        sent = []
        def run(args, **kwargs):
            if args[1:3] == ["type", "i"]:
                sent.append(args)
                return subprocess.CompletedProcess(args, 1, json.dumps({
                    "success": False, "error": {"code": "VALIDATION_ERROR",
                    "message": "delivery refused"}}), "")
            return base(args, **kwargs)
        observed = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=run)
        result = recover_startup_debug_dialog(self.session, self.binding,
            observed["image_sha256"], snapshot=lambda _pid: self.generation, runner=run)
        self.assertEqual(len(sent), 1)
        self.assertEqual(sent[0], ["/opt/homebrew/bin/peekaboo", "type", "i",
            "--pid", "61861", "--bridge-socket",
            "/Users/josefhorvath/Library/Application Support/Peekaboo/bridge.sock", "--json"])
        self.assertFalse(result["ok"])
        self.assertEqual(result["state"], "input_delivery_failed")
        self.assertEqual(result["input"]["delivery"], "failed")
        self.assertEqual(result["input"]["peekaboo"]["error"], "VALIDATION_ERROR")
        self.assertEqual(result["after"]["message"], ERRORS[0][2])
        self.assertEqual(json.loads(Path(result["attempt_path"]).read_text())[
            "intended_input"], "Peekaboo type i --pid (native debug Ignore)")
        recorded = json.loads(Path(result["result_path"]).read_text())
        self.assertFalse(recorded["ok"])
        self.assertEqual(recorded["input"]["delivery"], "failed")
        self.assertIn("Debug Ignore: no confirmed delivery", plain_player_output(result))
        self.assertIn("Delivery error: VALIDATION_ERROR; delivery refused",
                      plain_player_output(result))

    def test_unknown_or_non_debug_ui_and_changed_capture_do_not_send_input(self):
        pressed = []

        def runner_for(lines):
            base = self._runner(lines)
            def run(args, **kwargs):
                if args[1] == "type":
                    pressed.append(args)
                    raise AssertionError("must not press an unconfirmed dialog")
                return base(args, **kwargs)
            return run

        for lines in (["Loading world TestSetup00..."], ["Do you really want to quit?"],
                      ["An error has occurred!", "DEBUG : An unfamiliar unmatched issue"]):
            result = recover_startup_debug_dialog(self.session, self.binding, "a" * 64,
                snapshot=lambda _pid: self.generation,
                runner=runner_for(lines))
            self.assertFalse(result["ok"])
            self.assertEqual(result["reason"], "current_exact_debug_dialog_unconfirmed")
        observed = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation,
            runner=runner_for(["An error has occurred!", "DEBUG : " + ERRORS[0][2]]))
        changed = recover_startup_debug_dialog(self.session, self.binding, "b" * 64,
            snapshot=lambda _pid: self.generation,
            runner=runner_for(["An error has occurred!", "DEBUG : " + ERRORS[0][2]]))
        self.assertNotEqual(observed["image_sha256"], "b" * 64)
        self.assertEqual(changed["reason"], "current_debug_capture_changed")
        self.assertEqual(pressed, [])

    def test_birth_change_after_capture_refuses_input(self):
        calls = [0]
        def snapshot(_pid):
            calls[0] += 1
            if calls[0] >= 3:
                return {**self.generation, "birth_identity": "reused PID"}
            return self.generation
        base = self._runner(["An error has occurred!", "DEBUG : " + ERRORS[0][2]])
        def no_press(args, **kwargs):
            if args[1] == "type":
                self.fail("identity change must refuse input")
            return base(args, **kwargs)
        observed = observe_startup_dialog(self.session, self.binding, self.status,
            snapshot=lambda _pid: self.generation, runner=no_press)
        result = recover_startup_debug_dialog(self.session, self.binding,
            observed["image_sha256"], snapshot=snapshot, runner=no_press)
        self.assertFalse(result["ok"])
        self.assertEqual(result["reason"], "identity_or_status_changed_before_input")
        self.assertEqual(list((self.session / "startup-dialog-recoveries").glob("*.attempt.json"))
                         if (self.session / "startup-dialog-recoveries").exists() else [], [])

    def test_confirmed_unfamiliar_debug_message_uses_same_recovery_without_allowlist(self):
        message = "Unexpected but recoverable loading warning for a new test fixture"
        with self.log.open("a", encoding="utf-8") as stream:
            stream.write(f"20:34:00 ERROR : src/new_load.cpp:77 [load] {message}\n")
        presses = []
        base = self._runner(["An error has occurred!", "DEBUG : " + message])
        def run(args, **kwargs):
            if args[1:3] == ["type", "i"]:
                presses.append(args)
                return subprocess.CompletedProcess(args, 0, json.dumps({"success": True}), "")
            return base(args, **kwargs)
        observed = observe_startup_dialog(self.session, self.binding, self.status,
                                          snapshot=lambda _pid: self.generation, runner=run)
        result = recover_startup_debug_dialog(self.session, self.binding,
            observed["image_sha256"], "No current item claim; preserve new diagnostic",
            snapshot=lambda _pid: self.generation, runner=run)
        self.assertTrue(result["ok"])
        self.assertEqual(result["before"]["message"], message)
        self.assertEqual(len(presses), 1)
        self.assertIn("same modal remains", plain_player_output(result))

    def test_cli_debug_ignore_routes_current_capture_without_mandatory_note(self):
        observed = {"state": "confirmed_debug_dialog", "message": ERRORS[0][2],
                    "source_file": "src/item_location.cpp", "source_line": 388,
                    "run_id": "run-r014", "pid": 61861,
                    "birth_identity": self.generation["birth_identity"]}
        result = {"schema": "caol-startup-debug-recovery-v1", "ok": True,
                  "state": "input_reported_delivered", "before": observed,
                  "after": {"state": "bridge_status_changed", "status": "ready"}}
        output = io.StringIO()
        with (patch.object(play_cli, "recover_startup_debug_dialog", return_value=result) as recover,
              contextlib.redirect_stdout(output)):
            code = play_cli.main(["--session", str(self.session), "debug-ignore",
                                  "--capture", "a" * 64])
        self.assertEqual(code, 0)
        recover.assert_called_once_with(self.session, self.binding, "a" * 64, "")
        self.assertIn("Debug Ignore: one key reported delivered", output.getvalue())
        self.assertIn("native World still needs verification", output.getvalue())

    def test_retained_r013_mac_images_correlate_distinct_native_diagnostics(self):
        workspace = Path(__file__).resolve().parents[2]
        screenshots = [("remote-capture-retry-startup.png", 388),
                       ("owner-authorized-post-i.png", 1012)]
        source = workspace / ".userdata/first-smoke-013-remote-capture-20260926/config/debug.log"
        if sys.platform != "darwin" or not source.is_file() or any(
                not (workspace / "build_logs/first-smoke-013" / name).is_file()
                for name, _line in screenshots):
            self.skipTest("retained R013 Mac screenshot and log not present")
        clues = debug_log_clues(source)
        for name, expected_line in screenshots:
            image = workspace / "build_logs/first-smoke-013" / name
            completed = subprocess.run(["swift", str(Path(__file__).with_name("ocr_image.swift")),
                "--image", str(image), "--region-of-interest", "0", "0.8", "0.8", "0.2"],
                capture_output=True, text=True, timeout=30, check=True)
            ocr = json.loads(completed.stdout)
            matched = correlate_visible_dialog(ocr["lines"], clues)
            self.assertEqual(matched["state"], "confirmed_debug_dialog", name)
            self.assertEqual(matched["source_line"], expected_line, name)


if __name__ == "__main__":
    unittest.main()
