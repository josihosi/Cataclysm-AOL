#!/usr/bin/env python3
"""Contract tests for the file-backed live cockpit bridge."""
from __future__ import annotations

import json
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_file_bridge import FileBackedCockpitBridge


CHILD = (
    "import json,sys; "
    "[print(json.dumps({'ok':True,'result':{'observation':'x'*2000000,'sequence':i}}),flush=True) "
    "for i,line in enumerate(sys.stdin,1)]"
)

SESSION_CHILD = (
    "import json,os,sys; "
    "print(json.dumps({'cockpit_live_session':{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}}),flush=True); "
    "[print(json.dumps({'ok':True,'result':json.loads(line)}),flush=True) for line in sys.stdin]"
)

PREFIX = [
    {"label": "first", "objective": "first window", "required_action_chain": ["world.wait"],
     "adaptive_interrupt_actions": ["activity.ignore"]},
    {"label": "second", "objective": "second window", "required_action_chain": ["wait.6h"],
     "adaptive_interrupt_actions": []},
]

PREFIX_CHILD = (
    "import json,os,sys; "
    "[print(json.dumps({'semantic_session':{'schema':'caol-adaptive-semantic-session-v1','run_id':'run-a',"
    "'objective':objective,'required_action_chain':actions,'adaptive_interrupt_actions':interrupts}}),flush=True) "
    "for objective,actions,interrupts in [('first window',['world.wait'],['activity.ignore']),"
    "('second window',['wait.6h'],[])]]; "
    "print(json.dumps({'cockpit_live_session':{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session',"
    "'run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}}),flush=True); "
    "[print(json.dumps({'ok':True,'result':json.loads(line)}),flush=True) for line in sys.stdin]"
)

HUD_PROGRESS_CHILD = (
    "import json,os,sys; "
    "print(json.dumps({'cockpit_bridge_progress':{'schema':'caol-cockpit-bridge-progress-v1',"
    "'state':'startup_hud_ready','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],"
    "'run_id':'run-a','pid':42,'gameplay_credit':False}}),flush=True); "
    "print(json.dumps({'cockpit_live_session':{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session',"
    "'run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}}),flush=True); "
    "[print(json.dumps({'ok':True,'result':json.loads(line)}),flush=True) for line in sys.stdin]"
)


