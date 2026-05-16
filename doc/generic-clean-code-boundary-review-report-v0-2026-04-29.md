# Generic clean-code boundary review report v0 - 2026-04-29

Scope: report-only boundary review after `Smart Zone Manager harness-audit retry packet v0` moved to implemented-but-unproven / Josef playtest package.

Repo state inspected: branch `dev`, active Plan lane `Generic clean-code boundary review packet v0`, Smart Zone checkpoint docs dirty, `src/handle_action.cpp` dispatch trace instrumentation dirty, new `tools/openclaw_harness/scenarios/smart_zone.ui_entry_current_runtime_guard.json` dirty.

## Findings

### fix now

1. `andi.handoff.md` was stale: it still said `Smart Zone Manager harness-audit retry packet v0` was active and kept `Generic clean-code boundary review packet v0` queued, while `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` now make Smart Zone a Josef-package checkpoint and Generic review active. This was corrected in the same checkpoint because stale handoff text would mislead the next agent.

2. Checkpoint the Smart Zone boundary cleanly before starting another feature lane. The current diff is bounded and reviewable: docs/canon demotion, gated `OPENCLAW_HARNESS_UI_TRACE` action-dispatch logging, the minimal `smart_zone.ui_entry_current_runtime_guard` scenario that produced the final red evidence, and this boundary report/handoff alignment. Leaving it dirty would make the next feature lane inherit proof-boundary archaeology.

### queue

1. Decide whether to keep or later remove the gated `src/handle_action.cpp` action-dispatch trace hook. It is useful if Smart Zone UI entry is reopened, and it is inert unless `OPENCLAW_HARNESS_UI_TRACE` is set, but it is support instrumentation rather than gameplay behavior.

2. If Smart Zone is reopened, fix the actual key-delivery primitive before any more UI proof attempts. The final run shows default `Y` dispatching as `action_menu`, not `zones`; more Zone Manager macros without a repaired primitive should stay blocked.

### ignore/watch

1. No broad build/test gate is needed for this report-only review beyond the gates already run for the touched Smart Zone instrumentation (`git diff --check`, scenario JSON validation, and `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`). Run broader tests only if a promoted follow-up changes gameplay behavior.

2. Current canon no longer shows a priority-order contradiction: `Plan.md` active/greenlit summary points to Generic review, with `C-AOL actual playtest verification stack v0` next. The old Smart Zone active text now only survives in `andi.handoff.md`.

## Recommendation

Commit the Smart Zone boundary + this report/handoff alignment, then let Schani review whether to keep the gated dispatch trace or queue its removal. Do not reopen Smart Zone live proof without a materially repaired UI-entry/key-delivery primitive or Josef manual evidence.
