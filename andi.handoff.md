# Andi handoff: C-AOL harness trust audit + proof-freeze v0

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`
- inventory/proof matrix: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`
- current harness entry point: `tools/openclaw_harness/startup_harness.py`

## Current active lane

Active lane is `C-AOL harness trust audit + proof-freeze v0`.

Do **not** steer back into the older `C-AOL debug-proof finish stack v0`; that stack reached its honest Schani-review/implemented-but-unproven boundaries where needed. Current work is a primitive trust audit: make the harness unable to turn load-and-close, wrong-screen key macros, stale runtime, or missing metadata into feature proof.

## Current next evidence target

Current blocker: `blocked_untrusted_brazier_deploy_selector`, now narrowed to the fixture-to-live inventory availability gap behind `filtered_brazier_selected`.

Do **not** run the older real-fire metadata-gated probe as the next step, and do **not** press `CONFIRM` / `RIGHT` in the deploy chain until the live selector row is proven. The current evidence target is one question:

> Why does saved `player.inv` contain `brazier`, while the live `Use item` activatable selector exposes only `smart_phone` before filtering and zero visible entries after filter `brazier`?

Current evidence anchors:

- `.userdata/dev-harness/harness_runs/20260427_203847/audit_saved_inventory_has_brazier_before_apply.metadata.json` proves saved `player.inv` has `brazier=1` and item metadata says `deploy_furn -> f_brazier`.
- `.userdata/dev-harness/harness_runs/20260427_203847/probe.artifacts.log` / `audit_ui_trace_filtered_brazier_selected.log_delta.txt` prove the live `Use item` selector list shows only `smart_phone` before filtering and zero entries after filter `brazier`.
- The run aborts at `audit_ui_trace_filtered_brazier_selected` before `CONFIRM`, `Deploy where?`, `RIGHT`, save, or east-tile `f_brazier` can be credited.

Recommended next action: inspect fixture install/load path, active avatar inventory ownership, and `activatable_inventory_preset`/selector predicate behavior until the saved-vs-live availability mismatch has a source-backed explanation or repair. Then rerun the guarded selector audit only up to `filtered_brazier_selected`.

## Proof rules to keep in view

- Load/readiness/close is `startup/load`, with `feature_proof=false`.
- `artifacts_matched` is not feature proof unless startup is clean, every step-local ledger row is green, and wait ledgers are not yellow/blocked.
- Screenshot/OCR steps need named expected visible facts; missing guards stay yellow/red/blocked instead of becoming green by silence.
- Exact saved-map state now belongs inside the probe via `kind: "audit_saved_map_tile_near_player"`, with `required_fields` / `required_items` / `required_furniture`; the step writes `<label>.metadata.json` and feeds `metadata_expectation` into `probe.step_ledger.json`.
- The still-red brazier deploy selector primitive remains untrusted until live selector availability, selected row, deploy prompt, right/east consumption, and saved east-tile `f_brazier` are each proven by step-local evidence. Do not start a key-variant loop around it.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit target.

## Schani handoff shape

Keep the final handoff concise: active item, what changed/validated, exact evidence artifacts, remaining state/blocker, and recommended next review/action. Routine progress stays agent-visible only unless genuinely parked/blocked/decision-needed.
