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

### Locker Zone V1 / V2 baseline + V3 deterministic slice
Latest relevant agent-side locker packet:
- committed HEAD `1381284982`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `171 assertions in 11 test cases`
- the targeted suite still covers the bundled locker baseline from V1/V2 and now also proves the landed V3 temperature slices:
  - locker policy serialization / save-load
  - representative locker item classification
  - locker-zone candidate gathering and reservation filtering
  - locker loadout planning and disabled-category behavior
  - service / equip / upgrade / return behavior
  - one-worker-at-a-time queue sequencing plus dirty-trigger follow-through
  - transient assigned-camp downtime rehydration
  - `camp_locker_service_readies_ranged_loadouts_from_locker_supply`
    - managed ranged loadout starts empty
    - locker service counts the weapon’s inserted magazine as part of the two-magazine readiness budget
    - locker supply contributes only the additional compatible magazines needed to reach that cap
    - locker-zone ammo fills those selected magazines and reloads the supported weapon
    - leftover locker magazine count and ammo depletion stay deterministic
  - V3 deterministic outerwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers a warmer outerwear upgrade (`jacket_light -> coat_winter`)
    - explicit hot-temperature probe (`85 F`) prefers a lighter outerwear upgrade (`coat_winter -> jacket_light`)
    - scope stays intentionally narrow: shirt/vest-slot outerwear that covers both torso and arms
  - new V3 deterministic legwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers full-length cargo pants over cargo shorts (`shorts_cargo -> pants_cargo`)
    - explicit hot-temperature probe (`85 F`) prefers cargo shorts over full-length cargo pants (`pants_cargo -> shorts_cargo`)
    - scope stays intentionally narrow: pants-slot short-vs-full-length coverage only
- latest proportional runtime locker proof still covers the V1/V2 baseline **and** the first V3 outerwear-temperature lane:
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - `make -j4 TILES=1 cataclysm-tiles`
  - controlled cold probe on `dev` / `Sandy Creek`
    - staged **Bruna Priest** in `jacket_light` with `coat_winter` on the `CAMP_LOCKER` tile via the repo `./zzip` save tool, then loaded through `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
    - harness load passed cleanly on `.userdata/dev/harness_runs/20260405_204703`
    - current `debug.log` on `d3b61027ec-dirty` emitted the real locker service packet:
      - `camp locker: before Bruna Priest ... temp_f=45.0 ... vest=[light jacket ... <jacket_light>] ... locker_raw=[puffer jacket (poor fit)<coat_winter>]`
      - `camp locker: plan for Bruna Priest ... changes=[... vest upgrade light jacket ... <jacket_light> -> puffer jacket (poor fit)<coat_winter> ...]`
      - `camp locker: after Bruna Priest ... vest=[puffer jacket (poor fit)<coat_winter>] ... locker=[... vest=[light jacket<jacket_light>] ...]`
  - controlled hot probe on the same save shape
    - restaged **Bruna Priest** in `coat_winter` with `jacket_light` on the locker tile, loaded through the harness, then forced `85 F` in the live debug temperature control before advancing to the next service window
    - the resulting live current-binary packet showed the planner re-queueing a lighter-outerwear swap:
      - `camp locker: before Bruna Priest ... vest=[puffer jacket ... <coat_winter>] ... locker_raw=[light jacket (poor fit)<jacket_light>]`
      - `camp locker: queued Bruna Priest state-dirty ... plan=[vest upgrade puffer jacket ... <coat_winter> -> light jacket (poor fit)<jacket_light>]`

Latest live-probe status for the open legwear lane:
- 2026-04-05 stale-binary audit first corrected the evidence path on current HEAD: the running window title was still `6b6de8d477-dirty`, so the tiles target was rebuilt before collecting new runtime proof; the live packet below comes from `d55ffe0a53`
- restored the original current `dev` / `Sandy Creek` save, backed it up under `.userdata/dev/save/manual_probe_backups/20260405_232203_ricky_legwear_cold/`, and used the packed-save path that the save audit had already identified as authoritative
- staged **Ricky Broughton** in `shorts_cargo` inside packed overmap `overmaps/o.0.0.zzip`, left `pants_cargo` on the packed locker tile in `maps/3.0.0.zzip`, then loaded through `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
- a plain fresh autoload still was not the missing evidence class by itself, so the honest runtime step remained live post-load gameplay: focused the current game window and sent `peekaboo press tab --count 3 --app cataclysm-tiles --window-id 10427`
- current `debug.log` on `d55ffe0a53` then emitted the real cold legwear locker packet at `23:23:32.719`:
  - `camp locker: action Ricky Broughton reason=worker-downtime ...`
  - `camp locker: reached worker_downtime for Ricky Broughton ...`
  - `camp locker: queued Ricky Broughton wake_dirty=true ...`
  - `camp locker: before Ricky Broughton ... pants=[cargo shorts > 6 items<shorts_cargo>] ... locker=[pants=[cargo pants (poor fit)<pants_cargo>]]`
  - `camp locker: plan for Ricky Broughton ... changes=[pants upgrade cargo shorts > 6 items<shorts_cargo> -> cargo pants (poor fit)<pants_cargo>] ...`
  - `camp locker: after Ricky Broughton ... pants=[cargo pants (poor fit)<pants_cargo>] ... locker=[pants=[cargo shorts<shorts_cargo>]]`
  - `camp locker: serviced Ricky Broughton applied=true ...`
- restored the staged save from that manual backup after the capture so the default `dev` world did not stay pinned to the probe state

Meaning:
- Locker Zone V1/V2 remain preserved under the new V3 code path at deterministic-suite level
- the outerwear V3 lane has both deterministic proof and proportional runtime proof on the current binary
- the legwear V3 lane now has deterministic proof plus a matching **cold-side** live packet on the current binary / current-save path
- the remaining runtime gap is now the **hot-side** legwear counterpart (`pants_cargo -> shorts_cargo`), not probe-path repair
- the hot-side outerwear runtime evidence currently comes from a real state-dirty replan packet after the live temperature flip rather than a second captured `after` packet, but it still proves the real planner/service path picks the lighter jacket when the temperature turns hot

---

## Pending probes

### Active queue — Locker Zone V3

1. capture the **hot-side** live packet for the new pants-slot legwear lane
   - keep `shorts_cargo` vs `pants_cargo` as the clean deterministic-shaped wardrobe pair
   - reuse the now-proven Ricky current-save path: packed-save staging, harness autoload, then live post-load `Tab` advance through Peekaboo
   - force the hot temperature before the service window so the missing runtime proof is the real `pants_cargo -> shorts_cargo` swap, not just another cold packet
2. keep using live post-load gameplay as the probe path; do not treat another plain startup-only harness rerun as new evidence unless the save shape changes materially
3. only after the hot-side packet lands, decide whether V3 should move to the next narrow follow-up lane or whether the live legwear evidence exposed a smaller corrective slice
4. keep per-NPC personality nuance out unless the hot-side legwear probe exposes a real need for a smaller corrective slice

### Non-blocking Josef notes

None yet for Locker Zone V3.
If later needed, add V3 notes here without turning them into blockers.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow deterministic Basecamp bark / routing check
- `./tests/cata_test "[camp][basecamp_ai]"`

### Narrow deterministic locker check
- `./tests/cata_test "[camp][locker]"`

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Startup/load smoke
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
