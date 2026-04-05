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

## Locker Zone follow-through

Status: ACTIVE

Baseline already reached:
- [x] `CAMP_LOCKER` exists as the explicit physical supply zone.
- [x] Locker policy state exists and influences locker candidate selection.
- [x] Planner / service / reservation groundwork exists for downtime locker servicing.
- [x] Wake-up dirty and policy-change dirty already feed the locker queue.
- [x] Agent-side deterministic, startup, and live downtime proof exists on the current binary.

Selected next chunk — locker dirty-trigger follow-through:
- [ ] New eligible locker-zone gear can dirty relevant assignees for later service.
- [ ] Losing/dropping important managed gear can dirty the affected assignee for later service.
- [ ] Deterministic coverage exists for the new dirty-trigger behavior.
- [ ] Proportional runtime validation for this new chunk is recorded in `TESTING.md`.
- [ ] Any Josef-specific follow-up checks for this chunk are written down as non-blocking notes, not treated as a plan blocker.

Notes:
- Basecamp bark / craft / board work is closed and does not belong in this active success state.
- Josef testing does not gate progress here.

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
