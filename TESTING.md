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
- Patrol Zone v1 is checkpointed / done for now.
- the topology spine is real: `CAMP_PATROL` exists and patrol tiles are grouped by 4-way connected clusters
- the deterministic planner contract is real: patrol workers are the assigned camp NPCs with patrol priority > 0, the planner splits them into day/night rosters, and allocation stays deterministic across the reference disconnected-post and connected-cluster cases
- the sticky-roster / interrupt-whitelist contract is real too: current shift rosters latch for the whole day/night block, routine chores do not steal active guards mid-shift, and urgent breaks can consume reserve backfill without rebalancing the whole roster
- deterministic on-map runtime intent is real: a guard with one fully staffed connected cluster holds a distinct tile, while understaffed or multi-post assignments walk a fixed loop that advances every 10 in-game minutes; off-shift patrol workers fall back to ordinary camp downtime instead of clinging to stale posts
- legacy-save patrol control-surface drift is covered too: old serialized camp-worker job maps can omit newer task keys, so `job_data::deserialize` now reseeds missing default camp jobs and keeps `ACT_CAMP_PATROL` available on older saves/fixtures
- narrow deterministic coverage lives in `legacy_job_data_load_adds_missing_patrol_priority_surface`, `camp_patrol_zone_surface_and_sorted_tiles`, `camp_patrol_zone_clusters_use_4_way_connectivity`, `camp_patrol_worker_pool_uses_patrol_priority_surface`, `camp_patrol_planner_contract`, `camp_patrol_interrupt_contract`, `camp_patrol_shift_roster_latches_until_boundary`, and `camp_patrol_runtime_contract`
- latest deterministic evidence is still the repaired fresh `make -j4 tests` plus `./tests/cata_test "[camp][patrol]"` on 2026-04-06 after the save-compat reseed fix
- current-binary live patrol proof now exists as separate screen/tests/artifacts packets plus a small explainer helper:
  - `patrol.disconnected_live` -> `.userdata/dev-harness/harness_runs/20260406_230124/probe.report.json` with `verdict: artifacts_matched`, a readable staffing crop, a readable zone-topology crop, `runtime_motion_compare.gif`, and `probe.patrol_summary.txt` explaining why 4 disconnected posts + a 1-guard day roster necessarily leaves gaps between visits
  - `patrol.connected_live` -> `.userdata/dev-harness/harness_runs/20260406_230552/probe.report.json` with `verdict: artifacts_matched`, a readable staffing crop, a readable zone-topology crop, `runtime_motion_compare.gif`, and `probe.patrol_summary.txt` explaining why 4 patrol-enabled workers still become a 2-guard day roster with 2 off-shift guards and 2 held tiles
- the stale-binary ambiguity on the earlier helper reruns is gone: the current patrol packet still reports `version_matches_runtime_paths: true` against the current runtime-relevant tree
- the player-legibility close-out now counts as met for this packet: loop-vs-hold, disconnected-vs-connected layout, uncovered posts, and off-shift counts are all stated directly enough without making the reader mine raw patrol trace logs

What counts next:
- no further patrol probe is queued unless later code or runtime evidence reopens the lane

### Existing baseline that should not be mistaken for patrol proof

- Locker Zone V1/V2 and locker-capable harness restaging remain checkpointed; do not rerun them by ritual now that Patrol Zone v1 is closed.
- The repo already has one honest packaged proof format that separates **screen** / **tests** / **artifacts** (for example `locker.weather_wait` at `.userdata/dev-harness/harness_runs/20260406_125056/probe.report.json`). Treat that as a packaging template only, not as Patrol Zone evidence.
- Current harness scenarios `chat.nearby_npc_basic` and `ambient.weird_item_reaction` remain hackathon-reserved scaffolding. They are not part of Patrol Zone v1.

### Meaning

- Patrol Zone v1 has now earned the topology, planner, sticky-roster, on-map behavior, live-proof, and player-legibility rows; the lane is closed unless later evidence breaks one of those claims.
- Zone-type plumbing, prose, and plausible-looking motion do **not** count as success by themselves.
- If a patrol report sounds cleaner than the code/tests/live packet beneath it, stop and audit before touching the ledgers.

---

## Pending probes

### Active queue

No active validation queue.
Patrol Zone v1 is closed, and the next lane is waiting on Josef's greenlight.

Still true:
1. keep the helper idea `sustain_npc` available only if a future patrol probe genuinely needs it

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

- **active lane:** none right now; Patrol Zone v1 is closed and the repo is parked for the next greenlight
- **last closed lane:** Patrol Zone v1 now has deterministic tests, current-binary disconnected/connected live packets, and the tiny `probe.patrol_summary.txt` companion helper for the last legibility questions
- **Josef ask:** pick the next lane; no patrol-specific follow-up is required for close-out

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
