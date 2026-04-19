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

### Combat-oriented locker policy

Current honest state:
- the first five combat-policy code slices are now landed narrowly on the current tree:
  - `camp_locker_slot`, `all_camp_locker_slots()`, and the persisted locker-policy surface now expose explicit `gloves`, `mask`, `belt`, and `holster` slots
  - `classify_camp_locker_item()` now routes representative combat support gear into those slots instead of dropping hip holsters on the floor or letting waist gear disappear into generic clothing logic
  - the locker policy surface now also has a persisted `Prefer bulletproof gear` toggle, and body-armor / helmet scoring adds extra bullet-resistance weight when that control is enabled
  - body-armor scoring now also counts loaded ablative plates as real value, and same-type ballistic vests can replace damaged-insert variants when their actual armor state is better
  - protective full-body suits in the pants lane now suppress missing-current shirt filler when the suit itself is the better combat/protective packet, so survivor-style suits stop inviting junk T-shirts underneath them just because the shirt lane is empty
  - that same protective-suit seam now also handles the first direct currently-worn comparison: when the suit wins the pants-lane upgrade honestly, a weaker current shirt can be demoted into duplicate cleanup instead of being layered underneath the new suit
  - deterministic coverage now checks bulletproof-toggle persistence, the scoring-gap shift toward more ballistic armor and helmets, loaded-vs-empty ballistic vest scoring, damaged insert scoring, same-type healthy-plate replacement behavior, the missing-shirt filler guard, and the new current-shirt displacement rule
- narrow deterministic validation passed on the current tree after the latest direct-current-shirt slice landed:
  - `make -j4 tests`
  - `./tests/cata_test "camp_locker_loadout_planning"`
  - `./tests/cata_test "camp_locker_service_keeps_armored_full-body_utility_suits_without_filling_missing_shirts_from_junk"`
  - `./tests/cata_test "camp_locker_service_uses_armored_full-body_utility_suits_over_weaker_current_shirts"`
- the filtered locker suite still covers the previously suspicious safety seams these slices could have broken, including great-helm classification, holster-vs-pants cleanup behavior, weird-garment preservation, weather-sensitive outerwear/legwear handling, and full-body suit protection logic
- the next missing evidence is no longer the first current-outfit comparison at all; it is the next direct upper-body tradeoff slice beyond weak shirts, most likely vest/body-armor comparisons when a clearly superior full-body battle/protective suit is available

### Recently closed, do not casually reopen

- Organic bulletin-board speech polish is now reclosed on both deterministic and live footing:
  - `make -j4 tests` plus `./tests/cata_test "[camp][basecamp_ai]"` passed for the widened organic status parsing and cleaned spoken bark
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - Robbie's same-packet follower crosstalk stays separate routing noise, not a reason to reopen the closed machine-speech seam
- Locker surface/control is also reclosed on both deterministic and live footing:
  - deterministic proof still covers locker policy persistence and sorting skip behavior on `CAMP_LOCKER`
  - live run `.userdata/dev-harness/harness_runs/20260419_141422/` created `Basecamp: Locker`, renamed it to `Probe Locker`, used the single-`Esc` -> save-prompt -> `Y` closeout, and reopened Zone Manager with `Probe Locker` still present
  - no type-mismatch popup or related stderr/debug failure surfaced on that live packet

### Meaning

- the missing evidence for the active lane is not another live board or locker rerun
- the next honest move is a narrow combat-policy implementation slice plus deterministic coverage for the next current-upper-body tradeoff beyond weak shirts, not a generic rerun of the now-landed direct-comparison seam

---

## Pending probes

- none yet for the next follow-up slice
- if the next combat-policy step expands direct current-outfit comparison into vest/body-armor tradeoffs, start again with narrow deterministic locker tests before asking for any live locker-service proof

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
