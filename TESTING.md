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

### Locker Zone V1 / V2 checkpoint baseline
Latest relevant agent-side locker packet:
- committed HEAD `0d03b1557e`
  - `make -j4 tests`
  - `./tests/cata_test "[camp][locker]"`
  - passed at `139 assertions in 11 test cases`
- the targeted suite still covers the bundled locker baseline from V1 while now proving the landed V2 contract:
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
- proportional runtime proof on the current binary:
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
- Locker Zone V2 is now checkpointed: deterministic coverage proves the two-magazine / locker-ammo reload contract, and the real save again executes the ranged locker path on current HEAD
- Locker Zone V1 remains preserved under the V2 implementation
- if V3 work breaks either bundled locker baseline, reopen the affected slice instead of performing ceremonial rebuilds

---

## Pending probes

### Active queue — Locker Zone V3

1. define the first V3 weather-sensitive clothing contract
   - choose one narrow lane with explicit scope (outerwear / blanket / shorts / similar)
   - name the deterministic inputs that drive it (temperature / weather / season)
   - keep per-NPC personality nuance out unless a smaller deterministic slice genuinely needs it
2. add/update deterministic coverage for that first V3 lane
3. use startup / live probing only after a landed V3 slice needs runtime evidence beyond the deterministic locker suite

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
