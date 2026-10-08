#!/usr/bin/env python3
"""Focused R-014 immediate evidence binding contracts."""

from __future__ import annotations

import copy
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


HARNESS_DIR = Path( __file__ ).resolve().parent
sys.path.insert( 0, str( HARNESS_DIR ) )

import startup_harness  # noqa: E402


class R014StepEvidenceTest( unittest.TestCase ):
    def test_recoverable_selection_owner_is_zero_credit_and_exactly_run_bound( self ) -> None:
        menu_owner = {
            "status": "required_state_present",
            "startup_owner": "nonworld_native_input_owner",
            "frame_run_id": "run-a",
            "frame_event": "surface_descriptor",
            "frame_state": "menu",
            "frame_breadcrumbs": ["Main menu", "Load character from TestSetup00"],
            "frame_title": "Load character from TestSetup00",
            "frame_trace_offset": 42,
            "advertised_actions": ["menu.choose", "menu.cancel"],
        }
        self.assertTrue(startup_harness.recoverable_native_selection_owner(
            menu_owner, run_id="run-a", matching_initial_world_frames=0,
            foreign_initial_world_frames=0,
        ))
        controls = (
            ({"frame_run_id": "run-b"}, 0, 0),
            ({"advertised_actions": []}, 0, 0),
            ({"frame_state": "query"}, 0, 0),
            ({"frame_event": "frame"}, 0, 0),
            ({}, 1, 0),
            ({}, 0, 1),
        )
        for changed, matching, foreign in controls:
            invalid = {**menu_owner, **changed}
            with self.subTest(changed=changed, matching=matching, foreign=foreign):
                self.assertFalse(startup_harness.recoverable_native_selection_owner(
                    invalid, run_id="run-a", matching_initial_world_frames=matching,
                    foreign_initial_world_frames=foreign,
                ))

    def test_menu_handoff_publishes_live_descriptor_then_rechecks_world( self ) -> None:
        run_id = "run-a"
        menu_owner = {
            "status": "required_state_present",
            "startup_owner": "nonworld_native_input_owner",
            "frame_run_id": run_id,
            "frame_event": "surface_descriptor",
            "frame_state": "menu",
            "frame_breadcrumbs": ["Main menu", "Load character from TestSetup00"],
            "frame_title": "Load character from TestSetup00",
            "frame_id": "run-a:surface:menu",
            "frame_trace_offset": 42,
            "advertised_actions": ["menu.choose", "menu.cancel"],
        }
        world_metadata = {
            "status": "required_state_present",
            "frame_run_id": run_id,
            "frame_state": "world",
            "frame_producer": "hud_world_ready",
            "initial_world_ready": True,
            "frame_trace_offset": 64,
            "advertised_actions": ["world.wait"],
        }
        checkpoint = {
            "required_state": "world",
            "required_actions": ["world.wait"],
            "require_initial_hud_world_ready_frame": True,
            "allow_recoverable_native_selection_handoff": True,
        }
        steps = [
            {"kind": "native_semantic_bootstrap", "label": "bind_world",
             "native_semantic_checkpoint": checkpoint},
            {"kind": "wait", "label": "settle_before_live", "seconds": 1.0},
            {"kind": "cockpit_live_session", "label": "select_saved_character",
             "live_operations": ["game.act"]},
        ]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            run_dir = root / "run"
            run_dir.mkdir()
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": run_id}), encoding="utf-8",
            )
            service = SimpleNamespace(run_channel=SimpleNamespace(_binding_id="binding-a"))

            def finish_live_session(_service, _stdin, _stdout):
                startup_harness.cockpit_live_final_path(
                    run_dir, "select_saved_character",
                ).write_text(json.dumps({
                    "schema": "caol-cockpit-live-final-v1",
                    "run_id": run_id,
                    "state": "finished",
                }), encoding="utf-8")
                return 0

            output = io.StringIO()
            with contextlib.redirect_stdout(output), \
                    mock.patch.object(startup_harness, "semantic_step_trace_start_for_source", return_value=0), \
                    mock.patch.object(startup_harness, "initial_hud_world_frame_counts",
                                      side_effect=[(0, 0), (1, 0)]), \
                    mock.patch.object(startup_harness, "cockpit_native_input_owner_ready",
                                      return_value=menu_owner), \
                    mock.patch.object(startup_harness, "semantic_wake_pipe_contract",
                                      return_value={"status": "bound"}), \
                    mock.patch.object(startup_harness, "await_r014_native_semantic_bootstrap",
                                      return_value=world_metadata) as strict_bootstrap, \
                    mock.patch.object(startup_harness, "open_cockpit_game_service",
                                      return_value=service), \
                    mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=None), \
                    mock.patch.object(startup_harness.time, "sleep"), \
                    mock.patch.object(startup_harness, "serve_cockpit_live",
                                      side_effect=finish_live_session):
                reports = startup_harness.execute_probe_steps(
                    12345, run_dir, steps, profile="profile-a", world="Test00",
                    grants_gameplay_proof=True,
                )

        descriptor_lines = [line for line in output.getvalue().splitlines()
                            if "cockpit_live_session" in line]
        self.assertEqual(len(descriptor_lines), 1)
        envelope = json.loads(descriptor_lines[0])
        self.assertFalse(envelope["cockpit_live_session"]["bootstrap_only"])
        self.assertEqual(strict_bootstrap.call_count, 1)
        self.assertEqual(len(reports), 3)
        self.assertEqual(reports[0]["native_selection_handoff"]["status"], "world_qualified")
        self.assertEqual(reports[0]["metadata"]["frame_state"], "world")
        self.assertEqual(reports[1]["kind"], "wait")
        self.assertTrue(reports[2]["metadata"]["gameplay_credit"])

    def test_menu_handoff_without_world_keeps_zero_credit_and_stops( self ) -> None:
        run_id = "run-a"
        menu_owner = {
            "status": "required_state_present",
            "startup_owner": "nonworld_native_input_owner",
            "frame_run_id": run_id,
            "frame_event": "surface_descriptor",
            "frame_state": "menu",
            "frame_breadcrumbs": ["Main menu", "Load character from TestSetup00"],
            "frame_title": "Load character from TestSetup00",
            "frame_id": "run-a:surface:menu",
            "frame_trace_offset": 42,
            "advertised_actions": ["menu.choose"],
        }
        checkpoint = {
            "required_state": "world",
            "required_actions": ["world.wait"],
            "require_initial_hud_world_ready_frame": True,
            "allow_recoverable_native_selection_handoff": True,
        }
        steps = [
            {"kind": "native_semantic_bootstrap", "label": "bind_world",
             "native_semantic_checkpoint": checkpoint},
            {"kind": "wait", "label": "settle_before_live", "seconds": 1.0},
            {"kind": "cockpit_live_session", "label": "select_saved_character",
             "live_operations": ["game.act"]},
        ]
        not_world = {
            "status": "scanned", "reason": "first_same_run_semantic_frame_timeout",
            "frame_run_id": run_id, "frame_state": "menu",
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            run_dir = root / "run"
            run_dir.mkdir()
            (run_dir / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": run_id}), encoding="utf-8",
            )
            service = SimpleNamespace(run_channel=SimpleNamespace(_binding_id="binding-a"))

            def finish_without_world(_service, _stdin, _stdout):
                startup_harness.cockpit_live_final_path(
                    run_dir, "select_saved_character",
                ).write_text(json.dumps({
                    "schema": "caol-cockpit-live-final-v1",
                    "run_id": run_id,
                    "state": "finished",
                }), encoding="utf-8")
                return 0

            output = io.StringIO()
            with contextlib.redirect_stdout(output), \
                    mock.patch.object(startup_harness, "semantic_step_trace_start_for_source", return_value=0), \
                    mock.patch.object(startup_harness, "initial_hud_world_frame_counts",
                                      side_effect=[(0, 0), (0, 0)]), \
                    mock.patch.object(startup_harness, "cockpit_native_input_owner_ready",
                                      return_value=menu_owner), \
                    mock.patch.object(startup_harness, "semantic_wake_pipe_contract",
                                      return_value={"status": "bound"}), \
                    mock.patch.object(startup_harness, "await_r014_native_semantic_bootstrap",
                                      return_value=not_world), \
                    mock.patch.object(startup_harness, "open_cockpit_game_service",
                                      return_value=service), \
                    mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=None), \
                    mock.patch.object(startup_harness.time, "sleep"), \
                    mock.patch.object(startup_harness, "serve_cockpit_live",
                                      side_effect=finish_without_world):
                reports = startup_harness.execute_probe_steps(
                    12345, run_dir, steps, profile="profile-a", world="Test00",
                    grants_gameplay_proof=True,
                )

        self.assertEqual(len(reports), 3)
        self.assertEqual(reports[0]["native_selection_handoff"]["status"], "world_not_qualified")
        self.assertEqual(reports[0]["abort"]["status"], "blocked_r019_initial_hud_world_frame_unqualified")
        self.assertEqual(reports[1]["kind"], "wait")
        self.assertFalse(reports[2]["metadata"]["gameplay_credit"])
        self.assertEqual(reports[2]["abort"]["status"], "blocked_initial_world_not_reached")

    def test_selection_handoff_skips_only_passive_wall_waits( self ) -> None:
        live = {"kind": "cockpit_live_session", "label": "select_saved_character"}
        passive = [
            {"kind": "native_semantic_bootstrap"},
            {"kind": "wait", "seconds": 1.0},
            live,
        ]
        self.assertIs(
            startup_harness.recoverable_selection_handoff_live_step(passive, 0), live,
        )
        for step_kind in ("audit_log_contains", "game_action", "screenshot"):
            with self.subTest(intervening_step=step_kind):
                blocked = [
                    {"kind": "native_semantic_bootstrap"},
                    {"kind": step_kind},
                    live,
                ]
                self.assertEqual(
                    startup_harness.recoverable_selection_handoff_live_step(blocked, 0), {},
                )
        bootstrap_only = [
            {"kind": "native_semantic_bootstrap"},
            {"kind": "wait", "seconds": 1.0},
            {"kind": "cockpit_live_session", "bootstrap_only": True},
        ]
        self.assertEqual(
            startup_harness.recoverable_selection_handoff_live_step(bootstrap_only, 0), {},
        )

    def test_bootstrap_requires_fresh_same_run_complete_native_frame( self ) -> None:
        frame = {
            "run_id": "run-a",
            "frame_id": "run-a:2",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 42,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=frame,
        ):
            root = Path( directory )
            green = startup_harness.r014_native_semantic_bootstrap_metadata(
                profile="test", run_dir=root, run_id="run-a", start_offset=0,
                press_trace_offset=41, required_state="world", required_actions=["world.wait"],
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"_event_offset": 40}, {"valid_actions": []},
                {"producer": "player_input"}, {"initial_world_ready": False},
            ):
                invalid = copy.deepcopy( frame )
                invalid.update( changed )
                with mock.patch.object( startup_harness, "current_semantic_step_frame", return_value=invalid ):
                    yellow = startup_harness.r014_native_semantic_bootstrap_metadata(
                        profile="test", run_dir=root, run_id="run-a", start_offset=0,
                        press_trace_offset=41, required_state="world", required_actions=["world.wait"],
                    )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_launcher_bootstrap_accepts_only_the_same_run_world_frame( self ) -> None:
        frame = {
            "run_id": "run-a",
            "frame_id": "run-a:1",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 1,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=frame,
        ):
            metadata = startup_harness.await_r014_native_semantic_bootstrap(
                profile="test", run_dir=Path( directory ), run_id="run-a",
                required_state="world", required_actions=["world.wait"],
                timeout_seconds=0.0, poll_seconds=0.0,
            )
        self.assertEqual( metadata["status"], "required_state_present" )
        self.assertEqual( metadata["bootstrap"], "launcher_first_same_run_semantic_frame" )

    def test_bootstrap_accepts_source_bound_world_surface_descriptor( self ) -> None:
        descriptor = {
            "event": "surface_descriptor",
            "schema_version": 1,
            "run_id": "run-a",
            "surface_id": "run-a:surface:1",
            "frame_id": "run-a:frame:1",
            "kind": "world",
            "breadcrumbs": ["World"],
            "payload": {},
            "valid_actions": [{
                "id": "world.inventory", "stable_id": "", "label": "world.inventory",
                "enabled": True,
            }],
            "_event_offset": 42,
        }
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            startup_harness, "current_semantic_step_frame", return_value=descriptor,
        ):
            metadata = startup_harness.r014_native_semantic_bootstrap_metadata(
                profile="test", run_dir=Path( directory ), run_id="run-a", start_offset=0,
                press_trace_offset=41, required_state="world", required_actions=["world.inventory"],
            )
        self.assertEqual( metadata["status"], "required_state_present" )
        self.assertEqual( metadata["frame_event"], "surface_descriptor" )

    def test_launcher_bootstrap_fails_closed_for_wrong_run_or_absent_frame( self ) -> None:
        wrong_run = {
            "run_id": "run-b",
            "frame_id": "run-b:1",
            "state": "world",
            "valid_actions": ["world.wait"],
            "producer": "hud_world_ready",
            "initial_world_ready": True,
            "_event_offset": 1,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path( directory )
            for result in ( wrong_run, ValueError( "absent" ) ):
                with mock.patch.object(
                    startup_harness, "current_semantic_step_frame",
                    side_effect=result if isinstance( result, Exception ) else None,
                    return_value=None if isinstance( result, Exception ) else result,
                ):
                    metadata = startup_harness.await_r014_native_semantic_bootstrap(
                        profile="test", run_dir=root, run_id="run-a",
                        required_state="world", required_actions=["world.wait"],
                        timeout_seconds=0.0, poll_seconds=0.0,
                    )
                self.assertEqual( metadata["status"], "scanned" )
                self.assertEqual( metadata["reason"], "first_same_run_semantic_frame_timeout" )

    def test_observation_requires_current_run_and_visible_native_result( self ) -> None:
        observation = {
            "ok": True,
            "result": {
                "run_id": "run-a", "observation_id": "run-a:3",
                "visible_local": [{"handle": "visible:run-a:1", "terrain": "floor"}],
            },
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path( directory ) / "observe.json"
            artifact.write_text( "{}", encoding="utf-8" )
            green = startup_harness.r014_cockpit_observation_metadata(
                observation, artifact_path=artifact, run_id="run-a",
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"visible_local": []},
            ):
                invalid = copy.deepcopy( observation )
                invalid["result"].update( changed )
                yellow = startup_harness.r014_cockpit_observation_metadata(
                    invalid, artifact_path=artifact, run_id="run-a",
                )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_live_session_requires_same_run_bound_complete_final( self ) -> None:
        final = {
            "run_id": "run-a", "binding_id": "binding-a", "state": "finished",
            "action_observation_sequence": [
                {"kind": "observation"},
                {"kind": "action", "result": {"ok": True}},
                {"kind": "observation"},
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path( directory ) / "cockpit.live.final.json"
            artifact.write_text( "{}", encoding="utf-8" )
            green = startup_harness.r014_cockpit_live_session_metadata(
                final, artifact_path=artifact, run_id="run-a", binding_id="binding-a", live_status=0,
            )
            self.assertEqual( green["status"], "required_state_present" )
            for changed in (
                {"run_id": "run-b"}, {"state": "active"},
                {"action_observation_sequence": [{"kind": "observation"}]},
            ):
                invalid = copy.deepcopy( final )
                invalid.update( changed )
                yellow = startup_harness.r014_cockpit_live_session_metadata(
                    invalid, artifact_path=artifact, run_id="run-a", binding_id="binding-a", live_status=0,
                )
                self.assertEqual( yellow["status"], "scanned" )
                self.assertTrue( startup_harness.metadata_checkpoint_verdict( yellow )[0].startswith( "yellow" ) )

    def test_declared_live_steps_own_distinct_terminal_artifacts( self ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path( directory )
            crossing = startup_harness.cockpit_live_final_path(
                root, "observe_cannibal_roof_fire_lifecycle",
            )
            persisted = startup_harness.cockpit_live_final_path(
                root, "observe_persisted_cannibal_return_lifecycle",
            )
            self.assertNotEqual( crossing, persisted )
            self.assertEqual(
                startup_harness.cockpit_live_final_path( root ).name,
                "cockpit.live.final.json",
            )
            first = startup_harness.finalize_cockpit_live_session(
                root, 1, {"run_id": "run-a", "state": "finished"},
                cleanup_process=False, final_path=crossing,
            )
            second = startup_harness.finalize_cockpit_live_session(
                root, 1, {"run_id": "run-a", "state": "finished"},
                cleanup_process=False, final_path=persisted,
            )
            self.assertEqual( first["final_report_ref"], crossing.name )
            self.assertEqual( second["final_report_ref"], persisted.name )
            self.assertTrue( crossing.is_file() )
            self.assertTrue( persisted.is_file() )


class SavedInputOwnerBootstrapTest(unittest.TestCase):
    """Real selector/bootstrap/factory/public reader; no native action dispatch."""

    @staticmethod
    def owner(sequence, kind, actions, run_id="saved-run"):
        return {"event": "surface_descriptor", "schema_version": 1,
                "run_id": run_id, "process_instance": "fixture-process",
                "sequence": sequence, "game_turn": 5228405,
                "game_minutes": 7940, "surface_id": "surface:" + str(sequence),
                "frame_id": "frame:" + str(sequence), "kind": kind,
                "breadcrumbs": [kind], "payload": {}, "valid_actions": actions}

    @staticmethod
    def encode(events):
        return b"".join((startup_harness.SEMANTIC_STEP_PREFIX + json.dumps(x) + "\n").encode()
                        for x in events)

    def pipeline(self, events, *, saved=True, strict=False, changed_birth=False, mirror=None, native_bytes=None, run_id="saved-run", declared_steps=None):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            run = root / "run"
            run.mkdir()
            config = root / "config"
            config.mkdir()
            snapshot = root / "snapshot"
            snapshot.mkdir()
            native = run / "semantic.native.events.jsonl"
            native.write_bytes(self.encode(events) if native_bytes is None else native_bytes)
            # Older mirrored owner must not replace a later same-process native owner.
            (config / "debug.log").write_bytes(self.encode(events[:1] if mirror is None else mirror))
            (run / startup_harness.TRANSITION_EVENT_BINDING_FILENAME).write_text(
                json.dumps({"run_id": run_id}))
            generation = startup_harness.process_generation_snapshot(os.getpid())
            if changed_birth:
                generation["birth_identity"] = "wrong-birth"
            (run / "process.json").write_text(json.dumps({"pid": os.getpid(),
                "process_generation": generation}))
            checkpoint = {"required_state": "native_input_owner", "required_actions": [],
                          "allow_saved_native_input_owner": True,
                          "require_initial_hud_world_ready_frame": strict}
            steps = [{"kind": "native_semantic_bootstrap", "label": "bind_saved_owner",
                      "native_semantic_checkpoint": checkpoint},
                     {"kind": "cockpit_live_session", "label": "story",
                      "live_operations": ["game.act"], "cleanup_on_finish": False}]
            if declared_steps is not None:
                steps = copy.deepcopy(declared_steps)
            public = []
            services = []
            real_serve = startup_harness.serve_cockpit_live

            def serve(service, _stdin, _stdout):
                services.append(service)
                output = io.StringIO()
                # EOF is fixture-only and cleanup_on_finish=False; no game exists.
                status = real_serve(service, io.StringIO('{"action":"game.observe"}\n'), output)
                for line in output.getvalue().splitlines():
                    wire = json.loads(line)
                    public.append(startup_harness.resolve_wire(wire,
                        directory=service.run_channel.archive.path.parent,
                        binding_id=service.run_channel._binding_id))
                return status

            output = io.StringIO()
            with contextlib.redirect_stdout(output), \
                    mock.patch.object(startup_harness, "config_dir_for_profile", return_value=config), \
                    mock.patch.object(startup_harness, "semantic_wake_pipe_contract",
                                      return_value={"status": "bound", "path": "fixture-only"}), \
                    mock.patch.object(startup_harness, "active_playtest_witness_charter", return_value=None), \
                    mock.patch.dict(os.environ, {"OPENCLAW_COCKPIT_BRIDGE_BINDING_ID": "fixture-binding",
                                                 "OPENCLAW_COCKPIT_BRIDGE_SESSION_DIR": str(root)}), \
                    mock.patch.object(startup_harness, "execute_semantic_act") as dispatch, \
                    mock.patch.object(startup_harness, "cleanup_game_process") as cleanup, \
                    mock.patch.object(startup_harness, "serve_cockpit_live", side_effect=serve):
                reports = startup_harness.execute_probe_steps(os.getpid(), run, steps,
                    profile="saved-profile", world="TestSetup00",
                    immutable_saved_world_snapshot=snapshot if saved else None,
                    semantic_bootstrap_timeout_seconds=0, semantic_bootstrap_poll_seconds=0)
                dispatch.assert_not_called()
                cleanup.assert_not_called()
            for service in services:
                # Factory retained the selected physical source, not the mirror's byte cursor.
                with mock.patch.object(startup_harness, "config_dir_for_profile", return_value=config):
                    native_frame = service.run_channel._read_native_frame()
                    selected = startup_harness.semantic_step_source_trace("saved-profile", run)
                self.assertEqual(native_frame["_source_path"], str(selected.resolve()))
                self.assertGreater(native_frame["_source_end"], native_frame["_source_offset"])
                service.run_channel.archive.close()
            self.assertFalse((run / "semantic.requests.jsonl").exists())
            envelopes = [json.loads(line) for line in output.getvalue().splitlines()
                         if "cockpit_live_session" in line]
            return reports, public, envelopes, str(native.resolve())

    def test_saved_activity_distraction_prompt_reaches_actual_public_factory(self):
        activity = self.owner(1, "activity_wait", [
            {"id": "activity.pause", "stable_id": "", "label": "Pause", "enabled": True}])
        distraction = self.owner(204, "activity_distraction", [
            {"id": "activity.continue", "stable_id": "", "label": "Continue", "enabled": True}])
        prompt = self.owner(205, "prompt", [
            {"id": "prompt.choose", "stable_id": "prompt-option:2", "label": "NO", "enabled": True}])
        for current in (activity, distraction, prompt):
            with self.subTest(kind=current["kind"]):
                turns = [{"event": "turn", "run_id": "saved-run"} for _ in range(80)]
                records = [activity, *turns, current] if current is not activity else [activity]
                reports, public, envelopes, source = self.pipeline(records)
                self.assertEqual(reports[0]["metadata"]["status"], "required_state_present")
                self.assertEqual(reports[0]["metadata"]["frame_state"], current["kind"])
                self.assertEqual(reports[0]["metadata"]["initial_world_readiness"],
                                 "not_claimed_saved_continuation")
                self.assertFalse(reports[0]["metadata"]["gameplay_credit"])
                self.assertEqual(len(envelopes), 1)
                self.assertTrue(public[0]["ok"], public[0])
                self.assertEqual(public[0]["result"]["surface"]["kind"], current["kind"])
                self.assertTrue(public[0]["operation_availability"]["game.act"])
                self.assertFalse(reports[1]["metadata"]["gameplay_credit"])

    def test_saved_owner_contract_refuses_fresh_start_strict_claim_or_changed_birth(self):
        event = self.owner(1, "activity_wait", [
            {"id": "activity.pause", "stable_id": "", "label": "Pause", "enabled": True}])
        for options in ({"saved": False}, {"strict": True}, {"changed_birth": True}):
            with self.subTest(options=options):
                reports, public, envelopes, _ = self.pipeline([event], **options)
                self.assertIn("abort", reports[0])
                self.assertFalse(public)
                self.assertFalse(envelopes)

    def test_newer_same_process_mirror_uses_its_own_source_offsets(self):
        action = {"id": "prompt.choose", "stable_id": "prompt-option:2", "label": "NO", "enabled": True}
        native = self.owner(204, "activity_distraction", [action])
        mirror = self.owner(205, "prompt", [action])
        reports, public, envelopes, _ = self.pipeline([native], mirror=[mirror])
        self.assertEqual(reports[0]["metadata"]["frame_state"], "prompt")
        self.assertEqual(public[0]["result"]["sequence"], 205)
        self.assertEqual(len(envelopes), 1)
        foreign = {**mirror, "process_instance": "other-process"}
        reports, public, envelopes, _ = self.pipeline([native], mirror=[foreign])
        self.assertEqual(public[0]["result"]["sequence"], 204)

    def test_current_owner_refuses_foreign_or_malformed_records(self):
        action = {"id": "prompt.choose", "stable_id": "prompt-option:2", "label": "NO", "enabled": True}
        foreign = self.owner(205, "prompt", [action], run_id="foreign-run")
        malformed = self.owner(205, "prompt", [action, action])
        for event in (foreign, malformed):
            with self.subTest(event=event):
                reports, public, envelopes, _ = self.pipeline([event])
                self.assertIn("abort", reports[0])
                self.assertFalse(public)
                self.assertFalse(envelopes)


if __name__ == "__main__":
    unittest.main()
