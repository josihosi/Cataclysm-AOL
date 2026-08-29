#!/usr/bin/env python3
"""Focused R-022 declaration and zero-credit receipt controls."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HARNESS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(HARNESS_DIR))

from r022_item_spawn_adapter import (ItemSpawnAdapterError, bind_item_spawn_receipts,
                                     item_spawn_child_environment, validate_item_spawn_declaration)
import startup_harness


class R022ItemSpawnAdapterTest(unittest.TestCase):
    declaration = {
        "transaction_id": "r022-disposable-item-spawn-v1",
        "type": "apple", "quantity": 3, "charges": 0, "damage": 0,
        "owner": "your_followers", "destination_offset_ms": [4, 0, 0],
        "native_source": "src/wish.cpp", "native_executable": "cataclysm-tiles",
        "transaction_owner": "debug_menu::debug_item_spawn_transaction",
        "cleanup_owner": "debug_menu::debug_item_spawn_transaction_cleanup",
        "evidence_ceiling": "none_for_debug_fixture_transaction",
    }

    def native_receipt(self, **overrides: object) -> dict[str, object]:
        receipt: dict[str, object] = {
            "accepted": True, "audit_passed": True, "zero_credit": True,
            "transaction_id": self.declaration["transaction_id"],
            "provenance": "debug_item_spawn_transaction: zero-credit setup mutation",
            "type": "apple", "quantity": 3, "charges": 0, "damage": 0,
            "owner": "your_followers", "destination_offset_ms": [4, 0, 0],
            "identities": [{"ordinal": ordinal, "type": "apple", "charges": 0,
                            "damage": 0, "owner": "your_followers"} for ordinal in range(3)],
        }
        receipt.update(overrides)
        return receipt

    def cleanup_receipt(self, **overrides: object) -> dict[str, object]:
        receipt: dict[str, object] = {
            "accepted": True, "audit_passed": True, "zero_credit": True,
            "transaction_id": self.declaration["transaction_id"], "removed": 3,
            "retained_untagged": 0,
        }
        receipt.update(overrides)
        return receipt

    def test_binds_exact_native_transaction_cleanup_and_runtime_authority(self) -> None:
        self.assertEqual(item_spawn_child_environment(self.declaration), {
            "OPENCLAW_HARNESS_R022_TRANSACTION_ID": "r022-disposable-item-spawn-v1"})
        artifact = bind_item_spawn_receipts(
            self.declaration, self.native_receipt(), self.cleanup_receipt(),
            {"source": "source-digest", "executable": "executable-digest"},
        )
        self.assertFalse(artifact["gameplay_credit"])
        self.assertEqual(artifact["transaction"]["cleanup_owner"],
                         "debug_menu::debug_item_spawn_transaction_cleanup")
        self.assertEqual(artifact["runtime_binding"]["executable"], "executable-digest")

    def test_rejects_provenance_identity_cleanup_and_binding_drift(self) -> None:
        cases = (
            (self.native_receipt(zero_credit=False), self.cleanup_receipt(), {"source": "s", "executable": "e"}),
            (self.native_receipt(identities=self.native_receipt()["identities"][:2]), self.cleanup_receipt(), {"source": "s", "executable": "e"}),
            (self.native_receipt(), self.cleanup_receipt(removed=2), {"source": "s", "executable": "e"}),
            (self.native_receipt(destination_offset_ms=[5, 0, 0]), self.cleanup_receipt(), {"source": "s", "executable": "e"}),
            (self.native_receipt(), self.cleanup_receipt(retained_untagged=1), {"source": "s", "executable": "e"}),
            (self.native_receipt(), self.cleanup_receipt(), {"source": "s", "executable": ""}),
        )
        for native, cleanup, binding in cases:
            with self.assertRaises(ItemSpawnAdapterError):
                bind_item_spawn_receipts(self.declaration, native, cleanup, binding)

    def test_owner_is_wish_cpp_and_not_a_gameplay_path(self) -> None:
        source = (HARNESS_DIR.parents[1] / "src" / "wish.cpp").read_text(encoding="utf-8")
        start = source.index("debug_menu::debug_item_spawn_receipt debug_menu::debug_item_spawn_transaction")
        end = source.index("namespace\n{\nclass wish_item_callback", start)
        native_path = source[start:end]
        self.assertIn("debug_item_spawn_transaction_cleanup", native_path)
        self.assertIn("debug_item_spawn_ordinal", native_path)
        self.assertNotIn("deal_damage", native_path)
        self.assertNotIn("ecology", native_path)

    def test_declaration_rejects_wrong_owner_or_nonzero_credit(self) -> None:
        for changed in ({"transaction_owner": "wishitem"}, {"evidence_ceiling": "focused"}):
            with self.assertRaises(ItemSpawnAdapterError):
                validate_item_spawn_declaration({**self.declaration, **changed})

    def test_bridge_authority_fails_closed_for_invalid_declaration(self) -> None:
        with self.assertRaises(ItemSpawnAdapterError):
            item_spawn_child_environment({**self.declaration, "quantity": 0})

    def test_step_ledger_accepts_only_the_complete_same_run_artifact(self) -> None:
        artifact = bind_item_spawn_receipts(
            self.declaration, self.native_receipt(), self.cleanup_receipt(),
            {"source": "source-digest", "executable": "executable-digest"},
        )
        artifact.update({
            "bridge_owner": "openclaw_harness_r022_item_spawn_bridge",
            "run_id": "fresh-run", "artifact_path": "receipt.json",
        })
        report = {"index": 1, "kind": "r022_item_spawn_bridge", "label": "spawn",
                  "run_id": "fresh-run", "metadata": artifact}
        self.assertEqual(startup_harness.build_probe_step_ledger([report])[0]["verdict"],
                         "green_step_r022_native_item_spawn_transaction")

    def test_step_ledger_rejects_missing_contradictory_and_bootstrap_artifacts(self) -> None:
        artifact = bind_item_spawn_receipts(
            self.declaration, self.native_receipt(), self.cleanup_receipt(),
            {"source": "source-digest", "executable": "executable-digest"},
        )
        artifact.update({
            "bridge_owner": "openclaw_harness_r022_item_spawn_bridge",
            "run_id": "fresh-run", "artifact_path": "receipt.json",
        })
        for changed in (
            {"run_id": "stale-run"},
            {"bridge_owner": "r022.item_spawn_bootstrap"},
            {"runtime_binding": {"source": "", "executable": "executable-digest"}},
            {"native_receipt": {**artifact["native_receipt"], "owner": "drifted"}},
            {"cleanup_receipt": {**artifact["cleanup_receipt"], "retained_untagged": 1}},
        ):
            report = {"index": 1, "kind": "r022_item_spawn_bridge", "label": "spawn",
                      "run_id": "fresh-run", "metadata": {**artifact, **changed}}
            self.assertEqual(startup_harness.build_probe_step_ledger([report])[0]["verdict"],
                             "red_step_r022_native_item_spawn_transaction_unbound")


if __name__ == "__main__":
    unittest.main()
