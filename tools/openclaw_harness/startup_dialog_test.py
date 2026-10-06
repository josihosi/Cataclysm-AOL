"""Production startup-dialog capture and player projection without live input."""
import contextlib
import io
import json
import shlex
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

    def test_sealed_reentry_look_exposes_current_warning_without_game_request(self):
        reentry = {"binding_id": self.binding, "state": "transitioning",
                   "phase": "awaiting_declared_reentry_descriptor", "session_generation": 0}
        self._write("status.json", reentry)
        self._write("play-client.json", {"binding_id": self.binding, "finished": True,
            "sealed_terminal": {"stop_reason": "saved and quit"}, "session_generation": 0})
        dialog = {"state": "confirmed_debug_dialog", "pid": 61861,
                  "birth_identity": self.generation["birth_identity"],
                  "message": ERRORS[0][2], "source_file": "src/item_location.cpp",
                  "source_line": 388, "log_path": str(self.log), "log_byte_offset": 0,
                  "evidence_class": "startup_ui_only", "session": str(self.session),
                  "image_sha256": "a" * 64}
        output = io.StringIO()
        with (patch.object(play_cli, "observe_startup_dialog", return_value=dialog) as observe,
              contextlib.redirect_stdout(output)):
            code = play_cli.main(["--session", str(self.session), "look"])
        self.assertEqual(code, 1)
        observe.assert_called_once()
        self.assertIn(ERRORS[0][2], output.getvalue())
        self.assertIn("debug-ignore --capture", output.getvalue())
        self.assertEqual(json.loads((self.session / "play-client.json").read_text())["finished"], True)
        self.assertFalse((self.session / "requests").exists())
        self._write("status.json", {**reentry, "phase": "different_phase"})
        refused = io.StringIO()
        with (patch.object(play_cli, "observe_startup_dialog") as unexpected,
              contextlib.redirect_stdout(refused)):
            self.assertEqual(play_cli.main(["--session", str(self.session), "look"]), 1)
        unexpected.assert_not_called()
        self.assertIn("journal_is_sealed", refused.getvalue())

    def test_declared_reentry_exact_item_warning_requires_current_phase_and_capture(self):
        reentry = {"binding_id": self.binding, "state": "transitioning",
                   "phase": "awaiting_declared_reentry_descriptor", "session_generation": 0}
        self._write("status.json", reentry)
        lines = ["An error has occurred! Written below is the error report:",
                 "DEBUG : " + ERRORS[0][2], "REPORTING FUNCTION : " + ERRORS[0][1]]
        presses = []
        base = self._runner(lines)
        def run(args, **kwargs):
            if args[1:3] == ["type", "i"]:
                presses.append(args)
                self._write("status.json", {**reentry, "phase": "different_phase"})
                return subprocess.CompletedProcess(args, 0, json.dumps({"success": True}), "")
            return base(args, **kwargs)
        observed = observe_startup_dialog(self.session, self.binding, reentry,
            snapshot=lambda _pid: self.generation, runner=run)
        self.assertEqual(observed["state"], "confirmed_debug_dialog")
        self.assertEqual((observed["source_file"], observed["source_line"]),
                         ("src/item_location.cpp", 388))
        wrong_phase = observe_startup_dialog(self.session, self.binding,
            {**reentry, "phase": "different_phase"},
            snapshot=lambda _pid: self.generation,
            runner=lambda *_a, **_k: self.fail("wrong phase must not capture"))
        self.assertEqual(wrong_phase["reason"], "not_bound_pre_world_startup")
        recovered = recover_startup_debug_dialog(self.session, self.binding,
            observed["image_sha256"], snapshot=lambda _pid: self.generation, runner=run)
        self.assertTrue(recovered["ok"])
        self.assertEqual(len(presses), 1)
        self.assertEqual(recovered["before"]["message"], ERRORS[0][2])
        self.assertEqual(recovered["after"]["state"], "bridge_status_changed")
        self.assertTrue(Path(recovered["result_path"]).is_file())

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


