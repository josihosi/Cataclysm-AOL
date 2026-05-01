# CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

Status: ACTIVE / GREENLIT / ANTI-REDUNDANCY PACKAGE / SERVICE LIVE-STATE REUSE GREEN

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
- [x] Ranged weapon readiness still selects compatible magazines/ammo and uses existing reload behavior correctly. Current carried-magazine discovery defers to `Character::find_ammo()` / reload compatibility and regression coverage proves magazines installed in other carried guns are not stolen or double-counted.
- [x] Live camp locker armor/clothing scoring now passes the worker as fit context and defers encumbrance penalties to `item::get_avg_encumber()` instead of camp-local armor-portion averaging; no-context helper calls retain the prior fallback.
- [x] Live camp locker worn-slot candidate collection filters through `Character::can_wear(..., true)` before scoring/planning, so worker-specific direct-wearability rules reject unwearable armor/clothing while no-worker helper classification stays stable.
- [x] Live service collection now reuses `collect_camp_locker_live_state()` for the pre-service camp-state pass, keeping worker/locker item collection, candidate classification, planning, cleanup, ranged readiness, and medical readiness on one shared aggregation path.
- [x] Focused faction/basecamp tests pass for the current camp-locker API-reduction slices without widening the active lane.

## Targeted tests

- Existing camp locker classification/plan tests in `tests/faction_camp_test.cpp` or a narrower helper if available.
- Carried cleanup regression: worn/wielded items stay, kept readiness supplies stay, dumpable carried junk returns to camp storage.
- Magazine/ammo readiness regression: compatible loaded magazines are preferred, reload uses existing item reload mechanics, leftovers return safely.
- Zone boundary regression: `CAMP_STORAGE` and `NO_NPC_PICKUP` behavior remains respected.

## Cautions

This package should make camp locker thinner, not dumber. Existing APIs are the source of physical truth; camp locker policy is the camp judgment on top. If the policy really needs a custom exception, name it as policy rather than smuggling it back as fake item physics.
