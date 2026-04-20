# Bandit perf + persistence budget probe v0 (2026-04-20)

Status: active greenlit contract.

## Why this exists

Now that the evaluator seam and scenario playback seam are real, we need an honest answer about runtime cost and save bloat before broader rollout.
Otherwise the system will grow a clever little bureaucracy that nobody can afford to run or serialize.

## Scope

1. Measure evaluator-loop cost on the named deterministic scenarios.
2. Track candidate counts, repeated scoring/path checks, and other obvious churn signals.
3. Estimate save-size impact for the bounded bandit camp state that v0 actually needs, including marks, pressure memory, and any explicitly persisted map knowledge or return-state payloads.
4. Compare the same scenarios across short and longer playback horizons so cost drift is visible.
5. Produce a small reviewer-readable budget note instead of a vibes-based claim that the system is probably fine.

## Non-goals

- final optimization pass
- final persistence schema freeze
- sweeping performance work outside the touched bandit seams
- broad live-play theater as the first evidence class

## Success shape

This item is good enough when:
1. at least the first named scenarios have repeatable cost measurements
2. obvious candidate/path/evaluation churn is visible instead of hidden
3. save-size growth has an honest first estimate tied to the actual persisted state shape
4. the packet can say whether the current design looks cheap enough, suspicious, or clearly too bloated

## Validation expectations

- reuse the named deterministic scenarios from the playback suite where possible
- keep the evidence reviewer-readable and repeatable
- escalate to broader rebuild/live proof only if the touched code actually makes that necessary

## Dependency note

This item is queued behind:
1. `doc/bandit-evaluator-dry-run-seam-v0-2026-04-20.md`
2. `doc/bandit-scenario-fixture-playback-suite-v0-2026-04-20.md`

Do not smuggle this measurement lane into the first evaluator patch except for tiny unavoidable counters or minimal persistence hooks.
