#!/usr/bin/env python3
"""Thin zero-credit binding for R-022's native item-spawn transaction.

The adapter deliberately does not create items.  ``src/wish.cpp`` owns the
native transaction and tag-scoped cleanup; this module only binds the declared
transaction, runtime authority, and its receipts into a setup-support artifact.
"""

from __future__ import annotations

from typing import Any, Dict, Mapping


class ItemSpawnAdapterError(RuntimeError):
    """The R-022 declaration or native receipt cannot be trusted."""


_NATIVE_OWNER = "debug_menu::debug_item_spawn_transaction"
_NATIVE_CLEANUP_OWNER = "debug_menu::debug_item_spawn_transaction_cleanup"
_ZERO_CREDIT_PROVENANCE = "debug_item_spawn_transaction: zero-credit setup mutation"


def _text(value: Any) -> str:
    return str(value or "").strip()


def validate_item_spawn_declaration(declaration: Mapping[str, Any]) -> Dict[str, Any]:
    """Normalize the one exact transaction the native owner may receive."""
    transaction_id = _text(declaration.get("transaction_id"))
    item_type = _text(declaration.get("type"))
    owner = _text(declaration.get("owner"))
    source = _text(declaration.get("native_source"))
    executable = _text(declaration.get("native_executable"))
    if not transaction_id or not item_type or not owner:
        raise ItemSpawnAdapterError("R-022 needs transaction_id, type, and owner")
    if int(declaration.get("quantity", 0)) <= 0 or int(declaration.get("charges", -1)) < 0 or \
            int(declaration.get("damage", -1)) < 0:
        raise ItemSpawnAdapterError("R-022 has an invalid declared item state")
    destination = declaration.get("destination_offset_ms")
    if not isinstance(destination, list) or len(destination) != 3 or \
            any(type(value) is not int for value in destination):
        raise ItemSpawnAdapterError("R-022 needs one exact integer destination_offset_ms")
    if _text(declaration.get("transaction_owner")) != _NATIVE_OWNER:
        raise ItemSpawnAdapterError("R-022 has the wrong native transaction owner")
    if _text(declaration.get("cleanup_owner")) != _NATIVE_CLEANUP_OWNER:
        raise ItemSpawnAdapterError("R-022 has the wrong native cleanup owner")
    if source != "src/wish.cpp" or not executable:
        raise ItemSpawnAdapterError("R-022 must bind src/wish.cpp and one executable")
    if _text(declaration.get("evidence_ceiling")) != "none_for_debug_fixture_transaction":
        raise ItemSpawnAdapterError("R-022 must remain a zero-credit setup transaction")
    return {
        "transaction_id": transaction_id,
        "type": item_type,
        "quantity": int(declaration["quantity"]),
        "charges": int(declaration["charges"]),
        "damage": int(declaration["damage"]),
        "owner": owner,
        "destination_offset_ms": list(destination),
        "native_source": source,
        "native_executable": executable,
        "transaction_owner": _NATIVE_OWNER,
        "cleanup_owner": _NATIVE_CLEANUP_OWNER,
        "evidence_ceiling": "none_for_debug_fixture_transaction",
    }


def item_spawn_child_environment(declaration: Mapping[str, Any]) -> Dict[str, str]:
    """Authorize exactly the declared native bridge in its disposable child."""
    exact = validate_item_spawn_declaration(declaration)
    return {"OPENCLAW_HARNESS_R022_TRANSACTION_ID": exact["transaction_id"]}


def bind_item_spawn_receipts(
    declaration: Mapping[str, Any], native_receipt: Mapping[str, Any],
    cleanup_receipt: Mapping[str, Any], runtime_binding: Mapping[str, Any],
) -> Dict[str, Any]:
    """Bind native transaction/cleanup receipts without granting item proof."""
    exact = validate_item_spawn_declaration(declaration)
    if native_receipt.get("accepted") is not True or native_receipt.get("audit_passed") is not True:
        raise ItemSpawnAdapterError("R-022 native item transaction was not accepted and audited")
    if native_receipt.get("transaction_id") != exact["transaction_id"]:
        raise ItemSpawnAdapterError("R-022 native item transaction identity drifted")
    if native_receipt.get("zero_credit") is not True or \
            native_receipt.get("provenance") != _ZERO_CREDIT_PROVENANCE:
        raise ItemSpawnAdapterError("R-022 native receipt attempted to escape zero-credit provenance")
    if native_receipt.get("type") != exact["type"] or \
            native_receipt.get("quantity") != exact["quantity"] or \
            native_receipt.get("charges") != exact["charges"] or \
            native_receipt.get("damage") != exact["damage"] or \
            native_receipt.get("owner") != exact["owner"] or \
            native_receipt.get("destination_offset_ms") != exact["destination_offset_ms"]:
        raise ItemSpawnAdapterError("R-022 native receipt declared item state or destination drifted")
    identities = native_receipt.get("identities")
    if not isinstance(identities, list) or len(identities) != exact["quantity"]:
        raise ItemSpawnAdapterError("R-022 native receipt has a partial or duplicate item quantity")
    expected_ordinals = set(range(exact["quantity"]))
    observed_ordinals = {identity.get("ordinal") for identity in identities if isinstance(identity, Mapping)}
    if observed_ordinals != expected_ordinals or any(
            not isinstance(identity, Mapping) or
            identity.get("type") != exact["type"] or
            identity.get("charges") != exact["charges"] or
            identity.get("damage") != exact["damage"] or
            identity.get("owner") != exact["owner"]
            for identity in identities):
        raise ItemSpawnAdapterError("R-022 native receipt item identity drifted")
    if cleanup_receipt.get("accepted") is not True or cleanup_receipt.get("audit_passed") is not True or \
            cleanup_receipt.get("transaction_id") != exact["transaction_id"] or \
            cleanup_receipt.get("zero_credit") is not True or \
            int(cleanup_receipt.get("removed", -1)) != exact["quantity"] or \
            int(cleanup_receipt.get("retained_untagged", -1)) != 0:
        raise ItemSpawnAdapterError("R-022 cleanup receipt is missing or not tag-scoped")
    source = _text(runtime_binding.get("source"))
    executable = _text(runtime_binding.get("executable"))
    if not source or not executable:
        raise ItemSpawnAdapterError("R-022 needs the registry source and executable binding")
    return {
        "artifact_kind": "r022_item_spawn_transaction",
        "transaction": exact,
        "native_receipt": dict(native_receipt),
        "cleanup_receipt": dict(cleanup_receipt),
        "runtime_binding": {"source": source, "executable": executable},
        "setup_support_authority": "debug_setup_only",
        "evidence_effect": "none_for_debug_fixture_transaction",
        "gameplay_credit": False,
    }
