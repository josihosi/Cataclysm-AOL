# CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

Status: GREENLIT / BACKLOG / ANTI-REDUNDANCY PACKAGE

Imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

## Summary

Reduce the writhing stalker's bespoke monster-planning seam without changing the approved fair-dread behavior. The target is not a tuning pass; it is architectural cleanup around the current `mon_writhing_stalker` special path so more of the behavior is expressed through reusable monster behavior, strategy, predicate, or special-attack seams instead of living as a private exception in `monmove.cpp`.

## Scope

- Inventory the current live path from `monster::plan()` through `apply_writhing_stalker_plan()` and `writhing_stalker::evaluate_live_response()`.
- Identify which parts are truly stalker-specific product judgment and which are reusable monster behavior decisions.
- Move or wrap reusable pieces behind existing monster behavior/strategy/special-attack plumbing where a narrow seam exists.
- Keep the current no-evidence, cover preference, light/focus counterplay, vulnerability strike, cooldown, repeated-strike, and injured-retreat behavior green.
- Leave clear comments/tests naming any custom part that remains custom because no existing seam honestly fits.

## Non-goals

- Do not retune the stalker's product fantasy, stats, spawn rate, or flavor.
- Do not reopen closed writhing-stalker v0 or live fun-scenario claims.
- Do not create a broad monster-AI framework rewrite.
- Do not hide behavior changes inside a “refactor” commit.

## Success state

- [ ] A short code audit note or commit message names the existing seam(s) reused and the bespoke code reduced.
- [ ] The live-facing monster path still reaches the writhing-stalker evaluator or its replacement through a named, test-covered seam.
- [ ] Focused `[writhing_stalker]` tests remain green, including no-omniscience, light/focus counterplay, cooldown anti-spam, repeated strikes, and injured retreat.
- [ ] At least one new/updated test proves the refactored seam is consumed by the live planner rather than only by a detached helper.
- [ ] No gameplay tuning claim is made unless a separate proof explicitly supports it.

## Targeted tests

- Focused deterministic gate: `./tests/cata_test "[writhing_stalker]"`.
- Seam-consumption test: a narrow test that fails if `monster::plan()` no longer routes the stalker through the intended behavior seam.
- Regression rows for no-evidence/no-beeline, cover route preference, bright/focused counterplay, vulnerability strike, cooldown, repeated strike rhythm, and injured retreat.

## Cautions

The current bespoke path is not garbage; it carries approved product behavior. Cut the duplicate wiring, not the dread. If the existing behavior infrastructure cannot express a piece honestly, keep that piece custom and make the boundary explicit.

## Relationship to predator behavior variety package

`CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0` is a product behavior change. This anti-redundancy package should preserve whichever stalker behavior is current and approved when it executes. If the zombie-shadow predator package runs first, this refactor preserves that newer quiet-side/zombie-pressure behavior, not the older hit-and-dash-adjacent shape.
