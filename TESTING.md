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
- the one-off Smart Zone Manager v1 code is real in the tree, not aux-doc theater
- the Basecamp inventory-zone prompt path exists in both honest UI seams:
  - initial `CAMP_STORAGE` placement in Zone Manager
  - later position/stretch edits of that same Basecamp zone
- the current planner stamps the intended v1 niche/support packet in deterministic code, including:
  - one crafting niche
  - one food/drink niche
  - one equipment niche
  - support placement for clothing, dirty, rotten, unsorted, and blanket/quilt-on-beds
  - the corrected fire knot: fire tile `SOURCE_FIREWOOD`, adjacent `splintered`, nearby wood
- the anchor/reuse audit is now closed honestly:
  - fire/food/equipment anchors are flag-first where the map exposes real signals
  - clothing storage and bed support still rely on small explicit id allowlists where the map does not expose a clean category signal
  - ordinary floor fallback stays in place instead of clever failure
  - built-in loot/custom zone machinery remains the default packet, with only `splintered`, `dirty`, `rotten`, `blanket`, and `quilt` as deliberate custom filters
- the rotten dump search is no longer limited to the first ring outside the Basecamp rectangle; it searches outward until it finds an outdoor passable tile, so ordinary one-tile wall rings do not trap the rotten zone on indoor/wall junk
- the deterministic proof lives in `tests/clzones_test.cpp` and still covers the active honest packet:
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
- stale-binary suspicion was real on the old McWilliams harness start, so the tiles binary was rebuilt before live proof:
  - tiles rebuild log: `build_logs/smart_zone_tiles_20260409.log`
  - follow-up diff check log: `build_logs/smart_zone_diffcheck_20260409_2.log`
- proportional live proof now exists on the rebuilt current binary at `.userdata/dev-harness/harness_runs/20260409_132407/`:
  - prepared-save seam: reinstall the McWilliams fixture into `dev-harness`, clear existing zone files, then run `tools/openclaw_harness/scenarios/smart_zone.live_probe_mcw_prepped.json`
  - live restage: spawn/wield/apply a brazier, deploy it east, pass one turn, open Zone Manager, filter zone type to `Basecamp: Storage`, place the zone, accept the Smart Zone Manager prompt, save, and reopen Zone Manager
  - screen evidence: the prompt fired on the real UI path, the stamped layout appeared in Zone Manager, and the saved/reopened zone list showed `Basecamp: Storage` plus the smart-zoned niche/support entries
  - artifacts/logs: no `llm_intent.log` packet is expected here; the live proof is a screen/UI packet backed by harness screenshots rather than LLM artifacts

### Meaning

- Smart Zone Manager v1 is no longer missing a real live packet
- the lane is good enough for the current pre-freeze goal and should stay closed unless code/runtime evidence breaks it
- the next action is not more smart-zone archaeology; it is Josef choosing the next lane

---

## Pending probes

None for Smart Zone Manager v1 right now.

Do not rerun the live packet as ritual.
Only reopen this if:
- code affecting smart zoning changes
- a reviewer disputes the prepared-save seam or live packet
- runtime evidence disproves one of the current claims

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

### Current smart-zone live probe
- `python3 tools/openclaw_harness/startup_harness.py install-fixture mcwilliams_live_debug_2026-04-07 --profile dev-harness --fixture-profile live-debug --replace`
- clear the installed McWilliams zone files in `.userdata/dev-harness/save/McWilliams/`
- `python3 tools/openclaw_harness/startup_harness.py probe smart_zone.live_probe_mcw_prepped`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
