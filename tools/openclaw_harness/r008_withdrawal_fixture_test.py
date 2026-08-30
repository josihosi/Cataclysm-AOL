#!/usr/bin/env python3
"""Fail-closed fixture controls for the R-008 withdrawal diagnosis."""

from __future__ import annotations

import shutil
import sys
import tempfile
import unittest
from unittest import mock
import json
from pathlib import Path


HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

import startup_harness  # noqa: E402


class R008WithdrawalFixtureTest(unittest.TestCase):
    fixture = "bandit_r008_natural_safe_watch_withdrawal_free_v1"
    profile = "r008-closure-231-withdrawal-free"
    player_save = "#R2xvcnkgVHJlam8=.sav.zzip"
    cause = {"type": "sleeping pill", "intensity": 20, "sated": 3599}

    def copied_world(self) -> tuple[Path, tempfile.TemporaryDirectory[str]]:
        temporary = tempfile.TemporaryDirectory()
        resolved = startup_harness.resolve_fixture_payload(self.fixture, self.profile)
        copied = Path(temporary.name) / "fixture"
        shutil.copytree(resolved["save_src"], copied)
        return copied / "r008-natural-bandit-039", temporary

    def test_removes_only_observed_withdrawal_cause_with_zero_credit_receipt(self) -> None:
        world, temporary = self.copied_world()
        with temporary:
            resolved = startup_harness.resolve_fixture_payload(self.fixture, self.profile)
            transforms = resolved["save_transforms"]
            receipts = startup_harness.apply_fixture_save_transforms(world, transforms)
            setup = startup_harness.r008_withdrawal_free_player_fixture_receipt(
                world,
                setup_contract={
                    "world": "r008-natural-bandit-039", "player_save": self.player_save,
                    "withdrawal_cause": self.cause, "cleanup_owner": "cleanup_harness_world",
                },
                fixture_install={"binding": startup_harness.fixture_source_binding(self.fixture, self.profile),
                                 "applied_save_transforms": receipts},
            )
        receipt = receipts[-1]
        self.assertEqual(receipt["kind"], "player_remove_addiction")
        self.assertEqual(receipt["before_addictions"], [self.cause])
        self.assertEqual(receipt["after_addictions"], [])
        self.assertEqual(receipt["removed_addiction"], self.cause)
        self.assertTrue(receipt["declared_mutation_only"])
        self.assertEqual(receipt["cleanup_owner"], "cleanup_harness_world")
        self.assertEqual(receipt["evidence_effect"], "none_for_disposable_player_fixture")
        self.assertFalse(receipt["gameplay_credit"])
        self.assertEqual(setup["status"], "prepared")
        self.assertEqual(setup["interventions"][0]["before_facts"]["player_addictions"], [self.cause])
        self.assertEqual(setup["interventions"][0]["after_facts"]["player_addictions"], [])

    def test_rejects_missing_ambiguous_wrong_and_undeclared_player_state(self) -> None:
        cases = {
            "missing": [],
            "ambiguous": [self.cause, self.cause],
            "wrong_type": [{**self.cause, "type": "caffeine"}],
        }
        for name, addictions in cases.items():
            with self.subTest(name=name):
                world, temporary = self.copied_world()
                with temporary:
                    player_save = world / self.player_save
                    extracted = player_save.with_suffix("")
                    startup_harness.run_zzip(player_save)
                    payload = json.loads(extracted.read_text(encoding="utf-8"))
                    payload["player"]["addictions"] = addictions
                    extracted.write_text(json.dumps(payload), encoding="utf-8")
                    startup_harness.run_zzip(extracted)
                    transform = {
                        "kind": "player_remove_addiction", "player_save": self.player_save,
                        "addiction_type": self.cause["type"], "expected_intensity": 20, "expected_sated": 3599,
                    }
                    with self.assertRaises(SystemExit):
                        startup_harness.apply_player_remove_addiction_transform(world, transform)

        world, temporary = self.copied_world()
        with temporary:
            transform = {
                "kind": "player_remove_addiction", "player_save": self.player_save,
                "addiction_type": self.cause["type"], "expected_intensity": 20, "expected_sated": 3599,
            }
            real_load = startup_harness.load_saved_player_payload

            def altered_post_write(*args: object, **kwargs: object) -> tuple:
                selected, path, payload, stat = real_load(*args, **kwargs)
                payload["player"]["undeclared_fixture_mutation"] = True
                return selected, path, payload, stat

            with mock.patch.object(startup_harness, "load_saved_player_payload", side_effect=altered_post_write):
                with self.assertRaisesRegex(RuntimeError, "undeclared player mutation"):
                    startup_harness.apply_player_remove_addiction_transform(world, transform)

        with self.assertRaises(SystemExit):
            startup_harness.normalize_fixture_save_transforms([
                {"kind": "player_remove_addiction", "player_save": self.player_save,
                 "addiction_type": "sleeping pill", "expected_intensity": 20,
                 "expected_sated": 3599, "undeclared": True},
            ], manifest_path=Path("fixture/manifest.json"))


if __name__ == "__main__":
    unittest.main()
