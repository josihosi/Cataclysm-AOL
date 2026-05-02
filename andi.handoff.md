# Andi handoff: CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

## Current canon state

`CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` is **ACTIVE / GREENLIT / ANTI-REDUNDANCY PACKAGE / DAMAGE-RESISTANCE SCORING API GREEN**.

`CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0` is closed/checkpointed green v0. Preserve its caveat: the named `targeted_live_plan_adapter` dispatch reduced inline live-planner exceptions, but no behavior-tree/special-attack seam honestly owns that destination-planning response today, so stalker-specific no-omniscience / quiet-side / light-focus / cooldown / repeated-strike / injured-retreat judgment remains custom and explicit.

## Canonical packet

- Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`
- Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`

## Green checkpoints already consumed

Do not repeat these seams unless a later change touches them again:

- coverage-helper API reduction
- zone-boundary / `NO_NPC_PICKUP` collection reduction
- medical-readiness `heal_actor` metadata reduction
- carried ranged-readiness `Character::find_ammo()` discovery reduction
- average-coverage `item::get_avg_coverage()` scoring reduction
- worker-item / cleanup enumeration `items_with()` reduction
- direct subpart `item::covers( sub_bodypart_id, false )` coverage reduction
- live worker-fit `item::get_avg_encumber()` scoring reduction
- live worker-wearability `Character::can_wear( ..., true )` candidate filtering
- shared `collect_camp_locker_live_state()` pre-pass and post-service summary reduction
- live weapon-wieldability `Character::can_wield()` candidate filtering
- ranged classification `item::is_gun()` reduction
- armor classification `item::is_armor()` reduction
- live weapon scoring `Character::evaluate_weapon( ..., true )` reduction
- worker-context reload viability `Character::can_reload()` reduction
- reload-supply ammo-state `item::ammo_remaining()` reduction
- magazine total-capacity `item::ammo_remaining()` plus `item::remaining_ammo_capacity()` reduction
- ranged readiness/reloadability-gate reduction through `item::can_reload_with()` / `Character::can_reload()`
- empty-magazine fallback through item-owned `remaining_ammo_capacity()` / default-ammo logic
- managed-ranged readiness helper `item::is_gun()` reduction
- ablative-insert compatibility reduction
- ranged ammo-state readiness `item::has_ammo()` reduction
- shared camp storage/locker `zone_manager::get_near()` tile collection reduction
- outer-layer classification `item::has_layer( { layer_level::OUTER } )` reduction
- final worn-slot equip validation through `Character::wear_item()` reduction
- direct medical supply lookup through `item::get_usable_item()` / `item::get_use()` reduction
- body-part-specific outer-layer classification `item::has_layer( ..., bodypart_id )` reduction for torso/arm/leg checks
- body-part id helper-call reduction through existing `body_part_*` ids instead of repeated local string-to-bodypart conversions
- sub-bodypart id helper-call reduction through `sub_bodypart_str_id` constants instead of repeated call-site string-to-sub-bodypart conversions
- belted-layer leg accessory classification through `item::has_layer({ layer_level::BELTED })` instead of raw `BELTED` flag reads for strapped armor
- damage-resistance scoring through the shared `resistances` aggregate instead of separate camp-local item-resistance lookups

## Next executor target

1. Inspect remaining camp locker scoring/readiness and cleanup seams for local ontology that still duplicates existing item, wearability, body coverage, reload, and zone APIs.
2. Pick the smallest refactor that removes duplicate ontology while preserving camp policy: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, camp-storage boundaries, and safe leftover returns.
3. Do not chase the retired quality-feedback line ids inside this camp-locker package: source inspection showed `basecamp.h:116` / `basecamp.h:122` are legacy basecamp resource/fuel `ammo_id` fields, and `basecamp.h:169` is the LLM craft-request `blockers` vector, not camp locker equipment API ontology.
4. After code changes, run the focused faction/basecamp/camp-locker gate identified by the audit.

## Non-goals/cautions

- Do not redesign basecamp missions, Smart Zones, or general NPC equipment selection.
- Do not retune outfit preferences beyond preservation needs.
- Do not reopen closed writhing-stalker, zombie-rider, flesh-raptor, bandit/horde, or camp proof lanes by drift.
