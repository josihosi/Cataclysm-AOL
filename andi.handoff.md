# Andi handoff: current C-AOL debug stack

## Canon anchors

- active plan: `Plan.md`
- active queue: `TODO.md`
- validation ledger: `TESTING.md`
- success ledger: `SUCCESS.md`
- active contract: `doc/basecamp-job-spam-debounce-exceptions-packet-v0-2026-04-26.md`
- next greenlit contract: `doc/caol-debug-proof-finish-stack-v0-2026-04-27.md`
- reopened bandit contract: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`

## Current state

Active lane remains `Basecamp job spam debounce + locker/patrol exceptions packet v0` unless Schani explicitly interrupts it.

After that, Josef has greenlit `C-AOL debug-proof finish stack v0`. Do not treat the remaining debug-proof items as parked/review-only. Finish them or move implemented-but-unproven behavior to Josef's playtest package under the attempt rule below.

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
- Clean threshold-surviving light proof remains open if live light still shows `matched_light_sites=0`.
- Smart Zone clean-save live proof is greenlit if still needed, but do not rerun the old McWilliams/bandit-contaminated macro as closure proof.

Keep the handoff to Schani concise: active item, what changed, evidence, remaining blocker, and recommended Schani action.