class CockpitFileBridgeTest(unittest.TestCase):
    def start(self, directory: Path):
        bridge = FileBackedCockpitBridge(directory, [sys.executable, "-u", "-c", CHILD], binding_id="bound-a")
        thread = threading.Thread(target=bridge.serve, daemon=True)
        thread.start()
        while not (directory / "status.json").is_file():
            time.sleep(0.01)
        while json.loads((directory / "status.json").read_text())["state"] == "starting":
            time.sleep(0.01)
        return bridge, thread

    def test_large_response_is_retained_without_public_stdout_backpressure(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge, thread = self.start(directory)
            self.assertEqual(bridge.send_request(directory, request_id="observe-1", binding_id="bound-a",
                                                  request={"action": "game.observe"}),
                             {"ok": True, "request_id": "observe-1"})
            while not (directory / "responses" / "observe-1.receipt.json").is_file():
                time.sleep(0.01)
            status = bridge.response_status(directory, "observe-1")
            self.assertTrue(status["ok"])
            artifact = directory / status["receipt"]["response_artifact"]
            self.assertGreater(artifact.stat().st_size, 1000000)
            self.assertEqual(bridge.response_slice(directory, "observe-1", "result.sequence")["slice"], 1)
            self.assertEqual(json.loads((directory / "status.json").read_text())["last_response"]["request_id"], "observe-1")
            receipt = json.loads((directory / "responses" / "observe-1.receipt.json").read_text())
            self.assertEqual(receipt["request_identity"], {"action": "game.observe"})
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_request_identity_order_stale_rejection_and_complete_artifacts(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge, thread = self.start(directory)
            for request_id in ("one", "two"):
                self.assertTrue(bridge.send_request(directory, request_id=request_id, binding_id="bound-a",
                                                    request={"action": "game.observe"})["ok"])
                while not (directory / "responses" / (request_id + ".receipt.json")).is_file():
                    time.sleep(0.01)
            one = bridge.response_status(directory, "one")["receipt"]
            two = bridge.response_status(directory, "two")["receipt"]
            self.assertEqual((one["sequence"], two["sequence"]), (1, 2))
            self.assertFalse(bridge.response_status(directory, "missing")["ok"])
            self.assertTrue(bridge.send_request(directory, request_id="one", binding_id="bound-a",
                                                request={"action": "game.observe"})["ok"])
            time.sleep(0.05)
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "rejected")
            self.assertFalse(bridge.send_request(directory, request_id="three", binding_id="wrong",
                                                 request={"action": "game.observe"})["ok"])
            bridge.cleanup(directory, "bound-a")
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_session_descriptor_fail_closed_on_missing_wrong_or_legacy_binding(self):
        descriptors = (
            {},
            {"schema": "caol-cockpit-live-session-v1", "entry_mode": "cockpit_live_session",
             "binding_id": "native-a", "bridge_binding_id": "bound-a"},
            {"schema": "caol-cockpit-live-session-v1", "entry_mode": "cockpit_live_session",
             "run_id": "run-a", "binding_id": "native-a", "bridge_binding_id": "wrong"},
            {"schema": "caol-cockpit-live-session-v1"},
        )
        for descriptor in descriptors:
            with self.subTest(descriptor=descriptor), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp) / "session"
                child = (
                    "import json,time; print(json.dumps({'cockpit_live_session':"
                    + repr(descriptor) + "}),flush=True); time.sleep(5)"
                )
                bridge = FileBackedCockpitBridge(
                    directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
                    require_session_ready=True,
                )
                self.assertEqual(bridge.serve(), 1)
                status = json.loads((directory / "status.json").read_text())
                self.assertEqual(status["state"], "process_dead")
                self.assertEqual(status["cleanup"], {"status": "accepted"})

    def test_process_death_is_reported_and_cleanup_is_owned(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(directory, [sys.executable, "-c", "pass"], binding_id="bound-a")
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file():
                time.sleep(0.01)
            time.sleep(0.05)
            bridge.send_request(directory, request_id="dead", binding_id="bound-a", request={"action": "game.observe"})
            time.sleep(0.05)
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "process_dead")
            thread.join(2)

    def test_child_exit_before_response_retains_stderr_and_no_response_receipt(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = "import sys; [(_ for _ in ()).throw(RuntimeError('before response')) for _ in sys.stdin]"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file():
                time.sleep(0.01)
            self.assertTrue(bridge.send_request(
                directory, request_id="dies", binding_id="bound-a", request={"action": "game.observe"},
            )["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["state"], "process_dead")
            self.assertEqual(status["failed_request_id"], "dies")
            self.assertFalse((directory / "responses" / "dies.receipt.json").exists())
            self.assertIn("before response", (directory / "child.stderr.log").read_text())

    def test_session_descriptor_is_consumed_before_the_first_request(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", SESSION_CHILD], binding_id="bound-a",
                require_session_ready=True,
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file():
                time.sleep(0.01)
            while json.loads((directory / "status.json").read_text())["state"] == "starting":
                time.sleep(0.01)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["session_descriptor"]["schema"], "caol-cockpit-live-session-v1")
            self.assertTrue(bridge.send_request(directory, request_id="observe-1", binding_id="bound-a",
                                                request={"action": "game.observe"})["ok"])
            while not (directory / "responses" / "observe-1.receipt.json").is_file():
                time.sleep(0.01)
            self.assertEqual(bridge.response_slice(directory, "observe-1", "result.action")["slice"], "game.observe")
            bridge.cleanup(directory, "bound-a")
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_bound_startup_hud_progress_transitions_to_ready_before_requests(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", HUD_PROGRESS_CHILD], binding_id="bound-a",
                require_session_ready=True,
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["startup_progress"]["state"], "startup_hud_ready")
            self.assertFalse(status["startup_progress"]["gameplay_credit"])
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_startup_progress_without_session_descriptor_fails_closed(self):
        child = (
            "import json,os; print(json.dumps({'cockpit_bridge_progress':"
            "{'schema':'caol-cockpit-bridge-progress-v1','state':'startup_hud_ready',"
            "'bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],"
            "'run_id':'run-a','pid':42,'gameplay_credit':False}}),flush=True)"
        )
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
                require_session_ready=True,
            )
            self.assertEqual(bridge.serve(), 1)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["state"], "process_dead")
            self.assertEqual(status["reason"], "pre_descriptor_no_progress")
            self.assertEqual(status["startup_progress"]["state"], "startup_hud_ready")

    def test_declared_pre_descriptor_prefix_is_retained_as_zero_credit_setup(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", PREFIX_CHILD], binding_id="bound-a",
                require_session_ready=True, pre_descriptor_prefix=PREFIX,
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file():
                time.sleep(0.01)
            while json.loads((directory / "status.json").read_text())["state"] == "starting":
                time.sleep(0.01)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["state"], "ready")
            self.assertEqual(status["bootstrap_receipts"], 2)
            receipts = [json.loads(line) for line in
                        (directory / "pre_descriptor.receipts.jsonl").read_text().splitlines()]
            self.assertEqual([receipt["sequence"] for receipt in receipts], [1, 2])
            self.assertTrue(all(receipt["gameplay_credit"] is False for receipt in receipts))
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_pre_descriptor_rejects_undeclared_or_credit_bearing_action(self):
        child = (
            "import json,time; print(json.dumps({'semantic_session':"
            "{'schema':'caol-adaptive-semantic-session-v1','run_id':'run-a','objective':'other',"
            "'required_action_chain':['world.wait'],'adaptive_interrupt_actions':[],"
            "'gameplay_credit':True}}),flush=True); time.sleep(5)"
        )
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
                require_session_ready=True, pre_descriptor_prefix=PREFIX[:1],
            )
            self.assertEqual(bridge.serve(), 1)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["reason"], "undeclared_pre_descriptor_action")
            self.assertEqual(status["cleanup"], {"status": "accepted"})

    def test_live_finish_keeps_child_until_scenario_terminalization_signals_safe_cleanup(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = (
                "import json,os,sys,time; "
                "d={'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}; "
                "print(json.dumps({'cockpit_live_session':d}),flush=True); "
                "line=json.loads(sys.stdin.readline()); "
                "print(json.dumps({'ok':True,'result':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True); time.sleep(.2); "
                "open(os.path.join(os.environ['OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR'],'cockpit.bridge.safe_to_cleanup.json'),'w').write(json.dumps({'schema':'caol-cockpit-scenario-terminalization-v1','binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],'state':'safe_to_cleanup'}))"
            )
            bridge = FileBackedCockpitBridge(directory, [sys.executable, "-u", "-c", child],
                                              binding_id="bound-a", require_session_ready=True)
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            self.assertTrue(bridge.send_request(directory, request_id="finish", binding_id="bound-a",
                                                request={"action":"run.finish"})["ok"])
            while json.loads((directory / "status.json").read_text())["state"] == "ready":
                time.sleep(0.01)
            self.assertFalse(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["state"], "safe_to_cleanup")
            self.assertEqual(status["cleanup"]["owner"], "scenario_terminalization")

    def test_fail_closed_final_under_error_also_terminalizes_the_scenario(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = (
                "import json,os,sys; "
                "d={'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}; "
                "print(json.dumps({'cockpit_live_session':d}),flush=True); sys.stdin.readline(); "
                "print(json.dumps({'ok':False,'error':'fail_closed','final':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True); "
                "open(os.path.join(os.environ['OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR'],'cockpit.bridge.safe_to_cleanup.json'),'w').write(json.dumps({'schema':'caol-cockpit-scenario-terminalization-v1','binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],'state':'safe_to_cleanup'}))"
            )
            bridge = FileBackedCockpitBridge(directory, [sys.executable, "-u", "-c", child],
                                              binding_id="bound-a", require_session_ready=True)
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            self.assertTrue(bridge.send_request(
                directory, request_id="failed", binding_id="bound-a", request={"action": "game.keep_watch"},
            )["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "safe_to_cleanup")

    def test_live_finish_drains_terminal_stdout_before_waiting_for_child_exit(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = (
                "import json,os,sys; "
                "d={'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}; "
                "print(json.dumps({'cockpit_live_session':d}),flush=True); "
                "sys.stdin.readline(); "
                "print(json.dumps({'ok':True,'result':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True); "
                "open(os.path.join(os.environ['OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR'],'cockpit.bridge.safe_to_cleanup.json'),'w').write(json.dumps({'schema':'caol-cockpit-scenario-terminalization-v1','binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],'state':'safe_to_cleanup'})); "
                "sys.stdout.write('x'*200000); sys.stdout.flush()"
            )
            bridge = FileBackedCockpitBridge(directory, [sys.executable, "-u", "-c", child],
                                              binding_id="bound-a", require_session_ready=True)
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            self.assertTrue(bridge.send_request(
                directory, request_id="finish", binding_id="bound-a", request={"action":"run.finish"},
            )["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "safe_to_cleanup")
            self.assertEqual((directory / "terminal.stdout.log").stat().st_size, 200000)

    def test_live_finish_without_terminalization_signal_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = (
                "import json,os,sys; "
                "print(json.dumps({'cockpit_live_session':{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session','run_id':'run-a','binding_id':'native-a','bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}}),flush=True); "
                "sys.stdin.readline(); print(json.dumps({'ok':True,'result':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True)"
            )
            bridge = FileBackedCockpitBridge(directory, [sys.executable, "-u", "-c", child],
                                              binding_id="bound-a", require_session_ready=True)
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            bridge.send_request(directory, request_id="finish", binding_id="bound-a", request={"action":"run.finish"})
            thread.join(2)
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "terminalization_failed")


if __name__ == "__main__":
    unittest.main()
