# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

---

## Current relevant evidence

### Smart Zone Manager v1

Current honest state:
- the one-off Smart Zone Manager v1 code is now real in the tree, not just aux-doc theater
- the Basecamp inventory-zone prompt path now exists in two honest places:
  - initial `CAMP_STORAGE` placement in Zone Manager
  - later position/stretch edits of that same Basecamp zone
- the current planner stamps the v1 niche/support packet in deterministic code, including:
  - one crafting niche
  - one food/drink niche
  - one equipment niche
  - support placement for clothing, dirty, rotten, unsorted, and blanket/quilt-on-beds
  - the corrected fire knot: fire tile `SOURCE_FIREWOOD`, adjacent `splintered`, nearby wood
- the rotten dump search is no longer limited to the first ring outside the Basecamp rectangle; it now searches outward until it finds an outdoor passable tile, so ordinary one-tile wall rings do not trap the rotten zone on indoor/wall junk
- the current deterministic proof now lives in `tests/clzones_test.cpp` and covers the active honest packet:
  - expected stamped layout on a representative indoor basecamp fixture
  - outdoor rotten placement beyond a simple wall ring
  - too-small-zone failure
  - non-destructive refusal when a required bed tile is already occupied by a non-basecamp zone
  - repeatability on the same layout
- current deterministic recheck on this Mac passed after `make -j4 tests`, via:
  - `./tests/cata_test "[zones][smart_zone][basecamp]"`
  - build log: `build_logs/smart_zone_build_20260409.log`
  - diff check: `build_logs/smart_zone_diffcheck_20260409.log`
  - test log: `build_logs/smart_zone_tests_20260409.log`

### Meaning

- the active question is no longer whether Smart Zone Manager v1 exists at all
- the active question is whether the remaining contract seams are honest enough and whether one proportional live packet can close the lane without reopening scope
- if a proposed next step starts inventing new zoning doctrine instead of finishing the current narrow packet, cut it

---

## Pending probes

### Active queue

1. **Contract seam audit:**
   - confirm the current anchor-selection story is honest about where the code is truly flag/category-first and where it still relies on narrow fallback heuristics
   - confirm the current built-in loot/custom zone reuse is still the narrowest sane v1 shape
2. **Live proof:**
   - prove Smart Zone Manager v1 on a realistic McWilliams basecamp setup
   - McWilliams still lacks a ready fire-source anchor, so the live packet may need a narrow harness/restaging step to place and activate a brazier or equivalent fire-source anchor before the crafting niche can be proved honestly
3. **Stop condition:**
   - once one honest live packet exists beside the deterministic proof, stop
   - do not turn this into another endless zone-edge-case collection run

### Meaning

- the missing evidence class is now live behavior, not another ceremonial deterministic rerun
- the remaining work should stay narrowly about finishing Smart Zone Manager v1, not drifting back into locker/basecamp archaeology

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Smart-zone deterministic check
- `./tests/cata_test "[zones][smart_zone][basecamp]"`

### Startup/load smoke for later live proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
