# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** — canonical roadmap and current delivery target
- **SUCCESS.md** — success-state ledger / crossed-off exit criteria for roadmap items
- **TODO.md** — short execution queue for the current target only
- **TESTING.md** — current validation policy, latest relevant evidence, and pending probes
- **TechnicalTome.md** — durable mechanic notes, not daily state tracking
- **COMMIT_POLICY.md** — checkpoint rules to prevent repo soup

If these files disagree, **Plan.md wins** and the other files should be repaired.

## Working rules for agents

- Do **not** mechanically grab the first unchecked-looking thing from some list.
- Follow the current delivery target below and move it to its **next real state**.
- Josef being unavailable for playtesting is **not** a blocker by itself.
- When a target is waiting on Josef, move to the next best unblocked target.
- If no good unblocked target remains, send Josef a short parked-options note so he can greenlight the next lane; do not just keep revalidating the old packet.
- Prefer batching human-only asks where practical. One useful packet with two real product questions beats two tiny pings.
- Keep these files lean. Remove finished fluff from `TODO.md` and `TESTING.md` instead of piling up crossed-off archaeology.
- Each real roadmap item needs an explicit success state in `SUCCESS.md` (or an equally explicit inline auxiliary) so completion is visible instead of guessed.
- Cross off reached success-state items; only remove the whole roadmap item from `Plan.md` once its success state is fully crossed off / done.
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## 1. Current delivery target — Locker Zone V2

**Status:** GREENLIT / ACTIVE

Josef wants the locker roadmap active again and wants Locker Zone **V1** represented as a small bundled task set so it cannot be crossed off half-built.
`SUCCESS.md` now treats V1 as a few bundled completion tasks; by that bundled definition, V1 is checkpointed and the next active locker slice is **V2**.

Current job:
- land **Locker Zone V2** ranged readiness and locker-based reload support
- keep **Locker Zone V3** queued immediately after V2
- if V2 work disproves any bundled V1 claim, reopen V1 first instead of pretending V2 can continue cleanly

### Immediate next move
- define the minimal V2 contract first: basic ranged readiness, up to two compatible magazines, and locker-ammo reload support
- wire that through the existing locker planner/service path without ballooning into V3 nuance
- keep `Plan.md`, `SUCCESS.md`, `TODO.md`, and `TESTING.md` aligned to the locker roadmap, not to stale Basecamp leftovers

---

## 2. Checkpointed — Locker Zone V1

**Status:** CHECKPOINTED / DONE FOR NOW

V1 is only considered done because the bundled V1 task set in `SUCCESS.md` is fully checked.
That bundled close-out is meant to stop false completion:
- locker surface/control exists as a real zone-manager + camp-policy feature
- locker outfitting core exists as real planner/service behavior
- locker maintenance rhythm exists as real dirty/queue/reservation behavior
- V1 has deterministic + proportional runtime proof recorded

If later code work shows any one of those bundled claims is false or incomplete, reopen V1 immediately.

---

## 3. Greenlit / queued after V2 — Locker Zone V3

**Status:** GREENLIT / QUEUED

After V2, continue straight into Locker Zone V3:
- seasonal dressing / winter-vs-summer logic
- coats / blankets / shorts / similar weather-sensitive wardrobe choices
- per-NPC overrides and nuance

Do **not** pull V3 detail into V2 unless the docs are updated first.
V2 stays about basic ranged readiness/reload support; V3 is the nuance pass.

---

## 4. Checkpointed — post-Locker-V1 Basecamp follow-through

**Status:** CHECKPOINTED / DONE FOR NOW

This queue reached its exit criteria for now:
- the board/job log packet is legible enough to compare against the deterministic router proof
- the deterministic board packaging is cleaner/upstream-friendlier
- the richer structured treatment now follows the board-emitted `next=` tokens instead of dropping straight back to spoken bark
- the testing/docs packet describes the closed state instead of an open queue

Keep this closed unless Josef explicitly reopens Basecamp prompt follow-through or a later change breaks the structured board/job lane again.

---

## 5. Checkpointed — LLM-side board snapshot path

**Status:** CHECKPOINTED

This slice reached its exit criteria for now:
- routing proof exists on the actual camp request router, not only on helper builders
- the richer structured/internal `show_board` lane is covered with deterministic evidence
- the short spoken board bark stayed separate
- the testing/docs packet can now describe current truth instead of an open routing question

Keep this out of the active queue unless later code changes break the route again or a new greenlit slice explicitly extends it.

---

## 6. Additional parked options

None outside the locker roadmap right now.
If Josef later wants more after V3, add a short fresh menu then instead of resurrecting stale parked clutter.

---

## 7. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
