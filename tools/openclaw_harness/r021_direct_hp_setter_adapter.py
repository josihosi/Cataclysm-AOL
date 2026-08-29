#!/usr/bin/env python3
"""Exact-fixture binding for R-021's native debug HP setter.

This adapter owns only declaration validation and receipt binding.  It never
mutates a save or interprets a damage/death event: the native debug-menu path
performs the direct ``monster::set_hp`` call.
"""

from __future__ import annotations

from typing import Any, Dict, Mapping


class DirectHpSetterAdapterError(RuntimeError):
    """The R-021 fixture declaration or native receipt is unsafe."""


def direct_hp_setter_child_environment(declaration: Mapping[str, Any]) -> Dict[str, str]:
    """Bind exactly one declared fixture actor into the game child."""
    actor_id = str(declaration.get("fixture_actor_id", "")).strip()
    if not actor_id:
        raise DirectHpSetterAdapterError("R-021 direct HP setter needs a fixture_actor_id")
    if int(declaration.get("target_hp", -1)) != 0:
        raise DirectHpSetterAdapterError("R-021 direct HP setter must declare target_hp=0")
    if str(declaration.get("action_owner", "")).strip() != "debug_menu.monster_set_hp":
        raise DirectHpSetterAdapterError("R-021 direct HP setter has the wrong action owner")
    if not str(declaration.get("cleanup_owner", "")).strip():
        raise DirectHpSetterAdapterError("R-021 direct HP setter needs a cleanup owner")
    return {"OPENCLAW_HARNESS_R021_FIXTURE_ACTOR_ID": actor_id}


def bind_direct_hp_setter_receipt(
    declaration: Mapping[str, Any], receipt: Mapping[str, Any],
) -> Dict[str, Any]:
    """Accept only one exact native direct-set receipt, never gameplay credit."""
    actor_id = direct_hp_setter_child_environment(declaration)[
        "OPENCLAW_HARNESS_R021_FIXTURE_ACTOR_ID"
    ]
    if receipt.get("accepted") is not True:
        raise DirectHpSetterAdapterError("R-021 native direct HP setter was not accepted")
    if receipt.get("target_fixture_actor_id") != actor_id:
        raise DirectHpSetterAdapterError("R-021 native receipt fixture identity is stale or wrong")
    if receipt.get("native_setter") != "monster::set_hp":
        raise DirectHpSetterAdapterError("R-021 receipt did not use monster::set_hp")
    if receipt.get("cause") != "debug_menu_direct_set_hp":
        raise DirectHpSetterAdapterError("R-021 receipt has an unapproved cause")
    if receipt.get("gameplay_credit") not in {False, "none"}:
        raise DirectHpSetterAdapterError("R-021 receipt attempted gameplay credit")
    if int(receipt.get("hp_before", 0)) <= 0 or int(receipt.get("hp_after", -1)) != 0:
        raise DirectHpSetterAdapterError("R-021 receipt lacks the declared direct HP transition")
    return {
        "operation": "r021_direct_hp_setter",
        "target_fixture_actor_id": actor_id,
        "native_receipt": dict(receipt),
        "cleanup_owner": str(declaration["cleanup_owner"]),
        "evidence_effect": "none_for_debug_fixture_transaction",
        "gameplay_credit": False,
    }
