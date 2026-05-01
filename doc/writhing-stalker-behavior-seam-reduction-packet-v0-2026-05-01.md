# CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

Status: ACTIVE / GREENLIT / ANTI-REDUNDANCY PACKAGE / FIRST REFACTOR NEXT

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

## Current seam inventory checkpoint

Live path today:
- `monster::plan()` first builds `monster_plan` target state, then calls `apply_writhing_stalker_plan()` before the generic hostile/fleeing destination fallback and before the later `monster::move()` behavior-tree/special-attack tick.
- `apply_writhing_stalker_plan()` is currently a narrow `mon_writhing_stalker` adapter: it gathers map/creature facts with `writhing_stalker_live_context()`, delegates product judgment to `writhing_stalker::evaluate_live_response()`, logs the decision, and translates the response into `set_dest()` / `unset_dest()` / `effect_run` side effects.
- Shadow destination selection still has product-specific zombie-shadow judgment in `writhing_stalker_shadow_destination()`: nearby zombie pressure is converted into quiet-side candidates through `writhing_stalker::choose_quiet_side_cutoff()`.
- Existing reusable seams available in this path are `mtype::get_goals()` / `behavior::tree` / `behavior::monster_oracle_t` predicates and the `type->special_attacks` action dispatch in `monster::move()`. These can honestly carry availability/dispatch gating or a thin named special-action adapter.
- Product-specific pieces that should stay custom unless a better seam appears: local-evidence/no-omniscience confidence, quiet-side/cutoff scoring, light/focus counterpressure, cooldown/repeated-strike rhythm, and injured retreat.

Next refactor target: shrink the private `monmove.cpp` planning exception into a thinner named adapter around existing behavior/special-attack dispatch where possible, while leaving the stalker evaluator and quiet-side scorer explicit and test-covered.

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
