# Current gameplay findings

## Confirmed active gameplay bugs

None currently established by the reconciled receipts. This is not an all-green test claim.
R-037 coverage and dependent R-026 acceptance remain open for ordinary evidence reconciliation.

## Current observations and limits

- R-037 inventory: receipt `bebbc8ae96665d23837a79d2788a3049b08cfed91bedc493345b1695cdaf521d` records negative wear/wield results and direct activate choosing `look_inventory|equip_melee`, native rock wield and an inactive lighter. API controls remain separate negative/inconclusive results. The demonstrated POSIX runner EOF fault was repaired and tested; it is not an open defect. These observations do not establish another causal gameplay bug.
- R-037 panic/calm: receipt `7c135856f634950dd5afa0a931cadf44d618d951ceb0a68a758cce702e42acfa` records E4B `panic_on` without attributable native flee/countdown, natural calm choosing `follow_close`, API panic-on movement/countdown/expiry, and an inconclusive API calm control. Preserve these distinctions in the coverage assessment; the API result does not prove the E4B route passed.
- R037-F018 is not an active bug or an owner-promotion wait. Josef's 2026-09-11 clarification accepts `equip_bow` as existing silent/ranged preference with ordinary game selection/fallback. Receipt `f71f69f3ba06cb9468efa0cb2c441db32181de96f6cc106cc5dfc52299eab595` still records six-shooter retention and no demonstrated bow/arrow consequence on the E4B and API runs. Its firearms-only explanation is false: `shortbow` has `GUN` subtype. Preinspection lists bow UID 129 and arrows UID 130 in NPC inventory, not worn; their location at selection and the exact eligibility/noise/ranking branch were not established. Do not call that unknown a proven floor/setup problem, a native pass, or a new forced-bow requirement.
- Broad fixture-suite failures and source/build limitations retain their existing evidence ceilings. No broad-suite pass is inferred from focused tests. Missing historical API usage remains unknown; old missing-credential and dependency entries are superseded by the verified, authorized `CATA_API_KEY` route.
- R-037 owner-clarified reconciliation 011 publishes all 20 catalog rows in `.de67/task-logs/r037-owner-clarified-coverage-reconciliation-011-manifest.json` (SHA `4a142a6edc91484c267e4153f9377c34241879b35707b7fd7dc73dbf46153f96`). It retires the former firearms-only/forced-bow interpretation and keeps E4B/API verdicts separate. Ordinary closure is not ready: the first remaining material gap is foreign-owner and zone-terminal pickup behavior, which is already assigned within the authorized continuation scope. No active gameplay bug is asserted by this reconciliation.

## Reconciled repairs and evidence

R029-F004 activation/aftermath and R029-F002 sound information are completed within accepted R-029 scope, not active repairs. Activation receipts `d7d55782706720d8c98bcb92c32a3c5fb400946acc3240de4272908e06111a08`, `dd6683cec9819497b6970907f7291f307716ac0230a34a9775dd0a5b9230af00` and `632168355fe0566c76f079b179a4130f99a0ef1838ea7ed4713640ef956e9c98` bind active ownership, combat, casualty, return and reload. Final sound receipt `c8d0ede5a40e26132e072df71bc783d3c6017f04a0bf08f01b81003ad996c7db` binds entry into the ordinary outing pipeline; no later report/arrival is added. Durable R-029 acceptance via `R-029-closure-sound-information-semantics-008` and the package guide retain all independent ceilings. Other previously repaired/accepted intake entries remain historical.

Full original observations, obsolete statuses and exact handles are preserved in
`.de67/state/review-owner-5eb4cdd4a66d/local-before/debug-findings.md` and their durable receipts.
The review report is `.de67/task-logs/review-owner-5eb4cdd4a66d/report.md`.
Sol owns current coverage reconciliation and any independently necessary remaining investigation;
owner clarification does not turn negative or inconclusive results into passes.

## R-ZL-LIGHT-OPTICS route-repair witness

The source-bound native run `6b54a47aa8878afa99f9b70d20ef2ce83425c3465c9a974b967c941987ca8c23`
bound the repaired executable and observed the isolated exposed light/control contrast through
game minute 8639. The bridge then exited with `KeyboardInterrupt` while decoding a partial
semantic JSON record, before the requested 8640 scheduler boundary; this leaves route ownership
and approach-distance evidence unestablished. The owned game was closed through the exact-PID
native quit route and its OS exit verified. Preserve the focused witness as inconclusive/repair:
`.de67/task-logs/r-zl-light-optics-route-repair-witness.json`.

## R-ZL natural-stalker fresh-account prompt obstruction

Fresh ordinary source-bound run `caa2472cc75426c3ced81b34c1351023f8765cd5fc2c32654f492c9847ddb295`
advanced through native movement and auto-travel until the game raised the player-visible prompt
`feral human spotted! Cancel auto move? (Case Sensitive)`.  The native descriptor advertised YES
and NO `prompt.choose` actions, but the submitted YES request
`play-20869db661fc40e3b45e9ec59017c0f9` ended in
`native_surface_receipt_timeout`; a fresh observation retained the identical prompt.  This is a
concrete cockpit-route obstruction, not evidence of natural writhing-stalker opportunism.  The
sealed witness is inconclusive/repair; terminalization verified the owned PID 8936 exited after
scenario cleanup (SIGTERM, no native-exit credit).  Retained journal SHA:
`0877676bdbc9f9b92168ab3a9363ce0f84141bfd9437b05b48fdac999178500a`.

## Current pickup continuation after task012

Task012's claim deadline expired with the branch unfinished; its earlier formal finding remains honest history. The repeated `pickup.item_missing` reason does not prove the item vanished or that no zone guard ran. Exact run `f01017a559f5afbb6c2387891e3ced0cb21b89e99274d5bc616d9b5c7a4e62f0` records `zone_skips=1` at the initial target consumer. The source skips NO_NPC_PICKUP tiles before searching their items, while the later `pickup.zone_forbidden` terminal belongs to the already-selected-target path. This is a demonstrated diagnostic ambiguity, not proof of a broken gameplay zone rule. Exact-item attribution/no-transfer and the distinct foreign-owner branch remain the material evidence question. Task013 commissions only the necessary observation/fixture/control and focused continuation; do not repeat persistence-only restaging or force a later reason code. Current handoff: `.de67/task-logs/review-r037-generation-007/handoff.md`.
