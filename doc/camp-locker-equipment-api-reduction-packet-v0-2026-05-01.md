# CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

Status: ACTIVE / GREENLIT / ANTI-REDUNDANCY PACKAGE / BODY-PART OUTER-LAYER API GREEN

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

## Summary

Trim the camp locker implementation where it re-describes item, clothing, ammo, magazine, reload, and zone truths that existing game APIs already know. The camp-specific policy layer stays; the target is to make that policy sit on top of the engine's item/wear/reload/zone behavior instead of quietly becoming a second wardrobe and readiness engine.

## Scope

- Audit `camp_locker_slot`, item classification, scoring, carried cleanup, ammo/magazine readiness, and zone item collection against existing item, wearability, coverage, reload, and zone APIs.
- Replace bespoke checks where an existing API gives the same truth with lower drift risk.
- Keep camp locker policy decisions explicit: enabled slots, bulletproof preference, weather-sensitive preference, readiness supplies, and camp-storage boundaries.
- Preserve worker downtime behavior and current locker safety constraints.
- Add or update focused tests around any seam moved from custom classification into existing APIs.

## Non-goals

- Do not remove the camp locker feature or its camp-specific policy layer.
- Do not redesign basecamp missions, Smart Zones, or general NPC equipment selection.
- Do not retune outfit preferences beyond what is required to preserve behavior while reducing duplicate ontology.
- Do not touch current zombie rider work in the same implementation blob.

## Success state

- [ ] A short audit note or commit message identifies which camp locker checks now defer to existing item/wear/reload/zone APIs.
- [ ] Camp locker candidate classification and upgrade selection remain green for clothing, armor, bags, melee/ranged weapons, ammo, magazines, and kept medical/readiness items.
- [x] Carried cleanup still dumps only safe non-kept baggage and preserves kept ammo/magazine/medical/insert readiness items. Current worker/equipped item enumeration and carried-cleanup summaries defer to the existing visitable `items_with()` API instead of local `visit_items()` collection loops.
- [x] Camp locker medical readiness still uses existing heal use-action metadata, and the direct-use lookup now goes through `item::get_usable_item("heal")` / `item::get_use("heal")` instead of raw type use-function lookup.
- [x] Ranged weapon readiness still selects compatible magazines/ammo and uses existing reload behavior correctly. Current carried-magazine discovery defers to `Character::find_ammo()` / reload compatibility and regression coverage proves magazines installed in other carried guns are not stolen or double-counted.
- [x] Live camp locker armor/clothing scoring now passes the worker as fit context and defers encumbrance penalties to `item::get_avg_encumber()` instead of camp-local armor-portion averaging; no-context helper calls retain the prior fallback.
- [x] Live camp locker worn-slot candidate collection filters through `Character::can_wear(..., true)` before scoring/planning, so worker-specific direct-wearability rules reject unwearable armor/clothing while no-worker helper classification stays stable.
- [x] Live camp locker weapon-slot candidate collection filters through `Character::can_wield()` before scoring/planning, so integrated/unwieldable weapon items remain classified by item type but are rejected from worker-specific live service candidates.
- [x] Camp locker weapon/armor classification now defers to existing `item::is_gun()` and `item::is_armor()` boundaries instead of narrower camp-local predicates/raw armor-slot lookup, while camp slot policy remains explicit.
- [x] Camp locker outerwear/outer-garment checks now defer outer-layer truth to existing armor layer data (`item::has_layer({ layer_level::OUTER })` and the body-part-specific `item::has_layer(..., bodypart_id)` overload) instead of treating the raw `OUTER` flag or global layer data plus separate coverage checks as parallel outerwear ontology; weather and slot policy remains explicit.
- [x] Camp locker worn-slot equip application now defers the final wearability decision to existing `Character::wear_item()` instead of doing a camp-local duplicate `can_wear()` precheck first; candidate filtering and slot policy remain explicit.
- [x] Live camp locker weapon scoring now defers to `Character::evaluate_weapon(..., true)` when a worker fit context is present, while no-context helper calls keep the prior fallback scoring.
- [x] Worker-context camp locker ammo readiness now defers reload viability to `Character::can_reload()` instead of only `item::can_reload_with()`, so engine-owned constraints such as ammo-belt linkages gate whether locker ammo can ready an item.
- [x] Compatible magazine preference now defers total-capacity ranking to existing item ammo-state APIs (`item::ammo_remaining()` plus `item::remaining_ammo_capacity()`) instead of camp-local `ammo_data()` / default-ammo type lookup plumbing.
- [x] Ranged readiness/reload selection now lets existing `item::can_reload_with()` / `Character::can_reload()` own reloadability before camp policy moves locker supplies, instead of gating candidate targets with camp-local `remaining_ammo_capacity()` prechecks.
- [x] Managed ranged-readiness recognition now asks existing `item::is_gun()` directly instead of re-entering camp locker classification, while enabled-slot policy stays explicit.
- [x] Ranged readiness ready/loaded checks now use existing `item::has_ammo()` instead of local `ammo_remaining() > 0` / `<= 0` predicates, while camp policy still decides which magazines and reload supplies to move.
- [x] Carried cleanup armor-insert preservation now asks existing ablative carrier pockets whether they can contain an item instead of using raw `CANT_WEAR` as insert ontology; ordinary carried armor still dumps through camp-storage cleanup.
- [x] Live service collection now reuses `collect_camp_locker_live_state()` for the pre-service camp-state pass and post-service summary, keeping worker/locker item collection, candidate classification, planning, cleanup, ranged readiness, and medical readiness on one shared aggregation path.
- [x] Camp storage and camp locker zone tile collection now share one thin `zone_manager::get_near()` adapter plus deterministic sorting, keeping explicit camp zone policy while removing duplicated local zone lookup boilerplate.
- [x] Focused faction/basecamp tests pass for the current camp-locker API-reduction slices without widening the active lane.

## Targeted tests

- Existing camp locker classification/plan tests in `tests/faction_camp_test.cpp` or a narrower helper if available.
- Carried cleanup regression: worn/wielded items stay, kept readiness supplies stay, dumpable carried junk returns to camp storage.
- Magazine/ammo readiness regression: compatible loaded magazines are preferred, reload uses existing item reload mechanics, leftovers return safely.
- Zone boundary regression: `CAMP_STORAGE` and `NO_NPC_PICKUP` behavior remains respected.

## Cautions

This package should make camp locker thinner, not dumber. Existing APIs are the source of physical truth; camp locker policy is the camp judgment on top. If the policy really needs a custom exception, name it as policy rather than smuggling it back as fake item physics.
