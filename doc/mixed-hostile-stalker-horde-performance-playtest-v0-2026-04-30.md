# Mixed hostile stalker/horde performance playtest v0

Date: 2026-04-30
Ledger ID: `CAOL-MIXED-HOSTILE-PERF-v0`
Status: GREEN/YELLOW METRICS REPORT ACCEPTED FOR V0

## Josef ask

> Can you add another playtest, performance playtest with bandit camp, cannibal camp, stalker and horde on the map? I want a report with METRICS

## Classification

This is a **performance playtest packet**, not a new behavior-design lane.

It should exercise already-promoted hostile systems together:

- one bandit camp / bandit live-world site;
- one cannibal camp / cannibal live-world site;
- one `mon_writhing_stalker` using the current v0 live monster-plan seam;
- one overmap horde or horde-pressure fixture near enough to matter;
- a player state/location that allows bounded time passage without instantly collapsing into an unreviewable fight.

The point is to produce a metrics report under mixed hostile load. Do not close this from startup/load alone, from deterministic tests alone, or from a fixture that silently leaves one of the four actors inert.

## Proposed scenario name

`performance.mixed_hostile_stalker_horde_mcw`

If the first implementation needs a narrower staged fixture, keep the public row name stable and label the setup honestly, e.g. `pre-staged active hostile load`, `natural dispatch`, or `mixed fixture support`.

## Evidence classes

Keep these separate in the report:

1. **Fixture/setup support** — saved-state audits prove the bandit camp, cannibal camp, stalker, and horde exist in the intended world neighborhood.
2. **Live game path** — time passage reaches real hostile maintenance / monster planning / horde movement paths.
3. **Timing/counters** — compact metrics prove the cost envelope and show which subsystem consumed time.
4. **Harness wall-clock** — whole-probe runtime, useful but not a substitute for game-path timing.
5. **Outcome/tuning** — whether mixed pressure produced a playable scene or immediate unreadable chaos.

## Required setup proof

Before measuring, the run must prove all four pieces are present:

- bandit site: site/profile/id, member count, active job state or remembered/structural lead state;
- cannibal site: site/profile/id, member count, active job state or live bootstrap line;
- stalker: exactly one `mon_writhing_stalker`, approximate player distance, alive status;
- horde: saved overmap horde metadata or same-run horde audit with type/location/destination/tracking fields;
- player: position/time/light/noise/condition summary sufficient to explain whether the stalker and hostile sites should care.

A setup row is support only. It does not prove performance until the live wait/turn window runs.

## Required live windows

Start with the smallest honest pair of windows:

1. **500-turn sampled window**
   - measures ordinary per-turn cost;
   - catches monster-plan spikes and horde movement churn;
   - should not use a single long wait if per-turn behavior is the question.

2. **30-minute bounded wait window**
   - measures cadence-gated hostile overmap work;
   - should emit the existing `bandit_live_world perf:` rows plus any new stalker/horde counters Andi adds;
   - must record before/after game clock or turn delta.

If the 500-turn window is too noisy or blocked by immediate combat, preserve it as red/yellow and switch to a materially changed setup rather than rerunning the same soup.

## Required metrics report

The final report must include a compact metrics table with at least:

| Metric | Required value |
|---|---|
| Runtime commit | repo head and dirty/clean state |
| Scenario/run path | harness scenario and `.userdata/.../harness_runs/.../` path |
| Setup mix | bandit sites, cannibal sites, stalker count, horde count |
| In-game window | sampled turns and/or waited minutes |
| Harness wall-clock | total real seconds |
| Turn cost | min / median / p95 / max per-turn ms for sampled turns where available |
| Hostile overmap cadence cost | min / median / max `bandit_live_world perf total_us` |
| Dispatch/signal slices | max `signal_us`, `dispatch_us`, `travel_us`, plus active job mix |
| Stalker planning cost | evaluation count and min / median / p95 / max us/ms if instrumented; otherwise explicitly `not instrumented` |
| Horde cost | horde retarget/movement count and min / median / p95 / max if instrumented; otherwise explicitly `not instrumented` |
| Debug/log spam | number of relevant perf/debug lines and whether it looks bounded |
| Stability | crashes, debug errors, hangs, wait interruptions |
| Playability note | readable pressure vs unreadable chaos |

## Green / yellow / red rules

Green means:

- current-runtime startup/load is clean;
- all four hostile ingredients are proven present;
- live time passage completes without crash/hang/debug-error flood;
- metrics are recorded from the relevant live paths;
- report clearly separates setup support from live-path timing;
- cost envelope is bounded enough for current playtest scale, or the report names a concrete budget concern with numbers.

