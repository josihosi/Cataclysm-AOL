# Andi handoff: CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

## Current canon state

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is the **ACTIVE / GREENLIT CHALLENGE PLAYTEST PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

`CAOL-ROOF-HORDE-NICE-FIRE-v0` and `CAOL-WRITHING-STALKER-v0` are closed. Do not reopen them.

## Goal

Run a real challenge gauntlet for live bandit overmap systems: multiple camps, structural bounty, mixed live signals, bounded time passage, and metrics.

## Canonical packet

- Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`
- Imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`
- Prior structural-bounty closure footing: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`
- Prior green structural-bounty run: `.userdata/dev-harness/harness_runs/20260430_115157/`

## Required challenge rows

### Challenge A — multi-camp structural stress

Create/run a named scenario, proposed:

- `bandit.multi_camp_structural_stress_mcw`

Required evidence:
- at least two bandit camps/sites, four if clean;
- bounded time passage;
- before/after site and active-group state;
- selected targets and reasons;
- harvested/dangerous/recently-checked/no-repeat state;
- dogpile vs spread vs hold readout;
- wall-clock/per-turn/cadence metrics and log/crash status.

### Challenge B — mixed signal coexistence

Create/run a named scenario, proposed:

- `bandit.mixed_signal_coexistence_mcw`

Required evidence:
- structural bounty plus at least one live smoke/fire/light/roof-fire signal;
- candidate priority/reason readout showing how signals compete;
- proof that live signals do not erase structural-bounty state and structural-bounty scans do not drown urgent live signals;
- active outing state and cost metrics.

### Challenge C — reload/resume continuity, if practical

If A or B creates meaningful active outing state:
- save;
- relaunch/reload;
- continue bounded time;
- verify active group/site state resumes without disappearing, duplicating, or becoming stale.

If not practical in the same packet, state exactly why and what seam remains.

## Non-goals/cautions

- Do not reopen roof-horde, writhing-stalker, Smart Zone, or old fire proof lanes.
- Do not call setup-only camp/signal presence a challenge result.
- Do not require fully natural unseeded discovery for v0; staged-but-live setup is acceptable if live maintenance/outing paths run honestly.
- Do not hide dogpile, reload loss, stale state, CPU churn, or log spam behind green wording.
- If a challenge breaks, preserve the red/yellow evidence and exact blocker. A truthful ugly report beats pretty nonsense.

## Completion report must include

- scenario/run ids;
- commit/dirty state;
- camp/site counts and active group counts;
- before/after target/reason/no-repeat state;
- mixed-signal priority readout;
- reload/resume result or deferred seam;
- timing/stability metrics;
- final green/yellow/red classification and caveats.
