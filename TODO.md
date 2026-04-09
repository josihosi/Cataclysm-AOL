# TODO

Short execution queue only.

Remove finished items when they are done.
Do not turn this file into a museum of crossed-off errands.
If the queue below stops matching `Plan.md`, fix this file.

## Now

Current delivery target: Smart Zone Manager v1.

Primary auxiliary:
- `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md`

Current slice: **Smart Zone Manager v1 — one-off basecamp auto-layout**
1. land the one-off trigger on Basecamp inventory zone placement/stretch:
  - ask once whether to run Smart Zoning now
  - if yes, stamp the discussed zones once and stop
  - later manual edits belong to the normal Zone Manager
2. preserve the discussed layout contract without widening scope:
  - crafting, food/drink, and equipment niches
  - support zones: clothing, dirty, rotten, unsorted, blanket/quilt-on-beds
  - custom-string zones stay blunt: `splintered`, `dirty`, `rotten`
  - both the fire-source tile and the adjacent `splintered` tile are covered by `SOURCE_FIREWOOD`
3. handle obvious failure/edge paths cleanly:
  - if the Basecamp inventory zone is too small, error out cleanly instead of placing garbage
  - keep placement deterministic and non-destructive by default
  - do not break the game chasing cleverness
4. keep live proof honest without regressing into testing madness:
  - McWilliams currently lacks a ready fireplace, so the live proof may need a narrow harness/restaging step to place/activate a brazier or equivalent fire-source anchor
  - prove the feature on a realistic live setup, then stop

Still true:
- ordinary harness footing should stay on `McWilliams` / `Zoraida Vick`, not drift back to the older default save
- the debug pass is packetized; use the auxiliary doc instead of rebuilding the note pile from chat memory
- hackathon-reserved — do not touch before the event:
  - chat interface over dialogue branches
  - ambient-trigger reaction lane / tiny ambient-trigger NPC model
