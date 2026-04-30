# Mixed hostile stalker/horde performance playtest v0

Date: 2026-04-30
Ledger ID: `CAOL-MIXED-HOSTILE-PERF-v0`
Status: GREENLIT / PERFORMANCE PLAYTEST PACKET

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

## Relation to current writhing-stalker lane

This packet should not replace the three basic writhing-stalker live proofs:

- `writhing_stalker.live_shadow_strike_mcw`
- `writhing_stalker.live_no_omniscient_beeline_mcw`
- `writhing_stalker.live_exposed_retreat_mcw`

Preferred order:

1. prove at least one basic stalker live scene so the monster path is real;
2. then run this mixed-hostile performance packet;
3. if Andi can safely prep fixture/scenario/instrumentation first, that is fine, but the report must label any missing stalker live proof honestly.

## Anti-soup note

Bandit camp + cannibal camp + stalker + horde is intentionally a soup test. Soup is allowed here because Josef asked for it. But the report must measure the soup; it must not merely stare into the pot and declare it broth.
