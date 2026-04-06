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

### Patrol Zone v1

Current honest state:
- the topology spine is now real: `CAMP_PATROL` exists and patrol tiles are grouped by 4-way connected clusters
- narrow deterministic coverage exists for that topology slice in `camp_patrol_zone_surface_and_sorted_tiles` and `camp_patrol_zone_clusters_use_4_way_connectivity`
- latest narrow evidence: `make -j4 tests` and `./tests/cata_test "[camp][patrol]"` on 2026-04-06
- there is still **no planner-specific deterministic coverage yet**
- there is still **no patrol-specific live proof yet**

What counts next:
- deterministic planner tests for the staffing/allocation contract
- then deterministic sticky-roster / interrupt-whitelist tests
- only after those are real should any live patrol packet be packaged

### Existing baseline that should not be mistaken for patrol proof

- Locker Zone V1/V2 and locker-capable harness restaging remain checkpointed; do not rerun them by ritual just because Patrol Zone v1 is now active.
- The repo already has one honest packaged proof format that separates **screen** / **tests** / **artifacts** (for example `locker.weather_wait` at `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json`). Treat that as a packaging template only, not as Patrol Zone evidence.
- Current harness scenarios `chat.nearby_npc_basic` and `ambient.weird_item_reaction` remain hackathon-reserved scaffolding. They are not part of Patrol Zone v1.

### Meaning

- Patrol Zone v1 has now earned the topology row and still needs to earn the planner / roster / live-proof rows.
- Zone-type plumbing, prose, and plausible-looking motion do **not** count as success by themselves.
- If a patrol report sounds cleaner than the code/tests/live packet beneath it, stop and audit before touching the ledgers.

---

## Pending probes

### Active queue — Patrol Zone v1

1. deterministic planner contract
   - patrol pool = NPCs with patrol priority > 0
   - two shifts
   - 1 NPC / 4 disconnected posts
   - 4 NPCs / 4 disconnected posts
   - mixed clusters => one-per-cluster coverage first
2. sticky shift roster / interrupt-whitelist contract
   - shift-boundary roster formation
   - routine work does not steal on-shift guards
   - urgent disruption can break patrol
   - reserve backfill does not trigger full-roster reshuffle
3. on-map hold-vs-loop behavior
   - understaffed connected cluster => fixed loop
   - fully staffed connected cluster => distinct holders
   - 16 NPCs / 4 connected squares stays explainable
4. only after the deterministic contract is real, package one honest live patrol proof with separate **screen** / **tests** / **artifacts** evidence
5. keep the helper idea `sustain_npc` available if a live patrol probe genuinely needs it

### Anti-hallucination rule for this lane

Do not treat any of these as success by themselves:
- only adding patrol zone-type plumbing
- only writing a planner without tests
- only writing docs/spec text
- only observing vaguely plausible NPC motion on-screen
- only claiming the interrupt rules in prose without deterministic proof

If the story starts sounding cleaner than the evidence, stop and audit.

**Hackathon-reserved — do not touch before the event:**
1. **chat interface over dialogue branches**
   - current `chat.nearby_npc_basic` evidence is harness-only scaffolding, not feature implementation
2. **ambient-trigger reaction lane / tiny ambient-trigger NPC model**
   - current `ambient.weird_item_reaction` evidence is harness-only scaffolding/observability, not feature implementation

**Later discussion topics, not current code lanes:**
1. reopen **Locker Zone V3** for one deliberately narrow next judgment slice
2. **smart zone manager**

### Non-blocking Josef notes

- None yet for Patrol Zone v1.
- If a later live packet is worth Josef's eyes, batch the visually important questions together (hold-vs-loop readability, interruption readability, and whether the patrol surface feels legible) instead of drip-feeding tiny asks.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Fresh full test rebuild on this Mac
- `make -j4 tests`

### Fresh tiles relink on this Mac
- `make -j4 TILES=1 cataclysm-tiles`

### Patrol-focused deterministic check
- once patrol tests exist, run the narrowest patrol-specific `cata_test` filter or named case instead of the whole suite

### Startup/load smoke for later live patrol proof
- `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
