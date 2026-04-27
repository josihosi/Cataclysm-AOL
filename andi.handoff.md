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

Do **not** steer back into the older `C-AOL debug-proof finish stack v0`; that stack reached its honest Schani-review/implemented-but-unproven boundaries where needed. Current work is a primitive trust audit: make the harness unable to turn load-and-close, wrong-screen key macros, stale runtime, missing metadata, or stale save state into feature proof.

## Current state boundary

The old blocker `blocked_untrusted_brazier_deploy_selector` is obsolete. Do **not** restart from the `20260427_203847` selector gap or burn proof budget rediscovering it.

Run `.userdata/dev-harness/harness_runs/20260427_214207/` greens the focused normal player-action brazier deploy primitive gate:

- fixture storage/accessibility repaired for this primitive: `brazier` is `live_accessible_wielded`; `2x4` and `lighter` are in a `live_accessible_worn_pocket`;
- UI trace proves selected `brazier`, `Deploy where?`, and right/east direction consumption;
- save prompt proof matched `and quit?` and `Case Sensitive`;
- confirmation was sent as uppercase `Y` for the case-sensitive `query_yn` prompt;
- saved-player mtime advanced from `1777318941193895502` to `1777318962722412486`;
- saved east tile `[3368,994,0]` contains `f_brazier` in `audit_saved_target_tile_for_brazier.metadata.json`.

This is **not** real-fire proof and not bandit product proof. It is only the green deploy primitive gate. The fuel/lighter/final `fd_fire` chain remains separate and must carry its own saved-state/writeback guards before any product claim.

## Recommended next evidence target

Next work should start from the now-green `f_brazier` deploy gate and decide one of two narrow paths:

1. If the goal is to make `bandit.live_world_nearby_camp_brazier_deploy_text_audit_mcw` itself an all-green proof packet, prune or link the remaining non-decisive yellow rows (`ocr_without_expected_text_guard`, baseline mtime scan, and confirmation step with no immediate artifact) so the step ledger no longer stays yellow for bookkeeping reasons.
2. If the goal is to continue toward player-lit fire proof, extend the same discipline behind the green deploy gate: fuel/drop guard, lighter/firestarter guard, post-action save/writeback guard, and final saved east-tile `fd_fire` / smoke metadata. Do not call this done until those later gates are green.

## Proof rules to keep in view

- Load/readiness/close is `startup/load`, with `feature_proof=false`.
- `artifacts_matched` is not feature proof unless startup is clean, every step-local ledger row is green, and wait ledgers are not yellow/blocked.
- Screenshot/OCR steps need named expected visible facts; missing guards stay yellow/red/blocked instead of becoming green by silence.
- Exact saved-map state belongs inside the probe via `kind: "audit_saved_map_tile_near_player"`, with `required_fields` / `required_items` / `required_furniture`; the step writes `<label>.metadata.json` and feeds `metadata_expectation` into `probe.step_ledger.json`.
- Exact save/writeback state now belongs inside the probe via `kind: "audit_player_save_mtime"`; do not read saved-map proof after a live action unless the same-run writeback gate is green.
- A green deploy primitive is not a green fire chain. Keep evidence classes explicit, ja, otherwise we are back to serving yesterday's soup as fresh.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit target.

## Schani handoff shape

Keep the final handoff concise: active item, what changed/validated, exact evidence artifacts, remaining state/blocker, and recommended next review/action. Routine progress stays agent-visible only unless genuinely parked/blocked/decision-needed.
