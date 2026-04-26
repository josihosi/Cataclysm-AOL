# Smart Zone Manager v1 — Josef playtest follow-up packet (2026-04-26)

## Classification

GREENLIT / QUEUED DEBUG FOLLOW-UP.

Josef confirmed on 2026-04-26 that the generated zones are broadly OK after checking them in Zone Manager. This follow-up is now narrowed to the remaining corrections below and queued behind the active test-vs-game audit / bandit wiring stack.

## Source

Josef's 2026-04-26 Smart Zone Manager playtest audio said the feature is already promising. A later manual Zone Manager check confirmed the existing generated zones are mostly OK, but the layout still needs a manual-books zone and full-storage auto-consume coverage.

## Scope when reopened

- Add one additional generated zone for `LOOT_MANUALS` on the same tile/cluster as basecamp books, because real skill/manual books and readable instructional magazines route through `LOOT_MANUALS` rather than only `LOOT_BOOKS`.
- Keep the existing `LOOT_BOOKS` zone for ordinary books/book-like reading material.
- Keep `LOOT_MAGAZINES` for gun magazines, especially the weapon-magazine zone near ammo; do not treat it as the readable magazine/manual bucket. If the user-facing label says only "magazines" in a way that can be confused with readable magazines, rename it to something like "Basecamp weapon magazines".
- Add `AUTO_DRINK` and `AUTO_EAT` coverage over the full Basecamp storage zone, because that zone covers the whole basecamp and contains the smart zones.
  - For both auto-consume zones, the zone option "Ignore items in this area when sorting?" / `ignore_contents` must be **NO / false**. If it is set to yes, the auto-eat/drink zone would make the whole Basecamp storage footprint ignored by loot sorting, defeating the Smart Zone Manager layout.
- Preserve the existing checked-good zones unless implementation evidence shows a real type/id mismatch. Do not re-open fire-source, bulky spreading, or unsorted-size work just because the earlier draft listed it.

## Non-goals

- Do not turn this into a broad basecamp automation rewrite.
- Do not reopen unrelated locker/patrol/bandit work.
- Do not chase cosmetic zone-label perfection before proving actual zone ids/types and sorting behavior.
- Do not reopen the already-checked zone layout work beyond the `LOOT_MANUALS`, label clarity, and auto-eat/auto-drink corrections unless fresh evidence proves it is wrong.

## Success state

- [ ] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [ ] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [ ] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [ ] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [ ] Harness or save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout.

## Testing notes

Start with narrow deterministic zone-layout tests for `LOOT_MANUALS`, preserved `LOOT_BOOKS`, preserved gun-magazine `LOOT_MAGAZINES`, and auto-eat/auto-drink `ignore_contents == false`, matching the manual UI answer **No** to "Ignore items in this area when sorting?" Follow with one focused harness/save proof only after the static/fixture shape is honest. Josef playtest is product confidence, not a substitute for proving the zone ids.
