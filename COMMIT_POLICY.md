# COMMIT_POLICY

Cataclysm-AOL needs checkpoint commits, not an endless heroic dirty tree.

This file exists to prevent repo soup.

## Core rule

Commit when a feature reaches a **real state boundary**, not only when the whole grand idea is finished.
A real state boundary is something like:
- a new behavior slice landed and passes the relevant narrow checks
- a probe/instrumentation improvement landed and now makes the next step possible
- a blocker fix landed and the blocked path is now testable again
- a documentation/ledger rewrite changed the active target or testing truth materially

Do **not** wait until fifteen files and three half-features are tangled together.

## Dirty-tree budget

If the tree starts feeling soupy, stop adding new behavior and checkpoint it.
As a practical rule, create or split a commit before continuing when any of these becomes true:
- the dirty tree spans multiple unrelated concerns
- code + ledger/doc churn are mixed together unclearly
- the diff is large enough that review is annoying instead of informative
- a file has broad formatting churn unrelated to the behavior change
- Josef would have trouble understanding what changed from a summary plus `git show`

This is a judgment call, not a magic number, but “one giant mudball” is explicitly not acceptable.

## Preferred commit shapes

### 1. Behavior commit
Contains:
- the code change
- the tests directly needed for that code change
- the smallest durable mechanic note if needed in `TechnicalTome.md`

Avoid bundling broad roadmap rewrites into this commit unless the behavior genuinely changed the roadmap.

### 2. Ledger/doc alignment commit
Contains only:
- `Plan.md`
- `SUCCESS.md`
- `TODO.md`
- `TESTING.md`
- maybe a small related note/doc file

Use this when the truth of the active target or test packet changed, but no further code change is required.
Do **not** make one of these every run just because you touched the files.

### 3. Probe/instrumentation commit
Contains only the temporary or support work needed to make the next proof possible:
- logging
- harness tweaks
- fixture helpers
- tiny repro hooks

If later removed, remove it deliberately in a later commit.
Do not hide this kind of change inside an unrelated behavior diff.

## What not to do

- Do not keep appending docs and code to the same uncommitted tree across many cron runs.
- Do not mix unrelated movement/Basecamp/locker work in one checkpoint commit.
- Do not rewrite `Plan.md`, `TODO.md`, and `TESTING.md` after every tiny rerun if the state did not materially change.
- Do not use Josef handoff timing as an excuse to keep everything uncommitted.
- Do not let formatting churn dominate a behavior diff.

## Before asking Josef to test

Prefer reaching a reviewable checkpoint first.
Josef should be testing a legible state, not an archaeological site.
That means ideally:
- the current slice is committed, or at least cleanly split and obviously bounded
- the relevant agent-side checks already passed
- the ask is batched with any other nearby human-only judgment if practical

## Minimum pre-commit discipline

Before committing a behavior/probe checkpoint:
- run `git diff --check`
- run the narrowest honest validation for the slice
- make sure `Plan.md`, `TODO.md`, and `TESTING.md` reflect the new truth if the active state actually changed

## CI checkpoint / linking rule

For any reviewable code checkpoint that could affect GitHub Actions, the handoff or commit notes must name:
- the changed file class, for example source, tests, workflow, package script, harness, or docs-only
- the narrow local gate actually run
- the broader GitHub Actions workflow/run link when available
- any remaining red check and whether it is classified as source, workflow/package, timeout/resource, external runner, or intentionally failing guardrail

Do not write “tested” when the relevant CI-shaped gate is merely hoped for.  Link the gate.  Name the red thing.  This is bureaucracy only in the sense that a kitchen has labels so nobody serves floor cleaner as soup.

## Interaction with Plan / SUCCESS / TODO / TESTING

- `Plan.md` tells you what the active target is.
- `SUCCESS.md` says what "done" means for that target and what remains unchecked.
- `TODO.md` is the short queue for moving that target forward.
- `TESTING.md` says what evidence still matters.
- `COMMIT_POLICY.md` tells you when to checkpoint the work instead of letting it rot in the tree.

If the active target changes, or the blocker changes, or the missing evidence changes, update the docs.
If none of those changed, you probably do **not** need another doc edit commit.
