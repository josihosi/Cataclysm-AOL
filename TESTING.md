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
- 2026-04-05 agent-side repro audit on current HEAD (`6b6de8d477-dirty`) showed that the missing legwear runtime packet is **not** just waiting to be collected from the old save ritual:
  - restored the original current `dev` / `Sandy Creek` save, staged **Bruna Priest** into `shorts_cargo` with `pants_cargo` on the locker tile at `35 F` via `./zzip`, then loaded through `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
  - that reload reached gameplay, but it did **not** emit a fresh legwear locker packet in `debug.log`
  - switched to the harness fixture path (`install-fixture basecamp_dev_manual_2026-04-02 --profile dev --replace`), restaged the same cold probe, advanced turns in live gameplay through Peekaboo, and saved
  - rereading the saved world after that run still showed `Bruna Priest` in `shorts_cargo` with `pants_cargo` still sitting on the locker tile, so that fixture path is also not yet a truthful live legwear reproducer
- same-day probe-path follow-up on current HEAD (`97eb3adfdc-dirty`) narrowed the blocker further instead of pretending the silence was informative:
  - added the smallest caller-side trace above `process_camp_locker_downtime` in `npc::move()` / `npc::worker_downtime`, then rebuilt with `make -j4 TILES=1 cataclysm-tiles`
  - a plain fresh harness load on the restored current-save path still produced **no** fresh `camp locker:` lines at all in the new log segment, so autoload by itself is not an honest legwear runtime probe on this save
  - focusing the already-loaded game and sending live post-load turn advance through Peekaboo (`peekaboo press tab --count 3 --app cataclysm-tiles --window-id ...`) immediately re-entered the locker service loop on the current binary, proving the missing evidence class is live gameplay after load rather than another ceremonial startup rerun
  - the resulting packet was for **Ricky Broughton**, not Bruna: `camp locker: queued Ricky Broughton state-dirty ... plan=[pants upgrade antarvasa ... -> cargo pants (poor fit)<pants_cargo>]`, followed by the matching `before` / `after` / `serviced` lines at `23:00:45`
  - packed-save audit also corrected the save-edit assumption: the restored current dev save is reading the packed `./zzip` overmap/map archives, not just loose extracted staging files; recompressing the staged locker tile back into `maps/3.0.0.zzip` made the `pants_cargo` candidate reachable at runtime again
  - Bruna's staged overmap wardrobe (`shorts_cargo` + `camp_locker_wake_dirty`) still did **not** hydrate into her live worker state, so the remaining blocker is now specifically the active NPC worn-state source for the current save, not proof that the locker loop or locker tile no longer work
  - the saved manual backup metadata at `.userdata/dev/save/manual_probe_backups/20260405_113052/current-char-zones.json` still shows the historical `CAMP_LOCKER` zone at `(2841,681,0)`, so the current failure is still not narrowed to a simple “locker zone never existed” claim

Meaning:
- Locker Zone V1/V2 remain preserved under the new V3 code path at deterministic-suite level
- the outerwear V3 lane has both deterministic proof and proportional runtime proof on the current binary
- the new legwear V3 lane still lacks matching live proof, and the blocker is now the **runtime probe path itself**, not just an uncaptured packet
- the hot-side outerwear runtime evidence currently comes from a real state-dirty replan packet after the live temperature flip rather than a second captured `after` packet, but it still proves the real planner/service path picks the lighter jacket when the temperature turns hot

---

## Pending probes

### Active queue — Locker Zone V3

1. rebuild a reliable live probe for the new pants-slot legwear lane
   - keep `shorts_cargo` vs `pants_cargo` as the clean deterministic-shaped wardrobe pair
   - do **not** count the old outerwear save ritual as a valid legwear reproducer anymore until it emits or persists the legwear swap on a fresh run again
2. next honest agent-side options are:
   - recover the real active NPC worn-state source that the restored current save hydrates for Bruna, instead of only editing the overmap entry
   - use the now-proven packed-save + post-load live-turn path to stage a clean `shorts_cargo` / `pants_cargo` pair onto a worker who definitely reaches locker service after load
   - keep using live post-load gameplay as the probe path; do not treat another plain startup-only harness rerun as new evidence unless the save shape changes materially
3. only after the probe path is trustworthy, capture the real locker planner/service path under cold and hot legwear setups
4. keep per-NPC personality nuance out unless the legwear live probe exposes a real need for a smaller corrective slice

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
