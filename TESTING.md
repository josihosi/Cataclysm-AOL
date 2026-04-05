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
- broader locker baseline remains anchored by committed HEAD `1381284982`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `171 assertions in 11 test cases`
- that broader suite still covers the bundled locker baseline from V1/V2 and the landed V3 temperature slices:
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
  - V3 deterministic legwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers full-length cargo pants over cargo shorts (`shorts_cargo -> pants_cargo`)
    - explicit hot-temperature probe (`85 F`) prefers cargo shorts over full-length cargo pants (`pants_cargo -> shorts_cargo`)
    - explicit duplicate-current hot probe keeps `pants_cargo` as the best current full-length legwear, marks `antarvasa` as duplicate current, and still selects `shorts_cargo` as the hot upgrade
    - scope stays intentionally narrow: pants-slot short-vs-full-length coverage only; it does **not** pretend to preserve layered lower-body nuance yet
- latest narrow validation for the new deterministic judgment lock on current HEAD `e6e2b38540-dirty`:
  - `make -j4 tests`
  - `./tests/cata_test "camp_locker_loadout_planning"`
  - passed at `68 assertions in 1 test case`
- note: `./tests/cata_test "[camp][locker]"` on `e6e2b38540-dirty` still reports `All tests passed (181 assertions in 11 test cases)`, but Catch exits non-zero because an existing `diag_value` debug error in the queue/downtime lane is treated as failure; the planner-only case above is the honest validation for this slice because no locker service code changed

Latest proportional runtime locker proof:
- outerwear lane on the recorded current-binary path
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
- legwear lane on the stale-binary-audited Ricky current-save path
  - 2026-04-05 stale-binary audit first corrected the evidence path on current HEAD: the running window title was still `6b6de8d477-dirty`, so the tiles target was rebuilt before collecting new runtime proof; the recorded live legwear packets below come from `d55ffe0a53`
  - restored the original current `dev` / `Sandy Creek` save, backed it up under `.userdata/dev/save/manual_probe_backups/20260405_232203_ricky_legwear_cold/`, and used the packed-save path that the save audit had already identified as authoritative
  - cold-side packet
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
  - hot-side packet
    - reused the same Ricky current-save path with `shorts_cargo` on the locker tile and Ricky wearing `cargo pants` alongside his existing `antarvasa`
    - current `debug.log` on `d55ffe0a53` then emitted the real hot legwear locker packet at `23:59:27.798`:
      - `camp locker: queued Ricky Broughton state-dirty ... plan=[pants upgrade cargo pants<pants_cargo> -> cargo shorts (poor fit)<shorts_cargo>; pants dedupe keep=cargo pants<pants_cargo> drop=[antarvasa > 6 items<antarvasa>]] ...`
      - `camp locker: before Ricky Broughton ... pants=[antarvasa > 6 items<antarvasa>; cargo pants<pants_cargo>] ... locker=[pants=[cargo shorts (poor fit)<shorts_cargo>]]`
      - `camp locker: after Ricky Broughton ... pants=[cargo shorts (poor fit)<shorts_cargo>] ... locker=[pants=[cargo pants<pants_cargo>; antarvasa<antarvasa>]]`
      - `camp locker: serviced Ricky Broughton applied=true ...`
  - restored the staged save from the manual backup after the captures so the default `dev` world did not stay pinned to the probe state

Meaning:
- Locker Zone V1/V2 remain preserved under the landed V3 code path at deterministic-suite level
- the outerwear and currently implemented legwear V3 lanes now both have deterministic proof and proportional runtime proof on recorded current-binary / current-save paths
- the `antarvasa` result is now an explicit policy judgment, not a mystery bug: current locker behavior is still one managed pants item per slot, so extra pants-slot garments are treated as duplicates and returned to the locker when a hot/cold swap lands
- the next missing evidence class is harness packaging/reporting discipline, not more locker packet collection

---

## Pending probes

### Active queue — hackathon runway: stabilization + harness

1. package the first harness uplift slice from `doc/harness-first-slice-plan-2026-04-06.md`
   - one reusable live probe contract/profile/save path on the current binary
   - explicit reporting split between **screen**, **tests**, and **artifacts/logs**
   - avoid probe-method drift mid-claim
2. make the first three scenarios real probe contracts
   - `locker.weather_wait`
   - `chat.nearby_npc_basic`
   - `ambient.weird_item_reaction`
3. after one harness slice is trustworthy, package a compact Josef-facing testing packet before the pre-holiday active-testing window gets chewed up by setup friction
4. do not spend more runs collecting fresh locker packets unless code changes or the harness work invalidates the recorded current-save path
5. keep per-NPC personality nuance and decorative side quests out unless the stabilization/harness work exposes a truly smaller corrective slice

### Non-blocking Josef notes

None yet for the hackathon runway lane.
If later needed, add notes here without turning them into blockers.

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
