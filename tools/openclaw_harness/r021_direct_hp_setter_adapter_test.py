#!/usr/bin/env python3
"""R-021 native direct HP setter adapter contract."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from r021_direct_hp_setter_adapter import (  # noqa: E402
    DirectHpSetterAdapterError,
    bind_direct_hp_setter_receipt,
    direct_hp_setter_child_environment,
)


class DirectHpSetterAdapterTest(unittest.TestCase):
    declaration = {
        "fixture_actor_id": "r021-disposable-hp-target-v1",
        "target_hp": 0,
        "action_owner": "debug_menu.monster_set_hp",
        "cleanup_owner": "r021_direct_hp_setter_bootstrap_v1",
    }

    def receipt(self, **overrides: object) -> dict[str, object]:
        value: dict[str, object] = {
            "accepted": True,
            "target_fixture_actor_id": "r021-disposable-hp-target-v1",
            "native_setter": "monster::set_hp",
            "cause": "debug_menu_direct_set_hp",
            "gameplay_credit": "none",
            "hp_before": 80,
            "hp_after": 0,
        }
        value.update(overrides)
        return value

    def test_binds_one_exact_zero_credit_native_receipt(self) -> None:
        self.assertEqual(
            direct_hp_setter_child_environment(self.declaration),
            {"OPENCLAW_HARNESS_R021_FIXTURE_ACTOR_ID": "r021-disposable-hp-target-v1"},
        )
        bound = bind_direct_hp_setter_receipt(self.declaration, self.receipt())
        self.assertEqual(bound["cleanup_owner"], "r021_direct_hp_setter_bootstrap_v1")
        self.assertFalse(bound["gameplay_credit"])

    def test_rejects_stale_ambiguous_or_non_native_receipts(self) -> None:
        for receipt in (
            self.receipt(target_fixture_actor_id="replacement"),
            self.receipt(accepted=False),
            self.receipt(native_setter="damage"),
            self.receipt(gameplay_credit=True),
        ):
            with self.assertRaises(DirectHpSetterAdapterError):
                bind_direct_hp_setter_receipt(self.declaration, receipt)

    def test_native_path_is_a_direct_hp_setter_without_gameplay_owners(self) -> None:
        source = (HARNESS_DIR.parents[1] / "src" / "debug_menu.cpp").read_text(encoding="utf-8")
        start = source.index('case D_HP: {')
        end = source.index('        break;\n        case D_MORALE:', start)
        direct_path = source[start:end]
        self.assertIn('critter->set_hp( value );', direct_path)
        self.assertIn('native_setter=monster::set_hp', direct_path)
        self.assertNotIn('deal_damage', direct_path)
        self.assertNotIn('apply_damage', direct_path)
        self.assertNotIn('killer', direct_path)
        self.assertNotIn('ecology', direct_path)


if __name__ == "__main__":
    unittest.main()
