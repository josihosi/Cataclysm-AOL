# Smart Zone Manager v1 — Josef playtest follow-up packet (2026-04-26)

## Classification

CLOSED / MOVED DOWNSTREAM.

Josef confirmed on 2026-04-26 that the generated zones are broadly OK after checking them in Zone Manager. This follow-up narrowed the remaining corrections to manual-books coverage and full-storage auto-consume coverage; those corrections are now implemented and tested.

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

- [x] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [x] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [x] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [x] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [x] Save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout through zone-manager serialize/deserialize proof.

## Testing notes

Implemented proof: `tests/clzones_test.cpp` now asserts `LOOT_MANUALS`, preserved `LOOT_BOOKS`, preserved gun-magazine `LOOT_MAGAZINES` with `Basecamp weapon magazines`, full-footprint `AUTO_EAT` / `AUTO_DRINK`, and `ignore_contents == false` before and after zone-manager serialize/deserialize. Validation passed with `make -j4 TILES=1 tests`, `./tests/cata_test "basecamp_smart_zoning_places_expected_layout"` (745 assertions in 1 test case), `./tests/cata_test "[smart_zone]"` (2847 assertions in 4 test cases), and focused `git diff --check`. A GUI harness artifact capture remained inconclusive because the captured temporary zone file contained ordinary camp zones only, so the save/reopen claim rests on deterministic zone-manager serialization proof rather than that flaky artifact path.
