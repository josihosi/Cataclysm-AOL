"""Bound process death must replace cached native input authority."""
import sys
from pathlib import Path
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cockpit import CockpitRunChannel, CockpitService


CHARTER = {"claim": "Inspect native interface usability", "material_proof": "Observed native facts",
           "current_uncertainty": "Whether the game remains actionable", "requested_evidence_ceiling": "zero-credit"}


def world():
    return {"schema_version": 1, "event": "surface_descriptor", "run_id": "run-a",
            "frame_id": "run-a:world:1", "surface_id": "world-1", "kind": "world",
            "breadcrumbs": ["world"], "payload": {"last_save_result": "unattempted"},
            "valid_actions": [{"id": action, "stable_id": "", "label": action, "enabled": True}
                              for action in ("world.move.north", "world.save_quit")]}


class ProcessExitTest(unittest.TestCase):
    def channel(self, *, alive=True, exit_code=None, dispatch=None):
        process = {"run_id": "run-a", "pid": 123, "alive": alive, "exit_code": exit_code,
                   "exit_observed_at": "2026-09-05T12:00:00Z", "evidence_ref": "game-process-exit.json"}
        read = Mock(side_effect=world)
        dispatch = dispatch or Mock()
        channel = CockpitRunChannel(read, dispatch, read_process_state=lambda: process,
                                    binding_id="bound-a", witness_charter=CHARTER,
                                    witness_identity={"scenario_id": "ergonomics", "source_identity": "src",
                                                      "executable_identity": "exe"},
                                    witness_evidence_ceiling="zero-credit")
        return channel, process, read, dispatch

    def test_dead_process_replaces_cached_world_without_native_frame_or_actions(self):
        channel, process, read, dispatch = self.channel()
        old = channel.observe()
        process["alive"] = False
        ended = channel.observe()
        self.assertEqual(read.call_count, 1)
        self.assertEqual(ended["surface"]["kind"], "process_exited")
        self.assertEqual(ended["advertised_actions"], [])
        self.assertEqual(ended["surface"]["actions"], [])
        self.assertNotIn("frame_id", ended)
        self.assertEqual(ended["surface"]["facts"]["exit_code"], None)
        self.assertFalse(ended["surface"]["facts"]["gameplay_credit"])
        self.assertEqual(ended["last_native_observation_id"], old["observation_id"])
        result = channel.act(observation_id=old["observation_id"], action_id="world.move.north")
        self.assertFalse(result["ok"])
        self.assertEqual(result["observation"], ended)
        dispatch.assert_not_called()
        self.assertEqual(channel.status()["state"], "active")
        # Re-observation does not duplicate terminal facts or revive authority.
        count = len(channel._transcript)
        self.assertEqual(channel.observe(), ended)
        self.assertEqual(len(channel._transcript), count)

    def test_exit_during_native_action_retains_receipt_and_distinguishes_exit_codes(self):
        for exit_code in (0, 7, None):
            with self.subTest(exit_code=exit_code):
                channel, process, read, _ = self.channel()
                def dispatch(frame, action):
                    process.update(alive=False, exit_code=exit_code)
                    return {"native_receipt": {"run_id": "run-a", "requested_run_id": "run-a", "requested_frame_id": frame["frame_id"],
                            "action_id": action, "accepted": True,
                            "requested_surface_id": "world-1", "consuming_surface_id": "world-1",
                            "consuming_frame_id": frame["frame_id"]}}
                channel._dispatch_advertised_action = dispatch
                observed = channel.observe()
                result = channel.act(observation_id=observed["observation_id"], action_id="world.save_quit")
                self.assertEqual(result["ok"], exit_code == 0)
                self.assertTrue(result["receipt"]["native_receipt"]["accepted"])
                self.assertEqual(result["observation"]["surface"]["facts"]["exit_code"], exit_code)
                self.assertFalse(result["observation"]["surface"]["facts"]["gameplay_credit"])
                self.assertEqual(read.call_count, 1)
                self.assertEqual(channel.status()["state"], "active")

    def test_exit_facts_remain_citable_through_journal_and_finish(self):
        channel, process, _, _ = self.channel(alive=False, exit_code=0)
        service = CockpitService(run_channel=channel)
        ended = service.call({"action": "game.observe"})["result"]
        blocked = service.call({"action": "game.wait", "wait": {}})
        self.assertEqual(blocked["error"], "game_process_exited")
        sealed = service.call({"action": "run.witness", "observation_id": ended["observation_id"],
                               "stop_reason": "process_ended", "unused_authority": "released"})
        self.assertTrue(sealed["ok"])
        journal = sealed["result"]["evidence_journal"]
        native = journal["entries"][1]["value"]["value"]
        self.assertEqual(native["surface"]["facts"]["pid"], 123)
        witness = {"verdict": "inconclusive", "smallest_supported_claim": "The bound process exited with code zero.",
                   "causal_account": "The process receipt does not establish saving or feature correctness.",
                   "citations": [{"citation_id": "J0002", "meaning": "Bound process exit",
                                  "checks": {"value.surface.facts.pid": 123, "value.surface.facts.exit_code": 0,
                                             "value.surface.facts.evidence_ref": "game-process-exit.json"}}],
                   "recommended_disposition": "continue", "evidence_ceiling": "zero-credit"}
        finished = service.call({"action": "run.finish", "observation_id": ended["observation_id"],
                                 "stop_reason": "process_ended", "unused_authority": "released", "witness": witness})
        self.assertTrue(finished["ok"])
        self.assertEqual(channel.status()["state"], "finished")

    def test_late_exit_status_receipt_refines_unknown_without_rewriting_prior_observation(self):
        channel, process, _, _ = self.channel(alive=False)
        unknown = channel.observe()
        process.update(exit_code=0, evidence_ref="confirmed-exit.json")
        confirmed = channel.observe()
        self.assertIsNone(unknown["surface"]["facts"]["exit_code"])
        self.assertEqual(confirmed["surface"]["facts"]["exit_code"], 0)
        self.assertNotEqual(unknown["observation_id"], confirmed["observation_id"])
        self.assertEqual(confirmed["surface"]["facts"]["evidence_ref"], "confirmed-exit.json")
        self.assertTrue(channel._observations[unknown["observation_id"]]["used"])
        self.assertEqual(confirmed["advertised_actions"], [])

    def test_wrong_run_process_cannot_replace_current_native_state(self):
        channel, process, _, _ = self.channel()
        channel.observe()
        process.update(alive=False, run_id="other-run")
        with self.assertRaisesRegex(ValueError, "process_exit_run_identity_mismatch"):
            channel.observe()


if __name__ == "__main__":
    unittest.main()
