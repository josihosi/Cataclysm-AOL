#!/usr/bin/env python3
"""Focused contracts for the deterministic NPC harness parser."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

RUNNER_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(RUNNER_DIR))

from npc_harness import (  # noqa: E402
    build_snapshot,
    normalize_csv_separators,
    parse_move_field,
    render_prompt,
    render_map_with_axes,
    ResolvedSummary,
    validate_csv_payload,
    validate_response_like_game,
)


class NpcHarnessParsingTest(unittest.TestCase):
    def test_prompt_and_parser_share_delta_move_grammar(self) -> None:
        prompt = render_prompt("{{action_list_with_target}}\n{{snapshot}}", "snapshot")

        self.assertIn("move=<dx>,<dy> <state>", prompt)
        self.assertNotIn("move: <coordinate>", prompt)
        self.assertEqual(
            parse_move_field("MOVE= -20,+20 WAIT_HERE"),
            ((-20, 20), "wait_here", None),
        )

    def test_snapshot_map_has_production_like_coordinate_axes(self) -> None:
        rendered_map = render_map_with_axes("-----\n--a--\n--|--\n-----\n-----")

        self.assertIn("        ..|..", rendered_map)
        self.assertIn("dy=+01 --a--", rendered_map)
        self.assertIn("dy=+00 --|--", rendered_map)
        self.assertIn("dy=-02 -----", rendered_map)

        snapshot = build_snapshot({}, ResolvedSummary(None, None, []), "req-map")
        self.assertIn(
            "map axes: +x east/right, -x west/left, +y north/up, -y south/down",
            snapshot,
        )
        self.assertIn("        ..|..", snapshot)

    def test_move_parser_rejects_unseen_or_ambiguous_deltas(self) -> None:
        invalid_fields = (
            "move=21,0 hold_position",
            "move=-21,0 wait_here",
            "move=1_0,0 wait_here",
            "move=1,\u0662 wait_here",
            "move=4, -2 hold_position",
            "move: E E hold_position",
        )

        for field in invalid_fields:
            with self.subTest(field=field):
                delta, terminal, error = parse_move_field(field)
                self.assertIsNone(delta)
                self.assertIsNone(terminal)
                self.assertTrue(error)

    def test_csv_attack_and_move_rules_match_the_game_contract(self) -> None:
        ok, error, actions = validate_csv_payload(
            "Engaging|attack=a,|equip_gun|follow_close panic_off"
        )
        self.assertTrue(ok, error)
        self.assertEqual(
            actions,
            ["equip_gun", "follow_close", "panic_off", "attack=a"],
        )

        ok, error, actions = validate_csv_payload(
            "Moving|move=4,-2 wait_here|equip_gun"
        )
        self.assertTrue(ok, error)
        self.assertEqual(actions, ["move=4,-2 wait_here", "equip_gun"])

        ok, error, actions = validate_csv_payload(
            " | Inspecting inventory... I'm carrying a sharpened rebar. | equip_melee"
        )
        self.assertTrue(ok, error)
        self.assertEqual(actions, ["equip_melee"])
        leading_separator = validate_response_like_game(
            " | Inspecting inventory... I'm carrying a sharpened rebar. | equip_melee"
        )
        self.assertEqual(
            leading_separator["parsed_speech"],
            "Inspecting inventory... I'm carrying a sharpened rebar.",
        )
        self.assertEqual(leading_separator["parsed_actions"], ["equip_melee"])
        strict_prefix = validate_response_like_game(
            "Listener NPC: Holding here. | hold_position"
        )
        self.assertEqual(strict_prefix["parsed_speech"], "Holding here.")
        lenient_prefix = validate_response_like_game(
            "Listener NPC: Switching weapons. | equip_melee extra_prose"
        )
        self.assertEqual(lenient_prefix["mode"], "lenient")
        self.assertEqual(lenient_prefix["parsed_speech"], "Switching weapons.")

        ok, error, actions = validate_csv_payload("||Speech|equip_melee")
        self.assertFalse(ok)
        self.assertEqual(error, "CSV speech field missing.")
        self.assertEqual(actions, [])

        ok, error, actions = validate_csv_payload(
            "|Speech|equip_melee|follow_close|panic_off|wait_here"
        )
        self.assertFalse(ok)
        self.assertEqual(error, "CSV has too many action fields.")
        self.assertEqual(actions, [])

        ok, error, actions = validate_csv_payload("|Speech|equip_melee|")
        self.assertFalse(ok)
        self.assertEqual(error, "CSV action token is invalid.")
        self.assertEqual(actions, [])

        ok, error, actions = validate_csv_payload(
            "No|move=1,0 wait_here|move=2,0 hold_position"
        )
        self.assertFalse(ok)
        self.assertEqual(error, "CSV move field repeated.")
        self.assertEqual(actions, [])

    def test_lenient_recovery_does_not_claim_legacy_text_is_movement(self) -> None:
        result = validate_response_like_game("On it|move: E E wait_here")

        self.assertTrue(result["ok"])
        self.assertEqual(result["mode"], "lenient")
        self.assertEqual(result["parsed_actions"], ["wait_here"])

    def test_normalization_and_mixed_token_recovery_match_game(self) -> None:
        self.assertEqual(
            normalize_csv_separators("Moving+move=-20,+20 wait_here"),
            "Moving|move=-20,+20 wait_here",
        )
        normalized_move = validate_response_like_game(
            "Moving+move=-20,+20 wait_here"
        )
        self.assertEqual(normalized_move["mode"], "strict")
        self.assertEqual(
            normalized_move["parsed_actions"],
            ["move=-20,+20 wait_here"],
        )

        mixed = validate_response_like_game("Speech|bogus follow_close")
        self.assertEqual(mixed["mode"], "lenient")
        self.assertEqual(mixed["parsed_actions"], ["follow_close"])

        mixed_attack = validate_response_like_game(
            "Speech|attack=A bogus follow_close"
        )
        self.assertEqual(mixed_attack["mode"], "lenient")
        self.assertEqual(
            mixed_attack["parsed_actions"],
            ["follow_close", "attack=a"],
        )


if __name__ == "__main__":
    unittest.main()
