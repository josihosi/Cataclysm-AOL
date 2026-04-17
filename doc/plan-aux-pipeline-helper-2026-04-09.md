# Plan/Aux pipeline helper (2026-04-09)

Status: parked future tooling

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

## Files it should be able to update

At minimum, the helper should know how to patch:
- `Plan.md`
- `TODO.md`
- `SUCCESS.md`
- `TESTING.md` when needed
- the auxiliary doc itself

## Constraints

- preserve the frozen intake/classification/packaging workflow
- do not skip the contract-verification step
- reduce manual canon edits without turning the workflow into hidden magic
- keep active vs parked vs bottom-of-stack distinctions explicit
- optimize for speed, consistency, and lower clerical drag

## Success shape

A good v1 of this tool would:
- make already-understood greenlights fast to package
- reduce manual editing errors and missed files
- keep canon updates structurally consistent
- optionally emit a ready-to-send Andi instruction packet from the same classified item

## Non-goal

This is not a replacement for the discussion/classification workflow itself.
It is a helper for the packaging step after that thinking is done.
