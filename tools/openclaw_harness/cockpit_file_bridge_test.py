#!/usr/bin/env python3
"""Contract tests for the file-backed live cockpit bridge."""
from __future__ import annotations

import json
import io
import hashlib
import sys
import tempfile
import threading
import time
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit_file_bridge import FileBackedCockpitBridge, FileBackedCockpitClient, FreshObservationSequence, main


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

DELAYED_CHILD = (
    "import json,sys,time; "
    "[time.sleep(0.1) or print(json.dumps({'ok':True,'result':json.loads(line)}),flush=True) "
    "for line in sys.stdin]"
)

OBSERVATION_CHAIN_CHILD = (
    "import json,sys\n"
    "frame=0\n"
    "for line in sys.stdin:\n"
    " request=json.loads(line)\n"
    " if request.get('action') == 'game.observe':\n"
    "  frame += 1; result={'ok':True,'result':{'observation_id':f'frame:{frame}'}}\n"
    " else:\n"
    "  result={'ok':True,'result':request}\n"
    " print(json.dumps(result),flush=True)\n"
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
            self.assertTrue(status["response"]["result"]["observation"]["omitted"])
            self.assertEqual(status["response"]["result"]["observation"]["count"], 2000000)
            self.assertNotIn("x" * 1000, json.dumps(status))
            recovered = bridge.response_artifact(
                directory, "observe-1", status["receipt"]["response_sha256"],
            )
            self.assertTrue(recovered["ok"])
            self.assertEqual(recovered["response"]["result"]["sequence"], 1)
            self.assertGreater(len(recovered["response"]["result"]["observation"]), 1000000)
            self.assertEqual(
                bridge.response_artifact(directory, "observe-1", "0" * 64),
                {"ok": False, "error": "response_artifact_digest_mismatch"},
            )
            self.assertEqual(bridge.response_slice(directory, "observe-1", "result.sequence")["slice"], 1)
            artifact.write_text("{}\n", encoding="utf-8")
            self.assertEqual(
                bridge.response_artifact(directory, "observe-1", status["receipt"]["response_sha256"]),
                {"ok": False, "error": "response_artifact_hash_mismatch"},
            )
            self.assertEqual(json.loads((directory / "status.json").read_text())["last_response"]["request_id"], "observe-1")
            receipt = json.loads((directory / "responses" / "observe-1.receipt.json").read_text())
            self.assertEqual(receipt["request_identity"], {"action": "game.observe"})
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_response_artifact_cli_recovers_only_the_receipt_bound_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            responses = directory / "responses"
            responses.mkdir(parents=True)
            raw = b'{"ok":true,"result":{"witness":{"verdict":"proved"}}}\n'
            digest = hashlib.sha256(raw).hexdigest()
            (responses / "witness-1.json").write_bytes(raw)
            (responses / "witness-1.receipt.json").write_text(json.dumps({
                "request_id": "witness-1", "binding_id": "bound-a",
                "response_sha256": digest, "response_artifact": "responses/witness-1.json",
            }), encoding="utf-8")
            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(main([
                    "response-artifact", "--session-dir", str(directory),
                    "--request-id", "witness-1", "--sha256", digest,
                ]), 0)
            recovered = json.loads(output.getvalue())
            self.assertEqual(recovered["response"]["result"]["witness"]["verdict"], "proved")
            rejected_output = io.StringIO()
            with redirect_stdout(rejected_output):
                self.assertEqual(main([
                    "response-artifact", "--session-dir", str(directory),
                    "--request-id", "witness-1", "--sha256", "0" * 64,
                ]), 0)
            self.assertEqual(json.loads(rejected_output.getvalue()), {
                "ok": False, "error": "response_artifact_digest_mismatch",
            })
            (responses / "witness-1.receipt.json").write_text(json.dumps({
                "request_id": "witness-1", "response_sha256": digest,
                "response_artifact": "../outside.json",
            }), encoding="utf-8")
            self.assertEqual(
                FileBackedCockpitBridge.response_artifact(directory, "witness-1", digest),
                {"ok": False, "error": "response_artifact_path_invalid"},
            )

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

    def test_delayed_response_collection_never_resubmits_request_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", DELAYED_CHILD], binding_id="bound-a",
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] == "starting":
                time.sleep(0.01)

            submitted = FileBackedCockpitClient.submit_once(
                directory, request_id="pause-1", binding_id="bound-a",
                request={"action": "game.act", "action_id": "world.pause"},
            )
            self.assertEqual(submitted, {"ok": True, "request_id": "pause-1"})
            self.assertFalse(FileBackedCockpitClient.collect_response(directory, "pause-1")["ok"])
            while not FileBackedCockpitClient.collect_response(directory, "pause-1")["ok"]:
                time.sleep(0.01)
            self.assertEqual(
                FileBackedCockpitClient.collect_response(directory, "pause-1")["receipt"]["request_id"],
                "pause-1",
            )

            # The client collected the delayed artifact.  A second submission
            # remains a bridge violation and must still fail closed.
            self.assertTrue(FileBackedCockpitBridge.send_request(
                directory, request_id="pause-1", binding_id="bound-a",
                request={"action": "game.act", "action_id": "world.pause"},
            )["ok"])
            time.sleep(0.05)
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "rejected")
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_final3_style_five_step_observe_act_chain_keeps_ids_and_observations_unique(self):
        """Regression: no extra status observe or retry may reuse a pause id."""
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", OBSERVATION_CHAIN_CHILD], binding_id="bound-a",
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] == "starting":
                time.sleep(0.01)

            client = FreshObservationSequence(directory, "bound-a", "final3")

            def collect_observation() -> str:
                request_id = client.observe()
                while not (observation_id := client.accept_observation(request_id)):
                    time.sleep(0.01)
                return observation_id

            first = collect_observation()
            pause_one = client.act("world.pause")
            while not client.collect(pause_one).get("ok"):
                time.sleep(0.01)
            second = collect_observation()
            pause_two = client.act("world.pause")
            while not client.collect(pause_two).get("ok"):
                time.sleep(0.01)
            third = collect_observation()

            self.assertEqual((first, second, third), ("frame:1", "frame:2", "frame:3"))
            receipts = sorted((directory / "responses").glob("*.receipt.json"))
            self.assertEqual(len(receipts), 5)
            identities = [json.loads(path.read_text())["request_id"] for path in receipts]
            self.assertEqual(len(identities), len(set(identities)))
            self.assertTrue(bridge.cleanup(directory, "bound-a")["ok"])
            thread.join(2)
            self.assertFalse(thread.is_alive())

    def test_delayed_collection_preserves_monotonic_shared_transition_sequence(self):
        with tempfile.TemporaryDirectory() as temp:
            run_dir = Path(temp)
            stream = run_dir / "transition.events.jsonl"
            stream.write_text("".join(
                json.dumps({"run_id": "run-a", "sequence": sequence}) + "\n"
                for sequence in range(1, 244)
            ), encoding="utf-8")
            request = {"request_id": "delayed-244", "action_id": "world.pause"}
            receipt = {"accepted": True, "resulting_frame_id": "frame-244"}
            from startup_harness import append_semantic_surface_transition_event
            event = append_semantic_surface_transition_event(run_dir, "run-a", request, receipt)
            self.assertEqual(event["sequence"], 244)
            sequences = [json.loads(line)["sequence"] for line in stream.read_text().splitlines()]
            self.assertEqual(sequences, list(range(1, 245)))

    def test_sequential_sessions_reject_prior_binding_and_frame(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first, first_thread = self.start(root / "first")
            self.assertTrue(first.send_request(
                root / "first", request_id="frame-1", binding_id="bound-a",
                request={"action": "game.observe"},
            )["ok"])
            while not (root / "first" / "responses" / "frame-1.receipt.json").is_file():
                time.sleep(0.01)
            self.assertTrue(first.cleanup(root / "first", "bound-a")["ok"])
            first_thread.join(2)

            second = FileBackedCockpitBridge(
                root / "second", [sys.executable, "-u", "-c", CHILD], binding_id="bound-b",
            )
            second_thread = threading.Thread(target=second.serve, daemon=True)
            second_thread.start()
            while not (root / "second" / "status.json").is_file() or \
                    json.loads((root / "second" / "status.json").read_text())["state"] == "starting":
                time.sleep(0.01)
            self.assertFalse(second.send_request(
                root / "second", request_id="frame-1", binding_id="bound-a",
                request={"action": "game.observe"},
            )["ok"])
            self.assertFalse(second.response_status(root / "second", "frame-1")["ok"])
            self.assertTrue(second.send_request(
                root / "second", request_id="frame-2", binding_id="bound-b",
                request={"action": "game.observe"},
            )["ok"])
            while not (root / "second" / "responses" / "frame-2.receipt.json").is_file():
                time.sleep(0.01)
            self.assertTrue(second.cleanup(root / "second", "bound-b")["ok"])
            second_thread.join(2)

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

    def test_startup_rejection_is_retained_before_descriptor_validation(self):
        child = "import json; print(json.dumps({'ok':False,'reason':'contract_preflight_rejected'}),flush=True)"
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
                require_session_ready=True,
            )
            self.assertEqual(bridge.serve(), 1)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["reason"], "missing_cockpit_session_descriptor")
            retained = [json.loads(line) for line in
                        (directory / "child.startup.stdout.jsonl").read_text().splitlines()]
            self.assertEqual(retained, [{"ok": False, "reason": "contract_preflight_rejected"}])

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
            while json.loads((directory / "status.json").read_text())["state"] in {"starting", "preparing"}:
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

    def test_declared_reentry_binds_a_second_live_session_after_first_finish(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "session"
            child = (
                "import json,os,sys; "
                "d=lambda n:{'schema':'caol-cockpit-live-session-v1','entry_mode':'cockpit_live_session',"
                "'run_id':'run-a','binding_id':'native-'+n,'bridge_binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID']}; "
                "print(json.dumps({'cockpit_live_session':d('first')}),flush=True); sys.stdin.readline(); "
                "print(json.dumps({'ok':True,'result':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True); "
                "print(json.dumps({'cockpit_live_session':d('second')}),flush=True); sys.stdin.readline(); "
                "print(json.dumps({'ok':True,'result':{'schema':'caol-cockpit-live-final-v1','state':'finished'}}),flush=True); "
                "open(os.path.join(os.environ['OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR'],'cockpit.bridge.safe_to_cleanup.json'),'w').write(json.dumps({'schema':'caol-cockpit-scenario-terminalization-v1','binding_id':os.environ['OPENCLAW_COCKPIT_BRIDGE_BINDING_ID'],'state':'safe_to_cleanup'}))"
            )
            bridge = FileBackedCockpitBridge(
                directory, [sys.executable, "-u", "-c", child], binding_id="bound-a",
                require_session_ready=True, session_reentries=1,
            )
            thread = threading.Thread(target=bridge.serve, daemon=True)
            thread.start()
            while not (directory / "status.json").is_file() or \
                    json.loads((directory / "status.json").read_text())["state"] != "ready":
                time.sleep(0.01)
            self.assertTrue(bridge.send_request(
                directory, request_id="finish-first", binding_id="bound-a",
                request={"action": "run.finish"},
            )["ok"])
            while json.loads((directory / "status.json").read_text()).get(
                    "session_descriptor", {}).get("binding_id") != "native-second":
                time.sleep(0.01)
            status = json.loads((directory / "status.json").read_text())
            self.assertEqual(status["state"], "ready")
            self.assertEqual(status["remaining_session_reentries"], 0)
            # send_request retains this envelope in the spool only.  The
            # bridge must admit that exact post-reentry identity once, rather
            # than requiring or accepting a FIFO replay.
            self.assertTrue(bridge.send_request(
                directory, request_id="keep-watch-after-relaunch", binding_id="bound-a",
                request={"action": "game.keep_watch"},
            )["ok"])
            while not (directory / "responses" / "keep-watch-after-relaunch.receipt.json").is_file():
                time.sleep(0.01)
            receipt = json.loads((directory / "responses" /
                                  "keep-watch-after-relaunch.receipt.json").read_text())
            self.assertEqual(receipt["request_id"], "keep-watch-after-relaunch")
            thread.join(2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(json.loads((directory / "status.json").read_text())["state"], "safe_to_cleanup")

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
