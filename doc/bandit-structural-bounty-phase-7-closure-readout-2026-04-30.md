# Bandit structural bounty phase 7 closure readout — 2026-04-30

Parent packet: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`

Ledger ID: `CAOL-BANDIT-STRUCT-BOUNTY-v0`

## Verdict

`CAOL-BANDIT-STRUCT-BOUNTY-v0` is accepted as **CLOSED / CHECKPOINTED GREEN FOR V0**.

The credited closure is not “perfect living ecology forever.” It is the narrower v0 claim:

- bandit camps have structural bounty substrate;
- scan/lead/dispatch/stalk/interest/harvest lifecycle exists;
- deterministic tests cover the core mechanics, persistence, no-repeat, and bounded counters;
- real `do_turn` maintenance wiring exists;
- live feature-path proof shows an idle camp dispatching to ordinary structural forest bounty without player smoke/light/direct-range bait and then harvesting once.

That is enough to close the v0 packet and stop circling the same proof crater like a municipal pigeon.

## Credited evidence

Credited green live run:

- `.userdata/dev-harness/harness_runs/20260430_115157/`

Credited live claims:

- idle bandit camp can select a structural forest bounty without player smoke/light/direct-range bait;
- same-run candidate economics are visible: `bounty=8`, `known_threat=0`, `confidence=3`, `effective_interest=11`, `decision=scavenge`;
- structural dispatch is applied;
- stalking-distance check keeps arrival open;
- arrival harvest occurs;
- `arrivals=1` and `members_returned=1` are reported;
- saved state records member return, no active outing, `status=harvested`, `times_harvested=1`, `bounty=0`, and no immediate repeat state.

Non-credit run:

- `.userdata/dev-harness/harness_runs/20260430_114106/` remains red/inconclusive for closure because the same-run candidate-planning evidence was missing and runtime freshness was not clean enough.

## Phase 7 tuning/readout

The current v0 numbers are accepted as **good enough to close**, not declared final balance:

- forest structural bounty can produce a low/medium scavenge target when the fixture supplies strong enough confidence/bounty;
- known threat currently subtracts from interest only after confirmation;
- the `bounty + confidence - known_threat` shape is deterministic and inspectable;
- deterministic Branch A covers dangerous/lost-interest no-repeat;
- live Branch B covers interest-survives/arrival/harvest.

Future tuning may adjust values, but it is no longer allowed to reopen the v0 lifecycle proof unless a regression is found.

## Regression classification

Existing player smoke/fire/light signal behavior remains represented by its own closed packets and receipts:

- `CAOL-REAL-FIRE-SIGNAL-v0`
- `CAOL-ROOF-HORDE-SPLIT-v0`
- related signal/playback coverage referenced from `TESTING.md` and `doc/work-ledger.md`

No fresh live smoke/light rerun is credited by this closure readout. Instead, this closure classifies smoke/light as a **separate reusable regression gate** for future code changes that touch signal observation, signal mark generation, dispatch scoring, or `do_turn` cadence. The structural-bounty v0 closure is not allowed to claim new smoke/light proof; it only says current structural closure does not supersede or erase those existing receipts.

## Reload-resume decision

Live reload-resume is classified as **optional future proof**, not a blocker for v0 closure.

Reason:

- deterministic Phase 4 tests already cover active structural outing serialization, post-reload stalking/arrival, once-only harvest, harvested scan suppression, and dangerous turnback persistence;
- Phase 6 live proof covers the real feature path without process reload;
- a live process-reload packet would raise confidence but would mostly prove harness/save continuity rather than the core mechanic for v0.

If promoted later, use a separate scenario name such as:

- `bandit.structural_bounty_reload_resume_mcw`

and make it prove process/save continuity, not dispatch all over again.

## Greenlit future playtests / not blockers

These are useful next playtests, but they are no longer closure blockers for `CAOL-BANDIT-STRUCT-BOUNTY-v0`:

1. **Branch A live danger turnback**
   - Scenario: `bandit.structural_bounty_live_danger_turnback_mcw`
   - Prove stalking-distance threat makes effective interest drop, party returns, bounty remains unharvested, lead becomes dangerous/sticky, and no immediate reselection happens.

2. **Live reload-resume**
   - Scenario: `bandit.structural_bounty_reload_resume_mcw`
   - Save before stalking, reload, prove the same outing continues; save after stalking, reload, prove harvest/return happens once.

3. **Two/four-camp wait stress**
   - Scenario family: `bandit.structural_bounty_multicamp_wait_stress_*`
   - Compare wait/pass-time wall clock, structural counters, debug line counts, and active outing counts with one/two/four camps.

4. **Mixed signal coexistence**
   - Scenario: `bandit.structural_bounty_signal_coexistence_mcw`
   - Prove a fresh player smoke/light/human lead can outrank structural work without permanently restoring harvested terrain bounty.

5. **Natural-ish idle world sample**
   - Scenario: `bandit.structural_bounty_natural_idle_sample_mcw`
   - Less blessed fixture, real nearby terrain, watch for dispatch/no-dispatch readout and confirm no silly global scan churn.

## Closure rule going forward

Do not reopen `CAOL-BANDIT-STRUCT-BOUNTY-v0` because someone wants a nicer proof shape. Open a new follow-up packet only when:

- a bug/regression is found;
- Josef explicitly asks for stricter live confidence;
- tuning changes code behavior materially;
- a later feature, such as writhing stalker shared interest, needs a substrate refactor.
