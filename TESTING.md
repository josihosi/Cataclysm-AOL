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
- committed HEAD `10750acaad`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `155 assertions in 11 test cases`
- the targeted suite still covers the bundled locker baseline from V1/V2 and now also proves the first landed V3 lane:
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
  - new V3 deterministic outerwear coverage in `camp_locker_loadout_planning`
    - explicit cold-temperature probe (`35 F`) prefers a warmer outerwear upgrade (`jacket_light -> coat_winter`)
    - explicit hot-temperature probe (`85 F`) prefers a lighter outerwear upgrade (`coat_winter -> jacket_light`)
    - scope is intentionally narrow: shirt/vest-slot outerwear that covers both torso and arms
- last recorded proportional runtime locker proof still covers the V1/V2 baseline, not the new V3 lane:
  - `make -j4 TILES=1 cataclysm-tiles`
  - `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
    - passed cleanly on `.userdata/dev/harness_runs/20260405_180427`
  - live current-binary probe on `dev` / `Sandy Creek`
    - let the loaded save advance with `Tab` x12 after the harness load
    - current `debug.log` on `0d03b1557e-dirty` then emitted a fresh locker packet for **Bruna Priest**:
      - `camp locker: before Bruna Priest ... ranged=[weapon=compact bullpup shotgun<ksg> mags=0 take=0 reload=0 ready=no]`
      - `camp locker: plan for Bruna Priest ... changes=[shirt dedupe keep=polo shirt<polo_shirt> drop=[flotation vest<flotation_vest>]; bag dedupe keep=messenger bag<mbag> drop=[leather belt<leather_belt>]] ranged=[weapon=compact bullpup shotgun<ksg> mags=0 take=0 reload=0 ready=no]`
      - `camp locker: after Bruna Priest ... locker=[shirt=[flotation vest<flotation_vest>]; bag=[leather belt<leather_belt>]] ranged=[weapon=compact bullpup shotgun<ksg> mags=0 take=0 reload=0 ready=no]`

Meaning:
- Locker Zone V1/V2 remain preserved under the new V3 code path at deterministic-suite level
- the first V3 outerwear-temperature lane is now landed with deterministic proof
- V3 still needs proportional runtime evidence on the current binary before it can claim more than a deterministic first slice

---

## Pending probes

### Active queue — Locker Zone V3

1. get proportional runtime evidence for the landed outerwear-temperature lane
   - prove the real planner/service path prefers warmer outerwear in a cold setup and lighter outerwear in a hot setup
   - if the standing `dev` / `Sandy Creek` save does not naturally present that choice, build the smallest controlled probe instead of faking confidence from the old ranged packet
2. once runtime proof exists, decide the next narrow V3 lane
   - likely shorts / hot-weather legwear
   - or a blanket / bedding-adjacent weather lane if that becomes easier to prove honestly
3. keep per-NPC personality nuance out unless a smaller deterministic clothing slice truly needs it

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
