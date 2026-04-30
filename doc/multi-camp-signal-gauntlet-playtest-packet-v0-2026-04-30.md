# Multi-camp signal gauntlet playtest packet v0 — 2026-04-30

## Classification

Status: **CLOSED / CHECKPOINTED GREEN V0**.

Proof: `doc/multi-camp-signal-gauntlet-proof-v0-2026-04-30.md`.

Ledger ID: `CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0`.

Imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`.

## Goal

Give Andi a real C-AOL challenge: prove that the live bandit overmap systems stay sane when multiple camps, structural bounty, and live player/world signals coexist.

This promoted three previously optional structural-bounty follow-ups into one bounded gauntlet:
1. two/four-camp wait stress;
2. mixed signal coexistence;
3. reload/resume continuity if the first two are green enough to preserve.

## Scope

Minimum closure requires two honest challenge rows:

### Challenge A — multi-camp structural stress

- Stage or reuse a live-world setup with at least two bandit camps; four camps is preferred if the harness can do it without turning the run into noise.
- Let structural bounty maintenance run across bounded time.
- Capture per-site/camp state before and after:
  - leads/targets considered;
  - active outings/groups;
  - selected target/reason;
  - harvested/dangerous/recently-checked/no-repeat state;
  - members returned/lost/active where available.
- Report whether camps spread out, hold back, or dogpile one target, and whether that behavior is acceptable for v0.
- Report timing/perf and debug/log spam.

### Challenge B — mixed signal coexistence

- In the same broad fixture family or a separate focused scenario, combine structural bounty with at least one live player/world signal already proved elsewhere: smoke/fire/light signal, roof/elevated fire signal, or equivalent.
- Capture how candidate reasons compete: structural bounty vs live signal vs threat/known danger.
- Prove that live signal handling does not erase structural-bounty state and structural-bounty scans do not drown out urgent live signals.
- Report target choice, rejected candidates/reasons where available, active outing state, and cost.

### Challenge C — reload/resume continuity, if practical

- If A or B creates meaningful active outing state, save, relaunch/reload, continue bounded time, and verify the active group/site state resumes instead of disappearing, duplicating, or becoming stale.
- If reload/resume is not practical in the same run, mark it as follow-up with exact missing seam rather than blocking closure of A/B.

## Non-goals

- Do not reopen `CAOL-ROOF-HORDE-NICE-FIRE-v0`, `CAOL-WRITHING-STALKER-v0`, or old fire-proof lanes.
- Do not implement a broad bandit AI redesign unless the playtest exposes a concrete missing bridge that blocks the gauntlet.
- Do not require natural unseeded discovery for v0; staged-but-live setups are acceptable if the live maintenance/outing paths run honestly.
- Do not call setup-only proof a challenge result. Time must pass and state must change or deliberately hold for named reasons.
- Do not hide dogpile/CPU/log-spam failures behind “technically completed”.

## Success state

- [x] A named Challenge A scenario/fixture exists and runs: `bandit.multi_camp_structural_stress_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_204416/`.
- [x] Challenge A proves at least two camp/site states before/after bounded time and reports active outings/target choices/no-repeat state.
- [x] Challenge A reports dogpile/spread/hold behavior and timing/log stability.
- [x] A named Challenge B scenario/fixture exists and runs: `bandit.mixed_signal_coexistence_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_203757/`.
- [x] Challenge B combines structural bounty with live smoke/fire signal footing and reports candidate priority/reasons.
- [x] Challenge B proves neither signal class silently wipes the other’s state.
- [x] Reload/resume continuity is green for meaningful active live-signal scout + structural scavenge outings: `bandit.mixed_signal_reload_resume_mcw`, green run `.userdata/dev-harness/harness_runs/20260430_203944/`.
- [x] Metrics include waited time/sampled turns, per-cadence timing, active group/site counts, warnings/errors/log spam, and crash status.
- [x] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `doc/work-ledger.md`, and `andi.handoff.md` match final state.

## Evidence boundary

Green closure must reach live maintenance/outing state and bounded time passage. Deterministic tests can support diagnosis, but the challenge itself needs live/harness proof rows with before/after state and metrics.

Final v0 caveats are preserved in the proof doc: two camps rather than four; staged-but-live signal footing rather than natural discovery; Challenge A proves harvested fixture leads plus east-camp followthrough/no-repeat, not all-camps-idle. No dogpile, stale-state, reload-loss, CPU churn, log spam, or crash failure was observed in the final green runs.

If this gauntlet is ever reopened with stricter criteria, preserve red/yellow results honestly. A red/yellow challenge report with exact dogpile, stale-state, reload-loss, or perf evidence is more useful than a fake green report.
