# Generic clean-code boundary review report v0 (2026-04-28)

## Inspected repo state

- Repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL`
- Branch: `dev`
- Base HEAD at inspection: `1f2054cd8e`
- Dirty tree at inspection: docs/ledger-only changes in `Plan.md`, `SUCCESS.md`, `TESTING.md`, `TODO.md`, `andi.handoff.md`, and the bandit checkpoint docs.
- Inputs inspected: `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `andi.handoff.md`, `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`, `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`, this packet contract, current `git status`, focused `git diff --stat`, stale-text searches, and `python3 tools/plan_status_summary.py /plan active` / `/plan greenlit`.

## Gate posture

- Ran: `python3 tools/plan_status_summary.py /plan active` and `/plan greenlit`; both reported no warnings after the status wording repair.
- Ran: `git diff --check`; passed with no whitespace errors.
- Did not run compile/build/test: this was a report-only/docs boundary review and no source/test/harness code was changed during the review. Existing recorded bandit gates remain the checkpoint evidence.

## Findings

### fix now

1. **`Plan.md` greenlit-order tail is stale after the bandit/generic promotion.**
   - Anchors: `Plan.md:117-159` correctly says bandit is closed and generic boundary review is active, but `Plan.md:232-233` still says `Bandit camp-map risk/reward dispatch planning packet v0 — promoted to the active lane above` and `Generic clean-code boundary review packet v0 — greenlit / queued boundary review`.
   - Risk: a later Andi run may follow the numbered backlog text instead of the active section and think the closed bandit lane is still active or the already-active generic review is still queued.
   - Bounded follow-up: after report review, rewrite only those two backlog entries to say bandit is closed/scoped green and generic review has produced this report; keep Smart Zone / actual-playtest ordering from canon rather than inventing a new priority.

2. **Bandit contract doc now says closed, but its original success checklist still reads like an open broad closure checklist.**
   - Anchors: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md:3` and `:9-11` mark the packet closed/scoped green, while `:181-190` still contains unchecked broad success bullets and `:187` still says `broader product closure remains`.
   - Risk: reviewers can read the file two ways: scoped checkpoint accepted by Frau, or broad product checklist still incomplete. That is exactly the kind of closure ambiguity this boundary review is meant to catch.
   - Bounded follow-up: either convert the old checklist into an explicitly historical/full-ambition checklist, or replace it with the scoped checkpoint checklist now mirrored in `SUCCESS.md`; preserve the caveats: no player-fire credit, no second-fixture anti-bias claim, no opening-present escalation claim.

### queue

3. **Close the generic boundary-review ledger only after this report is reviewed/accepted.**
   - Anchors: `SUCCESS.md:192-207` still says the generic review is `GREENLIT / QUEUED` with unchecked report-success bullets; `TODO.md:12-27` and `TESTING.md:56-62` correctly say the active target is to produce this report.
   - Risk: once this report exists, those ledgers are intentionally one step behind until Schani/Josef accepts either report-only closure or a small follow-up patch.
   - Bounded follow-up: mark the generic review as report-produced/report-ready, then select the next canon item without applying cleanup fixes silently.

4. **Local style gate still has an environment blocker.**
   - Anchors: `Plan.md:146` and `TESTING.md:73` record `make astyle-diff` as locally blocked because `astyle` is not installed.
   - Risk: not a source correctness blocker, but repeated checkpoints will keep carrying the same partial style posture until the environment is fixed or the project decides to stop naming that local gate.
   - Bounded follow-up: queue an environment/tooling note or install/fix `astyle` separately; do not block the docs-only boundary on it.

### ignore/watch

5. **No source build/test rerun needed for this report-only pass.**
   - Anchors: dirty tree contains docs/ledger files only; `git diff --stat` shows no `src/`, `tests/`, or harness code files changed by this boundary review; `git diff --check` passed.
   - Watch condition: if the follow-up patch edits code, tests, harnesses, or generated fixtures, run the smallest relevant gate then.

6. **`TODO.md` names Smart Zone as the next narrow queued work while `Plan.md` keeps the broader actual-playtest stack wrapper.**
   - Anchors: `TODO.md:22-33` says the next narrow work is this report and later queue starts with Smart Zone; `Plan.md:224-234` wraps that under `C-AOL actual playtest verification stack v0`, with fuel continuation preserved as an already-red boundary at item 1 and Smart Zone as item 2.
   - Current verdict: acceptable if the next handoff says Smart Zone is the first unblocked proof retry inside the actual-playtest stack, not a priority reshuffle.

## Recommended next action

Schani/Josef should review this report and either approve one small ledger/doc cleanup patch for the two `fix now` items, or explicitly accept the current ambiguity and proceed. Frau nudge is not needed unless product wants to challenge the scoped bandit closure verdict; this report found documentation drift, not a new behavior/proof dispute.
