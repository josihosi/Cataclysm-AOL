#!/usr/bin/env python3
"""Focused pre-launch contracts for checkpoint-chain scenarios."""

from __future__ import annotations

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness  # noqa: E402


class CheckpointContractPreflightTest(unittest.TestCase):
    def contract(self) -> dict:
        return {
            "manifest_version": 2,
            "name": "test.checkpoint.contract",
            "steps": [{"label": "gate_boundary", "kind": "wait"}],
            "capabilities": {"capabilities.test.route": "declared"},
            "runtime_contract": {
                "permitted_input": ["press:."],
                "forbidden_input": ["debug:spawn"],
                "setup_only_debug": True,
                "disposable_copy": True,
                "helpers": ["Peekaboo"],
                "permissions": ["accessibility"],
                "platform": ["macos"],
                "profile": "test-profile",
                "fixture": "test-fixture",
                "requirements": {
                    "os": "macos",
                    "source": "current-worktree",
                    "executable": "cataclysm-tiles",
                    "profile": "test-profile",
                    "fixture": "test-fixture",
                    "helper": "Peekaboo",
                    "peekaboo": True,
                    "input": ["press:."],
                    "ocr": True,
                    "cleanup": True,
                },
                "grants_gameplay_proof": False,
            },
            "run_class": "non_combat",
            "observer_character": True,
            "required_stabilizer_traits": [
                "DEBUG_LS",
                "DEBUG_NOTEMP",
                "DEBUG_STAMINA",
                "DEBUG_CARDIO",
                "DEBUG_CLAIRVOYANCE",
                "DEBUG_NIGHTVISION",
            ],
            "installed_save_player": "#player.sav.zzip",
            "proof_gates": [{
                "id": "test_gate",
                "label": "Test causal gate",
                "boundary_step": "gate_boundary",
                "predecessors": [],
                "expectations": [{
                    "kind": "structured_event",
                    "predicate": {"transition": "test", "committed": True},
                }],
                "checkpoint_safe_ui": {"screen_text_contains": ["Move:"]},
            }],
            "proof_route": {
                "gates": ["test_gate"],
                "terminal": ["test_gate"],
                "capability_gates": {
                    "capabilities.test.route": {"terminal": ["test_gate"]},
                },
            },
        }

    def test_only_declared_traits_are_required_regardless_of_run_class(self) -> None:
        for run_class, observer in (("combat", False), ("non_combat", True)):
            contract = self.contract()
            contract.update(run_class=run_class, observer_character=observer,
                            required_stabilizer_traits=["DEBUG_NIGHTVISION"])
            policy = startup_harness.checkpoint_contract_trait_policy(contract)
            self.assertEqual(policy["required_traits"], ["DEBUG_NIGHTVISION"])
            self.assertEqual(policy["declaration_error"], "")
            del contract["required_stabilizer_traits"]
            self.assertEqual(startup_harness.checkpoint_contract_trait_policy(contract)["required_traits"], [])

    def test_malformed_declared_traits_reject_before_save_audit(self) -> None:
        for declared in (None, "DEBUG_LS", [True], [""], [" DEBUG_LS"]):
            with self.subTest(declared=declared):
                contract = self.contract()
                contract["required_stabilizer_traits"] = declared
                with mock.patch.object(startup_harness, "audit_saved_player_condition") as audit:
                    result = startup_harness.run_checkpoint_contract_preflight(
                        contract, source={}, validation={}, profile="test-profile", world="World", fixture_install={})
                self.assertFalse(result["accepted"])
                self.assertEqual(result["status"], "rejected_static_trait_policy")
                audit.assert_not_called()

    def test_installed_ordinary_camp_needs_no_blanket_traits_but_explicit_constraints_hold(self) -> None:
        cases = ((None, None, [], True), ([], None, [], True),
                 (["DEBUG_NIGHTVISION"], None, [], False),
                 (None, "invisible", [], False),
                 (None, "invisible", ["DEBUG_CLOAK"], True))
        for declared, safety, saved_traits, accepted in cases:
            with self.subTest(declared=declared, safety=safety, saved_traits=saved_traits), tempfile.TemporaryDirectory() as tmp:
                contract = self.contract()
                contract.pop("required_stabilizer_traits")
                if declared is not None:
                    contract["required_stabilizer_traits"] = declared
                if safety is not None:
                    contract["observer_safety_mode"] = safety
                world = Path(tmp) / "Camp"
                world.mkdir()
                def extract(path):
                    path.with_suffix("").write_text(json.dumps({"player": {"traits": saved_traits}}))
                with (mock.patch.object(startup_harness, "save_dir_for_profile", return_value=Path(tmp)),
                      mock.patch.object(startup_harness, "run_zzip", side_effect=extract)):
                    result = startup_harness.run_checkpoint_contract_preflight(
                        contract, source={}, validation={}, profile="camp", world="Camp", fixture_install={})
                self.assertEqual(result["accepted"], accepted)
                self.assertEqual(result["installed_save_audit"]["observed_traits"], saved_traits)
                if not accepted:
                    self.assertTrue(result["installed_save_audit"]["missing_traits"])

    def test_invisible_observer_contract_requires_cloak(self) -> None:
        contract = self.contract()
        contract["observer_safety_mode"] = "invisible"
        contract["required_stabilizer_traits"].append("DEBUG_CLOAK")
        observer = startup_harness.checkpoint_contract_trait_policy(contract)
        self.assertEqual(observer["required_traits"][-1], "DEBUG_CLOAK")
        self.assertEqual(observer["declaration_error"], "")

    def test_valid_installed_save_records_the_exact_policy_and_observations(self) -> None:
        observed_audit = {
            "status": "required_state_present",
            "required_traits": [
                "DEBUG_LS",
                "DEBUG_NOTEMP",
                "DEBUG_STAMINA",
                "DEBUG_CARDIO",
                "DEBUG_CLAIRVOYANCE",
                "DEBUG_NIGHTVISION",
            ],
            "observed_traits": [
                "DEBUG_CARDIO",
                "DEBUG_CLAIRVOYANCE",
                "DEBUG_LS",
                "DEBUG_NIGHTVISION",
                "DEBUG_NOTEMP",
                "DEBUG_STAMINA",
            ],
            "missing_traits": [],
            "forbidden_traits": [],
            "observed_forbidden_traits": [],
        }
        with mock.patch.object(
                startup_harness, "audit_saved_player_condition", return_value=observed_audit):
            result = startup_harness.run_checkpoint_contract_preflight(
                self.contract(),
                source={"path": "/tmp/contract.json", "sha256": "bound"},
                validation={"status": "valid", "review_required": False},
                profile="test-profile",
                world="Test World",
                fixture_install={"fixture": "test-fixture"},
            )

        self.assertTrue(result["accepted"])
        self.assertEqual(result["status"], "accepted")
        self.assertEqual(result["execution_status"], "not_started")
        self.assertEqual(result["static_validation"]["status"], "valid")
        self.assertEqual(result["installed_save_audit"], observed_audit)
        self.assertEqual(result["normalized_contract"], self.contract())

    def test_rejected_installed_save_writes_preflight_and_never_launches(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            manifest_path = root / "contract.json"
            manifest_path.write_text(json.dumps(self.contract()), encoding="utf-8")
            run_dir = root / "run"
            plan = startup_harness.StartupPlan(
                profile="test-profile",
                userdir=str(root / "userdir"),
                executable=str(root / "cataclysm-tiles"),
                strategy="play_now_default",
                reason="test",
                target_world="Test World",
                existing_worlds=[],
                fixture="test-fixture",
                run_dir=str(run_dir),
            )
            args = startup_harness.build_parser().parse_args([
                "start",
                "--profile", "test-profile",
                "--world", "Test World",
                "--fixture", "test-fixture",
                "--replace-existing-worlds",
                "--scenario-identity", "test.checkpoint.contract",
                "--scenario-contract-path", str(manifest_path),
            ])
            launch_game = mock.Mock()
            rejected_audit = {
                "status": "required_state_missing",
                "required_traits": [
                    "DEBUG_LS", "DEBUG_NOTEMP", "DEBUG_STAMINA", "DEBUG_CARDIO",
                    "DEBUG_CLAIRVOYANCE", "DEBUG_NIGHTVISION",
                ],
                "observed_traits": ["DEBUG_LS", "DEBUG_NOTEMP", "DEBUG_CARDIO", "DEBUG_CLAIRVOYANCE", "DEBUG_NIGHTVISION"],
                "missing_traits": ["DEBUG_STAMINA"],
                "forbidden_traits": [],
                "observed_forbidden_traits": [],
            }
            with (
                mock.patch.object(startup_harness, "load_profile_config", return_value={}),
                mock.patch.object(startup_harness, "install_fixture", return_value={"fixture": "test-fixture"}) as install_fixture,
                mock.patch.object(startup_harness, "purge_profile_flexbuffer_cache", return_value={}),
                mock.patch.object(startup_harness, "build_plan", return_value=plan),
                mock.patch.object(startup_harness, "build_runtime_binding", return_value={"ok": True}),
                mock.patch.object(startup_harness, "audit_saved_player_condition", return_value=rejected_audit) as audit,
                mock.patch.object(startup_harness, "launch_game", launch_game),
                redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(startup_harness.run_startup(args), 1)

            install_fixture.assert_called_once()
            self.assertEqual(
                audit.call_args.kwargs["required_traits"],
                [
                    "DEBUG_LS",
                    "DEBUG_NOTEMP",
                    "DEBUG_STAMINA",
                    "DEBUG_CARDIO",
                    "DEBUG_CLAIRVOYANCE",
                    "DEBUG_NIGHTVISION",
                ],
            )
            launch_game.assert_not_called()
            self.assertFalse((run_dir / "process.json").exists())
            preflight = json.loads((run_dir / startup_harness.CONTRACT_PREFLIGHT_FILENAME).read_text())
            self.assertEqual(preflight["status"], "rejected_installed_save_policy")
            self.assertEqual(preflight["execution_status"], "not_started")


if __name__ == "__main__":
    unittest.main()
