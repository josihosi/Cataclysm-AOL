# Andi handoff: C-AOL harness trust audit + proof-freeze v0

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`
- actual-playtest stack: `doc/c-aol-actual-playtest-verification-stack-v0-2026-04-27.md`
- inventory checkpoint: `doc/c-aol-harness-trust-audit-inventory-v0-2026-04-27.md`
- Frau-reviewed proof-freeze matrix: `doc/c-aol-harness-proof-freeze-matrix-v0-2026-04-28.md`
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

The first debug-spawn item path, scoped talker-selector recipe, and scoped follower-rules menu recipe are already green as primitives:

- `.userdata/dev-harness/harness_runs/20260428_011205/`: debug item spawn -> save/writeback -> same-run saved-player `toothbrush_electric=1` metadata.
- `.userdata/dev-harness/harness_runs/20260428_014348/`: nearby interaction menu -> `Talk to whom` -> highlighted-row `enter` -> NPC dialogue response surface (`Your response`).
- `.userdata/dev-harness/harness_runs/20260428_014825/`: visible `[rules] ... work together` response -> `Rules for your follower`.

Do **not** steer back into those already-credited primitives. The current unblocked next target is one of:

1. broader debug-spawn family target-state gates, e.g. field/furniture/NPC/weather same-run metadata proof shaped like the item primitive;
2. follower rules toggle proof after the already-proven rules UI opens;
3. another explicit Schani/Josef-named harness seam.

Keep the same proof burden: scenario contract names the exact claim, step ledger guards the GUI/key path, and same-run metadata or claim-scoped artifacts prove the target state. Scoped navigation/process proof remains scoped; do not inflate it into package product behavior.

Smart Zone Manager live layout verification is no longer the next target. It is implemented-but-unproven in Josef's playtest package:

- the prior bounded attempts exhausted the Smart Zone live/UI budget without generated-layout proof;
- the current-runtime follow-up `.userdata/smart-zone-safe-clean-20260427/harness_runs/20260428_001347/` is startup/load red only;
- OCR on `failure_timeout.png` says `Dunn has no characters to load`;
- no Smart Zone feature steps ran and no product layout proof is credited.

Do **not** run another Smart Zone clean-profile probe unless Schani/Josef explicitly reopen it after a materially repaired loadable-profile/UI primitive. Do not reuse the contaminated old McWilliams Smart Zone macro as closure proof.

Fire/player-lit bandit signal work stays blocked behind fuel. Do not continue to lighter/final `fd_fire` until a post-fuel same-run save/writeback advances mtime and saved-map metadata proves current-tile `f_brazier` plus exact `2x4`.

## Proof rules to keep in view

- Load/readiness/close is `startup/load`, with `feature_proof=false`.
- `artifacts_matched` is not feature proof unless startup is clean, every step-local ledger row is green, wait ledgers are not yellow/blocked, and the matched artifact is **claim-scoped**: named in the scenario contract as evidence for that exact claim, not just some generic debug line.
- The current classifier is not a metadata-only feature-proof mode. All-green metadata rows without a claim-scoped artifact match remain process/metadata evidence unless a later harness change adds an explicit metadata-only proof classification.
- Green ledger rows are necessary, not magical. Raw `press`/`type` steps only go green with immediate screen/metadata proof or a named causal deferred guard; baseline-only rows are setup, not closure.
- `proof_deferred_to_label` may only credit a step when the deferred guard actually covers that step's expected effect. One final state must not launder several unrelated blind keypresses.
- Screenshot/OCR steps need named expected visible facts; missing guards stay yellow/red/blocked instead of becoming green by silence.
- Exact saved-map state belongs inside the probe via `kind: "audit_saved_map_tile_near_player"`, with `required_fields` / `required_items` / `required_furniture`; the step writes `<label>.metadata.json` and feeds `metadata_expectation` into `probe.step_ledger.json`.
- Exact save/writeback state belongs inside the probe via `kind: "audit_player_save_mtime"`; do not read saved-map proof after a live action unless the same-run writeback gate is green.
- Same-save/provenance is an anti-fixture-bias gate: a transform may not create the exact state being claimed as product behavior. If it does, the run is setup/synthetic proof only. Copied saves need post-copy live-accessible preflight; do not trust inherited inventory by vibes, because apparently that is how the soup gets haunted.
- A green deploy primitive is scoped primitive proof, not product proof. A green deploy primitive is not a green fire chain. A red fuel visibility blocker is not permission for another blind key variant. Keep evidence classes explicit, ja, otherwise we are back to serving yesterday's soup as fresh.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit target.

## Schani handoff shape

Keep the final handoff concise: active item, what changed/validated, exact evidence artifacts, remaining state/blocker, and recommended next review/action. Routine progress stays agent-visible only unless genuinely parked/blocked/decision-needed.
