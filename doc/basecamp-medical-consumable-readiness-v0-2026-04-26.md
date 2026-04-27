# Basecamp medical consumable readiness v0 (2026-04-26)

Status: active / greenlit now.

## Normalized contract

**Title:** Basecamp medical consumable readiness v0

**Request kind:** Josef debug-note follow-up / parked implementation package

**Summary:** Josef put bandages in the `CAMP_LOCKER` zone and NPCs did not pick them up. Current code indicates this is policy, not player error: locker service classifies outfit/loadout items, preserves some already-carried bandages, but does not fetch fresh medical consumables from the locker zone. This packet adds a bounded medical-readiness layer without turning the camp locker into a pharmacy brain.

## Evidence this package is based on

- `src/basecamp.cpp::classify_camp_locker_item()` currently admits loadout/outfit gear such as armor, clothing slots, holsters, melee weapons, ranged weapons, ammo/mag support, and similar readiness gear.
- Drug/medical items such as `bandages` and `adhesive_bandages` do not currently classify as locker candidates for pickup.
- Existing carried bandages are kept by the carried-item preservation path, but that is not the same as fetching new bandages from camp storage.
- `adhesive_bandages` needs explicit treatment rather than relying on a narrow bandage-name predicate.

## Scope

1. Add a bounded **medical consumable readiness** classification for camp locker/service logic.
2. Start with obvious first-aid consumables:
   - `bandages`
   - `adhesive_bandages`
   - optionally closely related basic wound-care items if current item definitions make the category obvious and testable.
3. Let NPCs stock a small, sensible carried reserve from `CAMP_LOCKER` / relevant Basecamp storage when they are preparing for work or danger.
4. Preserve existing carried medical items rather than stripping them during outfit/loadout refresh.
5. Keep the rule count small and reviewer-readable: named helper/classification, small limits, explicit item IDs/categories, deterministic tests.

## Non-goals

- No full medical AI or injury triage rewrite.
- No automatic drug-use doctrine, painkiller policy, antibiotics logic, or NPC doctor behavior.
- No infinite hoarding of every `MEDICAL` tagged item.
- No reopening Locker Zone V3 clothing/weapon/ammo logic.
- No Smart Zone Manager layout work in this packet.
- No requirement that Josef playtest this before agent-side deterministic proof exists.

## Success state

- [ ] Camp locker/service logic recognizes at least `bandages` and `adhesive_bandages` as bounded medical readiness supplies when stocking NPCs.
- [ ] NPCs can pick up a small reserve from the relevant Basecamp/locker storage path without hoarding all medical items.
- [ ] Existing carried bandages and adhesive bandages are preserved through locker refresh instead of being discarded as clutter.
- [ ] Non-medical loadout behavior, ammo/magazine readiness, and clothing/armor selection remain unchanged.
- [ ] Deterministic tests cover fresh pickup, carried-item preservation, cap/anti-hoarding behavior, and a negative case for unrelated drugs/items.
- [ ] If a live/harness proof is practical, one Basecamp/locker probe shows an NPC can acquire the intended medical consumable from camp storage; otherwise the packet states plainly why deterministic proof is sufficient for the first slice.

## Suggested validation packet

Minimum implementation validation once greenlit:

1. Deterministic tests in the camp/locker family for:
   - `bandages` pickup from locker supply;
   - `adhesive_bandages` pickup from locker supply;
   - preservation of already carried bandages/adhesive bandages;
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