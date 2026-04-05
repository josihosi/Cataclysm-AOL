# SUCCESS

Exit criteria ledger for roadmap items.

Use this file so completion is explicit instead of vibes-based.

## Rules

- Every real roadmap item in `Plan.md` should have a matching success state here (or an equally explicit inline auxiliary).
- When a success-state condition is reached, **cross it off here instead of deleting it immediately**.
- When all conditions for a roadmap item are crossed off, that roadmap item is **DONE** and can be removed from `Plan.md`.
- Josef self-testing is **never** a blocker in this file.
- If Josef-specific tests or checks are useful, write them down as non-blocking notes so Josef can do them later from his own testing list.
- This file is a completion ledger, not a changelog.

---

## Locker Zone v1

Status: CHECKPOINTED / DONE FOR NOW

V1 success state:
- [x] `CAMP_LOCKER` exists as the explicit physical supply zone.
- [x] Locker policy/menu state exists, influences locker candidate selection, and survives save/load.
- [x] Representative locker item classification exists across the managed V1 slots.
- [x] Locker-zone candidate gathering and reservation filtering work deterministically.
- [x] Per-slot score helpers and meaningful-upgrade thresholds exist for obvious V1 choices.
- [x] Planner / service / reservation groundwork exists for downtime locker servicing.
- [x] Locker execution can equip missing categories, apply obvious upgrades, and return replaced managed gear.
- [x] Duplicate managed gear cleanup works.
- [x] Wake-up dirty, policy-change dirty, new eligible locker gear, and lost/dropped managed gear all feed the locker queue.
- [x] One-worker-at-a-time queue sequencing works.
- [x] Deterministic coverage exists for the V1 behavior slice.
- [x] Proportional runtime validation for the same downtime/service path is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- `dirty-trigger follow-through` was the final V1 chunk landed, not a separate greenlight lane.
- Locker candidate scanning now uses sorted locker tiles so debug/state summaries stay deterministic enough for dirty-trigger tracking and tests.
- Basecamp bark / craft / board work stays closed and does not belong back in this slice.
- Josef testing did not gate progress here.

---

## LLM-side board snapshot path

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] Routing proof exists on the real `handle_heard_camp_request` path.
- [x] Structured/internal `show_board` emits the richer handoff snapshot.
- [x] Spoken `show me the board` stays on the concise spoken bark path.
- [x] Deterministic evidence for that split is recorded.

Notes:
- Retained here so the closed slice stays visible without drifting back into active work.