class TerminalStartupDialogTest(unittest.TestCase):
    _write = StartupDialogTest._write

    def setUp(self):
        StartupDialogTest.setUp(self)
        from hashlib import sha256
        from curses_terminal_transport import _host_identity
        self.run = self.userdir.resolve() / "harness_runs" / "current"
        self.run.mkdir(parents=True)
        self.endpoint = Path(self.temp.name).resolve() / "broker" / "input.sock"
        self.endpoint.parent.mkdir()
        self.endpoint.touch()
        self.transcript = self.run / "game.terminal.log"
        self.lines = ["An error has occurred! Written below is the error report:",
                      "DEBUG : " + ERRORS[0][2], "REPORTING FUNCTION : test",
                      "Press space bar to continue the game.",
                      "Press I (or i) to also ignore this particular message in the future."]
        self._frame(self.lines)
        host = _host_identity()
        self.bridge = {"pid": 71000, "alive": True, "birth_identity": "bridge-birth", "command": "bridge"}
        self.status.update(bridge_pid=71000, bridge_process_generation=self.bridge)
        self._write("status.json", self.status)
        self.owner = json.loads((self.session / "game-process.json").read_text())
        self.owner.update(playtest_mode="terminal", userdir=str(self.userdir), host=host)
        self.owner["log_paths"]["native_semantic_events"] = {
            "path": str(self.run / "semantic.native.events.jsonl"), "scope": "run_bound"}
        self._write("game-process.json", self.owner)
        command = ["/python", "/curses_terminal_transport.py", "--broker", "--transcript",
                   str(self.transcript), "--endpoint", str(self.endpoint), "--owner",
                   str(self.endpoint.parent / "owner.json"), "--run-owner", str(self.run / "terminal.owner.json")]
        self.broker = {"pid": 70000, "alive": True, "birth_identity": "broker-birth",
                       "command": shlex.join(command)}
        self.terminal = {"schema": "caol-curses-terminal-owner-v1", "host": host,
                         "run_id": self.owner["run_id"], "game_pid": self.generation["pid"],
                         "game_process_generation": self.generation,
                         "broker_pid": self.broker["pid"], "broker_process_generation": self.broker,
                         "broker_command": command, "endpoint": str(self.endpoint),
                         "run_owner_path": str(self.run / "terminal.owner.json")}
        self._owners()
        self.declaration = Path(self.temp.name) / "scenario.json"
        declaration = {"name": "terminal-test", "runtime_contract": {
            "permitted_input": ["semantic:world.move.*"], "forbidden_input": ["keyboard"],
            "setup_only_debug": True, "playtest_mode": "terminal", "terminal_transport": "pty"}}
        declaration["runtime_contract"]["permitted_input"].append("debug:ignore")
        selected = (json.dumps(declaration, indent=2) + "\n").encode()
        preflight = {"scenario": "terminal-test", "scenario_source": {
            "path": str(self.declaration), "sha256": sha256(selected).hexdigest()}}
        (self.run / "contract.preflight.json").write_text(json.dumps(preflight))
        self.declaration.write_bytes(selected)

    def _owners(self):
        for path in [self.run / "terminal.owner.json", self.endpoint.parent / "owner.json"]:
            path.write_text(json.dumps(self.terminal))

    def _frame(self, lines):
        self.transcript.write_text("\x1b[2J\x1b[H" + "\r\n".join(lines))

    def _snapshot(self, pid):
        return self.bridge if pid == 71000 else self.broker if pid == self.broker["pid"] else self.generation

    def _observe(self):
        return observe_startup_dialog(self.session, self.binding, self.status,
                                      snapshot=self._snapshot,
                                      runner=lambda *a, **k: self.fail("terminal path called GUI"))

    def _recover(self, capture=None):
        return recover_startup_debug_dialog(self.session, self.binding,
            capture or self._observe()["capture_sha256"], snapshot=self._snapshot,
            runner=lambda *a, **k: self.fail("terminal path called GUI"))

    def _receipt(self, endpoint, **kwargs):
        from hashlib import sha256
        return {"ok": True, "request_id": kwargs["request_id"], "run_id": kwargs["run_id"],
                "pid": kwargs["pid"], "host": self.terminal["host"], "keys": ["i"],
                "process_generation": self.generation, "owner": "run_bound_pty",
                "payload_sha256": sha256(b"i").hexdigest()}

    def _retained_native_warning(self):
        self.status["state"] = "process_dead"
        self.status["child_exit_code"] = 1
        self._write("status.json", self.status)
        (self.userdir / "config" / "debug.log").write_text("no duplicate ERROR line here")
        self._frame([" " + line for line in [self.lines[0], self.lines[1],
                     "REPORTING FUNCTION : " + ERRORS[0][1],
                     "C++ SOURCE FILE : src/item_location.cpp", "LINE : 388",
                     "VERSION : f4bef7f87e-dirty", *self.lines[-2:]]])
        original_snapshot = self._snapshot
        self._snapshot = lambda pid: {"pid": pid, "alive": False} if pid == self.bridge["pid"] else original_snapshot(pid)

    def test_retained_native_warning_without_profile_duplicate_uses_existing_transaction(self):
        self._retained_native_warning()
        seen = self._observe()
        self.assertEqual(seen["state"], "confirmed_debug_dialog")
        self.assertEqual(seen["source_file"], "src/item_location.cpp")
        self.assertEqual(seen["source_line"], 388)
        self.assertEqual(seen["message_source"], "current_run_owned_terminal_frame")
        self.assertTrue(seen["retained_startup"])
        with patch("curses_terminal_transport.dispatch_input", side_effect=self._receipt) as send:
            result = self._recover(seen["capture_sha256"])
        self.assertTrue(result["ok"])
        send.assert_called_once()
        self.assertEqual(send.call_args.kwargs["process_generation"], self.generation)
        attempt = json.loads(Path(result["attempt_path"]).read_text())
        self.assertEqual(attempt["birth_identity"], self.generation["birth_identity"])
        self.assertEqual(attempt["current_dialog"]["capture_sha256"], seen["capture_sha256"])
        self.assertTrue(Path(result["result_path"]).exists())

    def test_retained_recovery_refuses_changed_capture_owner_and_unknown_reporter(self):
        self._retained_native_warning()
        capture = self._observe()["capture_sha256"]
        original_snapshot = self._snapshot
        for changed_pid in (self.generation["pid"], self.broker["pid"], self.bridge["pid"]):
            self._snapshot = lambda pid: ({**original_snapshot(pid), "birth_identity": "other"} if pid == changed_pid else original_snapshot(pid)) if changed_pid != self.bridge["pid"] else ({"pid":pid,"alive":True} if pid == changed_pid else original_snapshot(pid))
            with patch("curses_terminal_transport.dispatch_input") as send:
                result = self._recover(capture)
            self.assertFalse(result["ok"])
            send.assert_not_called()
        self._snapshot = original_snapshot
        self._frame(["\x1b[2J", "Loading character save"])
        with patch("curses_terminal_transport.dispatch_input") as send:
            result = self._recover(capture)
        self.assertFalse(result["ok"])
        send.assert_not_called()

    def test_native_provenance_requires_complete_current_block(self):
        self._retained_native_warning()
        original = self.transcript.read_text()
        for field in ("C++ SOURCE FILE : src/item_location.cpp", "LINE : 388",
                      "VERSION : f4bef7f87e-dirty", self.lines[-1]):
            self.transcript.write_text(original.replace(field, ""))
            self.assertNotEqual(self._observe()["state"], "confirmed_debug_dialog")

    def test_valid_terminal_warning_single_dispatch_and_attempt(self):
        seen = self._observe()
        self.assertEqual(seen["state"], "confirmed_debug_dialog")
        self.assertEqual(seen["message"], ERRORS[0][2])
        self.assertTrue(seen["ignore_advertised"])
        with patch("curses_terminal_transport.dispatch_input", side_effect=self._receipt) as send:
            result = self._recover(seen["capture_sha256"])
        self.assertTrue(result["ok"])
        send.assert_called_once()
        self.assertEqual(send.call_args.kwargs["keys"], ["i"])
        self.assertEqual(result["authorization"]["scope"], "startup_only")
        self.assertTrue(Path(result["attempt_path"]).exists())
        self.assertEqual(json.loads(Path(result["result_path"]).read_text())["state"], "input_reported_delivered")
        output = plain_player_output({"ok": False, "status": self.status, "startup_dialog": seen})
        self.assertIn("terminal frame", output)
        self.assertIn("debug-ignore --capture " + seen["capture_sha256"], output)

    def test_stale_owner_wrong_run_and_private_mismatch_refuse(self):
        original = dict(self.terminal)
        for update in [{"run_id": "other-run"}, {"game_pid": 123},
                       {"broker_process_generation": {**self.broker, "birth_identity": "stale"}}]:
            with self.subTest(update=update):
                self.terminal = {**original, **update}
                self._owners()
                with patch("curses_terminal_transport.dispatch_input") as send:
                    result = self._recover("a" * 64)
                self.assertEqual(result["state"], "input_not_sent")
                send.assert_not_called()
        self.terminal = original
        self._owners()
        (self.endpoint.parent / "owner.json").write_text("{}")
        self.assertEqual(self._observe()["state"], "unavailable")

    def test_absent_changed_and_unmatched_warning_and_absent_ignore_refuse(self):
        captured = self._observe()["capture_sha256"]
        for lines in [["Loading"], self.lines[:3],
                      [self.lines[0], "DEBUG : unknown error", *self.lines[2:]],
                      [self.lines[0], "DEBUG : " + ERRORS[1][2], *self.lines[2:]]]:
            with self.subTest(lines=lines):
                self._frame(lines)
                with patch("curses_terminal_transport.dispatch_input") as send:
                    result = self._recover(captured)
                self.assertFalse(result["ok"])
                send.assert_not_called()

    def test_ready_wrong_binding_and_generation_refuse(self):
        capture = self._observe()["capture_sha256"]
        for update in [{"state": "ready"}, {"state": "process_dead"}, {"binding_id": "other"},
                       {"session_generation": 1}, {"bridge_process_generation": {**self.bridge, "alive": False}}]:
            with self.subTest(update=update):
                self._write("status.json", {**self.status, **update})
                with patch("curses_terminal_transport.dispatch_input") as send:
                    result = self._recover(capture)
                self.assertFalse(result["ok"])
                send.assert_not_called()

    def test_only_exact_selected_debug_ignore_permission_is_accepted(self):
        capture = self._observe()["capture_sha256"]
        original = json.loads(self.declaration.read_text())
        for mutation in ["remove_permission", "allow_keyboard", "different_name", "extra_permission"]:
            value = json.loads(json.dumps(original))
            contract = value["runtime_contract"]
            if mutation == "remove_permission": contract["permitted_input"].remove("debug:ignore")
            elif mutation == "allow_keyboard": contract["forbidden_input"].remove("keyboard")
            elif mutation == "different_name": value["name"] = "different"
            else: contract["permitted_input"].append("debug:teleport")
            self.declaration.write_text(json.dumps(value, indent=2) + "\n")
            with patch("curses_terminal_transport.dispatch_input") as send:
                result = self._recover(capture)
            self.assertFalse(result["ok"])
            self.assertEqual(result["reason"], "startup_debug_ignore_permission_unavailable")
            send.assert_not_called()

    def test_changed_serialization_and_old_hash_additive_grant_refuse(self):
        from hashlib import sha256
        capture = self._observe()["capture_sha256"]
        selected = self.declaration.read_bytes()
        declaration = json.loads(selected)
        for changed in [json.dumps(declaration).encode(),
                        (json.dumps(declaration, indent=2) + "\n\n").encode()]:
            self.declaration.write_bytes(changed)
            with patch("curses_terminal_transport.dispatch_input") as send:
                result = self._recover(capture)
            self.assertEqual(result["reason"], "startup_debug_ignore_permission_unavailable")
            send.assert_not_called()
        self.declaration.write_bytes(selected)
        declaration["runtime_contract"]["permitted_input"].remove("debug:ignore")
        preflight = json.loads((self.run / "contract.preflight.json").read_text())
        preflight["scenario_source"]["sha256"] = sha256(
            (json.dumps(declaration, indent=2) + "\n").encode()).hexdigest()
        (self.run / "contract.preflight.json").write_text(json.dumps(preflight))
        with patch("curses_terminal_transport.dispatch_input") as send:
            result = self._recover(capture)
        self.assertEqual(result["reason"], "startup_debug_ignore_permission_unavailable")
        send.assert_not_called()

    def test_uncertain_delivery_and_acknowledgement_mismatch_record_unknown_without_retry(self):
        capture = self._observe()["capture_sha256"]
        for behavior in [TimeoutError("lost acknowledgement"), {"ok": True, "request_id": "wrong"}, None]:
            with self.subTest(behavior=behavior):
                options = {"side_effect": behavior} if isinstance(behavior, Exception) else {"return_value": behavior}
                with patch("curses_terminal_transport.dispatch_input", **options) as send:
                    result = self._recover(capture)
                self.assertEqual(result["state"], "input_outcome_unknown")
                output = plain_player_output(result)
                self.assertIn("Input outcome unknown:", output)
                self.assertNotIn("Input not sent:", output)
                self.assertIn("do not retry automatically", output)
                self.assertFalse(result["ok"])
                send.assert_called_once()
                self.assertEqual(json.loads(Path(result["result_path"]).read_text())["state"], "input_outcome_unknown")

    def test_ambiguous_exact_log_sources_refuse(self):
        with self.log.open("a") as stream:
            stream.write("20:33:53.928 ERROR : src/other.cpp:99 [test] " + ERRORS[0][2] + "\n")
        with patch("curses_terminal_transport.dispatch_input") as send:
            result = self._recover("a" * 64)
        self.assertFalse(result["ok"])
        self.assertEqual(result["before"]["state"], "debug_dialog_unmatched")
        send.assert_not_called()

    def test_previous_modal_erased_from_current_screen_is_not_recoverable(self):
        raw = self.transcript.read_text()
        self.transcript.write_text(raw + "\x1b[2J\x1b[HWorld ready")
        self.assertNotEqual(self._observe()["state"], "confirmed_debug_dialog")


if __name__ == "__main__":
    unittest.main()
