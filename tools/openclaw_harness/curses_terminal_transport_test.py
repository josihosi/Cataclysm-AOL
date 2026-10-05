#!/usr/bin/env python3
"""Focused checks for the harness-owned curses PTY bootstrap."""

from __future__ import annotations

import subprocess
import sys
import json
import socket
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from curses_terminal_transport import (
    _key_bytes, cleanup_dispatcher, dispatch_input, dispatcher_status,
    should_use_curses_terminal,
)


class CursesTerminalTransportTest(unittest.TestCase):
    def test_terminal_charset_controls_do_not_shift_current_text(self) -> None:
        from startup_harness import render_curses_terminal_screen
        raw = "\x1b[1;1H\x1b(0x\x1b(BDebt $12\x1b[1;8H34"
        self.assertEqual(render_curses_terminal_screen(raw), ["xDebt $34"])

    def test_terminal_vertical_position_and_erase_remove_old_modal(self) -> None:
        from startup_harness import render_curses_terminal_screen
        raw = "Old modal\x1b[2d\rGold coins\x1b[1d\r\x1b[9XDebt $4"
        self.assertEqual(render_curses_terminal_screen(raw), ["Debt $4", "Gold coins"])

    def test_text_reaches_bound_terminal_instead_of_gui(self) -> None:
        import startup_harness as harness
        generation = {"pid": 123, "birth_identity": "birth", "command": "owned game"}
        owner = {"endpoint": "/tmp/owned-terminal", "run_id": "run", "run_dir": "/tmp",
                 "host": "owned-host", "process_generation": generation}
        with mock.patch.dict(harness.TERMINAL_NATIVE_INPUTS, {123: owner}), \
                mock.patch.object(harness, "dispatch_terminal_input", return_value={"ok": True, "request_id": "text"}) as send, \
                mock.patch.object(harness, "write_json"), \
                mock.patch.object(harness, "run_peekaboo_interaction") as gui:
            receipt = harness.peekaboo_type_text(123, "gold coin")
        send.assert_called_once()
        self.assertEqual(_key_bytes(send.call_args.kwargs["keys"]), b"gold coin")
        self.assertEqual(send.call_args.kwargs["run_id"], "run")
        self.assertEqual(send.call_args.kwargs["pid"], 123)
        self.assertEqual(send.call_args.kwargs["delay_ms"], 20)
        self.assertEqual(send.call_args.kwargs["host"], "owned-host")
        self.assertEqual(send.call_args.kwargs["process_generation"], generation)
        gui.assert_not_called()
        self.assertTrue(receipt["ok"])

    def test_gui_text_keeps_existing_route(self) -> None:
        import startup_harness as harness
        with mock.patch.dict(harness.TERMINAL_NATIVE_INPUTS, {}, clear=True), \
                mock.patch.object(harness, "terminal_native_press_sequence") as send, \
                mock.patch.object(harness, "peekaboo_command", return_value=["type", "gold coin"]), \
                mock.patch.object(harness, "run_peekaboo_interaction") as gui:
            harness.peekaboo_type_text(123, "gold coin", focus_pid=False)
        send.assert_not_called()
        gui.assert_called_once_with(123, ["type", "gold coin"], focus_pid=False)

    def test_auto_selects_only_non_tiles_binary(self) -> None:
        self.assertTrue(should_use_curses_terminal(Path("cataclysm"), "auto"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm-tiles"), "auto"))
        self.assertTrue(should_use_curses_terminal(Path("cataclysm-tiles"), "pty"))
        self.assertFalse(should_use_curses_terminal(Path("cataclysm"), "pipes"))

    def test_f1_uses_xterm_ss3_sequence_for_native_trade_autobalance(self) -> None:
        self.assertEqual(_key_bytes(["F1"]), b"\x1bOP")

    def test_tab_uses_the_native_control_character_for_trade_pane_switching(self) -> None:
        self.assertEqual(_key_bytes(["tab"]), b"\t")

    def test_arrow_keys_match_curses_application_keypad_mode(self) -> None:
        import curses
        curses.setupterm("xterm-256color")
        for key, capability in (("up", "kcuu1"), ("down", "kcud1"),
                                ("left", "kcub1"), ("right", "kcuf1")):
            self.assertEqual(_key_bytes([key]), curses.tigetstr(capability))

    def test_curses_child_has_a_controlling_terminal_and_transcript(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            script = (
                "import os, sys; "
                "print('stdin_tty=' + str(os.isatty(sys.stdin.fileno()))); sys.stdout.flush()"
            )
            from curses_terminal_transport import CursesTerminalTransport

            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                process = subprocess.Popen(
                    [sys.executable, "-c", script], stdin=slave_fd, stdout=slave_fd, stderr=slave_fd,
                    start_new_session=True, preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                )
            finally:
                import os
                os.close(slave_fd)
            transport.start_reader()
            self.assertEqual(process.wait(timeout=5), 0)
            transport.close()
            self.assertIn("stdin_tty=True", (run_dir / "game.terminal.log").read_text())

    def test_detached_dispatcher_accepts_only_the_bound_run_and_pid(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            from curses_terminal_transport import CursesTerminalTransport
            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                process = subprocess.Popen(
                    [sys.executable, "-c", "import os, sys, tty, time; tty.setraw(0); print(os.read(0, 1).decode(), flush=True); time.sleep(.2)"],
                    stdin=slave_fd, stdout=slave_fd, stderr=slave_fd, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                )
            finally:
                import os
                os.close(slave_fd)
            endpoint = transport.hand_off_to_dispatcher(run_id="run-a", pid=process.pid)
            with self.assertRaisesRegex(RuntimeError, "host, run, or PID"):
                dispatch_input(endpoint, run_id="wrong-run", pid=process.pid, keys=["f"])
            with self.assertRaisesRegex(RuntimeError, "host, run, or PID"):
                dispatch_input(endpoint, run_id="run-a", pid=process.pid, keys=["f"], host="other-host")
            owner = json.loads((run_dir / "terminal.owner.json").read_text())
            stale_generation = dict(owner["game_process_generation"], birth_identity="old-birth")
            with self.assertRaisesRegex(RuntimeError, "expected process generation"):
                dispatch_input(endpoint, run_id="run-a", pid=process.pid, keys=["f"],
                               process_generation=stale_generation)
            self.assertEqual(owner["run_id"], "run-a")
            self.assertEqual(owner["game_process_generation"]["pid"], process.pid)
            self.assertTrue(owner["game_process_generation"]["birth_identity"])
            self.assertTrue(owner["broker_process_generation"]["birth_identity"])
            self.assertTrue(owner["broker_command"])
            self.assertEqual(endpoint.stat().st_mode & 0o777, 0o600)
            self.assertEqual(endpoint.parent.stat().st_mode & 0o777, 0o700)
            receipt = dispatch_input(endpoint, run_id="run-a", pid=process.pid, keys=["f"])
            self.assertTrue(receipt["ok"])
            self.assertEqual(receipt["owner"], "run_bound_pty")
            self.assertEqual(receipt["host"], owner["host"])
            deadline = time.monotonic() + 2
            while "f" not in (run_dir / "game.terminal.log").read_text() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertIn("f", (run_dir / "game.terminal.log").read_text())
            self.assertEqual(process.wait(timeout=5), 0)
            deadline = time.monotonic() + 2
            while endpoint.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertFalse(endpoint.exists())
            self.assertEqual(cleanup_dispatcher(endpoint, owner_path=run_dir / "terminal.owner.json")["status"],
                             "cleaned_after_game_exit")
            self.assertFalse(endpoint.parent.exists(), "exact private endpoint directory should be removed")
            transport.close()

    def test_live_receiver_is_retained_and_exact_child_exit_cleans_only_its_endpoint(self) -> None:
        import os
        from curses_terminal_transport import CursesTerminalTransport

        def launch(root: Path, run_id: str):
            transport, slave = CursesTerminalTransport.open(root / "game.terminal.log")
            code = "import os,tty; tty.setraw(0); print('ready',flush=True); " \
                   "key=os.read(0,1); print('key='+key.decode(),flush=True); " \
                   "os.read(0,1)"
            try:
                child = subprocess.Popen([sys.executable, "-c", code], stdin=slave,
                    stdout=slave, stderr=slave, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal)
            finally:
                os.close(slave)
            endpoint = transport.hand_off_to_dispatcher(run_id=run_id, pid=child.pid)
            deadline = time.monotonic() + 3
            while "ready" not in (root / "game.terminal.log").read_text() and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertIn("ready", (root / "game.terminal.log").read_text())
            return transport, child, endpoint

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "a").mkdir()
            (root / "b").mkdir()
            transport_a, child_a, endpoint_a = launch(root / "a", "run-a")
            transport_b, child_b, endpoint_b = launch(root / "b", "run-b")
            self.assertNotEqual(endpoint_a, endpoint_b)
            self.assertEqual(cleanup_dispatcher(endpoint_a)["status"], "retained_live_game")
            self.assertTrue(endpoint_a.exists())
            first = dispatch_input(endpoint_a, run_id="run-a", pid=child_a.pid, keys=["a"])
            self.assertTrue(first["ok"])
            self.assertEqual(first["keys"], ["a"])
            # Exit A through its own terminal. B remains live and addressable.
            dispatch_input(endpoint_a, run_id="run-a", pid=child_a.pid, keys=["q"])
            self.assertEqual(child_a.wait(timeout=5), 0)
            self.assertTrue(endpoint_b.exists())
            self.assertEqual(dispatcher_status(endpoint_b)["game_status"], "alive")
            second = dispatch_input(endpoint_b, run_id="run-b", pid=child_b.pid, keys=["b"])
            self.assertTrue(second["ok"])
            dispatch_input(endpoint_b, run_id="run-b", pid=child_b.pid, keys=["q"])
            self.assertEqual(child_b.wait(timeout=5), 0)
            self.assertEqual(cleanup_dispatcher(
                endpoint_a, owner_path=root / "a" / "terminal.owner.json")["status"],
                "cleaned_after_game_exit")
            self.assertEqual(cleanup_dispatcher(
                endpoint_b, owner_path=root / "b" / "terminal.owner.json")["status"],
                "cleaned_after_game_exit")
            self.assertFalse(endpoint_a.parent.exists())
            self.assertFalse(endpoint_b.parent.exists())
            transport_a.close()
            transport_b.close()

    def test_private_cleanup_failure_reports_exact_retained_path(self) -> None:
        import os
        from curses_terminal_transport import CursesTerminalTransport

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            transport, slave = CursesTerminalTransport.open(root / "game.terminal.log")
            try:
                child = subprocess.Popen(
                    [sys.executable, "-c", "import os,tty; tty.setraw(0); os.read(0,1)"],
                    stdin=slave, stdout=slave, stderr=slave, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                )
            finally:
                os.close(slave)
            endpoint = transport.hand_off_to_dispatcher(run_id="retained-cleanup", pid=child.pid)
            marker = endpoint.parent / "unowned-residue.txt"
            marker.write_text("keep and report")
            owner_path = root / "terminal.owner.json"
            dispatch_input(endpoint, run_id="retained-cleanup", pid=child.pid, keys=["q"])
            self.assertEqual(child.wait(timeout=5), 0)
            result = cleanup_dispatcher(endpoint, owner_path=owner_path)
            self.assertEqual(result["status"], "private_directory_retained_after_game_exit")
            self.assertEqual(result["retained_paths"], [str(marker)])
            self.assertFalse(endpoint.exists())
            self.assertFalse((endpoint.parent / "owner.json").exists())
            self.assertTrue(owner_path.exists(), "durable run owner record must remain")
            marker.unlink()
            endpoint.parent.rmdir()
            transport.close()

    def test_inherited_profile_lease_fd_stays_held_until_child_and_broker_exit(self) -> None:
        import fcntl
        import os
        from curses_terminal_transport import CursesTerminalTransport

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            lease_path = root / "profile.lease"
            lease_fd = os.open(lease_path, os.O_CREAT | os.O_RDWR, 0o600)
            fcntl.flock(lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            transport, slave = CursesTerminalTransport.open(root / "game.terminal.log")
            script = "import os,tty; tty.setraw(0); os.read(0,1)"
            try:
                child = subprocess.Popen([sys.executable, "-c", script], stdin=slave,
                    stdout=slave, stderr=slave, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal,
                    pass_fds=(lease_fd,))
            finally:
                os.close(slave)
            endpoint = transport.hand_off_to_dispatcher(
                run_id="leased-run", pid=child.pid, lease_fd=lease_fd,
            )
            os.close(lease_fd)
            owner = json.loads((root / "terminal.owner.json").read_text())
            self.assertTrue(owner["lease_fd_inherited"])

            competitor = os.open(lease_path, os.O_RDWR)
            with self.assertRaises(BlockingIOError):
                fcntl.flock(competitor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            dispatch_input(endpoint, run_id="leased-run", pid=child.pid, keys=["q"])
            self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual(cleanup_dispatcher(
                endpoint, owner_path=root / "terminal.owner.json")["status"],
                "cleaned_after_game_exit")
            deadline = time.monotonic() + 2
            acquired = False
            while time.monotonic() < deadline:
                try:
                    fcntl.flock(competitor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    acquired = True
                    break
                except BlockingIOError:
                    time.sleep(.02)
            os.close(competitor)
            self.assertTrue(acquired, "broker retained or leaked the profile lease after child exit")
            transport.close()

    def test_reconnect_rejects_wrong_birth_and_deduplicates_lost_receipt(self) -> None:
        import os
        from curses_terminal_transport import CursesTerminalTransport

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            transport, slave = CursesTerminalTransport.open(root / "game.terminal.log")
            script = "import os,tty,time; tty.setraw(0); key=os.read(0,1); print('received='+key.decode(),flush=True); time.sleep(.3)"
            try:
                child = subprocess.Popen([sys.executable, "-c", script], stdin=slave,
                    stdout=slave, stderr=slave, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal)
            finally:
                os.close(slave)
            endpoint = transport.hand_off_to_dispatcher(run_id="reconnect-run", pid=child.pid)
            owner = json.loads((root / "terminal.owner.json").read_text())
            wrong_birth = dict(owner["game_process_generation"], birth_identity="old-generation")
            forged = {
                "schema": "caol-curses-terminal-request-v1", "request_id": "wrong-birth",
                "host": owner["host"], "run_id": "reconnect-run", "pid": child.pid,
                "process_generation": wrong_birth, "keys": ["z"], "delay_ms": 0,
            }
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.connect(str(endpoint))
                client.sendall((json.dumps(forged) + "\n").encode())
                rejected = json.loads(client.recv(4096).decode())
            self.assertFalse(rejected["ok"])
            self.assertIn("process_generation_mismatch", rejected["error"])

            request_id = "retry-same-input"
            request = {
                "schema": "caol-curses-terminal-request-v1", "request_id": request_id,
                "host": owner["host"], "run_id": "reconnect-run", "pid": child.pid,
                "process_generation": owner["game_process_generation"], "keys": ["x"], "delay_ms": 0,
            }
            # Simulate a disconnected client after request delivery but before
            # receipt collection, then reconnect with the same request ID.
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.connect(str(endpoint))
                client.sendall((json.dumps(request) + "\n").encode())
            deadline = time.monotonic() + 2
            while "received=x" not in (root / "game.terminal.log").read_text() and time.monotonic() < deadline:
                time.sleep(.01)
            receipt = dispatch_input(endpoint, run_id="reconnect-run", pid=child.pid,
                                     keys=["x"], request_id=request_id)
            self.assertTrue(receipt["duplicate_request"])
            self.assertEqual(receipt["request_id"], request_id)
            changed_request = dict(request, keys=["z"])
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.connect(str(endpoint))
                client.sendall((json.dumps(changed_request) + "\n").encode())
                rejected_retry = json.loads(client.recv(4096).decode())
            self.assertFalse(rejected_retry["ok"])
            self.assertEqual(rejected_retry["error"], "duplicate_request_payload_mismatch")
            self.assertEqual((root / "game.terminal.log").read_text().count("received=x"), 1)
            self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual(cleanup_dispatcher(
                endpoint, owner_path=root / "terminal.owner.json")["status"],
                "cleaned_after_game_exit")
            transport.close()

    def test_detached_dispatcher_preserves_requested_key_spacing(self) -> None:
        import os
        from curses_terminal_transport import CursesTerminalTransport
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "terminal.log"
            transport, slave = CursesTerminalTransport.open(log)
            script = ("import os,tty,time; tty.setraw(0); print('ready',flush=True); "
                      "a=os.read(0,1); t=time.monotonic(); b=os.read(0,1); "
                      "print('paced=' + str(time.monotonic()-t > .05),flush=True); time.sleep(.2)")
            try:
                child = subprocess.Popen([sys.executable, "-c", script], stdin=slave,
                    stdout=slave, stderr=slave, start_new_session=True,
                    preexec_fn=CursesTerminalTransport.make_controlling_terminal)
            finally:
                os.close(slave)
            endpoint = transport.hand_off_to_dispatcher(run_id="paced", pid=child.pid)
            deadline = time.monotonic() + 3
            while "ready" not in log.read_text() and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertIn("ready", log.read_text())
            dispatch_input(endpoint, run_id="paced", pid=child.pid, keys=["a", "b"], delay_ms=100)
            deadline = time.monotonic() + 2
            while "paced=True" not in log.read_text() and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertIn("paced=True", log.read_text())
            self.assertEqual(child.wait(timeout=3), 0)
            deadline = time.monotonic() + 2
            while endpoint.exists() and time.monotonic() < deadline:
                time.sleep(.01)
            transport.close()
            self.assertIn("paced=True", log.read_text())

    def test_detached_dispatcher_does_not_inherit_starter_capture_streams(self) -> None:
        """A detached broker must not keep a start subprocess's stdout pipe open."""
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            from curses_terminal_transport import CursesTerminalTransport
            transport, slave_fd = CursesTerminalTransport.open(run_dir / "game.terminal.log")
            try:
                endpoint = Path("/tmp") / "caol-test-broker-stdio.sock"
                with mock.patch.object(CursesTerminalTransport, "hand_off_to_dispatcher") as handoff:
                    handoff.return_value = endpoint
                    self.assertEqual(transport.hand_off_to_dispatcher(run_id="run", pid=1), endpoint)
                # Regression coverage is source-level because the real broker
                # intentionally outlives this test's starter process.
                source = Path(__file__).with_name("curses_terminal_transport.py").read_text()
                self.assertIn("stdout=subprocess.DEVNULL", source)
                self.assertIn("stderr=subprocess.DEVNULL", source)
            finally:
                import os
                os.close(slave_fd)
                transport.close()


if __name__ == "__main__":
    unittest.main()
