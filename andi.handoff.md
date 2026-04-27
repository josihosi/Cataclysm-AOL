# Andi handoff: current C-AOL debug stack

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`
- closed prerequisite: `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`
- first reopened product-proof contract: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`

## Current state

Active lane is now `C-AOL debug-proof finish stack v0`.

`Basecamp job spam debounce + locker/patrol exceptions packet v0` is closed/checkpointed by `5ecf5ebdf2 Debounce basecamp job chatter`. Do not treat the remaining debug-proof items as parked/review-only. Finish them or move implemented-but-unproven behavior to Josef's playtest package under the attempt rule below.

## Attempt rule

For the same `TODO.md` item or phase-blocker:

1. Attempts 1-2 may be Andi solo retries.
2. Multiple focused harness attempts in one cron run are allowed again when each attempt changes setup, instrumentation, or evidence class and output stays small.
3. Consult Frau Knackal before attempt 3.
4. Attempts 3-4 are the final changed retries after consultation.
5. After attempt 4, do **not** close or park implemented code just because the proof still fails. Write a concise implemented-but-unproven packet to `/Users/josefhorvath/.openclaw/workspace/runtime/josef-playtest-package.md`, preserve the caveat in canon, and move to the next greenlit debug note.

## Reopened debug-proof stack

- Raw saved `fd_fire` / `fd_smoke` proof is reader/consumer proof only; it cannot close player-fire product behavior.
- A bounded synthetic smoke-source proof is allowed only when labeled `synthetic smoke-source/live-signal proof`.
- Full player-fire proof still means normal game mechanics: brazier, wood/fuel, lighter/ignition, visible fire/smoke, safe player/survival state, several-hour wait, and matched-site bandit signal evidence.
- Clean threshold-surviving light proof for the synthetic loaded-map `fd_fire` source path is covered by `.userdata/dev-harness/harness_runs/20260427_114034/`; do not mistake it for full player-lit-fire proof.
- Smart Zone Manager layout correction reached its honest agent-side boundary: deterministic geometry/separation proof is green, the clean GUI macro still could not honestly inspect generated layout, and the manual close recipe is in Josef's playtest package as `smart-zone-live-layout-separation-correction`. Do not rerun the contaminated McWilliams macro or another Smart Zone GUI loop unless Josef reopens it or the harness primitive materially changes.
- Harness warning from Josef: if the run only opens the game, reaches a screen, and closes, it is **startup/load proof only**. Future harness-driven feature proof needs named screenshots/checkpoints before action, UI/menu open, after generation/action, and final inspected layout. Do not claim feature proof from load-and-close artifacts.
- Bandit local sight-avoid + scout return is reopened as a follow-up after Smart Zone. Josef's current live playtest says smoke attraction works, but the bandit gets too close and does not time out/return home. First inspect recent logs/save fields for the sortie clocks and `return_home`/`local_contact`/`sight_avoid`; then fix/prove the product path without relying solely on pre-aged harness seam proof.
- New greenlit process package: `C-AOL harness trust audit + proof-freeze packet v0` (`doc/c-aol-harness-trust-audit-and-proof-freeze-packet-v0-2026-04-27.md`). Do not start it unless the lane is promoted, but do obey its trust rule now: every harness-driven feature proof needs stepwise screenshot/metadata evidence, and load-and-close is startup proof only. When activated, Andi should audit the whole harness surface with Frau Knackal review rather than run another feature retry.

Keep the handoff to Schani concise: active item, what changed, evidence, remaining blocker, and recommended Schani action.
