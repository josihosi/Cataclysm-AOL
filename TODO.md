# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Active: `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0`.

`CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0` is closed/checkpointed green v0 after the first targeted anti-redundancy refactor. Preserve its caveat: the named `targeted_live_plan_adapter` dispatch reduces inline live-planner exceptions, while stalker-specific no-omniscience, quiet-side/cutoff, light/focus, cooldown, repeated-strike, and injured-retreat judgment remains custom and explicit.

Current narrow slice: camp-locker API-reduction audit/refactor, after the coverage-helper, zone-boundary, medical-readiness, carried ranged-readiness, average-coverage, worker-item collection, direct subpart coverage, item encumbrance, worker-wearability, service live-state reuse, weapon-wieldability, ranged-classification, armor-classification, weapon-scoring, worker-context reload-readiness, reload-supply ammo-state, magazine-capacity, and reloadability-gate API reductions.

1. Continue auditing remaining local camp locker ontology: scoring/readiness edges against existing item, wearability, reload, and zone APIs; carried-cleanup item enumeration now uses `items_with()`, ranged-weapon classification now uses `item::is_gun()`, armor classification now uses `item::is_armor()`, live weapon scoring now uses `Character::evaluate_weapon(..., true)`, worker-context ammo readiness now uses `Character::can_reload()`, reload-supply selection now uses `item::ammo_remaining()`, magazine preference now uses `item::ammo_capacity()`, ranged readiness/reload gates now use existing reloadability APIs instead of camp-local capacity prechecks, and kept/dump policy remains camp-specific.
2. Identify the next smallest honest refactor that preserves camp policy: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, and camp-storage boundaries. The `NO_NPC_PICKUP` zone collection boundary, heal-action medical readiness recognition, direct subpart coverage API use, worker-fit encumbrance scoring, `Character::can_wear()` worn-candidate filtering, `Character::can_wield()` weapon-candidate filtering, `item::is_gun()` ranged classification, `item::is_armor()` armor classification, worker-context `Character::evaluate_weapon()` weapon scoring, worker-context `Character::can_reload()` ammo readiness, reload-supply `item::ammo_remaining()` handling, magazine-capacity `item::ammo_capacity()` handling, reloadability-gate handling, and service live-state reuse are green; do not repeat them unless later changes touch those seams again.
3. After each code change, run the narrow focused faction/basecamp/camp-locker gate named by the audit; do not broaden into basecamp mission redesign or Smart Zone work.
4. Keep unresolved quality feedback identifiers visible for the next checkpoint: `basecamp.h:116`, `basecamp.h:122`, `basecamp.h:169`.
5. Do not start bandit-signal, eigenspectre, flesh-raptor, zombie-rider, or product-tuning work by drift.
