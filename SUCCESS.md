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

## Locker Zone V2

Status: ACTIVE

Success state:
- [ ] Locker service can select up to two compatible magazines from locker supply when a managed ranged loadout needs them.
- [ ] Locker service can reload supported ranged weapons from locker-zone ammo supply.
- [ ] V2 ranged-readiness logic stays basic and deterministic instead of ballooning into V3 fashion/season nuance.
- [ ] Deterministic coverage exists for the V2 ranged readiness / reload behavior.
- [ ] Proportional runtime validation for V2 is recorded in `TESTING.md`.
- [ ] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V2 is the active next locker slice after the bundled V1 close-out.
- If V2 work disproves any bundled V1 claim, reopen V1 first instead of pretending V2 can continue cleanly.
- V3 stays queued until V2 is actually closed.

---

## Locker Zone V1

Status: CHECKPOINTED / DONE FOR NOW

Bundled V1 completion tasks:
- [x] **Locker surface/control task** — `CAMP_LOCKER` exists as a Zone Manager zone, camp locker policy state exists, the player-facing locker policy menu/control exists, and the policy survives save/load.
- [x] **Locker outfitting core task** — representative locker item classification, candidate gathering, score helpers, planning, equip/upgrade behavior, duplicate cleanup, and returning replaced managed gear all exist.
- [x] **Locker maintenance rhythm task** — wake-up dirty, policy-change dirty, new-gear dirty, lost-gear dirty, queue sequencing, and reservation behavior all exist.
- [x] **Locker V1 proof task** — deterministic coverage plus proportional runtime validation for the same downtime/service path are recorded in `TESTING.md`.
- [x] Any Josef-specific V1 follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V1 is only considered done because the whole bundled task set above is checked.
- `dirty-trigger follow-through` was the final V1 chunk landed, not the name of the whole feature.
- Locker candidate scanning now uses sorted locker tiles so debug/state summaries stay deterministic enough for dirty-trigger tracking and tests.
- If later code or testing disproves any bundled V1 task, reopen V1 immediately.

---

## Locker Zone V3

Status: GREENLIT / QUEUED AFTER V2

Success state:
- [ ] Seasonal dressing / winter-vs-summer wardrobe logic exists.
- [ ] Weather-sensitive wardrobe choices (coats / blankets / shorts / similar) are handled deliberately rather than by V1/V2 shortcuts.
- [ ] Per-NPC overrides / nuance exist without undoing the simpler V1/V2 deterministic spine.
- [ ] Deterministic coverage exists for the V3 behavior that is actually implemented.
- [ ] Proportional runtime validation for V3 is recorded in `TESTING.md`.
- [ ] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- V3 is already greenlit, but it stays queued until V2 is actually done.
- Do not quietly drag V3 nuance into V2.

---

## Post-Locker-V1 Basecamp follow-through

Status: CHECKPOINTED / DONE FOR NOW

Success state:
- [x] The live `DEBUG_LLM_INTENT_LOG` board/job artifact packet is made legible enough to stand beside the deterministic router proof.
- [x] The deterministic Basecamp board/job work is pruned/packaged into a cleaner upstream-ready shape.
- [x] The richer structured board/prompt treatment is extended beyond `show_board` in a deliberate next slice.
- [x] Proportional validation for each finished sub-slice is recorded in `TESTING.md`.
- [x] Any Josef-specific follow-up checks are written down as non-blocking notes rather than treated as plan blockers.

Notes:
- This follow-through lane was already greenlit; it is now closed again instead of parked half-finished.
- Landed order: artifact proof cleanup -> upstream deterministic cleanup -> structured `next=` token follow-through.
- No Josef-specific follow-up checks are currently needed for this closed slice.

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
