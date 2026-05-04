# Monsterbone spear proof v0 — 2026-05-04

Active lane: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, Slice 11.

Status: GREEN / JSON + distribution checkpoint.

## Contract covered

- Adds `monsterbone_spear` / `monsterbone spear` as a cannibal ritual/status spear.
- Keeps it strong but bounded: durable reach spear, stronger than ordinary wood/steel spear in the narrow test sanity check, still below `qiang` stab damage and not a late-game artifact weapon.
- Surfaces rare camp copies through a camp-local relic item group with `count: [ 1, 2 ]` and a cannibal-camp map placement.
- Adds only bounded elite NPC access: the cannibal camp leader now uses `NC_CANNIBAL_ELITE_weapon`, where `monsterbone_spear` is a 25-probability option; ordinary hunter and butcher weapon groups do not contain it.
- Preserves non-goals: no broad cannibal lore rewrite, no monsterbone crafting economy, no common/every-cannibal weapon flood, no shakedown/diplomacy change.

## Changed files

- `data/json/items/melee/spears_and_polearms.json`
  - new `monsterbone_spear` item with huge-monster-bone / monster-meat madness description.
- `data/json/itemgroups/cannibal_camp.json`
  - new `cannibal_camp_monsterbone_relic` collection, `monsterbone_spear` count `[ 1, 2 ]`.
- `data/json/mapgen/hells_raiders/cannibal_camp.json`
  - places the camp-local relic group in the bone area at 100% chance.
- `data/json/itemgroups/SUS/lodge.json`
  - adds `monsterbone_spear` to `cannibal_weapons` at low probability `2`.
- `data/json/npcs/cannibals/classes.json`
  - adds `NC_CANNIBAL_ELITE_weapon`; leader uses it, hunter/butcher groups remain without the spear.
- `tests/item_group_test.cpp`
  - adds stat/distribution guard `monsterbone_spear_is_bounded_elite_cannibal_gear`.

## Evidence

- JSON/style: `json_formatter.cgi` accepted the five changed JSON files; `git diff --check` passed.
- JSON extractor sanity:
  - `monsterbone_spear_damage={'bash': 10, 'stab': 32}` with flags `DURABLE_MELEE,SPEAR,REACH_ATTACK,NONCONDUCTIVE,SHEATH_SPEAR`.
  - `cannibal_camp_monsterbone_relic` contains `monsterbone_spear` with `count: [1, 2]`.
  - `cannibal_weapons` contains `monsterbone_spear` with probability `2`.
  - `NC_CANNIBAL_LEADER.weapon_override == NC_CANNIBAL_ELITE_weapon`.
  - camp mapgen places `cannibal_camp_monsterbone_relic` at `x=[18,22]`, `y=[10,12]`, `chance=100`.
- Build/test gates:
  - `make -j4 tests/item_group_test.o tests LINTJSON=0 ASTYLE=0` passed.
  - `./tests/cata_test "[cannibal][item_group]" --reporter compact` passed `1` test case / `14` assertions.
  - `./tests/cata_test "[item_group]" --reporter compact` passed `8` test cases / `662` assertions.

## Boundaries

This is a JSON/loadout/distribution checkpoint, not a live cannibal raid/contact proof. It does not claim new cannibal AI behavior, full camp playfeel, natural discovery frequency beyond the configured camp/group placement, or any crafting economy.
