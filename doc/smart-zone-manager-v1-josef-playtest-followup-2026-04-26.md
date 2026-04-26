# Smart Zone Manager v1 — Josef playtest follow-up packet (2026-04-26)

## Classification

PARKED / COLLECTED DEBUG FOLLOW-UP.

This is intentionally not the active Andi lane while Lacapult backend installer work is active. Keep it available for later greenlight without switching Andi off Lacapult prematurely.

## Source

Josef's 2026-04-26 Smart Zone Manager playtest audio said the feature is already promising, but several generated-zone details need correction or proof before calling the layout good.

## Scope when reopened

- Re-check fire-source placement and zone types:
  - fireplace/brazier tile should be covered by `SOURCE_FIREWOOD`
  - adjacent `splintered` tile should also be covered by `SOURCE_FIREWOOD`
  - clarify how the separate kindling zone interacts with this so the UI/result is not contradictory
- Improve layout spreading for bulky storage categories when Basecamp space allows:
  - wood should have its own square/zone
  - tools should have its own square/zone
  - spare parts should have its own square/zone
  - containers should have its own square/zone
  - books and readable magazines may share if needed
  - add one additional generated zone for `LOOT_MANUALS` on the same tile/cluster as basecamp books, because real skill/manual books route through `LOOT_MANUALS` rather than `LOOT_BOOKS`
  - keep the existing gun-magazine `LOOT_MAGAZINES` coverage; do not remove the weapon-magazine zone near ammo
  - chemicals and drugs sharing is acceptable
- Make the unsorted drop zone larger than the current 2x2 result; target a simple rectangular footprint such as 2x4 unless local constraints require another shape.
- Verify generated zones use the correct C:DDA zone types, not only convincing labels. Example: the basecamp gun/ammo zone must actually be the appropriate gun/ammo sorting zone.
- Add `AUTO_DRINK` and auto-eat coverage over the full Basecamp storage zone, because that zone covers the whole basecamp and contains the smart zones.

## Non-goals

- Do not turn this into a broad basecamp automation rewrite.
- Do not reopen unrelated locker/patrol/bandit work.
- Do not chase cosmetic zone-label perfection before proving actual zone ids/types and sorting behavior.
- Do not interrupt the active Lacapult backend installer lane unless Josef explicitly reprioritizes.

## Success state

- [ ] Deterministic proof shows fireplace and adjacent `splintered` tile both carry the intended firewood-source behavior, with kindling interaction explained.
- [ ] Deterministic or harness proof shows bulky categories spread across separate tiles when sufficient Basecamp storage space exists.
- [ ] Unsorted zone footprint is larger and still fits inside Basecamp storage.
- [ ] Zone-id/type proof confirms the generated gun/ammo/clothing/food/drink/etc. zones are actually the intended C:DDA sorting zones.
- [ ] Auto-eat and auto-drink coverage spans the full Basecamp storage zone.
- [ ] Live or harness smoke confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout.

## Testing notes

Start with narrow deterministic zone-layout tests where possible. Follow with one focused live/harness save proof only after the static/fixture shape is honest. Josef playtest is product confidence, not a substitute for proving the zone ids.