Yellow means:

- the run completes but one subsystem lacks direct timing instrumentation;
- the horde or stalker is present but has weak live-path proof;
- wall-clock is acceptable but per-turn attribution is incomplete;
- the scene is playable but confusing enough to need tuning.

Red means:

- crash, hang, runaway wait, or repeated debug errors;
- one of the four requested hostile ingredients is absent;
- metrics are missing or only measure startup/load;
- the scene degenerates immediately into unreviewable chaos before any useful window is measured.

## Run report - 2026-04-30 `20260430_181748`

Status: **GREEN/YELLOW PERFORMANCE READOUT**. The harness classified the run as `artifacts_matched`, `evidence_class=feature-path`, `feature_proof=true`, with `green_wait_steps_proven` and `green_step_local_proof`. The yellow caveat is attribution, not presence: horde setup is proven, but horde movement/retarget cost is not separately instrumented in this run.

Evidence path: `.userdata/dev-harness/harness_runs/20260430_181748/`

Scenario: `performance.mixed_hostile_stalker_horde_mcw`

Command shape: `python3 tools/openclaw_harness/startup_harness.py probe performance.mixed_hostile_stalker_horde_mcw` after JSON validation.

### Evidence class separation

- **Fixture/setup support:** preflight saved-state audits prove the active bandit site, active cannibal site, single `mon_writhing_stalker`, and nearby `mon_zombie` horde before measurement. The fixture is explicitly `pre-staged active hostile load`, not natural dispatch, not `/fire/map` or `/smoke/light` attraction proof, and not a horde signal-response proof.
- **Live game path:** the 500 individual-turn sample and bounded 30-minute wait completed through the game path; the run saved afterward and `audit_saved_game_turn_after_mixed_perf_windows` proved `observed_delta_turns=2300` from baseline `5227200` to `5229500`.
- **Timing/counters:** `advance_turns` recorded sampled turn wall-clock, `bandit_live_world perf:` emitted hostile overmap cadence counters, and `writhing_stalker live_plan:` emitted `eval_us=` timing.
- **Harness wall-clock:** about `153.0s` from first probe screenshot to final report write; the 500-turn sample itself took `118.119432s`.
- **Outcome/tuning:** stable and readable under this staged noon load; not immediate chaos. Horde-specific cost remains `not instrumented`.

### Setup proof

| Actor | Proof | Result |
|---|---|---|
| Bandit camp | `preflight_mixed_perf_bandit_active_job.metadata.json` | `overmap_special:bandit_camp@140,51,0`, `member_count=4`, `active_outside_count=4`, `active_job_type=stalk`, `required_state_present` |
| Cannibal camp | `preflight_mixed_perf_cannibal_active_job.metadata.json` | `overmap_special:cannibal_camp@150,39,0`, `member_count=2`, `active_outside_count=2`, `active_job_type=stalk`, `required_state_present` |
| Stalker | `preflight_mixed_perf_writhing_stalker.metadata.json` | exactly one `mon_writhing_stalker`, `hp=90`, `friendly=0`, `dead=false`, offset `[8,0,0]`, `required_state_present` |
| Horde | `preflight_mixed_perf_horde.metadata.json` | required `mon_zombie` horde at offset `[0,-240,0]`, destination offset `[0,-120,0]`, `tracking_intensity=10`, plus one incidental nearby `mon_robin`; `required_state_present` |
| Player/time | saved turn audits | player at `[3372,948,0]` / OMT `[140,39,0]`; baseline `time_of_day_text=12:00:00`, final `12:38:20` |

### Metrics table

