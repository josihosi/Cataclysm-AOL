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
- the topology spine is real: `CAMP_PATROL` exists and patrol tiles are grouped by 4-way connected clusters
- the deterministic planner contract is real: patrol workers are the assigned camp NPCs with patrol priority > 0, the planner splits them into day/night rosters, and allocation stays deterministic across the reference disconnected-post and connected-cluster cases
- the sticky-roster / interrupt-whitelist contract is real too: current shift rosters latch for the whole day/night block, routine chores do not steal active guards mid-shift, and urgent breaks can consume reserve backfill without rebalancing the whole roster
- deterministic on-map runtime intent is now real: a guard with one fully staffed connected cluster holds a distinct tile, while understaffed or multi-post assignments walk a fixed loop that advances every 10 in-game minutes; off-shift patrol workers fall back to ordinary camp downtime instead of clinging to stale posts
- legacy-save patrol control-surface drift is now covered too: old serialized camp-worker job maps can omit newer task keys, so `job_data::deserialize` now reseeds missing default camp jobs and keeps `ACT_CAMP_PATROL` available on older saves/fixtures
- narrow deterministic coverage now lives in `legacy_job_data_load_adds_missing_patrol_priority_surface`, `camp_patrol_zone_surface_and_sorted_tiles`, `camp_patrol_zone_clusters_use_4_way_connectivity`, `camp_patrol_worker_pool_uses_patrol_priority_surface`, `camp_patrol_planner_contract`, `camp_patrol_interrupt_contract`, `camp_patrol_shift_roster_latches_until_boundary`, and `camp_patrol_runtime_contract`
- latest narrow evidence: repaired fresh `make -j4 tests` plus `./tests/cata_test "[camp][patrol]"` on 2026-04-06 after the save-compat reseed fix
- live patrol proof now exists as separate screen/tests/artifacts packets on the **current binary**:
  - `patrol.disconnected_live` -> `.userdata/dev-harness/harness_runs/20260406_221126/probe.report.json` with `verdict: artifacts_matched`, `workers=2 roster=1 active=1`, a readable `close_ricky_priorities.after.png` staffing crop, a readable `open_zones_manager.after.png` topology crop, and a tight `runtime_motion_compare.gif` blink helper showing the disconnected-post loop advance one dwell later
  - `patrol.connected_live` -> `.userdata/dev-harness/harness_runs/20260406_221548/probe.report.json` with `verdict: artifacts_matched`, `workers=4 roster=2 active=2`, a readable `close_milo_priorities.after.png` staffing crop, a readable `open_zones_manager.after.png` topology crop, and a tight `runtime_motion_compare.gif` blink helper showing the connected-cluster hold state staying put one dwell later
- the stale-binary ambiguity on the earlier helper reruns is gone: the current patrol packet now launches `cataclysm-tiles` at repo head `4e3b63650d-dirty` and reports `version_matches_runtime_paths: true`
- the remaining honest gap is now the **broader player-legibility bar**, not raw loop-vs-hold visibility: the packet now explains loop vs hold much better, but it still needs an audit for whether uncovered posts and off-shift / reserve state are understandable enough without leaning on artifact logs

What counts next:
- audit the improved current-binary packet against the remaining player-legibility questions (coverage gaps and off-shift / reserve readability)
- if that is still muddy, add only the narrowest companion evidence/helper that explains those remaining questions

### Existing baseline that should not be mistaken for patrol proof

- Locker Zone V1/V2 and locker-capable harness restaging remain checkpointed; do not rerun them by ritual just because Patrol Zone v1 is now active.
- The repo already has one honest packaged proof format that separates **screen** / **tests** / **artifacts** (for example `locker.weather_wait` at `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json`). Treat that as a packaging template only, not as Patrol Zone evidence.
- Current harness scenarios `chat.nearby_npc_basic` and `ambient.weird_item_reaction` remain hackathon-reserved scaffolding. They are not part of Patrol Zone v1.

### Meaning

- Patrol Zone v1 has now earned the topology, planner, and sticky-roster rows; it still needs to earn the on-map behavior / live-proof rows.
- Zone-type plumbing, prose, and plausible-looking motion do **not** count as success by themselves.
- If a patrol report sounds cleaner than the code/tests/live packet beneath it, stop and audit before touching the ledgers.

---

## Pending probes

### Active queue — Patrol Zone v1

1. audit the improved patrol packet against the remaining **player-legibility** questions
   - `patrol.disconnected_live` should still show the looping disconnected-post case plus the resulting coverage gap clearly enough
   - `patrol.connected_live` should still show the staffed connected-cluster hold case plus why extra workers are off-shift / not dogpiling the same square
   - treat the staffing crop + topology crop + `runtime_motion_compare.gif` trio as the new baseline packet
2. keep the helper idea `sustain_npc` available only if a future patrol probe genuinely needs it
3. if the remaining legibility audit still exposes confusion, tighten only the smallest companion evidence/constants/docs needed to explain it

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

### Active-lane handoff block

- **finish line:** the packaged live-proof packet already exists on the current binary; the remaining close-out is whether the full patrol packet is understandable enough in play (coverage gaps, connected-vs-disconnected behavior, off-shift / reserve state) while still looking like simple v1 patrol rather than smart-zone soup
- **deterministic tests:** topology, planner contract, sticky roster/interrupt contract, hold-vs-loop runtime intent coverage, and legacy save-compat priority-surface coverage are in place
- **agent live proof:** packaged disconnected-loop and connected-hold scenarios both exist with separate screen/tests/artifacts evidence plus a `runtime_motion_compare.gif` helper on the current binary
- **Josef ask:** none yet; batch visually important patrol questions together only if the improved packet is worth human eyes
- **likely tweak round:** player-legibility audit first, then any tiny companion-evidence/constants/docs cleanup if the improved packet still reads oddly

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
