#!/usr/bin/env python3
"""Focused canonical-route coverage for R-021 native debug HP transactions."""

from __future__ import annotations

import sys
import tempfile
import unittest
import json
from pathlib import Path
from unittest import mock

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness  # noqa: E402
import r021_direct_hp_transaction  # noqa: E402


class R021CanonicalRouteTest(unittest.TestCase):
    declaration = {
        "fixture_actor_id": "fixture",
        "expected_typeid": "mon_zombie",
        "target_hp": 0,
        "action_owner": "debug_menu.monster_set_hp",
        "cleanup_owner": "fixture_cleanup",
    }

    def test_native_parser_keeps_only_current_dispatch_records(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            log = Path(temp_dir) / "debug.log"
            log.write_text("prior data\n", encoding="utf-8")
            offset = log.stat().st_size
            log.write_text(log.read_text(encoding="utf-8") + "\n".join([
                "openclaw_harness_debug_transaction: operation=monster_set_hp snapshot=before target_fixture_actor_id=fixture creature_handle=7 creature_fixture_actor_id=fixture creature_type=mon_zombie creature_position=(4,0,0) creature_hp=80 creature_dead=false gameplay_credit=none",
                "openclaw_harness_debug_transaction: operation=monster_set_hp snapshot=after target_fixture_actor_id=fixture creature_handle=7 creature_fixture_actor_id=fixture creature_type=mon_zombie creature_position=(4,0,0) creature_hp=0 creature_dead=false gameplay_credit=none",
                "openclaw_harness_debug_transaction: operation=monster_set_hp target_handle=7 target_fixture_actor_id=fixture target_type=mon_zombie target_position=(4,0,0) hp_before=80 hp_after=0 cause=debug_menu_direct_set_hp native_setter=monster::set_hp gameplay_credit=none\n",
            ]), encoding="utf-8")
            receipts, snapshots = startup_harness._r021_native_transaction_records(log, offset)
        self.assertEqual(len(receipts), 1)
        self.assertEqual(receipts[0]["hp_after"], 0)
        self.assertEqual([item["hp"] for item in snapshots], [80, 0])
        self.assertEqual(receipts[0]["target_position"], [4, 0, 0])

    def test_direct_hp_synthesizes_the_dead_after_snapshot_from_one_native_receipt(self) -> None:
        declaration = {**self.declaration, "selection_query": "zombie"}
        receipt = {
            "accepted": True, "target_fixture_actor_id": "fixture", "target_handle": "7",
            "target_type": "mon_zombie", "target_position": [4, 0, 0],
            "native_setter": "monster::set_hp", "cause": "debug_menu_direct_set_hp",
            "gameplay_credit": "none", "hp_before": 80, "hp_after": 0,
        }
        before = [{"typeid": "mon_zombie", "location_ms": [4, 0, 0], "hp": 80,
                   "values": {"caol_fixture_actor_id": "fixture"}, "handle": "7", "dead": False,
                   "phase": "before"}]
        after = [{"typeid": receipt["target_type"], "location_ms": receipt["target_position"],
                  "hp": receipt["hp_after"], "values": {"caol_fixture_actor_id": "fixture"},
                  "handle": receipt["target_handle"], "dead": True, "phase": "after_native_receipt"}]
        artifact = r021_direct_hp_transaction.bind_direct_hp_transaction(
            declaration, [receipt], before, after, cleanup={"accepted": True},
        )
        self.assertEqual(artifact["after_creatures"][0]["hp"], 0)

    def test_execute_probe_steps_binds_the_executor_artifact(self) -> None:
        artifact = {
            "artifact_kind": "r021_direct_hp_setter",
            "artifact_path": "/tmp/r021.receipt.json",
            "native_receipt": {"accepted": True, "hp_after": 0},
            "changed_creature_identities": [("fixture", "mon_zombie", (4, 0, 0))],
            "cleanup_receipt": {"accepted": True},
            "gameplay_credit": False,
        }
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "execute_r021_direct_hp_setter", return_value=artifact) as executor:
            log = Path(temp_dir) / "debug.log"
            log.write_text("", encoding="utf-8")
            reports = startup_harness.execute_probe_steps(
                1, Path(temp_dir), [{"kind": "r021_direct_hp_setter", "label": "direct", "declaration": self.declaration}],
                profile="test", world="world", artifact_log=log,
            )
        self.assertEqual(len(reports), 1)
        self.assertEqual(reports[0]["metadata"], artifact)
        executor.assert_called_once()

    def test_direct_hp_waits_for_the_observed_monster_submenu_before_h(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path"), mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence") as press, mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "green"}), mock.patch.object(
                startup_harness, "fill_numeric_prompt"), mock.patch.object(
                startup_harness, "_r021_native_transaction_records", return_value=([], [])), mock.patch.object(
                r021_direct_hp_transaction, "bind_direct_hp_transaction", side_effect=RuntimeError("stop after ordering")):
            with self.assertRaisesRegex(RuntimeError, "stop after ordering"):
                startup_harness.execute_r021_direct_hp_setter(
                    1, declaration=self.declaration, artifact_log=Path(temp_dir) / "debug.log",
                    artifact_start=0, run_dir=Path(temp_dir),
                )
        sequences = [call.args[1] for call in press.call_args_list]
        self.assertEqual(sequences[:2], [["enter"], ["h"]])

    def test_direct_hp_enters_the_edit_monster_list_before_filtering(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path") as menu_path, mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence"), mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "red"}):
            with self.assertRaisesRegex(RuntimeError, "did not settle"):
                startup_harness.execute_r021_direct_hp_setter(
                    1, declaration=self.declaration, artifact_log=Path(temp_dir) / "debug.log",
                    artifact_start=0, run_dir=Path(temp_dir),
                )
        self.assertEqual(menu_path.call_args.args[1], ["c", "c"])

    def test_selector_observation_exits_without_sending_hp_input(self) -> None:
        declaration = {**self.declaration, "observe_only": True, "selection_query": "zombie"}
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path"), mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence") as press, mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "green"}):
            artifact = startup_harness.execute_r021_direct_hp_setter(
                1, declaration=declaration, artifact_log=Path(temp_dir) / "debug.log",
                artifact_start=0, run_dir=Path(temp_dir),
            )
        self.assertEqual(artifact["artifact_kind"], "r021_monster_selector_observation")
        self.assertNotIn(["h"], [call.args[1] for call in press.call_args_list])

    def test_selector_observation_accepts_the_text_evaluator_matched_verdict(self) -> None:
        declaration = {**self.declaration, "observe_only": True}
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path"), mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence"), mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "matched"}):
            artifact = startup_harness.execute_r021_direct_hp_setter(
                1, declaration=declaration, artifact_log=Path(temp_dir) / "debug.log",
                artifact_start=0, run_dir=Path(temp_dir),
            )
        self.assertEqual(artifact["artifact_kind"], "r021_monster_selector_observation")

    def test_direct_hp_refuses_to_send_h_without_observed_monster_submenu(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path"), mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence") as press, mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "red"}):
            with self.assertRaisesRegex(RuntimeError, "did not settle"):
                startup_harness.execute_r021_direct_hp_setter(
                    1, declaration=self.declaration, artifact_log=Path(temp_dir) / "debug.log",
                    artifact_start=0, run_dir=Path(temp_dir),
                )
        self.assertNotIn(["h"], [call.args[1] for call in press.call_args_list])

    def test_direct_hp_passes_complete_observation_context_to_the_submenu_guard(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                startup_harness, "run_debug_menu_shortcut_path"), mock.patch.object(
                startup_harness, "apply_uilist_filter"), mock.patch.object(
                startup_harness, "peekaboo_press_sequence"), mock.patch.object(
                startup_harness, "capture_screenshot", return_value={"screen_summary": {}}), mock.patch.object(
                startup_harness, "capture_screen_text_artifact", return_value={"ok": True}), mock.patch.object(
                startup_harness, "evaluate_screen_text_or_rendered_hud_expectation",
                return_value={"status": "red"}) as submenu_guard:
            with self.assertRaisesRegex(RuntimeError, "did not settle"):
                startup_harness.execute_r021_direct_hp_setter(
                    1, declaration=self.declaration, artifact_log=Path(temp_dir) / "debug.log",
                    artifact_start=0, run_dir=Path(temp_dir),
                )
        self.assertEqual(submenu_guard.call_args.kwargs["action_trace_log"], None)
        self.assertEqual(submenu_guard.call_args.kwargs["run_id"], "")
        self.assertEqual(submenu_guard.call_args.kwargs["startup_overlay_recovery"], None)

    def test_startup_resolves_the_r021_declaration_from_the_bound_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            contract = Path(temp_dir) / "scenario.json"
            contract.write_text(json.dumps({"r021_direct_hp_setter": self.declaration}), encoding="utf-8")
            args = type("Args", (), {"scenario_contract_path": str(contract)})()
            self.assertEqual(
                startup_harness.r021_direct_hp_setter_declaration_for_startup(args), self.declaration,
            )

    def test_bound_r021_startup_reaches_the_canonical_launch_boundary(self) -> None:
        class LaunchReached(RuntimeError):
            pass

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            contract = root / "scenario.json"
            contract.write_text(json.dumps({"r021_direct_hp_setter": self.declaration}), encoding="utf-8")
            plan = startup_harness.StartupPlan(
                profile="r021-startup-test",
                userdir=str(root / "userdir"),
                executable=str(root / "cataclysm-tiles"),
                strategy="harness_new_world",
                reason="test",
                target_world="",
                existing_worlds=[],
                fixture="",
                run_dir=str(root / "run"),
            )
            args = startup_harness.build_parser().parse_args([
                "start",
                "--profile", "r021-startup-test",
                "--scenario-identity", "r021.direct_hp_setter_bootstrap",
                "--scenario-contract-path", str(contract),
            ])
            loaded_contract = {
                "normalized_contract": {"r021_direct_hp_setter": self.declaration},
                "source": {"path": str(contract), "sha256": "bound"},
                "validation": {"status": "valid", "review_required": False},
            }
            launch_game = mock.Mock(side_effect=LaunchReached("canonical launch reached"))
            declaration_resolver = startup_harness.r021_direct_hp_setter_declaration_for_startup
            with (
                mock.patch.object(startup_harness, "load_profile_config", return_value={"startup": {}}),
                mock.patch.object(startup_harness, "purge_profile_flexbuffer_cache", return_value={}),
                mock.patch.object(startup_harness, "build_plan", return_value=plan),
                mock.patch.object(startup_harness, "build_runtime_binding", return_value={"ok": True}),
                mock.patch.object(startup_harness, "load_checkpoint_contract", return_value=loaded_contract),
                mock.patch.object(
                    startup_harness, "run_checkpoint_contract_preflight", return_value={"accepted": True}
                ),
                mock.patch.object(startup_harness, "game_child_environment", return_value={}),
                mock.patch.object(startup_harness, "startup_gui_automation_required", return_value=False),
                mock.patch.object(startup_harness, "kill_existing_game_processes", return_value=[]),
                mock.patch.object(startup_harness, "config_dir_for_profile", return_value=root / "config"),
                mock.patch.object(startup_harness, "latest_world_save_marker", return_value={}),
                mock.patch.object(startup_harness, "copy_file_if_exists"),
                mock.patch.object(
                    startup_harness,
                    "r021_direct_hp_setter_declaration_for_startup",
                    wraps=declaration_resolver,
                ) as resolve_declaration,
                mock.patch.object(startup_harness, "launch_game", launch_game),
            ):
                with self.assertRaisesRegex(LaunchReached, "canonical launch reached"):
                    startup_harness.run_startup(args)

            resolve_declaration.assert_called_once_with(args)
            launch_game.assert_called_once()

    def test_route_ledger_rejects_missing_native_snapshot_or_cleanup(self) -> None:
        ledger = startup_harness.build_probe_step_ledger([{
            "index": 1,
            "kind": "r021_direct_hp_setter",
            "label": "direct",
            "metadata": {"artifact_kind": "r021_direct_hp_setter", "gameplay_credit": False},
        }])
        self.assertEqual(ledger[0]["verdict"], "red_step_r021_native_direct_hp_transaction_unbound")


if __name__ == "__main__":
    unittest.main()
