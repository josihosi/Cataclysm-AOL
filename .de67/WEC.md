# WEC

*User intent and language brief*

## User outcome

Agents can run trustworthy bandit and cannibal harness scenarios without waiting through long monolithic runs only to discover a missing proof obligation or invisible final state.

The harness should detect invalid contracts early, explain the first causal failure, preserve completed work through checkpoints, and produce authoritative evidence across a verified checkpoint chain.

## Intended experience

Before launch, the harness checks that the scenario’s proof route is coherent and observable.

During execution:

- Successful named proof gates create checkpoints.
- Structured transition events explain product progress.
- Transport actions and keypresses are supporting actions, not individual proof obligations.
- Slow runs expose game-turn progress, wall time, resource use, and useful diagnostics.
- A failure report leads with the first causal divergence, its expected and observed states, and the latest valid checkpoint.

After interruption or failure, the harness recommends the latest valid checkpoint and explains it. The agent decides whether to resume it, choose another checkpoint, or rerun.

A verified chain of resumed segments may collectively certify the scenario. Any relevant code, data, or scenario change invalidates the whole certification chain.

## Project language and terminology

Use these terms consistently:

- **Contract preflight** — validation performed before launching the game.
- **Proof gate** — a named, causally meaningful contract boundary.
- **Checkpoint** — captured state and evidence after a successful proof gate.
- **Checkpoint chain** — ordered segments that collectively prove the scenario.
- **Diagnostic run** — execution intended to locate and explain divergence.
- **Certification chain** — verified execution segments that satisfy final proof.
- **First divergence** — the earliest failed causal expectation.
- **Structured transition event** — machine-readable evidence emitted by product or harness state changes.
- **Incidental-hostile suppression** — the non-combat harness facility informally called “autokill.”
- **Actor receipt** — evidence identifying every entity affected by suppression.

Avoid describing ordinary input delivery as failed proof merely because it produced no immediate artifact.

## Boundaries

Current behavior under test is limited to bandits and cannibals.

Writhing-stalker behavior, zombie-rider behavior, and the later hardening of production perception logic are outside this round. The known writhing-stalker interaction with debug clairvoyance does not create a current harness exception.

The harness must not refuse causally unchanged reruns. It may retain history, explain similarities, and recommend actions, but agents retain authority to rerun.

Current stabilizer policy:

- `DEBUG_LS` and `DEBUG_NOTEMP` are mandatory everywhere.
- `DEBUG_STAMINA` and `DEBUG_CARDIO` are mandatory for non-combat scenarios.
- `DEBUG_CLAIRVOYANCE` and `DEBUG_NIGHTVISION` are standard observer-character traits.
- Vision-sensitive product hardening is later work.

Incidental-hostile suppression is non-combat only. It must affect only eligible nearby incidental hostiles, exclude ecology actors under test, fail closed when identity is ambiguous, avoid ordinary combat/death side effects, and emit actor receipts.

## Decisions

- Checkpoint chains may count as final certification.
- Any relevant code, data, or scenario change resets the entire certification chain.
- The harness recommends a resume point; the agent chooses.
- Every successful named proof gate creates a checkpoint.
- Observer traits remain standard; production perception hardening is deferred.
- The harness informs agents but does not restrict trusted reruns.
- Structured events and gate-level evidence replace log-substring archaeology and per-keypress proof accounting.

## Prototype or reaction questions

Concrete prototypes should resolve:

- How the report presents the first divergence, completed gates, and recommended checkpoint.
- How checkpoint-chain lineage and invalidation are shown clearly.
- How diagnostic and certification states are distinguished without adding operator friction.
- How incidental-hostile receipts appear in the report.

## Handoff to DE-67-2

The working repository is `/Users/josefhorvath/Schanigarten/Cataclysm-AOL-hostile-ecology-dev`, branch `dev`, observed at `7f4697ee6b17fb897461e3ceb290342b83787a30`. It is heavily dirty; preserve all existing work.

Primary artifacts:

- `.agents/skills/caol-harness/SKILL.md`
- `.de67/work-ledger.md`
- `tools/openclaw_harness/scenarios/bandit.scout_to_decision_observer_live_mcw.json`

The latest inspected run lasted about 18 minutes and failed at homeward materialization because the loaded bubble lacked paired entry or staging positions. Its large report also treated many successful transport actions as yellow and repeated the same diagnostic thousands of times. DE-67-2 should preserve this as the motivating failure shape.

No repository files were changed. This completes DE67 phase 1 only.
