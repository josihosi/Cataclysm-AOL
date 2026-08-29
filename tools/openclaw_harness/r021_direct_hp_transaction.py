#!/usr/bin/env python3
"""Fail-closed receipt and creature-delta controls for R-021.

The registry launch owns dispatch; this module owns the narrow interpretation
of the native debug-menu transaction.  It intentionally grants no gameplay
credit: its output is a disposable-fixture setup artifact only.
"""

from __future__ import annotations

from typing import Any, Mapping, Sequence

from r021_direct_hp_setter_adapter import (
    DirectHpSetterAdapterError,
    bind_direct_hp_setter_receipt,
)


class DirectHpTransactionError(RuntimeError):
    """The native R-021 transaction cannot be bound safely."""


def _identity(creature: Mapping[str, Any]) -> tuple[str, str, tuple[Any, ...]]:
    values = creature.get("values", {})
    actor_id = values.get("caol_fixture_actor_id", "") if isinstance(values, Mapping) else ""
    position = creature.get("location_ms", creature.get("position", []))
    if not isinstance(position, Sequence) or isinstance(position, (str, bytes)):
        position = []
    return str(actor_id), str(creature.get("typeid", creature.get("target_type", ""))), tuple(position)


def bind_direct_hp_transaction(
    declaration: Mapping[str, Any], receipts: Sequence[Mapping[str, Any]],
    before: Sequence[Mapping[str, Any]], after: Sequence[Mapping[str, Any]], *,
    cleanup: Mapping[str, Any], avatar_targeted: bool = False,
    operation_owned_ecology_targeted: bool = False,
) -> dict[str, Any]:
    """Bind exactly one direct-set receipt and prove its sole creature delta."""
    if avatar_targeted or operation_owned_ecology_targeted:
        raise DirectHpTransactionError("R-021 transaction targeted a protected non-fixture owner")
    if len(receipts) != 1:
        raise DirectHpTransactionError("R-021 transaction requires exactly one native receipt")
    try:
        bound = bind_direct_hp_setter_receipt(declaration, receipts[0])
    except DirectHpSetterAdapterError as exc:
        raise DirectHpTransactionError(str(exc)) from exc
    actor_id = str(declaration.get("fixture_actor_id", ""))
    before_fixture_count = sum(_identity(item)[0] == actor_id for item in before)
    before_by_identity = {_identity(item): item for item in before}
    after_by_identity = {_identity(item): item for item in after}
    fixture_matches = [item for key, item in before_by_identity.items() if key[0] == actor_id]
    if before_fixture_count != 1 or len(fixture_matches) != 1:
        raise DirectHpTransactionError("R-021 fixture identity is stale or duplicated before dispatch")
    changed = [key for key in set(before_by_identity) | set(after_by_identity)
               if before_by_identity.get(key) != after_by_identity.get(key)]
    fixture_key = _identity(fixture_matches[0])
    if changed != [fixture_key]:
        raise DirectHpTransactionError("R-021 changed a non-selected creature or has a partial mutation")
    before_target = before_by_identity[fixture_key]
    after_target = after_by_identity.get(fixture_key)
    if after_target is None:
        raise DirectHpTransactionError("R-021 native receipt did not bind an after-state for the fixture")
    if int(before_target.get("hp", 0)) <= 0 or int(after_target.get("hp", -1)) != 0:
        raise DirectHpTransactionError("R-021 does not prove the selected HP-to-zero transition")
    if cleanup.get("accepted") is not True:
        raise DirectHpTransactionError("R-021 direct-operation cleanup failed")
    return {
        "artifact_kind": "r021_direct_hp_setter",
        "operation": "r021_direct_hp_setter",
        "native_receipt": bound["native_receipt"],
        "fixture_actor_id": actor_id,
        "before_creatures": list(before),
        "after_creatures": list(after),
        "changed_creature_identities": [fixture_key],
        "cleanup_receipt": dict(cleanup),
        "evidence_effect": "none_for_debug_fixture_transaction",
        "gameplay_credit": False,
    }
