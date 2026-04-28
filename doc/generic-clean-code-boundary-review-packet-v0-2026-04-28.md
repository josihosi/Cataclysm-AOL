# Generic clean-code boundary review packet v0 (2026-04-28)

Status: greenlit / queued boundary review.

Imagination source of truth: `doc/generic-clean-code-boundary-review-imagination-source-of-truth-2026-04-28.md`.

## Why this exists

A long C-AOL push can leave small contradictions, stale queue text, build/test hazards, and half-useful cleanup ideas scattered around the repo. A fresh clean Code/agent pass is useful, but only at a checkpoint boundary. Mid-lane generic cleanup is how a helpful goblin rearranges the pantry while the soup is on fire.

This packet preserves the useful version: a report-first boundary review after the current active lane reaches a real checkpoint, before the next lane is promoted.

## Scope

1. Start from current canon, not memory: `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `andi.handoff.md`, and the active lane's canonical docs.
2. Inspect repo health for concrete contradictions, stale TODO/canon drift, obvious build/test/lint hazards, and small cleanup risks.
3. Check the current dirty tree / recent checkpoint boundary before making claims.
4. Produce a short report with exact file/path/test anchors for every real finding.
5. Classify each finding as:
   - `fix now`
   - `queue`
   - `ignore/watch`
6. Recommend only bounded follow-up patches; do not apply them until Schani/Josef reviews the report or explicitly promotes a fix.

## Non-goals

- no feature work
- no active-lane interruption
- no release publishing
- no broad refactor or architecture rewrite
- no priority reshuffle by vibes
- no private workspace/setup cleanup outside the C-AOL repo unless separately requested
- no edits during the first pass unless explicitly authorized

## Success state

This packet is good enough when:

1. One compact boundary-review report exists and names the repo state it inspected.
2. The report checks canon consistency across `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `andi.handoff.md`.
3. The report checks build/test/lint risk at the appropriate level for the current boundary, or explains why a gate was not run.
4. Findings are concrete, anchored, and classified as `fix now`, `queue`, or `ignore/watch`.
5. No feature lane is reopened, promoted, or deprioritized without explicit Schani/Josef review.
6. Any proposed edits are bounded follow-up items, not silent cleanup drift.

## Timing and priority

Run this only after the current active bandit camp-map lane reaches a real checkpoint or Schani/Josef explicitly promotes the boundary review. It should normally happen before starting the next major queued lane, because its purpose is to keep the floor clean between work packets.

## Suggested first-pass command shape

The exact agent/runtime can vary, but the brief should stay close to:

> Fresh clean-code boundary review for Cataclysm-AOL. Read current `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `andi.handoff.md`, current git status/diff, and the active lane docs. Produce a report only. Look for canon contradictions, stale TODOs, obvious build/test/lint hazards, and small cleanup risks. Do not change files. Classify findings as fix now / queue / ignore-watch. Do not invent feature work or reorder priorities.

## Evidence expectations

- Report-only first pass.
- If the report claims build/test risk, it should name the gate it inspected or ran.
- If it claims canon drift, it should quote or cite the conflicting headings/sections.
- If later promoted to edits, those edits need the smallest appropriate validation gate for their file class.
