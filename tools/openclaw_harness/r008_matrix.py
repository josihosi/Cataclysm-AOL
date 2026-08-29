"""Finite, independently classified R-008 qualification matrix."""

from __future__ import annotations

from typing import Any, Mapping, Sequence


R008_MATRIX: tuple[dict[str, Any], ...] = (
    {
        "id": "bandit_local_return_home",
        "ecology": "bandit",
        "outcome": "scout returns home",
        "boundary": "local to abstract return-home decision",
        "save_points": ("after_return_home_decision", "after_relaunch"),
        "actor_identity": "scout member_ids from active_outing",
        "generation": "active_outing.generation",
        "owner": "active_outing.simulation_owner",
        "receipt": "crossing receipt for return-home handoff",
        "normalization": "saved active_outing and local_handoff agree",
        "production_owner": "do_turn live bandit return motor",
        "scenario": "bandit.local_scout_return_followthrough_mcw",
        "artifact": "structured transition stream plus saved-state audit",
    },
    {
        "id": "bandit_local_return_preaged",
        "ecology": "bandit",
        "outcome": "pre-aged scout return decision",
        "boundary": "abstract return decision to local projection",
        "save_points": ("before_return_decision", "after_relaunch"),
        "actor_identity": "scout member_ids from saved active_outing",
        "generation": "active_outing.generation",
        "owner": "active_outing.simulation_owner",
        "receipt": "return decision crossing receipt",
        "normalization": "saved cursor and outing identity remain paired",
        "production_owner": "bandit_live_world return decision transaction",
        "scenario": "bandit.local_scout_return_preaged_mcw",
        "artifact": "saved-state audit and transition stream",
    },
    {
        "id": "cannibal_dispatch_local_contact",
        "ecology": "cannibal",
        "outcome": "night active-sortie local contact",
        "boundary": "abstract dispatch to local contact",
        "save_points": ("after_dispatch", "after_local_contact_relaunch"),
        "actor_identity": "actor_ids from committed dispatch event",
        "generation": "dispatch event generation",
        "owner": "abstract then local",
        "receipt": "active_sortie dispatch and local-contact receipts",
        "normalization": "saved site has one active group and matching actor set",
        "production_owner": "bandit_live_world active-sortie scheduler",
        "scenario": "cannibal.live_world_night_local_contact_pack_mcw",
        "artifact": "two committed transition events and saved-state audit",
    },
    {
        "id": "cannibal_exposed_sight_avoid",
        "ecology": "cannibal",
        "outcome": "exposed sight avoidance remains responsive",
        "boundary": "local visibility response without ownership transfer",
        "save_points": ("after_exposed_window", "after_relaunch"),
        "actor_identity": "named cannibal members from saved site",
        "generation": "saved active outing generation or zero when idle",
        "owner": "local",
        "receipt": "no crossing receipt expected; absence is checked",
        "normalization": "saved site remains unchanged except permitted response state",
        "production_owner": "do_turn local visibility and sight-avoidance path",
        "scenario": "cannibal.live_world_exposed_sight_avoid_mcw",
        "artifact": "saved-state audit and no-injection control",
    },
)


def validate_r008_matrix(matrix: Sequence[Mapping[str, Any]] = R008_MATRIX) -> dict[str, Any]:
    required = {"id", "ecology", "outcome", "boundary", "save_points", "actor_identity", "generation",
                "owner", "receipt", "normalization", "production_owner", "scenario", "artifact"}
    rows = list(matrix)
    ids = [str(row.get("id", "")) for row in rows]
    errors: list[str] = []
    if len(ids) != len(set(ids)):
        errors.append("duplicate_row_identity")
    if {str(row.get("ecology", "")) for row in rows} != {"bandit", "cannibal"}:
        errors.append("both_ecology_families_required")
    for row in rows:
        missing = sorted(required - set(row))
        if missing:
            errors.append(f"{row.get('id', '<unknown>')}:missing:{','.join(missing)}")
        if not isinstance(row.get("save_points"), (list, tuple)) or len(row.get("save_points", ())) != 2:
            errors.append(f"{row.get('id', '<unknown>')}:two_save_points_required")
    return {"status": "green" if not errors else "red", "row_count": len(rows), "errors": errors}


__all__ = ["R008_MATRIX", "validate_r008_matrix"]
