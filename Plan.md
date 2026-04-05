# Plan

Canonical roadmap for Cataclysm-AOL.

This file answers one question: **what should the agent advance next?**
It is not a changelog, not a graveyard of crossed-off substeps, and not a place to preserve every historical detour.
Rewrite it as reality changes.

## File roles

- **Plan.md** — canonical roadmap and current delivery target
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
- Prefer agent-side playtesting first. Josef should be used for product judgment, feel, priority calls, or genuinely human-only checks.
- Validation should match risk:
  - docs-only change -> no compile
  - small local code change -> narrow compile/test
  - broad or risky code change, or a Josef handoff -> broader rebuild / startup harness as needed
- Follow `COMMIT_POLICY.md`. Do not let the repo turn into one giant dirty blob.

---

## 1. Current delivery target — Locker Zone v1

**Status:** ACTIVE

### Goal
Ship the first genuinely useful camp locker system on `dev`:
- explicit `CAMP_LOCKER` zone
- persisted camp-wide locker policy
- locker candidate classification and planning
- live NPC downtime service path for duplicate cleanup / obvious upgrades
- queue/reservation logic so camp workers do not fight over the same gear

### What is already true
- Locker policy, zone type, menu wiring, save/load, classifier, candidate gathering, planner logic, and the first live equip/drop path all exist.
- The downtime orchestration tail exists too: wake/dirty queueing, temporary reservations, and the first-service self-requeue fix are already in the tree.
- Deterministic locker tests cover the current orchestration slice.
- Human-readable locker debug summaries now exist for live probes (`before` / `plan` / `after`).
- Fresh startup/load smoke on current locker binaries is green again after explicit relinks.
- A real current-binary live probe now exists on `dev` / `Sandy Creek`: after restoring the missing `CAMP_LOCKER` character-zone fixture from backup and running an in-game wait-to-midnight probe, Bruna Priest hit the actual downtime queue path and deduped managed gear into locker storage on `1a72369cfb-dirty`.

### What is still missing
The missing blocker is no longer live locker proof.
What remains is repo hygiene: the locker tree still needs to be checkpointed or split into reviewable commits instead of living as one big dirty blob.
If the commit split changes behavior, rerun only the narrow locker evidence that honestly covers the changed slice.

### Exit criteria for this target
Before this target can move out of active status:
1. run and document one real downtime-driven live locker probe
2. capture before/after evidence clearly enough that a human can tell what changed
3. checkpoint or split the locker diff into reviewable commits instead of one giant mudball
4. update `TODO.md` and `TESTING.md` to reflect the new truth without keeping dead checklist fluff

### Immediate next move
Checkpoint or split the locker work per `COMMIT_POLICY.md` now that the live downtime packet is real.
Keep the commit shape legible: locker behavior/test/data/mechanic-note work together, ledger alignment separately if needed, and leave unrelated bark/prompt churn out of the locker checkpoint.

---

## 2. Waiting on Josef — Basecamp bark feel pass

**Status:** WAITING ON JOSEF

The Basecamp bark pass is already agent-validated enough.
Josef input is still wanted for tone/feel on:
- short spoken board bark
- subject-first status bark
- blocked craft bark
- clear/archive wording

This is **not** the active execution target while Josef feedback is pending.
Do not keep revalidating the same packet unless the code changes again.
If other agent-side work exists, keep moving there.
If several Josef-only judgments accumulate naturally, batch them.

---

## 3. Next target after Locker Zone v1 — LLM-side board snapshot path

**Status:** QUEUED

The product split is settled for now:
- live natural speech `show me the board` stays concise and human-facing
- the richer `planner_move` + overmap snapshot belongs in the structured / LLM-side path

So the next movement-side work, after locker proof, is not another spoken-board debate.
It is to wire the richer snapshot into the real LLM fallback/prompt path and prove it entered that path with the right evidence.

---

## 4. Parked projects available for greenlight if active lanes stall

**Status:** PARKED MENU

If the active target is honestly blocked and there is no good unblocked next-best lane, send Josef a short menu from here instead of spinning in place.
Current parked candidates:
- upstream deterministic PR/reference cleanup
- broader LLM-side board snapshot / prompt work beyond the queued next slice
- any later Basecamp UX cleanup that is not needed for Locker Zone v1

Keep the menu short and concrete: what it is, why it is parked, and what one greenlighted next chunk would be.

---

## 5. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
