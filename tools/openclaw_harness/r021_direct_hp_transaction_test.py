#!/usr/bin/env python3
"""Focused controls for the registry-owned R-021 transaction binding."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from r021_direct_hp_transaction import DirectHpTransactionError, bind_direct_hp_transaction


class R021TransactionTest(unittest.TestCase):
    declaration = {"fixture_actor_id": "fixture", "target_hp": 0,
                   "action_owner": "debug_menu.monster_set_hp", "cleanup_owner": "fixture"}

    def creature(self, actor="fixture", hp=80, position=(4, 0, 0)):
        return {"typeid": "mon_zombie", "location_ms": list(position), "hp": hp,
                "values": {"caol_fixture_actor_id": actor}}

    def receipt(self):
        return {"accepted": True, "target_fixture_actor_id": "fixture",
                "native_setter": "monster::set_hp", "cause": "debug_menu_direct_set_hp",
                "gameplay_credit": "none", "hp_before": 80, "hp_after": 0}

    def bind(self, **kwargs):
        before = kwargs.pop("before", [self.creature()])
        after = kwargs.pop("after", [self.creature(hp=0)])
        return bind_direct_hp_transaction(self.declaration, kwargs.pop("receipts", [self.receipt()]),
                                          before, after, cleanup=kwargs.pop("cleanup", {"accepted": True}), **kwargs)

    def test_accepts_exact_one_fixture_receipt_with_zero_credit(self):
        result = self.bind()
        self.assertFalse(result["gameplay_credit"])
        self.assertEqual(len(result["changed_creature_identities"]), 1)

    def test_rejects_exact_receipt_count_and_identity_controls(self):
        for receipts in ([], [self.receipt(), self.receipt()], [dict(self.receipt(), target_fixture_actor_id="stale")]):
            with self.assertRaises(DirectHpTransactionError): self.bind(receipts=receipts)
        with self.assertRaises(DirectHpTransactionError): self.bind(before=[self.creature(), self.creature(position=(5, 0, 0))])

    def test_rejects_protected_incidental_partial_and_nonselected_changes(self):
        cases = (
            {"avatar_targeted": True}, {"operation_owned_ecology_targeted": True},
            {"after": []}, {"after": [self.creature(hp=40)]},
            {"before": [self.creature(), self.creature("nearby", 20, (5, 0, 0))],
             "after": [self.creature(hp=0), self.creature("nearby", 19, (5, 0, 0))]},
            {"cleanup": {"accepted": False}},
        )
        for case in cases:
            with self.assertRaises(DirectHpTransactionError): self.bind(**case)


if __name__ == "__main__":
    unittest.main()
