# Andi handoff: CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

## Current canon state

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is **CLOSED / CHECKPOINTED GREEN V0**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` aligned downstream. This handoff is now a terse closure packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

`CAOL-ROOF-HORDE-NICE-FIRE-v0`, old `CAOL-WRITHING-STALKER-v0`, Smart Zone, old fire proof lanes, and this gauntlet must not be reopened without explicit Schani/Josef promotion.

Queued-next visibility: `CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` is greenlit behind the closed gauntlet, with contract `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`. Do **not** switch to it unless Schani/Josef explicitly promote it to active; it is listed here so the next lane does not disappear.

## Canonical packet

- Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`
- Imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`
- Proof: `doc/multi-camp-signal-gauntlet-proof-v0-2026-04-30.md`
- Prior structural-bounty closure footing: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`
- Prior green structural-bounty run: `.userdata/dev-harness/harness_runs/20260430_115157/`

## Closed challenge rows

### Challenge A — multi-camp structural stress

- Scenario: `bandit.multi_camp_structural_stress_mcw`
- Green run: `.userdata/dev-harness/harness_runs/20260430_204416/`
- Result: two clean camps dispatched separate structural bounty leads through bounded time; fixture leads harvested; west camp returned idle; east camp immediately followed through on a fresh structural lead without repeating the harvested fixture target.
- Caveat: this proves harvested/no-repeat/followthrough, not all-camps-idle after harvest.

### Challenge B — mixed signal coexistence

- Scenario: `bandit.mixed_signal_coexistence_mcw`
- Green run: `.userdata/dev-harness/harness_runs/20260430_203757/`
- Result: live smoke/fire signal camp chose `candidate_reason=live_signal` while separate structural camp dispatched its structural bounty in the same bounded run; saved state preserved both live-signal and structural-bounty classes.
- Caveat: staged-but-live signal source, not natural unseeded discovery proof.

### Challenge C — reload/resume continuity

- Scenario: `bandit.mixed_signal_reload_resume_mcw`
- Green run: `.userdata/dev-harness/harness_runs/20260430_203944/`
- Result: no-fixture reload preserved one active live-signal scout and one active structural scavenge group; bounded resume wait/save kept both groups stable without disappearance, duplication, or stale target loss.

## Stability readout

Final green runs had green step ledgers and green wait ledgers. No dogpile, stale-state, reload-loss, CPU churn, log spam, debug ERROR/WARNING flood, or crash failure was observed. See proof doc for metrics and exact state excerpts.

## Non-goals/cautions

- Do not reopen closed lanes casually.
- Do not call setup-only camp/signal presence a challenge result.
- Do not hide dogpile, reload loss, stale state, CPU churn, or log spam behind green wording.
- If a future stricter challenge breaks, preserve the red/yellow evidence and exact blocker. A truthful ugly report beats pretty nonsense.
