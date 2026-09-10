# R-029 local activation repair

Deliver the complete `R-029-S002` outcome from the current `dev` source. Implement canonical NPC activation and exact movement arbitration, then build and obtain fresh source-bound native proof for the affected refusal/combat/aftermath/persistence route.

The authoritative behavior is the `R-029-S002` slice in `.de67/FS.md`. Reconcile that slice with current source before editing. Preserve the accepted rolling gate/control/reload and Pay evidence at their existing ceilings; do not replay them merely to regenerate artifacts.

Start from the proven diagnosis in receipt `58e6879974cd7923a97f81904ed6fd4b6e24838434efb940e3a508e8d737667f` and `build_logs/r029-activation-plan-001.md`: placement currently relocates overmap NPC pointers but does not canonically admit them to the active creature tracker. Treat that as a hypothesis to confirm against current HEAD, not as permission to copy a dated patch.

Required implementation and checks:

- Preserve the exact operation, reservation, member IDs, and shared NPC objects.
- Validate the complete party and empty in-bounds placements before moving inactive members; call canonical `game::load_npcs()` once after placement.
- Require pointer identity, active/reality-bubble presence, and exactly-once creature-tracker membership before local interaction succeeds. Repeated admission must be idempotent. Partial, blocked, or missing admission must fail recoverably.
- Retain strategic orders. Exclude only exact IDs in an active local `committed_contact` hostile reservation from generic overmap travel until authoritative return or terminal handoff. Preserve unrelated NPC movement, rolling ambush, normal parley, paid retreat, and surviving return.
- Add focused tests for identity, ownership, admission failure/idempotence, stale inherited travel orders, downstream gates, and unrelated controls.
- Capture complete build and test output in the task's normal log route. Inspect exit status and relevant compiler/test diagnostics; do not return only a summary.
- Fresh native verification must reach real active refusal/player attack, an attributable combat consequence, casualty/aftermath and surviving return state, plus affected save/reload continuity. Setup, posture, accepted input, OCR, terminal bytes, and rendered text receive zero gameplay credit.

Use the qualified harness documentation and current registry/cockpit routes. You own every game attempt, helper attempt, replacement, broker, and runner through verified PID/birth exit or an explicit retained-session handoff. Do not impose automatic time or RSS limits. Preserve failed startups and replacements with exact identities. Keep credentials and unrelated dirty work untouched.

Return findings, contradictions, uncertainty, exact source/executable/run/request/frame identities, complete-log handles, tests, process cleanup, and the first remaining boundary. A passing suite does not substitute for native consequences. If repository-owned harness, fixture, registry, observation, or native semantic instrumentation blocks the affected route, diagnose and repair it within scope, then recover the interaction. Do not edit the independent `R-029-S003` sound behavior or DE67 policy/clock/ledger state.

The pending owner corrections are active: no OCR proof; no replay as a substitute for affected fresh proof; exact runtime ownership and cleanup; preserve historical results at their stated ceilings. Explicitly acknowledge these corrections and show how they were applied, or state a concrete unresolved limitation.
