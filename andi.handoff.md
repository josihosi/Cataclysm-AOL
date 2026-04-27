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

Before any wider feature claim, run exactly one current-runtime real-fire metadata-gated probe and inspect the bounded report fields:

```sh
python3 tools/openclaw_harness/startup_harness.py probe bandit.live_world_nearby_camp_real_fire_exact_items_tile_audit_mcw
```

Expected evidence artifacts from the run:

- `<run>/probe.report.json`
- `<run>/probe.step_ledger.json`
- `<run>/audit_saved_target_tile_for_fd_fire.metadata.json`

The probe is green for this slice only if startup is clean/current-runtime, the step ledger is green through the saved-map audit step, and `audit_saved_target_tile_for_fd_fire.metadata.json` reports required target-tile `fd_fire` present. If the metadata is missing/unreadable, classify it as red/blocked harness evidence, not as a feature regression and not as feature proof.

## Proof rules to keep in view

- Load/readiness/close is `startup/load`, with `feature_proof=false`.
- `artifacts_matched` is not feature proof unless startup is clean, every step-local ledger row is green, and wait ledgers are not yellow/blocked.
- Screenshot/OCR steps need named expected visible facts; missing guards stay yellow/red/blocked instead of becoming green by silence.
- Exact saved-map state now belongs inside the probe via `kind: "audit_saved_map_tile_near_player"`, with `required_fields` / `required_items` / `required_furniture`; the step writes `<label>.metadata.json` and feeds `metadata_expectation` into `probe.step_ledger.json`.
- The still-red dialogue/rules selector primitive remains untrusted unless a tiny menu-entry/hotkey metadata capture is added. Do not start a key-variant loop around it.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit target.

## Schani handoff shape

Keep the final handoff concise: active item, what changed/validated, exact evidence artifacts, remaining state/blocker, and recommended next review/action. Routine progress stays agent-visible only unless genuinely parked/blocked/decision-needed.
