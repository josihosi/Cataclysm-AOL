# Plan/Aux pipeline helper (2026-04-09)

Status: checkpointed tooling contract

This packages Josef's request for a small helper around the `Plan.md` / auxiliary-doc workflow.

## Problem

Once a lane or idea is already understood, greenlighting it should not require slow manual canon carpentry every time.

Right now the workflow is correct but clunky:
- discuss the item
- print the contract back
- collect corrections
- classify it
- patch multiple canon files by hand
- optionally produce an Andi handoff packet

That is leverage-heavy but still too manual.

## Goal

Build a small helper that preserves the frozen workflow while making the packaging step faster and more consistent.

## Required workflow shape

The helper should support this sequence:
1. take a proposed item, greenlight, or parked-lane request
2. print the contract back for verification
3. collect corrections before touching canon files
4. ask the final classification question:
   - active
   - parked
   - bottom-of-stack
5. patch the relevant canon files consistently
6. optionally generate the Andi handoff packet too

## Constraints

- preserve the frozen intake/classification/packaging workflow
- do not skip the contract-verification step
- reduce manual canon edits without turning the workflow into hidden magic
- keep active vs parked vs bottom-of-stack distinctions explicit
- optimize for speed, consistency, and lower clerical drag

## Suspicion-first audit result

The repo already had:
- stable file-role truth for roadmap / queue / success / testing in `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md`
- repeated aux-doc patterns that are regular enough to template

The repo did **not** already have:
- a repo-local helper for contract preview
- an explicit correction-merge step
- a canon/snippet packaging helper
- a bounded in-place patch path for the existing canon headings

That made a spec-first helper the smallest honest shape.
Pretending broad freeform canon mutation was already solved would have been fake progress.

## Current landed helper

Repo-local tool:
- `tools/plan_aux_pipeline_helper.py`

Landed commands:
- `schema` — prints the normalized JSON contract shape for this repo
- `show` — prints repo config, contract preview, and patch matrix before canon edits
- `merge-corrections` — deep-merges an explicit corrections file into the reviewed contract
- `emit` — writes reviewer-visible aux/canon snippet files from the same classified contract and can now also emit a terse `andi.handoff.md` packet when `handoff_needed` is `yes`
- `apply` — patches the auxiliary doc plus known existing `Plan.md` / `TODO.md` / `SUCCESS.md` / `TESTING.md` anchors in place

Current helper shape keeps the workflow visible and reviewable:
- if the heading already exists on the known repo shape, `apply` can patch it in place
- if the heading does **not** exist, the honest fallback stays `emit` output for review/paste instead of magical guessing
- Andi handoff output is downstream-only, derived from the same frozen contract, and does not widen canon mutation
- the tool still does not pretend to solve broad arbitrary canon mutation

## Current proof

The bounded patch path plus the downstream handoff output have now been validated narrowly on a temp repo copy through:
- `python3 -m py_compile tools/plan_aux_pipeline_helper.py`
- `python3 tools/plan_aux_pipeline_helper.py schema`
- `python3 tools/plan_aux_pipeline_helper.py show <spec.json>`
- `python3 tools/plan_aux_pipeline_helper.py emit <spec.json> --out-dir <dir> --force`
- review of emitted `andi.handoff.md` from that same sample spec
- `python3 tools/plan_aux_pipeline_helper.py apply <spec.json> --root <temp_repo_copy>`

## Success shape

A good current version of this tool now means:
- already-understood greenlights are faster to package
- contract verification and corrections stay explicit
- classification stays explicit
- one contract can already drive consistent aux/canon snippet generation, optional Andi handoff output, and bounded known-heading patching
- the workflow stays visible instead of mutating into hidden file-carpentry magic

## Non-goal

This is not a replacement for the discussion/classification workflow itself.
It is a helper for the packaging step after that thinking is done.