| Metric | Value |
|---|---|
| Runtime commit | game window title `Cataclysm: Dark Days Ahead - d49bd9435b-dirty`; report extraction checkout `8ae3938f7f` with dirty tree only from the new mixed-perf fixture/scenario before this report commit |
| Scenario/run path | `performance.mixed_hostile_stalker_horde_mcw`; `.userdata/dev-harness/harness_runs/20260430_181748/` |
| Setup mix | 2 hostile live-world sites (`sites=2`, `active_sites=2`, `active_job_mix=camp_style:stalk=1,cannibal_camp:stalk=1`), 1 `mon_writhing_stalker`, required 1 `mon_zombie` horde |
| In-game window | `500` sampled individual turns plus bounded `30m` wait; saved turn delta `2300` turns / `38:20` game time |
| Harness wall-clock | about `153.0s` probe screenshot-to-report; 500-turn sample `118.119432s` |
| Turn cost | 5 batches: min `235.214ms/turn`, median `236.203ms/turn`, p95 `240.447ms/turn`, max `240.447ms/turn`; average `236.239ms/turn` |
| Hostile overmap cadence cost | 8 unique `bandit_live_world perf:` rows; `total_us` min `475`, median `522`, p95/max `3777` |
| Dispatch/signal slices | `signal_us` max `1008`; `dispatch_us` max `2944`; `travel_us` max `53`; max row was the cadence due/dispatch due row at `total_us=3777` |
| Stalker planning cost | `writhing_stalker live_plan:` eval count `155`; `eval_us` min `4`, median `10`, p95 `21`, max `54`; `target_probe` count `2306` |
| Horde cost | **not instrumented separately**; setup horde metadata is green, but this run has no horde movement/retarget timing counter |
| Debug/log spam | `probe.artifacts.log` has `5582` lines; relevant mixed perf/debug count about `2469` (`8` bandit perf rows, `155` live-plan rows, `2306` target-probe rows). Bounded for harness, too verbose for always-on normal logs if left at this density. |
| Stability | no crash, no stderr, no debug-error/warning hits found; wait completed by artifact delta; cleanup terminated `cataclysm-tiles --userdir .userdata/dev-harness/ --world McWilliams` with `SIGTERM` after proof capture |
| Playability note | readable staged pressure, not immediate unreadable chaos. Noon/exposure made the stalker mostly disengage rather than dogpile: live decisions were `ignore=126`, `cooling_off=27`, `withdraw=2`; `zombie_pressure=0` throughout stalker decisions, so the staged horde did not amplify the stalker locally in this fixture. |

### Tuning readout

- **Too common:** no evidence of spawn spam. The fixture intentionally stages exactly one `mon_writhing_stalker`; natural rarity remains governed by the rare singleton `GROUP_ZOMBIE` footing, not by this performance fixture.
- **Too fast:** not indicated by this run. The mixed 500-turn sample averaged `236.239ms/turn` under harness, and the stalker retreated/ignored more than it attacked under bright noon conditions.
- **Too tanky:** not judged from this performance fixture because it did not run a player damage race. Existing monster footing is `hp=90` with hit-and-run/cooldown behavior; live strike/withdraw scenes cover behavior, not a full combat balance duel.
- **Too invisible:** not indicated as a perf failure here, but this run was a staged harness setup rather than a human readability pass.
- **Too honest / too stupid:** possibly a little too honest under bright noon mixed load: after initial contact, the stalker mostly produced `live_no_believable_evidence` / cooldown behavior and fled, which is safe and readable but low-pressure. Keep this as a future feel-tuning question, not a blocker for v0 metrics.
- **Too horde-amplified:** not shown. `zombie_pressure=0` in stalker live-plan lines, and horde direct cost/retarget behavior is not separately instrumented.
- **Too expensive:** no at this scale. Hostile overmap cadence stayed sub-4ms per emitted perf row (`total_us` max `3777`), and stalker live evaluation stayed sub-0.1ms (`eval_us` max `54`). The main cost visible to the harness is ordinary turn advancement/UI automation, not the named C-AOL counters.
- **Too noisy:** debug instrumentation is noisy (`2306` target-probe rows); acceptable for this harness packet, but should stay debug/probe-facing rather than ordinary player-log chatter.

### Verdict

`CAOL-MIXED-HOSTILE-PERF-v0` is green/yellow and accepted for Josef's requested mixed-hostile performance metrics packet with one explicit caveat: horde presence is proven by saved setup audit, but horde cost is `not instrumented` in this run. Frau accepted this attribution caveat for v0 closure; stricter horde movement/retarget timing is future-only unless Schani/Josef explicitly promote it. No new behavior-design lane is implied.

## Relation to current writhing-stalker lane

This packet no longer substitutes for the basic writhing-stalker live proofs; those have separate green evidence:

- `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/`
- `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/`
- `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/`

Next order:

1. run this mixed-hostile performance packet;
2. report metrics and a tuning/readability verdict;
3. label any missing ingredient or missing metric honestly instead of using setup/startup proof as performance proof.

## Anti-soup note

Bandit camp + cannibal camp + stalker + horde is intentionally a soup test. Soup is allowed here because Josef asked for it. But the report must measure the soup; it must not merely stare into the pot and declare it broth.
