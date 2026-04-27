# Andi handoff: Basecamp medical consumable readiness v0

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`
- just-closed prior packet: `doc/smart-zone-manager-v1-josef-playtest-followup-2026-04-26.md`

## Current state

`Smart Zone Manager v1 Josef playtest corrections` is closed. The landed code adds `LOOT_MANUALS` on the books tile while preserving `LOOT_BOOKS`, keeps gun magazines as `LOOT_MAGAZINES` with label `Basecamp weapon magazines`, adds full Basecamp-footprint `AUTO_EAT` / `AUTO_DRINK`, and keeps generated ignorable zones at `ignore_contents=false`.

Validation passed:

```sh
make -j4 TILES=1 tests
./tests/cata_test "basecamp_smart_zoning_places_expected_layout"
./tests/cata_test "[smart_zone]"
git diff --check -- src/clzones.cpp src/clzones.h tests/clzones_test.cpp
```

The focused Smart Zone test passed with 745 assertions in 1 test case; the `[smart_zone]` filter passed with 2847 assertions in 4 test cases. The GUI harness artifact attempt is not product proof because its captured `zoneszmgr-temp.json` contained only ordinary camp zones; the durable persistence proof is the deterministic zone-manager serialize/deserialize assertion in `tests/clzones_test.cpp`.

## Active implementation target

Read `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`, then inspect the camp locker/service code and tests. Desired narrow corrections:

- recognize at least `bandages` and `adhesive_bandages` as bounded medical readiness supplies
- let NPCs stock a small carried reserve from Basecamp locker/storage
- preserve already-carried bandages and adhesive bandages through locker refresh
- keep ammo/magazine/readiness and clothing/armor behavior unchanged
- avoid hoovering every medical/pharmacy item into camp locker policy

## Validation plan

Start with deterministic camp/locker tests for fresh pickup, carried preservation, cap/anti-hoarding behavior, a negative unrelated-item case, and no ammo/mag/readiness regression. Use one focused live/harness proof only if it answers a real product-path risk. No broad medical AI rewrite, no Smart Zone Manager reopen, no Lacapult, no release publication, and no user-data mutation.
