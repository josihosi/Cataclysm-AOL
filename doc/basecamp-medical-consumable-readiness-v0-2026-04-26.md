# Basecamp medical consumable readiness v0 (2026-04-26)

Status: closed / checkpointed.

## Normalized contract

**Title:** Basecamp medical consumable readiness v0

**Request kind:** Josef debug-note follow-up / parked implementation package

**Summary:** Josef put bandages in the `CAMP_LOCKER` zone and NPCs did not pick them up. Current code indicates this is policy, not player error: locker service classifies outfit/loadout items, preserves some already-carried bandages, but does not fetch fresh medical consumables from the locker zone. Josef later clarified that this should cover the practical bandage family, not just two exact ids: NPCs should strive to carry roughly 10 pieces/charges of bandage-like wound-care stock each, including obvious variants such as medical gauze. This packet adds a bounded medical-readiness layer without turning the camp locker into a pharmacy brain.

## Evidence this package is based on

- `src/basecamp.cpp::classify_camp_locker_item()` currently admits loadout/outfit gear such as armor, clothing slots, holsters, melee weapons, ranged weapons, ammo/mag support, and similar readiness gear.
- Drug/medical items such as `bandages`, `adhesive_bandages`, medical gauze, and other obvious bandage-family wound-care supplies do not currently classify as locker candidates for pickup.
- Existing carried bandages are kept by the carried-item preservation path, but that is not the same as fetching new bandages from camp storage.
- `adhesive_bandages` and medical gauze need explicit coverage without falling back to a sloppy all-`MEDICAL` hoover.

## Scope

1. Add a bounded **medical consumable readiness** classification for camp locker/service logic.
2. Cover the practical bandage-family first-aid consumables:
   - `bandages`
   - `adhesive_bandages`
   - medical gauze / other obvious bandage-like wound-care stock if current item definitions make the category explicit and testable.
3. Let NPCs stock a small, sensible carried reserve from `CAMP_LOCKER` / relevant Basecamp storage when they are preparing for work or danger: target roughly 10 pieces/charges of bandage-family supplies per NPC, counting already carried stock toward the cap.
4. Preserve existing carried medical items rather than stripping them during outfit/loadout refresh.
5. Keep the rule count small and reviewer-readable: named helper/classification, small limits, explicit item IDs/categories, deterministic tests.

## Non-goals

- No full medical AI or injury triage rewrite.
- No automatic drug-use doctrine, painkiller policy, antibiotics logic, or NPC doctor behavior.
- No infinite hoarding of every `MEDICAL` tagged item; medical gauze/bandage-family coverage is allowed, painkillers/antibiotics/drugs remain outside this slice unless separately greenlit.
- No reopening Locker Zone V3 clothing/weapon/ammo logic.
- No Smart Zone Manager layout work in this packet.
- No requirement that Josef playtest this before agent-side deterministic proof exists.

## Success state

- [x] Camp locker/service logic recognizes the practical bandage family, including at least `bandages`, `adhesive_bandages`, and medical gauze when item definitions expose it cleanly, as bounded medical readiness supplies when stocking NPCs.
- [x] NPCs can pick up roughly 10 pieces/charges of bandage-family stock from the relevant Basecamp/locker storage path without hoarding all medical items.
- [x] Existing carried bandage-family stock counts toward the reserve and is preserved through locker refresh instead of being discarded as clutter.
- [x] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [x] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [x] Live/harness proof is not required for this first slice; deterministic camp/locker tests exercise the actual service path and the focused `[camp][locker]` pass covers readiness behavior.

## Suggested validation packet

Minimum implementation validation once greenlit:

1. Deterministic tests in the camp/locker family for:
   - `bandages` pickup from locker supply;
   - `adhesive_bandages` pickup from locker supply;
   - medical gauze / obvious bandage-family pickup from locker supply when represented by current item definitions;
   - preservation of already carried bandage-family stock;
   - roughly 10-piece/charge reserve cap across already-carried plus newly picked stock;
   - refusal to sweep unrelated medical/pharmacy items into the same rule by accident;
   - unchanged ammo/mag/readiness behavior.
2. Narrow compile/test:
   - touched-object compile for `src/basecamp.cpp` if implementation stays there;
   - `./tests/cata_test "[camp][locker]"` or the narrowest existing locker readiness filter.
3. Optional live/harness proof:
   - one small Basecamp fixture with bandages in the locker zone and a worker/follower needing readiness stock;
   - artifact or save inspection showing the item moved for the intended reason.

## Handoff note for Andi

Keep this separate from the bandit live-signal packet unless Josef explicitly bundles them. The medical readiness bug is real, but it is not the same failure chain as remote bandit camp ownership and signal dispatch. Two sausages, two plates.

## Closure evidence

Implemented in `src/basecamp.cpp` / `src/basecamp.h` with tests in `tests/faction_camp_test.cpp`. The readiness helper recognizes `bandages`, `adhesive_bandages`, `medical_gauze`, and makeshift bandage variants; already-carried readiness stock counts toward a 10-piece reserve; unrelated pharmacy items such as `aspirin` are not swept into the rule.

Evidence:

- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused medical locker tests: fresh pickup, carried-counted cap/anti-hoarding, unrelated-item negative
- `./tests/cata_test "[camp][locker]"` -> 2009 assertions in 67 test cases

No live/harness Basecamp probe was added because the deterministic tests drive the real `service_camp_locker` path directly and there is no remaining UI/harness-only product-path risk in this bounded slice.
