# Smart Zone Manager v1 — Josef playtest follow-up packet (2026-04-26)

## Classification

REOPENED / ACTIVE FOLLOW-UP.

Josef confirmed on 2026-04-26 that the generated zones are broadly OK after checking them in Zone Manager. This follow-up narrowed the remaining corrections to manual-books coverage and full-storage auto-consume coverage; those corrections are now implemented and tested.

## Source

Josef's 2026-04-26 Smart Zone Manager playtest audio said the feature was promising. A later manual Zone Manager check initially confirmed the generated zones were mostly OK, but Josef's 2026-04-27 live test supersedes that closure: generated zones are still lumping onto a single tile / overlapping incorrectly. The previous manual-books and auto-consume corrections remain useful, but they no longer close the package.

## Scope when reopened

### 2026-04-27 live layout separation correction

- Fix the actual Smart Zone Manager planner so generated zones do not all lump onto one tile / overlap incorrectly in live use.
- Preserve intended overlaps only where the contract explicitly allows them, e.g. full-footprint `AUTO_EAT` / `AUTO_DRINK` over Basecamp storage with `ignore_contents == false`, and any deliberate nearby cluster overlaps that do not break sorting.
- Enforce intended separation from the v1 aux plan: fire-source `SOURCE_FIREWOOD` on the fire tile, adjacent `splintered`, nearby-but-distinct wood, readable food/drink/equipment/crafting niches, clothing/dirty support, rotten outside Basecamp, and a larger unsorted intake area where applicable.
- Add deterministic tests that assert zone geometry/separation and the overlap allowlist, not just zone ids/types/options.
- Prove the fix with clean live/UI evidence where possible; if the harness still cannot honestly prove it inside the attempt budget, write a Josef playtest package with the exact manual close recipe.

### Previous 2026-04-26 follow-up corrections

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
- Do not reopen unrelated locker/patrol/bandit work; the reopened evidence is specifically Smart Zone player-visible layout lumping / incorrect overlap.

## Success state

- [x] Smart Zone Manager adds `LOOT_MANUALS` coverage on/near the Basecamp books cluster without removing ordinary `LOOT_BOOKS` coverage.
- [x] Gun-magazine coverage remains `LOOT_MAGAZINES`, preferably with an unambiguous user-facing label such as "Basecamp weapon magazines" if label text is touched.
- [x] Auto-eat and auto-drink coverage spans the full Basecamp storage zone, with `ignore_contents` explicitly false so Basecamp sorting still sees the covered items.
- [x] Deterministic tests assert the actual zone ids/types and the `ignore_contents == false` option, not just label text.
- [x] Save inspection confirms generated zones remain saved/reopenable and do not crash or corrupt the camp layout through zone-manager serialize/deserialize proof.
- [ ] Live/player-visible generated zones no longer lump onto a single tile when the contract says they should be separate.
- [ ] Deterministic tests assert zone geometry/separation and the intended-overlap allowlist.
- [ ] The corrected layout respects the v1 aux-plan separation rules instead of relying on id/options-only proof.

## Testing notes

Previous implemented proof, now insufficient for live layout closure: `tests/clzones_test.cpp` asserts `LOOT_MANUALS`, preserved `LOOT_BOOKS`, preserved gun-magazine `LOOT_MAGAZINES` with `Basecamp weapon magazines`, full-footprint `AUTO_EAT` / `AUTO_DRINK`, and `ignore_contents == false` before and after zone-manager serialize/deserialize. Validation passed with `make -j4 TILES=1 tests`, `./tests/cata_test "basecamp_smart_zoning_places_expected_layout"` (745 assertions in 1 test case), `./tests/cata_test "[smart_zone]"` (2847 assertions in 4 test cases), and focused `git diff --check`. A GUI harness artifact capture remained inconclusive because the captured temporary zone file contained ordinary camp zones only, so the save/reopen claim rests on deterministic zone-manager serialization proof rather than that flaky artifact path.
