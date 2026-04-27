# Andi handoff: Basecamp locker armor ranking + blocker removal packet v0

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

## Closed implementation checkpoint

Closed implementation: read `doc/basecamp-medical-consumable-readiness-v0-2026-04-26.md`, then inspect the camp locker/service code and tests. Landed narrow corrections:

- recognized the practical bandage family as bounded medical readiness supplies: `bandages`, `adhesive_bandages`, `medical_gauze`, and makeshift bandage variants
- let NPCs stock roughly 10 pieces of bandage-family supplies per NPC from Basecamp locker/storage
- preserved already-carried bandage-family stock through locker refresh and counted it toward the reserve cap
- kept ammo/magazine/readiness and clothing/armor behavior under the existing `[camp][locker]` suite
- avoided hoovering unrelated medical/pharmacy items (`aspirin` remains in storage)

## Validation plan

Validation passed: `git diff --check`; `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`; focused medical locker tests; `./tests/cata_test "[camp][locker]"` -> 2009 assertions in 67 test cases. No live/harness proof was added because deterministic proof exercises the real `service_camp_locker` path. Next active work is `Basecamp locker armor ranking + blocker removal packet v0`; start from `doc/basecamp-locker-armor-ranking-blocker-removal-packet-v0-2026-04-26.md` and do not hardcode RM13.
