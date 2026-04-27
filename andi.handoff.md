# Andi handoff: C-AOL harness trust audit + proof-freeze v0

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`
- actual-playtest stack: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- inventory/proof matrix: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`
- current harness entry point: `tools/openclaw_harness/startup_harness.py`

## Current active lane

Active lane remains `C-AOL harness trust audit + proof-freeze v0`.

Do **not** steer back into the older `C-AOL debug-proof finish stack v0`; that stack reached its honest Schani-review/implemented-but-unproven boundaries where needed. Current work is a primitive trust audit and actual-playtest proof discipline: the harness must not turn load-and-close, wrong-screen key macros, stale runtime, missing metadata, or stale save state into feature proof.

## Current state boundary

The old blockers `blocked_untrusted_brazier_deploy_selector`, `blocked_untrusted_post_action_save_writeback`, and the generic fuel save/writeback loop are obsolete as next targets. Do **not** restart from `20260427_203847`, `20260427_214207`, or the post-fuel save prompt variants just to rediscover them.

Run `.userdata/dev-harness/harness_runs/20260427_222635/` is the preserved all-green scoped deploy primitive proof:

- selected `brazier` is proven;
- `Deploy where?` is proven;
- right/east direction consumption is proven;
- case-sensitive save prompt is proven before uppercase `Y`;
- saved-player mtime advances from `1777321610298746508` to `1777321628903849060`;
- saved east tile `[3368,994,0]` contains `furniture=["f_brazier"]`;
- step ledger is 16/16 green with `step_ledger_summary.status=green_step_local_proof`, `evidence_class=feature-path`, `feature_proof=true` for this scoped deploy primitive only.

Fuel continuation reached its current honest boundary at `.userdata/dev-harness/harness_runs/20260427_232220/`: `blocked_untrusted_drop_filter_or_inventory_visibility`.

That means filtered Multidrop has no visible/selectable `typeid="2x4"` / plank fuel row before selection keys run. Therefore no exact count selection, confirm-return, post-fuel save request, post-fuel mtime/writeback, current-tile `2x4`, lighter, or final `fd_fire` proof is credited. Keep the fuel scenario as red/non-green audit evidence unless the fixture/live fuel availability primitive materially changes or Josef returns manual evidence.

This is **not** real-fire proof and not bandit product proof. It is deploy proof plus a named fuel blocker. Do not loop fuel/lighter/fire macros from here.

## Recommended next evidence target

Next work is **Smart Zone Manager live layout verification packet v0** on a clean/disposable actual-playtest path.

Proof burden:

- capture/generated zone positions after creation and reopen;
- prove intended-separate zones do not share a tile;
- allow overlaps only where explicitly intended by the deterministic allowlist;
- use screenshots and/or exact saved/UI zone metadata as live product proof;
- treat deterministic geometry tests as support only, not product closure;
- do not reuse the contaminated old McWilliams Smart Zone macro as closure proof;
- if the clean live path cannot honestly reach or inspect generated layout, name the UI/harness blocker instead of closing the claim.

Fire/player-lit bandit signal work stays blocked behind fuel. Do not continue to lighter/final `fd_fire` until a post-fuel same-run save/writeback advances mtime and saved-map metadata proves current-tile `f_brazier` plus exact `2x4`.

## Proof rules to keep in view

- Load/readiness/close is `startup/load`, with `feature_proof=false`.
- `artifacts_matched` is not feature proof unless startup is clean, every step-local ledger row is green, and wait ledgers are not yellow/blocked.
- Screenshot/OCR steps need named expected visible facts; missing guards stay yellow/red/blocked instead of becoming green by silence.
- Exact saved-map state belongs inside the probe via `kind: "audit_saved_map_tile_near_player"`, with `required_fields` / `required_items` / `required_furniture`; the step writes `<label>.metadata.json` and feeds `metadata_expectation` into `probe.step_ledger.json`.
- Exact save/writeback state belongs inside the probe via `kind: "audit_player_save_mtime"`; do not read saved-map proof after a live action unless the same-run writeback gate is green.
- A green deploy primitive is not a green fire chain. A red fuel visibility blocker is not permission for another blind key variant. Keep evidence classes explicit, ja, otherwise we are back to serving yesterday's soup as fresh.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit target.

## Schani handoff shape

Keep the final handoff concise: active item, what changed/validated, exact evidence artifacts, remaining state/blocker, and recommended next review/action. Routine progress stays agent-visible only unless genuinely parked/blocked/decision-needed.
