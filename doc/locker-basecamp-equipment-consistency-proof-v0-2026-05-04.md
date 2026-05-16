# Locker/basecamp equipment consistency proof v0 — 2026-05-04

Lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 10.

## Claim

Basecamp locker service no longer leaves NPC workers carrying loose orphan ammo or magazines after the retained firearm loadout no longer supports them, and better-condition bags can replace damaged bags without losing container contents silently.

## Source / behavior checkpoint

Changed source/tests:

- `src/basecamp.cpp`: adds orphan supply cleanup state/signature/logging for standalone carried magazines and ammo; compatibility is checked against retained firearms/magazines via existing reload/ammo APIs; cleanup stores orphan supplies to the camp cleanup/storage drop tile. Embedded supply inside carried guns, tools, or magazines is ignored.
- `src/basecamp.cpp`: state-dirty and no-op/service logging now include `orphan=[...]`, so orphan supply cleanup can wake/drive locker service even when no slot upgrade is planned.
- `tests/faction_camp_test.cpp`: covers orphan magazine/ammo cleanup after a ranged-weapon locker replacement, compatible magazine/ammo retention when the matching firearm remains carried, carried-junk cleanup expectations for loose ammo/mags, and damaged daypack replacement with kept/dumped contents accounted for.

## Evidence

Build/test gates after current edits:

```text
make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0
git diff --check
./tests/cata_test "[camp][locker]" --reporter compact
```

Results:

```text
make gate: exit 0
[camp][locker]: Passed all 80 test cases with 2181 assertions.
```

Focused fixture rows inside the credited `[camp][locker]` pass:

```text
camp_locker_service_drops_orphaned_carried_ammo_and_magazines
camp_locker_service_keeps_supply_for_retained_firearm
camp_locker_service_swaps_in_better_condition_equivalent_bags
camp_locker_service_dumps_carried_junk_outside_curated_locker_stock
camp_locker_ranged_readiness_defers_worker_reload_rules
```

## Boundary

This closes Slice 10 for deterministic/basecamp-fixture equipment consistency. It does not claim broader outfit-design changes, new NPC combat loadout taste, or live human playfeel beyond the locker/basecamp service path covered by the focused fixture suite.
