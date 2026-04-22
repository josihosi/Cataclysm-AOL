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

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.

---

## Current relevant evidence

No probe obligation is active right now.
The repo is in checkpointed waiting state.

### Latest closed lane - Live bandit + Basecamp playtesting feasibility probe v0

- canonical packet: `doc/live-bandit-basecamp-playtesting-feasibility-probe-v0-2026-04-21.md`
- fresh current-build startup proof lives under `.userdata/dev/harness_runs/20260422_002122/`, with build head `5af2fb80d8-dirty`, `version_matches_repo_head = true`, and `version_matches_runtime_paths = true`
- bounded live restage proof lives under `.userdata/dev-harness/harness_runs/20260422_002329/`, showing named NPC debug spawn of hostile `Stefany Billings, Bandit`
- honest verdict: current-build bandit + Basecamp live playtesting is practical now, but reviewer-clean packaged overmap-bandit harness footing is still absent

### Latest closed lane - Basecamp AI capability audit/readout packet v0

- canonical packet: `doc/basecamp-ai-capability-audit-readout-packet-v0-2026-04-21.md`
- honest verdict: the player-facing spoken Basecamp surface is still a narrow deterministic craft-request plus board/job router, while the richer prompt-shaped layer is mostly snapshot/planner packaging rather than core spoken command interpretation
- no compile or live probe was required because this was a current-tree capability readout, not a fresh runtime behavior claim

### Latest closed lane - Locker Zone V3

- canonical packet: `doc/locker-zone-v3-reopen-packet-v0-2026-04-21.md`
- honest verdict: hot/cold outerwear bias, hot/cold legwear bias, moderate-temperature seasonal dressing, rainy moderate-weather rainproof preference, and bounded worker-specific wardrobe preservation now all land on the same real locker seam
- current focused validation: `make -j4 tests` and `./tests/cata_test "[camp][locker]"`

### Closed bandit readiness train

Use the auxiliary packet docs for the detailed proof shape.
The canonical closed packets are:
- `doc/bandit-overmap-local-handoff-interaction-packet-v0-2026-04-21.md`
- `doc/bandit-elevated-light-and-z-level-visibility-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-benchmark-suite-packet-v0-2026-04-21.md`
- `doc/bandit-weather-concealment-refinement-packet-v0-2026-04-21.md`
- `doc/bandit-overmap-local-pressure-rewrite-packet-v0-2026-04-21.md`
- `doc/bandit-long-range-directional-light-proof-packet-v0-2026-04-21.md`
- `doc/bandit-bounded-scout-explore-seam-v0-2026-04-21.md`
- `doc/bandit-repeated-site-revisit-behavior-packet-v0-2026-04-21.md`
- `doc/bandit-scoring-refinement-seam-v0-2026-04-21.md`
- `doc/bandit-moving-bounty-memory-seam-v0-2026-04-21.md`

Current honest summary:
- the playback/proof surface is now checkpointed through the handoff, visibility, benchmark, weather, pressure-rewrite, long-range-light, scout/explore, moving-memory, scoring, and repeated-site packets
- the bandit proof family has current deterministic coverage on the tree, with `./tests/cata_test "[bandit]"` as the broad filtered confidence pass when code in that family changes
- no further rerun is warranted until a fresh greenlight or contradictory evidence appears

---

## Pending probes

No active probe obligation is greenlit right now.

- Do not rerun the live-feasibility packet ceremonially.
- Wait for a fresh greenlight before widening into packaging work or another live pass.
- If the live-feasibility lane is deliberately reopened later, the next honest question is a packaging/helper packet for the named-bandit restage seam, not another generic feasibility lap.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow locker deterministic pass after a code slice lands
- `make -j4 tests`
- `./tests/cata_test "[camp][locker]"`

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Bandit dry-run evaluator seam foundation
- `make -j4 tests`
- `./tests/cata_test "[bandit][dry_run]"`

### Fresh tiles rebuild only if a later combat-policy handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
